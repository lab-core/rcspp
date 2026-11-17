#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""One run of the C++ column generation example through its Python bindings, written to
its run folder.

    python runners/cpp_bindings_runner.py --instance C101_25    # into results/<run name>
    python runners/cpp_bindings_runner.py --file results/<run name>/run.toml --output results/again

The instance gives the pricing graph, the master's rows and its own starting columns;
this file holds only the column generation calls of the example's package,
cpp_column_generation: a master over a C++ HiGHS model or a highspy or gurobipy one, a
graph pricer with the run's stages, the start, the loop, then the integer master over the
generated columns. The loop and the pricer run in C++; so does the master, except over
gurobipy.
"""

from __future__ import annotations

import math
import sys
import time
from pathlib import Path
from typing import TYPE_CHECKING, Any

import numpy as np

# The code the runners share is in the folder above.
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from column_generation_benchmark import runner, source_tree  # noqa: E402
from column_generation_benchmark.instances import Instance, make_instance  # noqa: E402
from column_generation_benchmark.options import (  # noqa: E402
    OptionsError,
    Run,
    make_algorithm_params,
)

if TYPE_CHECKING:
    from cpp_column_generation import (
        ColumnGeneration,
        MasterColumn,
        MasterProblem,
        MasterSolver,
    )

LAYER = "cpp-bindings"


# MARK: Building


def make_master_solver(run: Run, lower: list[float], upper: list[float]) -> MasterSolver:
    """The master solver over a model of the instance's rows, without columns, with the
    run's parameters set on it (its log is off unless they turn it on)."""
    import cpp_column_generation

    solver: MasterSolver

    if run.master.type == "gurobipy":
        model = runner.gurobipy_model(lower, upper)
        solver = cpp_column_generation.GurobipyMasterSolver(model)
    elif not hasattr(cpp_column_generation, "HighsMasterSolver"):
        raise source_tree.SourceTreeError(
            f"master {run.master.type} needs the example built with "
            "USE_COLUMN_GENERATION_EXAMPLE_HIGHS=ON"
        )
    elif run.master.type == "highspy":
        import highspy

        # The master solver copies the model, not its options: they go to the copy below.
        model = highspy.Highs()
        model.setOptionValue("output_flag", False)

        for low, up in zip(lower, upper, strict=True):
            model.addRow(low, up, 0, [], [])

        solver = cpp_column_generation.HighspyMasterSolver(model)
    else:
        solver = cpp_column_generation.HighsMasterSolver(row_lower=lower, row_upper=upper)

    for name, value in run.master.parameters.items():
        try:
            solver.set_option(name, value)
        except ValueError as error:
            raise OptionsError(
                f"master: {run.master.type} refuses {name} = {value!r} ({error})"
            ) from None

    return solver


def build(run: Run) -> tuple[Instance, MasterProblem, ColumnGeneration]:
    """The instance, the master and its loop, ready to run: graph, master, pricer and
    start built.

    Raises:
        SourceTreeError: When the build has not the example, or not the part a master needs.
    """
    source_tree.use_example()

    from cpp_column_generation import (
        ArtificialColumns,
        ColumnGeneration,
        ColumnGenerationParams,
        GraphPricer,
        MasterProblem,
        NoInitialization,
        PricingStage,
        RelaxationParams,
        make_tolerances,
    )

    instance = make_instance(run.instance.to_table())
    graph = instance.build_graph(run.graph.to_table())
    master = MasterProblem(make_master_solver(run, *instance.row_bounds()))
    stages = [PricingStage(*make_algorithm_params(stage)) for stage in run.pricing.stages]
    graph_pricer = GraphPricer(graph, stages)
    pricer = master.add_pricer(graph_pricer)

    # The example's parameters are set one by one: what the run leaves out keeps its default.
    params = ColumnGenerationParams()

    if run.start.type == "artificial":
        # One artificial column per row, costing the factor times the largest arc cost.
        assert run.start.cost_factor is not None
        params.initialization = ArtificialColumns(
            cost=run.start.cost_factor * max(1.0, graph_pricer.cost_scale())
        )
    else:
        master.add_columns(pricer, instance.starting_columns(run.start.type))
        params.initialization = NoInitialization()

    settings = run.column_generation

    if settings.max_iterations is not None:
        params.max_iterations = settings.max_iterations

    if settings.timeout_s is not None:
        params.timeout_s = settings.timeout_s

    if settings.tolerances is not None:
        tolerances = dict(settings.tolerances)
        params.tolerances = make_tolerances(tolerances.pop("type"), **tolerances)

    # Every master solve sets the solver's threads from these.
    if settings.threads is not None:
        params.relaxation = RelaxationParams(threads=settings.threads)

    return instance, master, ColumnGeneration(master, params)


# MARK: Solving


def solve(loop: ColumnGeneration) -> dict[str, Any]:
    """Runs the loop; its result, as ``result.json`` holds it."""
    started = time.perf_counter()
    result = loop.run()
    seconds = time.perf_counter() - started
    statistics = result.statistics
    # The example's objective is infinite until an LP is solved; the package's is None.
    objective = None if math.isinf(result.objective) else result.objective
    return {
        "status": result.status.name,
        "objective": objective,
        "iterations": result.iterations,
        "columns": statistics.generated_columns,
        "artificial_columns": statistics.artificial_columns,
        "master_seconds": statistics.relaxation_seconds,
        "pricing_seconds": statistics.pricing_seconds,
        "other_seconds": seconds - statistics.relaxation_seconds - statistics.pricing_seconds,
        "solve_seconds": seconds,
    }


def describe_columns(
    columns: list[MasterColumn], values: list[float]
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    """The columns with a value, as ``solution.json`` lists them, and apart the
    artificial ones."""
    from cpp_column_generation import MasterProblem

    used: list[dict[str, Any]] = []
    artificial: list[dict[str, Any]] = []

    for column, value in zip(columns, values, strict=True):
        if value == 0.0:
            continue

        if column.pricer == MasterProblem.no_pricer:
            artificial.append({"id": column.id, "value": value})
            continue

        solution = column.solution
        used.append(
            {
                "id": column.id,
                "value": value,
                "cost": solution.column.cost,
                "path": list(solution.path_node_ids),
                "rows": [[row.index, row.coefficient] for row in solution.column.rows],
            }
        )

    return used, artificial


def read_solution(master: MasterProblem, objective: float | None) -> dict[str, Any]:
    """The last LP solution, as ``solution.json`` holds it: its objective, the columns
    with a value (artificial ones apart) and the duals.

    ``lp`` is null when no LP was solved, or when the run stopped at a limit after pricing
    added columns: the master solvers drop the last solution's values when columns are
    added, so ``lp_missing`` then says why.
    """
    if objective is None:
        return {"lp": None, "lp_missing": "no master LP was solved"}

    columns = master.columns()

    try:
        values = master.solver().primal(np.array([column.id for column in columns], np.uint64))
        duals = master.solver().duals()
    except runner.values_gone() as error:
        missing = f"columns were added after the last LP solve ({error})"
        return {"lp": None, "lp_missing": missing}

    used, artificial = describe_columns(columns, values.tolist())
    return {
        "lp": {
            "objective": objective,
            "solved_again": False,
            "columns": used,
            "artificial_columns": artificial,
            "duals": duals.tolist(),
        }
    }


def solve_lp_again(master: MasterProblem, run: Run) -> tuple[dict[str, Any], float]:
    """The LP over every column the master holds, solved once more after a run that
    stopped at a limit after pricing, as :func:`read_solution` gives it
    (``solved_again`` set), and the seconds it took."""
    from cpp_column_generation import RelaxationParams, SolverStatus

    threads = run.column_generation.threads
    started = time.perf_counter()
    status = master.solver().solve_relaxation(RelaxationParams(threads=threads or 0))
    master.solver().restore_user_model()
    seconds = time.perf_counter() - started

    if status != SolverStatus.Optimal:
        return {"lp": None, "lp_missing": f"the LP solved again ended {status.name}"}, seconds

    solution = read_solution(master, master.solver().objective())

    if solution["lp"] is not None:
        solution["lp"]["solved_again"] = True

    return solution, seconds


def solve_integer(loop: ColumnGeneration, master: MasterProblem, run: Run) -> dict[str, Any]:
    """Solves the integer master over the generated columns; its status, seconds,
    objective and columns (None and empty without a solution)."""
    from cpp_column_generation import ColumnType, IntegerParams

    settings = run.integer
    given: dict[str, Any] = {}

    if settings.relative_gap is not None:
        given["relative_gap"] = settings.relative_gap

    # The integer solve's own threads, else those of every LP solve.
    threads = settings.threads if settings.threads is not None else run.column_generation.threads

    if threads is not None:
        given["threads"] = threads

    column_type = ColumnType.Binary if settings.column_type == "binary" else ColumnType.Integer
    params = IntegerParams(timeout_s=settings.timeout_s, column_type=column_type, **given)
    started = time.perf_counter()
    status = loop.solve_restricted_master_integer(params)
    seconds = time.perf_counter() - started
    columns = master.columns()
    found: dict[str, Any] = {"status": status.name, "seconds": seconds}

    try:
        values = master.solver().primal(np.array([column.id for column in columns], np.uint64))
        objective = master.solver().objective()
    except runner.values_gone():
        return {**found, "objective": None, "columns": [], "artificial_columns": []}

    used, artificial = describe_columns(columns, values.tolist())
    return {**found, "objective": objective, "columns": used, "artificial_columns": artificial}


def main(arguments: list[str] | None = None) -> int:
    return runner.main(
        __doc__,
        Path(__file__),
        sys.argv[1:] if arguments is None else arguments,
        layer=LAYER,
        build=build,
        solve=solve,
        read_solution=read_solution,
        solve_lp_again=solve_lp_again,
        solve_integer=solve_integer,
    )


if __name__ == "__main__":
    sys.exit(main())
