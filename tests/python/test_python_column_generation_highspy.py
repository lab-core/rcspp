#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The pure-Python master solver over the user's highspy model (highspy installed)."""

import math
import os
import signal
import sys
import time
import warnings
from collections.abc import Callable
from types import SimpleNamespace
from typing import Any

import numpy as np
import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "python", "src")
)

from python_column_generation_fakes import ScriptedPricer, make_path  # noqa: E402

import rcspp.python_column_generation as layer  # noqa: E402
from rcspp.python_column_generation import (  # noqa: E402
    ColumnBatch,
    ColumnType,
    HighspyMasterSolver,
    IntegerParams,
    RelaxationParams,
    RowBatch,
    SolverStatus,
)

highspy = pytest.importorskip("highspy")

INFINITY = math.inf


def new_model() -> Any:
    """An empty, quiet highspy model."""
    model = highspy.Highs()
    model.setOptionValue("output_flag", False)
    return model


def self_test_lp_by_rows() -> Any:
    """The LP of the master solver self-test, built row by row (a row-wise matrix).

    min x1 + 3 x2 + x3 subject to x1 + x2 = 2, x2 + x3 >= 1, x1 + x3 <= 1.5, x >= 0; its optimum is
    3.75 with the duals (1.5, 1.5, -0.5).
    """
    model = new_model()

    for cost in (1.0, 3.0, 1.0):
        model.addVar(0.0, highspy.kHighsInf)
        model.changeColCost(model.getNumCol() - 1, cost)

    for lower, upper, columns in (
        (2.0, 2.0, [0, 1]),
        (1.0, highspy.kHighsInf, [1, 2]),
        (-highspy.kHighsInf, 1.5, [0, 2]),
    ):
        model.addRow(lower, upper, 2, np.array(columns, dtype=np.int32), np.ones(2))

    return model


def self_test_lp_by_columns() -> Any:
    """The same LP, built column by column (a column-wise matrix)."""
    model = new_model()
    model.addRows(
        3,
        np.array([2.0, 1.0, -highspy.kHighsInf]),
        np.array([2.0, highspy.kHighsInf, 1.5]),
        0,
        [],
        [],
        [],
    )

    for cost, rows in ((1.0, [0, 2]), (3.0, [0, 1]), (1.0, [1, 2])):
        model.addCol(cost, 0.0, highspy.kHighsInf, 2, np.array(rows, dtype=np.int32), np.ones(2))

    return model


def entries(model: Any) -> Any:
    """The matrix of a highspy model, dense: one row per row, one column per column."""
    lp = model.getLp()
    matrix = lp.a_matrix_
    dense = np.zeros((lp.num_row_, lp.num_col_))
    by_column = matrix.format_ == highspy.MatrixFormat.kColwise
    starts, indices, values = list(matrix.start_), list(matrix.index_), list(matrix.value_)

    for major in range(lp.num_col_ if by_column else lp.num_row_):
        for entry in range(starts[major], starts[major + 1]):
            row, column = (indices[entry], major) if by_column else (major, indices[entry])
            dense[row, column] = values[entry]

    return dense


def column(
    column_id: int,
    cost: float,
    entries: list[tuple[int, float]],
    lower: float = 0.0,
    upper: float = INFINITY,
) -> ColumnBatch:
    """A batch of one column."""
    return ColumnBatch(
        ids=[column_id],
        cost=[cost],
        lower=[lower],
        upper=[upper],
        starts=[0, len(entries)],
        row_indices=[row for row, _ in entries],
        values=[value for _, value in entries],
    )


def self_test_script(solver: HighspyMasterSolver) -> None:
    """What the master solver self-test checks, on a master solver over
    an empty model: rows, columns under ids that are not their positions, values read in
    another order, a column added after a solve, the integer solve with integer, binary, then
    integer columns."""
    solver.add_rows(RowBatch([2.0, 1.0, -INFINITY], [2.0, INFINITY, 1.5]))
    lower, upper = solver.row_bounds()
    assert lower.tolist() == [2.0, 1.0, -INFINITY] and upper.tolist() == [2.0, INFINITY, 1.5]

    solver.add_columns(
        ColumnBatch(
            ids=[7, 3, 5],
            cost=[1.0, 3.0, 1.0],
            lower=[0.0, 0.0, 0.0],
            upper=[INFINITY] * 3,
            starts=[0, 2, 4, 6],
            row_indices=[0, 2, 0, 1, 1, 2],
            values=[1.0] * 6,
        )
    )
    assert solver.solve_relaxation(RelaxationParams()) == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(3.75)
    assert solver.duals() == pytest.approx([1.5, 1.5, -0.5])
    ids = np.array([5, 7, 3], dtype=np.uint64)
    assert solver.primal(ids) == pytest.approx([0.25, 1.25, 0.75])

    solver.add_columns(column(9, 0.5, [(0, 1.0), (1, 1.0)]))
    assert solver.solve_relaxation(RelaxationParams()) == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(1.0)
    assert solver.duals() == pytest.approx([0.5, 0.0, 0.0])
    assert solver.primal(np.array([9, 7], dtype=np.uint64)) == pytest.approx([2.0, 0.0])

    # The relaxation gives 0.55; the integer optimum stays 1.0.
    solver.add_columns(column(11, 0.2, [(0, 4.0)]))
    assert solver.solve_integer(IntegerParams()) == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(1.0)
    assert solver.primal(np.array([11, 9, 7], dtype=np.uint64)) == pytest.approx([0.0, 2.0, 0.0])

    # Binary columns: x4 can no longer be 2. Then integer ones again, without that bound.
    assert solver.solve_integer(IntegerParams(column_type=ColumnType.Binary)) == (
        SolverStatus.Optimal
    )
    assert solver.objective() == pytest.approx(1.5)
    assert solver.primal(np.array([11, 9, 7], dtype=np.uint64)) == pytest.approx([0.0, 1.0, 1.0])
    assert solver.solve_integer(IntegerParams()) == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(1.0)


# MARK: The model


def test_passes_the_script_of_the_master_solver_self_test() -> None:
    self_test_script(HighspyMasterSolver(new_model()))

    maximizing = new_model()
    maximizing.changeObjectiveSense(highspy.ObjSense.kMaximize)
    self_test_script(HighspyMasterSolver(maximizing))


@pytest.mark.parametrize("build", [self_test_lp_by_rows, self_test_lp_by_columns])
def test_the_users_model(build: Callable[[], Any]) -> None:
    model = build()
    solver = HighspyMasterSolver(model)
    assert solver.model is model
    assert solver.number_of_rows() == 3
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(3.75)
    assert solver.duals() == pytest.approx([1.5, 1.5, -0.5])


def test_a_maximizing_model_with_an_offset() -> None:
    model = self_test_lp_by_rows()
    model.changeColsCost(3, np.arange(3, dtype=np.int32), np.array([-1.0, -3.0, -1.0]))
    model.changeObjectiveSense(highspy.ObjSense.kMaximize)
    model.changeObjectiveOffset(1.0)
    solver = HighspyMasterSolver(model)
    assert solver.solve_relaxation() == SolverStatus.Optimal

    # max -3.75 + 1 is min 3.75 - 1; the duals are those of the minimization.
    assert solver.objective() == pytest.approx(2.75)
    assert solver.duals() == pytest.approx([1.5, 1.5, -0.5])

    # A generated column is given as its minimization cost.
    solver.add_columns(column(0, 0.5, [(2, 1.0)]))
    assert model.getLp().col_cost_[3] == -0.5


def test_the_rows_that_columns_enter() -> None:
    model = self_test_lp_by_rows()
    solver = HighspyMasterSolver(model, rows=[2, 0])
    assert solver.number_of_rows() == 2
    lower, upper = solver.row_bounds()
    assert lower.tolist() == [-INFINITY, 2.0]
    assert upper.tolist() == [1.5, 2.0]

    # A column entering master row 0 enters model row 2.
    solver.add_columns(column(7, 0.5, [(0, 1.0)]))
    assert solver.column(7) == 3
    assert model.getNumCol() == 4
    assert entries(model)[:, 3].tolist() == [0.0, 0.0, 1.0]

    with pytest.raises(KeyError, match="no column has the id 8"):
        solver.column(8)

    # The duals are those of the master rows only, in master order.
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert len(solver.duals()) == 2

    for rows in ([3], [-1]):
        with pytest.raises(IndexError, match="not a row of the model"):
            HighspyMasterSolver(self_test_lp_by_rows(), rows=rows)


def test_rows_added_to_the_model() -> None:
    model = self_test_lp_by_rows()
    solver = HighspyMasterSolver(model, rows=[2, 0])
    solver.add_rows(RowBatch([-INFINITY, 1.0], [4.0, 3.0], ["fleet", ""]))
    assert solver.number_of_rows() == 4
    assert model.getNumRow() == 5
    lower, upper = solver.row_bounds()
    assert lower.tolist() == [-INFINITY, 2.0, -INFINITY, 1.0]
    assert upper.tolist() == [1.5, 2.0, 4.0, 3.0]
    assert model.getLp().row_names_[3:] == ["fleet", ""]

    # A column enters the added rows, which are master rows 2 and 3.
    solver.add_columns(column(1, 1.0, [(2, 1.0), (3, 2.0)]))
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert len(solver.duals()) == 4


def test_rows_the_model_cannot_hold_are_refused() -> None:
    solver = HighspyMasterSolver(self_test_lp_by_rows())

    for lower, upper in ((math.nan, 1.0), (INFINITY, INFINITY), (-INFINITY, -INFINITY), (2.0, 1.0)):
        with pytest.raises(ValueError, match="row bounds must be numbers"):
            solver.add_rows(RowBatch([0.0, lower], [1.0, upper]))

    assert solver.number_of_rows() == 3 and solver.model.getNumRow() == 3


def test_rows_highs_refuses_raise() -> None:
    model = self_test_lp_by_rows()
    solver = HighspyMasterSolver(model)

    def add_rows(*args: Any) -> Any:
        return highspy.HighsStatus.kError

    model.addRows = add_rows

    with pytest.raises(ValueError, match="HiGHS refused the rows"):
        solver.add_rows(RowBatch([0.0], [1.0]))

    assert solver.number_of_rows() == 3


def test_infinite_row_bounds() -> None:
    # Bounds beyond infinite_bound are infinite for HiGHS, which only turns them into infinity
    # when they are added.
    model = new_model()
    model.addRows(2, np.array([-1e16, 0.0]), np.array([1e17, 1e5]), 0, [], [], [])
    solver = HighspyMasterSolver(model)
    assert solver.row_bounds()[0].tolist() == [-1e16, 0.0]
    model.setOptionValue("infinite_bound", 1e15)
    lower, upper = solver.row_bounds()
    assert lower.tolist() == [-INFINITY, 0.0] and upper.tolist() == [INFINITY, 1e5]


def test_later_changes_to_the_model_are_seen() -> None:
    model = self_test_lp_by_rows()
    solver = HighspyMasterSolver(model)
    model.changeColCost(0, 100.0)
    assert solver.solve_relaxation() == SolverStatus.Optimal

    # x1 = 0, x2 = 2, x3 = 0.
    assert solver.objective() == pytest.approx(6.0)


# MARK: The columns


def test_the_generated_columns_are_added_to_the_model() -> None:
    model = self_test_lp_by_rows()
    solver = HighspyMasterSolver(model)
    solver.add_columns(
        ColumnBatch(
            ids=[10, 4],
            cost=[0.5, 2.0],
            lower=[0.0, 0.0],
            upper=[INFINITY, 3.0],
            starts=[0, 2, 3],
            row_indices=[0, 2, 1],
            values=[1.0, 1.0, 2.0],
        )
    )
    assert (solver.column(10), solver.column(4)) == (3, 4)
    lp = model.getLp()
    assert lp.num_col_ == 5
    assert lp.col_cost_.tolist() == [1.0, 3.0, 1.0, 0.5, 2.0]
    assert list(lp.col_upper_) == [INFINITY, INFINITY, INFINITY, INFINITY, 3.0]
    solver.add_columns(ColumnBatch([], [], [], [], [0], [], []))
    assert model.getNumCol() == 5

    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.primal(np.array([4, 10], dtype=np.uint64)).tolist() == pytest.approx(
        [model.getSolution().col_value[4], model.getSolution().col_value[3]]
    )


def test_batches_are_checked_and_a_refused_batch_adds_nothing() -> None:
    model = self_test_lp_by_rows()
    solver = HighspyMasterSolver(model)
    solver.add_columns(column(1, 1.0, [(0, 1.0)]))

    with pytest.raises(ValueError, match="column id 1 is already used"):
        solver.add_columns(column(1, 1.0, [(0, 1.0)]))

    twice = ColumnBatch([5, 5], [1.0, 1.0], [0.0, 0.0], [1.0, 1.0], [0, 0, 0], [], [])

    with pytest.raises(ValueError, match="column id 5 is twice in the batch"):
        solver.add_columns(twice)

    with pytest.raises(IndexError, match="row that does not exist"):
        solver.add_columns(column(2, 1.0, [(0, 1.0), (3, 1.0)]))

    for cost, entry in ((math.nan, 1.0), (INFINITY, 1.0), (1.0, math.nan), (1.0, -INFINITY)):
        with pytest.raises(ValueError, match="not finite"):
            solver.add_columns(column(2, cost, [(0, entry)]))

    for lower, upper in ((math.nan, 1.0), (INFINITY, INFINITY), (-INFINITY, -INFINITY), (2.0, 1.0)):
        with pytest.raises(ValueError, match="bounds that no column can hold"):
            solver.add_columns(column(2, 1.0, [(0, 1.0)], lower, upper))

    # HiGHS refuses a row listed twice in a column, which the shared checks let through.
    with pytest.raises(ValueError, match="HiGHS refused the columns"):
        solver.add_columns(column(2, 1.0, [(0, 1.0), (0, 2.0)]))

    assert model.getNumCol() == 4

    with pytest.raises(KeyError):
        solver.column(2)

    solver.add_columns(column(2, 1.0, [(1, 1.0)]))
    assert solver.column(2) == 4


# MARK: The solves


def test_integer_columns_stay_integer_and_are_relaxed_by_lp_solves_only() -> None:
    # min -x s.t. 2 x <= 3, x >= 0 integer: the LP gives -1.5, the integer solve -1.
    model = new_model()
    model.addVar(0.0, highspy.kHighsInf)
    model.changeColCost(0, -1.0)
    model.changeColIntegrality(0, highspy.HighsVarType.kInteger)
    model.addRow(-highspy.kHighsInf, 3.0, 1, np.array([0], dtype=np.int32), np.array([2.0]))
    solver = HighspyMasterSolver(model)
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(-1.5)
    assert model.getLp().integrality_[0] == highspy.HighsVarType.kInteger
    assert solver.solve_integer(IntegerParams()) == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(-1.0)


def test_semi_continuous_columns_are_refused() -> None:
    for kind in (highspy.HighsVarType.kSemiContinuous, highspy.HighsVarType.kSemiInteger):
        model = self_test_lp_by_rows()
        model.changeColIntegrality(1, kind)

        with pytest.raises(ValueError, match="semi-continuous"):
            HighspyMasterSolver(model)


def test_the_generated_columns_of_an_integer_solve_are_integer() -> None:
    model = new_model()
    solver = HighspyMasterSolver(model)
    solver.add_rows(RowBatch([2.0], [2.0]))
    solver.add_columns(column(9, 0.5, [(0, 1.0)]))
    solver.add_columns(column(11, 0.2, [(0, 4.0)], upper=1.0))

    # The relaxation takes x11 = 0.5; as an integer column it must be 0.
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(0.1)
    assert solver.solve_integer(IntegerParams(column_type=ColumnType.Integer)) == (
        SolverStatus.Optimal
    )
    assert solver.objective() == pytest.approx(1.0)
    assert solver.primal(np.array([9, 11], dtype=np.uint64)).tolist() == pytest.approx([2.0, 0.0])
    assert list(model.getLp().integrality_) == [highspy.HighsVarType.kInteger] * 2

    # An LP solve relaxes them again, without changing their type.
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(0.1)


def test_a_binary_solve_bounds_the_generated_columns_until_the_next_solve() -> None:
    model = new_model()
    solver = HighspyMasterSolver(model)
    solver.add_rows(RowBatch([-INFINITY], [10.0]))

    # min -x1 - 2 x2 s.t. x1 + x2 <= 10, x2 <= 7: the LP gives -17 (x1 = 3, x2 = 7), binary -3.
    solver.add_columns(column(1, -1.0, [(0, 1.0)]))
    solver.add_columns(column(2, -2.0, [(0, 1.0)], upper=7.0))

    binary = IntegerParams(column_type=ColumnType.Binary)
    assert solver.solve_integer(binary) == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(-3.0)
    assert list(model.getLp().col_upper_) == [1.0, 1.0]

    # The next LP solve puts the generated columns' own bounds back.
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(-17.0)
    assert list(model.getLp().col_upper_) == [INFINITY, 7.0]

    # So does restore_user_model, which a run calls when it ends.
    assert solver.solve_integer(binary) == SolverStatus.Optimal
    assert list(model.getLp().col_upper_) == [1.0, 1.0]
    solver.restore_user_model()
    assert list(model.getLp().col_upper_) == [INFINITY, 7.0]
    solver.restore_user_model()
    assert list(model.getLp().col_upper_) == [INFINITY, 7.0]

    # An integer (not binary) solve leaves the bounds alone.
    assert solver.solve_integer(IntegerParams()) == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(-17.0)
    assert list(model.getLp().col_upper_) == [INFINITY, 7.0]


def lp_with_a_row(cost: float, lower: float, upper: float, coefficients: list[float]) -> Any:
    """Min cost*x s.t.

    lower <= coefficients*x <= upper, x >= 0 (one variable per coefficient).
    """
    model = new_model()

    for _ in coefficients:
        model.addVar(0.0, highspy.kHighsInf)
        model.changeColCost(model.getNumCol() - 1, cost)

    model.addRow(
        lower,
        upper,
        len(coefficients),
        np.arange(len(coefficients), dtype=np.int32),
        np.array(coefficients),
    )
    return model


def test_statuses_of_real_solves() -> None:
    # min -x s.t. x - y <= 1: unbounded (HiGHS may first answer "infeasible or unbounded").
    model = lp_with_a_row(-1.0, -highspy.kHighsInf, 1.0, [1.0, -1.0])
    model.changeColCost(1, 0.0)
    assert HighspyMasterSolver(model).solve_relaxation() == SolverStatus.Unbounded

    # x >= 2 and x <= 1.
    model = lp_with_a_row(1.0, 2.0, highspy.kHighsInf, [1.0])
    model.addRow(-highspy.kHighsInf, 1.0, 1, np.array([0], dtype=np.int32), np.array([1.0]))
    assert HighspyMasterSolver(model).solve_relaxation() == SolverStatus.Infeasible


def patch_statuses(model: Any, *statuses: Any) -> list[str]:
    """Makes the model's next solves end with these statuses, then with its own; returns
    the calls to run and clearSolver, in order, with the presolve option each run
    saw."""
    calls: list[str] = []
    pending = list(statuses)
    real_status = model.getModelStatus
    real_run = model.run
    real_clear = model.clearSolver

    def run() -> Any:
        calls.append(f"run presolve={model.getOptionValue('presolve')[1]}")
        return real_run()

    def clear_solver() -> Any:
        calls.append("clearSolver")
        return real_clear()

    model.run = run
    model.clearSolver = clear_solver
    model.getModelStatus = lambda: pending.pop(0) if pending else real_status()
    return calls


@pytest.mark.parametrize(
    ("highs_status", "status"),
    [
        ("kInfeasible", SolverStatus.Infeasible),
        ("kUnbounded", SolverStatus.Unbounded),
        ("kTimeLimit", SolverStatus.TimeLimit),
        ("kIterationLimit", SolverStatus.Other),
        ("kObjectiveBound", SolverStatus.Other),
    ],
)
def test_the_status_of_a_solve(highs_status: str, status: SolverStatus) -> None:
    model = self_test_lp_by_rows()
    solver = HighspyMasterSolver(model)
    patch_statuses(model, getattr(highspy.HighsModelStatus, highs_status))
    assert solver.solve_relaxation() == status
    assert solver.retried_solves == 0


def test_infeasible_or_unbounded_is_solved_again_without_presolve() -> None:
    model = self_test_lp_by_rows()
    solver = HighspyMasterSolver(model)
    calls = patch_statuses(model, highspy.HighsModelStatus.kUnboundedOrInfeasible)
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert calls == ["run presolve=choose", "run presolve=off"]
    assert model.getOptionValue("presolve")[1] == "choose"

    # A second answer of the same kind is the status: two runs, no more.
    both = highspy.HighsModelStatus.kUnboundedOrInfeasible
    patch_statuses(model, both, both)
    assert solver.solve_relaxation() == SolverStatus.Other


# MARK: A failed LP solve is solved again from scratch


@pytest.mark.parametrize("failure", ["kSolveError", "kUnknown", "kNotset"])
def test_a_failed_lp_solve_is_solved_again_on_a_cleared_solver(failure: str) -> None:
    model = self_test_lp_by_rows()
    solver = HighspyMasterSolver(model)
    calls = patch_statuses(model, getattr(highspy.HighsModelStatus, failure))
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert calls == ["run presolve=choose", "clearSolver", "run presolve=choose"]
    assert solver.retried_solves == 1
    assert solver.objective() == pytest.approx(3.75)
    assert solver.duals() == pytest.approx([1.5, 1.5, -0.5])

    # The count adds up, and a solve that works is not counted.
    patch_statuses(model, highspy.HighsModelStatus.kSolveError)
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.retried_solves == 2


def test_the_retry_has_what_is_left_of_the_time_limit() -> None:
    # HiGHS's time limit counts all the solves of the model, on a clock that clearSolver
    # keeps: the retry runs under the limit of the failed run, so it has what that run left.
    model = self_test_lp_by_rows()
    solver = HighspyMasterSolver(model)
    patch_statuses(model, highspy.HighsModelStatus.kSolveError)
    seen: list[tuple[float, float]] = []
    patched_run = model.run

    def run() -> Any:
        seen.append((model.getOptionValue("time_limit")[1], model.getRunTime()))
        return patched_run()

    model.run = run
    assert solver.solve_relaxation(RelaxationParams(timeout_s=5.0)) == SolverStatus.Optimal

    (limit, clock), (retry_limit, retry_clock) = seen
    assert retry_limit == limit < INFINITY
    assert retry_clock >= clock


def test_a_solve_that_fails_twice_is_other() -> None:
    model = self_test_lp_by_rows()
    solver = HighspyMasterSolver(model)
    error = highspy.HighsModelStatus.kSolveError
    calls = patch_statuses(model, error, error)
    assert solver.solve_relaxation() == SolverStatus.Other
    assert calls.count("clearSolver") == 1 and solver.retried_solves == 1


def test_a_run_that_highs_refuses_is_a_failed_solve() -> None:
    model = self_test_lp_by_rows()
    solver = HighspyMasterSolver(model)
    real_run = model.run
    refused = [True]

    def run() -> Any:
        if refused:
            refused.pop()
            return highspy.HighsStatus.kError

        return real_run()

    model.run = run
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.retried_solves == 1


def test_the_integer_solve_is_not_solved_again() -> None:
    model = self_test_lp_by_rows()
    solver = HighspyMasterSolver(model)
    calls = patch_statuses(model, highspy.HighsModelStatus.kSolveError)
    assert solver.solve_integer() == SolverStatus.Other
    assert calls == ["run presolve=choose"] and solver.retried_solves == 0


def test_a_thread_count_highs_refuses_runs_with_the_count_in_force() -> None:
    model = self_test_lp_by_rows()
    solver = HighspyMasterSolver(model)
    real_run = model.run

    def run() -> Any:
        # HiGHS keeps the count of its first solve in the process.
        if model.getOptionValue("threads")[1] > 0:
            return highspy.HighsStatus.kError

        return real_run()

    model.run = run

    with pytest.warns(RuntimeWarning, match="asked for 3 threads") as caught:
        assert solver.solve_relaxation(RelaxationParams(threads=3)) == SolverStatus.Optimal

    # The warning points at the line that asked for the solve.
    assert caught[0].filename == __file__

    # The user's count is back; the same request does not warn again, another one does.
    assert model.getOptionValue("threads")[1] == 0

    with warnings.catch_warnings():
        warnings.simplefilter("error")
        assert solver.solve_relaxation(RelaxationParams(threads=3)) == SolverStatus.Optimal

    with pytest.warns(RuntimeWarning, match="asked for 2 threads"):
        assert solver.solve_integer(IntegerParams(threads=2)) == SolverStatus.Optimal

    # No retry for the thread count: it is not a failed LP.
    assert solver.retried_solves == 0


def cover_mip(rows: int, columns: int, rows_per_column: int) -> tuple[Any, HighspyMasterSolver]:
    """A random cover MIP: every row covered at least once by columns within 0 and 1, of
    random costs, each in `rows_per_column` rows. The columns are generated ones, made
    integer by the integer solve."""
    rng = np.random.default_rng(1)
    model = new_model()
    solver = HighspyMasterSolver(model)
    solver.add_rows(RowBatch(np.ones(rows), np.full(rows, INFINITY)))
    solver.add_columns(
        ColumnBatch(
            ids=np.arange(columns),
            cost=1.0 + 100.0 * rng.random(columns),
            lower=np.zeros(columns),
            upper=np.ones(columns),
            starts=np.arange(0, columns * rows_per_column + 1, rows_per_column),
            row_indices=np.concatenate(
                [rng.choice(rows, rows_per_column, replace=False) for _ in range(columns)]
            ),
            values=np.ones(columns * rows_per_column),
        )
    )
    return model, solver


def test_the_integer_solve_keeps_its_time_limit_after_lp_solves() -> None:
    # A cover MIP that takes seconds, after LP solves that run HiGHS's clock: its MIP solver
    # compares the time limit with a clock of its own, and overran a limit set from the other.
    model, solver = cover_mip(rows=300, columns=6000, rows_per_column=8)

    for _ in range(10):
        model.clearSolver()
        assert solver.solve_relaxation() == SolverStatus.Optimal

    assert model.getRunTime() > 0.2

    start = time.perf_counter()
    assert solver.solve_integer(IntegerParams(timeout_s=0.2)) == SolverStatus.TimeLimit
    assert time.perf_counter() - start < 1.0

    # The solve leaves no callback behind.
    assert not model.cbMipInterrupt.callbacks


def users_cover_mip() -> tuple[Any, HighspyMasterSolver]:
    """A cover MIP whose integer columns are the user's, so that no integer solve
    changes the model; HiGHS solves it in some twenty callbacks, not in presolve
    alone."""
    model, _ = cover_mip(rows=60, columns=300, rows_per_column=5)
    integer = np.full(300, highspy.HighsVarType.kInteger.value, np.uint8)
    model.changeColsIntegrality(300, np.arange(300, dtype=np.int32), integer)
    return model, HighspyMasterSolver(model)


def test_an_integer_solve_out_of_time_does_not_stop_the_next_one(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    # HiGHS keeps the interrupt request of a solve stopped in its search for the next solves,
    # until the model changes. The time is up at the fifth callback: the first reading of the
    # clock sets the deadline, each callback reads it once.
    _, solver = users_cover_mip()
    readings: list[None] = []

    def perf_counter() -> float:
        readings.append(None)
        return 0.0 if len(readings) <= 5 else 1000.0

    module = sys.modules[HighspyMasterSolver.__module__]
    monkeypatch.setattr(module, "time", SimpleNamespace(perf_counter=perf_counter))
    assert solver.solve_integer(IntegerParams(timeout_s=100.0)) == SolverStatus.TimeLimit
    assert len(readings) == 6

    monkeypatch.undo()
    assert solver.solve_integer(IntegerParams(timeout_s=100.0)) == SolverStatus.Optimal


def test_the_integer_solve_keeps_an_interrupt_of_the_users() -> None:
    # A handler of the user's on the same callback stops the solve at its fifth call.
    model, solver = users_cover_mip()
    calls: list[None] = []

    def stop(event: Any) -> None:
        calls.append(None)

        if len(calls) == 5:
            event.interrupt()

    model.cbMipInterrupt.subscribe(stop)
    assert solver.solve_integer(IntegerParams(timeout_s=100.0)) == SolverStatus.Other
    assert model.getModelStatus() == highspy.HighsModelStatus.kInterrupt
    assert len(calls) == 5


def test_ctrl_c_during_an_lp_solve_waits_for_its_end_and_leaves_the_model_working() -> None:
    model, solver = cover_mip(rows=20, columns=60, rows_per_column=3)
    handler = signal.getsignal(signal.SIGINT)

    # Ctrl-C comes at the first simplex callback, from a handler of the user's: Python would
    # raise it in that handler.
    pressed: list[bool] = []

    def press_ctrl_c(event: Any) -> None:
        if not pressed:
            pressed.append(True)
            signal.raise_signal(signal.SIGINT)

    model.cbSimplexInterrupt.subscribe(press_ctrl_c)

    # HiGHS does not stop on it: the program gets it once the solve has returned.
    with pytest.raises(KeyboardInterrupt):
        solver.solve_relaxation()

    assert pressed
    assert model.getModelStatus() == highspy.HighsModelStatus.kOptimal
    assert signal.getsignal(signal.SIGINT) is handler

    model.cbSimplexInterrupt.unsubscribe(press_ctrl_c)
    assert solver.solve_relaxation() == SolverStatus.Optimal


def test_ctrl_c_stops_an_integer_solve_and_leaves_the_model_working() -> None:
    model, solver = cover_mip(rows=20, columns=60, rows_per_column=3)
    handler = signal.getsignal(signal.SIGINT)

    # Ctrl-C comes at the first callback, from a handler of the user's.
    pressed: list[bool] = []

    def press_ctrl_c(event: Any) -> None:
        if not pressed:
            pressed.append(True)
            signal.raise_signal(signal.SIGINT)

    model.cbMipInterrupt.subscribe(press_ctrl_c)

    # The solve stops, then the program gets Ctrl-C.
    with pytest.raises(KeyboardInterrupt):
        solver.solve_integer(IntegerParams(timeout_s=100.0))

    assert model.getModelStatus() == highspy.HighsModelStatus.kInterrupt
    assert signal.getsignal(signal.SIGINT) is handler

    # An exception raised in a callback would have broken the model: every run would fail.
    model.cbMipInterrupt.unsubscribe(press_ctrl_c)
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.solve_integer(IntegerParams(timeout_s=100.0)) == SolverStatus.Optimal


# MARK: The options of a solve


def test_every_solve_sets_its_options_and_puts_the_users_back() -> None:
    model = self_test_lp_by_rows()
    model.setOptionValue("time_limit", 100.0)
    solver = HighspyMasterSolver(model)
    seen: list[dict[str, Any]] = []
    real_run = model.run

    def run() -> Any:
        names = ("time_limit", "threads", "output_flag", "solve_relaxation", "mip_rel_gap")
        seen.append({name: model.getOptionValue(name)[1] for name in names})
        return real_run()

    model.run = run
    assert solver.solve_relaxation(RelaxationParams(timeout_s=5.0, verbose=True)) == (
        SolverStatus.Optimal
    )
    assert solver.solve_integer(IntegerParams(timeout_s=7.0, relative_gap=0.25)) == (
        SolverStatus.Optimal
    )
    assert solver.solve_relaxation() == SolverStatus.Optimal

    relaxation, integer, unlimited = seen

    # The time limit counts all the solves of the model: the time run already, plus the solve's.
    assert 5.0 <= relaxation["time_limit"] <= 5.0 + model.getRunTime()
    assert relaxation["output_flag"] and relaxation["solve_relaxation"]
    assert 7.0 <= integer["time_limit"] <= 7.0 + model.getRunTime()
    assert not integer["output_flag"] and not integer["solve_relaxation"]
    assert integer["mip_rel_gap"] == 0.25
    assert unlimited["time_limit"] == INFINITY and unlimited["threads"] == 0

    assert model.getOptionValue("time_limit")[1] == 100.0
    assert not model.getOptionValue("output_flag")[1]
    assert not model.getOptionValue("solve_relaxation")[1]
    assert model.getOptionValue("mip_rel_gap")[1] == 1e-4


def test_a_negative_time_limit_is_zero() -> None:
    model = self_test_lp_by_rows()
    solver = HighspyMasterSolver(model)
    seen: list[float] = []
    real_run = model.run

    def run() -> Any:
        seen.append(model.getOptionValue("time_limit")[1])
        return real_run()

    model.run = run
    solver.solve_relaxation(RelaxationParams(timeout_s=-3.0))
    assert 0.0 <= seen[0] <= model.getRunTime()


def test_options_by_highs_name() -> None:
    model = self_test_lp_by_rows()
    solver = HighspyMasterSolver(model)
    solver.set_option("simplex_strategy", 4)
    solver.set_option("primal_feasibility_tolerance", 1e-8)
    solver.set_option("presolve", "off")
    solver.set_option("output_flag", True)

    # An integer for a double option is taken as a double.
    solver.set_option("time_limit", 30)
    assert [
        model.getOptionValue(name)[1]
        for name in (
            "simplex_strategy",
            "primal_feasibility_tolerance",
            "presolve",
            "output_flag",
            "time_limit",
        )
    ] == [4, 1e-8, "off", True, 30.0]
    assert isinstance(model.getOptionValue("time_limit")[1], float)

    model.setOptionValue("output_flag", False)
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.dual_tolerance() == 1e-7
    solver.set_option("dual_feasibility_tolerance", 1e-6)
    assert solver.dual_tolerance() == 1e-6


@pytest.mark.parametrize(
    ("name", "value"),
    [
        ("no_such_option", True),
        ("simplex_strategy", True),
        ("simplex_strategy", 1.5),
        ("simplex_strategy", 2**40),
        ("simplex_strategy", -(2**40)),
        ("time_limit", "soon"),
        ("output_flag", 1),
    ],
)
def test_options_highs_refuses(name: str, value: Any) -> None:
    solver = HighspyMasterSolver(self_test_lp_by_rows())

    with pytest.raises(ValueError, match=f"HiGHS refused the option {name!r}"):
        solver.set_option(name, value)


# MARK: What the last solve gave


def test_the_answers_of_a_solve(monkeypatch: pytest.MonkeyPatch) -> None:
    solver = HighspyMasterSolver(self_test_lp_by_rows())

    with pytest.raises(RuntimeError, match="gave no duals"):
        solver.duals()

    solver.add_columns(column(4, 0.5, [(0, 1.0), (1, 1.0)]))

    with pytest.raises(RuntimeError, match="gave no values"):
        solver.primal(np.array([4], dtype=np.uint64))

    assert solver.primal(np.array([], dtype=np.uint64)).tolist() == []

    assert solver.solve_relaxation() == SolverStatus.Optimal

    with pytest.raises(KeyError):
        solver.primal(np.array([4, 5], dtype=np.uint64))

    assert solver.primal(np.array([4], dtype=np.uint64)).shape == (1,)

    # A column added after the solve leaves no values; if HiGHS kept them, they would be too few.
    solver.add_columns(column(5, 0.5, [(0, 1.0)]))

    with pytest.raises(RuntimeError, match="gave no values"):
        solver.primal(np.array([4, 5], dtype=np.uint64))

    class KeptSolution:
        value_valid = True
        col_value = [0.0] * 4

    monkeypatch.setattr(solver.model, "getSolution", lambda: KeptSolution())

    with pytest.raises(RuntimeError, match="added after the last solve"):
        solver.primal(np.array([4, 5], dtype=np.uint64))


def test_the_largest_cost_of_the_users_columns() -> None:
    assert HighspyMasterSolver(new_model()).largest_user_cost() == 0.0

    model = self_test_lp_by_rows()
    model.changeColCost(0, -7.0)
    solver = HighspyMasterSolver(model)
    assert solver.largest_user_cost() == 7.0

    # The columns the master solver adds are not the user's.
    solver.add_columns(column(1, 100.0, [(0, 1.0)]))
    solver.add_columns(column(2, -200.0, [(1, 1.0)]))
    assert solver.largest_user_cost() == 7.0

    only_generated = new_model()
    only_generated.addRows(1, np.array([0.0]), np.array([1.0]), 0, [], [], [])
    solver = HighspyMasterSolver(only_generated)
    solver.add_columns(column(1, 100.0, [(0, 1.0)]))
    assert solver.largest_user_cost() == 0.0


def test_the_capabilities() -> None:
    capabilities = HighspyMasterSolver(new_model()).capabilities()
    assert capabilities.add_rows and capabilities.integer_solve
    assert not capabilities.farkas and not capabilities.basis
    assert HighspyMasterSolver(new_model()).farkas() is None


def test_passes_the_master_solver_self_test() -> None:
    maximizing = new_model()
    maximizing.changeObjectiveSense(highspy.ObjSense.kMaximize)
    report = layer.master_solver_self_test(
        HighspyMasterSolver(new_model()), HighspyMasterSolver(maximizing)
    )
    assert report.failures == ()


def test_a_master_problem_over_the_master_solver() -> None:
    model = new_model()
    solver = HighspyMasterSolver(model)
    master = layer.MasterProblem(solver)
    master.add_rows_between([2.0, 1.0, -INFINITY], [2.0, INFINITY, 1.5])
    pricer = master.add_pricer(ScriptedPricer(), layer.ConvexityRow(max_paths=5.0))  # row 3
    master.add_columns(
        pricer,
        [
            make_path(1.0, [(0, 1.0), (2, 1.0)], 0.0),
            make_path(3.0, [(0, 1.0), (1, 1.0)], 0.0),
            make_path(1.0, [(1, 1.0), (2, 1.0)], 0.0),
        ],
    )
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(3.75)
    assert solver.duals() == pytest.approx([1.5, 1.5, -0.5, 0.0])

    # A row listed twice in a column is read as the sum, out of order too.
    with pytest.warns(RuntimeWarning, match="more than once"):
        master.add_columns(pricer, [make_path(0.5, [(1, 1.0), (0, 0.25), (0, 0.75)], 0.0)])

    assert entries(model)[:, solver.column(3)].tolist() == [1.0, 1.0, 0.0, 1.0]
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(1.0)

    # The artificial columns are columns of the model too.
    master.add_artificial_columns([1], [-1.0], 9.0)
    assert model.getLp().col_cost_[solver.column(4)] == 9.0
    assert entries(model)[:, solver.column(4)].tolist() == [0.0, -1.0, 0.0, 0.0]


def test_the_class_is_the_one_of_the_package() -> None:
    assert layer.HighspyMasterSolver is HighspyMasterSolver
    assert issubclass(HighspyMasterSolver, layer.MasterSolver)
    assert sys.modules["rcspp.python_column_generation"] is layer
