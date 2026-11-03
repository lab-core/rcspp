#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The pure-Python column generation loop over master solvers and pricers written in
Python (the cases of test_column_generation.py), its options and its results."""

import gc
import math
import os
import sys
import time
from dataclasses import FrozenInstanceError, replace
from typing import Any

import numpy as np
import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from python_column_generation_fakes import (  # noqa: E402
    ScaledPricer,
    ScriptedMasterSolver,
    ScriptedPricer,
    make_path,
    proven_none,
)

from rcspp.graph import ResourceGraph  # noqa: E402
from rcspp.python_column_generation import (  # noqa: E402
    DEFAULT_REDUCED_COST_TOLERANCE,
    AbsoluteTolerances,
    ArtificialColumns,
    ColumnGeneration,
    ColumnGenerationParams,
    ColumnGenerationResult,
    ColumnGenerationStatus,
    ColumnType,
    ConvergenceAction,
    ConvergenceContext,
    ConvexityRow,
    GraphPricer,
    IntegerParams,
    IterationInfo,
    MasterInitialization,
    MasterProblem,
    NoInitialization,
    PricingChanges,
    PricingRequest,
    PricingResult,
    PricingStatus,
    RelativeTolerances,
    RelaxationParams,
    RowSense,
    SolverStatus,
    Statistics,
    StatisticsParams,
    ToleranceContext,
    Tolerances,
)
from rcspp.python_column_generation.types import FloatArray  # noqa: E402
from rcspp.resource import (  # noqa: E402
    AdditionExtensionFunction,
    TrivialFeasibilityFunction,
    ValueCostFunction,
    ValueDominanceFunction,
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
        self.solver.on_solve = self.solve

    @staticmethod
    def solve(lp: ScriptedMasterSolver) -> SolverStatus:
        has_path = len(lp.columns) > 1
        artificial_cost = lp.columns[0].cost
        lp.objective_value = PATH_COST if has_path else artificial_cost
        lp.dual_values = [lp.objective_value]
        lp.values = {0: 0.0 if has_path else 1.0, 1: 1.0}
        return SolverStatus.Optimal


class ScaledPricerWithPath(ScriptedPricer):
    """The pricer of CoverOneRow: counts updates too."""

    def __init__(self, toy: CoverOneRow) -> None:
        super().__init__(self.respond_with_path)
        self.toy = toy
        self.updates: list[PricingChanges] = []

    def respond_with_path(self, duals: FloatArray, request: PricingRequest) -> PricingResult:
        result = proven_none()
        reduced_cost = PATH_COST - float(duals[0])
        if self.toy.path_allowed and reduced_cost < request.reduced_cost_threshold:
            result.columns = [make_path(PATH_COST, [(0, 1.0)], reduced_cost)]
        return result

    def update(self, changes: PricingChanges) -> None:
        self.updates.append(changes)


class MisbehavingMasterSolver(ScriptedMasterSolver):
    """A scripted master solver whose duals can be replaced by a wrong answer."""

    def __init__(self) -> None:
        super().__init__()
        self.duals_answer: Any = None

    def duals(self) -> FloatArray:
        return super().duals() if self.duals_answer is None else self.duals_answer


def run_with_answer(answer: PricingResult) -> ColumnGenerationResult:
    """A run over one row whose pricer always gives `answer`."""
    solver = ScriptedMasterSolver()
    master = MasterProblem(solver)
    master.add_rows(1, RowSense.GreaterEqual, 1.0)
    master.add_pricer(ScriptedPricer(lambda duals, request: answer))
    return ColumnGeneration(master).run()


# ── The loop ─────────────────────────────────────────────────────────────────


def test_converges_and_pushes_the_artificial_column_out() -> None:
    toy = CoverOneRow()
    result = ColumnGeneration(toy.master).run()

    assert result.status == ColumnGenerationStatus.Optimal
    assert result.iterations == 2
    assert result.objective == PATH_COST
    assert result.lower_bound == PATH_COST
    assert result.statistics.generated_columns == 1
    assert result.statistics.artificial_columns == 1
    assert result.statistics.artificial_cost == ArtificialColumns.AUTOMATIC_COST_FACTOR
    assert result.statistics.total_seconds >= 0.0
    # The rows came through add_rows, the columns through add_columns.
    assert toy.solver.lower == [1.0] and toy.solver.upper == [math.inf]
    assert toy.solver.columns[0] == (
        0,
        ArtificialColumns.AUTOMATIC_COST_FACTOR,
        0.0,
        math.inf,
        [(0, 1.0)],
    )
    assert toy.solver.columns[1] == (1, PATH_COST, 0.0, math.inf, [(0, 1.0)])
    # No convexity row: the threshold is minus the reduced-cost tolerance.
    assert len(toy.pricer.requests) == 2
    assert toy.pricer.requests[0].reduced_cost_threshold == -DEFAULT_REDUCED_COST_TOLERANCE
    assert toy.pricer.seen_duals[0].dtype == np.float64
    assert toy.pricer.seen_duals[1].tolist() == [PATH_COST]
    assert toy.solver.restore_calls == 1


def test_threshold_includes_the_convexity_dual() -> None:
    solver = ScriptedMasterSolver()
    master = MasterProblem(solver)
    master.add_rows(1, RowSense.GreaterEqual, 1.0)
    pricer = ScriptedPricer()
    assert master.add_pricer(pricer, ConvexityRow(max_paths=4.0)) == 0
    assert master.pricers[0].convexity_row == 1
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

    assert [info.iteration for info in seen] == [1, 2]
    assert [info.new_columns for info in seen] == [1, 0]
    assert [info.total_columns for info in seen] == [2, 2]
    assert seen[1].objective == PATH_COST
    assert seen[1].elapsed_seconds >= seen[0].elapsed_seconds >= 0.0
    with pytest.raises(AttributeError):
        seen[0].iteration = 5  # type: ignore[misc]


def test_the_convergence_hook_can_restart_the_loop() -> None:
    toy = CoverOneRow()
    toy.path_allowed = False
    seen: list[tuple[int, float, FloatArray, FloatArray]] = []

    def on_converged(context: ConvergenceContext) -> ConvergenceAction:
        seen.append((context.iteration, context.objective, context.duals, context.values))
        assert context.master is toy.master
        if not toy.path_allowed:
            toy.path_allowed = True  # e.g. a pricer changed its problem
            context.master.pricers[0].pricer.update(PricingChanges(structure=True))
            return ConvergenceAction.Continue
        return ConvergenceAction.Stop

    result = ColumnGeneration(toy.master, ColumnGenerationParams(on_converged=on_converged)).run()

    assert result.status == ColumnGenerationStatus.Optimal
    assert result.objective == PATH_COST
    assert len(seen) == 2
    iteration, objective, duals, values = seen[0]
    assert (iteration, objective) == (1, ArtificialColumns.AUTOMATIC_COST_FACTOR)
    assert duals.tolist() == [ArtificialColumns.AUTOMATIC_COST_FACTOR]
    assert values.tolist() == [1.0]
    assert seen[1][0] == 3 and seen[1][3].tolist() == [0.0, 1.0]
    assert len(toy.pricer.updates) == 1 and toy.pricer.updates[0].structure


def test_the_convergence_hook_stopping_ends_the_run_as_converged() -> None:
    toy = CoverOneRow()
    result = ColumnGeneration(
        toy.master, ColumnGenerationParams(on_converged=lambda context: ConvergenceAction.Stop)
    ).run()
    assert result.status == ColumnGenerationStatus.Optimal and result.iterations == 2


def test_the_hooks_are_not_called_without_a_proof() -> None:
    called: list[str] = []
    solver = ScriptedMasterSolver()
    master = MasterProblem(solver)
    master.add_rows(1, RowSense.GreaterEqual, 1.0)
    master.add_pricer(ScriptedPricer(lambda duals, request: PricingResult()))

    def on_converged(context: ConvergenceContext) -> ConvergenceAction:
        called.append("converged")
        return ConvergenceAction.Stop

    params = ColumnGenerationParams(on_converged=on_converged)
    result = ColumnGeneration(master, params).run()

    assert result.status == ColumnGenerationStatus.NotProven
    assert called == []


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
    assert result.iterations == 1
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


def test_no_iteration_allowed_solves_nothing() -> None:
    toy = CoverOneRow()
    result = ColumnGeneration(toy.master, ColumnGenerationParams(max_iterations=0)).run()
    assert result.status == ColumnGenerationStatus.IterationLimit
    assert result.iterations == 0 and toy.solver.relaxation_calls == []


def test_timers_measure_the_lp_solves_and_the_pricing() -> None:
    def slow(lp: ScriptedMasterSolver) -> SolverStatus:
        time.sleep(0.02)
        return CoverOneRow.solve(lp)

    def slow_pricing(duals: FloatArray, request: PricingRequest) -> PricingResult:
        time.sleep(0.02)
        return toy.pricer.respond_with_path(duals, request)

    toy = CoverOneRow()
    toy.solver.on_solve = slow
    toy.pricer.respond = slow_pricing
    # Two iterations: each solves and prices once.
    statistics = ColumnGeneration(toy.master).run().statistics

    assert 0.04 <= statistics.relaxation_seconds <= statistics.total_seconds
    assert 0.04 <= statistics.pricing_seconds <= statistics.total_seconds


def test_the_default_run_has_no_limit() -> None:
    params = ColumnGenerationParams()
    assert params.max_iterations == math.inf and params.timeout_s == math.inf
    toy = CoverOneRow()
    ColumnGeneration(toy.master).run()
    assert toy.solver.relaxation_calls[0].timeout_s == math.inf
    assert toy.pricer.requests[0].timeout_s == math.inf


def test_the_time_left_caps_the_lp_solves_and_the_pricing() -> None:
    toy = CoverOneRow()
    params = ColumnGenerationParams(timeout_s=30.0, relaxation=RelaxationParams(timeout_s=100.0))
    ColumnGeneration(toy.master, params).run()

    assert 29.0 < toy.solver.relaxation_calls[0].timeout_s <= 30.0
    assert 29.0 < toy.pricer.requests[0].timeout_s <= 30.0
    # The LP's own limit when it is the smaller.
    other = CoverOneRow()
    params = ColumnGenerationParams(timeout_s=30.0, relaxation=RelaxationParams(timeout_s=5.0))
    ColumnGeneration(other.master, params).run()
    assert other.solver.relaxation_calls[0].timeout_s == 5.0


def test_a_run_without_time_stops_before_the_first_solve() -> None:
    toy = CoverOneRow()
    result = ColumnGeneration(toy.master, ColumnGenerationParams(timeout_s=0.0)).run()

    assert result.status == ColumnGenerationStatus.TimeLimit
    assert result.iterations == 0 and result.objective is None
    assert toy.solver.relaxation_calls == []
    assert toy.solver.restore_calls == 1


def test_pricing_cut_short_by_the_time_limit_is_a_time_limit() -> None:
    solver = ScriptedMasterSolver()
    master = MasterProblem(solver)
    master.add_rows(1, RowSense.GreaterEqual, 1.0)

    def out_of_time(duals: FloatArray, request: PricingRequest) -> PricingResult:
        time.sleep(request.timeout_s + 0.01)
        return PricingResult(status=PricingStatus.SolverLimit)

    master.add_pricer(ScriptedPricer(out_of_time))
    result = ColumnGeneration(master, ColumnGenerationParams(timeout_s=0.05)).run()
    assert result.status == ColumnGenerationStatus.TimeLimit


@pytest.mark.parametrize(
    ("solver_status", "run_status"),
    [
        (SolverStatus.Infeasible, ColumnGenerationStatus.RestrictedMasterInfeasible),
        (SolverStatus.Unbounded, ColumnGenerationStatus.Unbounded),
        (SolverStatus.TimeLimit, ColumnGenerationStatus.TimeLimit),
        (SolverStatus.Other, ColumnGenerationStatus.SolverError),
    ],
)
def test_a_failed_lp_solve_ends_the_run(
    solver_status: SolverStatus, run_status: ColumnGenerationStatus
) -> None:
    toy = CoverOneRow()
    toy.solver.on_solve = lambda lp: solver_status
    result = ColumnGeneration(toy.master).run()

    assert result.status == run_status
    assert result.iterations == 0 and result.objective is None
    assert result.lower_bound == -math.inf
    assert toy.pricer.requests == []
    assert toy.solver.restore_calls == 1


def test_a_status_that_is_not_a_solver_status_is_refused() -> None:
    toy = CoverOneRow()
    not_a_status: Any = 1
    toy.solver.on_solve = lambda lp: not_a_status
    message = "^MasterSolver.solve_relaxation: expected a SolverStatus, got int$"
    with pytest.raises(TypeError, match=message):
        ColumnGeneration(toy.master).run()
    assert toy.solver.restore_calls == 1


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
    assert master.initialized


def test_no_initialization_runs_a_feasible_model_as_it_is() -> None:
    toy = CoverOneRow()
    toy.master.add_columns(0, [make_path(12.0, [(0, 1.0)], 0.0)])

    def solve(lp: ScriptedMasterSolver) -> SolverStatus:
        cheaper = len(lp.columns) > 1
        lp.objective_value = PATH_COST if cheaper else 12.0
        lp.dual_values = [lp.objective_value]
        lp.values = {0: 0.0 if cheaper else 1.0, 1: 1.0}
        return SolverStatus.Optimal

    toy.solver.on_solve = solve
    params = ColumnGenerationParams(initialization=NoInitialization())
    result = ColumnGeneration(toy.master, params).run()

    assert result.status == ColumnGenerationStatus.Optimal and result.objective == PATH_COST
    assert result.statistics.artificial_columns == 0
    assert toy.master.artificial_column_ids.tolist() == []
    assert toy.master.number_of_columns == 2


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
    assert loop.tolerance_context.cost_scale == 1.0  # set by the first run
    result = loop.run()

    # The scale is the largest of the pricer's and the user's costs.
    assert loop.tolerance_context.cost_scale == 50.0
    assert isinstance(loop.params.tolerances, RelativeTolerances)
    assert isinstance(loop.params.initialization, ArtificialColumns)
    assert result.statistics.artificial_cost == 7.0
    assert solver.columns[0].cost == 7.0
    assert master.artificial_cost == 7.0
    assert master.artificial_column_ids.tolist() == [0]


def test_the_columns_of_the_master_count_in_the_scale() -> None:
    toy = CoverOneRow()
    toy.master.add_columns(0, [make_path(250.0, [(0, 1.0)], 0.0)])
    loop = ColumnGeneration(toy.master)
    loop.run()
    assert loop.tolerance_context.cost_scale == 250.0


def scale_of(pricers: list[float], starting_cost: float | None, user_cost: float) -> Any:
    """The loop and the master of a run over one row with pricers of these scales, a
    starting column of the first pricer, and the user's own costs."""
    solver = ScriptedMasterSolver()
    solver.largest_user_cost_value = user_cost
    master = MasterProblem(solver)
    master.add_rows(1, RowSense.GreaterEqual, 1.0)
    for scale in pricers:
        master.add_pricer(ScaledPricer(scale))
    if starting_cost is not None:
        master.add_columns(0, [make_path(starting_cost, [(0, 1.0)], 0.0)])
    loop = ColumnGeneration(master)
    loop.run()
    return loop, master


def test_the_scale_is_the_largest_cost_magnitude() -> None:
    loop, _ = scale_of([0.5, 5.0], None, 0.0)
    assert loop.tolerance_context.cost_scale == 5.0

    loop, master = scale_of([0.5], -20.0, 0.0)
    assert loop.tolerance_context.cost_scale == 20.0
    assert master.pricers[0].pricer.updates == []

    loop, master = scale_of([0.5], None, 30.0)
    assert loop.tolerance_context.cost_scale == 30.0
    assert master.artificial_cost == ArtificialColumns.AUTOMATIC_COST_FACTOR * 30.0


def test_the_tolerance_context_is_frozen() -> None:
    toy = CoverOneRow()
    loop = ColumnGeneration(toy.master)
    loop.run()
    with pytest.raises(FrozenInstanceError):
        loop.tolerance_context.cost_scale = 1e9  # type: ignore[misc]
    assert loop.tolerance_context.cost_scale == 1.0


def test_the_reduced_cost_tolerance_is_never_below_the_solvers() -> None:
    toy = CoverOneRow()
    toy.solver.dual_tolerance_value = 1e-3
    ColumnGeneration(toy.master).run()
    assert toy.pricer.requests[0].reduced_cost_threshold == -1e-3

    # A tolerance above the solver's is kept.
    toy = CoverOneRow()
    toy.solver.dual_tolerance_value = 1e-6
    ColumnGeneration(toy.master, ColumnGenerationParams(tolerances=AbsoluteTolerances(1e-4))).run()
    assert toy.pricer.requests[0].reduced_cost_threshold == -1e-4


def test_the_default_components_are_filled_in() -> None:
    toy = CoverOneRow()
    loop = ColumnGeneration(toy.master)
    assert isinstance(loop.params.tolerances, AbsoluteTolerances)
    assert isinstance(loop.params.initialization, ArtificialColumns)
    assert loop.params.on_iteration is None
    assert loop.params.on_converged is None


def test_the_loop_keeps_the_params_it_was_given() -> None:
    toy = CoverOneRow()
    params = ColumnGenerationParams(max_iterations=1, relaxation=RelaxationParams(threads=2))
    loop = ColumnGeneration(toy.master, params)

    # Frozen: a changed copy is another set of params, which the loop does not see.
    with pytest.raises(FrozenInstanceError):
        params.max_iterations = 5  # type: ignore[misc]
    assert replace(params, max_iterations=5).max_iterations == 5
    assert loop.params == params
    assert loop.run().status == ColumnGenerationStatus.IterationLimit
    assert toy.solver.relaxation_calls[0].threads == 2


def test_the_options_of_an_lp_solve_are_the_loops_own_copy() -> None:
    toy = CoverOneRow()
    relaxation = RelaxationParams(threads=3)
    ColumnGeneration(toy.master, ColumnGenerationParams(relaxation=relaxation)).run()
    first, second = toy.solver.relaxation_calls
    assert first is not relaxation and first is not second
    assert relaxation.timeout_s == math.inf


def test_a_second_run_continues_from_the_columns_it_has() -> None:
    toy = CoverOneRow()
    loop = ColumnGeneration(toy.master)
    loop.run()
    result = loop.run()

    assert result.status == ColumnGenerationStatus.Optimal
    assert result.iterations == 1 and result.statistics.generated_columns == 0
    assert toy.solver.restore_calls == 2
    assert len(toy.solver.columns) == 2


def test_the_master_is_initialized_by_the_first_loop_only() -> None:
    toy = CoverOneRow()
    ColumnGeneration(toy.master).run()
    ColumnGeneration(toy.master).run()
    assert len(toy.solver.columns) == 2 and toy.master.artificial_column_ids.tolist() == [0]


# ── What proves convergence ──────────────────────────────────────────────────


def test_an_artificial_column_in_use_at_convergence_is_infeasible() -> None:
    toy = CoverOneRow()
    toy.path_allowed = False
    result = ColumnGeneration(toy.master).run()

    assert result.status == ColumnGenerationStatus.Infeasible
    assert result.lower_bound == -math.inf
    assert result.objective == ArtificialColumns.AUTOMATIC_COST_FACTOR


def test_an_artificial_column_below_the_feasibility_tolerance_is_not_in_use() -> None:
    toy = CoverOneRow()
    toy.path_allowed = False

    def tiny(lp: ScriptedMasterSolver) -> SolverStatus:
        status = CoverOneRow.solve(lp)
        lp.values = {0: 1e-7}
        return status

    toy.solver.on_solve = tiny
    result = ColumnGeneration(toy.master).run()
    assert result.status == ColumnGenerationStatus.Optimal


def test_a_pricer_proves_with_an_exact_stage_that_finished() -> None:
    assert run_with_answer(proven_none()).status == ColumnGenerationStatus.Optimal
    infeasible = PricingResult(status=PricingStatus.Infeasible, exact_stage=True)
    assert run_with_answer(infeasible).status == ColumnGenerationStatus.Optimal


def test_a_pricer_with_nothing_to_price_proves_nothing_is_missing() -> None:
    result = run_with_answer(PricingResult(status=PricingStatus.NotApplicable))
    assert result.status == ColumnGenerationStatus.Optimal


@pytest.mark.parametrize(
    "answer",
    [
        PricingResult(
            status=PricingStatus.Infeasible, min_reduced_cost_bound=0.0, exact_stage=True
        ),
        PricingResult(status=PricingStatus.NotApplicable),
    ],
)
def test_a_second_pricer_without_a_path_does_not_block_the_proof(answer: PricingResult) -> None:
    toy = CoverOneRow()
    toy.master.add_pricer(ScriptedPricer(lambda duals, request: answer))
    result = ColumnGeneration(toy.master).run()
    assert result.status == ColumnGenerationStatus.Optimal and result.objective == PATH_COST


def test_no_path_with_an_artificial_column_in_use_is_infeasible() -> None:
    solver = ScriptedMasterSolver()

    def solve(lp: ScriptedMasterSolver) -> SolverStatus:
        lp.values = {0: 1.0}
        return SolverStatus.Optimal

    solver.on_solve = solve
    master = MasterProblem(solver)
    master.add_rows(1, RowSense.GreaterEqual, 1.0)
    infeasible = PricingResult(status=PricingStatus.Infeasible, exact_stage=True)
    master.add_pricer(ScriptedPricer(lambda duals, request: infeasible))
    assert ColumnGeneration(master).run().status == ColumnGenerationStatus.Infeasible


@pytest.mark.parametrize(
    "answer",
    [
        PricingResult(status=PricingStatus.SolverLimit, exact_stage=True),
        PricingResult(status=PricingStatus.Unknown),
        PricingResult(status=PricingStatus.Optimal, exact_stage=False),
        PricingResult(),
        PricingResult(status=PricingStatus.Infeasible, min_reduced_cost_bound=0.0),
    ],
)
def test_anything_else_proves_nothing(answer: PricingResult) -> None:
    result = run_with_answer(answer)
    assert result.status == ColumnGenerationStatus.NotProven
    assert result.lower_bound == -math.inf


def test_every_pricer_is_priced_and_all_must_prove() -> None:
    solver = ScriptedMasterSolver()
    master = MasterProblem(solver)
    master.add_rows(2, RowSense.GreaterEqual, 1.0)
    proving, unproven = ScriptedPricer(), ScriptedPricer(lambda duals, request: PricingResult())
    master.add_pricer(proving, ConvexityRow(max_paths=3.0))
    master.add_pricer(unproven)
    solver.dual_values = [0.0, 0.0, 5.0]
    result = ColumnGeneration(master, ColumnGenerationParams(initialization="none")).run()

    assert result.status == ColumnGenerationStatus.NotProven
    assert len(proving.requests) == 1 and len(unproven.requests) == 1
    # Each pricer's threshold has its own convexity dual: the second has none.
    assert proving.requests[0].reduced_cost_threshold == pytest.approx(5.0 - 1e-9)
    assert unproven.requests[0].reduced_cost_threshold == pytest.approx(-1e-9)


def test_the_columns_found_go_to_the_pricer_that_found_them() -> None:
    solver = ScriptedMasterSolver()
    master = MasterProblem(solver)
    master.add_rows(2, RowSense.GreaterEqual, 1.0)
    first_path, second_path = make_path(3.0, [(0, 1.0)], -1.0), make_path(4.0, [(1, 1.0)], -1.0)
    once = {"first": True, "second": True}

    def respond(name: str, path: Any) -> Any:
        def price(duals: FloatArray, request: PricingRequest) -> PricingResult:
            result = proven_none()
            if once[name]:
                once[name] = False
                result.columns = [path]
            return result

        return price

    master.add_pricer(ScriptedPricer(respond("first", first_path)), ConvexityRow(max_paths=1.0))
    master.add_pricer(ScriptedPricer(respond("second", second_path)))
    result = ColumnGeneration(master, ColumnGenerationParams(initialization="none")).run()

    assert result.iterations == 2 and result.statistics.generated_columns == 2
    # Only the first pricer has a convexity row, which the master fills in its columns.
    assert [(column.cost, column.entries) for column in solver.columns] == [
        (3.0, [(0, 1.0), (2, 1.0)]),
        (4.0, [(1, 1.0)]),
    ]
    assert [(column.pricer, column.id) for column in master.columns] == [(0, 0), (1, 1)]


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
    assert toy.solver.restore_calls == 1


def test_ctrl_c_in_pricing_stops_the_run_and_restores_the_model() -> None:
    toy = CoverOneRow()

    def interrupted(duals: FloatArray, request: PricingRequest) -> PricingResult:
        raise KeyboardInterrupt

    toy.pricer.respond = interrupted
    with pytest.raises(KeyboardInterrupt):
        ColumnGeneration(toy.master).run()
    assert toy.solver.restore_calls == 1


def test_ctrl_c_in_an_lp_solve_stops_the_run_and_restores_the_model() -> None:
    toy = CoverOneRow()

    def interrupted(lp: ScriptedMasterSolver) -> SolverStatus:
        raise KeyboardInterrupt

    toy.solver.on_solve = interrupted
    with pytest.raises(KeyboardInterrupt):
        ColumnGeneration(toy.master).run()
    assert toy.solver.restore_calls == 1


def test_answers_of_the_wrong_size_are_refused() -> None:
    solver = MisbehavingMasterSolver()
    toy = CoverOneRow(solver)
    solver.duals_answer = np.zeros(0)
    with pytest.raises(ValueError, match=r"MasterSolver.duals: expected 1 values, got 0"):
        ColumnGeneration(toy.master).run()
    assert solver.restore_calls == 1

    solver.duals_answer = np.zeros((1, 1))
    with pytest.raises(ValueError, match=r"MasterSolver.duals: expected 1 values, got 1"):
        ColumnGeneration(toy.master).run()


def test_duals_that_are_not_numbers_are_refused() -> None:
    solver = MisbehavingMasterSolver()
    toy = CoverOneRow(solver)
    solver.duals_answer = ["not a number"]
    with pytest.raises(ValueError, match="could not convert"):
        ColumnGeneration(toy.master).run()


def test_duals_as_a_list_reach_the_pricers_as_an_array() -> None:
    solver = MisbehavingMasterSolver()
    toy = CoverOneRow(solver)
    solver.duals_answer = [PATH_COST]
    ColumnGeneration(toy.master).run()
    assert toy.pricer.seen_duals[0].dtype == np.float64


def test_wrong_types_are_refused() -> None:
    with pytest.raises(TypeError, match="expected a MasterProblem"):
        ColumnGeneration(ScriptedMasterSolver())  # type: ignore[arg-type]
    with pytest.raises(TypeError, match="expected ColumnGenerationParams"):
        ColumnGeneration(CoverOneRow().master, None)  # type: ignore[arg-type]


def test_the_loop_keeps_its_master_solver_and_pricers_alive() -> None:
    calls: list[str] = []

    def solve(lp: ScriptedMasterSolver) -> SolverStatus:
        calls.append("solve")
        return SolverStatus.Optimal

    def price(duals: FloatArray, request: PricingRequest) -> PricingResult:
        calls.append("price")
        return proven_none()

    def build() -> MasterProblem:
        solver = ScriptedMasterSolver()
        solver.on_solve = solve
        master = MasterProblem(solver)
        master.add_rows(1, RowSense.GreaterEqual, 1.0)
        master.add_pricer(ScriptedPricer(price))
        return master

    loop = ColumnGeneration(build())
    gc.collect()
    result = loop.run()
    assert result.status == ColumnGenerationStatus.Optimal
    assert calls == ["solve", "price"]


# ── Params and results ───────────────────────────────────────────────────────


def test_params_read_back_and_change_by_copy() -> None:
    params = ColumnGenerationParams(max_iterations=5, timeout_s=60.0)
    assert (params.max_iterations, params.timeout_s) == (5, 60.0)
    seen: list[IterationInfo] = []
    changed = replace(
        params,
        max_iterations=7,
        relaxation=RelaxationParams(timeout_s=1.0),
        statistics=StatisticsParams(timers=False),
        tolerances=RelativeTolerances(),
        initialization=NoInitialization(),
        on_iteration=seen.append,
    )
    assert changed.max_iterations == 7 and changed.timeout_s == 60.0
    assert changed.relaxation.timeout_s == 1.0 and not changed.statistics.timers
    assert isinstance(changed.tolerances, RelativeTolerances)
    assert isinstance(changed.initialization, NoInitialization)
    assert callable(changed.on_iteration)
    assert params.max_iterations == 5 and params.on_iteration is None


def test_the_defaults_of_the_params() -> None:
    params = ColumnGenerationParams()
    assert params.max_iterations == math.inf and params.timeout_s == math.inf
    assert params.relaxation.timeout_s == math.inf and params.statistics.timers
    assert isinstance(params.tolerances, AbsoluteTolerances)
    assert isinstance(params.initialization, ArtificialColumns)
    assert params.on_iteration is None and params.on_converged is None
    # Each set of params has its own components.
    assert ColumnGenerationParams().tolerances is not params.tolerances
    with pytest.raises(TypeError):
        ColumnGenerationParams(5)  # type: ignore[call-arg]  # pyright: ignore[reportCallIssue]


def test_the_statistics_and_the_result_are_frozen_records() -> None:
    result = ColumnGenerationResult(
        status=ColumnGenerationStatus.Optimal,
        objective=3.0,
        lower_bound=3.0,
        iterations=2,
        statistics=Statistics(
            generated_columns=1,
            artificial_columns=0,
            artificial_cost=None,
            total_seconds=0.5,
            relaxation_seconds=0.25,
            pricing_seconds=0.125,
        ),
    )
    assert result.status == ColumnGenerationStatus.Optimal and result.iterations == 2
    assert result.statistics.generated_columns == 1 and result.statistics.total_seconds == 0.5
    with pytest.raises(TypeError):
        ColumnGenerationResult()  # type: ignore[call-arg]  # pyright: ignore[reportCallIssue]
    with pytest.raises(AttributeError):
        result.iterations = 1  # type: ignore[misc]
    with pytest.raises(AttributeError):
        result.statistics.generated_columns = 1  # type: ignore[misc]


def test_the_statuses_and_actions() -> None:
    assert list(ColumnGenerationStatus.__members__) == [
        "Optimal",
        "NotProven",
        "Infeasible",
        "RestrictedMasterInfeasible",
        "Unbounded",
        "TimeLimit",
        "IterationLimit",
        "SolverError",
    ]
    assert list(ConvergenceAction.__members__) == ["Stop", "Continue"]


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
    loop.run()

    tolerances = loop.params.tolerances
    assert isinstance(tolerances, HalfTolerances)
    assert tolerances.contexts[0].cost_scale == 1.0
    assert toy.pricer.requests[0].reduced_cost_threshold == -0.5


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


# ── The graph pricer in the loop ─────────────────────────────────────────────

# Path A, 0 → 1 → 3, costs 3 and covers row 0; path B, 0 → 2 → 3, costs 5 and covers row 1.
COST_A = 3.0
COST_B = 5.0


def two_path_graph() -> ResourceGraph:
    """Two paths from the source 0 to the sink 3, each covering one master row."""
    graph = ResourceGraph()
    graph.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    graph.add_node(0, source=True)
    graph.add_node(1)
    graph.add_node(2)
    graph.add_node(3, sink=True)
    graph.add_arc(COST_A, 0, 1, cost=COST_A, rows=[(0, 1.0)])
    graph.add_arc(0.0, 1, 3, cost=0.0)
    graph.add_arc(COST_B, 0, 2, cost=COST_B, rows=[(1, 1.0)])
    graph.add_arc(0.0, 2, 3, cost=0.0)
    return graph


def test_column_generation_from_scratch_with_a_graph_pricer() -> None:
    # Rows "cover row 0 ≥ 1" and "cover row 1 ≥ 1"; the master starts from two artificial
    # columns, ids 0 and 1, one per row.
    solver = ScriptedMasterSolver()
    master = MasterProblem(solver)
    master.add_rows(2, RowSense.GreaterEqual, 1.0)

    def solve(lp: ScriptedMasterSolver) -> SolverStatus:
        # The LP's answer: each row is covered by its path once found, else by its artificial
        # column; its dual is the cost of what covers it.
        covering = {row: column.cost for column in lp.columns[2:] for row, _ in column.entries}
        lp.dual_values = [covering.get(row, lp.columns[row].cost) for row in range(2)]
        lp.objective_value = sum(lp.dual_values)
        lp.values = {row: 0.0 if row in covering else 1.0 for row in range(2)}
        lp.values.update({column.id: 1.0 for column in lp.columns[2:]})
        return SolverStatus.Optimal

    solver.on_solve = solve
    master.add_pricer(GraphPricer(two_path_graph()))
    result = ColumnGeneration(master).run()

    # A first (it dominates B while both duals are artificial), then B, then the proof.
    assert result.status == ColumnGenerationStatus.Optimal
    assert result.iterations == 3
    assert result.objective == COST_A + COST_B
    assert result.statistics.generated_columns == 2
    assert [column.cost for column in solver.columns[2:]] == [COST_A, COST_B]


def test_components_that_are_neither_objects_nor_names_are_refused() -> None:
    master = CoverOneRow().master
    nothing: Any = None
    with pytest.raises(TypeError, match="^ColumnGeneration: tolerances must be Tolerances or"):
        ColumnGeneration(master, ColumnGenerationParams(tolerances=nothing))
    with pytest.raises(TypeError, match="^ColumnGeneration: initialization must be a Master"):
        ColumnGeneration(master, ColumnGenerationParams(initialization=nothing))
