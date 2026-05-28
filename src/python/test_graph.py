#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

import math
import os
import sys

sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "python_interface")
)

import pytest  # noqa: E402

np = pytest.importorskip("numpy")

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
    rg.add_arc(1.0, 0, 1, cost=1.0)
    rg.add_arc(5.0, 0, 2, cost=5.0)
    rg.add_arc(3.0, 1, 2, cost=3.0)
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
    rg.add_arc(1.0, 0, 1, cost=1.0)
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
    rg.add_arc(1.0, 0, 1, cost=1.0)
    rg.add_arc(2.0, 0, 1, cost=2.0)
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


# ── update_reduced_costs numpy tests ──────────────────────────────────────────


def _make_rg_with_rows():
    """2-arc graph where arc costs are set via rows.

    Arc 0 (0→1): base cost=10, row index=0 coef=1  → reduced = 10 - duals[0]
    Arc 1 (1→2): base cost=20, row index=1 coef=2  → reduced = 20 - 2*duals[1]
    """
    from rcspp.graph import Row

    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction(),
        MinMaxFeasibilityFunction(0.0, 100.0),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_node(0, source=True)
    rg.add_node(1)
    rg.add_node(2, sink=True)
    rg.add_arc(1.0, 0, 1, cost=10.0, rows=[Row(0, 1.0)])
    rg.add_arc(1.0, 1, 2, cost=20.0, rows=[Row(1, 2.0)])
    rg.update()
    return rg


def test_update_reduced_costs_numpy_1d():
    """update_reduced_costs accepts a 1-D numpy array and applies it correctly."""
    rg = _make_rg_with_rows()
    duals = np.array([3.0, 4.0])  # reduced: arc0 = 10-3=7, arc1 = 20-8=12

    rg.update_reduced_costs(duals)
    sols = rg.solve(preprocess=False)

    assert len(sols) >= 1
    assert math.isclose(
        sols[0].cost, 7.0 + 12.0, abs_tol=1e-6
    ), f"Expected 19.0, got {sols[0].cost}"


# ── remove_arcs / restore_arcs bulk tests ────────────────────────────────────


def test_remove_arcs_list():
    """remove_arcs(list) removes exactly the requested arcs."""
    rg = make_diamond()

    removed = rg.remove_arcs([0, 2])

    assert sorted(removed) == [0, 2]
    assert rg.get_arc(0) is None
    assert rg.get_arc(2) is None
    assert rg.get_arc(1) is not None
    assert rg.number_of_arcs() == 1


def test_remove_arcs_numpy():
    """remove_arcs(np.array) produces the same result as the list overload."""
    rg = make_diamond()

    removed = rg.remove_arcs(np.array([0, 2], dtype=np.intp))

    assert sorted(removed) == [0, 2]
    assert rg.number_of_arcs() == 1


def test_remove_arcs_skips_missing():
    """remove_arcs silently ignores ids not present in the graph."""
    rg = make_diamond()

    removed = rg.remove_arcs([0, 99])

    assert removed == [0]
    assert rg.number_of_arcs() == 2


def test_restore_arcs_list():
    """restore_arcs(list) restores exactly the requested arcs."""
    rg = make_diamond()
    rg.remove_arcs([0, 2])

    restored = rg.restore_arcs([0, 2])

    assert sorted(restored) == [0, 2]
    assert rg.number_of_arcs() == 3


def test_restore_arcs_numpy():
    """restore_arcs(np.array) produces the same result as the list overload."""
    rg = make_diamond()
    rg.remove_arcs([0, 2])

    restored = rg.restore_arcs(np.array([0, 2], dtype=np.intp))

    assert sorted(restored) == [0, 2]
    assert rg.number_of_arcs() == 3


def test_restore_arcs_skips_missing():
    """restore_arcs silently ignores ids not in the removed-arc pool."""
    rg = make_diamond()
    rg.remove_arcs([1])

    restored = rg.restore_arcs([1, 99])

    assert restored == [1]


def test_remove_restore_arcs_roundtrip():
    """Bulk remove then bulk restore leaves the graph identical."""
    rg = make_diamond()
    ids = [0, 1, 2]

    rg.remove_arcs(ids)
    assert rg.number_of_arcs() == 0

    rg.restore_arcs(ids)
    assert rg.number_of_arcs() == 3
    for arc_id in ids:
        assert rg.get_arc(arc_id) is not None


def test_update_reduced_costs_numpy_matches_list():
    """Numpy array and plain list produce identical reduced costs."""
    rg_np = _make_rg_with_rows()
    rg_list = _make_rg_with_rows()
    duals_list = [5.0, 3.0]
    duals_np = np.array(duals_list)

    rg_np.update_reduced_costs(duals_np)
    rg_list.update_reduced_costs(duals_list)

    sols_np = rg_np.solve(preprocess=False)
    sols_list = rg_list.solve(preprocess=False)

    assert len(sols_np) >= 1 and len(sols_list) >= 1
    assert math.isclose(sols_np[0].cost, sols_list[0].cost, abs_tol=1e-9)


# ── add_arc predicted-id tests ────────────────────────────────────────────────


def test_add_arc_returns_sequential_ids():
    """add_arc returns 0, 1, 2, … in insertion order."""
    rg = make_resource_graph()
    rg.add_node(0, source=True)
    rg.add_node(1)
    rg.add_node(2, sink=True)

    id0 = rg.add_arc(1.0, 0, 1, cost=1.0)
    id1 = rg.add_arc(5.0, 0, 2, cost=5.0)
    id2 = rg.add_arc(3.0, 1, 2, cost=3.0)

    assert id0 == 0
    assert id1 == 1
    assert id2 == 2


def test_add_arc_predicted_id_matches_get_arc():
    """The id returned by add_arc is the id under which the arc is stored after
    flush."""
    rg = make_resource_graph()
    rg.add_node(0, source=True)
    rg.add_node(1, sink=True)

    arc_id = rg.add_arc(2.5, 0, 1, cost=2.5)
    rg.update()

    arc = rg.get_arc(arc_id)
    assert arc is not None
    assert arc.id == arc_id
    assert math.isclose(arc.cost, 2.5)


def test_add_arc_ids_continue_across_flushes():
    """IDs are monotonically increasing even when arcs are flushed in batches."""
    rg = make_resource_graph()
    rg.add_node(0, source=True)
    rg.add_node(1)
    rg.add_node(2, sink=True)

    id0 = rg.add_arc(1.0, 0, 1, cost=1.0)
    rg.update()  # flush first arc

    id1 = rg.add_arc(2.0, 1, 2, cost=2.0)
    rg.update()  # flush second arc

    assert id0 == 0
    assert id1 == 1
    assert rg.get_arc(id0) is not None
    assert rg.get_arc(id1) is not None


def test_add_arc_id_survives_remove_restore():
    """IDs continue incrementing after a remove/restore cycle; the slot is reused."""
    rg = make_resource_graph()
    rg.add_node(0, source=True)
    rg.add_node(1)
    rg.add_node(2, sink=True)

    id0 = rg.add_arc(1.0, 0, 1, cost=1.0)
    id1 = rg.add_arc(5.0, 0, 2, cost=5.0)
    rg.update()

    rg.remove_arc(id0)
    rg.restore_arc(id0)

    id2 = rg.add_arc(3.0, 1, 2, cost=3.0)
    rg.update()

    assert id2 == 2  # next id after 0 and 1
    assert rg.get_arc(id0) is not None
    assert rg.get_arc(id1) is not None
    assert rg.get_arc(id2) is not None
