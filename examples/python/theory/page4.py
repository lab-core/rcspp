"""Page 4, algorithms and preprocessing: the numbers of docs/theory/algorithms-and-preprocessing.md.

Everything runs on page 2's pricing graph (time windows, capacity 11), at duals pi = 20 unless
stated otherwise.
"""

from __future__ import annotations

from rcspp.graph import AlgorithmParams

from . import toy

PI = {1: 20.0, 2: 20.0, 3: 20.0}
PI_KNOWN_ISSUE = {1: 12.0, 2: 12.5, 3: 0.0}


def graph(duals=PI):
    """Page 2's pricing graph, priced at ``duals``."""
    return toy.priced(toy.pricing_graph(windows=True), duals)


def solve(duals=PI, algorithm: str = "simple", **params):
    """Solve page 2's graph with ``upper_bound=-1e-9`` and the given ``AlgorithmParams`` fields."""
    p = AlgorithmParams()
    for name, value in params.items():
        setattr(p, name, value)
    return graph(duals).solve(algorithm=algorithm, upper_bound=-toy.EPSILON, params=p)


def numbers() -> dict:
    """Compute every number page 4 states."""
    out: dict = {}

    # 4.2: the four exact labelling algorithms agree
    out["exact"] = {}
    for algorithm in ("simple", "pushing", "pulling", "astar"):
        result = solve(algorithm=algorithm)
        out["exact"][algorithm] = (result.status_string(), toy.routes(result))

    # 4.2 note: the cycle 1 → 2 → 1 at page 1's final duals
    out["cycle_at_final_duals"] = (4 - 11.5) + (4 - 12.5)

    # 4.3: trading exactness for speed
    out["truncation"] = {
        "default": toy.routes(solve()),
        "num_labels_to_extend_by_node = 1": toy.routes(solve(num_labels_to_extend_by_node=1)),
        "num_labels_to_extend_by_node = 1, num_max_phases = 3": toy.routes(
            solve(num_labels_to_extend_by_node=1, num_max_phases=3)
        ),
    }

    # 4.4: the dive algorithms
    out["greedy"] = toy.routes(solve(algorithm="greedy", stop_after_X_solutions=1))
    out["tabu"] = toy.routes(solve(algorithm="tabu", max_iterations=5))

    # 4.5: the known issue with truncation and status
    exact = solve(PI_KNOWN_ISSUE)
    truncated = solve(PI_KNOWN_ISSUE, num_labels_to_extend_by_node=1)
    out["known_issue"] = {
        "exact": (exact.status_string(), toy.routes(exact)),
        "truncated": (truncated.status_string(), toy.routes(truncated)),
    }

    # 4.6: feasibility preprocessing removes two arcs, which stay removed after the solve
    rg = graph()
    before = rg.get_arcs_size()
    rg.solve(upper_bound=-toy.EPSILON)
    removed = rg._graph.removed_arc_ids()  # the wrapper has no public accessor for these
    out["feasibility_removed"] = sorted(_arc_name(*toy.arcs()[arc_id]) for arc_id in removed)
    out["arcs_before_and_after"] = (before, rg.get_arcs_size())

    # 4.6: the shortest-path bound, on the distances without duals, for routes of length <= 24
    out["shortest_path_bound"] = {
        "1→3": _through(1, 3),
        "1→2": _through(1, 2),
    }
    return out


def _arc_name(i: int, j: int) -> str:
    return f"{i}→{'0′' if j == toy.SINK else j}"


def _through(u: int, v: int) -> float:
    """The cheapest route through arc u → v: from(u) + c + to(v), on the distances alone."""
    return toy.distance(0, u) + toy.distance(u, v) + toy.distance(v, toy.SINK)


def report() -> str:
    """Page 4's numbers, as text."""
    n = numbers()
    lines = ["Page 4: algorithms and preprocessing (page 2's graph, pi = 20)", ""]
    for algorithm, (status, found) in n["exact"].items():
        lines.append(f"4.2 {algorithm}: {status}, {_costs(found)}")
    lines.append(f"    1→2→1 at page 1's final duals: {n['cycle_at_final_duals']:g}")
    for setting, found in n["truncation"].items():
        lines.append(f"4.3 simple, {setting}: {_costs(found)}")
    lines.append(f"4.4 greedy, stop_after_X_solutions = 1: {_costs(n['greedy'])}")
    lines.append(
        "    tabu, max_iterations = 5: "
        + ", ".join(f"{cost:g} via {_route(path)}" for cost, path in n["tabu"])
    )
    for name, (status, found) in n["known_issue"].items():
        lines.append(f"4.5 pi = (12, 12.5, 0), {name}: {status}, {_costs(found) or 'no route'}")
    lines.append(
        f"4.6 feasibility preprocessing removed {', '.join(n['feasibility_removed'])};"
        f" live arcs before and after the solve: {n['arcs_before_and_after']}"
    )
    lines.append(
        "    cheapest route through each arc, without duals: "
        + ", ".join(f"{arc}: {cost:g}" for arc, cost in n["shortest_path_bound"].items())
    )
    return "\n".join(lines)


def _costs(found) -> str:
    return ", ".join(f"{cost:g}" for cost, _ in found)


def _route(path) -> str:
    return "→".join("0′" if n == toy.SINK else str(n) for n in path)
