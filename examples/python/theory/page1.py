"""Page 1, master problem, pricing and duals: the numbers of docs/theory/master-and-pricing.md."""

from __future__ import annotations

from itertools import combinations
from math import perm

from . import toy


def numbers() -> dict:
    """Compute every number page 1 states."""
    out: dict = {}

    # 1.3: the legal routes of the capacity-only instance, and their costs
    legal = toy.elementary_routes(toy.CUSTOMERS, toy.DEMANDS, toy.CAPACITY)
    out["route_costs"] = {_name(r): toy.route_cost(r) for r in legal}

    # 1.4: every integer plan, by brute force over sets of legal routes
    plans = {}
    for k in range(1, len(legal) + 1):
        for chosen in combinations(legal, k):
            visits = sorted(i for r in chosen for i in r[1:-1])
            if visits == list(toy.CUSTOMERS):
                plans[" + ".join(_name(r) for r in chosen)] = sum(toy.route_cost(r) for r in chosen)
    out["integer_plans"] = plans
    out["integer_optimum"] = min(plans.values())

    # 1.4: how many routes visit each of n customers at most once, ignoring capacity
    out["route_counts"] = {n: sum(perm(n, k) for k in range(1, n + 1)) for n in (3, 5, 10, 25, 100)}

    # 1.5 and 1.6: the master LP over every legal route, and its duals
    lp = toy.master_lp(legal, [toy.route_cost(r) for r in legal])
    out["lp_value"] = lp.value
    out["lp_solution"] = {_name(r): x for r, x in zip(legal, lp.x) if x > toy.EPSILON}
    out["lp_duals"] = lp.duals

    # 1.10: column generation by hand, one route per iteration, pricing with rcspp
    history, _, _ = toy.column_generation(
        lambda: toy.pricing_graph(), toy.single_customer_routes(), best_only=True
    )
    out["column_generation"] = [
        {
            "columns": it.columns,
            "value": it.value,
            "duals": it.duals,
            "best_reduced_cost": it.best_reduced_cost,
            "added": _name(it.added) if it.added else None,
        }
        for it in history
    ]

    # 1.11: does a new column always lower the cost? The two steps of 1.10 that add a pair route
    out["new_column"] = _new_column(history)

    # 1.12: the early lower bound at iteration 0, with kappa = 3
    first = history[0]
    out["early_lower_bound_iteration_0"] = first.value + 3 * first.best_reduced_cost

    # Check yourself 2: pi = (5, 19, 20) at iteration 1
    pi = {1: 5.0, 2: 19.0, 3: 20.0}
    out["exercise_2_reduced_costs"] = {
        _name(r): toy.route_cost(r) - sum(pi[i] for i in r[1:-1]) for r in legal if len(r) == 4
    }

    # Check yourself 3: the depot at distance 2 from every customer
    near = {**toy.DISTANCES, (0, 1): 2, (0, 2): 2, (0, 3): 2}
    near_routes = toy.elementary_routes(toy.CUSTOMERS, toy.DEMANDS, toy.CAPACITY, near)
    near_costs = [toy.route_cost(r, near) for r in near_routes]
    out["exercise_3_lp"] = toy.master_lp(near_routes, near_costs).value
    out["exercise_3_ip"] = toy.master_ip(near_routes, near_costs).value
    return out


def _new_column(history) -> dict:
    """Section 1.11: what adding p_12 at iteration 0, then p_13 at iteration 1, does to the LP.

    A change dx of the column values that keeps every customer covered once changes the cost by
    sum_p c_p dx_p = sum_p cbar_p dx_p, since the dual terms cancel. So the cost moves at the rate
    of the reduced costs of the columns that move.
    """
    sink = toy.SINK
    p1, p2, p3 = [0, 1, sink], [0, 2, sink], [0, 3, sink]
    p12, p13 = [0, 1, 2, sink], [0, 1, 3, sink]
    cost = toy.route_cost

    def lp(columns):
        return toy.master_lp(columns, [cost(c) for c in columns])

    def reduced(route, duals):
        pi = dict(zip(toy.CUSTOMERS, duals))
        return cost(route) - sum(pi[i] for i in route[1:-1])

    def rate(direction, duals=None):
        """The cost change per unit along ``direction``, from the costs or the reduced costs."""
        if duals is None:
            return sum(cost(r) * d for r, d in direction)
        return sum(reduced(r, duals) * d for r, d in direction)

    pi0, pi1 = history[0].duals, history[1].duals
    # Iteration 0 -> 1: x_1 = x_2 = x_3 = 1. p_12 takes over customers 1 and 2 from p_1 and p_2.
    grow_p12 = [(p12, 1), (p1, -1), (p2, -1)]
    # Iteration 1 -> 2: x_12 = x_3 = 1, x_1 = x_2 = 0. Where p_13 covers customer 1, p_12 must
    # make room, and customer 2, which p_12 covered too, falls back to p_2.
    grow_p13 = [(p13, 1), (p3, -1), (p12, -1), (p2, 1)]
    # What the duals of iteration 1 price: p_13 taking over from p_1 and p_3, but x_1 is already 0
    priced_p13 = [(p13, 1), (p1, -1), (p3, -1)]
    lp1 = lp([p1, p2, p3, p12])
    return {
        "iteration_0": {
            "reduced_cost": reduced(p12, pi0),
            "rate": rate(grow_p12),
            "rate_from_reduced_costs": rate(grow_p12, pi0),
            "value_after": lp([p1, p2, p3, p12]).value,
        },
        "iteration_1": {
            "columns_above_zero": sum(x > toy.EPSILON for x in lp1.x),
            "reduced_cost": reduced(p13, pi1),
            "priced_rate": rate(priced_p13),
            "rate": rate(grow_p13),
            "rate_from_reduced_costs": rate(grow_p13, pi1),
            "old_duals_on_p13": dict(zip(toy.CUSTOMERS, pi1))[1] + dict(zip(toy.CUSTOMERS, pi1))[3],
            "value_after": lp([p1, p2, p3, p12, p13]).value,
        },
        # Check yourself 6: add p_13 at iteration 0 instead of p_12
        "exercise_p13_first": lp([p1, p2, p3, p13]).value,
    }


def _name(route) -> str:
    """``p_12`` style name of a route, from the set of customers it visits.

    Distances are symmetric on page 1, so a route and its reverse are the same column.
    """
    return "p_" + "".join(str(i) for i in sorted(route[1:-1]))


def report() -> str:
    """Page 1's numbers, as text."""
    n = numbers()
    lines = ["Page 1: master problem, pricing and duals", ""]
    lines.append(
        "1.3 legal routes: " + ", ".join(f"{k} = {v:g}" for k, v in n["route_costs"].items())
    )
    lines.append(
        "1.4 integer plans: " + ", ".join(f"{k} = {v:g}" for k, v in n["integer_plans"].items())
    )
    lines.append(f"    best plan: {n['integer_optimum']:g}")
    lines.append(
        "    routes visiting n customers at most once: "
        + ", ".join(
            f"n={k}: {v:.3g}" if v > 1e8 else f"n={k}: {v:,}" for k, v in n["route_counts"].items()
        )
    )
    lines.append(
        f"1.5 master LP: {n['lp_value']:g} with "
        + ", ".join(f"{k} = {toy.fraction(v)}" for k, v in n["lp_solution"].items())
    )
    lines.append(f"1.6 duals: {_fmt(n['lp_duals'])}")
    lines.append("1.10 column generation, one route per iteration:")
    for i, it in enumerate(n["column_generation"]):
        lines.append(
            f"    iteration {i}: {it['columns']} columns, RMP {it['value']:g},"
            f" pi = {_fmt(it['duals'])}, best reduced cost {it['best_reduced_cost']:g},"
            f" adds {it['added']}"
        )
    for name, step in n["new_column"].items():
        if name.startswith("iteration"):
            lines.append(
                f"1.11 {name}: " + ", ".join(f"{k} = {v:g}" for k, v in step.items())
            )
    lines.append(
        f"     exercise, p_13 at iteration 0: RMP {n['new_column']['exercise_p13_first']:g}"
    )
    lines.append(f"1.12 early lower bound at iteration 0: {n['early_lower_bound_iteration_0']:g}")
    lines.append(
        "Exercise 2, pi = (5, 19, 20): "
        + ", ".join(f"{k}: {v:g}" for k, v in n["exercise_2_reduced_costs"].items())
    )
    lines.append(
        f"Exercise 3, depot at distance 2: LP {n['exercise_3_lp']:g}, IP {n['exercise_3_ip']:g}"
    )
    return "\n".join(lines)


def _fmt(values) -> str:
    return "(" + ", ".join(f"{v:g}" for v in values) + ")"
