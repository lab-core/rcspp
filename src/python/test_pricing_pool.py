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
            results = p.starmap(_worker_add, [(handle, sol_data)] * n_workers)
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
    """`pool.shared()` and `pool.handle()` + `PricingPool.attach()` are equivalent."""
    pool = PricingPool(n_constraints=5, max_cols=20)
    try:
        pool.add(make_solution(5.0, [(0, 3.0)], [0]))
        shared = pool.shared()
        duals = np.array([4.0, 0.0, 0.0, 0.0, 0.0])
        indices1, rcs1 = pool.price(duals)
        indices2, rcs2 = shared.price(duals)
        assert list(indices1) == list(indices2)
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


def test_pricing_pool_remove_from_view_list():
    """remove_from_view accepts lists of arc_ids and cpp_ids."""
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
        sub.add_to_view(cpp_ids=[c0, c1])
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
    """pool.price() results are consistent with what the C++ pool prices."""
    pool = PricingPool(n_constraints=5, max_cols=50)
    try:
        solutions = [
            make_solution(5.0, [(0, 1.0), (1, 2.0)], [10, 11]),
            make_solution(3.0, [(0, 0.5)], [20, 21]),
            make_solution(8.0, [(1, 3.0)], [30, 31]),
        ]
        for sol in solutions:
            pool.add(sol)

        duals = [4.0, 1.5, 0.0, 0.0, 0.0]
        # C++ pool pricing (returns all with rc < 0, unsorted).
        cpp_results = pool._cpp_fp.price(duals, threshold=0.0)
        cpp_ids = {pc.id for pc in cpp_results}

        # Shared pool pricing (sorted, threshold=-1e-9).
        indices, rcs = pool.price(np.array(duals))

        assert len(indices) == len(cpp_ids)
        for idx, rc in zip(indices, rcs):
            sol = solutions[idx]
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
