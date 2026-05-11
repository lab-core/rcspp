#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# flake8: noqa

import math
import os
import signal
import sys
import threading
import time

relative_path = "../../cmake-build-release/src/python_interface/"
sys.path.insert(0, os.path.abspath(relative_path))

from rcspp import LogLevel, set_log_level
from rcspp.graph import Algorithm, AlgorithmParams, ResourceGraph
from rcspp.resource import (  # Generic (type-unspecialized) wrappers — resolved to the right C++ template; automatically by add_real_resource / add_int_resource / add_real_set_resource / etc.; Real-resource–only functions
    AdditionExtensionFunction,
    ContainDominanceFunction,
    InclusionDominanceFunction,
    IntersectionExtensionFunction,
    MinMaxFeasibilityFunction,
    RealAdditionExtensionFunction,
    RealTrivialFeasibilityFunction,
    RealValueCostFunction,
    RealValueDominanceFunction,
    SizeFeasibilityFunction,
    SubtractExtensionFunction,
    TimeWindowExtensionFunction,
    TimeWindowFeasibilityFunction,
    TrivialCostFunction,
    TrivialFeasibilityFunction,
    UnionExtensionFunction,
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

    rg.add_arc(10.0, 0, 1, cost=10.0)
    rg.add_arc((20.0,), 0, 2, cost=20.0)
    rg.add_arc((15.0,), 1, 3, cost=15.0)
    rg.add_arc((5.0,), 2, 3, cost=5.0)
    rg.add_arc((30.0,), 1, 2, cost=30.0)

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

    rg.add_arc((1,), 0, 1)
    rg.add_arc((1,), 0, 2)
    rg.add_arc((1,), 1, 3)
    rg.add_arc((1,), 2, 3)
    rg.add_arc((1,), 1, 2)  # 0→1→2→3 would be 3 hops → infeasible

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

    rg.add_arc((10.0, 1), 0, 1, cost=10.0)
    rg.add_arc((20.0, 1), 0, 2, cost=20.0)
    rg.add_arc((15.0, 1), 1, 3, cost=15.0)
    rg.add_arc((5.0, 1), 2, 3, cost=5.0)
    # 3-hop path 0→1→2→3 violates the hop constraint (3 > 2) and must be pruned
    rg.add_arc((1.0, 1), 1, 2, cost=1.0)

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

    rg.add_arc((5.0, 8.0), 0, 1, cost=5.0)  # arrives at node 1 at time 8
    rg.add_arc((10.0, 12.0), 0, 2, cost=10.0)  # arrives at sink at time 12
    rg.add_arc((3.0, 15.0), 1, 2, cost=3.0)  # from 1 to sink

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

    rg.add_arc((5.0,), 0, 1, cost=5.0)
    rg.add_arc((3.0,), 1, 2, cost=3.0)
    rg.add_arc((10.0,), 0, 2, cost=10.0)

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


# ── Example 6: UIntResource (unsigned integer distance, min ≤ 3 hops) ────────


def example_uint_resource():
    """4-node graph with one unsigned-int resource (hop count ≤ 3)."""
    rg = ResourceGraph()
    rg.add_uint_resource(
        AdditionExtensionFunction(),
        MinMaxFeasibilityFunction(0, 3),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_node(0, source=True)
    rg.add_node(1)
    rg.add_node(2)
    rg.add_node(3, sink=True)

    rg.add_arc((1,), 0, 1)
    rg.add_arc((1,), 0, 2)
    rg.add_arc((1,), 1, 3)
    rg.add_arc((1,), 2, 3)
    rg.add_arc((1,), 1, 2)  # 0→1→2→3 costs 3 hops — still feasible

    sols = rg.solve()
    print_solutions("uint-resource", sols)
    assert len(sols) >= 1, "Expected at least one solution"
    assert sols[0].cost <= 3.0, f"Expected cost ≤ 3, got {sols[0].cost}"


# ── Example 7: SetResource (forbidden-node tracking via set union) ────────────
# Resource tracks the set of visited nodes.  A path that revisits node 1 (via
# 0→1→2→1→3) would be a cycle and should be pruned by the hop limit or the
# graph itself.  Here we use a simpler two-hop path to demonstrate set semantics.


def example_set_resource():
    """3-node graph: real cost + int_set resource tracking visited nodes (InclusionDominance)."""
    rg = ResourceGraph()
    # real resource 0: accumulated arc cost (optimisation objective)
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    # int_set resource 1: accumulate visited node IDs; smaller set dominates larger set
    rg.add_int_set_resource(
        UnionExtensionFunction(),
        TrivialFeasibilityFunction(),
        TrivialCostFunction(),
        InclusionDominanceFunction(),
    )
    rg.add_node(0, source=True)
    rg.add_node(1)
    rg.add_node(2, sink=True)

    rg.add_arc((5.0, {1, 3}), 0, 1, cost=5.0)
    rg.add_arc((3.0, {2}), 1, 2, cost=3.0)
    rg.add_arc((10.0, {2, 3}), 0, 2, cost=10.0)

    sols = rg.solve()
    print_solutions("int-set resource", sols)
    assert len(sols) >= 1, "Expected at least one solution"


# ── Example 8: BitsetResource (NG-path style forbidden-node set) ─────────────
# uint_bitset encodes a set of forbidden node IDs in a compact bitset.
# We use SizeFeasibilityFunction to ensure the accumulated set has at most 2 elements.


def example_bitset_resource():
    """4-node graph: real cost + uint_bitset resource tracking forbidden nodes."""
    rg = ResourceGraph()
    # real resource 0: accumulated arc cost (optimisation objective)
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    # uint_bitset resource 1: forbidden-node set, at most 2 distinct nodes
    rg.add_uint_bitset_resource(
        UnionExtensionFunction(),
        SizeFeasibilityFunction(0, 2),  # allow at most 2 distinct nodes in the set
        TrivialCostFunction(),
        ContainDominanceFunction(),  # lhs dominates rhs if lhs ⊇ rhs
    )
    rg.add_node(0, source=True)
    rg.add_node(1)
    rg.add_node(2)
    rg.add_node(3, sink=True)

    # Each arc adds its destination node to the bitset
    rg.add_arc((5.0, {1}), 0, 1, cost=5.0)
    rg.add_arc((3.0, {2}), 0, 2, cost=3.0)
    rg.add_arc((4.0, {3}), 1, 3, cost=4.0)
    rg.add_arc((6.0, {3}), 2, 3, cost=6.0)
    rg.add_arc((1.0, {2}), 1, 2, cost=1.0)  # 0→1→2→3 accumulates 3 nodes → pruned

    sols = rg.solve()
    print_solutions("uint-bitset resource", sols)
    assert len(sols) >= 1, "Expected at least one solution"
    # Each solution must visit at most 2 non-source nodes
    for s in sols:
        assert len(s.path_node_ids) - 1 <= 2, f"Size constraint violated: {s.path_node_ids}"


# ── Example 9: SIGINT handler ─────────────────────────────────────────────────
# Uses ContainDominanceFunction on a uint_bitset resource to prevent all label
# pruning: every arc carries a unique bit, so every partial path has a distinct
# bitset and no label ever dominates another, forcing 2^(N-2) labels to be kept.
# A background thread fires SIGINT partway through and the test asserts that
# solve() raises KeyboardInterrupt promptly.
#
# N=13 → 78 arcs, ~200ms solve on a typical machine.  The signal fires at 50ms,
# well inside the solve, making the test reliable without being slow.


def example_sigint_handler():
    """Test that SIGINT during a long solve() raises KeyboardInterrupt."""
    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    # ContainDominanceFunction: A dominates B only if A.set ⊇ B.set.
    # Each arc carries a unique bit → all partial paths have incomparable
    # bitsets → no pruning → exponential label count → long solve.
    rg.add_uint_bitset_resource(
        UnionExtensionFunction(),
        TrivialFeasibilityFunction(),
        TrivialCostFunction(),
        ContainDominanceFunction(),
    )

    N = 200
    for i in range(N):
        rg.add_node(i, source=(i == 0), sink=(i == N - 1))

    arc_id = 0
    for i in range(N):
        for j in range(i + 1, N):
            rg.add_arc((float(j - i), {arc_id}), i, j, cost=float(j - i))
            arc_id += 1

    # Run solve() in a background thread and interrupt it with SIGINT sent
    # directly to that thread via pthread_kill.  Targeting the solve thread
    # (rather than os.kill(getpid(), …) which lets the OS pick a recipient)
    # makes the test deterministic: the signal is guaranteed to land in the
    # C++ loop where our flag-setter handler is active.
    raised = False
    solve_tid = [None]
    thread_started = threading.Event()

    def run_solve():
        nonlocal raised
        solve_tid[0] = threading.get_ident()
        thread_started.set()
        try:
            rg.solve()
        except KeyboardInterrupt:
            raised = True

    t = threading.Thread(target=run_solve, daemon=True)
    t.start()
    thread_started.wait()  # ensure tid is captured before we use it
    signal.pthread_kill(solve_tid[0], signal.SIGINT)
    t.join(timeout=3.0)

    assert raised, "Expected KeyboardInterrupt from SIGINT during solve()"


# ── Run all examples ──────────────────────────────────────────────────────────

if __name__ == "__main__":
    set_log_level(LogLevel.Debug)
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

    print("\n" + "=" * 60)
    print("Example 6: UIntResource")
    print("=" * 60)
    example_uint_resource()

    print("\n" + "=" * 60)
    print("Example 7: SetResource (int_set)")
    print("=" * 60)
    example_set_resource()

    print("\n" + "=" * 60)
    print("Example 8: BitsetResource (uint_bitset)")
    print("=" * 60)
    example_bitset_resource()

    print("\n" + "=" * 60)
    print("Example 9: SIGINT handler")
    print("=" * 60)
    example_sigint_handler()
    print("KeyboardInterrupt raised as expected.")

    print("\nAll examples passed.")
