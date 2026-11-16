"""Page 6, from LP to integer solutions: the numbers of docs/theory/lp-to-integer.md.

Page 2's time-window instance (capacity 11), with column generation run around ``rcspp`` as the
page describes: the master LP and IP are solved with HiGHS, pricing with the library. Section 6.2's
counterexample is a separate instance with four customers.
"""

from __future__ import annotations

from collections import Counter
from itertools import permutations

from . import toy

FLEET_ROW = 0  # the vehicle-count cut's row: customers use rows 1 to 3

# Section 6.2's counterexample: four customers, capacity 10, no time windows.
CE_CUSTOMERS = (1, 2, 3, 4)
CE_DEMANDS = {1: 3, 2: 3, 3: 5, 4: 2}
CE_CAPACITY = 10
CE_DISTANCES = {
    (0, 1): 8, (0, 2): 5, (0, 3): 6, (0, 4): 7,
    (1, 2): 4, (1, 3): 10, (1, 4): 15,
    (2, 3): 6, (2, 4): 12,
    (3, 4): 10,
}  # fmt: skip


def time_window_routes() -> list[list[int]]:
    """Every legal route of page 2's instance.

    Each customer is visited at most once, within the capacity and the time windows.
    """
    found = []
    for k in range(1, len(toy.CUSTOMERS) + 1):
        for order in permutations(toy.CUSTOMERS, k):
            path = [0, *order, toy.SINK]
            if sum(toy.DEMANDS[i] for i in order) <= toy.CAPACITY and _meets_windows(path):
                found.append(path)
    return found


def _meets_windows(path: list[int]) -> bool:
    time = 0.0
    for i, j in zip(path, path[1:]):
        time = max(time + toy.distance(i, j), toy.TIME_WINDOWS[j][0])
        if time > toy.TIME_WINDOWS[j][1]:
            return False
    return True


def arc_flows(columns, x) -> dict[str, float]:
    """How much the LP solution uses each arc: the sum of x_p over the paths p that use it."""
    flows: Counter = Counter()
    for path, value in zip(columns, x):
        for i, j in zip(path, path[1:]):
            flows[_arc(i, j)] += value
    return {arc: flow for arc, flow in flows.items() if flow > toy.EPSILON}


def branch(removed_arcs) -> tuple[toy.LPResult, list[list[int]]]:
    """Column generation in a branch that removes ``removed_arcs`` from the pricing graph.

    Columns that use a removed arc are left out of the RMP, and an artificial column per customer
    keeps it feasible, as section 6.4 suggests. Returns the final LP and its columns.
    """
    removed = set(removed_arcs)
    initial = [c for c in toy.single_customer_routes() if not _uses(c, removed)]
    _, columns, lp = toy.column_generation(
        lambda: toy.pricing_graph(windows=True, removed_arcs=removed),
        initial,
        best_only=False,
        artificial=True,
    )
    return lp, columns


def _uses(path, arcs) -> bool:
    return any((i, j) in arcs for i, j in zip(path, path[1:]))


def numbers() -> dict:
    """Compute every number page 6 states."""
    out: dict = {}

    # The legal routes and the root LP and IP
    legal = time_window_routes()
    costs = [toy.route_cost(r) for r in legal]
    out["routes"] = {_route(r): c for r, c in zip(legal, costs)}
    root = toy.master_lp(legal, costs)
    out["root_lp"] = root.value
    out["root_solution"] = {
        _route(r): toy.fraction(x) for r, x in zip(legal, root.x) if x > toy.EPSILON
    }
    out["root_ip"] = toy.master_ip(legal, costs).value

    # 6.1: the gap
    out["gap"] = (out["root_ip"] - out["root_lp"]) / out["root_ip"]

    # 6.2: the counterexample
    history, columns, lp = toy.column_generation(
        lambda: toy.pricing_graph(
            capacity=CE_CAPACITY,
            memory="elementary",
            customers=CE_CUSTOMERS,
            demands=CE_DEMANDS,
            distances=CE_DISTANCES,
        ),
        toy.single_customer_routes(CE_CUSTOMERS),
        customers=CE_CUSTOMERS,
        distances=CE_DISTANCES,
        best_only=True,
    )
    sink = toy.sink_of(CE_CUSTOMERS)
    ce_costs = [toy.route_cost(c, CE_DISTANCES, sink) for c in columns]
    every = toy.elementary_routes(CE_CUSTOMERS, CE_DEMANDS, CE_CAPACITY, CE_DISTANCES)
    every_costs = [toy.route_cost(c, CE_DISTANCES, sink) for c in every]
    rmp_ip = toy.master_ip(columns, ce_costs, CE_CUSTOMERS)
    true_ip = toy.master_ip(every, every_costs, CE_CUSTOMERS)
    duals = dict(zip(CE_CUSTOMERS, lp.duals))
    route_34 = [0, 3, 4, sink]
    out["counterexample"] = {
        "added": [
            (_set(it.added), toy.route_cost(it.added, CE_DISTANCES, sink))
            for it in history
            if it.added
        ],
        "lp": lp.value,
        "lp_solution": [toy.fraction(x) for x in lp.x if x > toy.EPSILON],
        "rmp_ip": rmp_ip.value,
        "rmp_ip_plan": _plan(columns, rmp_ip.x),
        "true_optimum": true_ip.value,
        "true_plan": _plan(every, true_ip.x),
        "final_duals": lp.duals,
        "reduced_cost_34": toy.route_cost(route_34, CE_DISTANCES, sink) - duals[3] - duals[4],
    }

    # 6.4: arc flows at the root, and the two branches on arc 1 → 2
    out["root_flows"] = arc_flows(legal, root.x)
    branch_a, columns_a = branch([(1, 3), (1, toy.SINK), (0, 2), (3, 2)])
    branch_b, _ = branch([(1, 2)])
    out["branch_a"] = {
        "lp": branch_a.value,
        "solution": _plan(columns_a, branch_a.x, exact=False),
        "integer": all(v < toy.EPSILON or v > 1 - toy.EPSILON for v in branch_a.x),
        "uses_artificial": branch_a.uses_artificial,
    }
    out["branch_b"] = {"lp": branch_b.value, "uses_artificial": branch_b.uses_artificial}

    # 6.5: the vehicle-count cut sum x >= 2, with its dual on every arc leaving the source
    _, cut_columns, cut = toy.column_generation(
        lambda: toy.pricing_graph(windows=True, fleet_row=FLEET_ROW),
        toy.single_customer_routes(),
        best_only=False,
        min_vehicles=2,
        fleet_row=FLEET_ROW,
    )
    out["cut"] = {
        "lp": cut.value,
        "solution": _plan(cut_columns, cut.x, exact=False),
        "duals": cut.duals,
        "sigma": cut.extra_dual,
    }
    return out


def _plan(columns, x, exact: bool = True) -> list[str]:
    """The routes an integer (or integral) solution uses."""
    return [_set(c) for c, v in zip(columns, x) if v > 0.5 or (not exact and v > toy.EPSILON)]


def _set(path) -> str:
    return "{" + ",".join(str(i) for i in path[1:-1]) + "}"


def _arc(i: int, j: int) -> str:
    return f"{i}→{'0′' if j == toy.SINK else j}"


def _route(path) -> str:
    return "→".join("0′" if n == toy.SINK else str(n) for n in path)


def report() -> str:
    """Page 6's numbers, as text."""
    n = numbers()
    ce = n["counterexample"]
    lines = ["Page 6: from LP to integer solutions (page 2's instance, capacity 11)", ""]
    lines.append("routes: " + ", ".join(f"{k} = {v:g}" for k, v in n["routes"].items()))
    lines.append(
        f"root LP {n['root_lp']:g} with "
        + ", ".join(f"{k} = {v}" for k, v in n["root_solution"].items())
        + f"; best integer plan {n['root_ip']:g}"
    )
    lines.append(f"6.1 gap: {n['gap']:.1%}")
    lines.append(
        "6.2 counterexample, column generation adds "
        + ", then ".join(f"{s} (cost {c:g})" for s, c in ce["added"])
    )
    lines.append(
        f"    LP {ce['lp']:g} using {', '.join(str(v) for v in ce['lp_solution'])}"
        " of the generated routes"
    )
    lines.append(f"    integer RMP {ce['rmp_ip']:g}: {' + '.join(ce['rmp_ip_plan'])}")
    lines.append(f"    true optimum {ce['true_optimum']:g}: {' + '.join(ce['true_plan'])}")
    lines.append(
        f"    final duals ({', '.join(f'{v:g}' for v in ce['final_duals'])}),"
        f" reduced cost of {{3,4}}: {ce['reduced_cost_34']:g}"
    )
    lines.append(
        "6.4 root arc flows: "
        + ", ".join(f"{k}: {toy.fraction(v)}" for k, v in n["root_flows"].items())
    )
    a, b = n["branch_a"], n["branch_b"]
    lines.append(
        f"    branch A (1→2 forced): LP {a['lp']:g} with {' + '.join(a['solution'])},"
        f" integer: {a['integer']}, artificial columns used: {a['uses_artificial']}"
    )
    lines.append(
        f"    branch B (1→2 forbidden): LP {b['lp']:g},"
        f" artificial columns used: {b['uses_artificial']}"
    )
    cut = n["cut"]
    lines.append(
        f"6.5 with the cut: LP {cut['lp']:g} with {' + '.join(cut['solution'])},"
        f" pi = ({', '.join(f'{v:g}' for v in cut['duals'])}), sigma = {cut['sigma']:g}"
    )
    return "\n".join(lines)
