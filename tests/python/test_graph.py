#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

import math
import os
import sys

sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "python", "src")
)

import pytest  # noqa: E402

np = pytest.importorskip("numpy")

import networkx as nx  # noqa: E402

from rcspp.graph import BucketAlgorithmParams, ResourceGraph  # noqa: E402
from rcspp.resource import (  # noqa: E402
    AdditionExtensionFunction,
    InclusionDominanceFunction,
    MinMaxFeasibilityFunction,
    TrivialCostFunction,
    TrivialFeasibilityFunction,
    UnionExtensionFunction,
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


# ── Arc.rows binding (P-1: live references kept alive by the arc) ──────────────


def test_arc_rows_are_live_references():
    """Arc.rows exposes the arc's row vector as live references — kept alive by the parent arc
    (return_value_policy::reference_internal), not as a frozen copy. Reads are correct and element
    mutation writes back into the arc's C++ vector. Guards against the gratuitous
    `return_value_policy::reference` override (no keep-alive) that P-1 removed.
    """
    rg = _make_rg_with_rows()  # arc 0 carries Row(index=0, coefficient=1.0)

    arc = rg.get_arc(0)
    rows = arc.rows
    assert len(rows) == 1
    assert rows[0].index == 0
    assert abs(rows[0].coefficient - 1.0) < 1e-12

    # Mutating through the returned reference writes back into the arc's vector (reference, not a
    # copy); re-reading via a fresh get_arc/rows observes the change.
    rows[0].coefficient = 9.0
    assert abs(rg.get_arc(0).rows[0].coefficient - 9.0) < 1e-12


# ── _parse_rg_class ───────────────────────────────────────────────────────────


def test_parse_rg_class_unknown_cpp_type():
    """_parse_rg_class returns None when a component is not in the Python registry."""
    from rcspp.graph import _parse_rg_class

    assert _parse_rg_class("_unknown_cpptype_resource_graph") is None


def test_parse_rg_class_non_matching_name():
    """_parse_rg_class returns None for names that don't fit the pattern."""
    from rcspp.graph import _parse_rg_class

    assert _parse_rg_class("not_a_resource_graph") is None
    assert _parse_rg_class("_") is None


# ── _ensure_graph error paths ─────────────────────────────────────────────────


def test_ensure_graph_no_resources_raises():
    """Update() on a graph with no resources registered raises ValueError."""
    rg = ResourceGraph()
    rg.add_node(0, source=True)
    rg.add_node(1, sink=True)
    with pytest.raises(ValueError, match="At least one resource"):
        rg.update()


def test_ensure_graph_first_resource_not_real_raises():
    """add_int_resource as the very first call raises ValueError."""
    rg = ResourceGraph()
    with pytest.raises(ValueError, match="first registered resource"):
        rg.add_int_resource(
            AdditionExtensionFunction(),
            TrivialFeasibilityFunction(),
            TrivialCostFunction(),
            ValueDominanceFunction(),
        )


def test_add_resource_after_graph_initialized_raises():
    """add_real_resource after the graph has been flushed raises RuntimeError."""
    rg = make_resource_graph()
    rg.add_node(0, source=True)
    rg.add_node(1, sink=True)
    rg.update()
    with pytest.raises(RuntimeError, match="Cannot call add_real_resource after"):
        rg.add_real_resource(
            AdditionExtensionFunction(),
            MinMaxFeasibilityFunction(0.0, 10.0),
            ValueCostFunction(),
            ValueDominanceFunction(),
        )


def test_ensure_graph_superset_lookup():
    """Registering real+int+bitset triggers the superset C++ class search."""
    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        TrivialCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_int_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        TrivialCostFunction(),
        ValueDominanceFunction(),
    )
    rg.add_bitset_resource(
        UnionExtensionFunction(),
        TrivialFeasibilityFunction(),
        TrivialCostFunction(),
        InclusionDominanceFunction(),
    )
    # No direct class for (real, int, bitset); a superset class is selected.
    rg.update()
    assert "bitset" in rg._graph_canonical
    assert "int_set" in rg._graph_canonical


# ── _resolve with concrete C++ object ────────────────────────────────────────


def test_resolve_with_concrete_cpp_object():
    """Passing a typed C++ function directly (not a generic descriptor) is accepted."""
    from rcspp.resource import (
        AdditionExtensionFunction_real,
        ValueCostFunction_real,
        ValueDominanceFunction_real,
    )

    rg = ResourceGraph()
    rg.add_real_resource(
        AdditionExtensionFunction_real(),
        MinMaxFeasibilityFunction(0.0, 100.0),
        ValueCostFunction_real(),
        ValueDominanceFunction_real(),
    )
    rg.add_node(0, source=True)
    rg.add_node(1, sink=True)
    rg.add_arc(1.0, 0, 1, cost=1.0)
    rg.update()
    assert rg.number_of_arcs() == 1


# ── reserve() paths ───────────────────────────────────────────────────────────


def test_reserve_before_flush():
    """Reserve() before any flush stores a hint applied at flush time."""
    rg = make_resource_graph()
    rg.reserve(10, 20)
    rg.add_node(0, source=True)
    rg.add_node(1, sink=True)
    rg.add_arc(1.0, 0, 1, cost=1.0)
    rg.update()
    assert rg.number_of_arcs() == 1


def test_reserve_after_flush():
    """Reserve() after flush forwards directly to the C++ graph."""
    rg = make_diamond()
    rg.reserve(10, 20)
    assert rg.number_of_arcs() == 3


# ── get_nodes_size / get_arcs_size ────────────────────────────────────────────


def test_get_nodes_size_and_arcs_size():
    """get_nodes_size and get_arcs_size include buffered items."""
    rg = make_resource_graph()
    rg.add_node(0, source=True)
    rg.add_node(1, sink=True)
    assert rg.get_nodes_size() == 2
    assert rg.get_arcs_size() == 0
    rg.add_arc(1.0, 0, 1, cost=1.0)
    assert rg.get_arcs_size() == 1
    rg.update()
    assert rg.get_nodes_size() == 2
    assert rg.get_arcs_size() == 1


# ── get_arcs ──────────────────────────────────────────────────────────────────


def test_get_arcs_between_nodes():
    """get_arcs returns all arcs between a pair of nodes."""
    rg = make_diamond()
    arcs = rg.get_arcs(0, 2)
    assert len(arcs) == 1
    assert arcs[0].id == 1


# ── update_arc ────────────────────────────────────────────────────────────────


def test_update_arc():
    """update_arc modifies an arc's resource consumption in place."""
    rg = make_diamond()
    arc = rg.get_arc(0)
    rg.update_arc(arc, 2.0)
    updated = rg.get_arc(0)
    assert updated is not None


# ── add_rows_to_arc with Row objects ─────────────────────────────────────────


def test_add_rows_to_arc_with_row_objects():
    """add_rows_to_arc accepts Row objects (not just tuples)."""
    from rcspp.graph import Row

    rg = make_resource_graph()
    rg.add_node(0, source=True)
    rg.add_node(1, sink=True)
    rg.add_arc(1.0, 0, 1, cost=5.0)
    arc_id = 0
    rg.add_rows_to_arc(arc_id, [Row(0, 1.5)])
    rg.update()
    arc = rg.get_arc(arc_id)
    assert len(arc.rows) == 1
    assert arc.rows[0].index == 0
    assert abs(arc.rows[0].coefficient - 1.5) < 1e-12


def test_add_rows_to_arc_single_tuple():
    """add_rows_to_arc accepts a single (index, coeff) tuple."""
    rg = make_resource_graph()
    rg.add_node(0, source=True)
    rg.add_node(1, sink=True)
    rg.add_arc(1.0, 0, 1, cost=5.0)
    rg.add_rows_to_arc(0, (2, 3.0))
    rg.update()
    arc = rg.get_arc(0)
    assert len(arc.rows) == 1
    assert arc.rows[0].index == 2


# ── add_rows with numpy ───────────────────────────────────────────────────────


def test_add_rows_with_numpy_array():
    """add_rows accepts a 2-D numpy array (arc_id, row_index, coeff)."""
    rg = make_resource_graph()
    rg.add_node(0, source=True)
    rg.add_node(1, sink=True)
    rg.add_arc(1.0, 0, 1, cost=5.0)
    data = np.array([[0, 0, 1.0], [0, 1, 2.0]], dtype=np.float64)
    rg.add_rows(data)
    rg.update()
    arc = rg.get_arc(0)
    assert len(arc.rows) == 2


# ── sort_nodes ────────────────────────────────────────────────────────────────


def test_sort_nodes_default():
    """sort_nodes() without comparator sorts by ascending id."""
    rg = make_diamond()
    rg.sort_nodes()


def test_sort_nodes_with_comp():
    """sort_nodes(comp) with a custom comparator runs without error."""
    rg = make_diamond()
    rg.sort_nodes(lambda a, b: a.id < b.id)


# ── to_string / __str__ / __repr__ ───────────────────────────────────────────


def test_to_string_empty_graph():
    """to_string on an unflushed empty graph returns empty string."""
    rg = ResourceGraph()
    assert rg.to_string() == ""


def test_str_and_repr():
    """Str() and repr() both call to_string() and return a non-empty string."""
    rg = make_diamond()
    s = str(rg)
    assert isinstance(s, str)
    r = repr(rg)
    assert isinstance(r, str)
    assert s == r


# ── solve() error paths ───────────────────────────────────────────────────────


def test_solve_negative_cost_index_raises():
    """Solve() with cost_index < 0 raises ValueError."""
    rg = make_diamond()
    with pytest.raises(ValueError, match="cost_index must be non-negative"):
        rg.solve(cost_index=-1)


def test_solve_unknown_algorithm_raises():
    """Solve() with an unrecognised algorithm string raises ValueError."""
    rg = make_diamond()
    with pytest.raises(ValueError, match="Unknown algorithm"):
        rg.solve(algorithm="not_an_algo")


def test_solve_with_bucket_params():
    """Solve() accepts BucketAlgorithmParams; selects bucket-based algorithm."""
    rg = make_diamond()
    params = BucketAlgorithmParams(range_buckets=10)
    result = rg.solve(algorithm="pushing", params=params)
    assert len(result) >= 1


# ── update_reduced_costs error / branch paths ─────────────────────────────────


def test_update_reduced_costs_negative_cost_index_raises():
    """update_reduced_costs with cost_index < 0 raises ValueError."""
    rg = _make_rg_with_rows()
    with pytest.raises(ValueError, match="cost_index must be non-negative"):
        rg.update_reduced_costs(np.array([1.0]), cost_index=-1)


def test_update_reduced_costs_empty_dict():
    """update_reduced_costs with an empty dict is a no-op (early return)."""
    rg = _make_rg_with_rows()
    rg.update_reduced_costs({})
    sols = rg.solve(preprocess=False)
    assert len(sols) >= 1


def test_update_reduced_costs_dict_nonempty():
    """update_reduced_costs accepts a non-empty dict of duals."""
    rg = _make_rg_with_rows()
    rg.update_reduced_costs({0: 3.0, 1: 4.0})
    sols = rg.solve(preprocess=False)
    assert len(sols) >= 1
    assert math.isclose(sols[0].cost, 7.0 + 12.0, abs_tol=1e-6)


def test_update_reduced_costs_list():
    """update_reduced_costs accepts a plain Python list as duals."""
    rg = _make_rg_with_rows()
    rg.update_reduced_costs([3.0, 4.0])
    sols = rg.solve(preprocess=False)
    assert len(sols) >= 1
    assert math.isclose(sols[0].cost, 7.0 + 12.0, abs_tol=1e-6)


# ── from_networkx error paths ─────────────────────────────────────────────────


def test_from_networkx_missing_source_raises():
    """from_networkx raises if no source node is set."""
    G = nx.DiGraph()
    G.add_node(0)
    G.add_node(1, sink=True)
    rg = make_resource_graph()
    with pytest.raises(ValueError, match="no source node"):
        rg.from_networkx(G)


def test_from_networkx_missing_sink_raises():
    """from_networkx raises if no sink node is set."""
    G = nx.DiGraph()
    G.add_node(0, source=True)
    G.add_node(1)
    rg = make_resource_graph()
    with pytest.raises(ValueError, match="no sink node"):
        rg.from_networkx(G)


def test_from_networkx_arc_missing_resource_when_registered_raises():
    """from_networkx with resources registered raises when arc lacks 'resource'."""
    G = nx.DiGraph()
    G.add_node(0, source=True)
    G.add_node(1, sink=True)
    G.add_edge(0, 1)  # no 'resource' attribute
    rg = make_resource_graph()
    with pytest.raises(ValueError, match="missing a 'resource' attribute"):
        rg.from_networkx(G)


def test_from_networkx_many_arcs_missing_resource_truncated_message():
    """from_networkx error message truncates after 5 missing arcs."""
    G = nx.DiGraph()
    G.add_node(0, source=True)
    for i in range(1, 8):
        G.add_node(i)
    G.add_node(8, sink=True)
    for i in range(1, 8):
        G.add_edge(0, i)  # no 'resource' on any arc
    G.add_edge(7, 8)
    rg = make_resource_graph()
    with pytest.raises(ValueError, match=r"more\)"):
        rg.from_networkx(G)


def test_from_networkx_arc_missing_resource_no_resources_registered_raises():
    """from_networkx raises on arc missing 'resource' even with no resources
    registered."""
    G = nx.DiGraph()
    G.add_node(0, source=True)
    G.add_node(1, sink=True)
    G.add_edge(0, 1)  # no 'resource' attribute, no resources registered either
    rg = ResourceGraph()
    with pytest.raises(ValueError, match="missing 'resource' attribute"):
        rg.from_networkx(G)


# ── BucketAlgorithmParams ─────────────────────────────────────────────────────


def test_bucket_params_init_with_positions():
    """BucketAlgorithmParams stores positions and forwards kwargs to C++."""
    params = BucketAlgorithmParams(range_buckets=50, bucket_resource_pos=0, sort_resource_pos=0)
    assert params._bucket_resource_pos == 0
    assert params._sort_resource_pos == 0


def test_bucket_params_getattr():
    """BucketAlgorithmParams.__getattr__ forwards non-underscore names to C++."""
    params = BucketAlgorithmParams(range_buckets=50)
    assert params.range_buckets == 50


def test_bucket_params_getattr_private_raises():
    """BucketAlgorithmParams.__getattr__ raises AttributeError for _ names."""
    params = BucketAlgorithmParams()
    with pytest.raises(AttributeError):
        _ = params._nonexistent_private_attr


def test_bucket_params_setattr_public():
    """BucketAlgorithmParams.__setattr__ forwards public names to C++."""
    params = BucketAlgorithmParams()
    params.range_buckets = 200
    assert params.range_buckets == 200


def test_bucket_params_to_cpp_with_positions():
    """_to_cpp resolves resource positions to C++ type/index."""
    rg = make_resource_graph()
    rg.add_node(0, source=True)
    rg.add_node(1, sink=True)
    rg.add_arc(1.0, 0, 1, cost=1.0)
    params = BucketAlgorithmParams(range_buckets=10, bucket_resource_pos=0, sort_resource_pos=0)
    result = rg.solve(algorithm="pushing", params=params)
    assert len(result) >= 1


def test_bucket_params_to_cpp_out_of_range_raises():
    """_to_cpp raises ValueError when position is out of range."""
    rg = make_resource_graph()
    rg.add_node(0, source=True)
    rg.add_node(1, sink=True)
    rg.add_arc(1.0, 0, 1, cost=1.0)
    params = BucketAlgorithmParams(range_buckets=10, bucket_resource_pos=99)
    with pytest.raises(ValueError, match="out of range"):
        rg.solve(params=params)


def test_bucket_params_kwargs_forwarded():
    """BucketAlgorithmParams kwargs are forwarded to the underlying C++ object."""
    params = BucketAlgorithmParams(range_buckets=50, stop_after_X_solutions=3)
    assert params.stop_after_X_solutions == 3


# ── ResourceGraph(nx_graph=...) constructor path ──────────────────────────────


def test_resource_graph_constructor_with_nx_graph():
    """ResourceGraph(nx_graph=G) invokes from_networkx; resources must be pre-added."""
    G = nx.DiGraph()
    G.add_node(0, source=True)
    G.add_node(1, sink=True)
    G.add_edge(0, 1, resource=(1.0,), cost=2.0)
    # from_networkx calls update() internally, so no resources → expected ValueError.
    # Line 107 (self.from_networkx(nx_graph)) is reached before the error bubbles up.
    with pytest.raises(ValueError, match="At least one resource"):
        ResourceGraph(nx_graph=G)
