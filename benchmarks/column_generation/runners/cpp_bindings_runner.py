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
gurobipy. :func:`main` runs them in order. ``package_runner.py`` is its twin over the
package: a change to one is made to the other.
"""

from __future__ import annotations

import math
import sys
import time
from collections.abc import Callable
from pathlib import Path
from typing import TYPE_CHECKING, Any

import numpy as np

# The code the runners share is in the folder above.
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from column_generation_benchmark import runner, source_tree  # noqa: E402
from column_generation_benchmark.instances import Instance, make_instance  # noqa: E402
from column_generation_benchmark.iterations import (  # noqa: E402
    IterationFile,
    statistics_switches,
)
from column_generation_benchmark.options import (  # noqa: E402
    OptionsError,
    Run,
    make_algorithm_params,
)

if TYPE_CHECKING:
    from cpp_column_generation import (
        ColumnGeneration,
        IterationInfo,
        MasterColumn,
        MasterProblem,
        MasterSolver,
    )

LAYER = "cpp-bindings"

#: The largest value a column has and still counts as unused.
ZERO = 1e-9


# MARK: Building


def make_master_solver(run: Run, lower: list[float], upper: list[float]) -> MasterSolver:
    """The master solver over a model of the instance's rows, without columns, with the
    run's parameters set through it.

    Its log stays off: every solve sets it.
    """
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

        for low, up in zip(lower, upper, strict=True):
            model.addRow(low, up, 0, [], [])

        solver = cpp_column_generation.HighspyMasterSolver(model)
    else:
        solver = cpp_column_generation.HighsMasterSolver(row_lower=lower, row_upper=upper)

    for name, value in run.master.parameters.items():
        try:
            solver.set_option(name, value)
        except (TypeError, ValueError) as error:
            raise OptionsError(
                f"master: {run.master.type} refuses {name} = {value!r} ({error})"
            ) from None

    return solver


def build(
    run: Run, on_iteration: Callable[[IterationInfo], None] | None = None
) -> tuple[Instance, MasterProblem, ColumnGeneration]:
    """The instance, the master and its loop, ready to run: graph, master, pricer and
    start built; `on_iteration` the loop's hook, with the statistics the run records.

    Raises:
        SourceTreeError: When the build has not the example, or not the part a master needs.
    """
    source_tree.use_example()

    from cpp_column_generation import (
        ArtificialColumns,
        ColumnGeneration,
        ColumnGenerationParams,
        ConvexityRow,
        DebugChecks,
        GraphPricer,
        MasterProblem,
        NoInitialization,
        PricingStage,
        RelaxationParams,
        StatisticsParams,
        make_tolerances,
    )

    instance = make_instance(run.instance.to_table())
    graph = instance.build_graph(run.graph.to_table())
    master = MasterProblem(make_master_solver(run, *instance.row_bounds()))
    stages = [PricingStage(*make_algorithm_params(stage)) for stage in run.pricing.stages]
    # Absent, preprocessing is on, as every layer has it.
    graph_pricer = GraphPricer(graph, stages, preprocess=run.pricing.preprocess is not False)
    # The fleet row, when the run sets one: the pricer's convexity row.
    vehicles = run.instance.vehicles
    convexity = ConvexityRow() if vehicles is None else ConvexityRow(max_paths=float(vehicles))
    pricer = master.add_pricer(graph_pricer, convexity)

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

    for name in ("absolute_gap", "relative_gap", "cutoff", "integer_objective"):
        if getattr(settings, name) is not None:
            setattr(params, name, getattr(settings, name))

    # The example logs through the engine's logger: its lines go to the run's output.
    if settings.log_frequency:
        params.log_frequency = settings.log_frequency

    params.debug_checks = DebugChecks(**settings.debug_check_switches())
    params.statistics = StatisticsParams(**statistics_switches(run.statistics.iterations))
    params.on_iteration = on_iteration
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
        "lower_bound": None if math.isinf(result.lower_bound) else result.lower_bound,
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
        # A solver's zero can be a rounding error away from it.
        if abs(value) <= ZERO:
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


def read_solution(master: MasterProblem, objective: float) -> dict[str, Any]:
    """The last LP solution, as ``solution.json`` holds it: its objective, the columns
    with a value (artificial ones apart) and the duals.

    Read only when that LP finished
    and nothing came after it (:data:`runner.LAST_LP_FINISHED`).
    """
    columns = master.columns()
    values = master.solver().primal(np.array([column.id for column in columns], np.uint64))
    duals = master.solver().duals()
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
    stopped at a limit or a failed solve, as :func:`read_solution` gives it
    (``solved_again`` set), and the seconds it took.

    It has the run's own time limit again, at most.
    """
    from cpp_column_generation import RelaxationParams, SolverStatus

    settings = run.column_generation
    timeout_s = math.inf if settings.timeout_s is None else settings.timeout_s
    started = time.perf_counter()
    status = master.solver().solve_relaxation(
        RelaxationParams(timeout_s=timeout_s, threads=settings.threads or 0)
    )
    master.solver().restore_user_model()
    seconds = time.perf_counter() - started

    if status != SolverStatus.Optimal:
        return {"lp": None, "lp_missing": f"the LP solved again ended {status.name}"}, seconds

    solution = read_solution(master, master.solver().objective())
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

    # Stopped at its limit, the solve may have a solution or none; without one, the solver
    # has no values to give.
    no_values: tuple[type[Exception], ...] = (RuntimeError,)

    if run.master.type == "gurobipy":
        import gurobipy

        no_values = (gurobipy.GurobiError,)

    try:
        values = master.solver().primal(np.array([column.id for column in columns], np.uint64))
        objective = master.solver().objective()
    except no_values:
        return {**found, "objective": None, "columns": [], "artificial_columns": []}

    used, artificial = describe_columns(columns, values.tolist())
    return {**found, "objective": objective, "columns": used, "artificial_columns": artificial}


# MARK: Running


def main(arguments: list[str] | None = None) -> int:
    """One run: its options read, its objects built, the loop run, the LP solution read,
    the integer master solved, the folder written.

    Returns:
        The exit code: 0; 1 when the integer solution fails the instance's check, or the
        run stopped after its folder was started; 2 when the run cannot start.
    """
    arguments = sys.argv[1:] if arguments is None else arguments
    script = Path(__file__)
    parsed = runner.parse_arguments(__doc__, arguments)
    folder = None

    try:
        run, build_folder = runner.read_run(parsed, LAYER)

        # Built first: wrong options stop the runner before its folder is touched. The
        # iterations' file opens once the folder exists.
        iterations = IterationFile(run.statistics.iterations)
        hook = iterations.write if run.statistics.iterations else None
        instance, master, loop = build(run, hook)

        if parsed.check:
            print(f"{run.name}: built")
            return 0

        folder = runner.start_folder(parsed, run, [str(script.resolve()), *arguments])

        if hook is not None:
            iterations.open(folder)

        try:
            result = solve(loop)
        finally:
            iterations.close()

        result["lp_solved_again_seconds"] = None
        solution = None

        # Read before the integer solve, which replaces the LP's values.
        if run.statistics.solution:
            if result["objective"] is None:
                solution = {"lp": None, "lp_missing": "no master LP was solved"}
            elif result["status"] in runner.LAST_LP_FINISHED:
                solution = read_solution(master, result["objective"])
            else:
                solution, result["lp_solved_again_seconds"] = solve_lp_again(master, run)

        integer = solve_integer(loop, master, run) if run.integer.solve else None
        fields, parts = runner.integer_results(instance, result, integer)
        result.update(fields)

        if solution is not None:
            solution.update(parts)

        runner.finish_folder(folder, parsed, run, build_folder, result, solution)
    except runner.ERRORS as error:
        return runner.stop(script.name, error, folder)

    return runner.report(script.name, run, result, parts)


if __name__ == "__main__":
    sys.exit(main())
