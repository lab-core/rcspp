"""Page 3, dominance and elementarity: the numbers of docs/theory/dominance-and-elementarity.md.

Sections 3.4 to 3.6 use page 1's instance without time windows, with the capacity raised to 13 so
that the cycle 0 → 1 → 2 → 1 → 0′ fits in the vehicle. The "no waiting" example of section 3.3 needs
per-node windows on a ``MinMaxFeasibilityFunction``, which Python does not expose: it is checked in
``tests/cpp/test_theory_docs.hpp``.
"""

from __future__ import annotations

from . import toy

CAPACITY = 13
PI_0 = {1: 20.0, 2: 20.0, 3: 20.0}
PI_FINAL = {1: 11.5, 2: 12.5, 3: 13.5}
MEMORIES = {"revisits allowed": None, "ng-paths": "ng", "elementary": "elementary"}


def graph(memory: str | None):
    """The capacity-13 pricing graph, with the route memory of section 3.5 or 3.6."""
    return toy.pricing_graph(capacity=CAPACITY, memory=memory)


def numbers() -> dict:
    """Compute every number page 3 states."""
    out: dict = {}

    # 3.4: the cycle 0 → 1 → 2 → 1 → 0′
    cycle = [0, 1, 2, 1, toy.SINK]
    out["cycle_load"] = sum(toy.DEMANDS[i] for i in cycle[1:-1])
    out["cycle_cost"] = toy.route_cost(cycle)

    # 3.4 and 3.6: the master LP for each set of allowed paths, by column generation
    out["lp"] = {}
    for name, memory in MEMORIES.items():
        _, columns, lp = toy.column_generation(
            lambda m=memory: graph(m), toy.single_customer_routes(), best_only=False
        )
        used = {_route(c): toy.fraction(x) for c, x in zip(columns, lp.x) if x > toy.EPSILON}
        out["lp"][name] = {"value": lp.value, "solution": used}

    # 3.4: pricing at page 1's final duals
    out["final_duals_best"] = {}
    for name, memory in MEMORIES.items():
        found = toy.routes(toy.priced(graph(memory), PI_FINAL).solve())
        out["final_duals_best"][name] = found[0]

    # 3.5: labels left at the sink at iteration 0
    out["labels_at_sink"] = {
        name: len(toy.priced(graph(memory), PI_0).solve().solutions)
        for name, memory in MEMORIES.items()
    }

    # 3.6: the ng memory along 0 → 2 → 1 → 3, as the page defines it and as the library stores it
    out["ng_walk"] = memory_walk([0, 2, 1, 3])

    # Check yourself 4: are these ng-paths?
    out["exercise_4"] = {_route(p): is_ng_path(p) for p in ([0, 1, 3, 1], [0, 1, 2, 3, 1])}
    return out


def memory_walk(path: list[int]) -> list[dict]:
    """Section 3.6's memory M after each node, and the set S the library stores on arrival.

    ``M`` follows the page: on visiting ``j``, ``M = (M ∩ N(j)) ∪ {j}``. ``S`` follows
    ``NgPathExtensionFunction``: on arc ``i → j``, ``S = (S ∪ {i}) ∩ N(j)``, where ``j`` always
    counts as part of ``N(j)`` and the depot is never remembered.
    """
    steps, m, s = [], set(), set()
    for i, j in zip(path, path[1:]):
        n_j = toy.NG_NEIGHBOURHOODS[j] | {j}
        m = (m & n_j) | {j}
        s = (s | ({i} if i != 0 else set())) & n_j
        steps.append({"node": j, "M": sorted(m), "S": sorted(s)})
    return steps


def is_ng_path(path: list[int]) -> bool:
    """Whether ``path`` never goes to a customer that is in its memory."""
    m: set[int] = set()
    for j in path[1:]:
        if j in m:
            return False
        m = (m & (toy.NG_NEIGHBOURHOODS[j] | {j})) | {j}
    return True


def report() -> str:
    """Page 3's numbers, as text."""
    n = numbers()
    lines = ["Page 3: dominance and elementarity (no time windows, capacity 13)", ""]
    lines.append(f"3.4 0→1→2→1→0′: load {n['cycle_load']:g}, distance {n['cycle_cost']:g}")
    for name, lp in n["lp"].items():
        solution = ", ".join(f"{k} = {v}" for k, v in lp["solution"].items())
        lines.append(f"    master LP, {name}: {lp['value']:g} with {solution}")
    lines.append("    best reduced cost at pi = (11.5, 12.5, 13.5):")
    for name, (cost, path) in n["final_duals_best"].items():
        lines.append(f"        {name}: {cost:g} ({_route(path)})")
    lines.append(
        "3.5 labels at the sink at pi = 20: "
        + ", ".join(f"{k}: {v}" for k, v in n["labels_at_sink"].items())
    )
    lines.append("3.6 memory along 0→2→1→3 (page: M; library: S):")
    for step in n["ng_walk"]:
        lines.append(
            f"    after {step['node']}: M = {set(step['M'])}, S = {set(step['S']) or '{}'}"
        )
    lines.append(
        "Exercise 4: "
        + ", ".join(f"{k} is {'an' if v else 'not an'} ng-path" for k, v in n["exercise_4"].items())
    )
    return "\n".join(lines)


def _route(path) -> str:
    return "→".join("0′" if n == toy.SINK else str(n) for n in path)
