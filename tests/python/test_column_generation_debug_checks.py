#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The debug checks of the pure-Python column generation loop: each fires on a master
solver or a pricer that contradicts the others, stays silent on noise below the tolerance,
skips what it cannot check, and costs nothing when off."""

import dataclasses
import math
import os
import sys
from collections.abc import Callable

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from column_generation_fakes import (  # noqa: E402
    ScriptedMasterSolver,
    ScriptedPricer,
    make_path,
    proven_none,
)

from rcspp.column_generation import (  # noqa: E402
    ColumnGeneration,
    ColumnGenerationParams,
    ColumnGenerationStatus,
    ConvexityRow,
    DebugCheckError,
    DebugChecks,
    IterationInfo,
    MasterProblem,
    NoInitialization,
    PricingRequest,
    PricingResult,
    PricingStatus,
    RowSense,
    SolverStatus,
)
from rcspp.column_generation.types import FloatArray  # noqa: E402

EVERY_CHECK = DebugChecks(
    pricer_reduced_costs=True, dual_signs=True, master_reduced_costs=True, objective=True
)

PATH_COST = 10.0
ARTIFICIAL_COST = 100.0


class Toy:
    """Row 0 "cover >= 1" with an artificial column (id 0, cost 100), and a pricer whose
    convexity row 1 allows one path; the pricer offers a path of cost 10 covering row 0
    while its reduced cost is negative.

    The LP of iteration 1 uses the artificial column: value 100, duals (100, 0). That of
    iteration 2 uses the path (id 1): value 10, duals (12, -2), so that the path's reduced
    cost, 10 - 12 - (-2), is 0 with its convexity entry.

    Each LP's answers are in ``lps``, which a test may change before the run.
    """

    def __init__(self) -> None:
        self.solver = ScriptedMasterSolver()
        self.master = MasterProblem(self.solver)
        self.master.add_rows(1, RowSense.GreaterEqual, 1.0)
        self.pricer = ScriptedPricer(self.respond)
        self.master.add_pricer(self.pricer, ConvexityRow(max_paths=1.0))
        self.master.add_artificial_columns([0], [1.0], ARTIFICIAL_COST)
        self.solver.on_solve = self.solve
        self.lps: list[tuple[float, list[float], dict[int, float]]] = [
            (ARTIFICIAL_COST, [ARTIFICIAL_COST, 0.0], {0: 1.0}),
            (PATH_COST, [12.0, -2.0], {1: 1.0}),
        ]
        # What the pricer reports as its path's reduced cost: by default, the true one.
        self.reported: Callable[[FloatArray], float] = lambda duals: PATH_COST - duals[0]

    def solve(self, solver: ScriptedMasterSolver) -> SolverStatus:
        lp = self.lps[min(len(solver.relaxation_calls), len(self.lps)) - 1]
        solver.objective_value, solver.dual_values, solver.values = lp
        return SolverStatus.Optimal

    def respond(self, duals: FloatArray, request: PricingRequest) -> PricingResult:
        reduced_cost = self.reported(duals)

        if reduced_cost >= request.reduced_cost_threshold:
            return proven_none()

        return PricingResult(
            columns=[make_path(PATH_COST, [(0, 1.0)], reduced_cost)],
            status=PricingStatus.Optimal,
            min_reduced_cost_bound=reduced_cost,
            exact_stage=True,
        )

    def run(self, checks: DebugChecks = EVERY_CHECK, **options: object) -> ColumnGenerationStatus:
        # A broken LP may keep offering the same path: a few iterations are enough.
        params = ColumnGenerationParams(
            initialization=NoInitialization(),
            max_iterations=5,
            debug_checks=checks,
            **options,  # type: ignore[arg-type]
        )
        return ColumnGeneration(self.master, params).run().status


def failure(toy: Toy, checks: DebugChecks = EVERY_CHECK) -> DebugCheckError:
    """The error a run of the toy raises."""
    with pytest.raises(DebugCheckError) as raised:
        toy.run(checks)

    return raised.value


# MARK: A correct run


def test_a_correct_run_passes_every_check() -> None:
    toy = Toy()
    assert toy.run() == ColumnGenerationStatus.Optimal
    assert len(toy.solver.relaxation_calls) == 2


def test_the_checks_are_off_by_default_and_frozen() -> None:
    checks = ColumnGenerationParams().debug_checks
    assert checks == DebugChecks()
    assert not any(dataclasses.asdict(checks).values())

    with pytest.raises(dataclasses.FrozenInstanceError):
        checks.dual_signs = True  # type: ignore[misc]

    with pytest.raises(TypeError):
        DebugChecks(True)  # type: ignore[misc]


# MARK: Each check


def test_pricer_reduced_costs_fires_on_a_misreported_reduced_cost() -> None:
    toy = Toy()
    toy.reported = lambda duals: PATH_COST - duals[0] + 1.0
    error = failure(toy)
    assert (error.check, error.iteration) == ("pricer_reduced_costs", 1)
    assert "pricer 0's column 0 reports the reduced cost -89.0" in error.message
    assert "its cost and rows give -90.0" in error.message
    assert str(error).startswith("debug check pricer_reduced_costs failed at iteration 1: ")


def test_pricer_reduced_costs_allows_noise_below_the_tolerance() -> None:
    # The scale: 1 + |10| + |100 * 1|; the tolerance 1e-9 by default.
    noise = 0.5 * 1e-9 * (PATH_COST + ARTIFICIAL_COST)
    toy = Toy()
    toy.reported = lambda duals: PATH_COST - duals[0] + noise
    assert toy.run() == ColumnGenerationStatus.Optimal

    toy = Toy()
    toy.reported = lambda duals: PATH_COST - duals[0] + 3 * noise
    assert failure(toy).check == "pricer_reduced_costs"


def test_pricer_reduced_costs_skips_starting_columns() -> None:
    # A starting column's reduced cost was never priced: whatever it holds is not checked.
    toy = Toy()
    toy.master.add_columns(0, [make_path(PATH_COST + 5.0, [(0, 1.0)], 12345.0)])
    assert toy.run(DebugChecks(pricer_reduced_costs=True)) == ColumnGenerationStatus.Optimal


def test_dual_signs_fire_on_a_wrong_sign_of_a_greater_or_less_row() -> None:
    toy = Toy()
    toy.lps[0] = (ARTIFICIAL_COST, [-1.0, 0.0], {0: 1.0})
    error = failure(toy, DebugChecks(dual_signs=True))
    assert (error.check, error.iteration) == ("dual_signs", 1)
    assert "row 0 (>= 1.0) has the dual -1.0" in error.message

    toy = Toy()
    toy.lps[0] = (ARTIFICIAL_COST, [ARTIFICIAL_COST, 1.0], {0: 1.0})
    error = failure(toy, DebugChecks(dual_signs=True))
    assert "row 1 (<= 1.0) has the dual 1.0" in error.message


def test_dual_signs_count_the_other_rows_that_fail() -> None:
    toy = Toy()
    toy.lps[0] = (ARTIFICIAL_COST, [-1.0, 1.0], {0: 1.0})
    error = failure(toy, DebugChecks(dual_signs=True))
    assert error.message.startswith("row 0 (>= 1.0) has the dual -1.0 (1 more); tolerance ")


def test_dual_signs_skip_equal_and_ranged_rows_and_check_rows_with_no_bound() -> None:
    # Rows 2 (= 3), 3 (0 ... 2) and 4 (no bound), none used by a column.
    def toy_with_rows(dual_of_free_row: float) -> Toy:
        toy = Toy()
        toy.master.add_rows(1, RowSense.Equal, 3.0)
        toy.master.add_rows_between([0.0, -math.inf], [2.0, math.inf])
        toy.lps = [
            (ARTIFICIAL_COST, [ARTIFICIAL_COST, 0.0, -7.0, -5.0, dual_of_free_row], {0: 1.0}),
            (PATH_COST, [12.0, -2.0, 7.0, 5.0, dual_of_free_row], {1: 1.0}),
        ]
        return toy

    assert toy_with_rows(0.0).run(DebugChecks(dual_signs=True)) == ColumnGenerationStatus.Optimal
    error = failure(toy_with_rows(0.5), DebugChecks(dual_signs=True))
    assert error.message.startswith("row 4 (no bound) has the dual 0.5;")


def test_dual_signs_allow_the_master_solvers_own_tolerance() -> None:
    toy = Toy()
    toy.solver.dual_tolerance_value = 1e-3
    toy.lps[0] = (ARTIFICIAL_COST, [ARTIFICIAL_COST, 5e-4], {0: 1.0})
    assert toy.run(DebugChecks(dual_signs=True)) == ColumnGenerationStatus.Optimal


def test_master_reduced_costs_fire_on_the_duals_of_another_solve() -> None:
    # Iteration 2 answers iteration 1's duals: the path's reduced cost is 10 - 100 = -90.
    toy = Toy()
    toy.lps[1] = (PATH_COST, [ARTIFICIAL_COST, 0.0], {1: 1.0})
    error = failure(toy, DebugChecks(master_reduced_costs=True))
    assert (error.check, error.iteration) == ("master_reduced_costs", 2)
    assert "column 1 has the reduced cost -90.0" in error.message


def test_master_reduced_costs_fire_on_a_used_column_whose_reduced_cost_is_not_zero() -> None:
    # Iteration 2 says the artificial column is used, while its reduced cost is 100 - 12.
    toy = Toy()
    toy.lps[1] = (PATH_COST, [12.0, -2.0], {0: 1.0, 1: 1.0})
    error = failure(toy, DebugChecks(master_reduced_costs=True))
    assert (error.check, error.iteration) == ("master_reduced_costs", 2)
    assert "column 0, of value 1.0, has the reduced cost 88.0" in error.message


def test_master_reduced_costs_read_the_convexity_entry() -> None:
    # Without the convexity dual -2, the path's reduced cost would be -2: it is 0.
    toy = Toy()
    assert toy.run(DebugChecks(master_reduced_costs=True)) == ColumnGenerationStatus.Optimal


def test_objective_fires_on_a_value_its_columns_do_not_cost() -> None:
    toy = Toy()
    toy.lps[1] = (PATH_COST + 1.0, [12.0, -2.0], {1: 1.0})
    error = failure(toy, DebugChecks(objective=True))
    assert (error.check, error.iteration) == ("objective", 2)
    assert "the LP value is 11.0, its columns' values cost 10.0" in error.message


def test_objective_is_not_checked_when_the_users_variables_cost_something() -> None:
    toy = Toy()
    toy.solver.largest_user_cost_value = 1.0
    toy.lps[1] = (PATH_COST + 1.0, [12.0, -2.0], {1: 1.0})
    assert toy.run(DebugChecks(objective=True)) == ColumnGenerationStatus.Optimal


# MARK: Around a failure


def misreport(toy: Toy) -> None:
    toy.reported = lambda duals: PATH_COST - duals[0] + 1.0


def flip_signs(toy: Toy) -> None:
    toy.lps[0] = (ARTIFICIAL_COST, [-1.0, 1.0], {0: 1.0})


def keep_old_duals(toy: Toy) -> None:
    toy.lps[1] = (PATH_COST, [ARTIFICIAL_COST, 0.0], {0: 1.0, 1: 1.0})


def add_to_objective(toy: Toy) -> None:
    toy.lps[1] = (PATH_COST + 1.0, [12.0, -2.0], {1: 1.0})


@pytest.mark.parametrize(
    "break_toy",
    [misreport, flip_signs, keep_old_duals, add_to_objective],
    ids=["pricer_reduced_costs", "dual_signs", "master_reduced_costs", "objective"],
)
def test_each_fault_goes_unseen_when_its_check_is_off(break_toy: Callable[[Toy], None]) -> None:
    toy = Toy()
    break_toy(toy)
    toy.run(DebugChecks())

    toy = Toy()
    break_toy(toy)
    assert isinstance(failure(toy), RuntimeError)


def test_a_failure_stops_before_the_hook_and_restores_the_users_model() -> None:
    toy = Toy()
    toy.lps[1] = (PATH_COST + 1.0, [12.0, -2.0], {1: 1.0})
    seen: list[IterationInfo] = []

    with pytest.raises(DebugCheckError):
        toy.run(DebugChecks(objective=True), on_iteration=seen.append)

    assert [info.iteration for info in seen] == [1]
    assert toy.solver.restore_calls == 1


def test_a_second_run_checks_the_columns_added_between_runs() -> None:
    toy = Toy()
    loop = ColumnGeneration(
        toy.master,
        ColumnGenerationParams(
            initialization=NoInitialization(), max_iterations=5, debug_checks=EVERY_CHECK
        ),
    )
    assert loop.run().status == ColumnGenerationStatus.Optimal

    # A column costing 1 under the duals (12, -2): its reduced cost is 1 - 12 + 2 < 0.
    toy.master.add_columns(0, [make_path(1.0, [(0, 1.0)], 0.0)])

    with pytest.raises(DebugCheckError, match="column 2 has the reduced cost -9.0"):
        loop.run()
