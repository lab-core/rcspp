#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The bidirectional algorithm, through the Python surface.

The algorithm itself is tested in C++. These tests cover the binding: name and enum,
params, setup refusal raised as an exception, and BudgetExtensionFunction for signed
types only.
"""

import os
import sys

import pytest

sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "python", "src")
)

import rcspp._core.resource as _resource  # noqa: E402
from rcspp.graph import ALGORITHMS, Algorithm, AlgorithmParams, ResourceGraph  # noqa: E402
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

# -- Helpers ------------------------------------------------------------------


def _cost_only_graph(arc_costs):
    """A line graph whose single resource is the cost."""
    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    for node_id in range(len(arc_costs) + 1):
        rg.add_node(node_id, source=(node_id == 0), sink=(node_id == len(arc_costs)))
    for i, cost in enumerate(arc_costs):
        rg.add_arc(cost, i, i + 1, cost=cost)
    return rg


def _time_window_graph(windows, arcs):
    """Cost in slot 0, a time window in slot 1 (usable as the bidirectional clock)."""
    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_real_resource(
        TimeWindowExtensionFunction(windows),
        TimeWindowFeasibilityFunction(windows),
        TrivialCostFunction(),
        ValueDominanceFunction(),
    )
    for node_id in sorted(windows):
        rg.add_node(node_id, source=(node_id == 0), sink=(node_id == max(windows)))
    for cost, time, origin, destination in arcs:
        rg.add_arc((cost, time), origin, destination, cost=cost)
    return rg


def _bidirectional_params(half_way_point, critical_resource_index=0):
    p = AlgorithmParams()
    p.half_way_point = half_way_point
    p.critical_resource_index = critical_resource_index
    return p


# -- Reachability and agreement with the forward search -----------------------


def test_bidirectional_is_reachable_by_name():
    """The string maps to the enum, and the enum value is the last one."""
    assert "bidirectional" in ALGORITHMS
    assert Algorithm.Bidirectional is not None
    # New values go at the end so persisted integer values stay stable.
    assert list(Algorithm.__members__)[-1] == "Bidirectional"

    rg = _cost_only_graph([1.0, 2.0, 3.0])
    result = rg.solve(algorithm="bidirectional", params=_bidirectional_params(5.0))
    assert len(result.solutions) > 0


def test_unknown_algorithm_message_lists_bidirectional():
    """The error listing valid names mentions bidirectional."""
    rg = _cost_only_graph([1.0])
    with pytest.raises(ValueError, match="bidirectional"):
        rg.solve(algorithm="not-an-algorithm")


@pytest.mark.parametrize(
    "arc_costs, half_way_point",
    [([1.0, 2.0, 3.0, 4.0], 5.0), ([2.0, -3.0, 1.0, 5.0, -1.0], 2.0)],
)
def test_matches_simple_on_a_small_instance(arc_costs, half_way_point):
    """Same optimum as algorithm='simple'.

    Compared with pytest.approx: a joined path sums two cost chains, so the last bits
    can differ from a forward accumulation.
    """
    expected = _cost_only_graph(arc_costs).solve(algorithm="simple").solutions[0].cost

    result = _cost_only_graph(arc_costs).solve(
        algorithm="bidirectional", params=_bidirectional_params(half_way_point)
    )
    assert len(result.solutions) > 0
    assert result.solutions[0].cost == pytest.approx(expected, abs=1e-9)


def test_matches_simple_with_a_time_window_clock():
    """With a real clock the bound is in force, and the answer matches the forward
    one."""
    windows = {0: (0.0, 100.0), 1: (0.0, 100.0), 2: (0.0, 100.0), 3: (0.0, 5.0)}
    # The cheap route 0 -> 2 -> 3 arrives at 6 and misses node 3's window.
    arcs = [
        (10.0, 1.0, 0, 1),
        (10.0, 1.0, 1, 3),
        (1.0, 3.0, 0, 2),
        (1.0, 3.0, 2, 3),
    ]

    expected = _time_window_graph(windows, arcs).solve(algorithm="simple").solutions[0].cost
    assert expected == pytest.approx(20.0, abs=1e-9)

    result = _time_window_graph(windows, arcs).solve(
        algorithm="bidirectional",
        params=_bidirectional_params(3.0, critical_resource_index=1),
    )
    assert len(result.solutions) > 0
    assert result.solutions[0].cost == pytest.approx(expected, abs=1e-9)


def test_short_route_is_found():
    """An optimum that never reaches the half-way point is still found (as a forward
    label that reached a sink, since the join cannot produce it)."""
    windows = {0: (0.0, 1000.0), 1: (0.0, 1000.0), 2: (0.0, 1000.0)}
    arcs = [(1.0, 1.0, 0, 1), (1.0, 1.0, 1, 2)]

    result = _time_window_graph(windows, arcs).solve(
        algorithm="bidirectional",
        params=_bidirectional_params(50.0, critical_resource_index=1),
    )
    assert len(result.solutions) > 0
    assert result.solutions[0].cost == pytest.approx(2.0, abs=1e-9)


# -- Params -------------------------------------------------------------------


def test_params_are_settable():
    """The two bidirectional params round-trip through AlgorithmParams."""
    p = AlgorithmParams()
    assert p.critical_resource_index == 0
    assert p.half_way_point == 0.0

    p.critical_resource_index = 2
    p.half_way_point = 12.5
    assert p.critical_resource_index == 2
    assert p.half_way_point == 12.5


def test_dynamic_half_way_is_not_exposed():
    """The dynamic half-way flag is inert, so it is not exposed to Python."""
    p = AlgorithmParams()
    assert not hasattr(p, "dynamic_half_way")


# -- Diagnostics on the result ------------------------------------------------


def test_result_reports_whether_the_bound_was_in_force():
    """The result reports whether the half-way bound was in force."""
    windows = {node_id: (0.0, 1000.0) for node_id in range(5)}
    arcs = [(1.0, 10.0, i, i + 1) for i in range(4)]

    result = _time_window_graph(windows, arcs).solve(
        algorithm="bidirectional",
        params=_bidirectional_params(20.0, critical_resource_index=1),
    )
    assert result.bounded_by_half_way is True
    assert result.number_of_joined_paths >= 0


def test_a_cost_only_model_reports_the_bound_off():
    """Cost cannot be a clock (reduced costs go negative), so the bound switches off and
    the result says so."""
    result = _cost_only_graph([1.0, 2.0, 3.0]).solve(
        algorithm="bidirectional",
        params=_bidirectional_params(3.0),
    )
    assert result.bounded_by_half_way is False


def test_a_forward_solve_leaves_the_diagnostics_at_their_defaults():
    """The fields are bidirectional-only; other algorithms leave them at their
    defaults."""
    result = _cost_only_graph([1.0, 2.0, 3.0]).solve(algorithm="simple")
    assert result.bounded_by_half_way is False
    assert result.number_of_joined_paths == 0


def test_half_way_point_reaches_the_algorithm():
    """Half-way points below and above the path's total clock all return the optimum."""
    windows = {node_id: (0.0, 1000.0) for node_id in range(5)}
    arcs = [(1.0, 10.0, i, i + 1) for i in range(4)]

    for half_way_point in (5.0, 20.0, 500.0):
        result = _time_window_graph(windows, arcs).solve(
            algorithm="bidirectional",
            params=_bidirectional_params(half_way_point, critical_resource_index=1),
        )
        assert len(result.solutions) > 0
        assert result.solutions[0].cost == pytest.approx(4.0, abs=1e-9)


# -- Setup refusal ------------------------------------------------------------


def test_undeclared_resource_raises_at_setup():
    """A model that cannot express backward semantics raises, naming the component.

    A budget with a time window's feasibility function is only wrong in combination: the
    window opens at 0.5, which the budget never waits for, so the backward search would
    reject deadlines a forward path meets. Forward solves on the same model still work.
    """
    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_real_resource(
        BudgetExtensionFunction(10.0),
        TimeWindowFeasibilityFunction({1: (0.5, 10.0)}, 10.0),
        TrivialCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_node(0, source=True)
    rg.add_node(1, sink=True)
    rg.add_arc((1.0, 1.0), 0, 1, cost=1.0)

    # Forward-only is fine on exactly this model.
    assert len(rg.solve(algorithm="simple").solutions) > 0

    with pytest.raises(RuntimeError, match="component 1: its feasibility function rejects"):
        rg.solve(algorithm="bidirectional", params=_bidirectional_params(1.0))


# -- BudgetExtensionFunction --------------------------------------------------


def test_budget_extension_function_is_bound_for_signed_types_only():
    """Signed only: extending a budget backwards subtracts, and unsigned would wrap."""
    assert hasattr(_resource, "BudgetExtensionFunction_real")
    assert hasattr(_resource, "BudgetExtensionFunction_int")
    assert not hasattr(_resource, "BudgetExtensionFunction_uint")

    with pytest.raises(TypeError, match="uint"):
        BudgetExtensionFunction(5).create("uint")


def test_budget_extension_function_takes_a_capacity():
    """The capacity is required, and is the same at every node."""
    assert BudgetExtensionFunction(5.0).create("real") is not None
    assert BudgetExtensionFunction(capacity=9).create("int") is not None
    with pytest.raises(TypeError):
        BudgetExtensionFunction()


def test_a_budget_whose_capacity_differs_from_its_feasibility_function_is_refused():
    """A budget whose capacity differs from its feasibility function is refused.

    The budget clamps backward labels to its own capacity, so it must be the one the
    feasibility function enforces forward.
    """
    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_real_resource(
        BudgetExtensionFunction(4.0),
        MinMaxFeasibilityFunction(0.0, 5.0),
        TrivialCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_node(0, source=True)
    rg.add_node(1, sink=True)
    rg.add_arc((1.0, 1.0), 0, 1, cost=1.0)

    assert len(rg.solve(algorithm="simple").solutions) > 0
    with pytest.raises(RuntimeError, match="clamped to 4.0+, but its feasibility function"):
        rg.solve(algorithm="bidirectional", params=_bidirectional_params(1.0))


def test_capacity_model_solves_with_a_budget_clock():
    """A capacity expressed as a budget is a usable bidirectional clock (the same
    capacity written with AdditionExtensionFunction solves too, but it is not a
    threshold, so it cannot be the clock)."""
    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_real_resource(
        BudgetExtensionFunction(5.0),
        MinMaxFeasibilityFunction(0.0, 5.0),
        TrivialCostFunction(),
        ValueDominanceFunction(),
    )
    for node_id in range(4):
        rg.add_node(node_id, source=(node_id == 0), sink=(node_id == 3))
    # The cheap route weighs 4 + 4 = 8 and busts the capacity of 5.
    rg.add_arc((1.0, 4.0), 0, 1, cost=1.0)
    rg.add_arc((1.0, 4.0), 1, 3, cost=1.0)
    rg.add_arc((3.0, 1.0), 0, 2, cost=3.0)
    rg.add_arc((3.0, 1.0), 2, 3, cost=3.0)

    expected = rg.solve(algorithm="simple").solutions[0].cost
    assert expected == pytest.approx(6.0, abs=1e-9)

    result = rg.solve(
        algorithm="bidirectional",
        params=_bidirectional_params(3.0, critical_resource_index=1),
    )
    assert len(result.solutions) > 0
    assert result.solutions[0].cost == pytest.approx(expected, abs=1e-9)


def _diamond_window_graph():
    """Two routes 0->{1,2}->3, so labels actually compete at a node.

    A line gives each node one label per direction, and the container is consulted
    before the insert, so a line performs zero dominance comparisons -- correctly.  The
    diamond is what makes ``dominance_checks`` non-zero.
    """
    windows = {node_id: (0.0, 1000.0) for node_id in range(4)}
    arcs = [
        (1.0, 10.0, 0, 1),
        (2.0, 10.0, 0, 2),
        (1.0, 10.0, 1, 3),
        (1.0, 10.0, 2, 3),
    ]
    return _time_window_graph(windows, arcs)


def test_result_reports_the_label_counts_and_dominance_checks():
    """A bounded bidirectional solve reports both sides and the H it used."""
    params = _bidirectional_params(20.0, critical_resource_index=1)
    result = _diamond_window_graph().solve(algorithm="bidirectional", params=params)

    assert result.bounded_by_half_way is True
    assert result.forward_labels > 0
    assert result.backward_labels > 0
    assert result.dominance_checks > 0
    assert result.half_way_point_used == params.half_way_point


def test_the_reported_half_way_point_is_the_one_applied_not_the_one_asked_for():
    """Cost is never a clock, so the bound switches itself off and H is reported as 0.

    Paired with ``bounded_by_half_way`` this is what tells a caller that the 0 means
    "no bound" rather than "H really was 0" -- the params still say 3.0.
    """
    result = _cost_only_graph([1.0, 2.0, 3.0]).solve(
        algorithm="bidirectional",
        params=_bidirectional_params(3.0),
    )
    assert result.bounded_by_half_way is False
    assert result.half_way_point_used == 0.0


def test_a_forward_solve_leaves_the_backward_diagnostics_at_zero():
    """Only a search that ran backwards reports backward labels."""
    result = _diamond_window_graph().solve(algorithm="simple")

    assert result.forward_labels > 0
    assert result.backward_labels == 0
    assert result.dominance_checks > 0
    assert result.half_way_point_used == 0.0
