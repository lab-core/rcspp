#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The debug checks from Python: the options, and DebugCheckError raised by the C++ loop
with the check's name and the iteration, over a master solver and a pricer written in Python.

Each check's cases are tested in C++ (the example's
tests/cpp/test_column_generation_debug_checks.hpp); here, one fault per check.
"""

import os
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "python"))

import rcspp._core as _core  # noqa: E402

# Built without the example (USE_COLUMN_GENERATION_EXAMPLE off): these tests skip.
if not hasattr(_core, "cpp_column_generation"):
    pytest.skip("rcspp built without the column generation example", allow_module_level=True)

from cpp_column_generation import (  # noqa: E402
    ColumnGeneration,
    ColumnGenerationParams,
    ColumnGenerationStatus,
    ConvexityRow,
    DebugCheckError,
    DebugChecks,
    MasterProblem,
    NoInitialization,
    PricingRequest,
    PricingResult,
    PricingStatus,
    RowSense,
    SolverStatus,
)
from cpp_column_generation_fakes import (  # noqa: E402
    FloatArray,
    ScriptedMasterSolver,
    ScriptedPricer,
    make_path,
    proven_none,
)

PATH_COST = 10.0
ARTIFICIAL_COST = 100.0

EVERY_CHECK = DebugChecks(
    pricer_reduced_costs=True, dual_signs=True, master_reduced_costs=True, objective=True
)


class Toy:
    """Row 0 "cover >= 1" with an artificial column (id 0, cost 100), and a pricer whose
    convexity row 1 allows one path of cost 10. Iteration 1: value 100, duals (100, 0);
    iteration 2, the path: value 10, duals (12, -2). As in the C++ tests."""

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
        self.offset = 0.0

    def solve(self, solver: ScriptedMasterSolver) -> SolverStatus:
        lp = self.lps[min(len(solver.relaxation_calls), len(self.lps)) - 1]
        solver.objective_value, solver.dual_values, solver.values = lp
        return SolverStatus.Optimal

    def respond(self, duals: FloatArray, request: PricingRequest) -> PricingResult:
        reduced_cost = PATH_COST - float(duals[0]) + self.offset

        if reduced_cost >= request.reduced_cost_threshold:
            return proven_none()

        return PricingResult(
            columns=[make_path(PATH_COST, [(0, 1.0)], reduced_cost)],
            status=PricingStatus.Optimal,
            min_reduced_cost_bound=reduced_cost,
            exact_stage=True,
        )

    def run(self, checks: DebugChecks) -> ColumnGenerationStatus:
        params = ColumnGenerationParams(
            initialization=NoInitialization(), max_iterations=5, debug_checks=checks
        )
        return ColumnGeneration(self.master, params).run().status


def test_the_checks_are_off_by_default_and_set_by_keyword() -> None:
    checks = ColumnGenerationParams().debug_checks
    assert not (
        checks.pricer_reduced_costs
        or checks.dual_signs
        or checks.master_reduced_costs
        or checks.objective
    )
    assert ColumnGenerationParams(debug_checks=DebugChecks(objective=True)).debug_checks.objective

    with pytest.raises(TypeError):
        DebugChecks(True)  # type: ignore[misc]


def test_a_correct_run_passes_every_check() -> None:
    assert Toy().run(EVERY_CHECK) == ColumnGenerationStatus.Optimal


def test_the_error_names_the_check_and_the_iteration() -> None:
    toy = Toy()
    toy.offset = 1.0

    with pytest.raises(DebugCheckError) as raised:
        toy.run(EVERY_CHECK)

    error = raised.value
    assert isinstance(error, RuntimeError)
    assert (error.check, error.iteration) == ("pricer_reduced_costs", 1)
    assert error.message == (
        "pricer 0's column 0 reports the reduced cost -89, its cost and rows give -90; "
        "tolerance 1e-09"
    )
    assert str(error) == f"debug check pricer_reduced_costs failed at iteration 1: {error.message}"
    assert toy.solver.restore_calls == 1


@pytest.mark.parametrize(
    "lp, check, iteration",
    [
        ((0, (ARTIFICIAL_COST, [-1.0, 0.0], {0: 1.0})), "dual_signs", 1),
        ((1, (PATH_COST, [ARTIFICIAL_COST, 0.0], {1: 1.0})), "master_reduced_costs", 2),
        ((1, (PATH_COST + 1.0, [12.0, -2.0], {1: 1.0})), "objective", 2),
    ],
)
def test_each_check_of_the_lp_fires_on_its_fault(
    lp: tuple[int, tuple[float, list[float], dict[int, float]]], check: str, iteration: int
) -> None:
    position, answer = lp

    toy = Toy()
    toy.lps[position] = answer
    toy.run(DebugChecks())  # off: unseen

    toy = Toy()
    toy.lps[position] = answer

    with pytest.raises(DebugCheckError) as raised:
        toy.run(EVERY_CHECK)

    assert (raised.value.check, raised.value.iteration) == (check, iteration)
