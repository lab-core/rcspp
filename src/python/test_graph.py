#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

import math
import os
import sys

relative_path = "../python_interface/"
sys.path.insert(0, os.path.abspath(relative_path))

from rcspp.graph import ResourceGraph  # noqa: E402
from rcspp.resource import (  # noqa: E402
    AdditionExtensionFunction,
    MinMaxFeasibilityFunction,
    ValueCostFunction,
    ValueDominanceFunction,
)

# ── helpers ───────────────────────────────────────────────────────────────────


def make_resource_graph():
    """Return a ResourceGraph with a single real resource (addition + min/max)."""
    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction(),
        MinMaxFeasibilityFunction(0.0, 100.0),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    return rg


def make_diamond():
    """3-node diamond graph:

         0 --a0(cost 1)--> 1 --a2(cost 3)--> 2
         0 --a1(cost 5)--> 2

    Arc ids assigned by insertion order: a0=0, a1=1, a2=2.
    """
    rg = make_resource_graph()
    rg.add_node(0, source=True)
    rg.add_node(1)
    rg.add_node(2, sink=True)
    rg.add_arc(1.0, 0, 1, cost=1.0, arc_id=0)
    rg.add_arc(5.0, 0, 2, cost=5.0, arc_id=1)
    rg.add_arc(3.0, 1, 2, cost=3.0, arc_id=2)
    rg.update()
    return rg


# ── force_arc tests ────────────────────────────────────────────────────────────


def test_force_arc_by_id_removes_competing_in_arc():
    """force_arc(arc_id) on arc 2 (1→2) removes arc 1 (0→2, competing in-arc of node
    2)."""
    rg = make_diamond()

    removed = rg.force_arc(2)

    assert sorted(removed) == [1], f"Expected [1], got {removed}"
    assert rg.get_arc(2) is not None, "Forced arc was removed"
    assert rg.get_arc(1) is None, "Competing arc still active"
    assert rg.get_arc(0) is not None, "Unrelated arc was removed"
    assert rg.number_of_arcs() == 2


def test_force_arc_by_id_removes_competing_out_arc():
    """force_arc(arc_id) on arc 0 (0→1) removes arc 1 (0→2, competing out-arc of node
    0)."""
    rg = make_diamond()

    removed = rg.force_arc(0)

    assert sorted(removed) == [1], f"Expected [1], got {removed}"
    assert rg.get_arc(0) is not None
    assert rg.get_arc(1) is None
    assert rg.get_arc(2) is not None
    n0 = rg.get_node(0)
    assert len(n0.out_arcs) == 1 and n0.out_arcs[0].id == 0


def test_force_arc_by_id_removes_both_sides():
    """force_arc(arc_id) on arc 1 (0→2) removes arc 0 (out of 0) and arc 2 (in to 2)."""
    rg = make_diamond()

    removed = rg.force_arc(1)

    assert sorted(removed) == [0, 2], f"Expected [0, 2], got {sorted(removed)}"
    assert rg.get_arc(1) is not None, "Forced arc was removed"
    assert rg.get_arc(0) is None
    assert rg.get_arc(2) is None
    assert rg.number_of_arcs() == 1


def test_force_arc_by_arc_object():
    """force_arc(arc) overload produces the same result as force_arc(arc_id)."""
    rg = make_diamond()
    arc = rg.get_arc(1)

    removed = rg.force_arc(arc)

    assert sorted(removed) == [0, 2], f"Expected [0, 2], got {sorted(removed)}"
    assert rg.number_of_arcs() == 1


def test_force_arc_nonexistent_id():
    """force_arc on a missing arc_id returns [] without modifying the graph."""
    rg = make_diamond()

    removed = rg.force_arc(99)

    assert removed == [], f"Expected [], got {removed}"
    assert rg.number_of_arcs() == 3


def test_force_arc_already_unique():
    """force_arc when the arc is already the only arc on both ends returns []."""
    rg = make_resource_graph()
    rg.add_node(0, source=True)
    rg.add_node(1, sink=True)
    rg.add_arc(1.0, 0, 1, cost=1.0, arc_id=0)
    rg.update()

    removed = rg.force_arc(0)

    assert removed == [], f"Expected [], got {removed}"
    assert rg.get_arc(0) is not None
    assert rg.number_of_arcs() == 1


def test_force_arc_parallel_arcs_dedup():
    """Parallel arcs (same origin→destination) appear in both lists but are counted
    once."""
    rg = make_resource_graph()
    rg.add_node(0, source=True)
    rg.add_node(1, sink=True)
    rg.add_arc(1.0, 0, 1, cost=1.0, arc_id=0)
    rg.add_arc(2.0, 0, 1, cost=2.0, arc_id=1)
    rg.update()

    removed = rg.force_arc(0)

    assert removed == [1], f"Expected [1], got {removed}"
    assert rg.number_of_arcs() == 1


def test_force_arc_removed_arcs_are_restorable():
    """Arcs removed by force_arc land in the removed-arc pool and can be restored."""
    rg = make_diamond()

    rg.force_arc(2)  # removes arc 1

    assert 1 in rg.removed_arc_ids()
    rg.restore_arc(1)
    assert rg.get_arc(1) is not None
    assert rg.number_of_arcs() == 3


def test_force_arc_solve_uses_forced_path():
    """After forcing arc 2 (1→2), solve must follow 0→1→2 (cost 4), not 0→2 (cost 5)."""
    rg = make_diamond()
    rg.force_arc(2)  # only path: 0→1→2, cost 1+3=4

    sols = rg.solve()

    assert len(sols) >= 1, "Expected at least one solution"
    assert math.isclose(
        sols[0].cost, 4.0, abs_tol=1e-6
    ), f"Expected cost 4.0 after forcing arc 2, got {sols[0].cost}"
    assert sols[0].path_node_ids == [0, 1, 2]
