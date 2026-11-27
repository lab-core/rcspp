#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""A Solomon instance drawn, and what lies on it.

A drawing is the instance (:func:`draw_instance`: the depot, the customers by demand), then
overlays, each a call on the same axes with its own data:

- :func:`draw_paths`: node sequences, a colour each (a solution's routes; later the columns an
  iteration added, a negative cycle);
- :func:`draw_arc_values`: arcs with a value, by width and shade (an LP's flow on each arc);
- :func:`draw_node_marks`: nodes marked (the customers no route visits).

A new overlay is a new function of the same kind, or new data for one of these. The time
windows have a chart of their own (:func:`draw_windows`), a row per route.

Nodes are the graph's: 0 the depot, 1..n the customers, n + 1 the sink, drawn at the depot.
"""

from collections.abc import Mapping, Sequence
from typing import Any

from column_generation_benchmark.instances import SolomonInstance, distance_between

#: Customer numbers are shown up to this many customers, unless told otherwise.
LABELS_UP_TO = 30


def position(instance: SolomonInstance, node: int) -> tuple[float, float]:
    """Where a node is: the sink is the depot."""
    customer = instance.customers[0 if node == instance.sink else node]
    return customer.x, customer.y


def draw_instance(axes: Any, instance: SolomonInstance, labels: bool | None = None) -> None:
    """The depot as a square, each customer as a dot of area by its demand, and their
    numbers (by default up to :data:`LABELS_UP_TO` customers)."""
    depot, customers = instance.customers[0], instance.customers[1:]
    largest = max((customer.demand for customer in customers), default=0.0) or 1.0
    axes.scatter(
        [customer.x for customer in customers],
        [customer.y for customer in customers],
        s=[10 + 60 * customer.demand / largest for customer in customers],
        color="0.55",
        zorder=3,
    )
    axes.scatter([depot.x], [depot.y], marker="s", s=80, color="black", zorder=4)

    if labels if labels is not None else len(customers) <= LABELS_UP_TO:
        for customer in customers:
            axes.annotate(
                str(customer.number),
                (customer.x, customer.y),
                textcoords="offset points",
                xytext=(4, 4),
                fontsize="x-small",
                color="0.3",
            )

    axes.set_aspect("equal", adjustable="datalim")
    axes.set_title(instance.name)


def draw_paths(
    axes: Any,
    instance: SolomonInstance,
    paths: Sequence[Sequence[int]],
    labels: Sequence[str] | None = None,
) -> None:
    """Each path as a line through its nodes, a colour each; in the legend with
    `labels`."""
    for index, path in enumerate(paths):
        points = [position(instance, node) for node in path]
        axes.plot(
            [x for x, _ in points],
            [y for _, y in points],
            color=f"C{index % 10}",
            linewidth=1.5,
            label=None if labels is None else labels[index],
            zorder=2,
        )

    if labels is not None:
        axes.legend(fontsize="x-small")


def draw_arc_values(
    axes: Any, instance: SolomonInstance, values: Mapping[tuple[int, int], float]
) -> None:
    """Each arc as a line whose width and shade grow with its value, against the largest
    one."""
    largest = max(values.values(), default=0.0) or 1.0

    for (start, end), value in values.items():
        share = value / largest
        points = [position(instance, start), position(instance, end)]
        axes.plot(
            [x for x, _ in points],
            [y for _, y in points],
            color="C0",
            linewidth=0.5 + 3.0 * share,
            alpha=0.25 + 0.75 * share,
            zorder=2,
        )


def draw_node_marks(
    axes: Any, instance: SolomonInstance, nodes: Sequence[int], label: str | None = None
) -> None:
    """Each node as a red cross, in the legend with `label`."""
    if not nodes:
        return

    points = [position(instance, node) for node in nodes]
    axes.scatter(
        [x for x, _ in points],
        [y for _, y in points],
        marker="x",
        s=60,
        color="red",
        label=label,
        zorder=5,
    )

    if label is not None:
        axes.legend(fontsize="x-small")


def unvisited(instance: SolomonInstance, paths: Sequence[Sequence[int]]) -> list[int]:
    """The customers no path visits."""
    visited = {node for path in paths for node in path}
    return [
        customer.number for customer in instance.customers[1:] if customer.number not in visited
    ]


def arc_values(columns: Sequence[Mapping[str, Any]]) -> dict[tuple[int, int], float]:
    """The flow on each arc: the values of the columns whose path takes it, summed."""
    found: dict[tuple[int, int], float] = {}

    for column in columns:
        path: list[int] = column.get("path") or []

        for start, end in zip(path, path[1:]):
            found[(start, end)] = found.get((start, end), 0.0) + float(column["value"])

    return found


def visits(instance: SolomonInstance, route: Sequence[int]) -> list[dict[str, Any]]:
    """Each customer of a route (customer numbers, without the depot) with its window,
    its arrival, the start of its service (the arrival, or its ready time if later) and the
    end: as the solution's check times it."""
    found: list[dict[str, Any]] = []
    previous = instance.customers[0]
    time = previous.ready_time

    for number in route:
        customer = instance.customers[number]
        arrival = time + previous.service_time + distance_between(previous, customer)
        start = max(customer.ready_time, arrival)
        found.append(
            {
                "customer": number,
                "ready_time": customer.ready_time,
                "due_time": customer.due_time,
                "arrival": arrival,
                "start": start,
                "end": start + customer.service_time,
            }
        )
        previous, time = customer, start

    return found


def draw_windows(axes: Any, instance: SolomonInstance, routes: Sequence[Sequence[int]]) -> None:
    """A row per route: each customer's window as a light bar, its service as a dark one
    from its start, its arrival as a dot; the depot's due time as a dashed line."""
    for row, route in enumerate(routes):
        colour = f"C{row % 10}"

        for visit in visits(instance, route):
            axes.barh(
                row,
                visit["due_time"] - visit["ready_time"],
                left=visit["ready_time"],
                height=0.6,
                color=colour,
                alpha=0.25,
            )
            axes.barh(
                row, visit["end"] - visit["start"], left=visit["start"], height=0.3, color=colour
            )
            axes.plot([visit["arrival"]], [row], marker="o", markersize=3, color="black")

    axes.axvline(instance.customers[0].due_time, color="black", linestyle="--", linewidth=0.8)
    axes.set_yticks(range(len(routes)), [f"route {row + 1}" for row in range(len(routes))])
    axes.invert_yaxis()
    axes.set_xlabel("time")
    axes.set_title(f"{instance.name}: windows (light), service (dark), arrivals (dots)")
