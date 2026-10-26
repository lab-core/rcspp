"""Page 5, searching from both ends: the numbers of docs/theory/searching-from-both-ends.md.

Page 2's pricing graph at duals pi = 20, with time (resource 1) as the clock. The tables of the
labels each search keeps need the bidirectional search's internals, which Python does not expose:
they are checked in ``tests/cpp/test_theory_docs.hpp``.
"""

from __future__ import annotations

from rcspp import presets
from rcspp.graph import ResourceGraph
from rcspp.resource import (
    InclusionDominanceFunction,
    IntersectionFeasibilityFunction,
    TrivialCostFunction,
    UnionExtensionFunction,
)

from . import toy

PI = {1: 20.0, 2: 20.0, 3: 20.0}
TIME = 1  # the clock: resource 1, after the reduced cost
HALF_WAY_POINTS = (16.0, 100.0, 0.0)


def graph(**kwargs):
    """Page 2's pricing graph, priced at ``PI``."""
    return toy.priced(toy.pricing_graph(windows=True, **kwargs), PI)


def bidirectional(rg, clock: int = TIME, half_way_point: float = 16.0):
    """A bidirectional solve with ``upper_bound=-1e-9``."""
    params = toy.bidirectional_params(clock, half_way_point)
    return rg.solve(algorithm="bidirectional", upper_bound=-toy.EPSILON, params=params)


def numbers() -> dict:
    """Compute every number page 5 states that Python can see."""
    out: dict = {}

    # 5.2: a backward search on its own, against a forward one
    out["forward"] = toy.routes(graph().solve(upper_bound=-toy.EPSILON))
    out["backward"] = toy.routes(graph().solve(direction="backward", upper_bound=-toy.EPSILON))

    # 5.2: two backward steps, by the formula min(l_i, d_j - t_ij)
    out["deadline_steps"] = {
        "3→0′ back along 1→3": min(toy.TIME_WINDOWS[1][1], 18 - toy.distance(1, 3)),
        "3→0′ back along 2→3": min(toy.TIME_WINDOWS[2][1], 18 - toy.distance(2, 3)),
    }

    # 5.4: the half-way point
    out["half_way"] = {}
    for h in HALF_WAY_POINTS:
        result = bidirectional(graph(), half_way_point=h)
        stats = result.bidirectional
        out["half_way"][h] = {
            "routes": toy.routes(result),
            "bounded": stats.bounded_by_half_way,
            "joined": stats.number_of_joined_paths,
        }

    # 5.4: the cost is not a valid clock
    cost_clock = bidirectional(graph(), clock=0, half_way_point=5.0).bidirectional
    out["cost_clock"] = {
        "bounded": cost_clock.bounded_by_half_way,
        "reason": cost_clock.half_way_off_reason,
    }

    # 5.6 and 5.8: the model checks, and the same model through the presets
    out["check_model"] = graph().check_model("bidirectional")
    with_presets = graph(with_presets=True)
    out["presets_check_model"] = with_presets.check_model("bidirectional")
    out["presets_routes"] = toy.routes(bidirectional(with_presets))

    # 5.5: memories at the join, on page 3's graph (no time, capacity 13; the load is the clock)
    out["ng_join"] = {}
    for name, memory in (("no memory", None), ("ng memory", "ng")):
        rg = toy.priced(toy.pricing_graph(capacity=13, memory=memory, with_presets=True), PI)
        out["ng_join"][name] = toy.routes(bidirectional(rg, clock=1, half_way_point=6.5))[0]

    # 5.6: a visited set built from arc values is refused
    out["union_set_problems"] = union_visited_set().check_model("bidirectional")
    return out


def union_visited_set():
    """Page 3's capacity-13 graph with section 3.5's visited set: a union of arc values {i}."""
    rg = ResourceGraph()
    presets.add_cost_resource(rg)
    presets.add_capacity_resource(rg, "real", 13.0)
    rg.add_int_set_resource(
        UnionExtensionFunction(),
        IntersectionFeasibilityFunction({i: {i} for i in toy.CUSTOMERS}, forbidden=True),
        TrivialCostFunction(),
        InclusionDominanceFunction(),
    )
    rg.add_node(0, source=True)
    for i in toy.CUSTOMERS:
        rg.add_node(i)
    rg.add_node(toy.SINK, sink=True)
    for i, j in toy.arcs():
        d = toy.distance(i, j)
        rg.add_arc((d, toy.DEMANDS.get(j, 0), set() if i == 0 else {i}), i, j, cost=d)
    return rg


def report() -> str:
    """Page 5's numbers, as text."""
    n = numbers()
    lines = ["Page 5: searching from both ends (page 2's graph, pi = 20, time as the clock)", ""]
    lines.append(f"5.2 forward: {_routes(n['forward'])}")
    lines.append(f"    backward: {_routes(n['backward'])}")
    lines.append(
        "    deadlines: "
        + ", ".join(f"{k}: {v:g}" for k, v in n["deadline_steps"].items())
        + " (below node 2's opening time 20: impossible)"
    )
    for h, run in n["half_way"].items():
        lines.append(
            f"5.4 H = {h:g}: {_routes(run['routes'])}; split in force: {run['bounded']},"
            f" routes from the join: {run['joined']}"
        )
    lines.append(
        f"    cost as the clock: split in force: {n['cost_clock']['bounded']}"
        f" ({n['cost_clock']['reason']})"
    )
    lines.append(f"5.5 best route without a memory: {_routes([n['ng_join']['no memory']])}")
    lines.append(f"    best route with the ng memory: {_routes([n['ng_join']['ng memory']])}")
    lines.append(
        f"5.6 check_model on page 2's graph: {n['check_model']},"
        f" with the presets: {n['presets_check_model']}"
    )
    lines.append(f"    presets, H = 16: {_routes(n['presets_routes'])}")
    lines.append("    check_model on a union visited set:")
    lines.extend(f"        {problem}" for problem in n["union_set_problems"])
    return "\n".join(lines)


def _routes(found) -> str:
    return ", ".join(f"{_route(path)} ({cost:g})" for cost, path in found)


def _route(path) -> str:
    return "→".join("0′" if n == toy.SINK else str(n) for n in path)
