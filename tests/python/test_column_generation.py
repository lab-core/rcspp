#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The column generation bindings: the C++ loop over master solvers and pricers written in
Python, and the arrays, options and results that cross the boundary."""

import gc
import math
import os
import subprocess
import sys
from typing import Any

import numpy as np
import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from column_generation_fakes import (  # noqa: E402
    FloatArray,
    Mistake,
    OptionValue,
    ScaledPricer,
    ScriptedMasterSolver,
    ScriptedPricer,
    SelfTestMasterSolver,
    WrappedModelMasterSolver,
    make_path,
    proven_none,
)

import rcspp.column_generation as column_generation  # noqa: E402
from rcspp.column_generation import (  # noqa: E402
    AbsoluteTolerances,
    ArtificialColumns,
    Capabilities,
    ColumnBatch,
    ColumnGeneration,
    ColumnGenerationParams,
    ColumnGenerationStatus,
    ColumnTag,
    ColumnType,
    ConvergenceAction,
    ConvergenceContext,
    ConvexityRow,
    IntegerParams,
    IterationInfo,
    MasterInitialization,
    MasterProblem,
    MasterSolver,
    NoInitialization,
    Pricer,
    PricingChanges,
    PricingRequest,
    PricingResult,
    PricingStatus,
    RelativeTolerances,
    RelaxationParams,
    RowBatch,
    RowSense,
    SolverStatus,
    StatisticsParams,
    ToleranceContext,
    Tolerances,
    default_reduced_cost_tolerance,
    default_relative_gap,
    master_solver_self_test,
)

PATH_COST = 10.0


class CoverOneRow:
    """The smallest master with a real choice: one row "cover ≥ 1"; the pricer offers one path
    of cost 10 covering it while its dual is above 10 and the path is allowed.

    With only the artificial column, the LP pays the artificial cost; once the path is in, it
    costs 10.
    """

    def __init__(self, solver: ScriptedMasterSolver | None = None) -> None:
        self.solver = solver if solver else ScriptedMasterSolver()
        self.master = MasterProblem(self.solver)
        self.master.add_rows(1, RowSense.GreaterEqual, 1.0)
        self.pricer = ScaledPricerWithPath(self)
        self.master.add_pricer(self.pricer)
        self.path_allowed = True
        self.solver.on_solve = self._solve

    @staticmethod
    def _solve(lp: ScriptedMasterSolver) -> SolverStatus:
        has_path = len(lp.columns) > 1
        artificial_cost = lp.columns[0].cost
        lp.objective_value = PATH_COST if has_path else artificial_cost
        lp.dual_values = [lp.objective_value]
        lp.values = {0: 0.0 if has_path else 1.0, 1: 1.0}
        return SolverStatus.Optimal


class ScaledPricerWithPath(ScriptedPricer):
    """The pricer of CoverOneRow: counts updates too."""

    def __init__(self, toy: CoverOneRow) -> None:
        super().__init__(self._respond)
        self.toy = toy
        self.updates: list[PricingChanges] = []

    def _respond(self, duals: FloatArray, request: PricingRequest) -> PricingResult:
        result = proven_none()
        reduced_cost = PATH_COST - float(duals[0])
        if self.toy.path_allowed and reduced_cost < request.reduced_cost_threshold:
            result.columns = [make_path(PATH_COST, [(0, 1.0)], reduced_cost)]
        return result

    def update(self, changes: PricingChanges) -> None:
        self.updates.append(changes)


class MisbehavingMasterSolver(ScriptedMasterSolver):
    """A scripted master solver whose duals and row bounds can be replaced by wrong
    answers."""

    def __init__(self) -> None:
        super().__init__()
        self.duals_answer: Any = None
        self.row_bounds_answer: Any = None

    def duals(self) -> FloatArray:
        return super().duals() if self.duals_answer is None else self.duals_answer

    def row_bounds(self) -> tuple[FloatArray, FloatArray]:
        if self.row_bounds_answer is None:
            return super().row_bounds()
        return self.row_bounds_answer


# ── The loop over Python components ──────────────────────────────────────────


def test_converges_and_pushes_the_artificial_column_out() -> None:
    toy = CoverOneRow()
    result = ColumnGeneration(toy.master).run()

    assert result.status == ColumnGenerationStatus.Optimal
    assert result.iterations == 2
    assert result.objective == PATH_COST
    assert result.lower_bound == PATH_COST
    assert result.statistics.generated_columns == 1
    assert result.statistics.artificial_columns == 1
    assert result.statistics.artificial_cost == ArtificialColumns.automatic_cost_factor
    assert result.statistics.total_seconds >= 0.0
    # The rows came through add_rows, the columns through add_columns, as numpy copies.
    assert toy.solver.lower == [1.0] and toy.solver.upper == [math.inf]
    assert toy.solver.columns[0] == (
        0,
        ArtificialColumns.automatic_cost_factor,
        0.0,
        math.inf,
        [(0, 1.0)],
    )
    assert toy.solver.columns[1] == (1, PATH_COST, 0.0, math.inf, [(0, 1.0)])
    # No convexity row: the threshold is minus the reduced-cost tolerance.
    assert len(toy.pricer.requests) == 2
    assert toy.pricer.requests[0].reduced_cost_threshold == -default_reduced_cost_tolerance
    assert toy.pricer.seen_duals[0].dtype == np.float64
    assert toy.pricer.seen_duals[1].tolist() == [PATH_COST]
    assert toy.solver.restore_calls == 1


def test_threshold_includes_the_convexity_dual() -> None:
    solver = ScriptedMasterSolver()
    master = MasterProblem(solver)
    master.add_rows(1, RowSense.GreaterEqual, 1.0)
    pricer = ScriptedPricer()
    assert master.add_pricer(pricer, ConvexityRow(max_paths=4.0)) == 0
    assert master.convexity_row(0) == 1
    assert solver.upper == [math.inf, 4.0]

    def solve(lp: ScriptedMasterSolver) -> SolverStatus:
        lp.dual_values = [12.0, 3.0]
        return SolverStatus.Optimal

    solver.on_solve = solve
    ColumnGeneration(master).run()

    assert pricer.requests[0].reduced_cost_threshold == pytest.approx(3.0 - 1e-9)
    assert pricer.seen_duals[0].tolist() == [12.0, 3.0]


def test_the_iteration_hook_keeps_what_it_is_given() -> None:
    toy = CoverOneRow()
    seen: list[IterationInfo] = []
    params = ColumnGenerationParams(on_iteration=seen.append)
    ColumnGeneration(toy.master, params).run()

    # Each IterationInfo is the hook's own copy, readable after the run.
    assert [info.iteration for info in seen] == [1, 2]
    assert [info.new_columns for info in seen] == [1, 0]
    assert [info.total_columns for info in seen] == [2, 2]
    assert seen[1].objective == PATH_COST
    assert seen[1].elapsed_seconds >= 0.0


def test_the_convergence_hook_can_restart_the_loop() -> None:
    toy = CoverOneRow()
    toy.path_allowed = False
    seen: list[tuple[int, float, FloatArray, FloatArray]] = []

    def on_converged(context: ConvergenceContext) -> ConvergenceAction:
        seen.append((context.iteration, context.objective, context.duals, context.values))
        if not toy.path_allowed:
            toy.path_allowed = True  # e.g. a pricer changed its problem
            context.master.pricer(0).update(PricingChanges(structure=True))
            return ConvergenceAction.Continue
        return ConvergenceAction.Stop

    params = ColumnGenerationParams()
    params.on_converged = on_converged
    result = ColumnGeneration(toy.master, params).run()

    assert result.status == ColumnGenerationStatus.Optimal
    assert result.objective == PATH_COST
    assert len(seen) == 2
    iteration, objective, duals, values = seen[0]
    assert (iteration, objective) == (1, ArtificialColumns.automatic_cost_factor)
    assert duals.tolist() == [ArtificialColumns.automatic_cost_factor]
    assert values.tolist() == [1.0]
    assert seen[1][3].tolist() == [0.0, 1.0]
    assert len(toy.pricer.updates) == 1 and toy.pricer.updates[0].structure


def test_limits_statistics_and_the_integer_solve() -> None:
    toy = CoverOneRow()
    params = ColumnGenerationParams(
        max_iterations=1,
        relaxation=RelaxationParams(threads=2, verbose=True),
        statistics=StatisticsParams(timers=False),
    )
    loop = ColumnGeneration(toy.master, params)
    result = loop.run()

    assert result.status == ColumnGenerationStatus.IterationLimit
    assert result.statistics.relaxation_seconds == 0.0
    assert result.statistics.pricing_seconds == 0.0
    assert toy.solver.relaxation_calls[0].threads == 2
    assert toy.solver.relaxation_calls[0].verbose

    status = loop.solve_restricted_master_integer(
        IntegerParams(column_type=ColumnType.Binary, relative_gap=0.01, timeout_s=5.0)
    )
    assert status == SolverStatus.Optimal
    received = toy.solver.integer_calls[0]
    assert received.column_type == ColumnType.Binary
    assert (received.relative_gap, received.timeout_s) == (0.01, 5.0)


def test_no_initialization_keeps_the_model_as_it_is() -> None:
    solver = ScriptedMasterSolver()
    master = MasterProblem(solver)
    master.add_rows(1, RowSense.GreaterEqual, 1.0)
    master.add_pricer(ScriptedPricer())
    solver.on_solve = lambda lp: SolverStatus.Infeasible
    params = ColumnGenerationParams(initialization=NoInitialization())
    result = ColumnGeneration(master, params).run()

    assert result.status == ColumnGenerationStatus.RestrictedMasterInfeasible
    assert solver.columns == []
    assert master.initialized()


def test_components_and_the_scale_reach_the_loop() -> None:
    solver = ScriptedMasterSolver()
    solver.largest_user_cost_value = 50.0
    solver.dual_tolerance_value = 1e-3
    master = MasterProblem(solver)
    master.add_rows(1, RowSense.GreaterEqual, 1.0)
    pricer = ScaledPricer(20.0)
    master.add_pricer(pricer)
    params = ColumnGenerationParams(
        tolerances=RelativeTolerances(1e-9),
        initialization=ArtificialColumns(7.0),
    )
    loop = ColumnGeneration(master, params)
    result = loop.run()

    # The scale is the largest of the pricer's and the user's costs.
    assert loop.tolerance_context().cost_scale == 50.0
    assert isinstance(loop.params().tolerances, RelativeTolerances)
    assert isinstance(loop.params().initialization, ArtificialColumns)
    assert result.statistics.artificial_cost == 7.0
    assert solver.columns[0].cost == 7.0
    assert master.artificial_cost() == 7.0
    assert master.artificial_column_ids().tolist() == [0]


def test_the_default_components_are_filled_in() -> None:
    toy = CoverOneRow()
    loop = ColumnGeneration(toy.master)
    assert isinstance(loop.params().tolerances, AbsoluteTolerances)
    assert isinstance(loop.params().initialization, ArtificialColumns)
    assert loop.params().on_iteration is None
    assert loop.params().on_converged is None


# ── Errors ───────────────────────────────────────────────────────────────────


def test_a_python_exception_leaves_run_and_the_model_is_restored() -> None:
    toy = CoverOneRow()

    def respond(duals: FloatArray, request: PricingRequest) -> PricingResult:
        raise ValueError("pricing failed")

    toy.pricer.respond = respond
    with pytest.raises(ValueError, match="pricing failed"):
        ColumnGeneration(toy.master).run()
    assert toy.solver.restore_calls == 1


def test_a_hook_exception_leaves_run() -> None:
    toy = CoverOneRow()

    def on_iteration(info: IterationInfo) -> None:
        raise KeyError("hook")

    with pytest.raises(KeyError):
        ColumnGeneration(toy.master, ColumnGenerationParams(on_iteration=on_iteration)).run()


def test_answers_of_the_wrong_size_are_refused() -> None:
    solver = MisbehavingMasterSolver()
    toy = CoverOneRow(solver)
    solver.duals_answer = np.zeros(0)
    with pytest.raises(ValueError, match=r"MasterSolver.duals: expected 1 values, got 0"):
        ColumnGeneration(toy.master).run()

    solver = MisbehavingMasterSolver()
    toy = CoverOneRow(solver)
    solver.duals_answer = "not numbers"
    with pytest.raises(TypeError, match="MasterSolver.duals"):
        ColumnGeneration(toy.master).run()

    solver.row_bounds_answer = ([1.0],)
    with pytest.raises(ValueError, match=r"\(lower, upper\)"):
        MasterSolver.row_bounds(solver)

    solver.duals_answer = [[1.0]]
    with pytest.raises(ValueError, match="one-dimensional"):
        MasterSolver.duals(solver)


def test_a_missing_method_is_reported() -> None:
    solver = MasterSolver()
    with pytest.raises(RuntimeError, match='pure virtual function "MasterSolver::objective"'):
        MasterSolver.objective(solver)
    batch = ColumnBatch(ids=[], cost=[], lower=[], upper=[], starts=[0], row_indices=[], values=[])
    with pytest.raises(RuntimeError, match='pure virtual function "MasterSolver::add_columns"'):
        MasterSolver.add_columns(solver, batch)
    with pytest.raises(RuntimeError, match='pure virtual function "Pricer::price"'):
        Pricer.price(Pricer(), np.array([1.0]))


def test_errors_of_the_master_map_to_python_exceptions() -> None:
    master = MasterProblem(WrappedModelMasterSolver())
    with pytest.raises(RuntimeError, match="cannot add rows"):
        master.add_rows(1, RowSense.Equal, 1.0)
    with pytest.raises(IndexError):
        master.add_pricer(ScriptedPricer(), ConvexityRow(existing_row=3))
    with pytest.raises(ValueError):
        master.add_pricer(ScriptedPricer(), ConvexityRow(existing_row=0, max_paths=1.0))


# ── The master solver's methods from Python ──────────────────────────────────


def test_the_bound_methods_reach_a_python_master_solver_through_cpp() -> None:
    # MasterSolver.<method>(solver) runs the binding, which calls the C++ virtual method,
    # which calls the Python override: both directions of every conversion.
    solver = ScriptedMasterSolver()
    MasterSolver.add_rows(solver, RowBatch([1.0, -math.inf], [1.0, 2.0], ["a", "b"]))
    MasterSolver.add_columns(
        solver,
        ColumnBatch(
            ids=[7, 3],
            cost=[1.0, 2.0],
            lower=[0.0, 0.0],
            upper=[math.inf, 1.0],
            starts=[0, 1, 3],
            row_indices=[0, 0, 1],
            values=[1.0, 2.0, 3.0],
        ),
    )
    assert solver.names == ["a", "b"]
    assert solver.columns == [
        (7, 1.0, 0.0, math.inf, [(0, 1.0)]),
        (3, 2.0, 0.0, 1.0, [(0, 2.0), (1, 3.0)]),
    ]
    assert MasterSolver.number_of_rows(solver) == 2
    lower, upper = MasterSolver.row_bounds(solver)
    assert lower.tolist() == [1.0, -math.inf] and upper.tolist() == [1.0, 2.0]

    solver.objective_value = 4.5
    solver.dual_values = [1.5, -0.5]
    solver.values = {7: 0.25, 3: 1.0}
    assert (
        MasterSolver.solve_relaxation(solver, RelaxationParams(timeout_s=3.0))
        == SolverStatus.Optimal
    )
    assert solver.relaxation_calls[0].timeout_s == 3.0
    assert MasterSolver.objective(solver) == 4.5
    assert MasterSolver.duals(solver).tolist() == [1.5, -0.5]
    assert MasterSolver.primal(solver, np.array([3, 7], dtype=np.uint64)).tolist() == [1.0, 0.25]
    assert MasterSolver.solve_integer(solver) == SolverStatus.Optimal
    assert solver.integer_calls[0].column_type == ColumnType.Integer
    assert MasterSolver.farkas(solver) is None

    values: list[OptionValue] = [True, 3, 0.5, "dual"]
    for value in values:
        MasterSolver.set_option(solver, "option", value)
    assert solver.options == [("option", True), ("option", 3), ("option", 0.5), ("option", "dual")]
    assert [type(value) for _, value in solver.options] == [bool, int, float, str]

    capabilities = MasterSolver.capabilities(solver)
    assert capabilities.add_rows and capabilities.integer_solve
    assert not capabilities.farkas and not capabilities.basis
    solver.dual_tolerance_value = 1e-7
    solver.largest_user_cost_value = 9.0
    assert MasterSolver.dual_tolerance(solver) == 1e-7
    assert MasterSolver.largest_user_cost(solver) == 9.0
    MasterSolver.restore_user_model(solver)
    assert solver.restore_calls == 1


def test_the_optional_methods_have_defaults() -> None:
    solver = WrappedModelMasterSolver()
    with pytest.raises(RuntimeError, match="cannot add rows"):
        MasterSolver.add_rows(solver, RowBatch([0.0], [1.0]))
    assert MasterSolver.farkas(solver) is None
    assert MasterSolver.dual_tolerance(solver) == 0.0
    assert MasterSolver.largest_user_cost(solver) == 0.0
    MasterSolver.restore_user_model(solver)
    assert MasterSolver.duals(solver).tolist() == []


def test_a_farkas_ray_comes_back_as_an_array() -> None:
    class WithRay(ScriptedMasterSolver):
        def farkas(self) -> FloatArray | None:
            return np.array([1.0, -2.0])

    solver = WithRay()
    MasterSolver.add_rows(solver, RowBatch([0.0, 0.0], [1.0, 1.0]))
    ray = MasterSolver.farkas(solver)
    assert ray is not None and ray.tolist() == [1.0, -2.0]


def test_batches_are_checked() -> None:
    good: dict[str, Any] = dict(
        ids=[0], cost=[1.0], lower=[0.0], upper=[1.0], starts=[0, 1], row_indices=[0], values=[1.0]
    )
    batch = ColumnBatch(**good)
    assert len(batch) == 1
    assert batch.ids.dtype == np.uint64 and batch.starts.dtype == np.uint32
    assert batch.cost.tolist() == [1.0] and batch.lower.tolist() == [0.0]
    assert batch.upper.tolist() == [1.0] and batch.row_indices.tolist() == [0]
    assert batch.values.tolist() == [1.0]
    wrong_sizes: list[dict[str, Any]] = [{"cost": [1.0, 2.0]}, {"starts": [0]}, {"values": []}]
    for change in wrong_sizes:
        with pytest.raises(ValueError, match="one value per id"):
            ColumnBatch(**(good | change))
    for starts in ([1, 1], [0, 0]):
        with pytest.raises(ValueError, match="starts must rise"):
            ColumnBatch(**(good | {"starts": starts}))
    with pytest.raises(ValueError, match="one-dimensional"):
        batch_input: dict[str, Any] = good | {"cost": [[1.0]]}
        ColumnBatch(**batch_input)

    rows = RowBatch([0.0, 1.0], [2.0, 3.0])
    assert len(rows) == 2 and rows.names == []
    assert rows.lower.tolist() == [0.0, 1.0] and rows.upper.tolist() == [2.0, 3.0]
    with pytest.raises(ValueError, match="same size"):
        RowBatch([0.0], [1.0, 2.0])
    with pytest.raises(ValueError, match="same size"):
        RowBatch([0.0], [1.0], ["a", "b"])


# ── Pricers ──────────────────────────────────────────────────────────────────


def test_a_python_pricer_through_cpp() -> None:
    pricer = ScriptedPricer()
    result = Pricer.price(
        pricer, np.array([1.0, 2.0]), PricingRequest(reduced_cost_threshold=-1.0, timeout_s=2.0)
    )
    assert result.status == PricingStatus.Optimal and result.exact_stage
    assert result.min_reduced_cost_bound == 0.0 and result.columns == []
    assert pricer.seen_duals[0].tolist() == [1.0, 2.0]
    assert (pricer.requests[0].reduced_cost_threshold, pricer.requests[0].timeout_s) == (-1.0, 2.0)
    # The optional methods keep the base class's behavior.
    assert Pricer.cost_scale(pricer) == 1.0
    Pricer.update(pricer, PricingChanges(costs=True, bounds=True))

    scaled = ScaledPricer(3.0)
    assert Pricer.cost_scale(scaled) == 3.0
    Pricer.update(scaled, PricingChanges(costs=True, bounds=True))
    assert scaled.updates[0].costs and scaled.updates[0].bounds and not scaled.updates[0].structure


def test_pricing_result_defaults_and_columns() -> None:
    result = PricingResult()
    assert result.status == PricingStatus.Unknown
    assert result.min_reduced_cost_bound == -math.inf
    assert not result.exact_stage
    path = make_path(5.0, [(0, 1.0)], -1.0)
    result.columns = [path]
    result.status = PricingStatus.SolverLimit
    assert result.columns[0].column.cost == 5.0
    request = PricingRequest()
    assert request.reduced_cost_threshold == 0.0 and request.timeout_s == math.inf


# ── The master problem ───────────────────────────────────────────────────────


def test_the_master_problem_from_python() -> None:
    solver = ScriptedMasterSolver()
    master = MasterProblem(solver)
    rows = master.add_rows([1.0, 0.0], [1.0, 5.0], ["visit", "load"])
    assert (rows.first, rows.count) == (0, 2)
    assert solver.names == ["visit", "load"]
    more = master.add_rows(1, RowSense.LessEqual, 3.0)
    assert (more.first, more.count) == (2, 1)
    assert master.number_of_rows() == 3

    pricer = ScriptedPricer()
    assert master.add_pricer(pricer, ConvexityRow(existing_row=2)) == 0
    assert master.number_of_pricers() == 1
    assert master.pricer(0) is pricer
    assert master.solver() is solver

    master.add_columns(0, [make_path(4.0, [(0, 1.0)], -1.0), make_path(6.0, [(1, 2.0)], -2.0)])
    master.add_artificial_columns([0], [1.0], 99.0)
    assert master.number_of_columns() == 3
    columns = master.columns()
    assert [column.id for column in columns] == [0, 1, 2]
    assert [column.tag for column in columns] == [
        ColumnTag.Real,
        ColumnTag.Real,
        ColumnTag.Artificial,
    ]
    assert columns[1].pricer == 0 and columns[2].pricer == MasterProblem.no_pricer
    assert columns[1].solution.column.cost == 6.0
    # The convexity row gets the 1 of each column.
    assert solver.columns[0].entries == [(0, 1.0), (2, 1.0)]
    assert master.artificial_column_ids().tolist() == [2]
    assert master.artificial_cost() == 99.0

    assert not master.initialized()
    master.mark_initialized()
    assert master.initialized()


def test_the_loop_keeps_its_master_solver_and_pricers_alive() -> None:
    calls: list[str] = []

    def build() -> MasterProblem:
        solver = ScriptedMasterSolver()
        solver.on_solve = lambda lp: calls.append("solve") or SolverStatus.Optimal
        master = MasterProblem(solver)
        master.add_rows(1, RowSense.GreaterEqual, 1.0)
        master.add_pricer(
            ScriptedPricer(lambda duals, request: calls.append("price") or proven_none())
        )
        return master

    loop = ColumnGeneration(build())
    gc.collect()
    result = loop.run()
    # Only the loop refers to the master, and only the master to the Python solver and pricer.
    assert result.status == ColumnGenerationStatus.Optimal
    assert calls == ["solve", "price"]


# ── Tolerances and initializations ───────────────────────────────────────────


def test_tolerances() -> None:
    context = ToleranceContext(cost_scale=10.0)
    absolute = AbsoluteTolerances(1e-6, 1e-5, 1e-4)
    assert absolute.reduced_cost(context) == 1e-6
    assert absolute.feasibility(context) == 1e-5
    assert absolute.optimality_gap(context) == 1e-4
    relative = RelativeTolerances(1e-6, 1e-5, 1e-4)
    assert relative.reduced_cost(context) == pytest.approx(1e-5)
    assert relative.feasibility(context) == 1e-5
    assert relative.optimality_gap(context) == pytest.approx(1e-3)
    assert ToleranceContext().cost_scale == 1.0

    params = ColumnGenerationParams()
    assert params.tolerances is None and params.initialization is None
    params.tolerances = relative
    assert params.tolerances is relative
    params.tolerances = None
    assert params.tolerances is None


def test_initializations() -> None:
    context = ToleranceContext(cost_scale=2.0)
    assert ArtificialColumns().cost(context) == 2.0 * ArtificialColumns.automatic_cost_factor
    assert ArtificialColumns(5.0).cost(context) == 5.0

    solver = ScriptedMasterSolver()
    master = MasterProblem(solver)
    master.add_rows(2, RowSense.GreaterEqual, 1.0)
    ArtificialColumns(3.0).initialize(master, context)
    assert [column.entries for column in solver.columns] == [[(0, 1.0)], [(1, 1.0)]]
    NoInitialization().initialize(master, context)
    assert len(solver.columns) == 2


def test_params_and_options_read_back() -> None:
    params = ColumnGenerationParams(max_iterations=5, timeout_s=60.0)
    assert (params.max_iterations, params.timeout_s) == (5, 60.0)
    params.max_iterations = 7
    params.relaxation = RelaxationParams(timeout_s=1.0)
    params.statistics = StatisticsParams(timers=False)
    params.initialization = NoInitialization()
    assert params.max_iterations == 7
    assert params.relaxation.timeout_s == 1.0 and not params.statistics.timers
    assert isinstance(params.initialization, NoInitialization)

    seen: list[IterationInfo] = []
    params.on_iteration = seen.append
    assert callable(params.on_iteration)
    params.on_iteration = None
    assert params.on_iteration is None

    capabilities = Capabilities(farkas=True, basis=True)
    capabilities.add_rows = True
    assert capabilities.farkas and capabilities.basis and capabilities.add_rows
    convexity = ConvexityRow()
    assert convexity.existing_row is None and convexity.max_paths == math.inf
    integer = IntegerParams()
    assert integer.relative_gap == default_relative_gap
    assert integer.threads == 0 and not integer.verbose


# ── Tolerances and initializations written in Python ─────────────────────────


class HalfTolerances(Tolerances):
    """Tolerances computed in Python: a reduced-cost tolerance of half the cost scale."""

    def __init__(self) -> None:
        super().__init__()
        self.contexts: list[ToleranceContext] = []

    def reduced_cost(self, context: ToleranceContext) -> float:
        self.contexts.append(context)
        return 0.5 * context.cost_scale

    def feasibility(self, context: ToleranceContext) -> float:
        return 1e-6

    def optimality_gap(self, context: ToleranceContext) -> float:
        return 1e-6


class StartingPath(MasterInitialization):
    """An initialization written in Python: the path of CoverOneRow as a starting column."""

    def __init__(self) -> None:
        super().__init__()
        self.calls: list[tuple[MasterProblem, float]] = []

    def initialize(self, master: MasterProblem, context: ToleranceContext) -> None:
        self.calls.append((master, context.cost_scale))
        master.add_columns(0, [make_path(PATH_COST, [(0, 1.0)], 0.0)])


def test_python_tolerances_set_the_threshold() -> None:
    toy = CoverOneRow()
    loop = ColumnGeneration(toy.master, ColumnGenerationParams(tolerances=HalfTolerances()))
    gc.collect()
    loop.run()

    # Only the loop's params refer to the tolerances, and they are still the Python object.
    tolerances = loop.params().tolerances
    assert isinstance(tolerances, HalfTolerances)
    assert tolerances.contexts[0].cost_scale == 1.0
    assert toy.pricer.requests[0].reduced_cost_threshold == -0.5
    # Through C++ too.
    assert Tolerances.reduced_cost(tolerances, ToleranceContext(cost_scale=4.0)) == 2.0
    assert Tolerances.feasibility(tolerances, ToleranceContext()) == 1e-6
    assert Tolerances.optimality_gap(tolerances, ToleranceContext()) == 1e-6


def test_a_python_initialization_starts_the_master() -> None:
    toy = CoverOneRow()
    initialization = StartingPath()
    loop = ColumnGeneration(toy.master, ColumnGenerationParams(initialization=initialization))
    result = loop.run()

    # The path is in from the start: no artificial column, and nothing better to price.
    assert result.status == ColumnGenerationStatus.Optimal
    assert result.objective == PATH_COST
    assert result.statistics.artificial_columns == 0
    assert len(initialization.calls) == 1
    master, cost_scale = initialization.calls[0]
    assert master is toy.master and cost_scale == 1.0
    # Once per master.
    loop.run()
    assert len(initialization.calls) == 1


def test_missing_tolerance_and_initialization_methods_are_reported() -> None:
    with pytest.raises(RuntimeError, match='pure virtual function "Tolerances::reduced_cost"'):
        Tolerances.reduced_cost(Tolerances(), ToleranceContext())
    master = MasterProblem(ScriptedMasterSolver())
    with pytest.raises(
        RuntimeError, match='pure virtual function "MasterInitialization::initialize"'
    ):
        MasterInitialization.initialize(MasterInitialization(), master, ToleranceContext())


# ── The master solver self-test ──────────────────────────────────────────────


def test_a_correct_python_master_solver_passes_the_self_test() -> None:
    report = master_solver_self_test(SelfTestMasterSolver(), SelfTestMasterSolver(maximizing=True))
    assert report.passed()
    assert report.failures == []


def test_the_self_test_reports_the_mistakes_of_a_python_master_solver() -> None:
    maximizing = SelfTestMasterSolver(maximizing=True)
    report = master_solver_self_test(
        SelfTestMasterSolver(Mistake.FLIPS_LESS_EQUAL_DUAL), maximizing
    )
    assert not report.passed()
    assert report.failures == [
        "minimizing model: first LP: dual of row 2 (<=): expected -0.5, got 0.5"
    ]

    report = master_solver_self_test(
        SelfTestMasterSolver(),
        SelfTestMasterSolver(Mistake.KEEPS_MAXIMIZATION, maximizing=True),
        tolerance=1e-9,
    )
    expected = "maximizing model: first LP: objective: expected 3.75, got -3.75"
    assert any(expected in failure for failure in report.failures)

    report = master_solver_self_test(
        SelfTestMasterSolver(Mistake.VALUES_BY_POSITION), SelfTestMasterSolver(maximizing=True)
    )
    expected = "first LP: value of x3 (id 5): expected 0.25, got 1.25"
    assert any(expected in failure for failure in report.failures)


def test_the_self_test_needs_master_solvers_that_add_rows() -> None:
    with pytest.raises(ValueError):
        master_solver_self_test(
            SelfTestMasterSolver(Mistake.CANNOT_ADD_ROWS), SelfTestMasterSolver(maximizing=True)
        )


# The module of GurobipyMasterSolver, loaded on first use.
gurobipy_master_solver_module = "rcspp.column_generation.master_solvers.gurobipy_master_solver"


# ── Master solvers over a model ──────────────────────────────────────────────


def test_importing_rcspp_does_not_load_gurobipy() -> None:
    # A new process: this one may have loaded gurobipy already.
    loaded = subprocess.run(
        [sys.executable, "-c", "import sys, rcspp; print('gurobipy' in sys.modules)"],
        capture_output=True,
        text=True,
        check=True,
        env={**os.environ, "PYTHONPATH": os.pathsep.join(sys.path)},
    )
    assert loaded.stdout.strip() == "False"


def test_the_gurobipy_master_solver_without_gurobipy_names_the_extra(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    # As if the class had never been loaded, then as if gurobipy were not installed (last:
    # delattr reads the attribute, which loads the class).
    monkeypatch.delattr(column_generation.master_solvers, "GurobipyMasterSolver", raising=False)
    monkeypatch.delattr(column_generation.master_solvers, "gurobipy_master_solver", raising=False)
    monkeypatch.delitem(sys.modules, gurobipy_master_solver_module, raising=False)
    monkeypatch.setitem(sys.modules, "gurobipy", None)

    with pytest.raises(ImportError, match=r"pip install rcspp\[gurobipy\]"):
        column_generation.GurobipyMasterSolver  # noqa: B018


def test_an_unknown_name_is_still_an_attribute_error() -> None:
    with pytest.raises(AttributeError):
        column_generation.NoSuchMasterSolver  # noqa: B018
    with pytest.raises(AttributeError):
        column_generation.master_solvers.NoSuchMasterSolver  # noqa: B018
