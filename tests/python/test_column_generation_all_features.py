#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The package's column generation with many features at once, over each master solver:
two vehicle types, each its own graph and convexity row, priced by several stages each
(greedy, tabu, a bidirectional search with a half-way point, label buckets, the
incumbent prune, ng-routes, a backward A* without preprocessing), from the artificial
start, with relative tolerances, both hooks and the integer solve after (the threads,
which HiGHS fixes per process, are the benchmark's). The LP and the best plan are
checked against every route enumerated.

The benchmark's all-features test covers the features a benchmark file can set; this one adds
what only code can: several pricers and the hooks. A new feature of the package that the
benchmark cannot set adds itself here.
"""

import itertools
import math
import os
import sys
from collections.abc import Callable
from dataclasses import dataclass

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from column_generation_fakes import ScriptedPricer, make_path  # noqa: E402
from test_column_generation_end_to_end import (  # noqa: E402
    GurobipyModel,
    HighspyModel,
    UserModel,
    add_customer_rows,
    needs_gurobi,
    needs_highspy,
)

from rcspp import presets  # noqa: E402
from rcspp._core.graph import AlgorithmParams, Solution  # noqa: E402
from rcspp.column_generation import (  # noqa: E402
    ColumnGeneration,
    ColumnGenerationParams,
    ColumnGenerationStatus,
    ConvergenceAction,
    ConvergenceContext,
    ConvexityRow,
    DebugChecks,
    GraphPricer,
    IntegerParams,
    IterationInfo,
    MasterProblem,
    PricingChanges,
    PricingStage,
    SolverStatus,
)
from rcspp.graph import BucketAlgorithmParams, ResourceGraph  # noqa: E402

# Accuracy of the values checked: within what LP solvers return.
TOLERANCE = 1e-6

#: What the engine logs when a bidirectional search cannot use its half-way point.
HALF_WAY_POINT_OFF = "the half-way point is off"


# MARK: The instance


@dataclass(frozen=True)
class VehicleType:
    """Vehicles of one capacity, whose routes cost `cost_factor` times their distance;
    at most `max_routes` of them, or any number."""

    capacity: float
    cost_factor: float
    max_routes: float | None


#: Four customers (demands 3, 3, 5, 2): one large vehicle at most, as many small, dearer
#: ones as needed.
DEMANDS = [3.0, 3.0, 5.0, 2.0]
DISTANCES = [
    [0.0, 8.0, 5.0, 6.0, 7.0],
    [8.0, 0.0, 4.0, 10.0, 15.0],
    [5.0, 4.0, 0.0, 6.0, 12.0],
    [6.0, 10.0, 6.0, 0.0, 10.0],
    [7.0, 15.0, 12.0, 10.0, 0.0],
]
LARGE = VehicleType(capacity=10.0, cost_factor=1.0, max_routes=1.0)
SMALL = VehicleType(capacity=6.0, cost_factor=1.5, max_routes=None)
CUSTOMERS = len(DEMANDS)


@dataclass(frozen=True)
class Route:
    """A route of one vehicle type: the customers it serves and its cost."""

    customers: frozenset[int]
    cost: float


def every_route(vehicle: VehicleType) -> list[Route]:
    """Every route the vehicle can run, each customer at most once, each set of
    customers in its cheapest order."""
    routes: list[Route] = []

    for size in range(1, CUSTOMERS + 1):
        for served in itertools.combinations(range(1, CUSTOMERS + 1), size):
            if sum(DEMANDS[customer - 1] for customer in served) > vehicle.capacity:
                continue

            distance = min(
                sum(
                    DISTANCES[origin][destination]
                    for origin, destination in itertools.pairwise((0, *order, 0))
                )
                for order in itertools.permutations(served)
            )
            routes.append(Route(frozenset(served), vehicle.cost_factor * distance))

    return routes


def best_plan() -> float:
    """The cheapest set of routes that serves each customer once, with at most one large
    route."""
    best = math.inf
    large = [None, *every_route(LARGE)]
    small = every_route(SMALL)

    for first in large:
        left = frozenset(range(1, CUSTOMERS + 1)) - (first.customers if first else frozenset())
        cost = first.cost if first else 0.0
        best = min(best, cost + cheapest_partition(left, small))

    return best


def cheapest_partition(left: frozenset[int], routes: list[Route]) -> float:
    """The cheapest routes of `routes` that serve the customers `left` once each."""
    if not left:
        return 0.0

    first = min(left)
    return min(
        (
            route.cost + cheapest_partition(left - route.customers, routes)
            for route in routes
            if first in route.customers and route.customers <= left
        ),
        default=math.inf,
    )


def routing_graph(vehicle: VehicleType, memory: str) -> ResourceGraph:
    """The vehicle's routes as a pricing graph: the depot is the source 0 and the sink n
    + 1. Resources: the cost, the load, and the route's memory, ``"elementary"`` or.

    ``"ng"`` (each customer remembers every other: the same routes, by another resource).
    An arc into customer j covers master row j - 1.
    """
    sink = CUSTOMERS + 1
    graph = ResourceGraph()
    presets.add_cost_resource(graph)
    presets.add_capacity_resource(graph, "real", vehicle.capacity)

    if memory == "elementary":
        presets.add_elementary_resource(graph, "bitset", range(sink + 1))
    else:
        everyone = range(1, CUSTOMERS + 1)
        presets.add_ng_path_resource(graph, "bitset", {node: everyone for node in range(sink + 1)})

    graph.add_node(0, source=True)

    for customer in range(1, CUSTOMERS + 1):
        graph.add_node(customer)

    graph.add_node(sink, sink=True)

    for origin in range(CUSTOMERS + 1):
        for destination in range(1, sink + 1):
            place = 0 if destination == sink else destination  # the sink is the depot again

            if place == origin:
                continue

            cost = vehicle.cost_factor * DISTANCES[origin][place]
            demand = 0.0 if destination == sink else DEMANDS[destination - 1]
            rows = [] if destination == sink else [(destination - 1, 1.0)]
            graph.add_arc((cost, demand, set[int]()), origin, destination, cost, rows)

    return graph


# MARK: The masters


@pytest.fixture(
    params=[
        pytest.param(HighspyModel, marks=needs_highspy, id="highspy"),
        pytest.param(GurobipyModel, marks=needs_gurobi, id="gurobipy"),
    ]
)
def make_model(request: pytest.FixtureRequest) -> Callable[[], UserModel]:
    """Makes a new, empty user model of one kind."""
    model_type: Callable[[], UserModel] = request.param
    return model_type


def convexity(vehicle: VehicleType) -> ConvexityRow:
    return ConvexityRow(max_paths=vehicle.max_routes) if vehicle.max_routes else ConvexityRow()


def lp_of_every_route(model: UserModel) -> float:
    """The LP over every route of both types, no pricing: the value to reach."""
    add_customer_rows(model, CUSTOMERS)
    master = MasterProblem(model.solver())

    for vehicle in (LARGE, SMALL):
        columns: list[Solution] = [
            make_path(route.cost, [(customer - 1, 1.0) for customer in route.customers], 0.0)
            for route in every_route(vehicle)
        ]
        master.add_columns(master.add_pricer(ScriptedPricer(), convexity(vehicle)), columns)

    result = ColumnGeneration(master).run()
    assert result.status == ColumnGenerationStatus.Optimal
    assert result.objective is not None
    return result.objective


def large_stages() -> list[PricingStage]:
    """Greedy (capped), tabu, a bidirectional search split half-way on the load, then
    pulling over label buckets with the incumbent prune: the last proves."""
    greedy = AlgorithmParams()
    greedy.max_solutions = 3
    tabu = AlgorithmParams()
    tabu.max_iterations = 50
    halves = AlgorithmParams()
    halves.critical_resource_index = 1  # the load, the second real resource
    halves.half_way_point = LARGE.capacity / 2
    buckets = BucketAlgorithmParams(bucket_width=2, bucket_resource_pos=1)
    buckets.prune_by_incumbent = True
    return [
        PricingStage("greedy", greedy),
        PricingStage("tabu", tabu),
        PricingStage("simple", halves, direction="bidirectional"),
        PricingStage("pulling", buckets),
    ]


def small_stages() -> list[PricingStage]:
    """A backward A* alone."""
    return [PricingStage("astar", direction="backward")]


# MARK: The test


def test_every_feature_together_gives_the_lp_and_a_plan(
    make_model: Callable[[], UserModel], capfd: pytest.CaptureFixture[str]
) -> None:
    lp = lp_of_every_route(make_model())

    model = make_model()
    add_customer_rows(model, CUSTOMERS)
    master = MasterProblem(model.solver())
    large = GraphPricer(routing_graph(LARGE, "elementary"), large_stages())
    small = GraphPricer(routing_graph(SMALL, "ng"), small_stages(), preprocess=False)
    master.add_pricer(large, convexity(LARGE))
    master.add_pricer(small, convexity(SMALL))

    # The hooks: every iteration kept; at the first convergence, the pricers are told that
    # their problem changed (it did not) and the loop goes on, then it stops.
    seen: list[IterationInfo] = []
    converged: list[ConvergenceContext] = []

    def on_converged(context: ConvergenceContext) -> ConvergenceAction:
        converged.append(context)

        if len(converged) == 1:
            for entry in context.master.pricers:
                entry.pricer.update(PricingChanges(structure=True))

            return ConvergenceAction.Continue

        return ConvergenceAction.Stop

    # Every debug check on: the master solver and the pricers agree at every iteration.
    params = ColumnGenerationParams(
        tolerances="relative",
        on_iteration=seen.append,
        on_converged=on_converged,
        debug_checks=DebugChecks(
            pricer_reduced_costs=True, dual_signs=True, master_reduced_costs=True, objective=True
        ),
    )
    loop = ColumnGeneration(master, params)
    result = loop.run()

    # The LP of every route, reached by pricing from the artificial columns alone.
    assert result.status == ColumnGenerationStatus.Optimal
    assert result.objective == pytest.approx(lp, rel=TOLERANCE)
    assert result.lower_bound == pytest.approx(lp, rel=TOLERANCE)
    assert result.statistics.artificial_columns == CUSTOMERS  # the start's, none left in use

    # Every iteration reached the iteration hook, each bound below the LP; the convergence
    # hook ran twice, the second time on the same LP.
    assert [info.iteration for info in seen] == list(range(1, len(seen) + 1))
    assert all(info.lower_bound <= lp + TOLERANCE * abs(lp) for info in seen)
    assert seen[-1].objective == pytest.approx(lp, rel=TOLERANCE)
    assert len(converged) == 2
    assert converged[1].objective == pytest.approx(lp, rel=TOLERANCE)
    assert len(converged[1].duals) == CUSTOMERS + 1  # the customers, the large type's row

    # The plan over the columns found costs at least the best plan, and at least the LP.
    assert loop.solve_restricted_master_integer(IntegerParams()) == SolverStatus.Optimal
    plan = model.solver().objective()
    assert plan >= best_plan() - TOLERANCE
    assert plan >= lp - TOLERANCE

    # The bidirectional search split at its half-way point. The engine logs its fallback once
    # per process, so after another test's fallback this check sees nothing; alone, it does.
    assert HALF_WAY_POINT_OFF not in capfd.readouterr().out
