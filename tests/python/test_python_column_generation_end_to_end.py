#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Pure-Python column generation end to end, with the values of the C++ end-to-end tests
(tests/cpp/test_column_generation_end_to_end.hpp).

The master is a model the user built for their LP solver; each kind of model is a
UserModel, and every test runs over each of them. A solver missing from this machine is
skipped.
"""

import _thread
import importlib.util
import math
import os
import sys
import threading
from collections.abc import Callable
from typing import Any, Protocol

import numpy as np
import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from python_column_generation_fakes import ScriptedPricer, make_path  # noqa: E402

from rcspp import presets  # noqa: E402
from rcspp._core.graph import Solution  # noqa: E402
from rcspp.graph import ResourceGraph  # noqa: E402
from rcspp.python_column_generation import (  # noqa: E402
    ColumnGeneration,
    ColumnGenerationParams,
    ColumnGenerationResult,
    ColumnGenerationStatus,
    ConvexityRow,
    GraphPricer,
    GurobipyMasterSolver,
    HighspyMasterSolver,
    IntegerParams,
    MasterProblem,
    MasterSolver,
    PricingStage,
    RelaxationParams,
    SolverStatus,
)

# Accuracy of the values checked: within what LP solvers return.
TOLERANCE = 1e-6


# MARK: The instances


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
            for cost, served in zip(cls.route_costs, cls.served, strict=True)
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

    The depot is the source 0 and the sink n + 1, the customers are the nodes 1 to n.
    Resources: the cost, the load (at most `capacity`) and the customers visited, so that a
    route visits each one at most once. An arc into customer j covers master row j - 1.
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


# MARK: The users' models


class UserModel(Protocol):
    """A master the user built in their LP solver: rows first, then its master solver."""

    def add_row(self, lower: float, upper: float, name: str) -> None:
        """A row the user builds, empty: the columns fill it."""
        ...

    def solver(self) -> MasterSolver:
        """The master solver over the rows added."""
        ...


class HighspyModel:
    """The user's rows in a highspy model, which HighspyMasterSolver works on."""

    def __init__(self) -> None:
        import highspy

        self.model = highspy.Highs()  # type: ignore[no-untyped-call]
        self.model.setOptionValue("output_flag", False)
        self._solver: MasterSolver | None = None

    def add_row(self, lower: float, upper: float, name: str) -> None:
        assert self._solver is None, "rows come before the master solver"
        self.model.addRow(lower, upper, 0, [], [])
        self.model.passRowName(self.model.getNumRow() - 1, name)

    def solver(self) -> MasterSolver:
        if self._solver is None:
            self._solver = HighspyMasterSolver(self.model)

        return self._solver


class GurobipyModel:
    """The user's rows in a gurobipy model, which GurobipyMasterSolver works on."""

    def __init__(self) -> None:
        import gurobipy

        self.model = gurobipy.Model()
        self.model.Params.OutputFlag = 0
        self._solver: MasterSolver | None = None

    def add_row(self, lower: float, upper: float, name: str) -> None:
        import gurobipy

        assert self._solver is None, "rows come before the master solver"
        expression = gurobipy.LinExpr()

        if lower == upper:
            self.model.addLConstr(expression, gurobipy.GRB.EQUAL, lower, name)
        elif math.isinf(upper):
            self.model.addLConstr(expression, gurobipy.GRB.GREATER_EQUAL, lower, name)
        else:
            self.model.addLConstr(expression, gurobipy.GRB.LESS_EQUAL, upper, name)

    def solver(self) -> MasterSolver:
        if self._solver is None:
            self._solver = GurobipyMasterSolver(self.model)

        return self._solver


def has_highspy() -> bool:
    """Whether highspy is installed."""
    return importlib.util.find_spec("highspy") is not None


def has_gurobi_license() -> bool:
    """Whether gurobipy is installed and Gurobi can build a model on this machine."""
    if importlib.util.find_spec("gurobipy") is None:
        return False

    import gurobipy

    try:
        gurobipy.Model().dispose()
    except gurobipy.GurobiError:
        return False

    return True


needs_highspy = pytest.mark.skipif(not has_highspy(), reason="highspy is not installed")
needs_gurobi = pytest.mark.skipif(
    not has_gurobi_license(), reason="gurobipy not installed, or no Gurobi license"
)


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


# MARK: Helpers


def add_customer_rows(model: UserModel, customers: int) -> None:
    """The user's rows "customer i is visited once", one per customer."""
    for customer in range(1, customers + 1):
        model.add_row(1.0, 1.0, f"customer {customer}")


def column_values(solver: MasterSolver, count: int) -> list[float]:
    """The values of the columns 0 to `count` - 1."""
    values: list[float] = solver.primal(np.arange(count, dtype=np.uint64)).tolist()
    return values


def master_from_scratch(
    model: UserModel,
    graph: ResourceGraph,
    customers: int,
    vehicles: float | None = None,
    stages: list[PricingStage] | None = None,
) -> MasterProblem:
    """A master priced over `graph`, with no route at all: the user's customer rows and,
    with `vehicles`, their row "at most that many routes", which the master fills as the
    pricer's convexity row."""
    add_customer_rows(model, customers)
    fleet = ConvexityRow()

    if vehicles is not None:
        model.add_row(-math.inf, vehicles, "fleet")
        fleet = ConvexityRow(existing_row=customers)

    master = MasterProblem(model.solver())
    master.add_pricer(GraphPricer(graph, stages), fleet)
    return master


def from_scratch(
    model: UserModel,
    graph: ResourceGraph,
    customers: int,
    vehicles: float | None = None,
    stages: list[PricingStage] | None = None,
) -> ColumnGenerationResult:
    """Column generation over `graph` from no route at all (see master_from_scratch)."""
    return ColumnGeneration(master_from_scratch(model, graph, customers, vehicles, stages)).run()


def toy_with_every_route(model: UserModel) -> tuple[ColumnGeneration, ScriptedPricer]:
    """The toy with every route as a starting column: the pricer finds nothing."""
    add_customer_rows(model, Toy.customers)
    master = MasterProblem(model.solver())
    pricer = ScriptedPricer()
    master.add_columns(master.add_pricer(pricer), Toy.routes())
    return ColumnGeneration(master), pricer


# MARK: The tests


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


def test_a_heuristic_stage_before_the_exact_one_gives_the_same_lp(
    make_model: Callable[[], UserModel],
) -> None:
    graph = routing_graph(Toy.capacity, Toy.demands, Toy.distances)
    stages = [PricingStage("tabu"), PricingStage("simple")]
    result = from_scratch(make_model(), graph, Toy.customers, stages=stages)

    assert result.status == ColumnGenerationStatus.Optimal
    assert result.objective == pytest.approx(Toy.lp, abs=TOLERANCE)


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


# MARK: Ctrl-C during an LP solve

# An assignment LP that simplex needs a second or more for.
ASSIGNMENT_SIZE = 600


def interrupted_run(master: MasterProblem, interrupter: Callable[[threading.Event], None]) -> None:
    """Runs the loop over `master` while a thread runs `interrupter(done)`, `done` being
    set when the run ends, and checks that the run raises KeyboardInterrupt."""
    pricer = ScriptedPricer()
    master.add_pricer(pricer)
    params = ColumnGenerationParams(
        initialization="none", relaxation=RelaxationParams(timeout_s=120.0)
    )
    done = threading.Event()
    thread = threading.Thread(target=interrupter, args=(done,))
    thread.start()

    try:
        with pytest.raises(KeyboardInterrupt):
            ColumnGeneration(master, params).run()
    finally:
        done.set()
        thread.join()

    # The run stopped in its first LP solve.
    assert pricer.requests == []


@needs_gurobi
def test_ctrl_c_stops_a_run_over_gurobi_and_the_model_is_the_users_again() -> None:
    import gurobipy

    model = gurobipy.Model()
    model.Params.OutputFlag = 0
    model.Params.Presolve = 0
    cost = np.random.default_rng(0).random((ASSIGNMENT_SIZE, ASSIGNMENT_SIZE))
    x = model.addMVar((ASSIGNMENT_SIZE, ASSIGNMENT_SIZE), vtype=gurobipy.GRB.BINARY, obj=cost)
    model.addConstr(x.sum(axis=0) == 1)
    model.addConstr(x.sum(axis=1) == 1)

    def terminate(done: threading.Event) -> None:
        # From the start and often, whatever the speed of the machine: a call before the solve
        # starts may be lost, and the solve lasts far longer than the interval.
        while not done.is_set():
            model.terminate()
            done.wait(0.01)

    # Gurobi stops with the status INTERRUPTED: the master solver turns it into the exception.
    interrupted_run(MasterProblem(GurobipyMasterSolver(model)), terminate)

    assert model.Status == gurobipy.GRB.INTERRUPTED
    assert model.Params.Method == gurobipy.GRB.METHOD_AUTO and model.Params.TimeLimit == math.inf

    # The user's binaries are binaries again (once Gurobi applies the pending changes).
    model.update()
    assert set(model.getAttr(gurobipy.GRB.Attr.VType, model.getVars())) == {gurobipy.GRB.BINARY}


@needs_highspy
def test_ctrl_c_stops_a_run_over_highspy_once_the_lp_solve_returns() -> None:
    import highspy

    size = ASSIGNMENT_SIZE
    columns = np.arange(size * size)
    lp = highspy.HighsLp()
    lp.num_col_, lp.num_row_ = size * size, 2 * size
    lp.col_cost_ = np.random.default_rng(0).random(size * size)
    lp.col_lower_, lp.col_upper_ = np.zeros(size * size), np.full(size * size, highspy.kHighsInf)
    lp.row_lower_ = lp.row_upper_ = np.ones(2 * size)
    lp.a_matrix_.format_ = highspy.MatrixFormat.kColwise
    lp.a_matrix_.start_ = np.arange(0, 2 * size * size + 1, 2)
    lp.a_matrix_.index_ = np.stack([columns // size, size + columns % size], axis=1).flatten()
    lp.a_matrix_.value_ = np.ones(2 * size * size)

    model = highspy.Highs()  # type: ignore[no-untyped-call]
    model.setOptionValue("output_flag", False)
    model.setOptionValue("presolve", "off")
    model.passModel(lp)

    # Ctrl-C comes once the simplex has started, told by its first callback, which stops
    # itself. Ctrl-C may come while that callback runs: the master solver holds it back.
    simplex = highspy.cb.HighsCallbackType.kCallbackSimplexInterrupt
    started = threading.Event()

    def on_simplex(*_: Any) -> None:
        model.stopCallback(simplex)
        started.set()

    model.setCallback(on_simplex, None)
    model.startCallback(simplex)

    def interrupt(done: threading.Event) -> None:
        while not started.wait(0.01):
            if done.is_set():
                return

        _thread.interrupt_main()

    # HiGHS does not stop on Ctrl-C: Python raises the exception when the solve returns, and
    # the solve was not aborted.
    interrupted_run(MasterProblem(HighspyMasterSolver(model)), interrupt)
    assert model.getModelStatus() == highspy.HighsModelStatus.kOptimal
