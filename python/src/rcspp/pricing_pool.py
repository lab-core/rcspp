"""Cross-process shared pricing pool for RCSPP column generation.

See class docstrings for full API documentation.

Quick reference
---------------
::

    # Master
    pool   = PricingPool(n_constraints=200, max_cols=50_000)
    col_id = pool.add(solution)            # syncs C++ + shared pool
    ids, rcs = pool.price(duals)           # returns ColumnIds, not shared indices

    # Workers
    handle = pool.handle()
    shared = PricingPool.attach(handle)
    indices, rcs = shared.price(duals)     # returns shared slot indices

    # B&B
    sub      = pool.new_filter(forbidden_arc_ids=[10], max_age=100)
    ids, rcs = sub.price(duals)
    pool.update_activity(basis_col_ids)    # forwarded to C++ FilteredSolutionPool
    pool.close()
"""

from __future__ import annotations

import warnings
from multiprocessing import Lock
from multiprocessing.shared_memory import SharedMemory
from typing import TYPE_CHECKING

import numpy as np

try:
    from scipy.sparse import csr_matrix as _sp_csr

    _SCIPY_AVAILABLE = True
except ImportError:  # pragma: no cover
    _SCIPY_AVAILABLE = False

try:
    from rcspp._core import solution_pool as _sp

    _SolutionPool = _sp.SolutionPool
except ImportError:  # pragma: no cover
    _SolutionPool = None

if TYPE_CHECKING:
    from rcspp._core.graph import Solution

_scipy_warning_emitted = False


def _untrack_shared_memory(shm: SharedMemory) -> None:  # pragma: no cover
    """Detach a non-owned segment from the ``resource_tracker``.

    When a process opens an existing :class:`SharedMemory` with
    ``create=False``, CPython still registers the segment with the
    per-process ``resource_tracker``, which unlinks it when that process
    exits.  Under the ``spawn`` start method (the default on macOS and
    Windows) every worker runs its own tracker, so the first worker to exit
    destroys the owner's segment; later workers then fail to attach, die
    mid-task, and :class:`multiprocessing.pool.Pool` deadlocks waiting for a
    result that never arrives.

    Unregistering here ensures only the owning process (via
    :meth:`SharedPricingPool.unlink`) ever destroys the segment.  This is a
    no-op on platforms or Python versions where the tracker is not used.

    Args:
        shm: The attached, non-owned shared-memory segment.
    """
    try:
        from multiprocessing import resource_tracker

        resource_tracker.unregister(shm._name, "shared_memory")
    except Exception:  # noqa: BLE001
        # No resource_tracker (e.g. Windows) or already unregistered.
        pass


# ── Shared-memory header ──────────────────────────────────────────────────────
_HEADER_DTYPE = np.dtype(
    [
        ("count", np.uint64),
        ("nnz", np.uint64),
        ("n_constraints", np.uint64),
        ("max_cols", np.uint64),
        ("max_nnz", np.uint64),
        ("valid_offset", np.uint64),
        ("costs_offset", np.uint64),
        ("row_starts_offset", np.uint64),
        ("col_indices_offset", np.uint64),
        ("col_values_offset", np.uint64),
    ]
)
_HEADER_BYTES = 128


def _align_up(n: int, align: int) -> int:
    """Round ``n`` up to the nearest multiple of ``align``."""
    return (n + align - 1) & ~(align - 1)


# ── SharedPricingPool ─────────────────────────────────────────────────────────


class SharedPricingPool:
    """Cross-process shared pricing pool — the only class workers need.

    Stores LP column data in CSR format inside a single ``SharedMemory``
    segment.  :meth:`price` is lock-free; only writes acquire the lock.

    Workers call ``PricingPool.attach(handle)`` to obtain one::

        shared = PricingPool.attach(handle)
        # returns (shared_slot_indices, reduced_costs), sorted best-first
        indices, rcs = shared.price(duals)
        shared.close()

    For the full workflow including ColumnIds and activity tracking, use
    :class:`PricingPool` on the master side.

    Args:
        n_constraints: Highest constraint index + 1.  Over-allocate (e.g. 1000).
        max_cols: Column capacity.
        max_nnz_per_col: Max non-zeros per column (default 50).
        name: ``SharedMemory`` segment name; auto-generated if ``None``.
        lock: External lock for spawn-safe use (``Manager().Lock()``).
    """

    def __init__(
        self,
        n_constraints: int,
        max_cols: int = 50_000,
        max_nnz_per_col: int = 50,
        name: str | None = None,
        lock: object | None = None,
    ) -> None:
        self._n_constraints = int(n_constraints)
        self._max_cols = int(max_cols)
        self._max_nnz = int(max_cols) * int(max_nnz_per_col)
        self._lock = lock if lock is not None else Lock()

        valid_offset = _HEADER_BYTES
        costs_offset = _align_up(valid_offset + max_cols, 8)
        row_starts_offset = _align_up(costs_offset + max_cols * 8, 8)
        col_indices_offset = _align_up(row_starts_offset + (max_cols + 1) * 4, 8)
        col_values_offset = _align_up(col_indices_offset + self._max_nnz * 4, 8)
        total = col_values_offset + self._max_nnz * 8

        self._shm = SharedMemory(name=name, create=True, size=total)
        self._shm.buf[:total] = b"\x00" * total

        hdr = np.ndarray((1,), dtype=_HEADER_DTYPE, buffer=self._shm.buf)
        hdr["count"] = 0
        hdr["nnz"] = 0
        hdr["n_constraints"] = n_constraints
        hdr["max_cols"] = max_cols
        hdr["max_nnz"] = self._max_nnz
        hdr["valid_offset"] = valid_offset
        hdr["costs_offset"] = costs_offset
        hdr["row_starts_offset"] = row_starts_offset
        hdr["col_indices_offset"] = col_indices_offset
        hdr["col_values_offset"] = col_values_offset

        self._valid_offset = valid_offset
        self._costs_offset = costs_offset
        self._row_starts_offset = row_starts_offset
        self._col_indices_offset = col_indices_offset
        self._col_values_offset = col_values_offset
        self._init_views()

    def _init_views(self) -> None:
        buf = self._shm.buf
        self._header = np.ndarray((1,), dtype=_HEADER_DTYPE, buffer=buf)
        self._valid = np.ndarray(
            (self._max_cols,), dtype=np.uint8, buffer=buf, offset=self._valid_offset
        )
        self._col_costs = np.ndarray(
            (self._max_cols,), dtype=np.float64, buffer=buf, offset=self._costs_offset
        )
        self._row_starts = np.ndarray(
            (self._max_cols + 1,),
            dtype=np.int32,
            buffer=buf,
            offset=self._row_starts_offset,
        )
        self._col_indices = np.ndarray(
            (self._max_nnz,), dtype=np.int32, buffer=buf, offset=self._col_indices_offset
        )
        self._col_values = np.ndarray(
            (self._max_nnz,), dtype=np.float64, buffer=buf, offset=self._col_values_offset
        )

    @classmethod
    def attach(cls, handle: dict) -> "SharedPricingPool":
        """Attach to an existing pool from a worker process (zero-copy)."""
        obj = object.__new__(cls)
        obj._lock = handle["lock"]
        # This process does not own the segment, so it must not let its
        # resource_tracker unlink it on exit (spawn-safety).  On Python 3.13+
        # ``track=False`` keeps the segment out of the tracker entirely — the
        # clean way, with no shutdown noise.  On older versions, register and
        # then immediately unregister (see _untrack_shared_memory); that is
        # still spawn-safe but the shared tracker logs a benign KeyError at
        # exit because the owner's unlink() unregisters the same name again.
        name = handle["shm_name"]
        try:
            obj._shm = SharedMemory(name=name, create=False, track=False)
        except TypeError:  # pragma: no cover  # Python < 3.13: no ``track`` parameter
            obj._shm = SharedMemory(name=name, create=False)
            _untrack_shared_memory(obj._shm)
        hdr = np.ndarray((1,), dtype=_HEADER_DTYPE, buffer=obj._shm.buf)
        obj._n_constraints = int(hdr["n_constraints"][0])
        obj._max_cols = int(hdr["max_cols"][0])
        obj._max_nnz = int(hdr["max_nnz"][0])
        obj._valid_offset = int(hdr["valid_offset"][0])
        obj._costs_offset = int(hdr["costs_offset"][0])
        obj._row_starts_offset = int(hdr["row_starts_offset"][0])
        obj._col_indices_offset = int(hdr["col_indices_offset"][0])
        obj._col_values_offset = int(hdr["col_values_offset"][0])
        obj._init_views()
        return obj

    def handle(self) -> dict:
        """Return a picklable handle for :meth:`attach`."""
        return {"shm_name": self._shm.name, "lock": self._lock}

    # ── Write operations ──────────────────────────────────────────────────────

    def _filter_in_range(self, idx: np.ndarray, coef: np.ndarray):
        """Drop (index, coefficient) pairs whose constraint index is out of range.

        Vectorized; returns the inputs unchanged (no copy) when every index is already
        ``< n_constraints`` — the common case with an over-allocated ``n_constraints``.
        """
        mask = idx < self._n_constraints
        if mask.all():
            return idx, coef
        return idx[mask], coef[mask]

    def add(self, solution: "Solution") -> int:
        """Add one column and return its internal shared slot index.

        The slot index is opaque to most callers — use :class:`PricingPool`
        or :class:`FilteredPricingPool` if you need ColumnIds::

            slot = shared.add(solution)   # used internally by PricingPool

        Args:
            solution: Solution whose ``column`` attribute contains LP cost and rows.

        Returns:
            Shared slot index (0-based, internal to this pool).

        Raises:
            RuntimeError: Pool is full (columns or non-zeros exhausted).
        """
        cost, idx, coef = solution.column.to_arrays()
        idx, coef = self._filter_in_range(idx, coef)
        n_rows = len(idx)
        with self._lock:
            count = int(self._header["count"][0])
            nnz = int(self._header["nnz"][0])
            if count >= self._max_cols:
                raise RuntimeError(f"SharedPricingPool full (cols: {count}/{self._max_cols})")
            new_nnz = nnz + n_rows
            if new_nnz > self._max_nnz:
                raise RuntimeError(
                    f"SharedPricingPool non-zero capacity exceeded ({new_nnz}/{self._max_nnz})"
                )
            self._col_costs[count] = cost
            self._col_indices[nnz:new_nnz] = idx  # int64 → int32 cast on assignment
            self._col_values[nnz:new_nnz] = coef
            self._row_starts[count + 1] = new_nnz
            self._valid[count] = 1
            self._header["nnz"] = new_nnz
            self._header["count"] = count + 1
        return count

    def add_columns(self, solutions: list) -> list[int]:
        """Batch-add multiple columns under a single lock acquisition.

        More efficient than calling :meth:`add` in a loop::

            slots = shared.add_columns([sol1, sol2, sol3])

        Args:
            solutions: List of Solution objects.

        Returns:
            List of shared slot indices, one per solution.

        Raises:
            RuntimeError: Batch would exceed column or non-zero capacity.
        """
        if not solutions:
            return []
        costs = np.empty(len(solutions), dtype=np.float64)
        lengths = np.empty(len(solutions), dtype=np.int64)
        idx_parts: list[np.ndarray] = []
        coef_parts: list[np.ndarray] = []
        for k, sol in enumerate(solutions):
            cost, idx, coef = sol.column.to_arrays()
            idx, coef = self._filter_in_range(idx, coef)
            costs[k] = cost
            lengths[k] = len(idx)
            idx_parts.append(idx)
            coef_parts.append(coef)
        all_idx = np.concatenate(idx_parts)
        all_coef = np.concatenate(coef_parts)
        total = int(lengths.sum())
        with self._lock:
            start = int(self._header["count"][0])
            nnz_start = int(self._header["nnz"][0])
            end = start + len(solutions)
            if end > self._max_cols:
                raise RuntimeError("SharedPricingPool: batch add would exceed column capacity")
            if nnz_start + total > self._max_nnz:
                raise RuntimeError(
                    f"SharedPricingPool: batch add would exceed non-zero capacity "
                    f"({nnz_start + total}/{self._max_nnz})"
                )
            self._col_costs[start:end] = costs
            self._col_indices[nnz_start : nnz_start + total] = all_idx
            self._col_values[nnz_start : nnz_start + total] = all_coef
            # CSR row pointers: prefix sums of per-column lengths, offset by nnz_start.
            self._row_starts[start + 1 : end + 1] = nnz_start + np.cumsum(lengths)
            self._valid[start:end] = 1
            self._header["nnz"] = nnz_start + total
            self._header["count"] = end
        return list(range(start, end))

    def add_from_lp_arrays(
        self,
        col_costs: np.ndarray,
        row_starts: np.ndarray,
        col_indices: np.ndarray,
        col_values: np.ndarray,
        valid_mask: np.ndarray | None = None,
    ) -> list[int]:
        """Bulk-add from pre-built CSR arrays (from SolutionPool.get_lp_arrays()).

        Zero-copy if the dtypes already match (float64 costs/values, uint32
        row_starts/indices); one copy otherwise.  Much faster than calling
        :meth:`add` in a loop when populating from the C++ pool::

            costs, rs, ci, cv = pool._cpp_pool.get_lp_arrays()
            shared_idxs = shared_pool.add_from_lp_arrays(costs, rs, ci, cv)

        Args:
            col_costs: float64 array of LP costs, shape ``(n_cols,)``.
            row_starts: uint32 CSR row-pointer array, shape ``(n_cols + 1,)``.
            col_indices: uint32 constraint indices, shape ``(nnz,)``.
            col_values: float64 coefficients, shape ``(nnz,)``.
            valid_mask: Optional bool array ``(n_cols,)``; only True slots added.

        Returns:
            List of shared slot indices assigned to the added columns.
        """
        col_costs = np.asarray(col_costs, dtype=np.float64)
        row_starts = np.asarray(row_starts, dtype=np.int32)
        col_indices = np.asarray(col_indices, dtype=np.int32)
        col_values = np.asarray(col_values, dtype=np.float64)

        sel = np.arange(len(col_costs))
        if valid_mask is not None:
            sel = sel[np.asarray(valid_mask, dtype=np.bool_)]

        n_add = len(sel)
        if n_add == 0:
            return []

        # Pre-compute new nnz counts per selected column.
        nnz_per = np.diff(row_starts)[sel]
        new_nnz_total = int(nnz_per.sum())

        with self._lock:
            start = int(self._header["count"][0])
            nnz_start = int(self._header["nnz"][0])
            if start + n_add > self._max_cols:
                raise RuntimeError("SharedPricingPool: bulk add would exceed column capacity")
            if nnz_start + new_nnz_total > self._max_nnz:
                raise RuntimeError("SharedPricingPool: bulk add would exceed non-zero capacity")
            cursor = nnz_start
            for local_i, src_i in enumerate(sel):
                slot = start + local_i
                self._col_costs[slot] = col_costs[src_i]
                src_start = int(row_starts[src_i])
                src_end = int(row_starts[src_i + 1])
                length = src_end - src_start
                self._col_indices[cursor : cursor + length] = col_indices[src_start:src_end]
                self._col_values[cursor : cursor + length] = col_values[src_start:src_end]
                cursor += length
                self._row_starts[slot + 1] = np.int32(cursor)
                self._valid[slot] = np.uint8(1)
            self._header["nnz"] = cursor
            self._header["count"] = start + n_add
        return list(range(start, start + n_add))

    def update(self, shared_index: int, solution: "Solution") -> None:
        """Refresh an existing slot's LP cost and coefficient values in place.

        Mirrors the C++ ``SolutionPool`` dedup refresh ("latest column wins"):
        the row *indices* are fixed by the arc path, so only ``col_cost`` and the
        matching coefficient *values* are overwritten.  The non-zero layout is
        left untouched, so the slot index stays valid and nothing is reallocated.
        An index present in the stored slot but absent from ``solution`` keeps its
        old value (consistent with the C++ in-place refresh)::

            slot = shared.add(sol_v1)     # cost/coef v1
            shared.update(slot, sol_v2)   # same arc path → refreshed in place

        When the re-proposed column is identical to what is already stored (the
        common "same column re-proposed unchanged" case), this returns after a
        lock-free comparison — without acquiring the write lock or dirtying the
        shared segment.

        Args:
            shared_index: Slot index previously returned by :meth:`add`.
            solution: Solution carrying the refreshed column cost/coefficients.
        """
        cost, idx, coef = solution.column.to_arrays()
        idx, coef = self._filter_in_range(idx, coef)
        slot = int(shared_index)
        new_cost = float(cost)
        start = int(self._row_starts[slot])
        end = int(self._row_starts[slot + 1])
        stored_idx = self._col_indices[start:end]
        stored_val = self._col_values[start:end]

        # Build the target coefficient values for this slot, matching the C++ refresh:
        # only indices that appear in the new column are overwritten; the rest are kept.
        idx32 = idx.astype(np.int32, copy=False)
        if len(idx32) == len(stored_idx) and np.array_equal(stored_idx, idx32):
            target = coef  # same row structure & order (the normal case) → fully vectorized
        else:
            new_coef = dict(zip(idx.tolist(), coef.tolist()))
            target = np.array(stored_val, dtype=np.float64)
            for k, ix in enumerate(stored_idx.tolist()):
                if ix in new_coef:
                    target[k] = new_coef[ix]

        # Short-circuit true duplicates: nothing changed → no lock, no write
        # (lock-free compare, consistent with the lock-free reads in price()).
        if self._col_costs[slot] == new_cost and np.array_equal(stored_val, target):
            return

        with self._lock:
            self._col_costs[slot] = new_cost
            self._col_values[start:end] = target

    def invalidate(self, shared_indices: list[int]) -> None:
        """Mark column slots as deleted so they are skipped during :meth:`price`.

        The slot data remains in memory; only the ``valid`` flag is cleared.
        Called automatically by :meth:`PricingPool.remove_stale` — direct use
        is rarely needed::

            shared.invalidate([slot_0, slot_5])   # advanced use only

        Args:
            shared_indices: Slot indices previously returned by :meth:`add`.
                An empty list is a no-op.
        """
        if not shared_indices:
            return
        with self._lock:
            self._valid[np.asarray(shared_indices, dtype=np.intp)] = np.uint8(0)

    # ── Pricing (lock-free) ───────────────────────────────────────────────────

    def price(
        self,
        duals: np.ndarray,
        threshold: float = -1e-9,
        view_mask: np.ndarray | None = None,
    ) -> tuple[np.ndarray, np.ndarray]:
        """Compute reduced costs.  Returns ``(shared_slot_indices, rcs)`` sorted best-
        first.

        Lock-free.  Workers use this directly::

            indices, rcs = shared.price(duals)

        For master-side use returning ColumnIds, see :meth:`PricingPool.price`.

        Args:
            duals: 1-D float64 LP dual values.
            threshold: Keep only columns with ``rc < threshold`` (default -1e-9).
            view_mask: Optional bool array ``(max_cols,)`` restricting which slots
                are priced (used internally by :class:`FilteredSharedPricingPool`).

        Returns:
            ``(shared_indices, reduced_costs)`` sorted ascending by rc.
        """
        global _scipy_warning_emitted  # noqa: PLW0603
        duals = np.asarray(duals, dtype=np.float64)
        n = int(self._header["count"][0])
        nnz = int(self._header["nnz"][0])
        if n == 0:
            return np.empty(0, dtype=np.intp), np.empty(0, dtype=np.float64)

        n_duals = min(len(duals), self._n_constraints)
        col_costs = np.asarray(self._col_costs[:n], dtype=np.float64)

        if _SCIPY_AVAILABLE:
            indptr = self._row_starts[: n + 1]
            sp_idx = self._col_indices[:nnz]
            sp_val = self._col_values[:nnz]
            A = _sp_csr((sp_val, sp_idx, indptr), shape=(n, self._n_constraints), copy=False)
            d = np.zeros(self._n_constraints, dtype=np.float64)
            d[:n_duals] = duals[:n_duals]
            rc = col_costs - A @ d
        else:  # pragma: no cover  # scipy not installed
            if not _scipy_warning_emitted:
                warnings.warn(
                    "scipy not installed — SharedPricingPool.price() uses a slower numpy "
                    "fallback.  Install scipy: pip install scipy",
                    stacklevel=2,
                )
                _scipy_warning_emitted = True
            rc = col_costs.copy()
            if nnz > 0:
                counts = np.diff(self._row_starts[: n + 1].astype(np.int64))
                row_idx = np.repeat(np.arange(n, dtype=np.int64), counts)
                ci = self._col_indices[:nnz].astype(np.int64)
                mask_nnz = ci < n_duals
                rc -= np.bincount(
                    row_idx[mask_nnz],
                    weights=self._col_values[:nnz][mask_nnz] * duals[ci[mask_nnz]],
                    minlength=n,
                )

        valid = self._valid[:n].view(np.bool_)
        active = valid
        if view_mask is not None:
            active = active & view_mask[:n]
        active = active & (rc < threshold)

        indices = np.where(active)[0]
        if len(indices) == 0:
            return np.empty(0, dtype=np.intp), np.empty(0, dtype=np.float64)
        rc_sel = rc[indices]
        order = np.argsort(rc_sel, kind="stable")
        return indices[order], rc_sel[order]

    # ── Views ─────────────────────────────────────────────────────────────────

    @property
    def col_costs_view(self) -> np.ndarray:
        """Zero-copy float64 LP costs for committed columns ``(count,)``."""
        return self._col_costs[: self.count]

    @property
    def row_starts_view(self) -> np.ndarray:
        """Zero-copy int32 CSR row-pointer array ``(count+1,)``."""
        return self._row_starts[: self.count + 1]

    @property
    def col_indices_view(self) -> np.ndarray:
        """Zero-copy int32 constraint-index array for non-zeros ``(nnz,)``."""
        return self._col_indices[: self.nnz]

    @property
    def col_values_view(self) -> np.ndarray:
        """Zero-copy float64 coefficient array for non-zeros ``(nnz,)``."""
        return self._col_values[: self.nnz]

    @property
    def valid_view(self) -> np.ndarray:
        """Zero-copy uint8 validity flags ``(max_cols,)``."""
        return self._valid

    @property
    def count(self) -> int:
        """Total committed column slots (including invalidated ones)."""
        return int(self._header["count"][0])

    @property
    def nnz(self) -> int:
        """Total committed non-zeros."""
        return int(self._header["nnz"][0])

    @property
    def active_count(self) -> int:
        """Number of currently active (non-invalidated) columns."""
        n = self.count
        return int(self._valid[:n].sum()) if n > 0 else 0

    def close(self) -> None:
        """Detach without destroying (call from workers)."""
        self._shm.close()

    def unlink(self) -> None:
        """Close and destroy the segment (call once from the owner)."""
        self._shm.close()
        self._shm.unlink()

    def __del__(self) -> None:
        try:
            self._shm.close()
        except Exception:  # noqa: BLE001  # pragma: no cover
            pass

    def __repr__(self) -> str:
        return (
            f"SharedPricingPool(count={self.count}, nnz={self.nnz}, "
            f"active={self.active_count}, max_cols={self._max_cols})"
        )


# ── FilteredSharedPricingPool ─────────────────────────────────────────────────


class FilteredSharedPricingPool:
    """Process-local boolean mask over a :class:`SharedPricingPool`.

    Used internally by :class:`FilteredPricingPool`; also available directly
    for advanced use.

    Args:
        shared: The backing ``SharedPricingPool``.
        view_indices: Shared slot indices to include.
            Empty array → empty view.  ``None`` → all currently valid columns.
    """

    def __init__(self, shared: SharedPricingPool, view_indices: np.ndarray | None = None) -> None:
        self._shared = shared
        self._mask = np.zeros(shared._max_cols, dtype=np.bool_)
        if view_indices is not None and len(view_indices) > 0:
            self._mask[np.asarray(view_indices, dtype=np.intp)] = True
        elif view_indices is None:
            n = shared.count
            if n > 0:
                self._mask[:n] = shared._valid[:n].view(np.bool_)

    def add_to_view(self, shared_indices: list[int] | np.ndarray) -> None:
        """Include slots in this view (B&B backtrack).

        Args:
            shared_indices: Shared slot indices to re-include in the mask.
        """
        if len(shared_indices) > 0:
            self._mask[np.asarray(shared_indices, dtype=np.intp)] = True

    def remove_from_view(self, shared_indices: list[int] | np.ndarray) -> None:
        """Exclude slots from this view (B&B restriction).

        Args:
            shared_indices: Shared slot indices to hide from pricing.
        """
        if len(shared_indices) > 0:
            self._mask[np.asarray(shared_indices, dtype=np.intp)] = False

    def price(self, duals: np.ndarray, threshold: float = -1e-9) -> tuple[np.ndarray, np.ndarray]:
        """Price only the slots visible through this mask.

        Args:
            duals: 1-D float64 LP dual values.
            threshold: Keep only columns with ``rc < threshold`` (default -1e-9).

        Returns:
            ``(shared_indices, reduced_costs)`` sorted ascending by rc.
        """
        return self._shared.price(duals, threshold=threshold, view_mask=self._mask)

    @property
    def view_count(self) -> int:
        """Number of slots currently in this view."""
        n = self._shared.count
        return int(self._mask[:n].sum()) if n > 0 else 0

    @property
    def mask(self) -> np.ndarray:
        """Zero-copy boolean mask ``(max_cols,)``."""
        return self._mask

    def __repr__(self) -> str:
        return f"FilteredSharedPricingPool(view_count={self.view_count})"


# ── FilteredPricingPool ───────────────────────────────────────────────────────


class FilteredPricingPool:
    """Unified C++ filter + numpy mask view.

    :meth:`price` returns **ColumnIds** (not shared slot indices) so results can be
    passed directly to ``update_activity``, ``get``, etc.

    All C++ ``FilteredSolutionPool`` methods are forwarded via ``__getattr__`` including
    ``update_activity(basis_col_ids)``.
    """

    def __init__(self, parent: "PricingPool", cpp_fp: object) -> None:
        """Wrap a C++ ``FilteredSolutionPool`` with a numpy mask view.

        Args:
            parent: Owning :class:`PricingPool` that holds the shared memory.
            cpp_fp: C++ ``FilteredSolutionPool`` instance to wrap.
        """
        self._parent = parent
        self._cpp_fp = cpp_fp
        self._numpy_fp = self._build_numpy_filter()

    def _build_numpy_filter(self) -> FilteredSharedPricingPool:
        col_ids = self._cpp_fp.get_column_ids()  # np.ndarray[uint64]
        if len(col_ids) == 0:
            return FilteredSharedPricingPool(
                self._parent._shared, view_indices=np.empty(0, dtype=np.intp)
            )
        shared_indices = self._parent._id_to_shared[col_ids.astype(np.int64)]
        valid = shared_indices >= 0
        return FilteredSharedPricingPool(
            self._parent._shared,
            view_indices=shared_indices[valid].astype(np.intp),
        )

    # ── Write ─────────────────────────────────────────────────────────────────

    def add(self, solution: "Solution") -> int:
        """Add to C++ pool, shared pool, and this numpy mask.  Returns ColumnId.

        Deduplication: if the same arc path has already been added the existing
        ColumnId is returned and no new shared slot is allocated::

            col_id = sub.add(solution)
            sub.update_activity([col_id])   # immediately mark as basis member

        Args:
            solution: Solution to add.

        Returns:
            C++ ColumnId (stable, can be used in ``update_activity``, ``get``, etc.)
        """
        cpp_id = self._cpp_fp.add(solution)
        cid = int(cpp_id)
        self._parent._ensure_id_capacity(cid)
        # Dedup guard: allocate a new shared slot only for a new ColumnId; on a
        # dedup hit refresh the existing slot so price() uses the latest column (H-1).
        existing = self._parent._id_to_shared[cid]
        if existing < 0:
            shared_idx = self._parent._shared.add(solution)
            self._parent._id_to_shared[cid] = shared_idx
            self._parent._shared_to_id[shared_idx] = cid
            self._numpy_fp.add_to_view([shared_idx])
        else:
            self._parent._shared.update(int(existing), solution)
        return cpp_id

    def add_columns(self, solutions: list) -> list[int]:
        """Batch-add multiple solutions.  Returns list of ColumnIds.

        More efficient than calling :meth:`add` in a loop::

            col_ids = pool.add_columns([sol1, sol2, sol3])

        Args:
            solutions: List of Solution objects.

        Returns:
            List of C++ ColumnIds, one per solution.
        """
        col_ids = self._cpp_fp.add(solutions)
        result = []
        for sol, cid_raw in zip(solutions, col_ids):
            cid = int(cid_raw)
            self._parent._ensure_id_capacity(cid)
            existing = self._parent._id_to_shared[cid]
            if existing < 0:
                shared_idx = self._parent._shared.add(sol)
                self._parent._id_to_shared[cid] = shared_idx
                self._parent._shared_to_id[shared_idx] = cid
                self._numpy_fp.add_to_view([shared_idx])
            else:  # dedup hit → refresh the shared slot (H-1)
                self._parent._shared.update(int(existing), sol)
            result.append(cid_raw)
        return result

    # ── Pricing ───────────────────────────────────────────────────────────────

    def price(
        self, duals: np.ndarray, threshold: float = -1e-9, track_activity: bool = True
    ) -> tuple[np.ndarray, np.ndarray]:
        """Lock-free filtered pricing.  Returns ``(ColumnIds, rcs)`` sorted best-first.

        ColumnIds can be passed directly to ``update_activity``::

            ids, rcs = sub.price(duals)
            sub.update_activity(lp_basis_ids)   # forwarded to C++ pool

        When ``track_activity`` is true (default) the C++ ``ColumnActivity`` for this
        view's columns is also updated (see :meth:`PricingPool.price`) so activity-based
        filters and :meth:`remove_stale` work off the public ``price()``.  Activity
        follows the C++ filter view; columns hidden only via :meth:`remove_from_view`
        (numpy mask) are still counted.  Pass ``track_activity=False`` to skip it.
        """
        duals = np.asarray(duals, dtype=np.float64)
        if track_activity:
            self._cpp_fp.price_numpy(duals, threshold)  # side effect: update ColumnActivity
        shared_indices, rcs = self._numpy_fp.price(duals, threshold)
        if len(shared_indices) == 0:
            return np.empty(0, dtype=np.uint64), rcs
        col_ids = self._parent._shared_to_id[shared_indices.astype(np.int64)]
        valid = col_ids >= 0
        return col_ids[valid].astype(np.uint64), rcs[valid]

    # ── Filter narrowing ──────────────────────────────────────────────────────

    def new_filter(self, **kwargs) -> "FilteredPricingPool":
        """Create a further-narrowed view.  All kwargs forwarded to C++
        ``new_filter()``.

        Accepts solution-level args (``forbidden_arc_ids``, ``compulsory_rows``, …)
        and activity-level args (``min_usage_rate``, ``max_age``, ``max_last_rc``)::

            # Chain-narrow: columns must pass both the parent filter AND this one.
            sub2 = sub.new_filter(compulsory_rows=[0], max_age=50)
            ids, rcs = sub2.price(duals)

        Returns:
            A new :class:`FilteredPricingPool` further restricting this view.
        """
        return FilteredPricingPool(self._parent, self._cpp_fp.new_filter(**kwargs))

    def refresh(self) -> None:
        """Rebuild the numpy mask from the current C++ view state.

        Useful when columns were added externally (through another pool or
        filter) and you want the mask to include them::

            pool.add(solution)   # added outside this filter
            sub.refresh()        # numpy mask now reflects C++ view
        """
        self._numpy_fp = self._build_numpy_filter()

    # ── Remove / invalidate ───────────────────────────────────────────────────

    def _col_ids_to_shared(self, col_ids) -> np.ndarray:
        arr = np.asarray(col_ids, dtype=np.int64)
        sidxs = self._parent._id_to_shared[arr]
        return sidxs[sidxs >= 0]

    def remove_stale(self, max_age: int, min_usage_rate: float = 0.0) -> list[int]:
        """Remove stale columns from the C++ view and invalidate shared slots.

        A column is removed if ``age > max_age`` OR
        (``priced_count > 0`` AND ``usage_rate < min_usage_rate``)::

            removed = sub.remove_stale(max_age=100, min_usage_rate=0.01)
            # Call sort_by_lp_index() afterwards if pricing performance matters.
            sub.sort_by_lp_index()

        Args:
            max_age:         Remove if not seen for more than this many price() rounds.
            min_usage_rate:  Remove if returned less than this fraction of pricings.

        Returns:
            List of removed ColumnIds.
        """
        removed = self._cpp_fp.remove_stale(max_age, min_usage_rate)
        if removed:
            sidxs = self._col_ids_to_shared(removed)
            ids_arr = np.asarray(removed, dtype=np.int64)
            self._parent._shared_to_id[sidxs] = -1
            self._parent._id_to_shared[ids_arr] = -1
            self._parent._shared.invalidate(sidxs.tolist())
        return removed

    def global_remove_if(self, pred) -> list[int]:
        """Hard-delete columns from the main pool; propagates to all views.

        Use sparingly — prefer :meth:`remove_stale` or the local C++ ``remove_if``
        (accessible via ``__getattr__``) for B&B::

            pool.global_remove_if(
                lambda col_id, sol, act: act.age > 500
            )

        Args:
            pred: Callable ``(ColumnId, Solution, ColumnActivity) -> bool``.

        Returns:
            List of permanently deleted ColumnIds.
        """
        removed = self._cpp_fp.global_remove_if(pred)
        if removed:
            sidxs = self._col_ids_to_shared(removed)
            ids_arr = np.asarray(removed, dtype=np.int64)
            self._parent._shared_to_id[sidxs] = -1
            self._parent._id_to_shared[ids_arr] = -1
            self._parent._shared.invalidate(sidxs.tolist())
        return removed

    # ── B&B operators ─────────────────────────────────────────────────────────

    def remove_from_view(
        self,
        *,
        arc_ids: list[int] | None = None,
        col_ids: list[int] | None = None,
    ) -> None:
        """Exclude columns from numpy mask (no shared-memory write, B&B restriction).

        Both args accept lists; any combination is valid::

            sub.remove_from_view(arc_ids=[10, 11])          # by arc
            sub.remove_from_view(col_ids=[col_id_1, col_id_2])  # by ColumnId
            sub.remove_from_view(arc_ids=[10], col_ids=[col_id_3])  # combined

        Args:
            arc_ids: Exclude all columns whose path traverses any of these arcs.
            col_ids: Exclude columns by ColumnId (as returned by :meth:`add`
                or :meth:`price`).
        """
        sidxs: list[int] = []
        if arc_ids is not None:
            for arc_id in arc_ids:
                removed = self._cpp_fp.remove_if_arc_present(arc_id)
                if removed:
                    sidxs += self._col_ids_to_shared(removed).tolist()
        if col_ids is not None:
            sidxs += self._col_ids_to_shared(col_ids).tolist()
        if sidxs:
            self._numpy_fp.remove_from_view(sidxs)

    def add_to_view(
        self,
        *,
        col_ids: list[int] | None = None,
    ) -> None:
        """Re-include columns previously excluded by :meth:`remove_from_view`.

        Modifies the numpy mask only — O(k), no shared-memory write::

            sub.add_to_view(col_ids=[col_id_1, col_id_2])   # backtrack

        .. note::
            To undo an arc-based restriction, save the ColumnIds returned by
            the related :meth:`price` call before the restriction, then pass
            them back here.

        Args:
            col_ids: ColumnIds to re-include (as returned by :meth:`add`
                or :meth:`price`).
        """
        sidxs: list[int] = []
        if col_ids is not None:
            sidxs = self._col_ids_to_shared(col_ids).tolist()
        if sidxs:
            self._numpy_fp.add_to_view(sidxs)

    # ── Accessors ─────────────────────────────────────────────────────────────

    @property
    def column_count(self) -> int:
        """Number of columns in the C++ filter view."""
        return self._cpp_fp.size()

    @property
    def shared_count(self) -> int:
        """Total committed slots in the shared pool (all filters share one pool)."""
        return self._parent._shared.count

    @property
    def active_shared_count(self) -> int:
        """Active (non-invalidated) slots in the shared pool."""
        return self._parent._shared.active_count

    def shared(self) -> SharedPricingPool:
        """Return the underlying :class:`SharedPricingPool`."""
        return self._parent._shared

    def handle(self) -> dict:
        """Return the picklable worker handle (same as parent pool's handle)."""
        return self._parent.handle()

    def __getattr__(self, name: str) -> object:
        """Forward to the C++ ``FilteredSolutionPool`` (incl.

        ``update_activity``).
        """
        return getattr(self._cpp_fp, name)

    def __repr__(self) -> str:
        return f"FilteredPricingPool(view_count={self._numpy_fp.view_count})"


# ── PricingPool ───────────────────────────────────────────────────────────────


class PricingPool:
    """Main column-generation pricing pool.

    Creates and manages the internal C++ ``SolutionPool`` and
    :class:`SharedPricingPool` transparently.

    :meth:`price` returns **ColumnIds** (not shared slot indices) so results
    feed directly into ``update_activity``, ``get``, ``new_filter``, etc.::

        ids, rcs = pool.price(duals)
        pool.update_activity(lp_basis_ids)   # forwarded to C++ pool

    All C++ ``FilteredSolutionPool`` methods forwarded via ``__getattr__``.

    Args:
        n_constraints: Constraint capacity (over-allocate, e.g. 1000).
        max_cols: Column capacity.
        max_nnz_per_col: Max non-zeros per column (default 50).
        lock: External lock for spawn-safe multiprocessing.
    """

    def __init__(
        self,
        n_constraints: int,
        max_cols: int = 50_000,
        max_nnz_per_col: int = 50,
        lock: object | None = None,
    ) -> None:
        self._cpp_pool = _SolutionPool()
        self._cpp_fp = self._cpp_pool.new_filter()

        self._shared = SharedPricingPool(
            n_constraints=n_constraints,
            max_cols=max_cols,
            max_nnz_per_col=max_nnz_per_col,
            lock=lock,
        )

        # ColumnId → shared slot index (-1 = unregistered).
        # Grows dynamically via _ensure_id_capacity().
        self._id_to_shared = np.full(max_cols + 2, -1, dtype=np.int64)

        # Shared slot index → ColumnId (-1 = unregistered/invalidated).
        self._shared_to_id = np.full(max_cols, -1, dtype=np.int64)

    # ── ID-map helpers ────────────────────────────────────────────────────────

    def _ensure_id_capacity(self, cpp_id: int) -> None:
        """Grow ``_id_to_shared`` if ``cpp_id`` would be out of bounds."""
        if cpp_id >= len(self._id_to_shared):
            new_size = max(cpp_id + 2, len(self._id_to_shared) * 2)
            new_arr = np.full(new_size, -1, dtype=np.int64)
            new_arr[: len(self._id_to_shared)] = self._id_to_shared
            self._id_to_shared = new_arr

    def _ensure_shared_capacity(self, shared_idx: int) -> None:
        """Grow ``_shared_to_id`` if ``shared_idx`` would be out of bounds."""
        if shared_idx >= len(self._shared_to_id):
            new_size = max(shared_idx + 2, len(self._shared_to_id) * 2)
            new_arr = np.full(new_size, -1, dtype=np.int64)
            new_arr[: len(self._shared_to_id)] = self._shared_to_id
            self._shared_to_id = new_arr

    def _col_ids_to_shared(self, col_ids) -> np.ndarray:
        arr = np.asarray(col_ids, dtype=np.int64)
        arr = arr[arr < len(self._id_to_shared)]
        sidxs = self._id_to_shared[arr]
        return sidxs[sidxs >= 0]

    # ── Worker support ────────────────────────────────────────────────────────

    @staticmethod
    def attach(handle: dict) -> SharedPricingPool:
        """Attach to the shared pool from a worker process.

        Workers call this to get a :class:`SharedPricingPool` for lock-free
        pricing.  The returned object's :meth:`~SharedPricingPool.price`
        method returns internal shared slot indices — these are opaque to
        workers and only used to communicate which columns have negative rc
        back to the master process, which then resolves them to ColumnIds::

            # Worker
            shared = PricingPool.attach(handle)
            slot_indices, rcs = shared.price(duals)
            # Return slot_indices to master; master calls pool.get_by_slot(...)
            # or simply uses the shared indices to retrieve solutions via
            # shared.col_costs_view[slot_indices], etc.
        """
        return SharedPricingPool.attach(handle)

    def handle(self) -> dict:
        """Picklable handle for worker processes."""
        return self._shared.handle()

    def shared(self) -> SharedPricingPool:
        """Return the underlying :class:`SharedPricingPool` (master shortcut)."""
        return self._shared

    # ── Write operations ──────────────────────────────────────────────────────

    def add(self, solution: "Solution") -> int:
        """Add to both C++ and shared pools.  Returns C++ ColumnId.

        Deduplication: if the same arc path has been added before the existing
        ColumnId is returned and no new shared slot is allocated::

            col_id = pool.add(solution)
            # col_id is stable; use in update_activity, get, new_filter, etc.

        Args:
            solution: Solution with a populated ``column`` attribute.

        Returns:
            C++ ColumnId (monotonically increasing, stable across calls).
        """
        cpp_id = self._cpp_fp.add(solution)
        cid = int(cpp_id)
        self._ensure_id_capacity(cid)
        existing = self._id_to_shared[cid]
        if existing < 0:  # new column, not a duplicate
            shared_idx = self._shared.add(solution)
            self._ensure_shared_capacity(shared_idx)
            self._id_to_shared[cid] = shared_idx
            self._shared_to_id[shared_idx] = cid
        else:  # dedup hit → refresh the shared slot so price() uses the latest column
            self._shared.update(int(existing), solution)
        return cpp_id

    def add_columns(self, solutions: list) -> list[int]:
        """Batch-add multiple solutions.  Returns list of ColumnIds.

        More efficient than calling :meth:`add` in a loop::

            col_ids = pool.add_columns([sol1, sol2, sol3])

        Args:
            solutions: List of Solution objects.

        Returns:
            List of C++ ColumnIds, one per solution.
        """
        col_ids = self._cpp_fp.add(solutions)
        result = []
        for sol, cid_raw in zip(solutions, col_ids):
            cid = int(cid_raw)
            self._ensure_id_capacity(cid)
            existing = self._id_to_shared[cid]
            if existing < 0:
                shared_idx = self._shared.add(sol)
                self._id_to_shared[cid] = shared_idx
                self._shared_to_id[shared_idx] = cid
            else:  # dedup hit → refresh the shared slot (H-1)
                self._shared.update(int(existing), sol)
            result.append(cid_raw)
        return result

    def populate_from_cpp_pool(self) -> list[int]:
        """Bulk-populate the shared pool from the C++ pool's LP arrays.

        Uses ``SolutionPool.get_lp_arrays()`` to copy the internal CSR data
        in one batch — far faster than calling :meth:`add` for each column::

            pool = PricingPool(n_constraints=200, max_cols=50_000)
            # ... C++ pool already populated ...
            pool.populate_from_cpp_pool()

        Returns:
            List of ColumnIds that were populated into the shared pool.
        """
        col_costs, row_starts, col_indices, col_values = self._cpp_pool.get_lp_arrays()
        # Get the ColumnIds for all entries so we can build the maps.
        entries = self._cpp_fp.get_all()
        if not entries:
            return []
        col_ids_raw = [int(e[0]) for e in entries]
        # Map ColumnId → LpStore index (lp_index is stored in Entry, not directly
        # accessible here; use the ordering from get_lp_arrays which matches lp_index).
        # We add them slot-by-slot to maintain the id→shared and shared→id maps.
        result = []
        for cid in col_ids_raw:
            self._ensure_id_capacity(cid)
            if self._id_to_shared[cid] >= 0:
                result.append(cid)
                continue
            # Find this column's lp_index by looking it up in the pool's lp_arrays.
            # Since add_from_lp_arrays adds in order, we push one slot per column.
            sol = self._cpp_fp.get(cid)
            if sol is None:  # pragma: no cover  # defensive: col removed between get_all/get
                continue
            shared_idx = self._shared.add(sol)
            self._id_to_shared[cid] = shared_idx
            self._shared_to_id[shared_idx] = cid
            result.append(cid)
        return result

    # ── Pricing ───────────────────────────────────────────────────────────────

    def price(
        self, duals: np.ndarray, threshold: float = -1e-9, track_activity: bool = True
    ) -> tuple[np.ndarray, np.ndarray]:
        """Price all valid columns.  Returns ``(ColumnIds, rcs)`` sorted best-first.

        ColumnIds can be passed directly to activity-tracking calls::

            ids, rcs = pool.price(duals)
            pool.update_activity(lp_basis_ids)

        The fast result comes from the shared (scipy) pool.  When ``track_activity`` is
        true (default) the C++ ``ColumnActivity`` is also updated — ``priced_count``,
        ``use_count``, ``age`` and ``last_reduced_cost`` — so activity-based filters
        (``max_last_rc``, ``min_usage_rate``) and :meth:`remove_stale` work off the
        public ``price()`` instead of needing a manual C++ pricing call.  This
        recomputes reduced costs in C++; the shared and C++ pools hold identical LP
        data so the values match.  Pass ``track_activity=False`` to skip it when only
        the fast shared result is needed and activity metrics are unused.
        """
        duals = np.asarray(duals, dtype=np.float64)
        if track_activity:
            self._cpp_fp.price_numpy(duals, threshold)  # side effect: update ColumnActivity
        shared_indices, rcs = self._shared.price(duals, threshold)
        if len(shared_indices) == 0:
            return np.empty(0, dtype=np.uint64), rcs
        col_ids = self._shared_to_id[shared_indices.astype(np.int64)]
        valid = col_ids >= 0
        return col_ids[valid].astype(np.uint64), rcs[valid]

    # ── Filter creation ───────────────────────────────────────────────────────

    def new_filter(self, **kwargs) -> FilteredPricingPool:
        """Create a filtered view.  All kwargs forwarded to C++ ``new_filter()``.

        Solution-level args (``forbidden_arc_ids``, ``compulsory_rows``, …) and
        activity-level args (``min_usage_rate``, ``max_age``, ``max_last_rc``) are
        forwarded to the C++ ``new_filter()`` and applied via ``remove_if()`` so the
        C++ view and the numpy mask stay in sync.

        New columns added after the filter is created are NOT automatically included;
        call :meth:`FilteredPricingPool.refresh` or use :meth:`FilteredPricingPool.add`
        to add them explicitly::

            sub = pool.new_filter(
                forbidden_arc_ids=[10, 11],   # arc restriction
                max_age=100,                  # activity filter
                max_last_rc=0.0,              # only historically negative columns
            )
            ids, rcs = sub.price(duals)
            sub.update_activity(lp_basis_ids)

        Returns:
            A :class:`FilteredPricingPool` whose :meth:`~FilteredPricingPool.price`
            returns ColumnIds.
        """
        cpp_fp = self._cpp_fp.new_filter(**kwargs)
        return FilteredPricingPool(self, cpp_fp)

    # ── Remove / invalidate ───────────────────────────────────────────────────

    def remove_stale(self, max_age: int, min_usage_rate: float = 0.0) -> list[int]:
        """Remove stale columns from both the C++ pool and the shared pool.

        A column is removed if ``age > max_age`` OR
        (``priced_count > 0`` AND ``usage_rate < min_usage_rate``)::

            removed = pool.remove_stale(max_age=100, min_usage_rate=0.01)

        Args:
            max_age:         Remove if not returned by ``price()`` for > this rounds.
            min_usage_rate:  Remove if returned < this fraction of pricings.

        Returns:
            List of removed ColumnIds.
        """
        removed = self._cpp_fp.remove_stale(max_age, min_usage_rate)
        if removed:
            sidxs = self._col_ids_to_shared(removed)
            ids_arr = np.asarray(removed, dtype=np.int64)
            valid_arr = ids_arr[ids_arr < len(self._id_to_shared)]
            self._shared_to_id[sidxs] = -1
            self._id_to_shared[valid_arr] = -1
            self._shared.invalidate(sidxs.tolist())
        return removed

    def global_remove_if(self, pred) -> list[int]:
        """Hard-delete columns from the main pool; propagates to all views.

        Use sparingly — prefer :meth:`remove_stale` or the local C++ ``remove_if``
        (via ``__getattr__``) for B&B node restrictions::

            pool.global_remove_if(
                lambda col_id, sol, act: act.age > 500
            )

        Args:
            pred: Callable ``(ColumnId, Solution, ColumnActivity) -> bool``.

        Returns:
            List of permanently deleted ColumnIds.
        """
        removed = self._cpp_fp.global_remove_if(pred)
        if removed:
            sidxs = self._col_ids_to_shared(removed)
            ids_arr = np.asarray(removed, dtype=np.int64)
            valid_arr = ids_arr[ids_arr < len(self._id_to_shared)]
            self._shared_to_id[sidxs] = -1
            self._id_to_shared[valid_arr] = -1
            self._shared.invalidate(sidxs.tolist())
        return removed

    # ── Stats ─────────────────────────────────────────────────────────────────

    @property
    def column_count(self) -> int:
        """Number of columns in the main C++ pool view."""
        return self._cpp_fp.size()

    @property
    def shared_count(self) -> int:
        """Total committed slots in the shared pool."""
        return self._shared.count

    @property
    def active_shared_count(self) -> int:
        """Active (non-invalidated) slots in the shared pool."""
        return self._shared.active_count

    # ── Delegation ────────────────────────────────────────────────────────────

    def __getattr__(self, name: str) -> object:
        """Forward to the C++ ``FilteredSolutionPool`` (incl.

        ``update_activity``).
        """
        return getattr(self._cpp_fp, name)

    def close(self) -> None:
        """Release the shared memory segment."""
        self._shared.unlink()

    def __repr__(self) -> str:
        return f"PricingPool(cols={self.column_count}, shared={self._shared!r})"
