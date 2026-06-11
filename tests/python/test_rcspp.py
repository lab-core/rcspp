#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# flake8: noqa

import math
import os
import signal
import sys
import threading
import time

sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "python", "src")
)

from rcspp import LogLevel, set_log_level
from rcspp.graph import Algorithm, AlgorithmParams, BucketAlgorithmParams, ResourceGraph
from rcspp.resource import (  # Generic (type-unspecialized) wrappers — resolved to the right C++ template; automatically by add_real_resource / add_int_resource / add_real_set_resource / etc.; Real-resource–only functions
    AdditionExtensionFunction,
    AdditionExtensionFunction_real,
    ContainDominanceFunction,
    InclusionDominanceFunction,
    IntersectionExtensionFunction,
    MinMaxFeasibilityFunction,
    MinMaxFeasibilityFunction_real,
    SizeFeasibilityFunction,
    SubtractExtensionFunction,
    TimeWindowExtensionFunction,
    TimeWindowFeasibilityFunction,
    TrivialCostFunction,
    TrivialFeasibilityFunction,
    TrivialFeasibilityFunction_real,
    UnionExtensionFunction,
    ValueCostFunction,
    ValueCostFunction_real,
    ValueDominanceFunction,
    ValueDominanceFunction_real,
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

    print(rg)

    sols = rg.solve()
    print_solutions("real-resource", sols)
    assert len(sols) >= 1, "Expected at least one solution"
    assert math.isclose(sols[0].cost, 25.0, abs_tol=1e-6), f"Expected cost 25, got {sols[0].cost}"


# ── Example 2: Real (cost) + IntResource (hop constraint) ────────────────────
# Real resource 0 counts hops (1.0 per arc) and is the optimisation objective.
# Int resource 1 also counts hops and enforces the feasibility budget of ≤ 2.
# A 3-hop path exceeds the int budget and is pruned.


def example_int_resource():
    """4-node graph: minimize hops (real cost), hop count constrained by int resource (≤
    2)."""
    rg = ResourceGraph()
    # Real resource 0: cost (1.0 per arc = hop count), must always be registered first.
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    # Int resource 1: hop count ≤ 2 (feasibility constraint only)
    rg.add_int_resource(
        AdditionExtensionFunction(),
        MinMaxFeasibilityFunction(0, 2),
        TrivialCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_node(0, source=True)
    rg.add_node(1)
    rg.add_node(2)
    rg.add_node(3, sink=True)

    rg.add_arc((1.0, 1), 0, 1, cost=1.0)
    rg.add_arc((1.0, 1), 0, 2, cost=1.0)
    rg.add_arc((1.0, 1), 1, 3, cost=1.0)
    rg.add_arc((1.0, 1), 2, 3, cost=1.0)
    rg.add_arc((1.0, 1), 1, 2, cost=1.0)  # 0→1→2→3 would be 3 hops → infeasible

    sols = rg.solve()
    print_solutions("int-resource", sols)
    assert len(sols) >= 1, "Expected at least one solution"
    # Both direct paths (0→1→3, 0→2→3) cost 2.0; the 3-hop path is pruned.
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
    tw = {1: (5.0, 20.0), 2: (0.0, 30.0)}  # (earliest, latest) arrival at each node

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
        TimeWindowExtensionFunction(tw),
        TimeWindowFeasibilityFunction(tw),
        ValueCostFunction(),
        ValueDominanceFunction(),
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

    # Enum values: Algorithm.Simple, Algorithm.Pushing, Algorithm.Pulling, Algorithm.Greedy, Algorithm.AStar
    sols_simple = rg.solve(Algorithm.Simple)
    sols_pushing = rg.solve(Algorithm.Pushing)
    sols_pulling = rg.solve(Algorithm.Pulling)
    sols_greedy = rg.solve(Algorithm.Greedy)
    sols_astar = rg.solve(Algorithm.AStar)
    # String aliases are also accepted for convenience
    sols_str = rg.solve("simple")
    sols_astar_str = rg.solve("astar")

    print_solutions("Algorithm.Simple", sols_simple)
    print_solutions("Algorithm.Pushing", sols_pushing)
    print_solutions("Algorithm.Pulling", sols_pulling)
    print_solutions("Algorithm.Greedy", sols_greedy)
    print_solutions("Algorithm.AStar", sols_astar)

    assert sols_simple[0].cost == sols_str[0].cost, "string alias must match enum"
    assert sols_astar[0].cost == sols_astar_str[0].cost, "astar string alias must match enum"
    assert all(
        s.cost == 8.0 for s in [sols_simple[0], sols_pulling[0], sols_greedy[0], sols_astar[0]]
    )

    # AlgorithmParams: stop after the first solution
    params = AlgorithmParams()
    params.stop_after_X_solutions = 1
    sols_one = rg.solve(Algorithm.Simple, params=params)
    print_solutions("stop_after_X_solutions=1", sols_one)
    assert len(sols_one) == 1, f"Expected exactly 1 solution, got {len(sols_one)}"


# ── Example 6: sort_nodes ─────────────────────────────────────────────────────
# sort_nodes() reorders the internal node processing sequence used by the
# labelling algorithms.  Calling it before solve() overrides the default
# shortest-path connectivity sort done by preprocessing.
# A custom comparator ``(n1, n2) -> bool`` returns True when n1 should come
# before n2 (same contract as C++ std::sort comparators).


def example_sort_nodes():
    """Demonstrate sort_nodes with default and custom comparator."""
    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    # Add nodes in non-sequential order to make the sort non-trivial.
    rg.add_node(3, sink=True)
    rg.add_node(1)
    rg.add_node(0, source=True)
    rg.add_node(2)

    rg.add_arc((1.0,), 0, 1, cost=1.0)
    rg.add_arc((2.0,), 1, 2, cost=2.0)
    rg.add_arc((3.0,), 2, 3, cost=3.0)
    rg.add_arc((8.0,), 0, 3, cost=8.0)  # longer direct path

    # ── Default sort: ascending node.id ──────────────────────────────────────
    rg.sort_nodes()
    for node_id in [0, 1, 2, 3]:
        pos = rg.get_node(node_id).pos()
        assert pos == node_id, f"Default sort: expected node {node_id} at pos {node_id}, got {pos}"

    # ── Custom comparator: descending id ─────────────────────────────────────
    rg.sort_nodes(lambda n1, n2: n1.id > n2.id)
    for node_id in [0, 1, 2, 3]:
        expected_pos = 3 - node_id  # id=3 → pos=0, id=0 → pos=3
        pos = rg.get_node(node_id).pos()
        assert (
            pos == expected_pos
        ), f"Custom sort: expected node {node_id} at pos {expected_pos}, got {pos}"
    print(f"  Node positions after descending sort: {[rg.get_node(i).pos() for i in range(4)]}")

    # Restore a sensible order and verify that solve() still finds the optimal path.
    rg.sort_nodes()
    sols = rg.solve(preprocess=False)  # skip preprocessing to keep the manual sort
    print_solutions("sort-nodes", sols)
    assert len(sols) >= 1, "Expected at least one solution after sort_nodes"
    assert math.isclose(
        sols[0].cost, 6.0, abs_tol=1e-6
    ), f"Expected optimal cost 6.0 (0→1→2→3), got {sols[0].cost}"


# ── Example 7: SetResource (forbidden-node tracking via set union) ────────────
# Resource tracks the set of visited nodes.  A path that revisits node 1 (via
# 0→1→2→1→3) would be a cycle and should be pruned by the hop limit or the
# graph itself.  Here we use a simpler two-hop path to demonstrate set semantics.


def example_set_resource():
    """3-node graph: real cost + int_set resource tracking visited nodes
    (InclusionDominance)."""
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
    rg.add_bitset_resource(
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


# ── Example 10: Pulling algorithm with truncated labeling ────────────────────
# num_labels_to_extend_by_node limits how many labels are processed per node
# per phase.  num_max_phases allows the algorithm to iterate and recover labels
# that were truncated in earlier phases, converging towards the optimal solution.


def example_pulling_truncated():
    """Pulling algorithm with num_labels_to_extend_by_node and num_max_phases."""
    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_node(0, source=True)
    rg.add_node(1)
    rg.add_node(2)
    rg.add_node(3, sink=True)

    # Two paths: short 0→1→3 (cost 2, optimal) and long 0→2→3 (cost 20)
    rg.add_arc((1.0,), 0, 1, cost=1.0)
    rg.add_arc((10.0,), 0, 2, cost=10.0)
    rg.add_arc((1.0,), 1, 3, cost=1.0)
    rg.add_arc((10.0,), 2, 3, cost=10.0)

    # Full solve finds the optimal path
    sols_full = rg.solve(Algorithm.Pulling)
    print_solutions("Pulling (full)", sols_full)
    assert math.isclose(sols_full[0].cost, 2.0, abs_tol=1e-6)

    # Truncated to 1 label/node with 1 phase — may explore fewer paths but still
    # finds a feasible solution.
    params = AlgorithmParams()
    params.num_labels_to_extend_by_node = 1
    params.num_max_phases = 1
    params.stop_after_X_solutions = 1
    sols_trunc = rg.solve(Algorithm.Pulling, params=params)
    print_solutions("Pulling (1 label/node, 1 phase)", sols_trunc)
    assert len(sols_trunc) >= 1, "Expected at least one solution with truncated Pulling"

    # Multiple phases restore truncated labels and let the algorithm converge;
    # with enough phases the optimal cost is recovered.
    params2 = AlgorithmParams()
    params2.num_labels_to_extend_by_node = 1
    params2.num_max_phases = 5
    sols_phases = rg.solve(Algorithm.Pulling, params=params2)
    print_solutions("Pulling (1 label/node, 5 phases)", sols_phases)
    assert len(sols_phases) >= 1
    assert math.isclose(
        sols_phases[0].cost, 2.0, abs_tol=1e-6
    ), f"Expected optimal cost 2.0 after 5 phases, got {sols_phases[0].cost}"


# ── Example 11: max_iterations and return_dominated_solutions ─────────────────
# max_iterations terminates the labelling loop early; solutions are then
# extracted from whatever labels have already reached sink nodes.
# return_dominated_solutions=True makes the loop extract solutions as labels
# hit sinks (instead of waiting until after the loop), enabling early stopping
# via stop_after_X_solutions without having to run main_loop to completion.


def example_advanced_params():
    """Demonstrates max_iterations and return_dominated_solutions."""
    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    # Graph: direct path 0→2 (cost 5) and optimal path 0→1→2 (cost 3).
    rg.add_node(0, source=True)
    rg.add_node(1)
    rg.add_node(2, sink=True)
    rg.add_arc((5.0,), 0, 2, cost=5.0)  # direct, suboptimal
    rg.add_arc((1.0,), 0, 1, cost=1.0)
    rg.add_arc((2.0,), 1, 2, cost=2.0)  # optimal path: cost 3

    # Full solve: optimal cost 3
    sols_full = rg.solve(Algorithm.Simple)
    print_solutions("Simple (full)", sols_full)
    assert math.isclose(sols_full[0].cost, 3.0, abs_tol=1e-6)

    # max_iterations=1: only the source label is processed in the main loop.
    # The direct arc 0→2 adds cost-5 label to the sink, which extract_remaining_solutions
    # picks up.  The indirect path via node 1 is not yet explored → suboptimal result.
    params = AlgorithmParams()
    params.max_iterations = 1
    sols_early = rg.solve(Algorithm.Simple, params=params)
    print_solutions("Simple (max_iterations=1)", sols_early)
    assert len(sols_early) == 1
    assert math.isclose(
        sols_early[0].cost, 5.0, abs_tol=1e-6
    ), f"Expected cost 5.0 with max_iterations=1, got {sols_early[0].cost}"

    # return_dominated_solutions=True + stop_after_X_solutions=1:
    # the main loop yields each label as it reaches the sink and stops as soon
    # as one solution is collected — regardless of optimality.  The first label
    # to arrive at the sink is the direct arc 0→2 (cost 5), so that is returned.
    params2 = AlgorithmParams()
    params2.return_dominated_solutions = True
    params2.stop_after_X_solutions = 1
    sols_first = rg.solve(Algorithm.Simple, params=params2)
    print_solutions("Simple (return_dominated=True, stop_after=1)", sols_first)
    assert len(sols_first) == 1
    assert math.isclose(
        sols_first[0].cost, 5.0, abs_tol=1e-6
    ), f"Expected cost 5.0 (first-found) with return_dominated_solutions, got {sols_first[0].cost}"


# ── Example 12: SIGINT handler ────────────────────────────────────────────────
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
    if not hasattr(signal, "pthread_kill"):
        # signal.pthread_kill is a POSIX-only API; skip on Windows.
        return

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
    rg.add_bitset_resource(
        UnionExtensionFunction(),
        TrivialFeasibilityFunction(),
        TrivialCostFunction(),
        ContainDominanceFunction(),
    )

    N = 13
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
    time.sleep(0.05)  # give solve() time to enter the C++ loop before firing
    signal.pthread_kill(solve_tid[0], signal.SIGINT)
    t.join(timeout=3.0)

    assert raised, "Expected KeyboardInterrupt from SIGINT during solve()"


# ── Example 13: resource refs survive GC ─────────────────────────────────────
# When function objects are created in a helper and only passed to the graph
# (no user-held reference afterwards), the Python GC must not collect them
# while the ResourceGraph is alive.  ResourceGraph._refs is the safety net.
#
# The test uses the typed C++ classes (e.g. AdditionExtensionFunction_real)
# rather than the generic Python descriptors so that we can take weakrefs to
# the exact objects that end up stored inside the C++ graph.


def test_memory_helpers():
    """process_memory_bytes and available_memory_bytes return positive integers."""
    import rcspp

    process_bytes = rcspp.process_memory_bytes()
    available_bytes = rcspp.available_memory_bytes()
    assert isinstance(process_bytes, int), "process_memory_bytes must return int"
    assert isinstance(available_bytes, int), "available_memory_bytes must return int"
    assert process_bytes >= 0, "process RSS must be non-negative"
    assert available_bytes >= 0, "available memory must be non-negative"


def test_resource_refs_survive_gc():
    """Two-part GC safety test for resource function objects.

    Part A — C++ wrapper objects (AdditionExtensionFunction_real etc.) must stay alive
    while the ResourceGraph is alive, because the C++ internals may call clone() on them
    at any time.  ResourceGraph._refs is the safety net.

    Part B — Time-window maps passed as Python dicts must be COPIED into the C++
    OwnedTimeWindowExtFn / OwnedTimeWindowFeasFn objects.  After the helper returns and
    GC runs, the Python dicts should be collectable (C++ no longer holds a reference to
    them), and the graph must still solve correctly.
    """
    import gc
    import weakref

    # ── Part A: wrapper objects stay alive via _refs ──────────────────────────

    weak_refs: dict[str, weakref.ref] = {}

    def build_graph_a():
        """Wrapper objects created here; only the graph is returned."""
        rg = ResourceGraph()
        ext = AdditionExtensionFunction_real()
        feas = MinMaxFeasibilityFunction_real(0.0, 50.0)
        cost = ValueCostFunction_real()
        dom = ValueDominanceFunction_real()
        weak_refs["ext"] = weakref.ref(ext)
        weak_refs["feas"] = weakref.ref(feas)
        weak_refs["cost"] = weakref.ref(cost)
        weak_refs["dom"] = weakref.ref(dom)
        rg.add_real_resource(ext, feas, cost, dom)
        rg.add_node(0, source=True)
        rg.add_node(1, sink=True)
        rg.add_arc(5.0, 0, 1, cost=5.0)
        return rg  # ext / feas / cost / dom leave scope here

    rg_a = build_graph_a()
    gc.collect()
    gc.collect()

    for name, ref in weak_refs.items():
        assert ref() is not None, (
            f"'{name}' was garbage collected while the ResourceGraph is still alive. "
            "ResourceGraph._refs must keep resource function wrappers live."
        )

    sols = rg_a.solve()
    assert len(sols) == 1 and math.isclose(sols[0].cost, 5.0, abs_tol=1e-6)

    del rg_a
    gc.collect()
    gc.collect()
    for name, ref in weak_refs.items():
        assert ref() is None, f"'{name}' should be collectable after the ResourceGraph is deleted."

    # ── Part B: time-window dicts are copied by C++, not held by reference ───
    # Plain dicts don't support weakref, so use a subclass that does.

    class TrackedDict(dict):
        pass

    tw_refs: dict[str, weakref.ref] = {}

    def build_graph_b():
        """Time-window maps created here; only the graph is returned."""
        tw = TrackedDict({1: (5.0, 20.0), 2: (0.0, 30.0)})
        tw_refs["tw"] = weakref.ref(tw)

        rg = ResourceGraph()
        rg.add_real_resource(
            AdditionExtensionFunction(),
            TrivialFeasibilityFunction(),
            ValueCostFunction(),
            ValueDominanceFunction(),
        )
        rg.add_real_resource(
            TimeWindowExtensionFunction(tw),
            TimeWindowFeasibilityFunction(tw),
            ValueCostFunction(),
            ValueDominanceFunction(),
        )
        rg.add_node(0, source=True)
        rg.add_node(1)
        rg.add_node(2, sink=True)
        rg.add_arc((5.0, 8.0), 0, 1, cost=5.0)
        rg.add_arc((10.0, 12.0), 0, 2, cost=10.0)
        rg.add_arc((3.0, 15.0), 1, 2, cost=3.0)
        return rg  # min_tw, max_tw leave scope here

    rg_b = build_graph_b()

    # Force collection — C++ owns copies of the maps, so the Python dicts should
    # now be unreachable and collected.
    gc.collect()
    gc.collect()

    for name, ref in tw_refs.items():
        assert ref() is None, (
            f"Python dict '{name}' is still alive after build_graph returned. "
            "OwnedTimeWindowExtFn / OwnedTimeWindowFeasFn must copy the map, "
            "not hold a reference to the Python dict."
        )

    # Functional check: graph must still solve correctly with its internal map copies.
    # Optimal path is 0→1→2 (cost 5+3=8); direct 0→2 costs 10.
    sols = rg_b.solve()
    assert len(sols) >= 1
    assert math.isclose(sols[0].cost, 8.0, abs_tol=1e-6)


# ── Example 14: BucketAlgorithmParams ────────────────────────────────────────
# BucketAlgorithmParams swaps the flat LabelList for LabelBuckets, which groups
# labels into resource-value ranges and sorts within each bucket to speed up
# dominance checks.  Results must be identical to solving with AlgorithmParams.
# bucket_resource_type selects which numerical resource drives the bucket
# boundaries; empty string (default) uses the graph's cost resource (CostRC).


def example_bucket_labels():
    """BucketAlgorithmParams produces the same optimal solutions as AlgorithmParams."""
    # ── Single real resource ──────────────────────────────────────────────────
    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction(),
        MinMaxFeasibilityFunction(0.0, 20.0),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_node(0, source=True)
    rg.add_node(1)
    rg.add_node(2)
    rg.add_node(3, sink=True)
    rg.add_arc((5.0,), 0, 1, cost=5.0)
    rg.add_arc((3.0,), 1, 2, cost=3.0)
    rg.add_arc((2.0,), 2, 3, cost=2.0)
    rg.add_arc((10.0,), 0, 3, cost=10.0)  # suboptimal direct path

    reference = rg.solve(params=AlgorithmParams())
    assert reference, "reference solve returned no solutions"
    ref_cost = reference[0].cost

    # Default BucketAlgorithmParams (bucket/sort resource = CostRC = real)
    bp_default = BucketAlgorithmParams()
    bp_default.range_buckets = 5
    sols_default = rg.solve(params=bp_default)
    print_solutions("BucketAlgorithmParams default", sols_default)
    assert sols_default, "bucket solve (default) returned no solutions"
    assert math.isclose(
        sols_default[0].cost, ref_cost, abs_tol=1e-6
    ), f"bucket default cost {sols_default[0].cost} != reference {ref_cost}"

    # Explicit bucket_resource_type='real'
    bp_real = BucketAlgorithmParams()
    bp_real.range_buckets = 5
    bp_real.bucket_resource_type = "real"
    sols_real = rg.solve(params=bp_real)
    print_solutions("BucketAlgorithmParams bucket_resource_type='real'", sols_real)
    assert math.isclose(
        sols_real[0].cost, ref_cost, abs_tol=1e-6
    ), f"bucket real cost {sols_real[0].cost} != reference {ref_cost}"

    # ── Real + Int resources: bucket by 'int' ─────────────────────────────────
    rg2 = ResourceGraph()
    rg2.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    rg2.add_int_resource(
        AdditionExtensionFunction(),
        MinMaxFeasibilityFunction(0, 10),
        TrivialCostFunction(),
        ValueDominanceFunction(),
    )
    rg2.add_node(0, source=True)
    rg2.add_node(1)
    rg2.add_node(2, sink=True)
    rg2.add_arc((4.0, 3), 0, 1, cost=4.0)
    rg2.add_arc((3.0, 2), 1, 2, cost=3.0)
    rg2.add_arc((9.0, 6), 0, 2, cost=9.0)

    ref2 = rg2.solve(params=AlgorithmParams())
    assert ref2
    ref2_cost = ref2[0].cost

    bp_int = BucketAlgorithmParams()
    bp_int.range_buckets = 3
    bp_int.bucket_resource_type = "int"
    sols_int = rg2.solve(params=bp_int)
    print_solutions("BucketAlgorithmParams bucket_resource_type='int'", sols_int)
    assert sols_int, "bucket solve (int) returned no solutions"
    assert math.isclose(
        sols_int[0].cost, ref2_cost, abs_tol=1e-6
    ), f"bucket int cost {sols_int[0].cost} != reference {ref2_cost}"

    # ── Invalid bucket_resource_type raises ValueError ────────────────────────
    bp_bad = BucketAlgorithmParams()
    bp_bad.bucket_resource_type = "not_a_type"
    try:
        rg.solve(params=bp_bad)
        assert False, "Expected ValueError for unknown bucket_resource_type"
    except ValueError:
        pass  # expected

    # ── BucketAlgorithmParams inherits all AlgorithmParams fields ─────────────
    bp_inh = BucketAlgorithmParams()
    bp_inh.stop_after_X_solutions = 1
    sols_one = rg.solve(params=bp_inh)
    assert len(sols_one) == 1, f"Expected 1 solution, got {len(sols_one)}"

    # ── Position-based API: bucket by int (pos 1), sort by real (pos 0) ───────
    # rg2 has real at registration pos 0 and int at pos 1.
    # bucket_resource_pos=1 → int resource; sort_resource_pos=0 → real (cost).
    bp_pos = BucketAlgorithmParams(
        range_buckets=3,
        bucket_resource_pos=1,
        sort_resource_pos=0,
    )
    sols_pos = rg2.solve(params=bp_pos)
    print_solutions("BucketAlgorithmParams pos-based (bucket=int, sort=real)", sols_pos)
    assert sols_pos, "bucket solve (pos-based) returned no solutions"
    assert math.isclose(
        sols_pos[0].cost, ref2_cost, abs_tol=1e-6
    ), f"bucket pos-based cost {sols_pos[0].cost} != reference {ref2_cost}"

    # Bucket by real (pos 0), sort by real (pos 0) — same type, first instance.
    bp_pos_real = BucketAlgorithmParams(
        range_buckets=5,
        bucket_resource_pos=0,
        sort_resource_pos=0,
    )
    sols_pos_real = rg2.solve(params=bp_pos_real)
    assert sols_pos_real, "bucket solve (pos 0,0) returned no solutions"
    assert math.isclose(
        sols_pos_real[0].cost, ref2_cost, abs_tol=1e-6
    ), f"bucket pos(0,0) cost {sols_pos_real[0].cost} != reference {ref2_cost}"

    # Out-of-range position raises ValueError.
    bp_oob = BucketAlgorithmParams(bucket_resource_pos=99)
    try:
        rg2.solve(params=bp_oob)
        assert False, "Expected ValueError for out-of-range bucket_resource_pos"
    except ValueError:
        pass  # expected


# ── Run all examples ──────────────────────────────────────────────────────────

if __name__ == "__main__":
    set_log_level(LogLevel.Debug)
    print("=" * 60)
    print("Example 1: single RealResource")
    print("=" * 60)
    example_real_resource()

    print("\n" + "=" * 60)
    print("Example 2: Real + IntResource (hop constraint)")
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
    print("Example 6: sort_nodes")
    print("=" * 60)
    example_sort_nodes()

    print("\n" + "=" * 60)
    print("Example 7: SetResource (int_set)")
    print("=" * 60)
    example_set_resource()

    print("\n" + "=" * 60)
    print("Example 8: BitsetResource (uint_bitset)")
    print("=" * 60)
    example_bitset_resource()

    print("\n" + "=" * 60)
    print("Example 10: Pulling with truncated labeling")
    print("=" * 60)
    example_pulling_truncated()

    print("\n" + "=" * 60)
    print("Example 11: max_iterations and return_dominated_solutions")
    print("=" * 60)
    example_advanced_params()

    print("\n" + "=" * 60)
    print("Example 12: ref and garbage collector")
    print("=" * 60)
    test_resource_refs_survive_gc()

    print("\n" + "=" * 60)
    print("Example 13: SIGINT handler")
    print("=" * 60)
    example_sigint_handler()
    print("KeyboardInterrupt raised as expected.")

    print("\n" + "=" * 60)
    print("Example 14: BucketAlgorithmParams")
    print("=" * 60)
    example_bucket_labels()

    print("\nAll examples passed.")
