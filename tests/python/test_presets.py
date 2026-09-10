#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""``rcspp.presets`` -- one call per resource kind, through the Python surface.

The claim a preset makes is "this is what you would have written by hand". These tests assert it,
rather than trusting that two lists of four descriptors stay in step. They also pin the
registration-order contract, which is Python-only: ``ResourceGraph`` requires the first registered
resource to be a ``"real"`` cost resource, and the C++ side has no such rule.

There is deliberately no ng-path preset on the Python side: ``NgPathExtensionFunction`` and
``IntersectionFeasibilityFunction`` have no Python descriptors, so an ng-path model cannot be
assembled from Python at all. ``test_no_ng_path_preset_because_the_components_are_unbound`` pins
that, so the asymmetry with C++ reads as a known gap rather than an oversight.
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
    BudgetExtensionFunction,
    MinMaxFeasibilityFunction,
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
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    _finish(by_hand)

    assert _solved(by_preset) == _solved(by_hand)


def test_budget_resource_matches_the_hand_built_model():
    capacity = 20

    by_preset = ResourceGraph()
    presets.add_cost_resource(by_preset)
    presets.add_budget_resource(by_preset, "int", capacity)
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
        BudgetExtensionFunction(None, capacity),
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
    """A budget preset is coherent backwards.

    Finding nothing is the signature of the accumulate-plus-ceiling-seed defect this preset
    exists to prevent, so a non-empty result is the property worth asserting.
    """
    rg = ResourceGraph()
    presets.add_cost_resource(rg)
    presets.add_window_resource(rg, "real", WINDOWS)
    _finish(rg)

    params = AlgorithmParams()
    params.half_way_point = 5.0
    params.critical_resource_index = 1
    result = rg.solve(algorithm="bidirectional", params=params)
    assert len(result.solutions) > 0


def test_registration_order_contract_is_enforced():
    """A preset does not exempt a caller from the cost-resource-first rule."""
    rg = ResourceGraph()
    with pytest.raises(ValueError, match="first registered resource"):
        presets.add_budget_resource(rg, "int", 20)


def test_no_ng_path_preset_because_the_components_are_unbound():
    """The C++/Python asymmetry, pinned so it reads as a known gap.

    ``rcspp::presets`` has ``add_ng_path_resource``; this module does not, because neither of the
    components it would need is exposed to Python. A preset cannot paper over that.
    """
    assert not hasattr(presets, "add_ng_path_resource")

    import rcspp.resource as resource_module

    assert not hasattr(resource_module, "NgPathExtensionFunction")
    assert not hasattr(resource_module, "IntersectionFeasibilityFunction")


def test_every_public_preset_is_in_all():
    public = {name for name in dir(presets) if name.startswith("add_")}
    assert public == set(presets.__all__)
