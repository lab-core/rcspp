"""Tests for SharedPricingPool and PricingPool."""

import multiprocessing as mp
import os
import sys

import numpy as np

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "../python_interface")))
sys.path.insert(0, os.path.abspath(os.path.dirname(__file__)))

from rcspp._core import solution_pool as _sp  # noqa: E402
from rcspp._core.graph import Column, Row, Solution  # noqa: E402
from rcspp.pricing_pool import PricingPool, SharedPricingPool  # noqa: E402

SolutionPool = _sp.SolutionPool


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


# ── SharedPricingPool unit tests ──────────────────────────────────────────────


def test_create_and_attach():
    pool = SharedPricingPool(n_constraints=10, max_cols=100)
    try:
        handle = pool.handle()
        worker = SharedPricingPool.attach(handle)
        try:
            # Write via pool, read via worker — verifies they share the same memory.
            pool.add(make_solution(42.0, [(3, 1.5)], [1]))
            assert worker.count == 1
            # matrix[:,0] = cost; matrix[:,j+1] = coef for constraint j
            assert abs(worker.matrix_view[0, 0] - 42.0) < 1e-9
            assert abs(worker.matrix_view[0, 4] - 1.5) < 1e-9  # constraint 3 → column 4
            # col_costs_view and row_matrix_view are zero-copy views.
            assert abs(float(worker.col_costs_view[0]) - 42.0) < 1e-9
            assert abs(float(worker.row_matrix_view[0, 3]) - 1.5) < 1e-9
        finally:
            worker.close()
    finally:
        pool.unlink()


def test_alignment():
    """matrix_offset must be 8-byte aligned for any max_cols value."""
    for max_cols in [1, 5, 7, 8, 100, 1_000]:
        pool = SharedPricingPool(n_constraints=5, max_cols=max_cols)
        try:
            assert (
                pool._matrix_offset % 8 == 0
            ), f"matrix_offset {pool._matrix_offset} not 8-byte aligned for max_cols={max_cols}"
        finally:
            pool.unlink()


def test_add_batch():
    pool = SharedPricingPool(n_constraints=5, max_cols=50)
    try:
        solutions = [make_solution(float(i), [(0, float(i))], [i]) for i in range(5)]
        idxs = pool.add_columns(solutions)
        assert idxs == [0, 1, 2, 3, 4]
        assert pool.count == 5
        assert pool.active_count == 5
        for i in range(5):
            assert abs(pool.col_costs_view[i] - float(i)) < 1e-9
            assert abs(pool.row_matrix_view[i, 0] - float(i)) < 1e-9
    finally:
        pool.unlink()


def test_price_sorted_by_rc():
    """Results must be sorted ascending by reduced cost (most improving first)."""
    pool = SharedPricingPool(n_constraints=5, max_cols=50)
    try:
        # With duals=[1,0,0,0,0]: rc = col_cost - 1*coef_at_0
        # col 0: rc = 1 - 1*4 = -3    (returned)
        # col 1: rc = 5 - 1*2 = 3     (above threshold, NOT returned)
        # col 2: rc = 1 - 1*5 = -4    (returned, most negative = first)
        # col 3: rc = 2 - 1*1 = 1     (above threshold, NOT returned)
        pool.add(make_solution(1.0, [(0, 4.0)], [0]))  # rc=-3
        pool.add(make_solution(5.0, [(0, 2.0)], [1]))  # rc=3, not returned
        pool.add(make_solution(1.0, [(0, 5.0)], [2]))  # rc=-4 (most negative)
        pool.add(make_solution(2.0, [(0, 1.0)], [3]))  # rc=1, not returned
        duals = np.array([1.0, 0.0, 0.0, 0.0, 0.0])
        indices, rcs = pool.price(duals)
        assert len(rcs) == 2
        assert list(rcs) == sorted(rcs), "results must be sorted ascending by rc"
        assert rcs[0] <= rcs[-1], "most negative rc must come first"
        assert all(rc < -1e-9 for rc in rcs)
    finally:
        pool.unlink()


def test_price_default_threshold():
    """Default threshold is -1e-9, not 0 — near-zero rc columns must be excluded."""
    pool = SharedPricingPool(n_constraints=2, max_cols=20)
    try:
        # Only one column in pool: cost=1, coef[(0,1)]. rc = 1 - dual[0]*1.
        pool.add(make_solution(1.0, [(0, 1.0)], [0]))
        duals_zero_rc = np.array([1.0, 0.0])  # rc = 1-1 = 0, must NOT be returned
        indices, rcs = pool.price(duals_zero_rc)
        assert len(indices) == 0, "rc=0 must not be returned with default threshold=-1e-9"
        duals_neg_rc = np.array([3.0, 0.0])  # rc = 1-3 = -2, must be returned
        indices, rcs = pool.price(duals_neg_rc)
        assert len(indices) == 1 and abs(rcs[0] - (-2.0)) < 1e-9
    finally:
        pool.unlink()


def test_filtered_shared_pool():
    """FilteredSharedPricingPool only returns columns in its view_mask."""
    from rcspp.pricing_pool import FilteredSharedPricingPool  # noqa: E402

    pool = SharedPricingPool(n_constraints=3, max_cols=20)
    try:
        i0 = pool.add(make_solution(5.0, [(0, 3.0)], [0]))  # rc=-1 (in view)
        i1 = pool.add(make_solution(5.0, [(0, 3.0)], [1]))  # rc=-1 (NOT in view)
        i2 = pool.add(make_solution(5.0, [(0, 3.0)], [2]))  # rc=-1 (in view)

        # Create a filtered view including only columns i0 and i2.
        fpool = FilteredSharedPricingPool(pool, view_indices=np.array([i0, i2]))
        duals = np.array([2.0, 0.0, 0.0])
        indices, rcs = fpool.price(duals)
        assert set(indices.tolist()) == {i0, i2}
        assert i1 not in indices.tolist()

        # Remove i0 from view.
        fpool.remove_from_view([i0])
        indices, rcs = fpool.price(duals)
        assert indices.tolist() == [i2]

        # Add i1 to view.
        fpool.add_to_view([i1])
        indices, rcs = fpool.price(duals)
        assert set(indices.tolist()) == {i1, i2}
    finally:
        pool.unlink()


def test_pricing_pool_new_numpy_filter():
    """new_numpy_filter() mirrors the C++ FilteredSolutionPool's column set."""
    cpp_pool = SolutionPool()
    fp = cpp_pool.new_filter()
    pp = PricingPool(fp, n_constraints=3, max_cols=20)
    try:
        s0 = make_solution(5.0, [(0, 3.0)], [0])
        s1 = make_solution(5.0, [(0, 3.0)], [1])
        pp.add(s0)
        pp.add(s1)

        # Create a further-filtered C++ view excluding s1 (arc_id=1).
        restricted = fp.new_filter(forbidden_arc_ids=[1])
        nf = pp.new_numpy_filter(restricted)
        duals = np.array([2.0, 0.0, 0.0])
        indices, rcs = nf.price(duals)
        # Only s0 should appear (s1 is forbidden).
        assert len(indices) == 1
    finally:
        pp.close()


def test_add_and_price_simple():
    pool = SharedPricingPool(n_constraints=5, max_cols=50)
    try:
        # col.cost=10, rows=[(0, 1.0)]; duals=[3] → rc=10-3=7 (not returned)
        # duals=[11] → rc=10-11=-1 (returned)
        idx = pool.add(make_solution(10.0, [(0, 1.0)], [1]))

        indices, rcs = pool.price(np.array([3.0, 0.0, 0.0, 0.0, 0.0]))
        assert len(indices) == 0

        indices, rcs = pool.price(np.array([11.0, 0.0, 0.0, 0.0, 0.0]))
        assert len(indices) == 1
        assert abs(rcs[0] - (-1.0)) < 1e-9
        assert indices[0] == idx
    finally:
        pool.unlink()


def test_price_multi_row():
    pool = SharedPricingPool(n_constraints=10, max_cols=50)
    try:
        # col.cost=2.5, rows=[(0,0.1),(1,0.2)]; duals=[3,4] → rc=2.5-0.3-0.8=1.4
        pool.add(make_solution(2.5, [(0, 0.1), (1, 0.2)], [10, 11]))
        indices, rcs = pool.price(np.array([3.0, 4.0] + [0.0] * 8), threshold=2.0)
        assert len(indices) == 1
        assert abs(rcs[0] - 1.4) < 1e-6
    finally:
        pool.unlink()


def test_invalidate_hides_column():
    pool = SharedPricingPool(n_constraints=5, max_cols=50)
    try:
        idx = pool.add(make_solution(1.0, [(0, 0.1)], [1]))
        # Before invalidation, column is returned.
        indices, rcs = pool.price(np.array([100.0, 0.0, 0.0, 0.0, 0.0]))
        assert len(indices) == 1

        pool.invalidate([idx])
        indices, rcs = pool.price(np.array([100.0, 0.0, 0.0, 0.0, 0.0]))
        assert len(indices) == 0
    finally:
        pool.unlink()


def test_dynamic_rows():
    """Columns added before a new cut have coef=0 for the new constraint."""
    pool = SharedPricingPool(n_constraints=10, max_cols=50)
    try:
        # Old column: only covers row 0.
        pool.add(make_solution(5.0, [(0, 1.0)], [1]))
        # New cut added (row 5). New column covers both row 0 and row 5.
        pool.add(make_solution(5.0, [(0, 1.0), (5, 2.0)], [2]))

        duals = np.zeros(6, dtype=np.float64)
        duals[0] = 3.0
        duals[5] = 1.0  # new cut dual

        # Old column: rc = 5 - 3*1 - 0 = 2 (not returned at threshold 0)
        # New column: rc = 5 - 3*1 - 1*2 = 0 (not returned at threshold 0)
        indices, rcs = pool.price(duals, threshold=0.0)
        assert len(indices) == 0

        # With higher duals, both should be negative.
        duals[0] = 6.0
        duals[5] = 0.5
        # Old column: rc = 5 - 6*1 - 0 = -1 (returned)
        # New column: rc = 5 - 6*1 - 0.5*2 = -2 (returned)
        indices, rcs = pool.price(duals)
        assert len(indices) == 2
    finally:
        pool.unlink()


def _worker_add(handle: dict, sol_data: list[tuple]) -> list[int]:
    """Worker function: attach to pool, build solutions locally, add them."""
    import os
    import sys  # noqa: E401

    sys.path.insert(
        0, os.path.abspath(os.path.join(os.path.dirname(__file__), "../python_interface"))
    )
    from rcspp._core.graph import Column, Row
    from rcspp._core.graph import Solution as _Sol  # noqa: E402

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
    """Four workers simultaneously add 25 columns each → total 100."""
    n_workers = 4
    n_per_worker = 25
    manager = mp.Manager()
    pool = SharedPricingPool(n_constraints=5, max_cols=200, lock=manager.Lock())
    try:
        handle = pool.handle()
        # Pass plain tuples (picklable) instead of Solution objects.
        sol_data = [(float(i), [(0, 1.0)], [i]) for i in range(n_per_worker)]
        with mp.Pool(n_workers) as p:
            results = p.starmap(_worker_add, [(handle, sol_data)] * n_workers)

        total = sum(len(r) for r in results)
        assert total == n_workers * n_per_worker
        assert pool.count == n_workers * n_per_worker
    finally:
        pool.unlink()
        manager.shutdown()


def test_price_matches_cpp_pool():
    """SharedPricingPool.price() must match FilteredSolutionPool.price() results."""
    solutions = [
        make_solution(5.0, [(0, 1.0), (1, 2.0)], [10, 11]),
        make_solution(3.0, [(0, 0.5)], [20, 21]),
        make_solution(8.0, [(1, 3.0)], [30, 31]),
    ]
    duals = [4.0, 1.5]

    # C++ pool pricing.
    cpp_pool = SolutionPool()
    fp = cpp_pool.new_filter()
    for sol in solutions:
        fp.add(sol)
    cpp_results = fp.price(duals, threshold=0.0)
    cpp_ids = {pc.id for pc in cpp_results}

    # Shared pool pricing.
    shared = SharedPricingPool(n_constraints=5, max_cols=50)
    try:
        idx_to_sol = {}
        for sol in solutions:
            idx = shared.add(sol)
            idx_to_sol[idx] = sol
        indices, rcs = shared.price(np.array(duals + [0.0, 0.0, 0.0]), threshold=0.0)

        assert len(indices) == len(
            cpp_ids
        ), f"C++ returned {len(cpp_ids)} columns, shared returned {len(indices)}"
        for idx, rc in zip(indices, rcs):
            # Verify reduced cost matches within tolerance.
            sol = idx_to_sol[idx]
            expected_rc = sol.column.cost - sum(
                float(row.coefficient) * duals[row.index]
                for row in sol.column.rows
                if row.index < len(duals)
            )
            assert abs(rc - expected_rc) < 1e-9, f"rc mismatch: {rc} vs {expected_rc}"
    finally:
        shared.unlink()


# ── PricingPool integration test ──────────────────────────────────────────────


def test_pricing_pool_add_and_price():
    cpp_pool = SolutionPool()
    fp = cpp_pool.new_filter()
    pp = PricingPool(fp, n_constraints=5, max_cols=50)
    try:
        sol = make_solution(10.0, [(0, 1.0)], [1])
        cpp_id = pp.add(sol)
        assert cpp_id != 0

        # price_shared should return the column.
        indices, rcs = pp.price_shared(np.array([11.0, 0.0, 0.0, 0.0, 0.0]))
        assert len(indices) == 1

        # C++ pool price() should also return it.
        cpp_results = pp.price([11.0, 0.0, 0.0, 0.0, 0.0])
        assert len(cpp_results) == 1
    finally:
        pp.close()


def test_pricing_pool_remove_stale_invalidates_shared():
    cpp_pool = SolutionPool()
    fp = cpp_pool.new_filter()
    pp = PricingPool(fp, n_constraints=5, max_cols=50)
    try:
        sol = make_solution(10.0, [(0, 1.0)], [1])
        cpp_id = pp.add(sol)

        # Column is visible before removal.
        indices, _ = pp.price_shared(np.array([11.0, 0.0, 0.0, 0.0, 0.0]))
        assert len(indices) == 1

        # Simulate the column becoming stale (age it manually then remove).
        fp.update_activity([])  # no basis columns → age increments
        fp.update_activity([])
        removed = pp.remove_stale(max_age=1)
        assert cpp_id in removed

        # Column should now be invisible in shared pricing.
        indices, _ = pp.price_shared(np.array([11.0, 0.0, 0.0, 0.0, 0.0]))
        assert len(indices) == 0
    finally:
        pp.close()
