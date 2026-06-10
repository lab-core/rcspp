#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# flake8: noqa

import math
import os
import sys

import networkx as nx

sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "python", "src")
)

from rcspp.graph import Algorithm, AlgorithmParams, ResourceGraph
from rcspp.resource import (
    AdditionExtensionFunction,
    MinMaxFeasibilityFunction,
    TrivialCostFunction,
    TrivialFeasibilityFunction,
    UnionExtensionFunction,
    ValueCostFunction,
    ValueDominanceFunction,
)

# ── Helpers ───────────────────────────────────────────────────────────────────


def make_real_rg():
    """ResourceGraph with one real (cost) resource."""
    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    return rg


def make_real_int_rg():
    """ResourceGraph with a real cost resource and an int hop-count resource."""
    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_int_resource(
        AdditionExtensionFunction(),
        MinMaxFeasibilityFunction(0, 3),  # at most 3 hops
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    return rg


def assert_raises(exc_type, fn, *args, keyword=None, **kwargs):
    """Assert fn(*args, **kwargs) raises exc_type, optionally checking the message."""
    try:
        fn(*args, **kwargs)
    except exc_type as exc:
        if keyword is not None and keyword.lower() not in str(exc).lower():
            raise AssertionError(f"Expected '{keyword}' in error message, got: {exc}") from exc
        return  # expected
    raise AssertionError(f"Expected {exc_type.__name__} but no exception was raised")


# ── Test 1: basic end-to-end ──────────────────────────────────────────────────


def test_basic_networkx():
    """Build a graph from NetworkX, solve, and verify the optimal cost."""
    G = nx.DiGraph()
    G.add_node(0, source=True)
    G.add_node(1)
    G.add_node(2, sink=True)
    G.add_edge(0, 1, resource=(5.0,), cost=5.0)
    G.add_edge(0, 2, resource=(10.0,), cost=10.0)
    G.add_edge(1, 2, resource=(3.0,), cost=3.0)

    rg = make_real_rg()
    rg.from_networkx(G)
    sols = rg.solve()
    assert len(sols) >= 1, "Expected at least one solution"
    assert math.isclose(
        sols[0].cost, 8.0, abs_tol=1e-6
    ), f"Expected optimal cost 8.0, got {sols[0].cost}"
    assert sols[0].path_node_ids == [
        0,
        1,
        2,
    ], f"Expected path [0, 1, 2], got {sols[0].path_node_ids}"


# ── Test 2: mixed resources (real + int) ──────────────────────────────────────


def test_mixed_resources_networkx():
    """Two-resource graph (cost + hop count) built from NetworkX."""
    G = nx.DiGraph()
    G.add_node(0, source=True)
    G.add_node(1)
    G.add_node(2)
    G.add_node(3, sink=True)
    # (real_cost, int_hops) per arc
    G.add_edge(0, 1, resource=(1.0, 1), cost=1.0)
    G.add_edge(0, 2, resource=(10.0, 1), cost=10.0)
    G.add_edge(1, 2, resource=(1.0, 1), cost=1.0)
    G.add_edge(1, 3, resource=(1.0, 1), cost=1.0)
    G.add_edge(2, 3, resource=(1.0, 1), cost=1.0)

    rg = make_real_int_rg()
    rg.from_networkx(G)
    sols = rg.solve()
    assert len(sols) >= 1, "Expected at least one solution"
    # Optimal: 0→1→3, cost 2, 2 hops (within the limit of 3)
    assert math.isclose(
        sols[0].cost, 2.0, abs_tol=1e-6
    ), f"Expected optimal cost 2.0, got {sols[0].cost}"


# ── Test 3: constructor shortcut ──────────────────────────────────────────────


def test_from_networkx_consistent():
    """Two separate ResourceGraph instances built from the same NetworkX graph give the
    same result."""
    G = nx.DiGraph()
    G.add_node(0, source=True)
    G.add_node(1, sink=True)
    G.add_edge(0, 1, resource=(7.0,), cost=7.0)

    rg1 = make_real_rg()
    rg1.from_networkx(G)
    rg2 = make_real_rg()
    rg2.from_networkx(G)

    sols1 = rg1.solve()
    sols2 = rg2.solve()
    assert math.isclose(sols1[0].cost, sols2[0].cost, abs_tol=1e-6)


# ── Test 4: multiple sources and sinks ───────────────────────────────────────


def test_multiple_sources_sinks():
    """Graph with two sources and two sinks is accepted."""
    G = nx.DiGraph()
    G.add_node(0, source=True)
    G.add_node(1, source=True)
    G.add_node(2, sink=True)
    G.add_node(3, sink=True)
    G.add_edge(0, 2, resource=(5.0,), cost=5.0)
    G.add_edge(0, 3, resource=(9.0,), cost=9.0)
    G.add_edge(1, 2, resource=(3.0,), cost=3.0)
    G.add_edge(1, 3, resource=(7.0,), cost=7.0)

    rg = make_real_rg()
    rg.from_networkx(G)
    sols = rg.solve()
    assert len(sols) >= 1


# ── Error tests ───────────────────────────────────────────────────────────────


def test_missing_source_raises():
    """from_networkx must raise ValueError when no node has source=True."""
    G = nx.DiGraph()
    G.add_node(0)  # no source=True
    G.add_node(1, sink=True)
    G.add_edge(0, 1, resource=(1.0,))

    assert_raises(ValueError, make_real_rg().from_networkx, G, keyword="source")


def test_missing_sink_raises():
    """from_networkx must raise ValueError when no node has sink=True."""
    G = nx.DiGraph()
    G.add_node(0, source=True)
    G.add_node(1)  # no sink=True
    G.add_edge(0, 1, resource=(1.0,))

    assert_raises(ValueError, make_real_rg().from_networkx, G, keyword="sink")


def test_arc_missing_resource_raises():
    """An arc without 'resource' data must raise ValueError when resources are
    registered."""
    G = nx.DiGraph()
    G.add_node(0, source=True)
    G.add_node(1, sink=True)
    G.add_edge(0, 1)  # missing resource=(...)

    assert_raises(ValueError, make_real_rg().from_networkx, G, keyword="resource")


def test_some_arcs_missing_resource_raises():
    """If even one arc is missing resource data, a clear error is raised."""
    G = nx.DiGraph()
    G.add_node(0, source=True)
    G.add_node(1)
    G.add_node(2, sink=True)
    G.add_edge(0, 1, resource=(5.0,), cost=5.0)
    G.add_edge(1, 2)  # missing resource — only this one

    assert_raises(ValueError, make_real_rg().from_networkx, G, keyword="resource")


def test_wrong_resource_count_raises():
    """Arc resource tuple length must match number of registered resources."""
    G = nx.DiGraph()
    G.add_node(0, source=True)
    G.add_node(1, sink=True)
    G.add_edge(0, 1, resource=(1.0, 99.0))  # 2 values, only 1 resource registered

    assert_raises(ValueError, make_real_rg().from_networkx, G, keyword="resource")


def test_no_resources_raises():
    """from_networkx must raise ValueError when no resources have been registered."""
    G = nx.DiGraph()
    G.add_node(0, source=True)
    G.add_node(1, sink=True)
    G.add_edge(0, 1, resource=(1.0,))

    rg = ResourceGraph()  # no resources registered
    assert_raises(ValueError, rg.from_networkx, G)


# ── Run all tests ─────────────────────────────────────────────────────────────

if __name__ == "__main__":
    tests = [
        ("basic NetworkX construction and solve", test_basic_networkx),
        ("mixed resources (real + int)", test_mixed_resources_networkx),
        ("from_networkx consistent across instances", test_from_networkx_consistent),
        ("multiple sources and sinks", test_multiple_sources_sinks),
        ("missing source raises ValueError", test_missing_source_raises),
        ("missing sink raises ValueError", test_missing_sink_raises),
        ("arc missing resource raises ValueError", test_arc_missing_resource_raises),
        ("some arcs missing resource raises ValueError", test_some_arcs_missing_resource_raises),
        ("wrong resource count raises ValueError", test_wrong_resource_count_raises),
        ("no resources registered raises ValueError", test_no_resources_raises),
    ]

    for name, fn in tests:
        print(f"Test: {name} ... ", end="", flush=True)
        fn()
        print("PASSED")

    print(f"\nAll {len(tests)} NetworkX tests passed.")
