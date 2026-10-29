#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Column generation end to end from Python, with the values of the C++ end-to-end tests
(tests/cpp/test_column_generation_end_to_end.hpp).

The master is a model the user built for their LP solver; each kind of model is a
UserModel, and every test runs over each of them. A solver missing from this build is
skipped.
"""

import importlib.util
import math
import os
import sys
from typing import Callable, Protocol

import numpy as np
import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from column_generation_fakes import ScriptedPricer, make_path  # noqa: E402

import rcspp.column_generation as column_generation  # noqa: E402
from rcspp import presets  # noqa: E402
from rcspp._core.graph import Solution  # noqa: E402
from rcspp.column_generation import (  # noqa: E402
    ColumnGeneration,
    ColumnGenerationResult,
    ColumnGenerationStatus,
    ConvexityRow,
    GraphPricer,
    IntegerParams,
    MasterProblem,
    MasterSolver,
    SolverStatus,
)
from rcspp.graph import ResourceGraph  # noqa: E402

# Accuracy of the values checked: within what LP solvers return.
TOLERANCE = 1e-6


# ── The instances ────────────────────────────────────────────────────────────


class Toy:
    """Three customers of demands 4, 5 and 6, each visited exactly once by vehicles of
    capacity 11, so a route serves one or two of them: {1}, {2}, {3}, {1, 2}, {1, 3},
    {2, 3}.

    From the depot to any customer is 10; customers 1 and 2 are 4 apart, 1 and 3 are 5,
    2 and 3 are 6. The LP optimum 37.5 takes half of each two-customer route; its duals
    (11.5, 12.5, 13.5) are unique. The best plan serves {1, 2} and {3}, at 24 + 20 = 44.
    """

    customers = 3
    capacity = 11.0
    demands = [4.0, 5.0, 6.0]
    distances = [
        [0.0, 10.0, 10.0, 10.0],
        [10.0, 0.0, 4.0, 5.0],
        [10.0, 4.0, 0.0, 6.0],
        [10.0, 5.0, 6.0, 0.0],
    ]
    served = [[0], [1], [2], [0, 1], [0, 2], [1, 2]]
    route_costs = [20.0, 20.0, 20.0, 24.0, 25.0, 26.0]
    lp = 37.5
    duals = [11.5, 12.5, 13.5]
    lp_values = [0.0, 0.0, 0.0, 0.5, 0.5, 0.5]
    best_plan = 44.0
    best_plan_values = [0.0, 0.0, 1.0, 1.0, 0.0, 0.0]

    @classmethod
    def routes(cls) -> list[Solution]:
        """Every route, in the order above; they get the ids 0 to 5."""
        return [
            make_path(cost, [(customer, 1.0) for customer in served], 0.0)
            for cost, served in zip(cls.route_costs, cls.served)
        ]


class FourCustomers:
    """Four customers whose LP optimum, 40, needs routes that visit each customer at most
    once: with capacity 10 and demands of 3, a route could serve a customer twice, and the
    LP drop to 37."""

    capacity = 10.0
    demands = [3.0, 3.0, 5.0, 2.0]
    distances = [
        [0.0, 8.0, 5.0, 6.0, 7.0],
        [8.0, 0.0, 4.0, 10.0, 15.0],
        [5.0, 4.0, 0.0, 6.0, 12.0],
        [6.0, 10.0, 6.0, 0.0, 10.0],
        [7.0, 15.0, 12.0, 10.0, 0.0],
    ]
    lp = 40.0


def routing_graph(
    capacity: float, demands: list[float], distances: list[list[float]]
) -> ResourceGraph:
    """Vehicle routes as a pricing graph.

    The depot is the source 0 and the sink n + 1, the
    customers are the nodes 1 to n. Resources: the cost, the load (at most `capacity`) and
    the customers visited, so that a route visits each one at most once. An arc into
    customer j covers master row j − 1.
    """
    customers = len(demands)
    sink = customers + 1
    graph = ResourceGraph()
    presets.add_cost_resource(graph)
    presets.add_capacity_resource(graph, "real", capacity)
    presets.add_elementary_resource(graph, "bitset", range(sink + 1))
    graph.add_node(0, source=True)
    for customer in range(1, customers + 1):
        graph.add_node(customer)
    graph.add_node(sink, sink=True)
    for origin in range(customers + 1):
        for destination in range(1, sink + 1):
            place = 0 if destination == sink else destination  # the sink is the depot again
            if place == origin:
                continue  # no loop, and no empty route
            distance = distances[origin][place]
            demand = 0.0 if destination == sink else demands[destination - 1]
            rows = [] if destination == sink else [(destination - 1, 1.0)]
            # The visited set ignores its arc value: it reads the arc's endpoints.
            graph.add_arc((distance, demand, set[int]()), origin, destination, distance, rows)
    return graph


# ── The users' models ────────────────────────────────────────────────────────


class UserModel(Protocol):
    """A master the user built in their LP solver: rows first, then its master solver."""

    def add_row(self, lower: float, upper: float, name: str) -> None:
        """A row the user builds, empty: the columns fill it."""
        ...

    def solver(self) -> MasterSolver:
        """The master solver over the rows added."""
        ...


class HighsFromArrays:
    """The user's rows handed to HighsMasterSolver as arrays (no column of their
    own)."""

    def __init__(self) -> None:
        self.lower: list[float] = []
        self.upper: list[float] = []
        self._solver: MasterSolver | None = None

    def add_row(self, lower: float, upper: float, name: str) -> None:
        assert self._solver is None, "rows come before the master solver"
        self.lower.append(lower)
        self.upper.append(upper)

    def solver(self) -> MasterSolver:
        if self._solver is None:
            self._solver = column_generation.HighsMasterSolver(
                row_lower=self.lower, row_upper=self.upper
            )
        return self._solver


class HighspyModel:
    """The user's rows in a highspy model, copied by HighspyMasterSolver."""

    def __init__(self) -> None:
        import highspy

        self.model = highspy.Highs()
        self.model.setOptionValue("output_flag", False)
        self._solver: MasterSolver | None = None

    def add_row(self, lower: float, upper: float, name: str) -> None:
        assert self._solver is None, "rows come before the master solver"
        self.model.addRow(lower, upper, 0, [], [])
        self.model.passRowName(self.model.getNumRow() - 1, name)

    def solver(self) -> MasterSolver:
        if self._solver is None:
            self._solver = column_generation.HighspyMasterSolver(self.model)
        return self._solver


def has_highspy() -> bool:
    """Whether highspy is installed."""
    return importlib.util.find_spec("highspy") is not None


@pytest.fixture(
    params=[
        pytest.param(
            HighsFromArrays,
            marks=pytest.mark.skipif(
                not hasattr(column_generation, "HighsMasterSolver"),
                reason="rcspp built without HiGHS",
            ),
            id="highs-from-arrays",
        ),
        pytest.param(
            HighspyModel,
            marks=pytest.mark.skipif(
                not hasattr(column_generation, "HighspyMasterSolver") or not has_highspy(),
                reason="rcspp built without HiGHS, or highspy not installed",
            ),
            id="highspy",
        ),
    ]
)
def make_model(request: pytest.FixtureRequest) -> Callable[[], UserModel]:
    """Makes a new, empty user model of one kind."""
    model_type: Callable[[], UserModel] = request.param
    return model_type


# ── Helpers ──────────────────────────────────────────────────────────────────


def add_customer_rows(model: UserModel, customers: int) -> None:
    """The user's rows "customer i is visited once", one per customer."""
    for customer in range(1, customers + 1):
        model.add_row(1.0, 1.0, f"customer {customer}")


def column_values(solver: MasterSolver, count: int) -> list[float]:
    """The values of the columns 0 to `count` − 1."""
    values: list[float] = solver.primal(np.arange(count, dtype=np.uint64)).tolist()
    return values


def from_scratch(
    model: UserModel, graph: ResourceGraph, customers: int, vehicles: float | None = None
) -> ColumnGenerationResult:
    """Column generation over `graph`, from no route at all: the user's customer rows
    and, with `vehicles`, their row "at most that many routes", which the master fills
    as the pricer's convexity row."""
    add_customer_rows(model, customers)
    fleet = ConvexityRow()
    if vehicles is not None:
        model.add_row(-math.inf, vehicles, "fleet")
        fleet = ConvexityRow(existing_row=customers)
    master = MasterProblem(model.solver())
    master.add_pricer(GraphPricer(graph), fleet)
    return ColumnGeneration(master).run()


def toy_with_every_route(model: UserModel) -> tuple[ColumnGeneration, ScriptedPricer]:
    """The toy with every route as a starting column: the pricer finds nothing."""
    add_customer_rows(model, Toy.customers)
    master = MasterProblem(model.solver())
    pricer = ScriptedPricer()
    master.add_columns(master.add_pricer(pricer), Toy.routes())
    return ColumnGeneration(master), pricer


# ── The tests ────────────────────────────────────────────────────────────────


def test_the_toy_lp_and_its_duals(make_model: Callable[[], UserModel]) -> None:
    model = make_model()
    loop, pricer = toy_with_every_route(model)
    result = loop.run()

    assert result.status == ColumnGenerationStatus.Optimal
    assert result.objective == pytest.approx(Toy.lp, abs=TOLERANCE)
    assert len(pricer.seen_duals) == 1
    assert pricer.seen_duals[0].tolist() == pytest.approx(Toy.duals, abs=TOLERANCE)
    values = column_values(model.solver(), len(Toy.route_costs))
    assert values == pytest.approx(Toy.lp_values, abs=TOLERANCE)


def test_the_restricted_master_mip_finds_the_best_plan(
    make_model: Callable[[], UserModel],
) -> None:
    model = make_model()
    loop, _ = toy_with_every_route(model)
    loop.run()

    assert loop.solve_restricted_master_integer(IntegerParams()) == SolverStatus.Optimal
    assert model.solver().objective() == pytest.approx(Toy.best_plan, abs=TOLERANCE)
    values = column_values(model.solver(), len(Toy.route_costs))
    assert values == pytest.approx(Toy.best_plan_values, abs=TOLERANCE)


def test_the_toy_from_scratch(make_model: Callable[[], UserModel]) -> None:
    graph = routing_graph(Toy.capacity, Toy.demands, Toy.distances)
    result = from_scratch(make_model(), graph, Toy.customers)

    assert result.status == ColumnGenerationStatus.Optimal  # no artificial column used
    assert result.objective == pytest.approx(Toy.lp, abs=TOLERANCE)
    assert result.lower_bound == pytest.approx(Toy.lp, abs=TOLERANCE)
    assert result.statistics.generated_columns > 0


def test_the_toy_with_a_fleet_limit(make_model: Callable[[], UserModel]) -> None:
    graph = routing_graph(Toy.capacity, Toy.demands, Toy.distances)

    # Two vehicles are enough: the LP optimum uses half of three routes, 1.5 vehicles.
    with_two = from_scratch(make_model(), graph, Toy.customers, 2.0)
    assert with_two.status == ColumnGenerationStatus.Optimal
    assert with_two.objective == pytest.approx(Toy.lp, abs=TOLERANCE)

    # One vehicle is not: the three customers weigh 15, more than its capacity.
    with_one = from_scratch(make_model(), graph, Toy.customers, 1.0)
    assert with_one.status == ColumnGenerationStatus.Infeasible


def test_routes_that_visit_each_customer_once(make_model: Callable[[], UserModel]) -> None:
    graph = routing_graph(FourCustomers.capacity, FourCustomers.demands, FourCustomers.distances)
    result = from_scratch(make_model(), graph, len(FourCustomers.demands))

    assert result.status == ColumnGenerationStatus.Optimal
    assert result.objective == pytest.approx(FourCustomers.lp, abs=TOLERANCE)
