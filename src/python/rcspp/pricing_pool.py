"""Cross-process shared pricing pool for RCSPP column generation.

Memory layout (single SharedMemory segment)
-------------------------------------------
Offset 0 … HEADER_BYTES-1:
    Structured header — see ``_HEADER_DTYPE`` below.
    Written once at construction; read-only after that.

CSR regions (all committed under the write lock):

    ``valid[max_cols]``              uint8   — 0=empty/invalid, 1=active
    ``col_costs[max_cols]``          float64 — LP cost per column
    ``row_starts[max_cols + 1]``     int32   — CSR row-pointer array
    ``col_indices[max_nnz]``         int32   — constraint indices (non-zeros)
    ``col_values[max_nnz]``          float64 — constraint coefficients

The ``row_starts`` sentinel pattern:
    row_starts[0]   = 0
    row_starts[i+1] = row_starts[i] + nnz_for_column_i

CSR pricing formula (lock-free, via scipy.sparse SPMV)
-------------------------------------------------------
``rc[i] = col_costs[i] - A[i, :n_duals] @ duals[:n_duals]``

where ``A`` is built as a ``scipy.sparse.csr_matrix`` directly over the shared
memory buffers (zero-copy when dtypes match: ``int32`` indices, ``float64`` values).
Complexity: ``O(nnz)`` — independent of ``n_constraints``.

Dynamic cuts
------------
``n_constraints`` is over-allocated at creation (e.g. 1000).  New cuts only
add new entries in the dual vector.  Old columns already have no entry at those
indices, so they are handled correctly without any data update.
"""

from __future__ import annotations

from multiprocessing import Lock
from multiprocessing.shared_memory import SharedMemory
from typing import TYPE_CHECKING

import numpy as np

try:
    from scipy.sparse import csr_matrix as _sp_csr

    _SCIPY_AVAILABLE = True
except ImportError:  # pragma: no cover
    _SCIPY_AVAILABLE = False

if TYPE_CHECKING:
    from rcspp._core.graph import Solution

# ── Shared-memory header ──────────────────────────────────────────────────────
# 10 × uint64 = 80 bytes, padded to 128 (two cache lines) for alignment.
_HEADER_DTYPE = np.dtype(
    [
        ("count", np.uint64),  # committed columns
        ("nnz", np.uint64),  # committed non-zeros (total across all columns)
        ("n_constraints", np.uint64),  # constraint capacity (over-allocated)
        ("max_cols", np.uint64),  # column capacity
        ("max_nnz", np.uint64),  # non-zero capacity
        ("valid_offset", np.uint64),  # byte offset of valid[]
        ("costs_offset", np.uint64),  # byte offset of col_costs[]
        ("row_starts_offset", np.uint64),  # byte offset of row_starts[]
        ("col_indices_offset", np.uint64),  # byte offset of col_indices[]
        ("col_values_offset", np.uint64),  # byte offset of col_values[]
    ]
)
_HEADER_BYTES = 128  # two cache lines — header fits in 80, leave 48 spare


def _align_up(n: int, align: int) -> int:
    """Round ``n`` up to the nearest multiple of ``align``."""
    return (n + align - 1) & ~(align - 1)


class SharedPricingPool:
    """Cross-process shared pricing pool with CSR storage and scipy SPMV pricing.

    Stores LP column data in **CSR format** (cost vector + sparse coefficient
    matrix).  Columns are sparse in practice (VRP: 2–5 rows per column out of
    50–200 constraints), so CSR avoids storing and multiplying the many zeros
    that a dense layout would contain.

    Pricing is ``O(nnz)`` via ``scipy.sparse`` — independent of
    ``n_constraints``.

    ``price(duals)`` is **lock-free**: it reads a snapshot of ``count`` and
    ``nnz`` from the header, builds a ``csr_matrix`` view over the committed
    CSR buffers, and calls scipy's SPMV.

    Args:
        n_constraints: Highest constraint index that will appear + 1.
            Over-allocate (e.g. 1000) to leave room for future cuts.
        max_cols: Maximum number of column slots.
        max_nnz_per_col: Expected maximum non-zeros per column (default 50).
            ``max_nnz = max_cols × max_nnz_per_col``.
        name: Explicit name for the SharedMemory segment; auto-generated if None.
        lock: External lock (e.g. ``Manager().Lock()`` for spawn-safe use).
            A plain ``Lock()`` is created if None.

    Raises:
        ImportError: If scipy is not installed.
    """

    def __init__(
        self,
        n_constraints: int,
        max_cols: int = 50_000,
        max_nnz_per_col: int = 50,
        name: str | None = None,
        lock: object | None = None,
    ) -> None:
        if not _SCIPY_AVAILABLE:
            raise ImportError(
                "SharedPricingPool requires scipy.  Install it with: pip install scipy"
            )
        self._n_constraints = int(n_constraints)
        self._max_cols = int(max_cols)
        self._max_nnz = int(max_cols) * int(max_nnz_per_col)
        self._lock = lock if lock is not None else Lock()

        # ── Compute offsets, all padded to 8-byte (float64) boundaries ────────
        valid_offset = _HEADER_BYTES
        costs_offset = _align_up(valid_offset + max_cols, 8)
        row_starts_offset = _align_up(costs_offset + max_cols * 8, 8)
        col_indices_offset = _align_up(row_starts_offset + (max_cols + 1) * 4, 8)
        col_values_offset = _align_up(col_indices_offset + self._max_nnz * 4, 8)
        total = col_values_offset + self._max_nnz * 8

        self._shm = SharedMemory(name=name, create=True, size=total)
        self._shm.buf[:total] = b"\x00" * total  # zero-fill

        # ── Write header ───────────────────────────────────────────────────────
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
        # row_starts[0] = 0 is already set by zero-fill.

    # ── Views ─────────────────────────────────────────────────────────────────

    def _init_views(self) -> None:
        """Build zero-copy numpy views over the shared buffer."""
        buf = self._shm.buf
        self._header = np.ndarray((1,), dtype=_HEADER_DTYPE, buffer=buf)
        self._valid = np.ndarray(
            (self._max_cols,), dtype=np.uint8, buffer=buf, offset=self._valid_offset
        )
        self._col_costs = np.ndarray(
            (self._max_cols,), dtype=np.float64, buffer=buf, offset=self._costs_offset
        )
        # row_starts has max_cols+1 entries (CSR sentinel).
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

    # ── Attach from another process ───────────────────────────────────────────

    @classmethod
    def attach(cls, handle: dict) -> "SharedPricingPool":
        """Attach to an existing pool from a worker process (zero-copy).

        Args:
            handle: dict returned by :meth:`handle`.

        Returns:
            A ``SharedPricingPool`` sharing the same memory.
        """
        if not _SCIPY_AVAILABLE:
            raise ImportError(
                "SharedPricingPool requires scipy.  Install it with: pip install scipy"
            )
        obj = object.__new__(cls)
        obj._lock = handle["lock"]
        obj._shm = SharedMemory(name=handle["shm_name"], create=False)

        # Read all layout params from the authoritative shared header.
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
        """Picklable handle — pass to worker processes via :meth:`attach`.

        Returns:
            ``{'shm_name': ..., 'lock': ...}`` — all layout params are read
            from the shared header on attach.
        """
        return {"shm_name": self._shm.name, "lock": self._lock}

    # ── Write operations ──────────────────────────────────────────────────────

    def add(self, solution: "Solution") -> int:
        """Add one solution's LP column to the CSR pool.

        Writes cost, row indices, and row values, then commits ``valid`` and
        ``count`` atomically under the lock.

        Returns:
            Shared index of the new column slot.

        Raises:
            RuntimeError: Pool is full (columns or non-zeros).
        """
        col = solution.column
        # Collect non-zeros outside the lock (pure Python, no contention).
        rows = [
            (int(r.index), float(r.coefficient))
            for r in col.rows
            if int(r.index) < self._n_constraints
        ]

        with self._lock:
            count = int(self._header["count"][0])
            nnz = int(self._header["nnz"][0])
            if count >= self._max_cols:
                raise RuntimeError(f"SharedPricingPool is full (columns: {count}/{self._max_cols})")
            new_nnz = nnz + len(rows)
            if new_nnz > self._max_nnz:
                raise RuntimeError(
                    f"SharedPricingPool non-zero capacity exceeded " f"({new_nnz}/{self._max_nnz})"
                )
            # Write LP data.
            self._col_costs[count] = float(col.cost)
            for k, (idx, val) in enumerate(rows):
                self._col_indices[nnz + k] = np.int32(idx)
                self._col_values[nnz + k] = val
            # Commit: update CSR sentinel, valid flag, then count and nnz.
            self._row_starts[count + 1] = np.int32(new_nnz)
            self._valid[count] = np.uint8(1)
            self._header["nnz"] = new_nnz
            self._header["count"] = count + 1
        return count

    def add_columns(self, solutions: list) -> list[int]:
        """Batch-add multiple solutions under a single lock acquisition.

        Returns:
            List of shared indices, one per solution.
        """
        if not solutions:
            return []

        # Prepare all CSR data outside the lock.
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
                    f"SharedPricingPool: adding {len(solutions)} columns would "
                    f"exceed capacity ({start}/{self._max_cols})"
                )
            # Write all columns.
            cursor = nnz_start
            for i, (col_rows) in enumerate(row_data):
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
        """Mark columns as deleted (skip during pricing).

        Args:
            shared_indices: Shared indices returned by :meth:`add`.
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
        """Compute reduced costs and return improving columns sorted best-first.

        **Lock-free.**  Reads a snapshot of ``count`` and ``nnz``, builds a
        ``scipy.sparse.csr_matrix`` **view** over the committed CSR buffers
        (zero-copy for ``int32`` indices and ``float64`` values), and calls
        scipy's SPMV::

            rc[i] = col_costs[i] - A[i, :] @ duals

        Complexity: ``O(nnz)`` — independent of ``n_constraints``.

        Args:
            duals: 1-D float64 LP dual values, indexed by constraint.
            threshold: Keep only columns with ``rc < threshold``.
                Default ``-1e-9`` avoids near-zero columns.
            view_mask: Optional boolean ``ndarray`` of shape ``(max_cols,)``.
                If provided, only columns where ``view_mask[i] = True`` are
                considered.  Used by :class:`FilteredSharedPricingPool`.

        Returns:
            ``(indices, reduced_costs)`` — 1-D arrays, sorted ascending by
            ``reduced_costs`` (most improving first).
        """
        duals = np.asarray(duals, dtype=np.float64)

        # Lock-free snapshot reads.
        n = int(self._header["count"][0])
        nnz = int(self._header["nnz"][0])
        if n == 0:
            return np.empty(0, dtype=np.intp), np.empty(0, dtype=np.float64)

        n_duals = min(len(duals), self._n_constraints)

        # ── Build CSR view (zero-copy) ─────────────────────────────────────────
        # scipy.csr_matrix with copy=False uses the shared-memory arrays directly
        # when dtypes match (int32 indices, float64 values).
        indptr = self._row_starts[: n + 1]  # view, int32
        indices = self._col_indices[:nnz]  # view, int32
        data = self._col_values[:nnz]  # view, float64
        A = _sp_csr(
            (data, indices, indptr),
            shape=(n, self._n_constraints),
            copy=False,
        )

        # ── SPMV: rc = col_costs - A[:, :n_duals] @ duals[:n_duals] ──────────
        col_costs = np.asarray(self._col_costs[:n], dtype=np.float64)  # contiguous copy
        d = np.zeros(self._n_constraints, dtype=np.float64)
        d[:n_duals] = duals[:n_duals]
        rc = col_costs - A @ d

        # ── Filter: valid ∩ view_mask ∩ (rc < threshold) ─────────────────────
        valid = self._valid[:n].view(np.bool_)
        active = valid
        if view_mask is not None:
            active = active & view_mask[:n]
        active = active & (rc < threshold)

        indices_out = np.where(active)[0]
        if len(indices_out) == 0:
            return np.empty(0, dtype=np.intp), np.empty(0, dtype=np.float64)

        # Sort ascending by reduced cost (most improving first).
        rc_sel = rc[indices_out]
        order = np.argsort(rc_sel, kind="stable")
        return indices_out[order], rc_sel[order]

    # ── Direct numpy views ────────────────────────────────────────────────────

    @property
    def col_costs_view(self) -> np.ndarray:
        """Zero-copy 1-D view of LP costs for committed columns.

        Shape: ``(count,)``.
        """
        return self._col_costs[: self.count]

    @property
    def row_starts_view(self) -> np.ndarray:
        """Zero-copy int32 CSR row-pointer array.

        Shape: ``(count+1,)``.
        """
        n = self.count
        return self._row_starts[: n + 1]

    @property
    def col_indices_view(self) -> np.ndarray:
        """Zero-copy int32 constraint-index array for non-zeros.

        Shape: ``(nnz,)``.
        """
        return self._col_indices[: self.nnz]

    @property
    def col_values_view(self) -> np.ndarray:
        """Zero-copy float64 coefficient array for non-zeros.

        Shape: ``(nnz,)``.
        """
        return self._col_values[: self.nnz]

    @property
    def valid_view(self) -> np.ndarray:
        """Zero-copy uint8 validity flags.

        Shape: ``(max_cols,)``.
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
            f"SharedPricingPool(count={self.count}, nnz={self.nnz}, "
            f"active={self.active_count}, max_cols={self._max_cols}, "
            f"n_constraints={self._n_constraints})"
        )


class FilteredSharedPricingPool:
    """A local (per-process) filtered view over a :class:`SharedPricingPool`.

    Holds a numpy boolean mask that restricts which column slots are visible
    during pricing.  The mask is process-local (not in shared memory), making
    this suitable for Branch-and-Bound where each node has its own exclusions.

    Typically constructed via :meth:`PricingPool.new_numpy_filter`.

    Args:
        shared: The backing ``SharedPricingPool``.
        view_indices: 1-D integer array of shared indices in this view.
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
            n = shared.count
            if n > 0:
                self._mask[:n] = shared._valid[:n].view(np.bool_)

    # ── View management ───────────────────────────────────────────────────────

    def add_to_view(self, shared_indices: list[int] | np.ndarray) -> None:
        """Include additional column slots in this filter view.

        Args:
            shared_indices: Shared indices to add.
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

        Args:
            duals: 1-D float64 LP dual values.
            threshold: Keep only columns with ``rc < threshold``.

        Returns:
            ``(indices, reduced_costs)`` sorted ascending.
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
        """Zero-copy boolean mask array ``(max_cols,)``."""
        return self._mask

    def __repr__(self) -> str:
        return (
            f"FilteredSharedPricingPool(view_count={self.view_count}, " f"shared={self._shared!r})"
        )


class PricingPool:
    """Keeps a C++ ``FilteredSolutionPool`` and a ``SharedPricingPool`` in sync.

    The master process owns both objects:

    * Structural operations (add, remove, filter, activity) go through the C++
      ``FilteredSolutionPool``.
    * Cross-process pricing uses the ``SharedPricingPool`` (CSR, scipy SPMV).

    Worker processes receive a ``shared_handle()`` and call
    ``SharedPricingPool.attach(handle)`` — they never touch the C++ pool.

    Args:
        filtered_pool: Existing ``FilteredSolutionPool`` from the C++ bindings.
        n_constraints: Passed to ``SharedPricingPool``.
        max_cols: Column capacity of the shared pool.
        max_nnz_per_col: Maximum non-zeros per column (default 50).
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
        max_nnz_per_col: int = 50,
        lock: object | None = None,
    ) -> None:
        object.__setattr__(self, "_fp", filtered_pool)
        object.__setattr__(
            self,
            "_shared",
            SharedPricingPool(
                n_constraints=n_constraints,
                max_cols=max_cols,
                max_nnz_per_col=max_nnz_per_col,
                lock=lock,
            ),
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
    ) -> FilteredSharedPricingPool:
        """Create a FilteredSharedPricingPool mirroring a C++ FilteredSolutionPool.

        Snapshots the column IDs currently in the C++ filter view, maps them to
        shared indices, and returns a ``FilteredSharedPricingPool``.

        Args:
            cpp_filter: A ``FilteredSolutionPool`` to mirror.
                ``None`` → use this pool's own filter (all columns).

        Returns:
            A process-local ``FilteredSharedPricingPool``.
        """
        fp = cpp_filter if cpp_filter is not None else self._fp
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
        """Lock-free CSR pricing (all valid columns).

        Returns ``(indices, reduced_costs)`` sorted ascending.
        """
        return self._shared.price(duals, threshold)

    def filtered_price(
        self,
        duals: np.ndarray,
        threshold: float = -1e-9,
    ) -> tuple[np.ndarray, np.ndarray]:
        """Price only the C++ pool's current filter view.

        Shorthand for ``new_numpy_filter().price(duals)``.

        Returns ``(indices, reduced_costs)`` sorted ascending.
        """
        return self.new_numpy_filter().price(duals, threshold=threshold)

    # ── Properties / delegation ───────────────────────────────────────────────

    @property
    def shared_pool(self) -> SharedPricingPool:
        """The underlying ``SharedPricingPool`` for direct CSR access."""
        return self._shared

    def __getattr__(self, name: str) -> object:
        return getattr(self._fp, name)

    def close(self) -> None:
        """Release the shared memory segment."""
        self._shared.unlink()

    def __repr__(self) -> str:
        return f"PricingPool(shared={self._shared!r})"
