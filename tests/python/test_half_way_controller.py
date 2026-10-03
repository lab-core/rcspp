#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The half-way controller, through the Python surface.

The controller's rules are tested in C++ (tests/cpp/test_half_way_controller.hpp) with
exact numbers. What is tested here is the Python route to them: rg.solve builds a fresh
algorithm every call, so a Python caller keeps the controller across the loop, feeds its
``h`` into ``half_way_point``, and hands it each result. These tests run that loop.
"""

import os
import sys

import pytest

sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "python", "src")
)

import rcspp  # noqa: E402
from rcspp import HalfWayController, HalfWayControllerParams, HalfWayMove  # noqa: E402
from rcspp.graph import AlgorithmParams, ResourceGraph  # noqa: E402
from rcspp.resource import (  # noqa: E402
    AdditionExtensionFunction,
    TimeWindowExtensionFunction,
    TimeWindowFeasibilityFunction,
    TrivialCostFunction,
    TrivialFeasibilityFunction,
    ValueCostFunction,
    ValueDominanceFunction,
)

# -- Helpers ------------------------------------------------------------------

ARCS = 10
STEP_TIME = 10.0

# The forward search stops after two arcs; the backward search, whose values count down
# from the window's end at 1000, never falls below H and covers the whole line. A
# backward-heavy split by construction, which the controller must answer by raising H.
LOW_H = 15.0


def _long_window_line():
    """Cost in slot 0, a wide time window in slot 1 -- the clock."""
    windows = {node_id: (0.0, 1000.0) for node_id in range(ARCS + 1)}
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
    for node_id in range(ARCS + 1):
        rg.add_node(node_id, source=(node_id == 0), sink=(node_id == ARCS))
    for i in range(ARCS):
        rg.add_arc((1.0, STEP_TIME), i, i + 1, cost=1.0)
    return rg


def _params(half_way_point):
    p = AlgorithmParams()
    p.half_way_point = half_way_point
    p.critical_resource_index = 1
    return p


# -- Construction -------------------------------------------------------------


def test_exported_from_the_package():
    """The documented import path works."""
    assert rcspp.HalfWayController is HalfWayController
    assert rcspp.HalfWayMove is HalfWayMove


def test_seeds_at_the_initial_h():
    controller = HalfWayController(100.0)
    assert controller.h == 100.0
    assert controller.initial_h == 100.0
    assert controller.range == 200.0
    assert controller.step == pytest.approx(0.2)
    assert controller.frozen is False
    assert controller.observations == 0
    assert "HalfWayController(h=100" in repr(controller)


def test_refuses_a_zero_seed_and_bad_params():
    """Zero is the 'bound off' sentinel, so there is nothing to adapt from it; a
    std::invalid_argument arrives as ValueError."""
    with pytest.raises(ValueError):
        HalfWayController(0.0)

    params = HalfWayControllerParams()
    params.step_decay = 0.5
    with pytest.raises(ValueError):
        HalfWayController(10.0, params)


def test_params_round_trip_and_are_used():
    params = HalfWayControllerParams()
    assert params.dead_zone == pytest.approx(0.2)
    params.initial_step = 0.1
    params.min_step = 0.01
    controller = HalfWayController(10.0, params)
    assert controller.step == pytest.approx(0.1)


# -- The loop -----------------------------------------------------------------


def test_the_python_loop_raises_h_and_keeps_the_answer():
    """The documented loop: read h into half_way_point, solve, update. H rises on this
    backward-heavy line, and every solve still returns the optimum."""
    rg = _long_window_line()
    controller = HalfWayController(LOW_H)

    used = []
    for _ in range(5):
        result = rg.solve(algorithm="bidirectional", params=_params(controller.h))
        assert result.bounded_by_half_way is True
        assert len(result.solutions) > 0
        assert result.solutions[0].cost == pytest.approx(float(ARCS), abs=1e-9)
        used.append(result.half_way_point_used)
        controller.update(result)

    assert used[0] == LOW_H
    assert used[1] > used[0], "a backward-heavy split must raise H"
    assert all(later >= earlier for earlier, later in zip(used, used[1:]))
    assert max(used) <= 0.95 * 2.0 * LOW_H + 1e-9, "the clamp holds"
    assert controller.moves > 0
    assert controller.observations == 5


def test_a_forward_solve_teaches_nothing():
    """A forward result has no bound in force, so the controller skips it."""
    rg = _long_window_line()
    controller = HalfWayController(LOW_H)
    move = controller.update(rg.solve(algorithm="simple"))
    assert move == HalfWayMove.Skipped
    assert controller.h == LOW_H


def test_truncated_is_honoured():
    """The status cannot show a per-node extension cap, so the caller says so."""
    rg = _long_window_line()
    controller = HalfWayController(LOW_H)
    result = rg.solve(algorithm="bidirectional", params=_params(LOW_H))
    assert controller.update(result, truncated=True) == HalfWayMove.Skipped
    assert controller.update(result) == HalfWayMove.Up


def test_freezing_and_resetting():
    rg = _long_window_line()
    controller = HalfWayController(LOW_H)
    controller.update(rg.solve(algorithm="bidirectional", params=_params(controller.h)))
    learned = controller.h
    assert learned > LOW_H

    controller.frozen = True
    result = rg.solve(algorithm="bidirectional", params=_params(controller.h))
    assert controller.update(result) == HalfWayMove.Frozen
    assert controller.h == learned
    assert controller.last_move == HalfWayMove.Frozen

    controller.reset()
    assert controller.h == LOW_H
    assert controller.frozen is True, "reset forgets what was learned, not the switch"
