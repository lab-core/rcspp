#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# flake8: noqa

import math
import os
import sys

relative_path = "../../cmake-build-release/src/python_interface/"
sys.path.insert(0, os.path.abspath(relative_path))

from rcspp.graph import Algorithm, AlgorithmParams, ResourceGraph
from rcspp.resource import (  # Generic (type-unspecialized) wrappers – resolved to the right C++ template; automatically by add_real_resource / add_int_resource.; Real-resource–only functions
    AdditionExtensionFunction,
    MinMaxFeasibilityFunction,
    RealAdditionExtensionFunction,
    RealTrivialFeasibilityFunction,
    RealValueCostFunction,
    RealValueDominanceFunction,
    TimeWindowExtensionFunction,
    TimeWindowFeasibilityFunction,
    TrivialFeasibilityFunction,
    ValueCostFunction,
    ValueDominanceFunction,
)

# ── Helpers ───────────────────────────────────────────────────────────────────


def print_solutions(tag, solutions):
    print(f"\n[{tag}] {len(solutions)} solution(s):")
    for s in solutions:
        print(f"  cost={s.cost:.4f}  nodes={s.path_node_ids}")


# ── Example 1: single RealResource (addition + min/max feasibility) ───────────
# Uses generic function names – no 'Real' prefix needed.


def example_real_resource():
    """Simple 4-node graph with one real resource (distance)."""
    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction(),
        MinMaxFeasibilityFunction(0.0, 50.0),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_node(0, source=True)
    rg.add_node(1)
    rg.add_node(2)
    rg.add_node(3, sink=True)

    # resource_consumption: tuple of lists-per-resource-type
    rg.add_arc(([(10.0,)],), 0, 1, cost=10.0)
    rg.add_arc(([(20.0,)],), 0, 2, cost=20.0)
    rg.add_arc(([(15.0,)],), 1, 3, cost=15.0)
    rg.add_arc(([(5.0,)],), 2, 3, cost=5.0)
    rg.add_arc(([(30.0,)],), 1, 2, cost=30.0)

    sols = rg.solve()
    print_solutions("real-resource", sols)
    assert len(sols) >= 1, "Expected at least one solution"
    assert math.isclose(sols[0].cost, 25.0, abs_tol=1e-6), f"Expected cost 25, got {sols[0].cost}"


# ── Example 2: single IntResource (hop count minimization) ───────────────────
# The integer resource IS the objective: minimize total hops.
# A 3-hop path that exceeds the budget of 2 is infeasible.


def example_int_resource():
    """4-node graph: minimize hops (int resource), budget ≤ 2."""
    rg = ResourceGraph()
    rg.add_int_resource(
        AdditionExtensionFunction(),
        MinMaxFeasibilityFunction(0, 2),  # ≤ 2 hops
        ValueCostFunction(),  # cost = accumulated hops
        ValueDominanceFunction(),
    )
    rg.add_node(0, source=True)
    rg.add_node(1)
    rg.add_node(2)
    rg.add_node(3, sink=True)

    rg.add_arc(([(1,)],), 0, 1)
    rg.add_arc(([(1,)],), 0, 2)
    rg.add_arc(([(1,)],), 1, 3)
    rg.add_arc(([(1,)],), 2, 3)
    rg.add_arc(([(1,)],), 1, 2)  # 0→1→2→3 would be 3 hops → infeasible

    sols = rg.solve()
    print_solutions("int-resource", sols)
    assert len(sols) >= 1, "Expected at least one solution"
    # Both direct paths (0→1→3, 0→2→3) cost 2 hops; the 3-hop path is pruned.
    assert math.isclose(sols[0].cost, 2.0, abs_tol=1e-6), f"Expected cost 2, got {sols[0].cost}"


# ── Example 3: mixed Real+Int resources ───────────────────────────────────────
# Resource 0 (real): accumulated distance, used as optimisation objective.
# Resource 1 (int):  hop count ≤ 2.


def example_mixed_resources():
    """4-node graph with one real resource (distance) and one int resource (hops)."""
    rg = ResourceGraph()
    # Real resource 0: distance (optimisation objective)
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    # Int resource 1: hop count, at most 2 hops
    rg.add_int_resource(
        AdditionExtensionFunction(),
        MinMaxFeasibilityFunction(0, 2),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_node(0, source=True)
    rg.add_node(1)
    rg.add_node(2)
    rg.add_node(3, sink=True)

    # add_arc takes (real_consumptions, int_consumptions) per resource type
    rg.add_arc(([(10.0,)], [(1,)]), 0, 1, cost=10.0)
    rg.add_arc(([(20.0,)], [(1,)]), 0, 2, cost=20.0)
    rg.add_arc(([(15.0,)], [(1,)]), 1, 3, cost=15.0)
    rg.add_arc(([(5.0,)], [(1,)]), 2, 3, cost=5.0)
    # 3-hop path 0→1→2→3 violates the hop constraint (3 > 2) and must be pruned
    rg.add_arc(([(1.0,)], [(1,)]), 1, 2, cost=1.0)

    sols = rg.solve()
    print_solutions("mixed real+int resources", sols)
    assert len(sols) >= 1, "Expected at least one solution"
    # Both 2-hop paths (cost 25) are feasible; the 3-hop shortcut must not appear
    for s in sols:
        assert len(s.path_node_ids) - 1 <= 2, f"Hop constraint violated: {s.path_node_ids}"


# ── Example 4: time-window resource ──────────────────────────────────────────
# Shows real-only time-window functions (not genericisable since they are
# real-specific).


def example_time_windows():
    """3-node graph with a time-window resource."""
    min_tw = {1: 5.0, 2: 0.0}  # earliest arrival at each node
    max_tw = {1: 20.0, 2: 30.0}  # latest arrival

    rg = ResourceGraph()
    # Resource 0: cost (generic)
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    # Resource 1: time (real-specific time-window functions)
    rg.add_real_resource(
        TimeWindowExtensionFunction(min_tw),
        TimeWindowFeasibilityFunction(max_tw),
        RealValueCostFunction(),
        RealValueDominanceFunction(),
    )
    rg.add_node(0, source=True)
    rg.add_node(1)
    rg.add_node(2, sink=True)

    rg.add_arc(([(5.0,), (8.0,)],), 0, 1, cost=5.0)  # arrives at node 1 at time 8
    rg.add_arc(([(10.0,), (12.0,)],), 0, 2, cost=10.0)  # arrives at sink at time 12
    rg.add_arc(([(3.0,), (15.0,)],), 1, 2, cost=3.0)  # from 1 to sink

    sols = rg.solve()
    print_solutions("time-window", sols)
    assert len(sols) >= 1, "Expected at least one solution"


# ── Example 5: Algorithm enum and AlgorithmParams ─────────────────────────────
# The algorithm is now a first-class argument of solve().
# Accepted as an Algorithm enum value or a convenience string.


def example_algorithm_params():
    """Demonstrates the Algorithm enum and AlgorithmParams."""
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

    rg.add_arc(([(5.0,)],), 0, 1, cost=5.0)
    rg.add_arc(([(3.0,)],), 1, 2, cost=3.0)
    rg.add_arc(([(10.0,)],), 0, 2, cost=10.0)

    # Enum values: Algorithm.Simple, Algorithm.Pulling, Algorithm.Greedy
    sols_simple = rg.solve(Algorithm.Simple)
    sols_pulling = rg.solve(Algorithm.Pulling)
    sols_greedy = rg.solve(Algorithm.Greedy)
    # String aliases are also accepted for convenience
    sols_str = rg.solve("simple")

    print_solutions("Algorithm.Simple", sols_simple)
    print_solutions("Algorithm.Pulling", sols_pulling)
    print_solutions("Algorithm.Greedy", sols_greedy)

    assert sols_simple[0].cost == sols_str[0].cost, "string alias must match enum"
    assert all(s.cost == 8.0 for s in [sols_simple[0], sols_pulling[0], sols_greedy[0]])

    # AlgorithmParams: stop after the first solution
    params = AlgorithmParams()
    params.stop_after_X_solutions = 1
    sols_one = rg.solve(Algorithm.Simple, params=params)
    print_solutions("stop_after_X_solutions=1", sols_one)
    assert len(sols_one) == 1, f"Expected exactly 1 solution, got {len(sols_one)}"


# ── Run all examples ──────────────────────────────────────────────────────────

if __name__ == "__main__":
    print("=" * 60)
    print("Example 1: single RealResource")
    print("=" * 60)
    example_real_resource()

    print("\n" + "=" * 60)
    print("Example 2: single IntResource")
    print("=" * 60)
    example_int_resource()

    print("\n" + "=" * 60)
    print("Example 3: mixed Real+Int resources")
    print("=" * 60)
    example_mixed_resources()

    print("\n" + "=" * 60)
    print("Example 4: time-window resource")
    print("=" * 60)
    example_time_windows()

    print("\n" + "=" * 60)
    print("Example 5: AlgorithmParams customization")
    print("=" * 60)
    example_algorithm_params()

    print("\nAll examples passed.")
