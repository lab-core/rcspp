#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Tests for pybind11 binding code paths not exercised by other tests.

Covers: SolveResult protocol, Solution.to_arrays(), memory helpers, FilteredSolutionPool
predicate-based filters, numpy methods, activity filters, make_filter statics, add_filter,
remove_if/global_remove_if, remove_arcs_if/restore_arcs_if, AlgorithmParams helpers,
check_interrupted, get_resource_factory, Node.resource, Arc.extender, and related API.
"""

import os
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../python/src"))

import rcspp._core as _core  # noqa: E402
from rcspp._core import solution_pool as _sp  # noqa: E402
from rcspp._core.graph import AlgorithmParams, Column, Row, Solution  # noqa: E402
from rcspp.graph import ResourceGraph  # noqa: E402
from rcspp.resource import (  # noqa: E402
    AdditionExtensionFunction,
    TrivialFeasibilityFunction,
    ValueCostFunction,
    ValueDominanceFunction,
)

SolutionPool = _sp.SolutionPool
FilteredSolutionPool = _sp.FilteredSolutionPool


# ── Helpers ──────────────────────────────────────────────────────────────────


def _make_graph():
    """Return a solved ResourceGraph (0→1→2) with one solution."""
    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_node(0, source=True)
    rg.add_node(1)
    rg.add_node(2, sink=True)
    rg.add_arc(1.0, 0, 1, cost=-2.0)
    rg.add_arc(1.0, 1, 2, cost=-3.0)
    return rg


def _make_sol(cost: float = 5.0, arc_ids: list | None = None) -> Solution:
    """Build a minimal Solution for pool tests."""
    if arc_ids is None:
        arc_ids = [10, 11]
    col = Column()
    col.cost = cost
    col.rows = [Row(index=0, coefficient=1.0)]
    sol = Solution()
    sol.cost = cost
    sol.path_arc_ids = arc_ids
    sol.path_node_ids = list(range(len(arc_ids) + 1))
    sol.column = col
    return sol


def _make_pool_with_two_solutions() -> tuple:
    """Return (SolutionPool, FilteredSolutionPool) with two entries."""
    pool = SolutionPool()
    fp = pool.new_filter()
    fp.add(_make_sol(5.0, [10, 11]))
    fp.add(_make_sol(7.0, [20, 21]))
    return pool, fp


# ── Memory helpers (rcspp.cpp) ────────────────────────────────────────────────


def test_process_memory_bytes():
    assert _core.process_memory_bytes() >= 0


def test_available_memory_bytes():
    assert _core.available_memory_bytes() >= 0


# ── SolveResult protocol (graph.cpp) ────────────────────────────────────────


def test_solve_result_iter():
    """SolveResult supports for-loop iteration."""
    rg = _make_graph()
    result = rg.solve()
    collected = [sol for sol in result]
    assert len(collected) > 0


def test_solve_result_getitem_negative_index():
    """SolveResult supports negative indexing."""
    rg = _make_graph()
    result = rg.solve()
    assert result[-1] is not None


def test_solve_result_getitem_out_of_range():
    """SolveResult.__getitem__ raises IndexError on out-of-range."""
    rg = _make_graph()
    result = rg.solve()
    with pytest.raises(IndexError):
        _ = result[9999]


def test_solve_result_repr():
    """SolveResult.__repr__ returns a non-empty string."""
    rg = _make_graph()
    result = rg.solve()
    s = repr(result)
    assert "SolveResult" in s


def test_solve_result_bool():
    """SolveResult is truthy when it has solutions."""
    rg = _make_graph()
    result = rg.solve()
    assert bool(result)


# ── Row constructor (graph.cpp) ───────────────────────────────────────────────


def test_row_constructor_with_args():
    """Row(index=, coefficient=) constructor populates both fields."""
    r = Row(index=3, coefficient=2.5)
    assert r.index == 3
    assert abs(r.coefficient - 2.5) < 1e-9


# ── Solution.to_arrays (graph.cpp) ────────────────────────────────────────────


def test_solution_to_arrays():
    """Solution.to_arrays() returns correct numpy arrays."""
    pytest.importorskip("numpy")
    sol = _make_sol(5.0, [10, 11])
    cost, nodes, ridx, rcoeff = sol.to_arrays()
    assert abs(cost - 5.0) < 1e-9
    assert list(nodes) == [0, 1, 2]
    assert list(ridx) == [0]
    assert abs(rcoeff[0] - 1.0) < 1e-9


# ── Graph.arc_ids (graph.cpp / graph_impl.hpp) ─────────────────────────────────


def test_arc_ids_lambda():
    """arc_ids() returns the correct number of arc IDs."""
    rg = _make_graph()
    ids = rg.arc_ids()
    assert len(ids) == 2


# ── Node / Arc repr and accessors (graph.cpp) ────────────────────────────────


def test_node_repr_contains_id():
    """Node.__repr__ returns a string containing the node id."""
    rg = _make_graph()
    node = rg.get_node(0)
    assert node is not None
    s = repr(node)
    assert "0" in s


def test_arc_origin_and_destination():
    """Arc.origin() and Arc.destination() return valid node objects."""
    rg = _make_graph()
    arc = rg.get_arc(0)
    assert arc is not None
    origin = arc.origin()
    dest = arc.destination()
    assert origin is not None
    assert dest is not None


# ── SolutionPool.make_filter (solution_pool.cpp) ──────────────────────────────


def test_solution_pool_make_filter_returns_callable():
    """SolutionPool.make_filter(forbidden_arc_ids=[...]) returns a predicate."""
    pred = SolutionPool.make_filter(forbidden_arc_ids=[999])
    assert callable(pred)


# ── FilteredSolutionPool.make_filter (solution_pool.cpp) ──────────────────────


def test_filtered_pool_make_filter_returns_callable():
    """FilteredSolutionPool.make_filter(forbidden_arc_ids=[...]) returns a predicate."""
    pred = FilteredSolutionPool.make_filter(forbidden_arc_ids=[999])
    assert callable(pred)


# ── SolutionPool.new_filter with predicate (solution_pool.cpp lines 64, 89-103)


def test_pool_new_filter_with_predicate():
    """SolutionPool.new_filter(filter=callable) creates a filtered view."""
    pool, _ = _make_pool_with_two_solutions()
    fp2 = pool.new_filter(filter=lambda sol: sol.cost < 6.0)
    assert fp2 is not None
    assert len(fp2) <= 2


def test_pool_new_filter_predicate_and_arc_filter():
    """new_filter with both predicate and row/arc constraints (combined path)."""
    pool, _ = _make_pool_with_two_solutions()
    fp2 = pool.new_filter(
        filter=lambda sol: sol.cost < 10.0,
        forbidden_arc_ids=[999],
    )
    assert fp2 is not None


# ── SolutionPool.new_filter with activity (solution_pool.cpp lines 106-119) ───


def test_pool_new_filter_with_max_age():
    """new_filter(max_age=...) applies remove_if with activity filter."""
    pool, fp = _make_pool_with_two_solutions()
    fp.price([0.0])  # give entries non-zero priced_count
    fp2 = pool.new_filter(max_age=100)
    assert fp2 is not None


def test_pool_new_filter_with_min_usage_rate():
    """new_filter(min_usage_rate=...) applies usage-rate filter."""
    pool, fp = _make_pool_with_two_solutions()
    fp.price([0.0])
    fp2 = pool.new_filter(min_usage_rate=0.0)
    assert fp2 is not None


def test_pool_new_filter_with_max_last_rc():
    """new_filter(max_last_rc=...) applies last-rc filter."""
    pool, fp = _make_pool_with_two_solutions()
    fp.price([0.0])
    fp2 = pool.new_filter(max_last_rc=1e9)
    assert fp2 is not None


# ── FilteredSolutionPool.new_filter with predicate ────────────────────────────


def test_filtered_pool_new_filter_with_predicate():
    """FilteredSolutionPool.new_filter(filter=callable) chains a filter."""
    pool, fp = _make_pool_with_two_solutions()
    fp2 = fp.new_filter(filter=lambda sol: sol.cost < 6.0)
    assert fp2 is not None


def test_filtered_pool_new_filter_predicate_and_arc():
    """FilteredSolutionPool.new_filter with both predicate and arc constraints."""
    pool, fp = _make_pool_with_two_solutions()
    fp2 = fp.new_filter(filter=lambda sol: True, forbidden_arc_ids=[999])
    assert fp2 is not None


def test_filtered_pool_new_filter_with_max_age():
    """FilteredSolutionPool.new_filter(max_age=...) applies activity filter."""
    pool, fp = _make_pool_with_two_solutions()
    fp.price([0.0])
    fp2 = fp.new_filter(max_age=100)
    assert fp2 is not None


# ── FilteredSolutionPool.add_filter (solution_pool.cpp lines 298, 318-331) ────


def test_filtered_pool_add_filter_arc():
    """add_filter(forbidden_arc_ids=[...]) narrows the view in-place."""
    pool, fp = _make_pool_with_two_solutions()
    before = len(fp)
    fp.add_filter(forbidden_arc_ids=[999])
    assert len(fp) <= before


def test_filtered_pool_add_filter_predicate():
    """add_filter(filter=callable) narrows the view by predicate in-place."""
    pool, fp = _make_pool_with_two_solutions()
    fp.add_filter(filter=lambda sol: sol.cost < 6.0)
    assert len(fp) <= 2


def test_filtered_pool_add_filter_combined():
    """add_filter with both predicate and arc constraints (combined path)."""
    pool, fp = _make_pool_with_two_solutions()
    fp.add_filter(filter=lambda sol: True, forbidden_arc_ids=[999])
    assert len(fp) <= 2


# ── FilteredSolutionPool.remove_if / global_remove_if ────────────────────────


def test_filtered_pool_remove_if_predicate():
    """remove_if(pred) removes entries matching the predicate (local)."""
    pool, fp = _make_pool_with_two_solutions()
    removed = fp.remove_if(lambda cid, sol, act: sol.cost > 6.0)
    assert isinstance(removed, list)


def test_filtered_pool_global_remove_if_predicate():
    """global_remove_if(pred) hard-deletes matching entries from the pool."""
    pool, fp = _make_pool_with_two_solutions()
    removed = fp.global_remove_if(lambda cid, sol, act: False)
    assert removed == []


# ── FilteredSolutionPool.remove_stale / remove_if_arc_present ────────────────


def test_filtered_pool_remove_stale():
    """remove_stale(max_age) runs without error."""
    pool, fp = _make_pool_with_two_solutions()
    fp.remove_stale(max_age=1000)


def test_filtered_pool_remove_if_arc_present():
    """remove_if_arc_present(arc_id) removes entries containing the arc."""
    pool, fp = _make_pool_with_two_solutions()
    fp.remove_if_arc_present(arc_id=999)
    assert len(fp) == 2  # arc 999 not in any solution


def test_filtered_pool_global_remove_if_arc_present():
    """global_remove_if_arc_present(arc_id) hard-deletes matching entries."""
    pool, fp = _make_pool_with_two_solutions()
    fp.global_remove_if_arc_present(arc_id=999)
    assert len(fp) == 2


def test_filtered_pool_global_remove_stale():
    """global_remove_stale(max_age) runs without error."""
    pool, fp = _make_pool_with_two_solutions()
    fp.global_remove_stale(max_age=1000)


# ── FilteredSolutionPool.get_column_ids (solution_pool.cpp line 420) ─────────


def test_filtered_pool_get_column_ids():
    """get_column_ids() returns a uint64 numpy array with correct length."""
    pytest.importorskip("numpy")
    pool, fp = _make_pool_with_two_solutions()
    ids = fp.get_column_ids()
    assert len(ids) == 2


# ── FilteredSolutionPool.price_numpy (solution_pool.cpp lines 450, 455) ──────


def test_filtered_pool_price_numpy():
    """price_numpy(duals) returns (ids, rcs) numpy arrays."""
    np = pytest.importorskip("numpy")
    pool, fp = _make_pool_with_two_solutions()
    ids, rcs = fp.price_numpy(np.array([0.0]), threshold=0.0)
    assert len(ids) == len(rcs)


# ── check_interrupted (graph.cpp) ─────────────────────────────────────────────


def test_check_interrupted_no_raise():
    """check_interrupted() is a no-op when no SIGINT is pending."""
    _core.graph.check_interrupted()


# ── AlgorithmParams.check / could_be_non_optimal (graph.cpp) ─────────────────


def test_algorithm_params_check():
    """AlgorithmParams.check() runs without error on a default-constructed params."""
    p = AlgorithmParams()
    p.check()


def test_algorithm_params_could_be_non_optimal():
    """AlgorithmParams.could_be_non_optimal() returns a bool."""
    p = AlgorithmParams()
    result = p.could_be_non_optimal()
    assert isinstance(result, bool)


# ── remove_arcs_if / restore_arcs_if (graph_impl.hpp) ────────────────────────


def test_remove_arcs_if():
    """remove_arcs_if(pred) removes arcs for which pred returns True."""
    rg = _make_graph()
    removed = rg.remove_arcs_if(lambda arc: arc.id == 0)
    assert 0 in removed


def test_restore_arcs_if():
    """restore_arcs_if(pred) restores previously removed arcs matching pred."""
    rg = _make_graph()
    rg.remove_arc(0)
    restored = rg.restore_arcs_if(lambda arc: arc.id == 0)
    assert 0 in restored


# ── FilteredSolutionPool.cleanup / sort_by_lp_index / pool ───────────────────


def test_filtered_pool_cleanup():
    """Cleanup() runs without error."""
    _, fp = _make_pool_with_two_solutions()
    fp.cleanup()


def test_filtered_pool_sort_by_lp_index():
    """sort_by_lp_index() runs without error."""
    _, fp = _make_pool_with_two_solutions()
    fp.sort_by_lp_index()


def test_filtered_pool_pool_property():
    """fp.pool() returns the parent SolutionPool (not None)."""
    pool, fp = _make_pool_with_two_solutions()
    parent = fp.pool()
    assert parent is not None
