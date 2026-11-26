#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""ResourceGraph.solver: one algorithm kept for one graph, which reuses its labels."""

import gc
import math
import os
import sys

sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "python", "src")
)

import pytest  # noqa: E402

pytest.importorskip("numpy")

import rcspp  # noqa: E402
from rcspp.graph import (  # noqa: E402
    AlgorithmParams,
    AlgorithmStatus,
    BucketAlgorithmParams,
    ResourceGraph,
    Solver,
)
from rcspp.resource import (  # noqa: E402
    AdditionExtensionFunction,
    MinMaxFeasibilityFunction,
    TrivialCostFunction,
    TrivialFeasibilityFunction,
    ValueCostFunction,
    ValueDominanceFunction,
)

# Dual sets that change which paths are cheapest: one dual per node, row = node id.
DUAL_SETS = [
    [0.0] * 11,
    [0.0, 4.0, 0.0, 1.0, 0.0, 6.0, 2.0, 0.0, 3.0, 5.0, 0.0],
    [0.0, 0.0, 7.0, 0.0, 5.0, 0.0, 1.0, 8.0, 0.0, 2.0, 0.0],
    [0.0] * 11,
]


def layered_graph(time_limit=6.0):
    """A source (0), three layers of three nodes (1-9) fully linked, and a sink (10).

    Two real resources: the cost, and a time limited to *time_limit*. Each arc into
    node v carries row v, so duals change the reduced costs, and the paths.
    """
    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_real_resource(
        AdditionExtensionFunction(),
        MinMaxFeasibilityFunction(0.0, time_limit),
        TrivialCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_node(0, source=True)
    for node in range(1, 10):
        rg.add_node(node)
    rg.add_node(10, sink=True)

    def add_arc(origin, destination):
        cost = float((origin * 7 + destination * 3) % 11) + 1.0
        time = float((origin * 5 + destination) % 3) + 1.0
        rg.add_arc((cost, time), origin, destination, cost=cost, rows=[(destination, 1.0)])

    for node in range(1, 4):
        add_arc(0, node)
    for layer in range(2):
        for origin in range(1, 4):
            for destination in range(1, 4):
                add_arc(3 * layer + origin, 3 * (layer + 1) + destination)
    for node in range(7, 10):
        add_arc(node, 10)
    return rg


def trade_off_graph():
    """Three paths from a source (0) to a sink (4), through node 1, 2 or 3.

    None dominates another: the cheaper, the slower.
    """
    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_real_resource(
        AdditionExtensionFunction(),
        MinMaxFeasibilityFunction(0.0, 10.0),
        TrivialCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_node(0, source=True)
    for node in range(1, 4):
        rg.add_node(node)
    rg.add_node(4, sink=True)
    for node in range(1, 4):
        cost = float(node)
        rg.add_arc((cost, 4.0 - cost), 0, node, cost=cost)
        rg.add_arc((cost, 4.0 - cost), node, 4, cost=cost)
    return rg


def paths(result):
    """The (cost, arc ids) of each solution, in order."""
    return [(s.cost, list(s.path_arc_ids)) for s in result.solutions]


@pytest.mark.parametrize("algorithm", ["simple", "pushing", "pulling", "astar", "greedy", "tabu"])
def test_repeated_solves_equal_graph_solve(algorithm):
    """Under changing duals, every solve of a solver returns what graph.solve does."""
    rg = layered_graph()
    params = AlgorithmParams()
    if algorithm == "tabu":
        params.max_iterations = 50
    solver = rg.solver(algorithm, params)
    for duals in DUAL_SETS:
        rg.update_reduced_costs(duals)
        expected = rg.solve(algorithm, params=params)
        actual = solver.solve()
        assert expected.solutions
        assert paths(actual) == paths(expected)
        assert actual.status == expected.status


def test_a_solve_like_one_before_allocates_no_label():
    """The labels of a solve are kept and reused: a repeat solve allocates none."""
    rg = layered_graph()
    solver = rg.solver("pulling")
    assert solver.number_of_retained_labels == 0
    first = solver.solve()
    created = solver.number_of_created_labels
    assert created > 0
    assert solver.number_of_retained_labels == created
    assert solver.number_of_extended_labels > 0

    again = solver.solve()
    assert paths(again) == paths(first)
    assert solver.number_of_created_labels == created


def test_max_retained_labels_bounds_what_is_kept():
    """A solve that ends with more than max_retained_labels labels releases them all."""
    rg = layered_graph()
    params = AlgorithmParams()
    params.max_retained_labels = 0
    solver = rg.solver("simple", params=params)
    assert solver.solve().solutions
    assert solver.number_of_retained_labels == 0


def test_the_callers_params_are_unchanged():
    """A solver keeps its labels without touching the params it was given."""
    rg = layered_graph()
    params = AlgorithmParams()
    rg.solver("simple", params=params, direction="backward")
    assert params.release_after_solve is True
    assert params.direction == rcspp.graph.SearchDirection.Forward


def test_max_solutions_holds_for_one_solve():
    rg = trade_off_graph()
    solver = rg.solver("simple")
    every = solver.solve()
    assert len(every.solutions) > 1

    one = solver.solve(max_solutions=1)
    assert len(one.solutions) == 1
    # A labelling search finds them all, and keeps the cheapest.
    assert one.solutions[0].cost == every.solutions[0].cost

    assert paths(solver.solve()) == paths(every)


def test_timeout_holds_for_one_solve():
    rg = layered_graph()
    params = AlgorithmParams()
    params.timeout_s = 0.0
    solver = rg.solver("simple", params=params)
    assert solver.solve().status == AlgorithmStatus.Timeout

    lifted = solver.solve(timeout_s=math.inf)
    assert lifted.status == AlgorithmStatus.Complete
    assert lifted.solutions[0].cost == rg.solve("simple").solutions[0].cost

    assert solver.solve().status == AlgorithmStatus.Timeout


def test_bucket_params():
    rg = layered_graph()
    params = BucketAlgorithmParams(bucket_width=2)
    solver = rg.solver("pulling", params=params)
    for duals in DUAL_SETS:
        rg.update_reduced_costs(duals)
        assert paths(solver.solve()) == paths(rg.solve("pulling", params=params))


def test_direction():
    """A solver searches in the direction it was built for."""
    rg = layered_graph()
    solver = rg.solver("simple", direction="backward")
    expected = rg.solve("simple", direction="backward")
    assert paths(solver.solve()) == paths(expected)


def test_a_direction_the_algorithm_does_not_search_is_refused():
    rg = layered_graph()
    with pytest.raises(ValueError):
        rg.solver("greedy", direction="bidirectional")


def test_an_unknown_algorithm_is_refused():
    with pytest.raises(ValueError):
        layered_graph().solver("unknown")


def test_negative_cost_index_is_refused():
    solver = layered_graph().solver()
    with pytest.raises(ValueError):
        solver.solve(cost_index=-1)


def test_the_solver_sees_arcs_added_after_it():
    """Nodes and arcs added after the solver was built are solved too."""
    rg = layered_graph()
    solver = rg.solver("simple")
    before = solver.solve().solutions[0].cost
    shortcut = rg.add_arc((0.5, 1.0), 0, 10, cost=0.5)
    after = solver.solve()
    assert after.solutions[0].cost == 0.5 < before
    assert list(after.solutions[0].path_arc_ids) == [shortcut]


def test_the_solver_keeps_its_graph_alive():
    solver = layered_graph().solver("pulling")
    gc.collect()
    assert isinstance(solver.graph, ResourceGraph)
    assert solver.solve().solutions


def test_repr_and_export():
    solver = layered_graph().solver()
    assert isinstance(solver, rcspp.Solver)
    assert isinstance(solver, Solver)
    solver.solve()
    assert repr(solver) == f"<Solver: {solver.number_of_retained_labels} labels retained>"
