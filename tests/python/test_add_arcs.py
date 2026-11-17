#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""ResourceGraph.add_arcs: arcs from arrays, the same graph add_arc builds."""

import os
import sys

sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "python", "src")
)

import pytest  # noqa: E402

np = pytest.importorskip("numpy")

from rcspp.graph import ResourceGraph  # noqa: E402
from rcspp.resource import (  # noqa: E402
    AdditionExtensionFunction,
    MinMaxFeasibilityFunction,
    SubsetDominanceFunction,
    TimeWindowExtensionFunction,
    TimeWindowFeasibilityFunction,
    TrivialCostFunction,
    TrivialFeasibilityFunction,
    UnionExtensionFunction,
    ValueCostFunction,
    ValueDominanceFunction,
)

# A grid of N x N nodes, arcs right and down: source 0 at the top left, sink at the bottom right.
N = 6
ARCS = [(row * N + col, row * N + col + 1) for row in range(N) for col in range(N - 1)] + [
    (row * N + col, (row + 1) * N + col) for row in range(N - 1) for col in range(N)
]
SINK = N * N - 1


def _graph(windows=None):
    """A cost, an int capacity and a real time window: registration order differs from
    the canonical one (real, int), so the columns must be reordered."""
    graph = ResourceGraph()
    graph.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    graph.add_int_resource(
        AdditionExtensionFunction(),
        MinMaxFeasibilityFunction(0, 7),
        TrivialCostFunction(),
        ValueDominanceFunction(),
    )
    windows = windows or {node: (0.0, 100.0) for node in range(N * N)}
    graph.add_real_resource(
        TimeWindowExtensionFunction(windows),
        TimeWindowFeasibilityFunction(windows),
        TrivialCostFunction(),
        ValueDominanceFunction(),
    )
    for node in range(N * N):
        graph.add_node(node, source=node == 0, sink=node == SINK)
    return graph


def _consumption():
    rng = np.random.default_rng(7)
    cost = rng.uniform(1.0, 10.0, len(ARCS))
    load = rng.integers(0, 2, len(ARCS)).astype(np.float64)
    time = rng.uniform(1.0, 5.0, len(ARCS))
    return np.column_stack([cost, load, time])


def _best(graph):
    result = graph.solve(algorithm="simple", preprocess=False)
    return result.solutions[0].cost, list(result.solutions[0].path_node_ids)


def test_arrays_build_the_graph_add_arc_builds():
    consumption = _consumption()
    one_by_one = _graph()
    for (origin, destination), (cost, load, time) in zip(ARCS, consumption):
        one_by_one.add_arc((cost, int(load), time), origin, destination)
    at_once = _graph()

    ids = at_once.add_arcs(consumption, [a for a, _ in ARCS], [b for _, b in ARCS])

    assert ids.tolist() == list(range(len(ARCS)))
    assert at_once.number_of_arcs() == one_by_one.number_of_arcs() == len(ARCS)
    assert _best(at_once) == _best(one_by_one)


def test_ids_follow_the_arcs_add_arc_buffered_first():
    graph = _graph()
    consumption = _consumption()
    first = graph.add_arc(
        tuple(consumption[0][:1]) + (int(consumption[0][1]),) + (consumption[0][2],), *ARCS[0]
    )

    ids = graph.add_arcs(consumption[1:], [a for a, _ in ARCS[1:]], [b for _, b in ARCS[1:]])

    assert first == 0
    assert ids.tolist() == list(range(1, len(ARCS)))
    assert graph.add_arc((1.0, 0, 1.0), 0, 1) == len(ARCS)


def test_fixed_costs_and_removal_by_the_ids_returned():
    graph = _graph()
    consumption = np.zeros((len(ARCS), 3))
    costs = np.ones(len(ARCS))
    ids = graph.add_arcs(consumption, [a for a, _ in ARCS], [b for _, b in ARCS], costs=costs)

    assert [graph.get_arc(int(arc_id)).cost for arc_id in ids[:3]] == [1.0, 1.0, 1.0]
    assert sorted(graph.remove_arcs(ids[:N].tolist())) == ids[:N].tolist()
    assert graph.number_of_arcs() == len(ARCS) - N


def test_an_arc_into_the_source_adds_none_of_the_batch():
    graph = _graph()
    consumption = np.ones((3, 3))

    with pytest.raises(ValueError):
        graph.add_arcs(consumption, [0, 1, 1], [1, 2, 0])

    assert graph.number_of_arcs() == 0
    assert graph.add_arcs(consumption[:1], [0], [1]).tolist() == [0]


def test_malformed_input_is_refused():
    graph = _graph()

    with pytest.raises(ValueError, match="shape"):
        graph.add_arcs(np.ones((2, 2)), [0, 1], [1, 2])
    with pytest.raises(ValueError, match="origins"):
        graph.add_arcs(np.ones((2, 3)), [0], [1, 2])
    with pytest.raises(ValueError, match="fraction"):
        graph.add_arcs(np.array([[1.0, 0.5, 1.0]]), [0], [1])


def test_a_container_resource_is_refused():
    graph = ResourceGraph()
    graph.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    graph.add_int_set_resource(
        UnionExtensionFunction(),
        TrivialFeasibilityFunction(),
        TrivialCostFunction(),
        SubsetDominanceFunction(),
    )
    graph.add_node(0, source=True)
    graph.add_node(1, sink=True)

    with pytest.raises(ValueError, match="numerical resources only"):
        graph.add_arcs(np.ones((1, 2)), [0], [1])


def test_a_single_resource_takes_a_flat_array():
    graph = ResourceGraph()
    graph.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    for node in range(3):
        graph.add_node(node, source=node == 0, sink=node == 2)

    graph.add_arcs(np.array([2.0, 3.0, 9.0]), [0, 1, 0], [1, 2, 2])

    assert _best(graph) == (5.0, [0, 1, 2])
