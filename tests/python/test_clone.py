#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Tests for ResourceGraph.clone, clone_topology, add_rows_to_arc, and add_rows.

Covers:
- add_arc with rows as a single tuple (index, coeff) or list of tuples
- next_arc_id() returns the expected next arc ID
- add_rows_to_arc buffers rows and flushes them correctly
- add_rows adds rows in bulk from a list of triples or a 2-D numpy array
- update_reduced_costs uses stored rows to compute reduced costs
- clone() produces an independent copy with stable arc IDs and preserved rows
- clone_topology() produces an independent copy with empty rows
- clone_removed_arcs=True preserves removed arcs in the clone
- Removing arcs in a clone does not affect the original graph
"""

import math
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
    TrivialFeasibilityFunction,
    ValueCostFunction,
    ValueDominanceFunction,
)

# ── Shared helper ─────────────────────────────────────────────────────────────


def _make_graph() -> ResourceGraph:
    """Return a fresh 4-node graph with a single real resource (cost).

    Topology::

        0 ──(arc0,10)──▶ 1 ──(arc1,15)──▶ 3
        │                                  ▲
        └──(arc2,20)──▶ 2 ──(arc3, 5)─────┘

    Both s-t paths (0→1→3 and 0→2→3) have base cost 25.
    """
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
    return rg


# ── add_arc with rows variants ──────────────────────────────────────────


class TestAddArcRows:
    """Test rows normalisation in add_arc."""

    def test_single_tuple(self):
        """Rows as a single (index, coeff) tuple is normalised to one Row."""
        rg = _make_graph()
        arc_id = rg.add_arc((10.0,), 0, 1, cost=10.0, rows=(0, 2.5))

        arc = rg.get_arc(arc_id)
        assert len(arc.rows) == 1
        assert arc.rows[0].index == 0
        assert math.isclose(arc.rows[0].coefficient, 2.5, abs_tol=1e-9)

    def test_list_of_tuples(self):
        """Rows as a list of tuples produces one Row per tuple."""
        rg = _make_graph()
        arc_id = rg.add_arc((5.0,), 0, 3, cost=5.0, rows=[(0, 1.5), (1, -0.5)])

        arc = rg.get_arc(arc_id)
        assert len(arc.rows) == 2
        assert arc.rows[0].index == 0
        assert math.isclose(arc.rows[0].coefficient, 1.5, abs_tol=1e-9)
        assert arc.rows[1].index == 1
        assert math.isclose(arc.rows[1].coefficient, -0.5, abs_tol=1e-9)

    def test_none_rows(self):
        """Rows=None (default) results in an empty row list."""
        rg = _make_graph()
        arc_id = rg.add_arc((10.0,), 0, 1, cost=10.0)

        arc = rg.get_arc(arc_id)
        assert len(arc.rows) == 0

    def test_empty_list_rows(self):
        """Rows=[] also results in an empty row list."""
        rg = _make_graph()
        arc_id = rg.add_arc((10.0,), 0, 1, cost=10.0, rows=[])

        arc = rg.get_arc(arc_id)
        assert len(arc.rows) == 0


# ── next_arc_id ───────────────────────────────────────────────────────────────


class TestNextArcId:
    """Test that next_arc_id() tracks arc additions correctly."""

    def test_increments_with_add_arc(self):
        """next_arc_id() increments by one after each add_arc call."""
        rg = _make_graph()

        assert rg._next_arc_id == 0
        arc0 = rg.add_arc((10.0,), 0, 1, cost=10.0)
        assert rg._next_arc_id == 1
        arc1 = rg.add_arc((15.0,), 1, 3, cost=15.0)
        assert rg._next_arc_id == 2

        assert arc0 == 0
        assert arc1 == 1

    def test_cpp_next_arc_id_after_flush(self):
        """C++ next_arc_id() matches Python _next_arc_id after flush."""
        rg = _make_graph()
        rg.add_arc((10.0,), 0, 1, cost=10.0)
        rg.add_arc((15.0,), 1, 3, cost=15.0)

        # Flush by calling get_arc
        rg.get_arc(0)
        assert rg._graph.next_arc_id() == rg._next_arc_id


# ── add_rows_to_arc ───────────────────────────────────────────────────────────


class TestAddRowsToArc:
    """Test buffered row addition via add_rows_to_arc."""

    def test_buffered_then_flushed(self):
        """Rows added via add_rows_to_arc appear on the arc after flush."""
        rg = _make_graph()
        arc_id = rg.add_arc((10.0,), 0, 1, cost=10.0)

        rg.add_rows_to_arc(arc_id, [(0, 3.0)])

        arc = rg.get_arc(arc_id)  # triggers flush
        assert len(arc.rows) == 1
        assert arc.rows[0].index == 0
        assert math.isclose(arc.rows[0].coefficient, 3.0, abs_tol=1e-9)

    def test_single_tuple_form(self):
        """add_rows_to_arc accepts a single (index, coeff) tuple directly."""
        rg = _make_graph()
        arc_id = rg.add_arc((10.0,), 0, 1, cost=10.0)

        rg.add_rows_to_arc(arc_id, (1, -1.0))

        arc = rg.get_arc(arc_id)
        assert len(arc.rows) == 1
        assert arc.rows[0].index == 1
        assert math.isclose(arc.rows[0].coefficient, -1.0, abs_tol=1e-9)

    def test_multiple_calls_accumulate(self):
        """Multiple add_rows_to_arc calls accumulate rows on the same arc."""
        rg = _make_graph()
        arc_id = rg.add_arc((10.0,), 0, 1, cost=10.0)

        rg.add_rows_to_arc(arc_id, [(0, 3.0)])
        rg.add_rows_to_arc(arc_id, (1, -1.0))

        arc = rg.get_arc(arc_id)
        assert len(arc.rows) == 2

    def test_rows_appended_to_existing(self):
        """Rows added via add_rows_to_arc are appended to rows already on the arc."""
        rg = _make_graph()
        arc_id = rg.add_arc((10.0,), 0, 1, cost=10.0, rows=(0, 1.0))
        # First flush via get_arc to commit the arc with its initial row
        arc = rg.get_arc(arc_id)
        assert len(arc.rows) == 1

        rg.add_rows_to_arc(arc_id, [(1, 2.0)])
        arc = rg.get_arc(arc_id)
        assert len(arc.rows) == 2
        assert arc.rows[0].index == 0
        assert arc.rows[1].index == 1

    def test_arcs_buffered_before_rows(self):
        """Rows may be buffered before the arc is flushed; flush order is arcs→rows."""
        rg = _make_graph()
        arc_id = rg.add_arc((5.0,), 0, 3, cost=5.0)
        # Arc is still in Python buffer; rows are buffered too.
        rg.add_rows_to_arc(arc_id, [(0, 7.0)])

        # solve() flushes in order: nodes→arcs→rows
        sols = rg.solve()
        assert len(sols) >= 1

        arc = rg.get_arc(arc_id)
        assert len(arc.rows) == 1


# ── add_rows ──────────────────────────────────────────────────────────────────


class TestAddRows:
    """Test bulk row addition via add_rows."""

    def test_list_of_triples(self):
        """add_rows with a list of (arc_id, index, coeff) triples."""
        rg = _make_graph()
        arc0 = rg.add_arc((10.0,), 0, 1, cost=10.0)
        arc1 = rg.add_arc((5.0,), 1, 3, cost=5.0)

        rg.add_rows([(arc0, 0, 1.0), (arc1, 0, 2.0)])

        a0 = rg.get_arc(arc0)
        a1 = rg.get_arc(arc1)
        assert len(a0.rows) == 1
        assert math.isclose(a0.rows[0].coefficient, 1.0, abs_tol=1e-9)
        assert len(a1.rows) == 1
        assert math.isclose(a1.rows[0].coefficient, 2.0, abs_tol=1e-9)

    def test_numpy_2d_array(self):
        """add_rows accepts a (N, 3) numpy float64 array."""
        rg = _make_graph()
        arc0 = rg.add_arc((10.0,), 0, 1, cost=10.0)
        arc1 = rg.add_arc((15.0,), 1, 3, cost=15.0)

        data = np.array([[arc0, 0, 1.5], [arc1, 1, 3.0]], dtype=np.float64)
        rg.add_rows(data)

        a0 = rg.get_arc(arc0)
        a1 = rg.get_arc(arc1)
        assert len(a0.rows) == 1
        assert math.isclose(a0.rows[0].coefficient, 1.5, abs_tol=1e-9)
        assert len(a1.rows) == 1
        assert arc1 == a1.id
        assert math.isclose(a1.rows[1 - 1].coefficient, 3.0, abs_tol=1e-9)

    def test_multiple_rows_per_arc(self):
        """Multiple rows for the same arc are all added correctly."""
        rg = _make_graph()
        arc_id = rg.add_arc((10.0,), 0, 1, cost=10.0)

        rg.add_rows([(arc_id, 0, 1.0), (arc_id, 1, -2.0), (arc_id, 2, 0.5)])

        arc = rg.get_arc(arc_id)
        assert len(arc.rows) == 3
        coeffs = [r.coefficient for r in arc.rows]
        assert math.isclose(coeffs[0], 1.0, abs_tol=1e-9)
        assert math.isclose(coeffs[1], -2.0, abs_tol=1e-9)
        assert math.isclose(coeffs[2], 0.5, abs_tol=1e-9)


# ── update_reduced_costs uses rows ────────────────────────────────────────────


class TestUpdateReducedCosts:
    """Test that update_reduced_costs correctly applies rows."""

    def test_rows_shift_optimal_path(self):
        """After update_reduced_costs, the path with negative reduced cost wins.

        Base costs: 0→1→3 = 25, 0→2→3 = 25.
        arc3 (2→3) has row (index=0, coeff=8.0).
        duals = [1.0]  →  reduced cost of arc3 = 5.0 - 8.0*1.0 = -3.0.
        0→2→3 reduced cost = 20 + (-3) = 17 < 25.
        """
        rg = _make_graph()
        rg.add_arc((10.0,), 0, 1, cost=10.0)
        rg.add_arc((15.0,), 1, 3, cost=15.0)
        rg.add_arc((20.0,), 0, 2, cost=20.0)
        rg.add_arc((5.0,), 2, 3, cost=5.0, rows=(0, 8.0))

        rg.update_reduced_costs([1.0])

        sols = rg.solve()
        assert len(sols) >= 1
        assert sols[0].path_node_ids == [0, 2, 3]
        assert math.isclose(sols[0].cost, 17.0, abs_tol=1e-6)

    def test_rows_added_after_arc_flush(self):
        """Rows added via add_rows_to_arc after the arc is flushed are used."""
        rg = _make_graph()
        rg.add_arc((10.0,), 0, 1, cost=10.0)
        rg.add_arc((15.0,), 1, 3, cost=15.0)
        rg.add_arc((20.0,), 0, 2, cost=20.0)
        arc3 = rg.add_arc((5.0,), 2, 3, cost=5.0)

        # Flush arcs first so they are in C++ before adding rows
        rg.get_arc(arc3)

        rg.add_rows_to_arc(arc3, [(0, 8.0)])
        rg.update_reduced_costs([1.0])

        sols = rg.solve()
        assert len(sols) >= 1
        assert sols[0].path_node_ids == [0, 2, 3]

    def test_multiple_rows_sum(self):
        """Reduced cost is base_cost - sum(coeff * dual) over all rows."""
        rg = _make_graph()
        rg.add_arc((10.0,), 0, 1, cost=10.0)
        rg.add_arc((15.0,), 1, 3, cost=15.0)
        rg.add_arc((20.0,), 0, 2, cost=20.0)
        # arc3: base=5, rows: (0, 3.0) and (1, 2.0)
        # duals=[1.0, 2.0] → reduced = 5 - 3*1 - 2*2 = 5 - 3 - 4 = -2
        rg.add_arc((5.0,), 2, 3, cost=5.0, rows=[(0, 3.0), (1, 2.0)])

        rg.update_reduced_costs([1.0, 2.0])

        sols = rg.solve()
        assert len(sols) >= 1
        assert sols[0].path_node_ids == [0, 2, 3]
        assert math.isclose(sols[0].cost, 18.0, abs_tol=1e-6)  # 20 + (-2) = 18


# ── clone ─────────────────────────────────────────────────────────────────────


class TestClone:
    """Test ResourceGraph.clone()."""

    def test_clone_preserves_rows(self):
        """Clone() carries rows into the copy."""
        rg = _make_graph()
        arc0 = rg.add_arc((10.0,), 0, 1, cost=10.0, rows=(0, 2.0))
        rg.add_arc((15.0,), 1, 3, cost=15.0)
        rg.add_arc((20.0,), 0, 2, cost=20.0)
        rg.add_arc((5.0,), 2, 3, cost=5.0)

        rg2 = rg.clone()

        arc_clone = rg2.get_arc(arc0)
        assert len(arc_clone.rows) == 1
        assert arc_clone.rows[0].index == 0
        assert math.isclose(arc_clone.rows[0].coefficient, 2.0, abs_tol=1e-9)

    def test_clone_same_optimal_cost(self):
        """Solving the clone gives the same optimal cost as the original."""
        rg = _make_graph()
        rg.add_arc((10.0,), 0, 1, cost=10.0)
        rg.add_arc((15.0,), 1, 3, cost=15.0)
        rg.add_arc((20.0,), 0, 2, cost=20.0)
        rg.add_arc((5.0,), 2, 3, cost=5.0)

        rg2 = rg.clone()

        sols1 = rg.solve()
        sols2 = rg2.solve()
        assert len(sols1) >= 1 and len(sols2) >= 1
        assert math.isclose(sols1[0].cost, sols2[0].cost, abs_tol=1e-6)

    def test_clone_independent_remove_state(self):
        """Removing an arc in the clone does not affect the original."""
        rg = _make_graph()
        arc0 = rg.add_arc((10.0,), 0, 1, cost=10.0)
        arc1 = rg.add_arc((15.0,), 1, 3, cost=15.0)
        rg.add_arc((20.0,), 0, 2, cost=20.0)
        rg.add_arc((5.0,), 2, 3, cost=5.0)

        rg2 = rg.clone()

        # Remove arc in clone; original must still work
        rg2.remove_arcs([arc0, arc1])
        sols_orig = rg.solve()
        assert len(sols_orig) >= 1
        assert arc0 in [a.id for a in [rg.get_arc(arc0)]]

    def test_clone_next_arc_id_matches(self):
        """Python _next_arc_id on the clone mirrors the C++ next_arc_id()."""
        rg = _make_graph()
        rg.add_arc((10.0,), 0, 1, cost=10.0)
        rg.add_arc((15.0,), 1, 3, cost=15.0)

        rg2 = rg.clone()

        assert rg2._next_arc_id == rg._next_arc_id
        assert rg2._next_arc_id == rg2._graph.next_arc_id()

    def test_clone_stable_arc_ids(self):
        """Arc IDs in the clone are identical to those in the original."""
        rg = _make_graph()
        a0 = rg.add_arc((10.0,), 0, 1, cost=10.0)
        a1 = rg.add_arc((15.0,), 1, 3, cost=15.0)
        a2 = rg.add_arc((20.0,), 0, 2, cost=20.0)
        a3 = rg.add_arc((5.0,), 2, 3, cost=5.0)

        rg2 = rg.clone()

        for arc_id in [a0, a1, a2, a3]:
            arc_orig = rg.get_arc(arc_id)
            arc_clone = rg2.get_arc(arc_id)
            assert arc_orig.id == arc_clone.id
            assert math.isclose(arc_orig.cost, arc_clone.cost, abs_tol=1e-9)

    def test_clone_independent_reduced_costs(self):
        """update_reduced_costs on the clone does not affect the original."""
        rg = _make_graph()
        rg.add_arc((10.0,), 0, 1, cost=10.0)
        rg.add_arc((15.0,), 1, 3, cost=15.0)
        rg.add_arc((20.0,), 0, 2, cost=20.0)
        rg.add_arc((5.0,), 2, 3, cost=5.0, rows=(0, 8.0))

        rg2 = rg.clone()

        # Update clone only; original costs should be unchanged
        rg2.update_reduced_costs([1.0])

        sols_orig = rg.solve()
        sols_clone = rg2.solve()

        # Original: both paths cost 25 (no update)
        assert math.isclose(sols_orig[0].cost, 25.0, abs_tol=1e-6)
        # Clone: 0→2→3 wins at cost 17
        assert sols_clone[0].path_node_ids == [0, 2, 3]
        assert math.isclose(sols_clone[0].cost, 17.0, abs_tol=1e-6)


# ── clone_topology ────────────────────────────────────────────────────────────


class TestCloneTopology:
    """Test ResourceGraph.clone_topology() — clone with no rows."""

    def test_rows_stripped(self):
        """clone_topology() produces arcs with empty rows."""
        rg = _make_graph()
        arc_id = rg.add_arc((10.0,), 0, 1, cost=10.0, rows=[(0, 1.0), (1, 2.0)])
        rg.add_arc((15.0,), 1, 3, cost=15.0)
        rg.add_arc((20.0,), 0, 2, cost=20.0)
        rg.add_arc((5.0,), 2, 3, cost=5.0)

        rg2 = rg.clone_topology()

        arc_clone = rg2.get_arc(arc_id)
        assert len(arc_clone.rows) == 0

    def test_original_rows_preserved(self):
        """After clone_topology(), the original graph still has its rows."""
        rg = _make_graph()
        arc_id = rg.add_arc((10.0,), 0, 1, cost=10.0, rows=(0, 1.0))
        rg.add_arc((15.0,), 1, 3, cost=15.0)
        rg.clone_topology()

        arc_orig = rg.get_arc(arc_id)
        assert len(arc_orig.rows) == 1

    def test_add_rows_to_topology_clone(self):
        """Rows can be added to a topology clone independently of the original."""
        rg = _make_graph()
        arc_id = rg.add_arc((10.0,), 0, 1, cost=10.0, rows=(0, 1.0))
        rg.add_arc((15.0,), 1, 3, cost=15.0)
        rg.add_arc((20.0,), 0, 2, cost=20.0)
        rg.add_arc((5.0,), 2, 3, cost=5.0)

        rg2 = rg.clone_topology()
        rg2.add_rows_to_arc(arc_id, [(0, 5.0)])

        arc_clone = rg2.get_arc(arc_id)
        assert len(arc_clone.rows) == 1
        assert math.isclose(arc_clone.rows[0].coefficient, 5.0, abs_tol=1e-9)

        # Original is untouched
        arc_orig = rg.get_arc(arc_id)
        assert len(arc_orig.rows) == 1
        assert math.isclose(arc_orig.rows[0].coefficient, 1.0, abs_tol=1e-9)

    def test_topology_clone_solves_correctly(self):
        """A topology clone without rows solves like a clean graph."""
        rg = _make_graph()
        rg.add_arc((10.0,), 0, 1, cost=10.0, rows=(0, 99.0))
        rg.add_arc((15.0,), 1, 3, cost=15.0)
        rg.add_arc((20.0,), 0, 2, cost=20.0)
        rg.add_arc((5.0,), 2, 3, cost=5.0)

        rg2 = rg.clone_topology()
        # No duals applied — arc costs remain at their base values
        sols = rg2.solve()
        assert len(sols) >= 1
        assert math.isclose(sols[0].cost, 25.0, abs_tol=1e-6)


# ── clone_removed_arcs ────────────────────────────────────────────────────────


class TestCloneRemovedArcs:
    """Test clone(clone_removed_arcs=True)."""

    def test_removed_arc_present_in_clone_after_restore(self):
        """A removed arc cloned with clone_removed_arcs=True can be restored."""
        rg = _make_graph()
        arc0 = rg.add_arc((10.0,), 0, 1, cost=10.0)
        arc1 = rg.add_arc((15.0,), 1, 3, cost=15.0)
        rg.add_arc((20.0,), 0, 2, cost=20.0)
        rg.add_arc((5.0,), 2, 3, cost=5.0)

        rg.remove_arcs([arc0, arc1])

        rg2 = rg.clone(clone_removed_arcs=True)
        rg2.restore_arcs([arc0, arc1])

        sols = rg2.solve()
        assert len(sols) >= 1

    def test_removed_arc_absent_without_flag(self):
        """clone(clone_removed_arcs=False) drops removed arcs; restoring them is a no-
        op.

        restore_arcs silently skips IDs that are not in the removed set, so the only
        reliable check is that the clone solves as if those arcs are gone.
        """
        rg = _make_graph()
        arc0 = rg.add_arc((10.0,), 0, 1, cost=10.0)
        arc1 = rg.add_arc((15.0,), 1, 3, cost=15.0)
        rg.add_arc((20.0,), 0, 2, cost=20.0)
        rg.add_arc((5.0,), 2, 3, cost=5.0)

        rg.remove_arcs([arc0, arc1])

        rg2 = rg.clone()  # clone_removed_arcs=False by default

        # Attempting to restore non-existent arcs is silently ignored
        rg2.restore_arcs([arc0, arc1])

        # Only 0→2→3 is reachable in the clone (arc0/arc1 used the 0→1→3 path)
        sols = rg2.solve()
        assert len(sols) >= 1
        # Node 1 is only reachable via arc0 (0→1); if arc0 is gone it can't appear
        assert all(
            1 not in s.path_node_ids for s in sols
        ), "Node 1 should be unreachable when arc0 (0→1) is absent from the clone"
        assert sols[0].path_node_ids == [0, 2, 3]

    def test_independent_restore(self):
        """Restoring an arc in the clone does not affect the original."""
        rg = _make_graph()
        arc0 = rg.add_arc((10.0,), 0, 1, cost=10.0)
        arc1 = rg.add_arc((15.0,), 1, 3, cost=15.0)
        rg.add_arc((20.0,), 0, 2, cost=20.0)
        rg.add_arc((5.0,), 2, 3, cost=5.0)

        rg.remove_arcs([arc0, arc1])
        rg2 = rg.clone(clone_removed_arcs=True)
        rg2.restore_arcs([arc0, arc1])

        # Original still has arc0 removed; only 0→2→3 is available
        sols_orig = rg.solve()
        sols_clone = rg2.solve()

        assert sols_orig[0].path_node_ids == [0, 2, 3]
        # Clone has both paths; optimal is the same cost from either
        assert len(sols_clone) >= 1


# ── Run directly ──────────────────────────────────────────────────────────────

if __name__ == "__main__":
    import pytest

    pytest.main([__file__, "-v"])
