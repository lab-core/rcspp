"""Page 2, resources and labels: the numbers of docs/theory/resources-and-labels.md.

The page traces the search by hand. Its counts (labels created, extensions rejected, labels
dominated) come from that trace, which the library does not report, so ``trace`` below repeats
it: the first-in-first-out search of section 2.6, written out in plain Python. Its result at the
sink is checked against the library's.
"""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass

from . import toy

PI = {1: 20.0, 2: 20.0, 3: 20.0}


@dataclass
class Trace:
    """What the search of section 2.6 did."""

    created: int
    infeasible: int
    dominated: int
    at_sink: list[tuple[list[int], tuple[float, float, float]]]


def trace(*, dominance: bool = True) -> Trace:
    """Run section 2.6's search on page 2's graph, at duals ``PI``.

    A label is ``(path, (reduced cost, time, load))``. Without ``dominance``, every feasible label
    is kept, which counts the routes that would reach the sink.
    """
    created = infeasible = dominated = 0
    kept: dict[int, list] = {}
    queue = deque([([0], (0.0, 0.0, 0.0))])
    while queue:
        path, values = queue.popleft()
        node = path[-1]
        if node == toy.SINK or (
            dominance and (path, values) not in kept.get(node, [(path, values)])
        ):
            continue
        for nxt in (*toy.CUSTOMERS, toy.SINK):
            if nxt == node or (node == 0 and nxt == toy.SINK):
                continue
            label = (path + [nxt], _extend(node, nxt, values))
            if not _feasible(nxt, label[1]):
                infeasible += 1
                continue
            created += 1
            here = kept.setdefault(nxt, [])
            if dominance and any(_dominates(other[1], label[1]) for other in here):
                dominated += 1
                continue
            if dominance:
                beaten = [other for other in here if _dominates(label[1], other[1])]
                dominated += len(beaten)
                here[:] = [other for other in here if other not in beaten]
            here.append(label)
            queue.append(label)
    return Trace(created, infeasible, dominated, kept.get(toy.SINK, []))


def _extend(i: int, j: int, values: tuple[float, float, float]) -> tuple[float, float, float]:
    cost, time, load = values
    d = toy.distance(i, j)
    reduced = d - PI.get(i, 0.0)
    return cost + reduced, max(time + d, toy.TIME_WINDOWS[j][0]), load + toy.DEMANDS.get(j, 0)


def _feasible(j: int, values: tuple[float, float, float]) -> bool:
    _, time, load = values
    return time <= toy.TIME_WINDOWS[j][1] and load <= toy.CAPACITY


def _dominates(a: tuple, b: tuple) -> bool:
    return all(x <= y for x, y in zip(a, b))


def numbers() -> dict:
    """Compute every number page 2 states."""
    with_dominance = trace()
    without = trace(dominance=False)
    rg = toy.priced(toy.pricing_graph(windows=True), PI)
    return {
        "labels_created": with_dominance.created,
        "extensions_infeasible": with_dominance.infeasible,
        "labels_dominated": with_dominance.dominated,
        "trace_at_sink": [(path, values) for path, values in with_dominance.at_sink],
        "routes_without_dominance": len(without.at_sink),
        "library_at_sink": toy.routes(rg.solve()),
        "library_negative": toy.routes(rg.solve(upper_bound=-toy.EPSILON)),
    }


def report() -> str:
    """Page 2's numbers, as text."""
    n = numbers()
    lines = ["Page 2: resources and labels (time windows, capacity 11, pi = 20)", ""]
    lines.append(
        f"2.6 the search created {n['labels_created']} feasible labels (plus the start), rejected"
        f" {n['extensions_infeasible']} extensions as infeasible, and discarded"
        f" {n['labels_dominated']} labels by dominance"
    )
    for path, (cost, time, load) in n["trace_at_sink"]:
        lines.append(f"    at the sink: {_route(path)}  ({cost:g}, {time:g}, {load:g})")
    lines.append(
        f"    without dominance, {n['routes_without_dominance']} labels would reach the sink"
    )
    lines.append("    the library at the sink: " + _routes(n["library_at_sink"]))
    lines.append("    with upper_bound=-1e-9: " + _routes(n["library_negative"]))
    return "\n".join(lines)


def _route(path) -> str:
    return "→".join("0′" if n == toy.SINK else str(n) for n in path)


def _routes(found) -> str:
    return ", ".join(f"{_route(path)} ({cost:g})" for cost, path in found)
