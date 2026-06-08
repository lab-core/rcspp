"""Tests for SharedPricingPool, PricingPool, and FilteredPricingPool."""

import multiprocessing as mp
import os
import sys

import numpy as np

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "../python_interface")))
sys.path.insert(0, os.path.abspath(os.path.dirname(__file__)))

from rcspp._core.graph import Column, Row, Solution  # noqa: E402
from rcspp.pricing_pool import (  # noqa: E402
    FilteredPricingPool,
    FilteredSharedPricingPool,
    PricingPool,
    SharedPricingPool,
)

# ── helpers ───────────────────────────────────────────────────────────────────


def make_solution(
    col_cost: float, rows: list[tuple[int, float]], arc_ids: list[int] = None
) -> Solution:
    col = Column()
    col.cost = col_cost
    col.rows = [Row(index=i, coefficient=c) for i, c in rows]
    sol = Solution()
    sol.cost = col_cost
    sol.column = col
    sol.path_arc_ids = arc_ids or []
    sol.path_node_ids = []
    return sol


# ── SharedPricingPool (low-level) ─────────────────────────────────────────────


def test_create_and_attach():
    pool = SharedPricingPool(n_constraints=10, max_cols=100)
    try:
        handle = pool.handle()
        worker = SharedPricingPool.attach(handle)
        try:
            pool.add(make_solution(42.0, [(3, 1.5)], [1]))
            assert worker.count == 1
            assert worker.nnz == 1
            assert abs(float(worker.col_costs_view[0]) - 42.0) < 1e-9
            assert int(worker.col_indices_view[0]) == 3
            assert abs(float(worker.col_values_view[0]) - 1.5) < 1e-9
        finally:
            worker.close()
    finally:
        pool.unlink()


def test_csr_alignment():
    for max_cols in [1, 5, 7, 8, 100, 1_000]:
        pool = SharedPricingPool(n_constraints=5, max_cols=max_cols)
        try:
            assert pool._costs_offset % 8 == 0
            assert pool._col_values_offset % 8 == 0
        finally:
            pool.unlink()


def test_add_batch():
    pool = SharedPricingPool(n_constraints=5, max_cols=50)
    try:
        solutions = [make_solution(float(i), [(0, float(i))], [k]) for k, i in enumerate(range(5))]
        idxs = pool.add_columns(solutions)
        assert idxs == [0, 1, 2, 3, 4]
        assert pool.count == 5
        assert pool.nnz == 5
        assert pool.active_count == 5
        assert list(pool.row_starts_view) == [0, 1, 2, 3, 4, 5]
    finally:
        pool.unlink()


def test_add_columns_rejects_nnz_overflow():
    """add_columns must raise RuntimeError (not a raw numpy IndexError) when a batch
    exceeds the non-zero capacity — the documented contract."""
    import pytest

    pool = SharedPricingPool(n_constraints=10, max_cols=100, max_nnz_per_col=2)
    try:
        # Capacity = 100 * 2 = 200 nnz; this batch needs 100 * 3 = 300.
        sols = [make_solution(1.0, [(0, 1.0), (1, 1.0), (2, 1.0)], [k]) for k in range(100)]
        with pytest.raises(RuntimeError):
            pool.add_columns(sols)
    finally:
        pool.unlink()


def test_csr_multiple_rows():
    pool = SharedPricingPool(n_constraints=10, max_cols=20)
    try:
        pool.add(make_solution(5.0, [(0, 1.0), (2, 2.0)], [0]))
        pool.add(make_solution(3.0, [(1, 3.0)], [1]))
        assert pool.count == 2
        assert pool.nnz == 3
        assert list(pool.row_starts_view) == [0, 2, 3]
        assert int(pool.col_indices_view[0]) == 0
        assert int(pool.col_indices_view[1]) == 2
        assert int(pool.col_indices_view[2]) == 1
    finally:
        pool.unlink()


def test_price_sorted_by_rc():
    pool = SharedPricingPool(n_constraints=5, max_cols=50)
    try:
        pool.add(make_solution(1.0, [(0, 4.0)], [0]))  # rc = 1-4 = -3
        pool.add(make_solution(5.0, [(0, 2.0)], [1]))  # rc = 5-2 = 3 (above threshold)
        pool.add(make_solution(1.0, [(0, 5.0)], [2]))  # rc = 1-5 = -4 (most negative)
        pool.add(make_solution(2.0, [(0, 1.0)], [3]))  # rc = 2-1 = 1 (above threshold)
        duals = np.array([1.0, 0.0, 0.0, 0.0, 0.0])
        indices, rcs = pool.price(duals)
        assert len(rcs) == 2
        assert list(rcs) == sorted(rcs)
        assert all(rc < -1e-9 for rc in rcs)
    finally:
        pool.unlink()


def test_price_default_threshold():
    pool = SharedPricingPool(n_constraints=2, max_cols=20)
    try:
        pool.add(make_solution(1.0, [(0, 1.0)], [0]))
        indices, rcs = pool.price(np.array([1.0, 0.0]))  # rc = 0 → excluded
        assert len(indices) == 0
        indices, rcs = pool.price(np.array([3.0, 0.0]))  # rc = -2 → included
        assert len(indices) == 1 and abs(rcs[0] - (-2.0)) < 1e-9
    finally:
        pool.unlink()


def test_invalidate_hides_column():
    pool = SharedPricingPool(n_constraints=5, max_cols=50)
    try:
        idx = pool.add(make_solution(1.0, [(0, 0.1)], [1]))
        indices, _ = pool.price(np.array([100.0, 0.0, 0.0, 0.0, 0.0]))
        assert len(indices) == 1
        pool.invalidate([idx])
        indices, _ = pool.price(np.array([100.0, 0.0, 0.0, 0.0, 0.0]))
        assert len(indices) == 0
    finally:
        pool.unlink()


def test_dynamic_rows():
    pool = SharedPricingPool(n_constraints=10, max_cols=50)
    try:
        pool.add(make_solution(5.0, [(0, 1.0)], [1]))
        pool.add(make_solution(5.0, [(0, 1.0), (5, 2.0)], [2]))
        duals = np.zeros(6)
        duals[0] = 6.0
        duals[5] = 0.5
        indices, rcs = pool.price(duals)
        assert len(indices) == 2
    finally:
        pool.unlink()


def _worker_add(handle: dict, sol_data: list[tuple]) -> list[int]:
    import os as _os
    import sys as _sys

    _sys.path.insert(
        0, _os.path.abspath(_os.path.join(_os.path.dirname(__file__), "../python_interface"))
    )
    from rcspp._core.graph import Column, Row
    from rcspp._core.graph import Solution as _Sol

    pool = SharedPricingPool.attach(handle)
    indices = []
    for col_cost, rows_data, arc_ids in sol_data:
        col = Column()
        col.cost = col_cost
        col.rows = [Row(index=i, coefficient=c) for i, c in rows_data]
        sol = _Sol()
        sol.cost = col_cost
        sol.column = col
        sol.path_arc_ids = arc_ids
        sol.path_node_ids = []
        indices.append(pool.add(sol))
    pool.close()
    return indices


def test_multiprocess_add():
    n_workers, n_per_worker = 4, 25
    manager = mp.Manager()
    pool = SharedPricingPool(n_constraints=5, max_cols=200, lock=manager.Lock())
    try:
        handle = pool.handle()
        sol_data = [(float(i), [(0, 1.0)], [i]) for i in range(n_per_worker)]
        with mp.Pool(n_workers) as p:
            async_result = p.starmap_async(_worker_add, [(handle, sol_data)] * n_workers)
            # Bound the wait: a worker that dies mid-task (e.g. a spawn-related
            # shared-memory failure) would otherwise make starmap hang forever.
            # Fail fast instead of stalling CI for hours.
            results = async_result.get(timeout=120)
        assert sum(len(r) for r in results) == n_workers * n_per_worker
        assert pool.count == n_workers * n_per_worker
    finally:
        pool.unlink()
        manager.shutdown()


# ── FilteredSharedPricingPool ─────────────────────────────────────────────────


def test_filtered_shared_pool():
    pool = SharedPricingPool(n_constraints=3, max_cols=20)
    try:
        i0 = pool.add(make_solution(5.0, [(0, 3.0)], [0]))
        i1 = pool.add(make_solution(5.0, [(0, 3.0)], [1]))
        i2 = pool.add(make_solution(5.0, [(0, 3.0)], [2]))
        duals = np.array([2.0, 0.0, 0.0])

        fpool = FilteredSharedPricingPool(pool, view_indices=np.array([i0, i2]))
        indices, _ = fpool.price(duals)
        assert set(indices.tolist()) == {i0, i2}
        assert i1 not in indices.tolist()

        fpool.remove_from_view([i0])
        indices, _ = fpool.price(duals)
        assert indices.tolist() == [i2]

        fpool.add_to_view([i1])
        indices, _ = fpool.price(duals)
        assert set(indices.tolist()) == {i1, i2}
    finally:
        pool.unlink()


# ── PricingPool (high-level) ──────────────────────────────────────────────────


def test_pricing_pool_basic():
    """PricingPool creates SolutionPool internally — no external pool needed."""
    pool = PricingPool(n_constraints=5, max_cols=20)
    try:
        sol = make_solution(10.0, [(0, 1.0)], [1])
        cpp_id = pool.add(sol)
        assert cpp_id != 0

        duals = np.array([11.0, 0.0, 0.0, 0.0, 0.0])
        indices, rcs = pool.price(duals)
        assert len(indices) == 1 and abs(rcs[0] - (-1.0)) < 1e-9
    finally:
        pool.close()


def test_pricing_pool_shared_shortcut():
    """`pool.price()` returns ColumnIds; `shared.price()` returns slot indices."""
    pool = PricingPool(n_constraints=5, max_cols=20)
    try:
        pool.add(make_solution(5.0, [(0, 3.0)], [0]))
        shared = pool.shared()
        duals = np.array([4.0, 0.0, 0.0, 0.0, 0.0])
        # pool.price() → ColumnIds (uint64)
        col_ids, rcs1 = pool.price(duals)
        assert col_ids.dtype == np.uint64
        assert len(col_ids) == 1
        # shared.price() → shared slot indices (intp)
        slot_ids, rcs2 = shared.price(duals)
        assert len(slot_ids) == 1
        # Reduced costs must match.
        assert list(rcs1) == list(rcs2)
    finally:
        pool.close()


def test_pricing_pool_new_filter_arc():
    """new_filter(forbidden_arc_ids) excludes matching columns from pricing."""
    pool = PricingPool(n_constraints=3, max_cols=20)
    try:
        s0 = make_solution(5.0, [(0, 3.0)], [0])
        s1 = make_solution(5.0, [(0, 3.0)], [1])
        pool.add(s0)
        pool.add(s1)

        sub = pool.new_filter(forbidden_arc_ids=[1])
        assert isinstance(sub, FilteredPricingPool)
        duals = np.array([2.0, 0.0, 0.0])
        indices, _ = sub.price(duals)
        assert len(indices) == 1  # only s0
    finally:
        pool.close()


def test_pricing_pool_new_filter_activity():
    """Activity args (max_age, min_usage_rate, max_last_rc) filter at C++ level.

    The activity args read ColumnActivity which is updated by the C++ pool's price()
    method, not by the shared-pool pricing.  We call the C++ pricing explicitly to
    populate last_reduced_cost and priced_count.
    """
    pool = PricingPool(n_constraints=3, max_cols=20)
    try:
        s0 = make_solution(5.0, [(0, 3.0)], [0])
        s1 = make_solution(5.0, [(0, 3.0)], [1])
        pool.add(s0)
        pool.add(s1)

        # Call C++ pricing to update last_reduced_cost and priced_count.
        duals_list = [2.0, 0.0, 0.0]
        pool._cpp_fp.price(duals_list, threshold=0.0)  # both rc=-1 < 0

        # max_last_rc=0.0: both last_rc=-1 < 0 → both included.
        sub = pool.new_filter(max_last_rc=0.0)
        duals = np.array([2.0, 0.0, 0.0])
        indices, _ = sub.price(duals)
        assert len(indices) == 2

        # max_age=1: both have age=0 (just priced) → both included (age <= max_age).
        sub2 = pool.new_filter(max_age=1)
        indices2, _ = sub2.price(duals)
        assert len(indices2) == 2

        # Age both columns by NOT pricing them (age increments).
        pool._cpp_fp.update_activity([])  # no basis columns → age++
        pool._cpp_fp.update_activity([])  # age = 2 now

        # max_age=1: age=2 > 1 → both excluded.
        sub3 = pool.new_filter(max_age=1)
        indices3, _ = sub3.price(duals)
        assert len(indices3) == 0
    finally:
        pool.close()


def test_public_price_updates_activity():
    """H-2: the public pool.price() updates C++ ColumnActivity, so activity-based
    filters work without a manual _cpp_fp.price() call."""
    pool = PricingPool(n_constraints=3, max_cols=20)
    try:
        cid = pool.add(make_solution(5.0, [(0, 3.0)], [0]))
        pool.add(make_solution(5.0, [(0, 3.0)], [1]))
        duals = np.array([2.0, 0.0, 0.0])  # both rc = 5 - 3*2 = -1 < 0
        ids, _ = pool.price(duals)
        assert len(ids) == 2
        # last_reduced_cost / priced_count / use_count are now populated.
        act = pool.get_activity(cid)
        assert act.priced_count == 1 and act.use_count == 1
        assert abs(act.last_reduced_cost - (-1.0)) < 1e-9
        assert abs(act.usage_rate() - 1.0) < 1e-9
        # max_last_rc=0.0 keeps only historically-negative columns; both qualify.
        # Would keep NONE if last_reduced_cost were still +inf (the H-2 bug).
        sub = pool.new_filter(max_last_rc=0.0)
        assert sub.column_count == 2
    finally:
        pool.close()


def test_filtered_public_price_updates_activity():
    """H-2: FilteredPricingPool.price() updates activity for its view as well."""
    pool = PricingPool(n_constraints=3, max_cols=20)
    try:
        cid = pool.add(make_solution(5.0, [(0, 3.0)], [0]))
        sub = pool.new_filter()
        sub.price(np.array([2.0, 0.0, 0.0]))
        assert sub.get_activity(cid).priced_count == 1
    finally:
        pool.close()


def test_price_track_activity_opt_out():
    """track_activity=False skips the C++ activity update (fast shared-only pricing)."""
    pool = PricingPool(n_constraints=3, max_cols=20)
    try:
        cid = pool.add(make_solution(5.0, [(0, 3.0)], [0]))
        duals = np.array([2.0, 0.0, 0.0])
        pool.price(duals, track_activity=False)
        assert pool.get_activity(cid).priced_count == 0  # untouched
        pool.price(duals)  # default tracks
        assert pool.get_activity(cid).priced_count == 1
    finally:
        pool.close()


def test_pricing_pool_remove_from_view_list():
    """remove_from_view accepts lists of arc_ids and col_ids."""
    pool = PricingPool(n_constraints=3, max_cols=20)
    try:
        c0 = pool.add(make_solution(5.0, [(0, 3.0)], [10]))
        c1 = pool.add(make_solution(5.0, [(0, 3.0)], [20]))
        sub = pool.new_filter()
        duals = np.array([2.0, 0.0, 0.0])

        # Exclude columns using arc 10 or 20 (both).
        sub.remove_from_view(arc_ids=[10, 20])
        indices, _ = sub.price(duals)
        assert len(indices) == 0

        # Backtrack.
        sub.add_to_view(col_ids=[c0, c1])
        indices, _ = sub.price(duals)
        assert len(indices) == 2
    finally:
        pool.close()


def test_pricing_pool_chain_filter():
    """new_filter() on a FilteredPricingPool further narrows the view."""
    pool = PricingPool(n_constraints=3, max_cols=20)
    try:
        pool.add(make_solution(5.0, [(0, 3.0)], [10]))
        pool.add(make_solution(5.0, [(0, 3.0)], [20]))
        sub = pool.new_filter(forbidden_arc_ids=[10])
        sub2 = sub.new_filter(forbidden_arc_ids=[20])
        duals = np.array([2.0, 0.0, 0.0])
        indices, _ = sub2.price(duals)
        assert len(indices) == 0
    finally:
        pool.close()


def test_pricing_pool_remove_stale():
    """remove_stale removes from C++ pool and invalidates shared pool."""
    pool = PricingPool(n_constraints=3, max_cols=20)
    try:
        sub = pool.new_filter()
        sub.add(make_solution(5.0, [(0, 3.0)], [0]))
        duals = np.array([2.0, 0.0, 0.0])

        # Column is visible.
        indices, _ = sub.price(duals)
        assert len(indices) == 1

        # Age the column (not in basis → age increments).
        sub._cpp_fp.update_activity([])
        sub._cpp_fp.update_activity([])
        sub.remove_stale(max_age=1)

        # Column should now be invisible.
        indices, _ = sub.price(duals)
        assert len(indices) == 0
    finally:
        pool.close()


def test_pricing_pool_price_matches_cpp():
    """pool.price() returns ColumnIds consistent with C++ pricing results."""
    pool = PricingPool(n_constraints=5, max_cols=50)
    try:
        solutions = [
            make_solution(5.0, [(0, 1.0), (1, 2.0)], [10, 11]),
            make_solution(3.0, [(0, 0.5)], [20, 21]),
            make_solution(8.0, [(1, 3.0)], [30, 31]),
        ]
        col_ids_map = {}  # ColumnId → solution
        for sol in solutions:
            cid = pool.add(sol)
            col_ids_map[cid] = sol

        duals = [4.0, 1.5, 0.0, 0.0, 0.0]
        # C++ pool pricing (returns all with rc < 0, unsorted).
        cpp_results = pool._cpp_fp.price(duals, threshold=0.0)
        cpp_ids = {pc.id for pc in cpp_results}

        # pool.price() returns ColumnIds (uint64), sorted best-first.
        returned_ids, rcs = pool.price(np.array(duals))
        assert returned_ids.dtype == np.uint64

        assert len(returned_ids) == len(cpp_ids)
        for col_id, rc in zip(returned_ids, rcs):
            sol = col_ids_map[int(col_id)]
            expected_rc = sol.column.cost - sum(
                float(row.coefficient) * duals[row.index]
                for row in sol.column.rows
                if row.index < len(duals)
            )
            assert abs(rc - expected_rc) < 1e-9
    finally:
        pool.close()


# ── get_column_ids / price_numpy (C++ bindings) ───────────────────────────────


def test_get_column_ids_numpy():
    """FilteredSolutionPool.get_column_ids() returns np.ndarray[uint64]."""
    pool = PricingPool(n_constraints=3, max_cols=20)
    try:
        c0 = pool.add(make_solution(5.0, [(0, 1.0)], [0]))
        c1 = pool.add(make_solution(3.0, [(1, 2.0)], [1]))
        ids = pool._cpp_fp.get_column_ids()
        assert isinstance(ids, np.ndarray)
        assert ids.dtype == np.uint64
        assert set(ids.tolist()) == {c0, c1}
    finally:
        pool.close()


def test_price_numpy_binding():
    """FilteredSolutionPool.price_numpy() returns (ids, rcs) as numpy arrays."""
    pool = PricingPool(n_constraints=3, max_cols=20)
    try:
        pool.add(make_solution(5.0, [(0, 3.0)], [0]))  # rc=-1 with duals=[2]
        pool.add(make_solution(5.0, [(0, 1.0)], [1]))  # rc=3 with duals=[2] (above 0)
        duals = [2.0, 0.0, 0.0]
        ids, rcs = pool._cpp_fp.price_numpy(duals, threshold=0.0)
        assert isinstance(ids, np.ndarray) and isinstance(rcs, np.ndarray)
        assert len(ids) == 1
        assert abs(rcs[0] - (-1.0)) < 1e-9
    finally:
        pool.close()


# ── New improvements ──────────────────────────────────────────────────────────


def test_price_returns_column_ids():
    """pool.price() and sub.price() return uint64 ColumnIds, not shared indices."""
    pool = PricingPool(n_constraints=3, max_cols=20)
    try:
        cid = pool.add(make_solution(5.0, [(0, 3.0)], [0]))
        duals = np.array([4.0, 0.0, 0.0])
        ids, rcs = pool.price(duals)
        assert ids.dtype == np.uint64
        assert len(ids) == 1
        assert int(ids[0]) == cid
        # Filtered pool also returns ColumnIds.
        sub = pool.new_filter()
        fids, frcs = sub.price(duals)
        assert fids.dtype == np.uint64
        assert len(fids) == 1
        assert int(fids[0]) == cid
    finally:
        pool.close()


def test_dedup_no_extra_shared_slot():
    """Adding the same path twice must not allocate a second shared slot."""
    pool = PricingPool(n_constraints=3, max_cols=20)
    try:
        s = make_solution(5.0, [(0, 1.0)], [99])
        c1 = pool.add(s)
        c2 = pool.add(s)  # duplicate path → same ColumnId
        assert c1 == c2
        assert pool.shared().count == 1  # only one shared slot
        assert pool.shared().active_count == 1
    finally:
        pool.close()


def test_dedup_refresh_updates_shared_pricing():
    """H-1: re-adding the same arc path with a changed column must refresh the shared
    pool so price() uses the latest cost/coefficients, not the first-seen (stale)
    ones."""
    pool = PricingPool(n_constraints=3, max_cols=20)
    try:
        c1 = pool.add(make_solution(5.0, [(0, 1.0)], [10, 11]))
        # Re-add the same arc path with a cheaper cost and a larger coefficient.
        c2 = pool.add(make_solution(3.0, [(0, 2.0)], [10, 11]))
        assert c1 == c2  # deduped by arc path
        assert pool.shared().count == 1  # no second shared slot

        # duals=[4]: stale column → 5 - 1*4 =  1 (>= 0, excluded);
        #            refreshed     → 3 - 2*4 = -5 (<  0, returned).
        duals = np.array([4.0, 0.0, 0.0])
        ids, rcs = pool.price(duals)
        assert len(ids) == 1 and int(ids[0]) == c1
        assert abs(rcs[0] - (-5.0)) < 1e-9  # refreshed coefficient, not the stale 1.0
    finally:
        pool.close()


def test_dedup_refresh_updates_shared_pricing_filtered():
    """H-1 (filtered): the refresh path also runs through FilteredPricingPool.add."""
    pool = PricingPool(n_constraints=3, max_cols=20)
    try:
        sub = pool.new_filter()
        c1 = sub.add(make_solution(5.0, [(0, 1.0)], [10, 11]))
        c2 = sub.add(make_solution(3.0, [(0, 2.0)], [10, 11]))
        assert c1 == c2
        assert pool.shared().count == 1

        duals = np.array([4.0, 0.0, 0.0])
        ids, rcs = sub.price(duals)
        assert len(ids) == 1 and int(ids[0]) == c1
        assert abs(rcs[0] - (-5.0)) < 1e-9
    finally:
        pool.close()


def test_dedup_identical_readd_short_circuit_preserves_data():
    """Re-adding an identical column hits the update() no-change short-circuit and must
    leave the stored cost/coefficients intact."""
    pool = PricingPool(n_constraints=3, max_cols=20)
    try:
        c1 = pool.add(make_solution(5.0, [(0, 1.0)], [10, 11]))
        c2 = pool.add(make_solution(5.0, [(0, 1.0)], [10, 11]))  # identical → short-circuit
        assert c1 == c2 and pool.shared().count == 1
        # rc = 5 - 1*6 = -1 confirms cost=5 and coef=1.0 survived intact.
        ids, rcs = pool.price(np.array([6.0, 0.0, 0.0]))
        assert len(ids) == 1 and int(ids[0]) == c1 and abs(rcs[0] - (-1.0)) < 1e-9
    finally:
        pool.close()


def test_id_to_shared_grows_dynamically():
    """_id_to_shared must grow when ColumnIds exceed initial capacity."""
    # Start with a tiny initial capacity to force growth.
    pool = PricingPool(n_constraints=3, max_cols=20)
    try:
        pool._id_to_shared = np.full(3, -1, dtype=np.int64)  # force small initial
        for i in range(1, 11):  # start at 1 so rc = i - 100*i < 0 for all
            pool.add(make_solution(float(i), [(0, float(i))], [i]))
        assert pool.shared().count == 10
        duals = np.zeros(3)
        duals[0] = 100.0
        ids, rcs = pool.price(duals)
        assert len(ids) == 10  # all columns have rc < 0
    finally:
        pool.close()


def test_filtered_pool_refresh():
    """Refresh() rebuilds the numpy mask from the current C++ view."""
    pool = PricingPool(n_constraints=3, max_cols=20)
    try:
        sub = pool.new_filter()
        duals = np.array([4.0, 0.0, 0.0])
        # No columns yet.
        ids, _ = sub.price(duals)
        assert len(ids) == 0
        # Add through the main pool (not through sub → mask not auto-updated).
        pool.add(make_solution(5.0, [(0, 3.0)], [0]))
        ids, _ = sub.price(duals)
        assert len(ids) == 0  # mask not yet updated
        # Refresh syncs mask from C++ view.
        sub.refresh()
        ids, _ = sub.price(duals)
        assert len(ids) == 1
    finally:
        pool.close()


def test_stats_properties():
    """column_count, shared_count, active_shared_count return correct values."""
    pool = PricingPool(n_constraints=3, max_cols=20)
    try:
        assert pool.column_count == 0
        assert pool.shared_count == 0
        pool.add(make_solution(5.0, [(0, 3.0)], [0]))
        pool.add(make_solution(3.0, [(0, 1.0)], [1]))
        assert pool.column_count == 2
        assert pool.shared_count == 2
        assert pool.active_shared_count == 2
        sub = pool.new_filter()
        assert sub.column_count == 2
        assert sub.shared_count == 2
        assert sub.active_shared_count == 2
    finally:
        pool.close()


def test_concurrent_pricing_shared_lock():
    """Price() and update_activity() now use shared_lock — test under threading."""
    import threading

    pool = PricingPool(n_constraints=3, max_cols=50)
    try:
        for i in range(10):
            pool.add(make_solution(5.0, [(0, float(i + 1))], [i]))
        duals = np.array([3.0, 0.0, 0.0])

        results = []
        errors = []

        def do_price():
            try:
                ids, rcs = pool.price(duals)
                results.append(len(ids))
            except Exception as e:  # noqa: BLE001
                errors.append(str(e))

        threads = [threading.Thread(target=do_price) for _ in range(4)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        assert not errors, f"Errors: {errors}"
        # All threads should see the same number of negative-rc columns.
        assert len(set(results)) == 1
    finally:
        pool.close()


def test_get_lp_arrays_binding():
    """SolutionPool.get_lp_arrays() returns four numpy arrays."""
    pool = PricingPool(n_constraints=5, max_cols=20)
    try:
        pool.add(make_solution(5.0, [(0, 1.0), (2, 2.0)], [0]))
        pool.add(make_solution(3.0, [(1, 0.5)], [1]))
        costs, row_starts, col_indices, col_values = pool._cpp_pool.get_lp_arrays()
        assert isinstance(costs, np.ndarray) and costs.dtype == np.float64
        assert isinstance(row_starts, np.ndarray) and row_starts.dtype == np.uint32
        assert len(costs) >= 2
        assert int(col_indices[0]) in {0, 1, 2}  # valid constraint index
    finally:
        pool.close()
