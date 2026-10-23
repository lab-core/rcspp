#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""``rcspp.presets`` -- one call per resource kind, through the Python surface.

Each preset must match the hand-built model. Also pins the Python-only rule that the first
registered resource is a ``"real"`` cost resource, and that the memory presets keep a path
elementary in every search direction.
"""

import os
import sys

import pytest

sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "python", "src")
)

import rcspp  # noqa: E402
from rcspp import presets  # noqa: E402
from rcspp.graph import AlgorithmParams, ResourceGraph  # noqa: E402
from rcspp.resource import (  # noqa: E402
    AdditionExtensionFunction,
    CapacityExtensionFunction,
    IntersectionFeasibilityFunction,
    MinMaxFeasibilityFunction,
    NgPathExtensionFunction,
    SubsetDominanceFunction,
    TimeWindowExtensionFunction,
    TimeWindowFeasibilityFunction,
    TrivialCostFunction,
    TrivialFeasibilityFunction,
    ValueCostFunction,
    ValueDominanceFunction,
)

WINDOWS = {0: (0.0, 100.0), 1: (0.0, 100.0), 2: (0.0, 100.0)}
ARCS = [(3.0, 4.0, 0, 1), (5.0, 6.0, 1, 2), (10.0, 2.0, 0, 2)]


def _finish(rg):
    for node_id in (0, 1, 2):
        rg.add_node(node_id, source=(node_id == 0), sink=(node_id == 2))
    for cost, time, origin, destination in ARCS:
        rg.add_arc((cost, time), origin, destination, cost=cost)
    return rg


def _solved(rg):
    result = rg.solve(algorithm="simple")
    return [(s.cost, tuple(s.path_arc_ids)) for s in result.solutions]


# -- The module is exported ---------------------------------------------------


def test_presets_is_exported_from_the_package():
    assert rcspp.presets is presets
    assert "presets" in rcspp.__all__


# -- Preset versus hand-built -------------------------------------------------


def test_window_resource_matches_the_hand_built_model():
    by_preset = ResourceGraph()
    presets.add_cost_resource(by_preset)
    presets.add_window_resource(by_preset, "real", WINDOWS)
    _finish(by_preset)

    by_hand = ResourceGraph()
    by_hand.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    by_hand.add_real_resource(
        TimeWindowExtensionFunction(WINDOWS),
        TimeWindowFeasibilityFunction(WINDOWS),
        TrivialCostFunction(),
        ValueDominanceFunction(),
    )
    _finish(by_hand)

    assert _solved(by_preset) == _solved(by_hand)


def test_capacity_resource_matches_the_hand_built_model():
    capacity = 20

    by_preset = ResourceGraph()
    presets.add_cost_resource(by_preset)
    presets.add_capacity_resource(by_preset, "int", capacity)
    for node_id in (0, 1, 2):
        by_preset.add_node(node_id, source=(node_id == 0), sink=(node_id == 2))
    for cost, _time, origin, destination in ARCS:
        by_preset.add_arc((cost, 5), origin, destination, cost=cost)

    by_hand = ResourceGraph()
    by_hand.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    by_hand.add_int_resource(
        CapacityExtensionFunction(capacity),
        MinMaxFeasibilityFunction(0, capacity),
        TrivialCostFunction(),
        ValueDominanceFunction(),
    )
    for node_id in (0, 1, 2):
        by_hand.add_node(node_id, source=(node_id == 0), sink=(node_id == 2))
    for cost, _time, origin, destination in ARCS:
        by_hand.add_arc((cost, 5), origin, destination, cost=cost)

    assert _solved(by_preset) == _solved(by_hand)


def test_cost_resource_matches_the_hand_built_model():
    by_preset = ResourceGraph()
    presets.add_cost_resource(by_preset)
    for node_id in (0, 1, 2):
        by_preset.add_node(node_id, source=(node_id == 0), sink=(node_id == 2))
    for cost, _time, origin, destination in ARCS:
        by_preset.add_arc(cost, origin, destination, cost=cost)

    by_hand = ResourceGraph()
    by_hand.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    for node_id in (0, 1, 2):
        by_hand.add_node(node_id, source=(node_id == 0), sink=(node_id == 2))
    for cost, _time, origin, destination in ARCS:
        by_hand.add_arc(cost, origin, destination, cost=cost)

    assert _solved(by_preset) == _solved(by_hand)


# -- The properties the presets exist for -------------------------------------


def test_preset_built_model_solves_bidirectionally():
    """A preset-built model is coherent backwards: the bidirectional solve finds a path."""
    rg = ResourceGraph()
    presets.add_cost_resource(rg)
    presets.add_window_resource(rg, "real", WINDOWS)
    _finish(rg)

    params = AlgorithmParams()
    params.half_way_point = 5.0
    params.critical_resource_index = 1
    result = rg.solve(algorithm="bidirectional", params=params)
    assert len(result.solutions) > 0


def test_capacity_preset_takes_a_uniform_capacity_only():
    """No per-node caps and no floor from Python.

    ``MinMaxFeasibilityFunction`` takes a single window from Python, and a floor on a threshold is
    never checked backward.
    """
    rg = ResourceGraph()
    presets.add_cost_resource(rg)
    with pytest.raises(TypeError):
        presets.add_capacity_resource(rg, "int", 20, per_node={1: 5})
    with pytest.raises(TypeError):
        presets.add_capacity_resource(rg, "int", 20, minimum=5)


def test_registration_order_contract_is_enforced():
    """A preset does not exempt a caller from the cost-resource-first rule."""
    rg = ResourceGraph()
    with pytest.raises(ValueError, match="first registered resource"):
        presets.add_capacity_resource(rg, "int", 20)


def _cycle_graph(add_memory):
    """0 -> 1 -> 2 -> 3, with 2 -> 1 closing a cycle that pays off: going around twice
    is the cheapest walk, so only a memory that forbids revisiting node 1 keeps the path
    elementary.

    The time window (slot 1) bounds the walk and is the bidirectional search's clock.
    """
    rg = ResourceGraph()
    presets.add_cost_resource(rg)
    presets.add_window_resource(rg, "real", {node: (0.0, 100.0) for node in range(4)})
    add_memory(rg)
    for node_id in range(4):
        rg.add_node(node_id, source=(node_id == 0), sink=(node_id == 3))
    # (cost, time, memory): the memory reads the arc's endpoints, so its value is empty.
    for cost, origin, destination in [(-1.0, 0, 1), (-5.0, 1, 2), (-5.0, 2, 1), (-1.0, 2, 3)]:
        rg.add_arc((cost, 10.0, set()), origin, destination, cost=cost)
    return rg


def _best(rg, algorithm, params=None):
    if params is None:
        result = rg.solve(algorithm=algorithm)
    else:
        result = rg.solve(algorithm=algorithm, params=params)
    return min(s.cost for s in result.solutions), result


@pytest.mark.parametrize(
    "add_memory",
    [
        lambda rg: presets.add_elementary_resource(rg, "bitset", range(4)),
        lambda rg: presets.add_ng_path_resource(rg, "bitset", {n: set(range(4)) for n in range(4)}),
    ],
    ids=["elementary", "ng-path with full neighbourhoods"],
)
def test_memory_presets_forbid_the_cycle_in_every_direction(add_memory):
    """Both presets keep the path elementary, forward and bidirectionally."""
    forward, result = _best(_cycle_graph(add_memory), "simple")
    assert forward == pytest.approx(-7.0)  # 0-1-2-3; the cycle 1-2-1 is forbidden
    for solution in result.solutions:
        assert len(solution.path_node_ids) == len(set(solution.path_node_ids))

    params = AlgorithmParams()
    params.half_way_point = 15.0
    params.critical_resource_index = 1
    bidirectional, _ = _best(_cycle_graph(add_memory), "bidirectional", params)
    assert bidirectional == pytest.approx(forward)


def test_ng_path_preset_matches_the_hand_built_model():
    """The ng preset is the four-object form, forbidding ``{v}`` at each node."""
    neighborhoods = {0: {0, 1}, 1: {1, 2}, 2: {1, 2}, 3: {3}}

    def by_hand(rg):
        rg.add_bitset_resource(
            NgPathExtensionFunction(neighborhoods),
            IntersectionFeasibilityFunction({n: {n} for n in range(4)}, forbidden=True),
            TrivialCostFunction(),
            SubsetDominanceFunction(),
        )

    preset, _ = _best(
        _cycle_graph(lambda rg: presets.add_ng_path_resource(rg, "bitset", neighborhoods)),
        "simple",
    )
    hand_built, _ = _best(_cycle_graph(by_hand), "simple")
    assert preset == pytest.approx(hand_built)


def test_every_public_preset_is_in_all():
    public = {name for name in dir(presets) if name.startswith("add_")}
    assert public == set(presets.__all__)
