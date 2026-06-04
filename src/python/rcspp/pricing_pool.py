"""Cross-process shared pricing pool for RCSPP column generation.

Two public entry points
-----------------------

:class:`PricingPool`
    **Main pool** — the only object the master process needs.  Creates and
    manages the internal C++ ``SolutionPool`` and the ``SharedPricingPool``
    transparently.  Produces :class:`FilteredPricingPool` views via
    :meth:`~PricingPool.new_filter`.

    Workers receive a handle via :meth:`~PricingPool.handle` and call
    ``PricingPool.attach(handle)`` to get a :class:`SharedPricingPool` for
    lock-free pricing.

:class:`FilteredPricingPool`
    **Filtered view** — returned by :meth:`~PricingPool.new_filter` or by
    chaining :meth:`~FilteredPricingPool.new_filter` on an existing view.
    Wraps a C++ ``FilteredSolutionPool`` and a process-local numpy mask in one
    object; both are kept in sync automatically.

    Provides B&B operators (:meth:`~FilteredPricingPool.remove_from_view` /
    :meth:`~FilteredPricingPool.add_to_view`) that modify only the numpy mask
    (no shared-memory write, trivial backtracking).

    All C++ ``FilteredSolutionPool`` methods (``update_activity``,
    ``remove_if``, ``global_remove_if``, ``get``, ``get_all``, ``size``,
    ``pricing_count``, etc.) are forwarded transparently via ``__getattr__``.

Advanced / worker classes
--------------------------

:class:`SharedPricingPool`
    Raw CSR shared-memory pool — the only class workers need.  Attach with
    ``PricingPool.attach(handle)``.

:class:`FilteredSharedPricingPool`
    Process-local boolean mask over a :class:`SharedPricingPool`.  Used
    internally by :class:`FilteredPricingPool`; available for advanced use.

Typical column-generation loop
-------------------------------
::

    # ── Master setup ──────────────────────────────────────────────────────────
    pool   = PricingPool(n_constraints=200, max_cols=50_000)
    handle = pool.handle()   # send once to every worker process

    # ── Worker (each iteration) ───────────────────────────────────────────────
    shared         = PricingPool.attach(handle)
    indices, rcs   = shared.price(duals)   # lock-free O(nnz) scipy SPMV

    # ── Master (each iteration) ───────────────────────────────────────────────
    # Add new columns returned by workers.
    for sol in new_solutions:
        pool.add(sol)                      # syncs C++ + shared

    # Solve LP master; get basis column ids from LP solver.
    basis_col_ids = lp_solver.basis_column_ids()

    # Update C++ activity so filters can use usage_rate / age / last_rc.
    pool.update_activity(basis_col_ids)    # forwarded to C++ FilteredSolutionPool

    # Prune columns that have not been useful recently.
    pool.remove_stale(max_age=100, min_usage_rate=0.01)

    # ── B&B pricing (master) ──────────────────────────────────────────────────
    # Restrict to promising columns for a particular B&B node.
    sub          = pool.new_filter(forbidden_arc_ids=[10, 11], max_last_rc=0.0)
    indices, rcs = sub.price(duals)

    # Backtrack: undo arc restriction.
    sub.add_to_view(arc_ids=[10, 11])      # (or let sub go out of scope)

    pool.close()

Shared-memory layout
--------------------
Single ``SharedMemory`` segment, CSR format::

    [header 128 B][valid uint8][col_costs f64][row_starts i32][col_indices i32][col_values f64]

See :class:`SharedPricingPool` for detailed offsets.
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
    """Cross-process shared pricing pool with CSR storage and O(nnz) SPMV pricing.

    Stores LP column data (costs + constraint coefficients) in **CSR format**
    inside a single ``SharedMemory`` segment.  Multiple processes map the same
    segment; :meth:`price` is lock-free.  Only :meth:`add`,
    :meth:`add_columns`, and :meth:`invalidate` acquire the write lock.

    If **scipy** is installed, :meth:`price` builds a zero-copy
    ``csr_matrix`` and calls scipy's SPMV.  Without scipy a
    ``np.bincount``-based fallback is used (same asymptotic cost, slightly
    slower) and a one-time warning is emitted.

    This class is primarily used by **worker processes** that only price
    columns and never touch the C++ pool::

        # Worker process
        shared = PricingPool.attach(handle)
        indices, rcs = shared.price(duals)   # lock-free, sorted best-first

    It can also be accessed from the master via ``pool.shared()``::

        pool   = PricingPool(n_constraints=200, max_cols=50_000)
        shared = pool.shared()               # same underlying segment

    Args:
        n_constraints: Highest constraint index + 1.  Over-allocate (e.g.
            ``1000``) so future LP cuts need no reallocation — just pass a
            longer ``duals`` array.
        max_cols: Maximum number of column slots.
        max_nnz_per_col: Expected maximum non-zeros per column (default 50).
            Total capacity: ``max_nnz = max_cols × max_nnz_per_col``.
        name: ``SharedMemory`` segment name; auto-generated if ``None``.
        lock: External lock for spawn-safe multiprocessing.  Pass
            ``multiprocessing.Manager().Lock()`` when starting workers with
            the ``spawn`` start method; the default ``Lock()`` works with
            ``fork``.
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

    # ── Attach / handle ───────────────────────────────────────────────────────

    @classmethod
    def attach(cls, handle: dict) -> "SharedPricingPool":
        """Attach to an existing pool from another process (zero-copy).

        Reads all layout parameters from the shared header so the segment is
        self-describing — the handle only needs the name and lock::

            # Worker
            shared = PricingPool.attach(handle)
            indices, rcs = shared.price(duals)

        Args:
            handle: dict returned by :meth:`handle`.

        Returns:
            A ``SharedPricingPool`` backed by the same memory.
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
        """Return a picklable dict for passing to worker processes.

        The dict contains the ``SharedMemory`` segment name and the write
        lock.  Workers call ``PricingPool.attach(handle)`` or
        ``SharedPricingPool.attach(handle)``::

            handle = pool.handle()
            # send handle to workers via multiprocessing.Pool.starmap, etc.

        Returns:
            ``{"shm_name": str, "lock": Lock}``
        """
        return {"shm_name": self._shm.name, "lock": self._lock}

    # ── Write operations (write-locked) ──────────────────────────────────────

    def add(self, solution: "Solution") -> int:
        """Add one column to the shared pool.

        Acquires the write lock.  The column is committed atomically: data is
        written before the valid flag and count are updated, so workers never
        see a partial write::

            shared_idx = pool.add(solution)
            # shared_idx can later be passed to invalidate()

        Args:
            solution: Solution whose ``column`` attribute contains the LP cost
                and row coefficients.

        Returns:
            The shared slot index (0-based).  Use with :meth:`invalidate` to
            mark the column as deleted later.

        Raises:
            RuntimeError: Pool is full (columns or non-zeros).
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
        """Batch-add multiple solutions under a single lock acquisition.

        More efficient than calling :meth:`add` in a loop for large batches
        because only one lock acquire/release is needed::

            shared_idxs = pool.add_columns([sol1, sol2, sol3])

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
        """Mark column slots as deleted so they are skipped during :meth:`price`.

        The column data remains in memory (slots are never reused); only the
        ``valid`` flag is cleared::

            removed_ids = pool.remove_stale(max_age=100)
            # PricingPool.remove_stale calls invalidate() automatically.
            # For direct use:
            pool.shared().invalidate([slot_idx])

        Args:
            shared_indices: Shared slot indices previously returned by
                :meth:`add`.  An empty list is a no-op.
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
        """Compute reduced costs and return improving columns sorted best-first.

        **Lock-free.**  Reads a snapshot of ``count`` from the header, builds
        a ``scipy.sparse.csr_matrix`` view over the committed CSR data
        (zero-copy), and performs a single BLAS SPMV::

            rc[i] = col_costs[i] - A[i, :] @ duals

        Results are filtered to ``rc < threshold`` and sorted ascending
        (most negative — most improving — first).

        Dynamic cuts: when new LP rows (cuts) are added, pass a longer
        ``duals`` vector.  Old columns have coefficient 0 at new indices
        (zero-filled at allocation) so they are handled correctly::

            # Worker
            indices, rcs = shared.price(duals)
            # indices: shared slot positions sorted by rcs (best first)
            # rcs:     reduced costs, all < threshold (default -1e-9)

        Args:
            duals: 1-D float64 LP dual values, indexed by constraint.
            threshold: Keep only columns with ``rc < threshold``.
                Default ``-1e-9`` excludes near-zero columns.
            view_mask: Optional boolean ``ndarray`` of shape ``(max_cols,)``
                restricting which slots are considered.  Used internally by
                :class:`FilteredSharedPricingPool`; pass ``None`` to price
                all valid columns.

        Returns:
            ``(indices, reduced_costs)`` — 1-D arrays of the same length,
            sorted ascending by ``reduced_costs``.
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

    # ── Zero-copy numpy views ─────────────────────────────────────────────────

    @property
    def col_costs_view(self) -> np.ndarray:
        """Zero-copy float64 view of LP costs for committed columns.

        Shape ``(count,)``.  Strided (column-major stride in the CSR matrix);
        copy explicitly if contiguous memory is required.
        """
        return self._col_costs[: self.count]

    @property
    def row_starts_view(self) -> np.ndarray:
        """Zero-copy int32 CSR row-pointer array.

        Shape ``(count + 1,)``.  ``row_starts[i]`` is the start of column
        ``i``'s non-zeros in :attr:`col_indices_view` / :attr:`col_values_view`.
        """
        return self._row_starts[: self.count + 1]

    @property
    def col_indices_view(self) -> np.ndarray:
        """Zero-copy int32 constraint-index array for all non-zeros.

        Shape ``(nnz,)``.
        """
        return self._col_indices[: self.nnz]

    @property
    def col_values_view(self) -> np.ndarray:
        """Zero-copy float64 coefficient array for all non-zeros.

        Shape ``(nnz,)``.  Parallel to :attr:`col_indices_view`.
        """
        return self._col_values[: self.nnz]

    @property
    def valid_view(self) -> np.ndarray:
        """Zero-copy uint8 validity flag array.

        Shape ``(max_cols,)``.  ``1`` = active, ``0`` = deleted/empty.
        """
        return self._valid

    # ── Counters ──────────────────────────────────────────────────────────────

    @property
    def count(self) -> int:
        """Total committed column slots (including invalidated ones)."""
        return int(self._header["count"][0])

    @property
    def nnz(self) -> int:
        """Total committed non-zeros across all columns."""
        return int(self._header["nnz"][0])

    @property
    def active_count(self) -> int:
        """Number of currently active (non-invalidated) columns."""
        n = self.count
        return int(self._valid[:n].sum()) if n > 0 else 0

    # ── Lifecycle ─────────────────────────────────────────────────────────────

    def close(self) -> None:
        """Detach from the shared segment without destroying it.

        Call from **worker** processes when pricing is done.  The segment remains alive
        as long as the owning process holds it open.
        """
        self._shm.close()

    def unlink(self) -> None:
        """Close and destroy the shared segment.

        Call **once** from the owning (master) process when the pool is no
        longer needed.  Equivalent to :meth:`PricingPool.close`.
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

    Holds a numpy ``bool_`` array (in the calling process's heap, **not** in
    shared memory) that restricts which column slots :meth:`price` considers.

    Used internally by :class:`FilteredPricingPool`.  Also available directly
    for advanced use — e.g. building a filtered view from a handle without
    touching the C++ pool::

        shared = PricingPool.attach(handle)
        fpool  = FilteredSharedPricingPool(shared, view_indices=allowed_idx)
        indices, rcs = fpool.price(duals)

    Args:
        shared: The backing ``SharedPricingPool``.
        view_indices: 1-D integer array of shared slot indices to include.
            Pass an **empty array** for an empty view.
            Pass ``None`` to include all currently valid columns.
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
        """Include additional slots in this filter view.

        Typically called during B&B backtracking to restore columns that were
        previously excluded::

            fpool.add_to_view([slot_i, slot_j])

        Args:
            shared_indices: Slot indices to include (list or 1-D array).
        """
        if len(shared_indices) > 0:
            self._mask[np.asarray(shared_indices, dtype=np.intp)] = True

    def remove_from_view(self, shared_indices: list[int] | np.ndarray) -> None:
        """Exclude slots from this filter view.

        Modifies only the local mask — no shared-memory write, O(k)::

            fpool.remove_from_view([slot_i, slot_j])

        Args:
            shared_indices: Slot indices to exclude (list or 1-D array).
        """
        if len(shared_indices) > 0:
            self._mask[np.asarray(shared_indices, dtype=np.intp)] = False

    def price(self, duals: np.ndarray, threshold: float = -1e-9) -> tuple[np.ndarray, np.ndarray]:
        """Price only the slots in this filter view.

        Delegates to :meth:`SharedPricingPool.price` with
        ``view_mask=self._mask``::

            indices, rcs = fpool.price(duals)

        Args:
            duals: 1-D float64 LP dual values.
            threshold: Keep only columns with ``rc < threshold``.

        Returns:
            ``(indices, reduced_costs)`` sorted ascending.
        """
        return self._shared.price(duals, threshold=threshold, view_mask=self._mask)

    @property
    def view_count(self) -> int:
        """Number of column slots currently in this view."""
        n = self._shared.count
        return int(self._mask[:n].sum()) if n > 0 else 0

    @property
    def mask(self) -> np.ndarray:
        """Zero-copy boolean mask array, shape ``(max_cols,)``."""
        return self._mask

    def __repr__(self) -> str:
        return f"FilteredSharedPricingPool(view_count={self.view_count})"


# ── FilteredPricingPool ───────────────────────────────────────────────────────


class FilteredPricingPool:
    """Unified C++ filter + numpy mask view over a :class:`PricingPool`.

    Combines a C++ ``FilteredSolutionPool`` (for deduplication, activity
    tracking, and structural removals) with a process-local
    :class:`FilteredSharedPricingPool` (for lock-free pricing).  Both are
    kept in sync automatically.

    Obtain via :meth:`PricingPool.new_filter` or chain
    :meth:`new_filter` on an existing view::

        sub  = pool.new_filter(forbidden_arc_ids=[10], max_age=100)
        sub2 = sub.new_filter(compulsory_rows=[0])   # further narrow

    All C++ ``FilteredSolutionPool`` methods are forwarded transparently via
    ``__getattr__``.  The most important forwarded methods are:

    * ``update_activity(basis_col_ids)`` — call after each LP solve to update
      ``age``, ``use_count``, and ``last_reduced_cost`` for each column based
      on LP basis membership::

            basis_col_ids = lp_solver.basis_column_ids()  # list[ColumnId]
            sub.update_activity(basis_col_ids)

    * ``get(col_id)`` — retrieve a ``Solution`` by ColumnId
    * ``get_all()`` — retrieve all ``(ColumnId, Solution, ColumnActivity)``
    * ``size()`` / ``len(sub)`` — number of columns in the C++ view
    * ``pricing_count()`` — total C++ price() calls on this view
    * ``remove_if(pred)`` — local removal by predicate (B&B-safe)
    * ``global_remove_if(pred)`` — hard delete from the main pool
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
        """Add a column to the C++ pool, the shared pool, and this numpy mask.

        The column is registered in all three places atomically from the
        perspective of the C++ deduplication guarantee::

            col_id = sub.add(solution)
            # col_id is a C++ ColumnId usable in update_activity(), etc.

        Args:
            solution: Solution to add.

        Returns:
            C++ ColumnId assigned by the pool.
        """
        cpp_id = self._cpp_fp.add(solution)
        shared_idx = self._parent._shared.add(solution)
        self._parent._id_to_shared[int(cpp_id)] = shared_idx
        self._numpy_fp.add_to_view([shared_idx])
        return cpp_id

    def add_columns(self, solutions: list) -> list[int]:
        """Batch-add multiple solutions.

        More efficient than calling :meth:`add` in a loop::

            col_ids = sub.add_columns([sol1, sol2, sol3])

        Args:
            solutions: List of Solution objects.

        Returns:
            List of C++ ColumnIds.
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
        """Lock-free pricing restricted to this filter view.

        Uses the numpy mask to skip columns excluded by the C++ filter or by
        B&B operators.  Returns columns sorted by reduced cost, best first::

            duals          = np.array([...])
            indices, rcs   = sub.price(duals)
            # indices are shared slot positions; rcs < threshold (default -1e-9)

        Args:
            duals: 1-D float64 LP dual values.
            threshold: Keep only columns with ``rc < threshold``.

        Returns:
            ``(indices, reduced_costs)`` sorted ascending by ``reduced_costs``.
        """
        return self._numpy_fp.price(duals, threshold)

    # ── Filter narrowing ──────────────────────────────────────────────────────

    def new_filter(self, **kwargs) -> "FilteredPricingPool":
        """Create a further-narrowed view from this view.

        Entries must pass **both** this view's existing filter and the new
        constraints.  All kwargs are forwarded to the C++ ``new_filter()``
        and include both solution-level and activity-level args::

            sub2 = sub.new_filter(
                forbidden_arc_ids=[20],   # additional arc restriction
                max_last_rc=0.0,          # only columns last priced negative
            )
            indices, rcs = sub2.price(duals)

        See :meth:`PricingPool.new_filter` for the full list of kwargs.

        Returns:
            A new :class:`FilteredPricingPool`.
        """
        return FilteredPricingPool(self._parent, self._cpp_fp.new_filter(**kwargs))

    # ── Activity update (forwarded) ───────────────────────────────────────────
    #
    # update_activity(basis_col_ids) is forwarded automatically via __getattr__
    # to the C++ FilteredSolutionPool.  It updates age / last_reduced_cost /
    # use_count for columns in this view based on LP basis membership:
    #
    #   basis_col_ids = lp_solver.basis_column_ids()   # list[ColumnId]
    #   sub.update_activity(basis_col_ids)
    #
    # Columns in basis_col_ids: age reset to 0, last_was_negative = True.
    # Other columns in this view: age incremented by 1.
    # Columns outside this view: untouched.

    # ── Remove / invalidate ───────────────────────────────────────────────────

    def remove_stale(self, max_age: int, min_usage_rate: float = 0.0) -> list[int]:
        """Remove columns that are too old or too rarely used.

        Removes from the C++ view (local — does not affect other filtered
        views) and invalidates the corresponding shared slots so workers stop
        pricing them::

            removed_ids = sub.remove_stale(max_age=100, min_usage_rate=0.01)

        A column is removed if ``age > max_age`` OR
        (``priced_count > 0`` AND ``usage_rate < min_usage_rate``).

        Args:
            max_age: Remove if ``age > max_age``.
            min_usage_rate: Remove if ``usage_rate < this`` (once priced).

        Returns:
            List of removed C++ ColumnIds.
        """
        removed = self._cpp_fp.remove_stale(max_age, min_usage_rate)
        if removed:
            sidxs = self._cpp_ids_to_shared(removed)
            self._parent._id_to_shared[np.asarray(removed, dtype=np.int64)] = -1
            self._parent._shared.invalidate(sidxs.tolist())
        return removed

    def global_remove_if(self, pred) -> list[int]:
        """Hard-delete columns from the main pool (propagates to all views).

        Use sparingly — prefer :meth:`remove_stale` or the local
        ``remove_if`` (forwarded via ``__getattr__``) for B&B::

            removed = sub.global_remove_if(
                lambda col_id, sol, act: act.age > 200
            )

        Args:
            pred: Callable ``(ColumnId, Solution, ColumnActivity) -> bool``.

        Returns:
            List of removed C++ ColumnIds.
        """
        removed = self._cpp_fp.global_remove_if(pred)
        if removed:
            sidxs = self._cpp_ids_to_shared(removed)
            self._parent._id_to_shared[np.asarray(removed, dtype=np.int64)] = -1
            self._parent._shared.invalidate(sidxs.tolist())
        return removed

    def _cpp_ids_to_shared(self, cpp_ids) -> np.ndarray:
        """Vectorized ColumnId → shared_index; filters out unregistered ids."""
        arr = np.asarray(cpp_ids, dtype=np.int64)
        sidxs = self._parent._id_to_shared[arr]
        return sidxs[sidxs >= 0]

    # ── B&B operators (numpy mask only) ──────────────────────────────────────

    def remove_from_view(
        self,
        *,
        arc_ids: list[int] | None = None,
        cpp_ids: list[int] | None = None,
        shared_indices: list[int] | None = None,
    ) -> None:
        """Exclude columns from this view without touching shared memory.

        Modifies only the local numpy mask — O(k), no locks, trivially
        backtrackable with :meth:`add_to_view`.  All arguments are optional
        and can be combined::

            # Exclude by arc (any column whose path uses these arcs):
            sub.remove_from_view(arc_ids=[10, 11])

            # Exclude by ColumnId (e.g. columns fixed to 0 in B&B):
            sub.remove_from_view(cpp_ids=[col_id_1, col_id_2])

            # Combine:
            sub.remove_from_view(arc_ids=[10], cpp_ids=[col_id_3])

        Args:
            arc_ids: List of arc IDs — exclude all columns whose path
                traverses any of these arcs.
            cpp_ids: List of C++ ColumnIds to exclude directly.
            shared_indices: List of shared slot indices to exclude directly
                (advanced — prefer ``arc_ids`` or ``cpp_ids``).
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
        """Re-include columns that were excluded by :meth:`remove_from_view`.

        Used for B&B backtracking — restores excluded columns to the numpy
        mask in O(k)::

            # Backtrack by ColumnId (the ids returned by add()):
            sub.add_to_view(cpp_ids=[col_id_1, col_id_2])

            # Backtrack by shared slot index:
            sub.add_to_view(shared_indices=[slot_0, slot_1])

        .. note::
            There is no ``arc_ids`` argument here — to undo an arc-based
            restriction, save the ColumnIds returned by :meth:`remove_from_view`
            and pass them back::

                sub.remove_from_view(arc_ids=[10])
                # ... B&B subtree ...
                sub.add_to_view(cpp_ids=previously_removed_ids)

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
        """Return the underlying :class:`SharedPricingPool`.

        Same object as ``pool.shared()`` — useful when you only have the
        filtered pool reference and need direct access to the shared segment::

            shared_pool = sub.shared()
            print(shared_pool.count, shared_pool.active_count)
        """
        return self._parent._shared

    def handle(self) -> dict:
        """Return the picklable worker handle.

        Identical to :meth:`PricingPool.handle` — all filtered views share the
        same underlying ``SharedPricingPool``.  Workers always price against
        the full shared pool and apply local filters on their side::

            handle = sub.handle()   # same as pool.handle()
        """
        return self._parent.handle()

    def __getattr__(self, name: str) -> object:
        """Delegate to the underlying C++ ``FilteredSolutionPool``.

        This makes all C++ pool methods available on the filtered pool::

            sub.update_activity(basis_col_ids)   # update age / use_count
            sub.get(col_id)                      # fetch one Solution
            n = len(sub)                         # number of columns in view
            sub.remove_if(lambda cid, sol, act: act.age > 50)
        """
        return getattr(self._cpp_fp, name)

    def __repr__(self) -> str:
        return f"FilteredPricingPool(view_count={self._numpy_fp.view_count})"


# ── PricingPool ───────────────────────────────────────────────────────────────


class PricingPool:
    """Main column-generation pricing pool.

    Creates and manages the internal C++ ``SolutionPool`` and the
    :class:`SharedPricingPool` transparently.  This is the **only object the
    master process needs** for a full CG / B&B workflow.

    All C++ ``FilteredSolutionPool`` methods are forwarded via
    ``__getattr__``, in particular:

    * ``update_activity(basis_col_ids)`` — call after each LP solve::

            pool.update_activity(lp.basis_column_ids())

    * ``get(col_id)``, ``get_all()``, ``size()``, ``pricing_count()``
    * ``remove_if(pred)``, ``global_remove_if(pred)``

    Full CG loop example::

        pool   = PricingPool(n_constraints=200, max_cols=50_000)
        handle = pool.handle()                 # send to workers once

        for iteration in range(max_iter):
            # Workers price (lock-free, parallel).
            indices, rcs = pool.price(duals)   # or: shared.price(duals) in worker

            # Master adds the new solutions found by workers.
            for sol in new_solutions:
                pool.add(sol)

            # Solve LP master; update activity.
            basis_ids = lp_solver.basis_column_ids()
            pool.update_activity(basis_ids)     # forwarded to C++ pool

            # Prune stale columns.
            pool.remove_stale(max_age=100, min_usage_rate=0.01)

        pool.close()

    B&B filtered pricing::

        sub          = pool.new_filter(forbidden_arc_ids=[arc_id], max_last_rc=0.0)
        indices, rcs = sub.price(duals)

        # Backtrack:
        sub.add_to_view(arc_ids=[arc_id])

    Args:
        n_constraints: Constraint capacity (over-allocate, e.g. ``1000``).
        max_cols: Column capacity.
        max_nnz_per_col: Maximum non-zeros per column (default 50).
        lock: External lock for spawn-safe multiprocessing (see
            :class:`SharedPricingPool`).
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
        # ColumnIds are sequential from 1; pre-fill with -1 (= unregistered).
        self._id_to_shared = np.full(max_cols + 2, -1, dtype=np.int64)

    # ── Worker support ────────────────────────────────────────────────────────

    @staticmethod
    def attach(handle: dict) -> SharedPricingPool:
        """Attach to the shared pool from a worker process.

        This is the **only method workers need** — they never touch the C++
        pool::

            # Worker process
            shared = PricingPool.attach(handle)
            indices, rcs = shared.price(duals)
            shared.close()

        Args:
            handle: dict returned by :meth:`handle`.

        Returns:
            A :class:`SharedPricingPool` for lock-free pricing.
        """
        return SharedPricingPool.attach(handle)

    def handle(self) -> dict:
        """Return a picklable handle for worker processes.

        Pass this to workers; they call ``PricingPool.attach(handle)``::

            handle = pool.handle()
            # send via multiprocessing queue, starmap arg, etc.

        Returns:
            ``{"shm_name": str, "lock": Lock}``
        """
        return self._shared.handle()

    def shared(self) -> SharedPricingPool:
        """Return the underlying :class:`SharedPricingPool` (master shortcut).

        Equivalent to ``PricingPool.attach(pool.handle())`` but without
        re-attaching::

            shared = pool.shared()
            print(shared.count, shared.active_count)
        """
        return self._shared

    # ── Write operations ──────────────────────────────────────────────────────

    def add(self, solution: "Solution") -> int:
        """Add a solution to both the C++ pool and the shared pool.

        Returns the C++ ColumnId which can be used in ``update_activity``,
        ``get``, filters, and B&B operators::

            col_id = pool.add(solution)
            pool.update_activity([col_id])   # mark as basis member

        Args:
            solution: Solution with a populated ``column`` attribute.

        Returns:
            C++ ColumnId (sequential integer starting at 1).
        """
        cpp_id = self._cpp_fp.add(solution)
        shared_idx = self._shared.add(solution)
        self._id_to_shared[int(cpp_id)] = shared_idx
        return cpp_id

    def add_columns(self, solutions: list) -> list[int]:
        """Batch-add multiple solutions under a single lock acquisition.

        More efficient than calling :meth:`add` in a loop::

            col_ids = pool.add_columns([sol1, sol2, sol3])

        Args:
            solutions: List of Solution objects.

        Returns:
            List of C++ ColumnIds.
        """
        cpp_ids = self._cpp_fp.add(solutions)
        shared_idxs = self._shared.add_columns(solutions)
        self._id_to_shared[np.asarray(cpp_ids, dtype=np.int64)] = np.asarray(
            shared_idxs, dtype=np.int64
        )
        return cpp_ids

    # ── Pricing ───────────────────────────────────────────────────────────────

    def price(self, duals: np.ndarray, threshold: float = -1e-9) -> tuple[np.ndarray, np.ndarray]:
        """Price all valid columns (unfiltered, lock-free).

        Returns shared slot indices and reduced costs, sorted ascending::

            indices, rcs = pool.price(duals)
            # indices: shared pool slot positions, sorted best-first
            # rcs:     reduced costs (all < threshold = -1e-9 by default)

        For **filtered pricing** (B&B), use :meth:`new_filter` instead::

            sub          = pool.new_filter(forbidden_arc_ids=[10])
            indices, rcs = sub.price(duals)

        Args:
            duals: 1-D float64 LP dual values.
            threshold: Keep only columns with ``rc < threshold``.

        Returns:
            ``(indices, reduced_costs)`` sorted ascending by reduced cost.
        """
        return self._shared.price(duals, threshold)

    # ── Filter creation ───────────────────────────────────────────────────────

    def new_filter(self, **kwargs) -> FilteredPricingPool:
        """Create a filtered pricing view combining C++ filter + numpy mask.

        All kwargs are forwarded to the C++ ``SolutionPool.new_filter()``.
        Activity-based args apply a local C++ ``remove_if()`` immediately so
        the C++ view and the numpy mask are always in sync.

        New columns added after the filter is created are **not**
        automatically included.  Re-create the filter or call
        ``sub.add(solution)`` explicitly.

        Accepted kwargs:

        * ``filter``: Python predicate ``(Solution) -> bool``
        * ``compulsory_rows``: Column must cover all these constraint indices
        * ``forbidden_rows``: Column must not cover any of these
        * ``compulsory_arc_ids``: Path must use all these arc IDs
        * ``forbidden_arc_ids``: Path must not use any of these arc IDs
        * ``min_usage_rate``: Exclude columns returned < this fraction of
          their price() calls (only applied when ``priced_count > 0``)
        * ``max_age``: Exclude columns not returned for > this many rounds
          (i.e. ``age > max_age`` after ``update_activity`` calls)
        * ``max_last_rc``: Exclude columns whose last reduced cost was ≥ this
          (only useful after at least one C++ ``price()`` call)

        Example — B&B node filter::

            sub = pool.new_filter(
                forbidden_arc_ids=[arc_id],   # structural: no arc arc_id
                max_last_rc=0.0,              # only columns last priced negative
                min_usage_rate=0.01,          # discard rarely-returned columns
            )
            indices, rcs = sub.price(duals)

        Returns:
            A :class:`FilteredPricingPool`.
        """
        cpp_fp = self._cpp_fp.new_filter(**kwargs)
        return FilteredPricingPool(self, cpp_fp)

    # ── Activity update (forwarded via __getattr__) ───────────────────────────
    #
    # pool.update_activity(basis_col_ids) is forwarded to the C++ pool.
    # basis_col_ids is a list[ColumnId] returned by the LP solver.
    #
    # Effect on each column in the pool's C++ view:
    #   in basis_col_ids → age = 0, last_was_negative = True
    #   not in basis     → age += 1
    #
    # Example:
    #   basis_ids = lp_solver.basis_column_ids()   # list[int] of ColumnIds
    #   pool.update_activity(basis_ids)

    # ── Remove / invalidate ───────────────────────────────────────────────────

    def remove_stale(self, max_age: int, min_usage_rate: float = 0.0) -> list[int]:
        """Remove columns that are too old or too rarely used from both pools.

        A column is removed if ``age > max_age`` OR
        (``priced_count > 0`` AND ``usage_rate < min_usage_rate``).

        Removal is **local** to the main filter view (does not affect other
        ``FilteredPricingPool`` views).  The shared pool slots are
        invalidated so workers stop pricing them::

            removed_ids = pool.remove_stale(max_age=100, min_usage_rate=0.01)

        Args:
            max_age: Remove if ``age > max_age``.
            min_usage_rate: Remove if ``usage_rate < this`` (once priced).

        Returns:
            List of removed C++ ColumnIds.
        """
        removed = self._cpp_fp.remove_stale(max_age, min_usage_rate)
        if removed:
            sidxs = self._cpp_ids_to_shared(removed)
            self._id_to_shared[np.asarray(removed, dtype=np.int64)] = -1
            self._shared.invalidate(sidxs.tolist())
        return removed

    def global_remove_if(self, pred) -> list[int]:
        """Hard-delete columns from the main pool; propagates to all views.

        Use sparingly — prefer :meth:`remove_stale` or a local ``remove_if``
        via a :class:`FilteredPricingPool`::

            pool.global_remove_if(lambda cid, sol, act: act.age > 500)

        Args:
            pred: Callable ``(ColumnId, Solution, ColumnActivity) -> bool``.

        Returns:
            List of removed C++ ColumnIds.
        """
        removed = self._cpp_fp.global_remove_if(pred)
        if removed:
            sidxs = self._cpp_ids_to_shared(removed)
            self._id_to_shared[np.asarray(removed, dtype=np.int64)] = -1
            self._shared.invalidate(sidxs.tolist())
        return removed

    def _cpp_ids_to_shared(self, cpp_ids) -> np.ndarray:
        arr = np.asarray(cpp_ids, dtype=np.int64)
        sidxs = self._id_to_shared[arr]
        return sidxs[sidxs >= 0]

    # ── Delegation ────────────────────────────────────────────────────────────

    def __getattr__(self, name: str) -> object:
        """Delegate to the underlying C++ ``FilteredSolutionPool``.

        Exposes the full C++ pool API including::

            pool.update_activity(basis_col_ids)
            pool.get(col_id)
            pool.get_all()          # → list[(ColumnId, Solution, ColumnActivity)]
            pool.size()             # or len(pool) — number of columns
            pool.pricing_count()    # total C++ price() calls
            pool.remove_if(pred)    # local removal by predicate
        """
        return getattr(self._cpp_fp, name)

    # ── Lifecycle ─────────────────────────────────────────────────────────────

    def close(self) -> None:
        """Release the shared memory segment.

        Call when the pool is no longer needed.  Worker processes should call
        ``shared.close()`` on their attached :class:`SharedPricingPool`
        before the master calls this::

            pool.close()   # master; destroys shared segment
        """
        self._shared.unlink()

    def __repr__(self) -> str:
        return f"PricingPool(shared={self._shared!r})"
