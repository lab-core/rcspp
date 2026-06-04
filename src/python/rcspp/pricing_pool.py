"""Cross-process shared pricing pool for RCSPP column generation.

Two public entry points
-----------------------

:class:`PricingPool`
    **Main pool** — the only object the master process needs.  Creates and
    manages the internal C++ ``SolutionPool`` and the ``SharedPricingPool``
    transparently.  Produces :class:`FilteredPricingPool` views via
    :meth:`new_filter`.

    Workers receive a handle via :meth:`handle` and call
    ``PricingPool.attach(handle)`` to get a :class:`SharedPricingPool` for
    lock-free pricing.

:class:`FilteredPricingPool`
    **Filtered view** — returned by :meth:`PricingPool.new_filter` or
    :meth:`FilteredPricingPool.new_filter`.  Wraps a C++
    ``FilteredSolutionPool`` and a local numpy mask in one object.

    Supports all the same column operations as ``PricingPool`` plus B&B
    operators (:meth:`remove_from_view` / :meth:`add_to_view`) that modify
    the numpy mask only (no shared-memory write, trivial backtracking).

Advanced / worker classes
--------------------------

:class:`SharedPricingPool`
    Raw shared-memory CSR pool — the only thing workers need.  Attach with
    ``PricingPool.attach(handle)``.

:class:`FilteredSharedPricingPool`
    Process-local boolean mask over a ``SharedPricingPool``.  Used internally
    by ``FilteredPricingPool``; also available for advanced users.

Example workflow::

    # Master
    pool = PricingPool(n_constraints=200, max_cols=50_000)
    pool.add(solution)
    sub  = pool.new_filter(forbidden_arc_ids=[10], max_age=100)
    handle = pool.handle()           # send to workers

    # Worker
    shared = PricingPool.attach(handle)
    indices, rcs = shared.price(duals)

    # B&B (master-side)
    sub.remove_from_view(arc_ids=[10, 11])
    indices, rcs = sub.price(duals)
    sub.add_to_view(cpp_ids=[c1, c2])   # backtrack

Shared-memory layout
--------------------
Single ``SharedMemory`` segment — see :class:`SharedPricingPool` for details.
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
except ImportError:  # pragma: no cover — only missing in isolated test envs
    _SolutionPool = None

if TYPE_CHECKING:
    from rcspp._core.graph import Solution

# Emit the scipy fallback warning at most once per process lifetime.
_scipy_warning_emitted = False

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
_HEADER_BYTES = 128  # two cache lines


def _align_up(n: int, align: int) -> int:
    """Round ``n`` up to the nearest multiple of ``align``."""
    return (n + align - 1) & ~(align - 1)


# ── SharedPricingPool ─────────────────────────────────────────────────────────


class SharedPricingPool:
    """Cross-process shared pricing pool with CSR storage and scipy SPMV pricing.

    Stores LP column data in CSR format inside a single ``SharedMemory``
    segment.  Multiple processes attach to the same segment; :meth:`price` is
    lock-free.  Only :meth:`add` and :meth:`invalidate` acquire the write lock.

    If **scipy** is installed, ``price()`` uses a zero-copy ``csr_matrix``
    SPMV.  Without scipy a ``np.bincount`` fallback is used (O(nnz)) with a
    one-time warning.

    Typically obtained from :meth:`PricingPool.handle` +
    ``PricingPool.attach(handle)`` in worker processes.

    Args:
        n_constraints: Highest constraint index + 1.  Over-allocate (e.g. 1000).
        max_cols: Column capacity.
        max_nnz_per_col: Max non-zeros per column (default 50).
        name: SharedMemory segment name; auto-generated if ``None``.
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
        """Attach to an existing pool from a worker process (zero-copy).

        Args:
            handle: dict returned by :meth:`handle`.
        """
        obj = object.__new__(cls)
        obj._lock = handle["lock"]
        obj._shm = SharedMemory(name=handle["shm_name"], create=False)
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
        """Return a picklable handle for :meth:`attach` in worker processes."""
        return {"shm_name": self._shm.name, "lock": self._lock}

    # ── Write operations ──────────────────────────────────────────────────────

    def add(self, solution: "Solution") -> int:
        """Add one column.

        Returns the shared slot index.
        """
        col = solution.column
        rows = [
            (int(r.index), float(r.coefficient))
            for r in col.rows
            if int(r.index) < self._n_constraints
        ]
        with self._lock:
            count = int(self._header["count"][0])
            nnz = int(self._header["nnz"][0])
            if count >= self._max_cols:
                raise RuntimeError(f"SharedPricingPool full (cols: {count}/{self._max_cols})")
            new_nnz = nnz + len(rows)
            if new_nnz > self._max_nnz:
                raise RuntimeError(
                    f"SharedPricingPool non-zero capacity exceeded ({new_nnz}/{self._max_nnz})"
                )
            self._col_costs[count] = float(col.cost)
            for k, (idx, val) in enumerate(rows):
                self._col_indices[nnz + k] = np.int32(idx)
                self._col_values[nnz + k] = val
            self._row_starts[count + 1] = np.int32(new_nnz)
            self._valid[count] = np.uint8(1)
            self._header["nnz"] = new_nnz
            self._header["count"] = count + 1
        return count

    def add_columns(self, solutions: list) -> list[int]:
        """Batch-add under a single lock acquisition."""
        if not solutions:
            return []
        costs = np.empty(len(solutions), dtype=np.float64)
        row_data: list[list[tuple[int, float]]] = []
        for k, sol in enumerate(solutions):
            costs[k] = float(sol.column.cost)
            row_data.append(
                [
                    (int(r.index), float(r.coefficient))
                    for r in sol.column.rows
                    if int(r.index) < self._n_constraints
                ]
            )
        with self._lock:
            start = int(self._header["count"][0])
            nnz_start = int(self._header["nnz"][0])
            end = start + len(solutions)
            if end > self._max_cols:
                raise RuntimeError(
                    f"SharedPricingPool: adding {len(solutions)} columns would exceed capacity"
                )
            cursor = nnz_start
            for i, col_rows in enumerate(row_data):
                slot = start + i
                self._col_costs[slot] = costs[i]
                for idx, val in col_rows:
                    self._col_indices[cursor] = np.int32(idx)
                    self._col_values[cursor] = val
                    cursor += 1
                self._row_starts[slot + 1] = np.int32(cursor)
                self._valid[slot] = np.uint8(1)
            self._header["nnz"] = cursor
            self._header["count"] = end
        return list(range(start, end))

    def invalidate(self, shared_indices: list[int]) -> None:
        """Mark columns as deleted (skip during pricing)."""
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
        """Compute reduced costs; return improving columns sorted best-first.

        Lock-free.  Uses scipy CSR SPMV when available; falls back to numpy
        bincount with a one-time warning otherwise.

        Args:
            duals: 1-D float64 LP dual values.
            threshold: Keep only columns with ``rc < threshold`` (default -1e-9).
            view_mask: Optional bool array restricting which slots are priced.

        Returns:
            ``(indices, reduced_costs)`` sorted ascending by ``reduced_costs``.
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
        else:
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
        """Zero-copy float64 view of LP costs for committed columns."""
        return self._col_costs[: self.count]

    @property
    def row_starts_view(self) -> np.ndarray:
        """Zero-copy int32 CSR row-pointer array ``(count+1,)``."""
        return self._row_starts[: self.count + 1]

    @property
    def col_indices_view(self) -> np.ndarray:
        """Zero-copy int32 constraint-index array for non-zeros."""
        return self._col_indices[: self.nnz]

    @property
    def col_values_view(self) -> np.ndarray:
        """Zero-copy float64 coefficient array for non-zeros."""
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

    # ── Lifecycle ─────────────────────────────────────────────────────────────

    def close(self) -> None:
        """Detach without destroying."""
        self._shm.close()

    def unlink(self) -> None:
        """Destroy the segment.

        Call once from the owner.
        """
        self._shm.close()
        self._shm.unlink()

    def __del__(self) -> None:
        try:
            self._shm.close()
        except Exception:  # noqa: BLE001
            pass

    def __repr__(self) -> str:
        return (
            f"SharedPricingPool(count={self.count}, nnz={self.nnz}, "
            f"active={self.active_count}, max_cols={self._max_cols}, "
            f"n_constraints={self._n_constraints})"
        )


# ── FilteredSharedPricingPool ─────────────────────────────────────────────────


class FilteredSharedPricingPool:
    """Process-local boolean mask over a :class:`SharedPricingPool`.

    Used internally by :class:`FilteredPricingPool`.  Also available for
    advanced use (e.g. creating a filtered view directly from a handle).

    Args:
        shared: The backing ``SharedPricingPool``.
        view_indices: 1-D integer array of shared slot indices to include.
            ``None`` → all currently valid columns.
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
        """Include additional slots (B&B backtrack)."""
        if len(shared_indices) > 0:
            self._mask[np.asarray(shared_indices, dtype=np.intp)] = True

    def remove_from_view(self, shared_indices: list[int] | np.ndarray) -> None:
        """Exclude slots from this view (B&B restriction)."""
        if len(shared_indices) > 0:
            self._mask[np.asarray(shared_indices, dtype=np.intp)] = False

    def price(self, duals: np.ndarray, threshold: float = -1e-9) -> tuple[np.ndarray, np.ndarray]:
        """Price only slots in this view.

        Returns sorted ``(indices, rcs)``.
        """
        return self._shared.price(duals, threshold=threshold, view_mask=self._mask)

    @property
    def view_count(self) -> int:
        """Number of slots in this view."""
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
    """Unified C++ + numpy filtered view over a :class:`PricingPool`.

    Wraps a C++ ``FilteredSolutionPool`` (for deduplication, activity tracking,
    removals) and a :class:`FilteredSharedPricingPool` (for lock-free pricing).
    Both are kept in sync automatically.

    Obtained via :meth:`PricingPool.new_filter` or by chaining
    :meth:`new_filter` on another ``FilteredPricingPool``.

    The numpy mask is built by snapshotting the C++ view at construction time
    using the vectorized ``_id_to_shared`` array on the parent — no Python
    loops.

    B&B operators (:meth:`remove_from_view`, :meth:`add_to_view`) modify only
    the numpy mask and are O(k) with zero shared-memory writes, making
    backtracking trivially cheap.
    """

    def __init__(self, parent: "PricingPool", cpp_fp: object) -> None:
        self._parent = parent
        self._cpp_fp = cpp_fp  # C++ FilteredSolutionPool (activity-filtered by remove_if)
        self._numpy_fp = self._build_numpy_filter()

    def _build_numpy_filter(self) -> FilteredSharedPricingPool:
        """Snapshot C++ view → numpy mask (vectorized, no Python loop)."""
        cpp_ids = self._cpp_fp.get_column_ids()  # np.ndarray[uint64] from C++
        if len(cpp_ids) == 0:
            # Empty C++ view → empty mask (NOT "include all").
            return FilteredSharedPricingPool(
                self._parent._shared, view_indices=np.empty(0, dtype=np.intp)
            )
        # Vectorized fancy-index: ColumnId → shared slot index.
        shared_indices = self._parent._id_to_shared[cpp_ids.astype(np.int64)]
        valid = shared_indices >= 0
        arr = shared_indices[valid].astype(np.intp)
        return FilteredSharedPricingPool(self._parent._shared, view_indices=arr)

    # ── Write operations ──────────────────────────────────────────────────────

    def add(self, solution: "Solution") -> int:
        """Add to C++ pool, shared pool, and numpy mask.

        Returns ColumnId.
        """
        cpp_id = self._cpp_fp.add(solution)
        shared_idx = self._parent._shared.add(solution)
        self._parent._id_to_shared[int(cpp_id)] = shared_idx
        self._numpy_fp.add_to_view([shared_idx])
        return cpp_id

    def add_columns(self, solutions: list) -> list[int]:
        """Batch-add to both pools.

        Returns list of ColumnIds.
        """
        cpp_ids = self._cpp_fp.add(solutions)
        shared_idxs = self._parent._shared.add_columns(solutions)
        ids_arr = np.asarray(cpp_ids, dtype=np.int64)
        sidx_arr = np.asarray(shared_idxs, dtype=np.int64)
        self._parent._id_to_shared[ids_arr] = sidx_arr
        self._numpy_fp.add_to_view(shared_idxs)
        return cpp_ids

    # ── Pricing ───────────────────────────────────────────────────────────────

    def price(self, duals: np.ndarray, threshold: float = -1e-9) -> tuple[np.ndarray, np.ndarray]:
        """Lock-free filtered pricing.

        Returns sorted ``(indices, rcs)``.
        """
        return self._numpy_fp.price(duals, threshold)

    # ── Filter narrowing ──────────────────────────────────────────────────────

    def new_filter(self, **kwargs) -> "FilteredPricingPool":
        """Create a further-narrowed view.  All kwargs forwarded to C++.

        Accepts all :meth:`PricingPool.new_filter` kwargs including activity
        filters (``min_usage_rate``, ``max_age``, ``max_last_rc``).
        """
        return FilteredPricingPool(self._parent, self._cpp_fp.new_filter(**kwargs))

    # ── Remove / invalidate ───────────────────────────────────────────────────

    def _cpp_ids_to_shared(self, cpp_ids) -> np.ndarray:
        """Vectorized ColumnId → shared_index; filters out unregistered ids."""
        arr = np.asarray(cpp_ids, dtype=np.int64)
        sidxs = self._parent._id_to_shared[arr]
        return sidxs[sidxs >= 0]

    def remove_stale(self, max_age: int, min_usage_rate: float = 0.0) -> list[int]:
        """Remove stale columns from C++ + shared pool."""
        removed = self._cpp_fp.remove_stale(max_age, min_usage_rate)
        if removed:
            sidxs = self._cpp_ids_to_shared(removed)
            self._parent._id_to_shared[np.asarray(removed, dtype=np.int64)] = -1
            self._parent._shared.invalidate(sidxs.tolist())
        return removed

    def global_remove_if(self, pred) -> list[int]:
        """Hard-delete from both pools."""
        removed = self._cpp_fp.global_remove_if(pred)
        if removed:
            sidxs = self._cpp_ids_to_shared(removed)
            self._parent._id_to_shared[np.asarray(removed, dtype=np.int64)] = -1
            self._parent._shared.invalidate(sidxs.tolist())
        return removed

    # ── B&B operators (numpy mask only) ──────────────────────────────────────

    def remove_from_view(
        self,
        *,
        arc_ids: list[int] | None = None,
        cpp_ids: list[int] | None = None,
        shared_indices: list[int] | None = None,
    ) -> None:
        """Exclude columns from this view.  Modifies numpy mask only — no shared-memory
        write, trivially backtrackable.

        Args:
            arc_ids: List of arc IDs — exclude all columns whose path uses any.
            cpp_ids: List of C++ ColumnIds to exclude.
            shared_indices: List of shared slot indices to exclude directly.
        """
        sidxs_list: list[int] = list(shared_indices or [])
        if arc_ids is not None:
            for arc_id in arc_ids:
                removed = self._cpp_fp.remove_if_arc_present(arc_id)
                if removed:
                    sidxs_list += self._cpp_ids_to_shared(removed).tolist()
        if cpp_ids is not None:
            sidxs_list += self._cpp_ids_to_shared(cpp_ids).tolist()
        if sidxs_list:
            self._numpy_fp.remove_from_view(sidxs_list)

    def add_to_view(
        self,
        *,
        cpp_ids: list[int] | None = None,
        shared_indices: list[int] | None = None,
    ) -> None:
        """Re-include columns (B&B backtrack).

        Args:
            cpp_ids: List of C++ ColumnIds to re-include.
            shared_indices: List of shared slot indices to re-include directly.
        """
        sidxs_list: list[int] = list(shared_indices or [])
        if cpp_ids is not None:
            sidxs_list += self._cpp_ids_to_shared(cpp_ids).tolist()
        if sidxs_list:
            self._numpy_fp.add_to_view(sidxs_list)

    # ── Shortcut accessors ────────────────────────────────────────────────────

    def shared(self) -> SharedPricingPool:
        """Return the underlying ``SharedPricingPool``."""
        return self._parent._shared

    def handle(self) -> dict:
        """Return the picklable worker handle (same as parent pool's handle)."""
        return self._parent.handle()

    def __getattr__(self, name: str) -> object:
        """Delegate unknown attributes to the C++ ``FilteredSolutionPool``."""
        return getattr(self._cpp_fp, name)

    def __repr__(self) -> str:
        return f"FilteredPricingPool(view_count={self._numpy_fp.view_count})"


# ── PricingPool ───────────────────────────────────────────────────────────────


class PricingPool:
    """Main column-generation pricing pool.

    Creates and manages the internal C++ ``SolutionPool`` and the
    :class:`SharedPricingPool` transparently.  All C++ pool operations (add,
    remove, activity tracking, filtering) go through the internal C++ pool;
    cross-process pricing goes through the shared memory pool.

    Example::

        pool = PricingPool(n_constraints=200, max_cols=50_000)

        # Add columns (syncs both C++ and shared pool).
        pool.add(solution)

        # Create a filtered view (one call — no separate numpy step).
        sub = pool.new_filter(forbidden_arc_ids=[10], max_age=100)
        indices, rcs = sub.price(duals)

        # Pass shared pool to workers.
        handle = pool.handle()
        shared = PricingPool.attach(handle)   # in worker process

        pool.close()

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
        # Hidden internal C++ pool.
        self._cpp_pool = _SolutionPool()
        self._cpp_fp = self._cpp_pool.new_filter()  # main unfiltered view

        self._shared = SharedPricingPool(
            n_constraints=n_constraints,
            max_cols=max_cols,
            max_nnz_per_col=max_nnz_per_col,
            lock=lock,
        )

        # Vectorized ColumnId → shared_index mapping.
        # ColumnIds are sequential starting at 1; pre-fill with -1 (= unregistered).
        # Size = max_cols + 2 to safely handle ids up to max_cols+1.
        self._id_to_shared = np.full(max_cols + 2, -1, dtype=np.int64)

    # ── Worker support ────────────────────────────────────────────────────────

    @staticmethod
    def attach(handle: dict) -> SharedPricingPool:
        """Attach to the shared pool from a worker process.

        Args:
            handle: dict returned by :meth:`handle`.

        Returns:
            A :class:`SharedPricingPool` for lock-free pricing.
        """
        return SharedPricingPool.attach(handle)

    def handle(self) -> dict:
        """Return a picklable handle for worker processes."""
        return self._shared.handle()

    def shared(self) -> SharedPricingPool:
        """Return the underlying :class:`SharedPricingPool` (master shortcut)."""
        return self._shared

    # ── Write operations ──────────────────────────────────────────────────────

    def add(self, solution: "Solution") -> int:
        """Add to both C++ and shared pool.

        Returns C++ ColumnId.
        """
        cpp_id = self._cpp_fp.add(solution)
        shared_idx = self._shared.add(solution)
        self._id_to_shared[int(cpp_id)] = shared_idx
        return cpp_id

    def add_columns(self, solutions: list) -> list[int]:
        """Batch-add.

        Returns list of C++ ColumnIds.
        """
        cpp_ids = self._cpp_fp.add(solutions)
        shared_idxs = self._shared.add_columns(solutions)
        self._id_to_shared[np.asarray(cpp_ids, dtype=np.int64)] = np.asarray(
            shared_idxs, dtype=np.int64
        )
        return cpp_ids

    # ── Pricing (unfiltered, lock-free) ──────────────────────────────────────

    def price(self, duals: np.ndarray, threshold: float = -1e-9) -> tuple[np.ndarray, np.ndarray]:
        """Price all valid columns.

        Returns sorted ``(indices, rcs)``.
        """
        return self._shared.price(duals, threshold)

    # ── Filter creation ───────────────────────────────────────────────────────

    def new_filter(self, **kwargs) -> FilteredPricingPool:
        """Create a filtered view combining C++ filter + numpy mask.

        All kwargs are forwarded to the C++ ``SolutionPool.new_filter()``.
        Activity-based args (``min_usage_rate``, ``max_age``, ``max_last_rc``)
        apply a local ``remove_if`` on the C++ view so the numpy mask and the
        C++ view are always in sync.

        Args:
            filter: Custom Python predicate ``(Solution) -> bool``.
            compulsory_rows: Column must cover all these constraint indices.
            forbidden_rows: Column must not cover any of these.
            compulsory_arc_ids: Path must use all these arcs.
            forbidden_arc_ids: Path must not use any of these arcs.
            min_usage_rate: Exclude columns priced but returned < this fraction.
            max_age: Exclude columns not returned for > this many rounds.
            max_last_rc: Exclude columns whose last reduced cost was ≥ this.

        Returns:
            A :class:`FilteredPricingPool`.
        """
        cpp_fp = self._cpp_fp.new_filter(**kwargs)
        return FilteredPricingPool(self, cpp_fp)

    # ── Remove / invalidate ───────────────────────────────────────────────────

    def _cpp_ids_to_shared(self, cpp_ids) -> np.ndarray:
        arr = np.asarray(cpp_ids, dtype=np.int64)
        sidxs = self._id_to_shared[arr]
        return sidxs[sidxs >= 0]

    def remove_stale(self, max_age: int, min_usage_rate: float = 0.0) -> list[int]:
        """Remove stale columns from both pools."""
        removed = self._cpp_fp.remove_stale(max_age, min_usage_rate)
        if removed:
            sidxs = self._cpp_ids_to_shared(removed)
            self._id_to_shared[np.asarray(removed, dtype=np.int64)] = -1
            self._shared.invalidate(sidxs.tolist())
        return removed

    def global_remove_if(self, pred) -> list[int]:
        """Hard-delete from both pools."""
        removed = self._cpp_fp.global_remove_if(pred)
        if removed:
            sidxs = self._cpp_ids_to_shared(removed)
            self._id_to_shared[np.asarray(removed, dtype=np.int64)] = -1
            self._shared.invalidate(sidxs.tolist())
        return removed

    # ── Delegation ────────────────────────────────────────────────────────────

    def __getattr__(self, name: str) -> object:
        """Delegate unknown attributes to the C++ ``FilteredSolutionPool``."""
        return getattr(self._cpp_fp, name)

    def close(self) -> None:
        """Release the shared memory segment."""
        self._shared.unlink()

    def __repr__(self) -> str:
        return f"PricingPool(shared={self._shared!r})"
