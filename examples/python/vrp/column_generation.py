#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Column generation for the vehicle routing problem, with ``rcspp.column_generation``.

The master chooses routes so that every customer is visited exactly once; pricing finds new
routes in the graph of ``vrp.graph``. The master's LP is solved by HiGHS (``highspy``) or by
Gurobi (``gurobipy``).
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import TYPE_CHECKING

import numpy as np
from vrp.graph import sink
from vrp.instance import Instance, distance

from rcspp._core.graph import AlgorithmParams, Column, Row, Solution
from rcspp.column_generation import (
    ColumnGeneration,
    ColumnGenerationParams,
    ColumnTag,
    ColumnType,
    GraphPricer,
    GurobipyMasterSolver,
    HighspyMasterSolver,
    IntegerParams,
    MasterProblem,
    MasterSolver,
    NoInitialization,
    PricingStage,
    SolverStatus,
)
from rcspp.graph import ResourceGraph

if TYPE_CHECKING:
    from collections.abc import Callable

    from rcspp.column_generation import IterationInfo

MASTERS = ("highspy", "gurobipy")


@dataclass(frozen=True)
class Route:
    """A route of the integer solution: the customers in the order visited."""

    customers: list[int]
    distance: float


def master_solver(master: str, customers: int) -> MasterSolver:
    """A master solver over a new model with one row per customer, ``= 1``, and no
    column.

    ``master`` is ``"highspy"`` or ``"gurobipy"``; the model is the LP solver's own, and you can
    add anything to it before the run: other rows, variables, options.
    """
    if master == "highspy":
        import highspy

        model = highspy.Highs()
        model.setOptionValue("output_flag", False)

        for _ in range(customers):
            model.addRow(1.0, 1.0, 0, [], [])

        return HighspyMasterSolver(model)

    if master == "gurobipy":
        import gurobipy as gp

        model = gp.Model()
        model.Params.OutputFlag = 0

        for _ in range(customers):
            model.addLConstr(gp.LinExpr(), gp.GRB.EQUAL, 1.0)

        return GurobipyMasterSolver(model)

    raise ValueError(f"unknown master {master!r}: expected one of {', '.join(MASTERS)}")


def starting_routes(instance: Instance) -> list[Solution]:
    """One route per customer, depot to customer and back: with them, the first master
    LP has a solution.

    A starting column is a ``Solution`` whose ``column`` holds the route's cost and the rows
    it covers; its path is only there to be read back.
    """
    depot = instance.depot
    routes: list[Solution] = []

    for customer in instance.customers[1:]:
        column = Column()
        column.cost = 2 * distance(depot, customer)
        column.rows = [Row(index=customer.number - 1, coefficient=1.0)]

        route = Solution()
        route.path_node_ids = [depot.number, customer.number, sink(instance)]
        route.column = column
        routes.append(route)

    return routes


def column_generation(
    instance: Instance,
    graph: ResourceGraph,
    master: str,
    max_solutions: int | None = None,
    timeout_s: float = math.inf,
    on_iteration: Callable[[IterationInfo], None] | None = None,
) -> tuple[MasterProblem, ColumnGeneration]:
    """The master over ``graph``'s routes, started from :func:`starting_routes`, and the
    loop that solves its LP.

    Args:
        instance: The customers.
        graph: The pricing graph, from :func:`vrp.graph.pricing_graph`.
        master: ``"highspy"`` or ``"gurobipy"``.
        max_solutions: The most routes a pricing call returns; ``None`` returns every
            improving route found.
        timeout_s: The run's time limit, in seconds.
        on_iteration: Called after each iteration, e.g. to print it.
    """
    problem = MasterProblem(master_solver(master, len(instance.customers) - 1))

    # Pricing: rcspp's exact search, which proves that no route improves when it finds none.
    params = AlgorithmParams()

    if max_solutions is not None:
        params.max_solutions = max_solutions

    pricer = problem.add_pricer(GraphPricer(graph, [PricingStage("simple", params)]))

    # The starting routes make the first LP feasible: no artificial columns needed.
    problem.add_columns(pricer, starting_routes(instance))

    options = ColumnGenerationParams(
        initialization=NoInitialization(), timeout_s=timeout_s, on_iteration=on_iteration
    )
    return problem, ColumnGeneration(problem, options)


def integer_routes(
    loop: ColumnGeneration, problem: MasterProblem, timeout_s: float = 60.0
) -> tuple[SolverStatus, list[Route]]:
    """Solves the master with integer columns, over the routes generated so far, and
    reads its routes (none when it found no solution).

    This is a heuristic: the best plan may need a route that column generation never produced.
    """
    status = loop.solve_restricted_master_integer(
        IntegerParams(timeout_s=timeout_s, column_type=ColumnType.Binary)
    )

    if status not in (SolverStatus.Optimal, SolverStatus.TimeLimit):
        return status, []

    columns = [column for column in problem.columns if column.tag == ColumnTag.Real]

    try:
        values = problem.solver.primal(np.array([column.id for column in columns], np.uint64))
    except Exception:  # stopped at the time limit before finding a solution: no values
        return status, []

    routes: list[Route] = []

    for column, value in zip(columns, values):
        if value > 0.5:
            path = list(column.solution.path_node_ids)
            routes.append(Route(path[1:-1], column.solution.column.cost))

    return status, routes
