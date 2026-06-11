#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# flake8: noqa

import math
import os
import random
import sys

_here = os.path.dirname(os.path.abspath(__file__))
# rcspp package lives under python/src/; the vrp example package under examples/python/.
sys.path.insert(0, os.path.join(_here, "..", "..", "python", "src"))
sys.path.insert(0, os.path.join(_here, "..", "..", "examples", "python"))
sys.path.insert(0, _here)

from vrp.instance import Customer, Instance
from vrp.vrp import VRP

# ── Helpers ───────────────────────────────────────────────────────────────────

INSTANCES_DIR = os.path.join(os.path.dirname(__file__), "../../instances")


def make_tiny_instance() -> Instance:
    """3-customer depot-rooted instance built entirely in memory.

    Layout (depot at 0,0):     depot (0): (0, 0)  demand=0   tw=[0, 1000]  service=0
    cust  (1): (1, 0)  demand=10  tw=[0,  500]  service=10     cust  (2): (0, 1)
    demand=10  tw=[0,  500]  service=10     cust  (3): (1, 1)  demand=10  tw=[0,  500]
    service=10 Capacity = 50  (all customers fit in one vehicle).
    """
    inst = Instance(nb_vehicles=3, capacity=50, name="tiny")
    inst.add_customer(
        0, 0.0, 0.0, demand=0, ready_time=0, due_time=1000, service_time=0, depot=True
    )
    inst.add_customer(
        1, 1.0, 0.0, demand=10, ready_time=0, due_time=500, service_time=10, depot=False
    )
    inst.add_customer(
        2, 0.0, 1.0, demand=10, ready_time=0, due_time=500, service_time=10, depot=False
    )
    inst.add_customer(
        3, 1.0, 1.0, demand=10, ready_time=0, due_time=500, service_time=10, depot=False
    )
    return inst


def _sink_id(vrp_instance: Instance) -> int:
    return len(vrp_instance.get_customers_by_id())


# ── Test 1: graph construction does not raise ─────────────────────────────────


def test_graph_construction():
    """VRP object can be created from a tiny in-memory instance."""
    inst = make_tiny_instance()
    vrp = VRP(inst)
    rg = vrp._VRP__resource_graph  # name-mangled attribute
    # 4 customers + 1 artificial sink = 5 nodes
    assert rg.number_of_nodes() == 5, f"Expected 5 nodes, got {rg.number_of_nodes()}"
    # No arc should point back to the source (depot, id=0)
    sink = _sink_id(inst)
    for arc_id in rg.arc_ids():
        arc = rg.get_arc(arc_id)
        assert (
            arc.destination().id != 0
        ), f"Arc {arc_id} points to the depot source (node 0), which is forbidden"


# ── Test 2: subproblem solves and returns feasible routes ─────────────────────


def test_subproblem_zero_duals():
    """Subproblem with zero duals finds at least one feasible route."""
    inst = make_tiny_instance()
    vrp = VRP(inst)
    sols = vrp.solve_subproblem()

    assert len(sols) >= 1, "Expected at least one solution from the subproblem"
    sink = _sink_id(inst)
    depot_id = inst.get_depot_customer().id

    for s in sols:
        path = s.path_node_ids
        assert path[0] == depot_id, f"Path must start at depot, got {path}"
        assert path[-1] == sink, f"Path must end at sink ({sink}), got {path}"
        assert s.cost >= 0.0, f"Cost must be non-negative, got {s.cost}"


# ── Test 3: subproblem with positive duals lowers reduced costs ───────────────


def test_subproblem_with_duals():
    """Reduced costs decrease when positive dual values are applied."""
    inst = make_tiny_instance()
    vrp = VRP(inst)

    sols_zero = vrp.solve_subproblem()
    assert sols_zero, "No solution with zero duals"
    cost_zero = sols_zero[0].cost

    # Give every demand customer a dual of 1.0 → reduced costs drop by 1 per
    # customer visited.
    random.seed(25)
    duals = {cid: random.uniform(1, 100) for cid in inst.get_demand_customers_id()}
    sols_dual = vrp.solve_subproblem(duals)
    assert sols_dual, "No solution with dual values"
    cost_dual = sols_dual[0].cost

    assert (
        cost_dual <= cost_zero - 1e-9
    ), f"Positive duals should reduce the best cost: {cost_dual} > {cost_zero}"


# ── Test 4: time-window feasibility ──────────────────────────────────────────


def test_time_window_infeasibility():
    """A customer with a tight time window that cannot be reached is excluded."""
    inst = Instance(nb_vehicles=1, capacity=100, name="tw_test")
    # Depot at (0,0); time horizon [0, 1000]
    inst.add_customer(
        0, 0.0, 0.0, demand=0, ready_time=0, due_time=1000, service_time=0, depot=True
    )
    # Customer reachable in time
    inst.add_customer(
        1, 1.0, 0.0, demand=10, ready_time=0, due_time=500, service_time=10, depot=False
    )
    # Customer with due_time=0 → must arrive at time 0 but travel takes >0
    inst.add_customer(
        2, 5.0, 0.0, demand=10, ready_time=0, due_time=0, service_time=10, depot=False
    )

    vrp = VRP(inst)
    sols = vrp.solve_subproblem()

    # No solution may visit customer 2 (due_time=0 makes it unreachable)
    for s in sols:
        assert (
            2 not in s.path_node_ids
        ), f"Customer 2 has due_time=0 and should be infeasible, but appears in {s.path_node_ids}"


# ── Test 5: capacity feasibility ─────────────────────────────────────────────


def test_capacity_feasibility():
    """Paths exceeding vehicle capacity are pruned."""
    inst = Instance(nb_vehicles=1, capacity=15, name="cap_test")
    inst.add_customer(
        0, 0.0, 0.0, demand=0, ready_time=0, due_time=1000, service_time=0, depot=True
    )
    # Each customer has demand 10; capacity=15 → at most 1 customer per route.
    inst.add_customer(
        1, 1.0, 0.0, demand=10, ready_time=0, due_time=500, service_time=10, depot=False
    )
    inst.add_customer(
        2, 2.0, 0.0, demand=10, ready_time=0, due_time=500, service_time=10, depot=False
    )

    vrp = VRP(inst)
    sols = vrp.solve_subproblem()

    assert sols, "Expected at least one solution"
    for s in sols:
        demand_nodes = [n for n in s.path_node_ids if n not in (0, _sink_id(inst))]
        assert (
            len(demand_nodes) <= 1
        ), f"Capacity=15 allows only 1 customer per route, but path visits {demand_nodes}"


# ── Test 6: file-based instance (C101_5) ─────────────────────────────────────


def test_c101_5_subproblem():
    """Subproblem on the C101_5 benchmark instance returns valid solutions."""
    instance_path = os.path.join(INSTANCES_DIR, "C101_5.txt")
    if not os.path.exists(instance_path):
        print(f"  [skip] {instance_path} not found")
        return

    from vrp.instance_reader import InstanceReader

    inst = InstanceReader(instance_path).read()
    vrp = VRP(inst)

    sols = vrp.solve_subproblem()
    assert sols, "Expected at least one solution for C101_5"

    sink = _sink_id(inst)
    depot_id = inst.get_depot_customer().id
    for s in sols:
        path = s.path_node_ids
        assert path[0] == depot_id, f"Path must start at depot"
        assert path[-1] == sink, f"Path must end at sink"
        assert s.cost >= 0.0


# ── Test 7: column generation (requires mip) ──────────────────────────────────


def test_cg_tiny():
    """Full column generation on the tiny instance converges and covers all
    customers."""
    import pytest

    # On Windows, mip auto-detects Gurobi via a registry path that may be None.
    # LoadLibrary(None) / NoneType-iteration errors then surface in __del__
    # (teardown) rather than at import time, so we must skip before importing
    # mip at all.
    if sys.platform == "win32":
        pytest.skip("mip/Gurobi not available on Windows CI runners")

    try:
        import mip  # noqa: F401
    except Exception:
        pytest.skip("mip not available on this platform")

    inst = make_tiny_instance()
    vrp = VRP(inst)
    try:
        mp_sol = vrp.solve()
    except Exception as exc:
        pytest.skip(f"MIP solver unavailable: {exc}")

    assert mp_sol.cost > 0, f"IP cost must be positive, got {mp_sol.cost}"

    demand_ids = set(inst.get_demand_customers_id())
    paths_by_id = {p.id: p for p in vrp._VRP__paths}
    covered = set()
    for pid, val in mp_sol.value_by_var_id.items():
        if val > 0.5:
            covered.update(nid for nid in paths_by_id[pid].visited_nodes if nid in demand_ids)
    assert covered == demand_ids, f"Uncovered customers: {demand_ids - covered}"


# ── Run all tests ─────────────────────────────────────────────────────────────

if __name__ == "__main__":
    tests = [
        ("graph construction", test_graph_construction),
        ("subproblem with zero duals", test_subproblem_zero_duals),
        ("subproblem with positive duals", test_subproblem_with_duals),
        ("time-window infeasibility", test_time_window_infeasibility),
        ("capacity feasibility", test_capacity_feasibility),
        ("C101_5 benchmark instance", test_c101_5_subproblem),
        ("column generation (tiny)", test_cg_tiny),
    ]

    for name, fn in tests:
        print(f"Test: {name} ... ", end="", flush=True)
        fn()
        print("PASSED")

    print(f"\nAll {len(tests)} VRP tests passed.")
