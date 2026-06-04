"""Cross-process shared pricing pool for RCSPP column generation.

Memory layout (single SharedMemory segment)
-------------------------------------------
Offset 0 … HEADER_BYTES-1:
    Structured header — see _HEADER_DTYPE below.
    Written once at construction; read-only after that.

Offset HEADER_BYTES … HEADER_BYTES+max_cols-1:
    ``valid[max_cols]`` — uint8 flag per column slot.
    0 = empty or invalidated, 1 = active.
    Written under lock; read lock-free during pricing.

Offset <aligned> … end:
    ``matrix[max_cols, n_constraints+1]`` — float64, C-contiguous.
    ``matrix[i, 0]``     = column i's LP cost (structural, from arc costs).
    ``matrix[i, j+1]``   = column i's coefficient for constraint j.
    Written under lock; read lock-free during pricing.

Alignment
---------
The matrix must start on an 8-byte boundary (float64 requirement).  The
``valid`` region is padded up to the next multiple of 8 before the matrix
begins.  The matrix itself is naturally aligned because the header is a
multiple of 8 bytes and the pad ensures no fractional float64 slot.

Pricing formula (lock-free)
---------------------------
``rc[i] = matrix[i, 0] - matrix[i, 1:n_duals+1] @ duals[:n_duals]``

Computed as a single BLAS DGEMV on the full (n × n_constraints) coefficient
sub-matrix, then filtered by validity and threshold.  No fancy (boolean-index)
copies — all reads go directly to the shared buffer.

Dynamic cuts
------------
``n_constraints`` is over-allocated at creation (e.g. 1000).  When a new cut
is added its row index is just a new dimension in the dual vector.  Old columns
have coefficient 0 at that index (zero-filled at allocation), so no
reallocation is ever needed — just pass a longer ``duals`` array.
"""

from __future__ import annotations

from multiprocessing import Lock
from multiprocessing.shared_memory import SharedMemory
from typing import TYPE_CHECKING

import numpy as np

if TYPE_CHECKING:
    from rcspp._core.graph import Solution

# ── Shared-memory header ──────────────────────────────────────────────────────
# 5 × uint64 = 40 bytes, padded to 64 (one cache line) for clean alignment.
_HEADER_DTYPE = np.dtype(
    [
        ("count", np.uint64),  # number of committed column slots
        ("n_constraints", np.uint64),  # max constraint index + 1
        ("max_cols", np.uint64),  # capacity
        ("valid_offset", np.uint64),  # byte offset of the valid[] region
        ("matrix_offset", np.uint64),  # byte offset of the matrix region
    ]
)
_HEADER_BYTES = 64  # one cache line — header fits in 40, leave 24 spare


def _align_up(n: int, align: int) -> int:
    """Round ``n`` up to the nearest multiple of ``align``."""
    return (n + align - 1) & ~(align - 1)


class SharedPricingPool:
    """Cross-process shared pricing pool backed by a single SharedMemory segment.

    ``price(duals)`` is **lock-free**: it reads the committed column count, valid
    flags, and LP matrix directly from shared memory without acquiring any lock,
    then performs a single BLAS DGEMV on the full active region.

    ``add(solution)`` and ``invalidate(indices)`` are write-locked.

    Args:
        n_constraints: Highest constraint index that will appear + 1.  Over-allocate
            (e.g. 1000) to leave room for future cuts without reallocation.
        max_cols: Maximum number of column slots.  Determines shared memory size.
        name: Explicit name for the SharedMemory segment; auto-generated if None.
        lock: External lock object (e.g. ``Manager().Lock()`` for spawn-safe
            multiprocessing).  A plain ``Lock()`` is created if None.
    """

    def __init__(
        self,
        n_constraints: int,
        max_cols: int = 50_000,
        name: str | None = None,
        lock: object | None = None,
    ) -> None:
        self._n_constraints = int(n_constraints)
        self._max_cols = int(max_cols)
        self._lock = lock if lock is not None else Lock()

        # Layout:
        #   [header:          _HEADER_BYTES bytes]
        #   [valid:           max_cols bytes, padded to 8-byte boundary]
        #   [matrix:          max_cols × (n_constraints+1) × 8 bytes]
        valid_offset = _HEADER_BYTES
        # Pad so the matrix starts on an 8-byte (float64) boundary.
        matrix_offset = _align_up(valid_offset + max_cols, 8)
        matrix_bytes = max_cols * (n_constraints + 1) * 8
        total = matrix_offset + matrix_bytes

        self._shm = SharedMemory(name=name, create=True, size=total)
        # Zero-fill (all valid flags = 0, all matrix slots = 0.0).
        self._shm.buf[:total] = b"\x00" * total

        # Write header.
        header = np.ndarray((1,), dtype=_HEADER_DTYPE, buffer=self._shm.buf)
        header["count"] = 0
        header["n_constraints"] = n_constraints
        header["max_cols"] = max_cols
        header["valid_offset"] = valid_offset
        header["matrix_offset"] = matrix_offset

        self._valid_offset = valid_offset
        self._matrix_offset = matrix_offset
        self._init_views()

    def _init_views(self) -> None:
        """Build numpy views over the shared buffer.

        Called after attaching.
        """
        buf = self._shm.buf
        self._header = np.ndarray((1,), dtype=_HEADER_DTYPE, buffer=buf)
        self._valid = np.ndarray(
            (self._max_cols,),
            dtype=np.uint8,
            buffer=buf,
            offset=self._valid_offset,
        )
        # Full matrix: column i → [col_cost | row_coef_0 … row_coef_{n-1}]
        self._matrix = np.ndarray(
            (self._max_cols, self._n_constraints + 1),
            dtype=np.float64,
            buffer=buf,
            offset=self._matrix_offset,
        )

    # ── Attach from another process ───────────────────────────────────────────

    @classmethod
    def attach(cls, handle: dict) -> "SharedPricingPool":
        """Attach to an existing pool from a worker process.

        Args:
            handle: dict returned by :meth:`handle`.

        Returns:
            A ``SharedPricingPool`` sharing the same memory (zero-copy).
        """
        obj = object.__new__(cls)
        obj._lock = handle["lock"]
        obj._shm = SharedMemory(name=handle["shm_name"], create=False)

        # Read layout from the header (authoritative source of offsets).
        hdr = np.ndarray((1,), dtype=_HEADER_DTYPE, buffer=obj._shm.buf)
        obj._n_constraints = int(hdr["n_constraints"][0])
        obj._max_cols = int(hdr["max_cols"][0])
        obj._valid_offset = int(hdr["valid_offset"][0])
        obj._matrix_offset = int(hdr["matrix_offset"][0])
        obj._init_views()
        return obj

    def handle(self) -> dict:
        """Picklable handle — pass to worker processes via ``attach()``.

        Returns:
            dict with ``shm_name`` and ``lock``.  All layout parameters are
            read from the shared header on attach, so no layout info needs to
            travel through the handle.
        """
        return {"shm_name": self._shm.name, "lock": self._lock}

    # ── Write operations ──────────────────────────────────────────────────────

    def add(self, solution: "Solution") -> int:
        """Add a solution's LP column (cost + row coefficients) to the pool.

        Acquires the write lock.  The column is committed atomically: the
        valid flag and count are set last so workers never see a partial write.

        Args:
            solution: Solution with a populated ``column`` attribute.

        Returns:
            Shared index of the new slot (use with :meth:`invalidate`).

        Raises:
            RuntimeError: Pool is full.
        """
        col = solution.column

        # Build row vector with numpy — one assignment per non-zero constraint.
        row_vec = np.zeros(self._n_constraints, dtype=np.float64)
        for row in col.rows:
            if row.index < self._n_constraints:
                row_vec[int(row.index)] = float(row.coefficient)

        with self._lock:
            count = int(self._header["count"][0])
            if count >= self._max_cols:
                raise RuntimeError(f"SharedPricingPool is full ({count}/{self._max_cols})")
            # Write data before setting valid/count (no partial-write visibility).
            self._matrix[count, 0] = float(col.cost)
            self._matrix[count, 1:] = row_vec
            self._valid[count] = np.uint8(1)  # mark active
            self._header["count"] = count + 1  # commit
        return count

    def add_columns(self, solutions: list) -> list[int]:
        """Batch-add multiple solutions under a single lock acquisition.

        Args:
            solutions: List of Solution objects.

        Returns:
            List of shared indices, one per solution.
        """
        if not solutions:
            return []

        n_new = len(solutions)
        # Build row matrix outside the lock (pure numpy, no GIL needed).
        costs = np.empty(n_new, dtype=np.float64)
        rows_mat = np.zeros((n_new, self._n_constraints), dtype=np.float64)
        for k, sol in enumerate(solutions):
            costs[k] = float(sol.column.cost)
            for row in sol.column.rows:
                if row.index < self._n_constraints:
                    rows_mat[k, int(row.index)] = float(row.coefficient)

        with self._lock:
            start = int(self._header["count"][0])
            end = start + n_new
            if end > self._max_cols:
                raise RuntimeError(
                    f"SharedPricingPool: adding {n_new} columns would exceed capacity "
                    f"({start}/{self._max_cols})"
                )
            self._matrix[start:end, 0] = costs
            self._matrix[start:end, 1:] = rows_mat
            self._valid[start:end] = np.uint8(1)
            self._header["count"] = end
        return list(range(start, end))

    def invalidate(self, shared_indices: list[int]) -> None:
        """Mark column slots as deleted.  They will be skipped during pricing.

        Args:
            shared_indices: Shared indices previously returned by :meth:`add`.
        """
        if not shared_indices:
            return
        idx = np.asarray(shared_indices, dtype=np.intp)
        with self._lock:
            self._valid[idx] = np.uint8(0)

    # ── Pricing (lock-free) ───────────────────────────────────────────────────

    def price(
        self,
        duals: np.ndarray,
        threshold: float = -1e-9,
        view_mask: np.ndarray | None = None,
    ) -> tuple[np.ndarray, np.ndarray]:
        """Compute reduced costs and return negative-RC columns sorted best-first.

        **Lock-free.**  Computes::

            rc[i] = matrix[i, 0] - matrix[i, 1:n_duals+1] @ duals[:n_duals]

        over the entire committed region (single BLAS DGEMV), then keeps only
        columns that are active, pass the optional ``view_mask``, and have
        ``rc < threshold``.  Results are returned sorted by reduced cost
        ascending (most negative / most improving first).

        Dynamic cuts: pass a longer ``duals`` vector when new cuts are added.
        Old columns already have coefficient 0 at those indices (zero-filled at
        pool creation), so no update is needed.

        Args:
            duals: 1-D float64 LP dual values, indexed by constraint.
            threshold: Keep only columns with ``rc < threshold``.
                Default ``-1e-9`` avoids returning near-zero columns.
            view_mask: Optional boolean numpy array of shape ``(max_cols,)``
                restricting pricing to a subset of columns (for filtered views).
                ``None`` → all valid columns are priced.

        Returns:
            ``(indices, reduced_costs)`` — 1-D arrays of the same length,
            sorted by ``reduced_costs`` ascending.  ``indices`` are 0-based
            positions in the shared matrix.
        """
        duals = np.asarray(duals, dtype=np.float64)

        # Single aligned read of count — safe without lock on x86/arm64.
        n = int(self._header["count"][0])
        if n == 0:
            return np.empty(0, dtype=np.intp), np.empty(0, dtype=np.float64)

        n_duals = min(len(duals), self._n_constraints)

        # Slice views into the committed region (no copy).
        mat = self._matrix[:n]  # shape (n, n_constraints+1)
        valid = self._valid[:n].view(np.bool_)  # zero-copy bool view

        # ── Core pricing: single BLAS DGEMV, no fancy indexing ────────────────
        col_costs = mat[:, 0].copy()  # force contiguous (col-strided otherwise)
        if n_duals > 0:
            rc = col_costs - mat[:, 1 : n_duals + 1] @ duals[:n_duals]
        else:
            rc = col_costs.copy()

        # ── Filter: valid ∩ view_mask ∩ (rc < threshold) ─────────────────────
        active = valid
        if view_mask is not None:
            active = active & view_mask[:n]
        active = active & (rc < threshold)

        indices = np.where(active)[0]
        if len(indices) == 0:
            return np.empty(0, dtype=np.intp), np.empty(0, dtype=np.float64)

        # Sort ascending by reduced cost (most improving first).
        rc_sel = rc[indices]
        order = np.argsort(rc_sel, kind="stable")
        return indices[order], rc_sel[order]

    # ── Direct numpy views ────────────────────────────────────────────────────

    @property
    def col_costs_view(self) -> np.ndarray:
        """Zero-copy 1-D view of LP costs for committed columns.

        Shape: ``(count,)``.  Strided (not contiguous); copy if needed for BLAS.
        """
        n = int(self._header["count"][0])
        return self._matrix[:n, 0]

    @property
    def row_matrix_view(self) -> np.ndarray:
        """Zero-copy 2-D view of coefficient sub-matrix for committed columns.

        Shape: ``(count, n_constraints)``.  C-contiguous row slice.
        Use ``row_matrix_view[:, :n_duals] @ duals`` for a pure-numpy DGEMV.
        """
        n = int(self._header["count"][0])
        return self._matrix[:n, 1:]

    @property
    def valid_view(self) -> np.ndarray:
        """Zero-copy uint8 view of the valid flags.

        Shape: ``(max_cols,)``.
        """
        return self._valid

    @property
    def matrix_view(self) -> np.ndarray:
        """Zero-copy view of the full committed matrix.

        Shape: ``(count, n_constraints+1)``.  Column 0 is LP cost; columns 1…n
        are constraint coefficients.
        """
        n = int(self._header["count"][0])
        return self._matrix[:n]

    # ── Counters ──────────────────────────────────────────────────────────────

    @property
    def count(self) -> int:
        """Total column slots committed (including invalidated ones)."""
        return int(self._header["count"][0])

    @property
    def active_count(self) -> int:
        """Number of currently active (non-invalidated) columns."""
        n = self.count
        return int(self._valid[:n].sum()) if n > 0 else 0

    # ── Lifecycle ─────────────────────────────────────────────────────────────

    def close(self) -> None:
        """Detach from the shared memory segment without destroying it."""
        self._shm.close()

    def unlink(self) -> None:
        """Destroy the shared memory segment.

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
            f"SharedPricingPool(count={self.count}, active={self.active_count}, "
            f"max_cols={self._max_cols}, n_constraints={self._n_constraints})"
        )


class FilteredSharedPricingPool:
    """A local (per-process) filtered view over a SharedPricingPool.

    Mirrors the concept of ``FilteredSolutionPool`` in Python space: holds a
    numpy boolean mask that restricts which column slots are visible during
    pricing.  The mask is process-local (not in shared memory), making this
    suitable for Branch-and-Bound where each node has its own column exclusions.

    Typically constructed via :meth:`PricingPool.new_numpy_filter` which
    automatically populates the mask from the corresponding C++
    ``FilteredSolutionPool``.

    Args:
        shared: The backing ``SharedPricingPool``.
        view_indices: 1-D integer array of shared indices that are in this view.
            ``None`` → all currently valid columns are included.
    """

    def __init__(
        self,
        shared: SharedPricingPool,
        view_indices: np.ndarray | None = None,
    ) -> None:
        self._shared = shared
        self._mask = np.zeros(shared._max_cols, dtype=np.bool_)
        if view_indices is not None and len(view_indices) > 0:
            self._mask[np.asarray(view_indices, dtype=np.intp)] = True
        elif view_indices is None:
            # Include all currently valid column slots.
            n = shared.count
            if n > 0:
                self._mask[:n] = shared._valid[:n].view(np.bool_)

    # ── View management ───────────────────────────────────────────────────────

    def add_to_view(self, shared_indices: list[int] | np.ndarray) -> None:
        """Include additional column slots in this filter view.

        Args:
            shared_indices: Shared indices to add (e.g. newly added columns).
        """
        if len(shared_indices) > 0:
            self._mask[np.asarray(shared_indices, dtype=np.intp)] = True

    def remove_from_view(self, shared_indices: list[int] | np.ndarray) -> None:
        """Exclude column slots from this filter view (B&B arc/row restriction).

        Args:
            shared_indices: Shared indices to exclude.
        """
        if len(shared_indices) > 0:
            self._mask[np.asarray(shared_indices, dtype=np.intp)] = False

    # ── Pricing ───────────────────────────────────────────────────────────────

    def price(
        self,
        duals: np.ndarray,
        threshold: float = -1e-9,
    ) -> tuple[np.ndarray, np.ndarray]:
        """Price only columns in this filter view.

        Delegates to :meth:`SharedPricingPool.price` with ``view_mask=self._mask``.
        Returns columns sorted by reduced cost ascending (most improving first).

        Args:
            duals: 1-D float64 LP dual values.
            threshold: Keep only columns with ``rc < threshold``.

        Returns:
            ``(indices, reduced_costs)`` sorted ascending by ``reduced_costs``.
        """
        return self._shared.price(duals, threshold=threshold, view_mask=self._mask)

    # ── Properties ────────────────────────────────────────────────────────────

    @property
    def view_count(self) -> int:
        """Number of column slots currently in this filter view."""
        n = self._shared.count
        return int(self._mask[:n].sum()) if n > 0 else 0

    @property
    def mask(self) -> np.ndarray:
        """Zero-copy boolean mask array (shape: ``[max_cols]``)."""
        return self._mask

    def __repr__(self) -> str:
        return (
            f"FilteredSharedPricingPool(view_count={self.view_count}, " f"shared={self._shared!r})"
        )


class PricingPool:
    """Keeps a C++ ``FilteredSolutionPool`` and a ``SharedPricingPool`` in sync.

    The master process owns both objects:

    * All structural operations (add, remove, filter, activity tracking) go
      through the C++ ``FilteredSolutionPool``.
    * Cross-process pricing uses the ``SharedPricingPool`` via shared memory.

    Worker processes receive a ``shared_handle()`` and call
    ``SharedPricingPool.attach(handle)`` — they never touch the C++ pool.

    Args:
        filtered_pool: Existing ``FilteredSolutionPool`` from the C++ bindings.
        n_constraints: Passed to ``SharedPricingPool``.  Should be ≥ the highest
            row index that will appear in any column (over-allocate for cuts).
        max_cols: Capacity of the shared pool.
        lock: Optional external lock for spawn-safe multiprocessing.

    Example::

        pool = SolutionPool()
        pp = PricingPool(pool.new_filter(), n_constraints=200, max_cols=50_000)
        handle = pp.shared_handle()   # pass to workers

        # worker:
        shared = SharedPricingPool.attach(handle)
        indices, rcs = shared.price(duals)
    """

    def __init__(
        self,
        filtered_pool: object,
        n_constraints: int,
        max_cols: int = 50_000,
        lock: object | None = None,
    ) -> None:
        # Set _fp before anything that might call __getattr__.
        object.__setattr__(self, "_fp", filtered_pool)
        object.__setattr__(
            self,
            "_shared",
            SharedPricingPool(n_constraints=n_constraints, max_cols=max_cols, lock=lock),
        )
        object.__setattr__(self, "_id_to_idx", {})

    def shared_handle(self) -> dict:
        """Picklable handle for workers to attach to the shared pool."""
        return self._shared.handle()

    # ── Write operations (keep both pools in sync) ────────────────────────────

    def add(self, solution: "Solution") -> int:
        """Add to both C++ pool and shared pool.

        Returns C++ ColumnId.
        """
        cpp_id = self._fp.add(solution)
        shared_idx = self._shared.add(solution)
        self._id_to_idx[int(cpp_id)] = shared_idx
        return cpp_id

    def add_columns(self, solutions: list) -> list[int]:
        """Batch-add to both pools.

        Returns list of C++ ColumnIds.
        """
        cpp_ids = self._fp.add(solutions)
        shared_idxs = self._shared.add_columns(solutions)
        for cid, sidx in zip(cpp_ids, shared_idxs):
            self._id_to_idx[int(cid)] = sidx
        return cpp_ids

    def remove_stale(self, max_age: int, min_usage_rate: float = 0.0) -> list[int]:
        """Remove stale columns from both pools.

        Returns removed C++ ColumnIds.
        """
        removed_ids = self._fp.remove_stale(max_age, min_usage_rate)
        self._shared.invalidate(
            [self._id_to_idx.pop(int(i)) for i in removed_ids if int(i) in self._id_to_idx]
        )
        return removed_ids

    def global_remove_if(self, pred) -> list[int]:
        """Global hard-delete from both pools.

        Returns removed C++ ColumnIds.
        """
        removed_ids = self._fp.global_remove_if(pred)
        self._shared.invalidate(
            [self._id_to_idx.pop(int(i)) for i in removed_ids if int(i) in self._id_to_idx]
        )
        return removed_ids

    # ── Filtered numpy views ──────────────────────────────────────────────────

    def new_numpy_filter(
        self,
        cpp_filter=None,
    ) -> "FilteredSharedPricingPool":
        """Create a FilteredSharedPricingPool mirroring a C++ FilteredSolutionPool.

        Snapshots the column IDs currently in the C++ filter view, maps them to
        shared indices via the internal ``ColumnId → shared_index`` mapping, and
        returns a ``FilteredSharedPricingPool`` with that mask.

        Columns added to the ``PricingPool`` after this call are NOT automatically
        added to the returned filter (call :meth:`FilteredSharedPricingPool.add_to_view`
        explicitly, or re-create the filter).  This matches CG/B&B usage where
        the filter snapshot is taken at the start of a pricing round.

        Args:
            cpp_filter: A ``FilteredSolutionPool`` whose column selection to mirror.
                ``None`` → use this pool's own ``FilteredSolutionPool`` (all columns).

        Returns:
            A process-local ``FilteredSharedPricingPool``.
        """
        fp = cpp_filter if cpp_filter is not None else self._fp
        # get_all() returns list of (ColumnId, Solution, ColumnActivity).
        cpp_ids = [int(entry[0]) for entry in fp.get_all()]
        shared_indices = [self._id_to_idx[cid] for cid in cpp_ids if cid in self._id_to_idx]
        arr = np.asarray(shared_indices, dtype=np.intp) if shared_indices else None
        return FilteredSharedPricingPool(self._shared, view_indices=arr)

    # ── Pricing ───────────────────────────────────────────────────────────────

    def price_shared(
        self,
        duals: np.ndarray,
        threshold: float = -1e-9,
    ) -> tuple[np.ndarray, np.ndarray]:
        """Lock-free pricing on shared memory (all valid columns).

        Returns ``(indices, reduced_costs)`` sorted ascending by reduced cost.
        """
        return self._shared.price(duals, threshold)

    def filtered_price(
        self,
        duals: np.ndarray,
        threshold: float = -1e-9,
    ) -> tuple[np.ndarray, np.ndarray]:
        """Price columns visible in this pool's own C++ filter view.

        Convenience shorthand for ``self.new_numpy_filter().price(duals)``.
        Re-snapshots the C++ filter on every call — use :meth:`new_numpy_filter`
        and cache the result if you call this in a tight loop.

        Returns ``(indices, reduced_costs)`` sorted ascending by reduced cost.
        """
        return self.new_numpy_filter().price(duals, threshold=threshold)

    # ── Direct numpy access ───────────────────────────────────────────────────

    @property
    def shared_pool(self) -> SharedPricingPool:
        """The underlying SharedPricingPool for direct numpy access."""
        return self._shared

    # ── Delegate everything else to the C++ FilteredSolutionPool ─────────────

    def __getattr__(self, name: str) -> object:
        # Called only when normal attribute lookup fails.
        # `_fp` is set via object.__setattr__ in __init__, so it is always found
        # through normal lookup — no recursion risk here.
        return getattr(self._fp, name)

    def close(self) -> None:
        """Release the shared memory segment."""
        self._shared.unlink()

    def __repr__(self) -> str:
        return f"PricingPool(shared={self._shared!r})"
