#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""A column generation loop written in Python.

:func:`run` does what ``rcspp.column_generation.ColumnGeneration.run`` does in C++: solve
the master LP, price its duals, add the columns found, until no column improves or a
limit is reached. It is built from the same pieces (the master, its master solver, the
pricers, the tolerances and the initialization), so it gives the same results. Copy it and
change it to try another loop: another pricing order, a stabilization of the duals, a log
of your own.

The pieces still run in C++: a ``GraphPricer`` prices without Python, and a
``HighspyMasterSolver`` solves the LP in C++. Only the loop itself is Python.

Run it on a toy: three customers, served by routes of at most two of them::

    pip install rcspp[highspy]
    python examples/python/column_generation_loop.py
"""

import math
import time
from dataclasses import dataclass

import numpy as np
from numpy.typing import NDArray

from rcspp import presets
from rcspp.column_generation import (
    AbsoluteTolerances,
    ArtificialColumns,
    ColumnGenerationParams,
    ColumnGenerationStatus,
    ConvexityRow,
    GraphPricer,
    IntegerParams,
    MasterProblem,
    PricingRequest,
    PricingStatus,
    RelaxationParams,
    SolverStatus,
    ToleranceContext,
)
from rcspp.graph import ResourceGraph


@dataclass
class LoopResult:
    """Result of a run: the fields of ``ColumnGenerationResult`` and its statistics."""

    status: ColumnGenerationStatus = ColumnGenerationStatus.SolverError
    objective: float = math.inf
    lower_bound: float = -math.inf
    iterations: int = 0
    generated_columns: int = 0
    artificial_columns: int = 0
    artificial_cost: float = 0.0
    total_seconds: float = 0.0
    relaxation_seconds: float = 0.0
    pricing_seconds: float = 0.0


# How a run ends when the master LP was not solved to optimality.
STATUS_AFTER_FAILED_SOLVE = {
    SolverStatus.Infeasible: ColumnGenerationStatus.RestrictedMasterInfeasible,
    SolverStatus.Unbounded: ColumnGenerationStatus.Unbounded,
    SolverStatus.TimeLimit: ColumnGenerationStatus.TimeLimit,
    SolverStatus.Other: ColumnGenerationStatus.SolverError,
}


def run(master: MasterProblem, params: ColumnGenerationParams | None = None) -> LoopResult:
    """Column generation over `master`, until no column improves or a limit is reached.

    Reads the limits, ``relaxation``, ``tolerances``, ``initialization`` and
    ``statistics`` of `params`; ``None`` tolerances or initialization mean
    :class:`AbsoluteTolerances` and :class:`ArtificialColumns`, as in C++. The hooks
    ``on_iteration`` and ``on_converged`` are not called: change the loop instead.
    """
    params = params if params is not None else ColumnGenerationParams()
    try:
        return iterate(master, params)
    finally:
        master.solver().restore_user_model()


def iterate(master: MasterProblem, params: ColumnGenerationParams) -> LoopResult:
    """The loop itself: solve, price, add columns."""
    result = LoopResult()
    start = time.perf_counter()
    tolerances = params.tolerances if params.tolerances is not None else AbsoluteTolerances()
    initialization = (
        params.initialization if params.initialization is not None else ArtificialColumns()
    )

    # The tolerances' inputs, then starting columns unless another loop added them.
    context = tolerance_context(master)
    if not master.initialized():
        initialization.initialize(master, context)
        master.mark_initialized()

    # Never below the LP solver's own tolerance, or a column it leaves out would come back
    # forever.
    solver = master.solver()
    reduced_cost_tolerance = max(tolerances.reduced_cost(context), solver.dual_tolerance())

    while True:
        # Limits.
        if result.iterations >= params.max_iterations:
            result.status = ColumnGenerationStatus.IterationLimit
            break

        remaining = params.timeout_s - (time.perf_counter() - start)
        if remaining <= 0.0:
            result.status = ColumnGenerationStatus.TimeLimit
            break

        # Solve the master LP, within the time left. A copy: params.relaxation is the
        # caller's.
        relaxation = RelaxationParams(
            timeout_s=min(params.relaxation.timeout_s, remaining),
            threads=params.relaxation.threads,
            verbose=params.relaxation.verbose,
        )

        relaxation_start = time.perf_counter()
        solver_status = solver.solve_relaxation(relaxation)
        result.relaxation_seconds += time.perf_counter() - relaxation_start
        if solver_status != SolverStatus.Optimal:
            result.status = STATUS_AFTER_FAILED_SOLVE[solver_status]
            break

        # Read its value and duals.
        result.iterations += 1
        result.objective = solver.objective()
        duals = solver.duals()

        # Price the duals and add the columns found.
        pricing_start = time.perf_counter()
        new_columns, proven = price(
            master, duals, params.timeout_s - (time.perf_counter() - start), reduced_cost_tolerance
        )
        result.pricing_seconds += time.perf_counter() - pricing_start
        result.generated_columns += new_columns

        # New columns: solve again. None: converged, proven or not.
        if new_columns > 0:
            continue

        # Unproven: the run's time limit may have cut pricing short.
        if not proven:
            result.status = (
                ColumnGenerationStatus.TimeLimit
                if time.perf_counter() - start >= params.timeout_s
                else ColumnGenerationStatus.NotProven
            )
            break

        # An artificial column still in use means no real solution exists.
        if uses_artificial_columns(master, tolerances.feasibility(context)):
            result.status = ColumnGenerationStatus.Infeasible
        else:
            result.status = ColumnGenerationStatus.Optimal
            result.lower_bound = result.objective
        break

    result.artificial_columns = len(master.artificial_column_ids())
    result.artificial_cost = master.artificial_cost()
    result.total_seconds = time.perf_counter() - start
    if not params.statistics.timers:
        result.relaxation_seconds = 0.0
        result.pricing_seconds = 0.0
    return result


def price(
    master: MasterProblem,
    duals: NDArray[np.float64],
    timeout_s: float,
    reduced_cost_tolerance: float,
) -> tuple[int, bool]:
    """Prices every pricer once and adds what they find.

    Returns the number of columns added, and whether pricing proved that no column
    improves.
    """
    new_columns = 0
    proven = True

    for index in range(master.number_of_pricers()):
        # A column improves when its reduced cost, convexity dual included, is negative.
        convexity = master.convexity_row(index)
        convexity_dual = duals[convexity] if convexity is not None else 0.0
        request = PricingRequest(
            reduced_cost_threshold=convexity_dual - reduced_cost_tolerance,
            timeout_s=max(0.0, timeout_s),
        )

        priced = master.pricer(index).price(duals, request)

        # Only an exact stage that finished proves there is no improving column. No path at
        # all (Infeasible) proves it too: no column of any cost exists. A pricer with
        # nothing to price holds no improving column.
        finished = priced.status in (PricingStatus.Optimal, PricingStatus.Infeasible)
        nothing_to_price = priced.status == PricingStatus.NotApplicable
        proven = proven and (nothing_to_price or (priced.exact_stage and finished))
        new_columns += len(priced.columns)
        master.add_columns(index, priced.columns)

    return new_columns, proven


def tolerance_context(master: MasterProblem) -> ToleranceContext:
    """The cost scale of the tolerances: the largest cost a pricer or a column may have."""
    scale = 1.0

    for index in range(master.number_of_pricers()):
        scale = max(scale, master.pricer(index).cost_scale())

    for column in master.columns():
        scale = max(scale, abs(column.solution.column.cost))

    # Starting columns the user built in their own model.
    scale = max(scale, master.solver().largest_user_cost())

    return ToleranceContext(cost_scale=scale)


def uses_artificial_columns(master: MasterProblem, tolerance: float) -> bool:
    """Whether an artificial column is used by the last LP solution."""
    values = master.solver().primal(master.artificial_column_ids())
    return bool((values > tolerance).any())


# ── The toy ──────────────────────────────────────────────────────────────────


def toy_graph() -> ResourceGraph:
    """Vehicle routes over three customers of demands 4, 5 and 6, with a capacity of 11.

    The depot is the source 0 and the sink 4. From the depot to any customer is 10;
    customers 1 and 2 are 4 apart, 1 and 3 are 5, 2 and 3 are 6. An arc into customer j
    covers master row j − 1.
    """
    demands = [4.0, 5.0, 6.0]
    distances = [
        [0.0, 10.0, 10.0, 10.0],
        [10.0, 0.0, 4.0, 5.0],
        [10.0, 4.0, 0.0, 6.0],
        [10.0, 5.0, 6.0, 0.0],
    ]
    sink = len(demands) + 1

    graph = ResourceGraph()
    presets.add_cost_resource(graph)
    presets.add_capacity_resource(graph, "real", 11.0)
    presets.add_elementary_resource(graph, "bitset", range(sink + 1))

    graph.add_node(0, source=True)
    for customer in range(1, sink):
        graph.add_node(customer)
    graph.add_node(sink, sink=True)

    for origin in range(sink):
        for destination in range(1, sink + 1):
            place = 0 if destination == sink else destination  # the sink is the depot again
            if place == origin:
                continue
            distance = distances[origin][place]
            demand = 0.0 if destination == sink else demands[destination - 1]
            rows = [] if destination == sink else [(destination - 1, 1.0)]
            graph.add_arc((distance, demand, set[int]()), origin, destination, distance, rows)
    return graph


def main() -> None:
    """Column generation on the toy, then the best plan from the routes generated."""
    # Here only: run() works with any master solver.
    import highspy

    from rcspp.column_generation import HighspyMasterSolver

    # The user's master: "customer i is visited once", filled by the routes.
    model = highspy.Highs()
    model.setOptionValue("output_flag", False)
    for _ in range(3):
        model.addRow(1.0, 1.0, 0, [], [])

    solver = HighspyMasterSolver(model)
    master = MasterProblem(solver)
    master.add_pricer(GraphPricer(toy_graph()), ConvexityRow())

    result = run(master)
    print(f"{result.status.name} after {result.iterations} iterations: LP {result.objective:g}")
    print(f"{result.generated_columns} routes generated in {result.total_seconds:.3f} s")

    if solver.solve_integer(IntegerParams()) == SolverStatus.Optimal:
        print(f"best plan from these routes: {solver.objective():g}")


if __name__ == "__main__":
    main()
