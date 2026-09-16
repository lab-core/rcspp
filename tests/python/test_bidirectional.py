#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The bidirectional algorithm, through the Python surface.

The algorithm itself is tested in C++ (tests/cpp/test_bidirectional.hpp). What is tested here is
the surface: the string maps to the enum, the three params reach the algorithm, the setup refusal
crosses the binding boundary as an exception rather than a crash, and BudgetExtensionFunction --
the extension function a bidirectional capacity model needs, and the only one that was C++-only --
is constructible for the signed numerical types and absent for the unsigned one.
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
    """Cost in slot 0, a time window in slot 1 -- the clock a bidirectional solve can
    use."""
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
    """The error listing valid names is built from ALGORITHMS, so it must mention the
    new one."""
    rg = _cost_only_graph([1.0])
    with pytest.raises(ValueError, match="bidirectional"):
        rg.solve(algorithm="not-an-algorithm")


@pytest.mark.parametrize(
    "arc_costs, half_way_point",
    [([1.0, 2.0, 3.0, 4.0], 5.0), ([2.0, -3.0, 1.0, 5.0, -1.0], 2.0)],
)
def test_matches_simple_on_a_small_instance(arc_costs, half_way_point):
    """Same optimum as algorithm='simple'.

    Compared with pytest.approx, never with ==: a joined path sums two independently
    accumulated cost chains, so the floating-point association order differs from a
    single forward accumulation and the totals can disagree in the last bits.
    """
    expected = _cost_only_graph(arc_costs).solve(algorithm="simple").solutions[0].cost

    result = _cost_only_graph(arc_costs).solve(
        algorithm="bidirectional", params=_bidirectional_params(half_way_point)
    )
    assert len(result.solutions) > 0
    assert result.solutions[0].cost == pytest.approx(expected, abs=1e-9)


def test_matches_simple_with_a_time_window_clock():
    """With a real clock the bound is in force, and the answer is still the forward
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
    """An instance whose optimum never reaches the half-way point is still found.

    Such a path crosses H on no arc, so the join cannot produce it; it exists only as a
    forward label that ran all the way to a sink.
    """
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
    """An inert flag in a public API invites a user to set it and conclude the policy is
    broken.

    It stays a C++-side placeholder until the dynamic policy exists.
    """
    p = AlgorithmParams()
    assert not hasattr(p, "dynamic_half_way")


# -- Diagnostics on the result ------------------------------------------------


def test_result_reports_whether_the_bound_was_in_force():
    """The documentation tells Python users to check this, so Python must be able to."""
    windows = {node_id: (0.0, 1000.0) for node_id in range(5)}
    arcs = [(1.0, 10.0, i, i + 1) for i in range(4)]

    result = _time_window_graph(windows, arcs).solve(
        algorithm="bidirectional",
        params=_bidirectional_params(20.0, critical_resource_index=1),
    )
    assert result.bounded_by_half_way is True
    assert result.number_of_joined_paths >= 0


def test_a_cost_only_model_reports_the_bound_off():
    """Cost is never a clock -- reduced costs go negative -- so the bound must switch
    itself off, and the result must say so rather than only logging it."""
    result = _cost_only_graph([1.0, 2.0, 3.0]).solve(
        algorithm="bidirectional",
        params=_bidirectional_params(3.0),
    )
    assert result.bounded_by_half_way is False


def test_a_forward_solve_leaves_the_diagnostics_at_their_defaults():
    """The fields are bidirectional-only; every other algorithm must leave them
    alone."""
    result = _cost_only_graph([1.0, 2.0, 3.0]).solve(algorithm="simple")
    assert result.bounded_by_half_way is False
    assert result.number_of_joined_paths == 0


def test_half_way_point_reaches_the_algorithm():
    """A half-way point that cuts the graph in two still returns the optimum.

    If the param were dropped on the way through the binding every solve here would be
    identical, so what matters is that values which change what each direction explores
    -- one below the path's total clock, one above it -- do not change the answer.
    """
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
    """A model that cannot express backward semantics raises, and names the component.

    An accumulating extension with a feasibility function that supplies a backward seed: each half
    is legal on its own, and only a check that sees both catches it. Forward-only solves on the
    same model are unaffected, which is what makes it worth refusing loudly.
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
        MinMaxFeasibilityFunction(0.0, 5.0),
        TrivialCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_node(0, source=True)
    rg.add_node(1, sink=True)
    rg.add_arc((1.0, 1.0), 0, 1, cost=1.0)

    # Forward-only is fine on exactly this model.
    assert len(rg.solve(algorithm="simple").solutions) > 0

    with pytest.raises(RuntimeError, match="component 1"):
        rg.solve(algorithm="bidirectional", params=_bidirectional_params(1.0))


# -- BudgetExtensionFunction --------------------------------------------------


def test_budget_extension_function_is_bound_for_signed_types_only():
    """Signed only: extending a budget backwards subtracts, and unsigned would wrap."""
    assert hasattr(_resource, "BudgetExtensionFunction_real")
    assert hasattr(_resource, "BudgetExtensionFunction_int")
    assert not hasattr(_resource, "BudgetExtensionFunction_uint")

    with pytest.raises(TypeError, match="uint"):
        BudgetExtensionFunction().create("uint")


def test_budget_extension_function_accepts_per_node_bounds_and_a_default():
    """Every constructor shape reaches C++: bare, per-node, and per-node with a
    default."""
    assert BudgetExtensionFunction().create("real") is not None
    assert BudgetExtensionFunction({0: 5.0, 1: 3.0}).create("real") is not None
    assert BudgetExtensionFunction({0: 5}, default_max=9).create("int") is not None


def test_capacity_model_solves_with_a_budget_clock():
    """A capacity expressed as a budget is a usable bidirectional clock.

    This is the pairing the algorithm asks for and the reason the binding had to exist: the same
    capacity written with AdditionExtensionFunction is refused at setup.
    """
    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_real_resource(
        BudgetExtensionFunction(),
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
