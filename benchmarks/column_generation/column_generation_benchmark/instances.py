#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The problems the runners solve.

An :class:`Instance` reads its file and gives what column generation needs: the pricing graph,
the bounds of the master's rows, and starting columns of its own kind (one route per customer
is a vehicle routing idea, so it belongs here). It also turns the integer master's columns into
a solution of its own kind and checks it against its file: only the instance knows what a column
means. An instance block's ``type`` picks the class in :data:`INSTANCE_TYPES`.
"""

from __future__ import annotations

import math
from abc import ABC, abstractmethod
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING, Any

from .source_tree import TREE

if TYPE_CHECKING:
    from rcspp._core.graph import Solution
    from rcspp.graph import ResourceGraph

#: Where an instance named in the options is read from.
INSTANCES_DIRECTORY = TREE / "instances"


class InstanceError(Exception):
    """An instance that cannot be read, or asked for something it does not have."""


class Instance(ABC):
    """A problem: its pricing graph, its master's rows and its own starting columns."""

    #: The instance's name, as runs and summaries show it.
    name: str

    #: The starts of its own kind, by a start block's type: checked before any run.
    STARTS: tuple[str, ...] = ()

    #: Whether its runs solve the integer master after column generation, and the type of
    #: the generated columns there, unless a run's ``integer`` block says otherwise.
    INTEGER: dict[str, Any] = {"solve": False, "column_type": "integer"}

    @abstractmethod
    def build_graph(self, options: Mapping[str, Any]) -> ResourceGraph:
        """The pricing graph, built with the run's ``graph`` options; its arcs give the
        rows they cover."""

    @abstractmethod
    def row_bounds(self) -> tuple[list[float], list[float]]:
        """The lower and upper bound of every master row, by row index."""

    @abstractmethod
    def starting_columns(self, start: str) -> list[Solution]:
        """The columns a start of type `start` adds before the first master solve, each
        a solution with its column set.

        Raises:
            InstanceError: When the instance has no such start.
        """

    @abstractmethod
    def resolve_solution(self, columns: Sequence[Mapping[str, Any]]) -> dict[str, Any]:
        """The instance's own solution from the integer master's columns, as
        ``solution.json`` lists them (``value``, ``cost``, ``path``)."""

    @abstractmethod
    def check_solution(self, solution: Mapping[str, Any], objective: float) -> list[str]:
        """What is wrong with a solution :meth:`resolve_solution` gave, checked against
        the instance's file and the integer master's `objective`; empty when nothing
        is."""


# MARK: Solomon VRPTW


@dataclass(frozen=True)
class Customer:
    """A customer of a Solomon file; customer 0 is the depot."""

    number: int
    x: float
    y: float
    demand: float
    ready_time: float
    due_time: float
    service_time: float


def distance_between(first: Customer, second: Customer) -> float:
    """The Euclidean distance between two customers, unrounded."""
    return math.sqrt((second.x - first.x) ** 2 + (second.y - first.y) ** 2)


class SolomonInstance(Instance):
    """A vehicle routing problem with time windows in Solomon's format.

    The graph's nodes are the customers by number, the depot 0 as the source and a sink after
    the last customer. An arc's cost is its distance; its resources are the distance, the time
    (travel and service, within the windows) and the load (within the capacity); an arc out of
    customer ``i`` covers row ``i``. Row 0 is free, every other row is covered exactly once. The
    empty route, depot to sink, is left out: its column covers nothing.
    """

    STARTS = ("single-customer",)
    INTEGER = {"solve": True, "column_type": "binary"}

    #: How far a recomputed time or distance may be from the one given.
    TOLERANCE = 1e-6

    def __init__(self, path: str | Path, name: str | None = None):
        self.path = Path(path)
        self.name = name or self.path.stem

        try:
            text = self.path.read_text()
        except OSError as error:
            raise InstanceError(f"instance {self.name}: {error}") from None

        self.vehicles, self.capacity, self.customers = self.parse(text)

    @classmethod
    def from_options(cls, block: Mapping[str, Any]) -> SolomonInstance:
        """The instance a block names: ``instances/<name>.txt`` of the tree."""
        return cls(INSTANCES_DIRECTORY / f"{block['name']}.txt", block["name"])

    def parse(self, text: str) -> tuple[int, float, list[Customer]]:
        """The number of vehicles, their capacity and the customers of a Solomon file: the
        first line with two numbers, then every line with seven."""
        vehicles: tuple[int, float] | None = None
        customers: list[Customer] = []

        for line in text.splitlines()[1:]:
            fields = line.split()

            try:
                numbers = [float(field) for field in fields]
            except ValueError:
                continue

            if len(numbers) == 2 and vehicles is None:
                vehicles = (int(numbers[0]), numbers[1])
            elif len(numbers) == 7:
                customers.append(Customer(int(numbers[0]), *numbers[1:]))

        if vehicles is None or not customers:
            raise InstanceError(f"instance {self.name}: no vehicles or no customers")

        if [customer.number for customer in customers] != list(range(len(customers))):
            raise InstanceError(f"instance {self.name}: customers must be numbered 0, 1, 2, ...")

        return vehicles[0], vehicles[1], customers

    @property
    def sink(self) -> int:
        """The sink's node: the number after the last customer."""
        return len(self.customers)

    def build_graph(self, options: Mapping[str, Any]) -> ResourceGraph:
        from rcspp._core.graph import Row
        from rcspp.graph import ResourceGraph
        from rcspp.resource import (
            AdditionExtensionFunction,
            MinMaxFeasibilityFunction,
            TimeWindowExtensionFunction,
            TimeWindowFeasibilityFunction,
            TrivialCostFunction,
            TrivialFeasibilityFunction,
            ValueCostFunction,
            ValueDominanceFunction,
        )

        windows = {
            customer.number: (customer.ready_time, customer.due_time) for customer in self.customers
        }
        # The sink is the depot again: a route is back before it closes, as the check wants.
        windows[self.sink] = windows[self.customers[0].number]
        graph = ResourceGraph()

        # Distance, the cost; time, within the windows; load, within the capacity.
        graph.add_real_resource(
            AdditionExtensionFunction(),
            TrivialFeasibilityFunction(),
            ValueCostFunction(),
            ValueDominanceFunction(),
        )
        graph.add_real_resource(
            TimeWindowExtensionFunction(windows),
            TimeWindowFeasibilityFunction(windows),
            TrivialCostFunction(),
            ValueDominanceFunction(),
        )
        graph.add_real_resource(
            AdditionExtensionFunction(),
            MinMaxFeasibilityFunction(0.0, self.capacity),
            TrivialCostFunction(),
            ValueDominanceFunction(),
        )

        depot = self.customers[0]

        for customer in self.customers:
            graph.add_node(customer.number, customer is depot)

            if customer is depot:
                graph.add_node(self.sink, False, True)

        for origin in self.customers:
            rows: list[Row] = [] if origin is depot else [Row(origin.number, 1.0)]
            targets = [(customer.number, customer) for customer in self.customers[1:]]

            if origin is not depot:
                targets.append((self.sink, depot))

            for node, destination in targets:
                if destination is origin:
                    continue

                length = distance_between(origin, destination)
                consumption = (length, origin.service_time + length, destination.demand)
                graph.add_arc(consumption, origin.number, node, length, rows)

        return graph

    def row_bounds(self) -> tuple[list[float], list[float]]:
        lower = [-math.inf] + [1.0] * (len(self.customers) - 1)
        upper = [math.inf] + [1.0] * (len(self.customers) - 1)
        return lower, upper

    def starting_columns(self, start: str) -> list[Solution]:
        if start not in self.STARTS:
            raise InstanceError(f"instance {self.name}: no {start!r} start")

        from rcspp._core.graph import Column, Row, Solution

        depot = self.customers[0]
        routes: list[Solution] = []

        # Depot, customer, depot: twice the distance to the depot.
        for customer in self.customers[1:]:
            column = Column()
            column.cost = 2 * math.hypot(customer.x - depot.x, customer.y - depot.y)
            column.rows = [Row(index=customer.number, coefficient=1.0)]
            route = Solution()
            route.cost = 0.0
            route.path_node_ids = [depot.number, customer.number, self.sink]
            route.path_arc_ids = []
            route.column = column
            routes.append(route)

        return routes

    def resolve_solution(self, columns: Sequence[Mapping[str, Any]]) -> dict[str, Any]:
        """The routes as customer sequences (a column's path without the depot and the
        sink, once per unit of its value), the vehicles they need and the total distance
        the columns cost."""
        routes: list[list[int]] = []
        distance = 0.0

        for column in columns:
            # The solvers give integer values up to a few ulps: 1.0000000000000002.
            times = round(column["value"])
            routes += [list(column["path"][1:-1]) for _ in range(times)]
            distance += times * column["cost"]

        return {"routes": routes, "vehicles": len(routes), "distance": distance}

    def check_solution(self, solution: Mapping[str, Any], objective: float) -> list[str]:
        """Each customer served once, every route within the time windows (back at the
        depot by its due time) and the capacity, and the distance recomputed from the
        coordinates equal to the solution's and to `objective`."""
        depot = self.customers[0]
        errors: list[str] = []
        visits = [0] * len(self.customers)
        distance = 0.0
        routes: list[list[int]] = solution["routes"]

        for index, route in enumerate(routes):
            if not route:
                errors.append(f"route {index} is empty")
                continue

            unknown = [number for number in route if not 0 < number < len(self.customers)]

            if unknown:
                errors.append(f"route {index} visits {unknown}, not customers")
                continue

            load = 0.0
            time = depot.ready_time
            previous = depot

            for number in route:
                customer = self.customers[number]
                visits[number] += 1
                load += customer.demand
                length = distance_between(previous, customer)
                distance += length
                time = max(customer.ready_time, time + previous.service_time + length)

                if time > customer.due_time + self.TOLERANCE:
                    errors.append(
                        f"route {index} reaches customer {number} at {time:g}, "
                        f"after its due time {customer.due_time:g}"
                    )

                previous = customer

            length = distance_between(previous, depot)
            distance += length
            back = time + previous.service_time + length

            if back > depot.due_time + self.TOLERANCE:
                errors.append(
                    f"route {index} is back at the depot at {back:g}, "
                    f"after its due time {depot.due_time:g}"
                )

            if load > self.capacity + self.TOLERANCE:
                errors.append(
                    f"route {index} carries {load:g}, over the capacity {self.capacity:g}"
                )

        for number, count in enumerate(visits[1:], start=1):
            if count != 1:
                errors.append(f"customer {number} is served {count} times")

        for name, given in (
            ("the solution's distance", solution["distance"]),
            ("the objective", objective),
        ):
            if abs(given - distance) > self.TOLERANCE * max(1.0, abs(distance)):
                errors.append(f"{name} {given!r} is not the recomputed distance {distance!r}")

        return errors


# MARK: Types

#: The instance types, by an instance block's ``type``.
INSTANCE_TYPES: dict[str, Any] = {"solomon": SolomonInstance}


def make_instance(block: Mapping[str, Any]) -> Instance:
    """The instance an expanded instance block describes.

    Raises:
        InstanceError: When its file cannot be read.
    """
    instance: Instance = INSTANCE_TYPES[block["type"]].from_options(block)
    return instance
