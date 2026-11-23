#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Column generation for a fleet of several vehicle types: one pricing graph and one pricer
per type, all over the same customer rows.

Each type has its capacity, its costs (fixed per route, and per distance), and may limit
how long its routes are and how many of its vehicles the plan uses. The master chooses the
routes, so it also chooses the fleet: how many vehicles of each type are worth using.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import TYPE_CHECKING

from vrp.column_generation import Route, integer_routes, master_solver
from vrp.graph import pricing_graph, sink
from vrp.instance import Instance, distance

from rcspp._core.graph import AlgorithmParams, Column, Row, Solution
from rcspp.column_generation import (
    ArtificialColumns,
    ColumnGeneration,
    ColumnGenerationParams,
    ConvexityRow,
    GraphPricer,
    MasterProblem,
    NoInitialization,
    PricingStage,
    SolverStatus,
)

if TYPE_CHECKING:
    from collections.abc import Callable

    from rcspp.column_generation import IterationInfo


@dataclass(frozen=True)
class VehicleType:
    """A kind of vehicle: what it carries, what it costs, and its limits."""

    name: str
    capacity: float
    fixed_cost: float = 0.0
    cost_per_distance: float = 1.0
    max_distance: float = math.inf
    #: The most vehicles of this type in the plan; none by default.
    max_vehicles: int | None = None

    def route_cost(self, length: float) -> float:
        return self.fixed_cost + self.cost_per_distance * length


# MARK: Reading a vehicle type

# The settings of a vehicle type on the command line.
SETTINGS = ("capacity", "fixed", "per-distance", "max-distance", "max")

# Fleets ready to use, as command-line vehicle types. The capacities are shares of the
# instance's, so a fleet fits every Solomon class (capacity 200 in C1, 1000 in R2). "single" is
# the plain VRP. The larger vehicle costs more to use and to drive, and pays off when its load
# saves routes: on C101_25, truck-and-van uses 2 trucks and 2 vans; on R101_25, whose time
# windows keep the routes short and light, vans only.
FLEETS = {
    "single": ["vehicle"],
    "truck-and-van": [
        "truck capacity=100% fixed=100 per-distance=1.2",
        "van capacity=30% fixed=20",
    ],
    "three-sizes": [
        "truck capacity=100% fixed=100 per-distance=1.2",
        "van capacity=30% fixed=20",
        "bike capacity=10% fixed=10 max-distance=60",
    ],
}
DEFAULT_FLEET = "truck-and-van"


def parse_vehicle_type(text: str, instance_capacity: float) -> VehicleType:
    """A vehicle type read from text: its name, then its settings.

    For example ``"van capacity=50% fixed=40 max=3"``.

    The settings: ``capacity`` (a number, or a share of ``instance_capacity`` such as
    ``50%``; the instance's by default), ``fixed`` (the cost of each route),
    ``per-distance``, ``max-distance`` (the longest route) and ``max`` (the most vehicles
    of the type).

    Raises:
        ValueError: When a setting is unknown, repeated or not a number.
    """
    name, *settings = text.split()

    if "=" in name:
        raise ValueError(f"vehicle type {text!r}: its name comes first")

    # The settings as text, each at most once.
    values: dict[str, str] = {}

    for setting in settings:
        key, _, value = setting.partition("=")

        if key not in SETTINGS:
            raise ValueError(
                f"vehicle type {name!r}: unknown setting {key!r}, expected one of "
                f"{', '.join(SETTINGS)}"
            )

        if key in values:
            raise ValueError(f"vehicle type {name!r}: {key} given twice")

        values[key] = value

    # Then each one as a number, or its default.
    capacity = instance_capacity

    if "capacity" in values:
        text = values["capacity"]

        if text.endswith("%"):
            capacity = instance_capacity * read_number(name, "capacity", text[:-1]) / 100.0
        else:
            capacity = read_number(name, "capacity", text)

    max_vehicles = None

    if "max" in values:
        number = read_number(name, "max", values["max"])

        if not number.is_integer():
            raise ValueError(f"vehicle type {name!r}: max={values['max']!r} is not a whole number")

        max_vehicles = int(number)

    return VehicleType(
        name,
        capacity,
        read_number(name, "fixed", values.get("fixed", "0")),
        read_number(name, "per-distance", values.get("per-distance", "1")),
        read_number(name, "max-distance", values.get("max-distance", "inf")),
        max_vehicles,
    )


def read_number(name: str, key: str, text: str) -> float:
    """The number in ``text``, the value of setting ``key`` of vehicle type ``name``.

    Raises:
        ValueError: When it is not a number.
    """
    try:
        return float(text)
    except ValueError:
        raise ValueError(f"vehicle type {name!r}: {key}={text!r} is not a number") from None


def describe(vehicle_type: VehicleType) -> str:
    """One line for the output: the type and its settings."""
    parts = [
        f"capacity {vehicle_type.capacity:g}",
        f"fixed cost {vehicle_type.fixed_cost:g}",
        f"{vehicle_type.cost_per_distance:g} per distance",
    ]

    if vehicle_type.max_distance != math.inf:
        parts.append(f"routes up to {vehicle_type.max_distance:g} long")

    if vehicle_type.max_vehicles is not None:
        parts.append(f"at most {vehicle_type.max_vehicles}")

    return f"{vehicle_type.name}: {', '.join(parts)}"


# MARK: Column generation


@dataclass(frozen=True)
class FleetRoute:
    """A route of the integer plan, with the vehicle that drives it."""

    vehicle_type: VehicleType
    customers: list[int]
    cost: float
    distance: float
    load: float


def route_distance(instance: Instance, customers: list[int]) -> float:
    """The length of the route from the depot through ``customers`` and back."""
    stops = [instance.depot, *(instance.customers[number] for number in customers), instance.depot]
    return sum(distance(first, second) for first, second in zip(stops, stops[1:]))


def starting_routes(
    instance: Instance, vehicle_types: list[VehicleType]
) -> dict[int, list[Solution]]:
    """For each customer, the route depot, customer, depot on the cheapest type that can
    drive it and has no vehicle limit (a limited type could not drive one route per
    customer).

    Returns the routes of each type, by the type's index; a customer that no such type
    can serve alone has no starting route.
    """
    depot = instance.depot
    routes: dict[int, list[Solution]] = {}

    for customer in instance.customers[1:]:
        length = 2 * distance(depot, customer)
        candidates = [
            (vehicle_type.route_cost(length), index)
            for index, vehicle_type in enumerate(vehicle_types)
            if vehicle_type.max_vehicles is None
            and customer.demand <= vehicle_type.capacity
            and length <= vehicle_type.max_distance
        ]

        if not candidates:
            continue

        cost, index = min(candidates)
        column = Column()
        column.cost = cost
        column.rows = [Row(index=customer.number - 1, coefficient=1.0)]

        route = Solution()
        route.path_node_ids = [depot.number, customer.number, sink(instance)]
        route.column = column
        routes.setdefault(index, []).append(route)

    return routes


def column_generation(
    instance: Instance,
    vehicle_types: list[VehicleType],
    master: str,
    max_solutions: int | None = None,
    timeout_s: float = math.inf,
    on_iteration: Callable[[IterationInfo], None] | None = None,
) -> tuple[MasterProblem, ColumnGeneration]:
    """The master over the routes of every vehicle type, and the loop that solves its
    LP.

    Pricer ``k`` prices the routes of ``vehicle_types[k]`` in its own graph; a type with
    ``max_vehicles`` gets the convexity row "at most that many of its routes".

    Args:
        instance: The customers.
        vehicle_types: The fleet; at least one type.
        master: ``"highspy"`` or ``"gurobipy"``.
        max_solutions: The most routes each pricing call returns; ``None`` returns every
            improving route found.
        timeout_s: The run's time limit, in seconds.
        on_iteration: Called after each iteration, e.g. to print it.
    """
    if not vehicle_types:
        raise ValueError("column_generation: no vehicle type")

    problem = MasterProblem(master_solver(master, len(instance.customers) - 1))
    params = AlgorithmParams()

    if max_solutions is not None:
        params.max_solutions = max_solutions

    # One graph and one pricer per type, in the order given: pricer k is vehicle_types[k].
    for vehicle_type in vehicle_types:
        graph = pricing_graph(
            instance,
            vehicle_type.capacity,
            vehicle_type.fixed_cost,
            vehicle_type.cost_per_distance,
            vehicle_type.max_distance,
        )
        convexity = ConvexityRow(
            max_paths=(
                None if vehicle_type.max_vehicles is None else float(vehicle_type.max_vehicles)
            )
        )
        problem.add_pricer(GraphPricer(graph, [PricingStage("simple", params)]), convexity)

    # Starting routes when every customer has one; otherwise, artificial columns make the
    # first LP feasible, and the loop prices them out.
    routes = starting_routes(instance, vehicle_types)

    for index, columns in routes.items():
        problem.add_columns(index, columns)

    covered = sum(len(columns) for columns in routes.values())
    every_customer = covered == len(instance.customers) - 1
    options = ColumnGenerationParams(
        initialization=NoInitialization() if every_customer else ArtificialColumns(),
        timeout_s=timeout_s,
        on_iteration=on_iteration,
    )
    return problem, ColumnGeneration(problem, options)


def integer_plan(
    loop: ColumnGeneration,
    problem: MasterProblem,
    instance: Instance,
    vehicle_types: list[VehicleType],
    timeout_s: float = 60.0,
) -> tuple[SolverStatus, list[FleetRoute]]:
    """The integer plan over the routes generated so far (see
    :func:`vrp.column_generation.integer_routes`), each route with its vehicle type."""
    status, routes = integer_routes(loop, problem, timeout_s)
    return status, [fleet_route(instance, vehicle_types, route) for route in routes]


def fleet_route(instance: Instance, vehicle_types: list[VehicleType], route: Route) -> FleetRoute:
    load = sum(instance.customers[number].demand for number in route.customers)
    return FleetRoute(
        vehicle_types[route.pricer],
        route.customers,
        route.distance,  # the column's cost
        route_distance(instance, route.customers),
        load,
    )
