#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The master solver over a gurobipy model: small models built as a user would build them
(the cases of test_column_generation_gurobipy.py, with batches of columns instead of a
master problem).

Skipped without gurobipy or a Gurobi license.
"""

import math
import os
import subprocess
import sys
import threading
import warnings
from typing import Any

import numpy as np
import pytest

sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "python", "src")
)

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from python_column_generation_fakes import ScriptedPricer, make_path  # noqa: E402

import rcspp.python_column_generation as layer  # noqa: E402

gp = pytest.importorskip("gurobipy")
GRB = gp.GRB


def has_license() -> bool:
    """Whether Gurobi can build a model on this machine."""
    try:
        gp.Model().dispose()
    except gp.GurobiError:
        return False

    return True


pytestmark = pytest.mark.skipif(not has_license(), reason="Gurobi has no license on this machine")

TOLERANCE = 1e-9


def new_model() -> Any:
    """An empty, quiet Gurobi model."""
    model = gp.Model()
    model.Params.OutputFlag = 0
    return model


def lp_rows(model: Any) -> list[Any]:
    """The rows of the LP most tests use, empty (the columns fill them):

    min x1 + 3 x2 + x3 subject to x1 + x2 = 2, x2 + x3 >= 1, x1 + x3 <= 1.5, x >= 0. Its
    optimum 3.75 is at (1.25, 0.75, 0.25), every row tight, with the duals (1.5, 1.5,
    -0.5).
    """
    return [
        model.addLConstr(gp.LinExpr(), GRB.EQUAL, 2.0),
        model.addLConstr(gp.LinExpr(), GRB.GREATER_EQUAL, 1.0),
        model.addLConstr(gp.LinExpr(), GRB.LESS_EQUAL, 1.5),
    ]


def column(
    column_id: int,
    cost: float,
    entries: list[tuple[int, float]],
    lower: float = 0.0,
    upper: float = math.inf,
) -> Any:
    """A batch of one column."""
    return layer.ColumnBatch(
        ids=[column_id],
        cost=[cost],
        lower=[lower],
        upper=[upper],
        starts=[0, len(entries)],
        row_indices=[row for row, _ in entries],
        values=[value for _, value in entries],
    )


def lp_columns() -> Any:
    """X1, x2, x3 as one batch, under the ids 0, 1, 2."""
    return layer.ColumnBatch(
        ids=[0, 1, 2],
        cost=[1.0, 3.0, 1.0],
        lower=[0.0] * 3,
        upper=[math.inf] * 3,
        starts=[0, 2, 4, 6],
        row_indices=[0, 2, 0, 1, 1, 2],
        values=[1.0] * 6,
    )


def one_entry_columns(ids: list[int], rows: list[int]) -> Any:
    """Columns of cost 1, each with a coefficient 1 in its row."""
    return layer.ColumnBatch(
        ids=ids,
        cost=[1.0] * len(ids),
        lower=[0.0] * len(ids),
        upper=[math.inf] * len(ids),
        starts=list(range(len(ids) + 1)),
        row_indices=rows,
        values=[1.0] * len(ids),
    )


def lp_solver(model: Any = None) -> Any:
    """A master solver over the LP's rows, with its columns added."""
    model = model if model is not None else new_model()
    solver = layer.GurobipyMasterSolver(model, lp_rows(model))
    solver.add_columns(lp_columns())
    return solver


def self_test_script(solver: Any) -> None:
    """What the master solver self-test checks, on a master solver over an empty model:

    rows, columns under ids that are not their positions, values read in another order,
    a column added after a solve, the integer solve with integer, binary, then integer
    columns.
    """
    solver.add_rows(layer.RowBatch([2.0, 1.0, -math.inf], [2.0, math.inf, 1.5]))
    lower, upper = solver.row_bounds()
    assert lower.tolist() == [2.0, 1.0, -math.inf] and upper.tolist() == [2.0, math.inf, 1.5]

    solver.add_columns(
        layer.ColumnBatch(
            ids=[7, 3, 5],
            cost=[1.0, 3.0, 1.0],
            lower=[0.0, 0.0, 0.0],
            upper=[math.inf] * 3,
            starts=[0, 2, 4, 6],
            row_indices=[0, 2, 0, 1, 1, 2],
            values=[1.0] * 6,
        )
    )
    assert solver.solve_relaxation(layer.RelaxationParams()) == layer.SolverStatus.Optimal
    assert solver.objective() == pytest.approx(3.75)
    assert solver.duals() == pytest.approx([1.5, 1.5, -0.5])
    ids = np.array([5, 7, 3], dtype=np.uint64)
    assert solver.primal(ids) == pytest.approx([0.25, 1.25, 0.75])

    solver.add_columns(column(9, 0.5, [(0, 1.0), (1, 1.0)]))
    assert solver.solve_relaxation(layer.RelaxationParams()) == layer.SolverStatus.Optimal
    assert solver.objective() == pytest.approx(1.0)
    assert solver.duals() == pytest.approx([0.5, 0.0, 0.0])
    assert solver.primal(np.array([9, 7], dtype=np.uint64)) == pytest.approx([2.0, 0.0])

    # The relaxation gives 0.55; the integer optimum stays 1.0.
    solver.add_columns(column(11, 0.2, [(0, 4.0)]))
    assert solver.solve_integer(layer.IntegerParams()) == layer.SolverStatus.Optimal
    assert solver.objective() == pytest.approx(1.0)
    assert solver.primal(np.array([11, 9, 7], dtype=np.uint64)) == pytest.approx([0.0, 2.0, 0.0])

    # Binary columns: x4 can no longer be 2. Then integer ones again.
    binary = layer.IntegerParams(column_type=layer.ColumnType.Binary)
    assert solver.solve_integer(binary) == layer.SolverStatus.Optimal
    assert solver.objective() == pytest.approx(1.5)
    assert solver.primal(np.array([11, 9, 7], dtype=np.uint64)) == pytest.approx([0.0, 1.0, 1.0])
    assert solver.solve_integer(layer.IntegerParams()) == layer.SolverStatus.Optimal
    assert solver.objective() == pytest.approx(1.0)


def test_row_bounds_from_the_model() -> None:
    model = new_model()
    solver = layer.GurobipyMasterSolver(model, lp_rows(model))
    lower, upper = solver.row_bounds()

    assert solver.number_of_rows() == 3
    assert lower.tolist() == [2.0, 1.0, -math.inf]
    assert upper.tolist() == [2.0, math.inf, 1.5]
    assert solver.model is model


def test_gurobis_infinity_is_read_as_infinite() -> None:
    # A row a*x >= -inf, which holds whatever x.
    model = new_model()
    row = model.addLConstr(gp.LinExpr(), GRB.GREATER_EQUAL, -GRB.INFINITY)
    lower, upper = layer.GurobipyMasterSolver(model, [row]).row_bounds()
    assert lower.tolist() == [-math.inf]
    assert upper.tolist() == [math.inf]


def test_a_master_with_no_row() -> None:
    solver = layer.GurobipyMasterSolver(new_model())
    assert solver.solve_relaxation() == layer.SolverStatus.Optimal
    assert solver.duals().tolist() == []
    lower, upper = solver.row_bounds()
    assert lower.tolist() == [] and upper.tolist() == []


def test_every_constraint_by_default() -> None:
    model = new_model()
    lp_rows(model)
    assert layer.GurobipyMasterSolver(model).number_of_rows() == 3


def test_passes_the_script_of_the_master_solver_self_test() -> None:
    self_test_script(layer.GurobipyMasterSolver(new_model()))

    maximizing = new_model()
    maximizing.ModelSense = GRB.MAXIMIZE
    self_test_script(layer.GurobipyMasterSolver(maximizing))


def test_passes_the_master_solver_self_test() -> None:
    maximizing = new_model()
    maximizing.ModelSense = GRB.MAXIMIZE
    report = layer.master_solver_self_test(
        layer.GurobipyMasterSolver(new_model()), layer.GurobipyMasterSolver(maximizing)
    )
    assert report.failures == ()


def test_a_master_problem_over_the_master_solver() -> None:
    model = new_model()
    solver = layer.GurobipyMasterSolver(model, [])
    master = layer.MasterProblem(solver)
    master.add_rows_between([2.0, 1.0, -math.inf], [2.0, math.inf, 1.5])
    pricer = master.add_pricer(ScriptedPricer())
    master.add_columns(
        pricer,
        [
            make_path(1.0, [(0, 1.0), (2, 1.0)], 0.0),
            make_path(3.0, [(0, 1.0), (1, 1.0)], 0.0),
            make_path(1.0, [(1, 1.0), (2, 1.0)], 0.0),
        ],
    )
    assert solver.solve_relaxation() == layer.SolverStatus.Optimal
    assert solver.objective() == pytest.approx(3.75, abs=TOLERANCE)
    assert solver.duals().tolist() == pytest.approx([1.5, 1.5, -0.5], abs=TOLERANCE)

    # x4 covers the first two rows at cost 0.5, listing the first one twice (0.25 + 0.75),
    # next to a convexity row of its own; the sum is what the model holds.
    convexity = master.add_pricer(ScriptedPricer(), layer.ConvexityRow(max_paths=3.0))
    assert master.pricers[convexity].convexity_row == 3

    with warnings.catch_warnings():
        warnings.simplefilter("ignore", RuntimeWarning)
        master.add_columns(convexity, [make_path(0.5, [(0, 0.25), (1, 1.0), (0, 0.75)], 0.0)])

    model.update()
    rows = model.getConstrs()
    assert model.getCoeff(rows[0], solver.variable(3)) == 1.0
    assert model.getCoeff(rows[3], solver.variable(3)) == 1.0
    assert solver.solve_relaxation() == layer.SolverStatus.Optimal
    assert solver.objective() == pytest.approx(1.0, abs=TOLERANCE)

    # The artificial columns are columns of the model too.
    master.add_artificial_columns([1], [-1.0], 9.0)
    model.update()
    assert solver.variable(4).Obj == 9.0
    assert model.getCoeff(rows[1], solver.variable(4)) == -1.0


def test_a_maximizing_model_is_seen_as_a_minimization() -> None:
    model = new_model()
    model.ModelSense = GRB.MAXIMIZE
    solver = lp_solver(model)

    assert solver.solve_relaxation(layer.RelaxationParams()) == layer.SolverStatus.Optimal

    # Gurobi maximizes the negated costs; the master solver reports the minimization.
    assert solver.variable(1).Obj == pytest.approx(-3.0, abs=TOLERANCE)
    assert model.ObjVal == pytest.approx(-3.75, abs=TOLERANCE)
    assert solver.objective() == pytest.approx(3.75, abs=TOLERANCE)
    assert solver.duals().tolist() == pytest.approx([1.5, 1.5, -0.5], abs=TOLERANCE)


def test_infeasible_and_unbounded_are_told_apart() -> None:
    # x >= 2 and x <= 1.
    model = new_model()
    rows = [
        model.addLConstr(gp.LinExpr(), GRB.GREATER_EQUAL, 2.0),
        model.addLConstr(gp.LinExpr(), GRB.LESS_EQUAL, 1.0),
    ]
    infeasible = layer.GurobipyMasterSolver(model, rows)
    infeasible.add_columns(column(0, 1.0, [(0, 1.0), (1, 1.0)]))
    assert infeasible.solve_relaxation() == layer.SolverStatus.Infeasible

    # min -y with y >= 1 only.
    unbounded_model = new_model()
    unbounded = layer.GurobipyMasterSolver(
        unbounded_model, [unbounded_model.addLConstr(gp.LinExpr(), GRB.GREATER_EQUAL, 1.0)]
    )
    unbounded.add_columns(column(0, -1.0, [(0, 1.0)]))
    assert unbounded.solve_relaxation() == layer.SolverStatus.Unbounded

    # The re-solve that tells them apart leaves the user's setting as it was.
    assert model.Params.DualReductions == 1
    assert unbounded_model.Params.DualReductions == 1


class ScriptedSolves:
    """A Gurobi model whose solves end with the statuses given, one per solve, and take
    the run times given (the real ones by default); records the time limit of each
    solve."""

    def __init__(
        self, model: Any, statuses: list[int], runtimes: list[float] | None = None
    ) -> None:
        self._model = model
        self._statuses = statuses
        self._runtimes = runtimes
        self.time_limits: list[float] = []

    def __getattr__(self, name: str) -> Any:
        return getattr(self._model, name)

    @property
    def solves(self) -> int:
        return len(self.time_limits)

    def optimize(self) -> None:
        self.time_limits.append(self._model.Params.TimeLimit)
        self._model.optimize()

    @property
    def Status(self) -> int:  # noqa: N802  # Gurobi's name
        return self._statuses[self.solves - 1]

    @property
    def Runtime(self) -> float:  # noqa: N802  # Gurobi's name
        if self._runtimes is None:
            runtime: float = self._model.Runtime
            return runtime

        return self._runtimes[self.solves - 1]


def test_the_second_solve_gets_the_time_the_first_left() -> None:
    # The solve that tells "infeasible or unbounded" apart, after a first one of 4 seconds.
    model = new_model()
    lp_rows(model)
    scripted = ScriptedSolves(model, [GRB.INF_OR_UNBD, GRB.OPTIMAL], runtimes=[4.0, 1.0])
    solver = layer.GurobipyMasterSolver(scripted)  # type: ignore[arg-type]

    params = layer.RelaxationParams(timeout_s=10.0)
    assert solver.solve_relaxation(params) == layer.SolverStatus.Optimal
    assert scripted.time_limits == [10.0, 6.0]
    assert model.Params.TimeLimit == math.inf


def test_zero_time_left_is_a_time_limit() -> None:
    solver = lp_solver()
    assert (
        solver.solve_relaxation(layer.RelaxationParams(timeout_s=0.0))
        == layer.SolverStatus.TimeLimit
    )

    # A negative time left is no time either.
    assert (
        solver.solve_relaxation(layer.RelaxationParams(timeout_s=-1.0))
        == layer.SolverStatus.TimeLimit
    )


def test_the_user_options_come_back_after_each_solve() -> None:
    model = new_model()
    model.Params.TimeLimit = 50.0
    model.Params.Threads = 3
    model.Params.MIPGap = 0.2
    solver = lp_solver(model)

    params = layer.RelaxationParams(timeout_s=100.0, threads=1, verbose=False)
    assert solver.solve_relaxation(params) == layer.SolverStatus.Optimal
    assert solver.objective() == pytest.approx(3.75, abs=TOLERANCE)  # outlives the options
    assert model.Params.TimeLimit == 50.0
    assert model.Params.Threads == 3
    assert model.Params.Method == GRB.METHOD_AUTO
    assert model.Params.OutputFlag == 0

    integer = layer.IntegerParams(timeout_s=100.0, threads=1)
    assert solver.solve_integer(integer) == layer.SolverStatus.Optimal
    assert model.Params.MIPGap == 0.2
    assert model.Params.Threads == 3

    # No time limit of the solve is an infinite one, and the user's comes back.
    assert solver.solve_relaxation() == layer.SolverStatus.Optimal
    assert model.Params.TimeLimit == 50.0


def test_the_user_model_is_restored_after_a_run() -> None:
    model = new_model()
    rows = lp_rows(model)
    integer = model.addVar(0.0, 5.0, 10.0, GRB.INTEGER)
    model.update()
    model.chgCoeff(rows[1], integer, 1.0)
    solver = layer.GurobipyMasterSolver(model, rows)
    solver.add_columns(lp_columns())
    assert solver.solve_relaxation() == layer.SolverStatus.Optimal
    assert integer.VType == GRB.CONTINUOUS

    solver.restore_user_model()

    # The last solve's values can still be read; Gurobi applies the types at the next update.
    assert solver.primal(np.array([0], dtype=np.uint64)).tolist() == pytest.approx(
        [1.25], abs=TOLERANCE
    )
    model.update()

    # The user's type is back; the generated columns stay, continuous.
    assert integer.VType == GRB.INTEGER
    assert solver.variable(0).VType == GRB.CONTINUOUS
    model.optimize()  # the user's own solve is a MIP again
    assert model.IsMIP == 1

    # A further LP solve relaxes it again.
    assert solver.solve_relaxation() == layer.SolverStatus.Optimal
    assert integer.VType == GRB.CONTINUOUS


def test_a_method_the_user_chose_is_kept() -> None:
    model = new_model()
    model.Params.Method = GRB.METHOD_BARRIER
    solver = lp_solver(model)

    assert solver.solve_relaxation() == layer.SolverStatus.Optimal
    assert solver.objective() == pytest.approx(3.75, abs=1e-6)
    assert model.Params.Method == GRB.METHOD_BARRIER


def test_resolves_from_the_last_basis_after_adding_a_column() -> None:
    model = new_model()
    solver = lp_solver(model)
    assert solver.solve_relaxation() == layer.SolverStatus.Optimal

    # x4 covers the first two rows at cost 0.5: reduced cost 0.5 - 1.5 - 1.5 < 0.
    solver.add_columns(column(3, 0.5, [(0, 1.0), (1, 1.0)]))
    assert solver.solve_relaxation() == layer.SolverStatus.Optimal

    assert solver.objective() == pytest.approx(1.0, abs=TOLERANCE)
    assert model.IterCount <= 3.0


def test_integrality_is_relaxed_then_restored() -> None:
    model = new_model()
    rows = lp_rows(model)

    # The user's own variables: an integer one in the `>=` row, a binary one in no row.
    integer = model.addVar(0.0, 5.0, 10.0, GRB.INTEGER)
    binary = model.addVar(0.0, 1.0, 0.0, GRB.BINARY)
    model.update()
    model.chgCoeff(rows[1], integer, 1.0)
    solver = layer.GurobipyMasterSolver(model, rows)
    solver.add_columns(lp_columns())

    # LP: everything continuous, same optimum (the integer variable costs too much).
    assert solver.solve_relaxation() == layer.SolverStatus.Optimal
    assert solver.objective() == pytest.approx(3.75, abs=TOLERANCE)
    assert integer.VType == GRB.CONTINUOUS
    assert binary.VType == GRB.CONTINUOUS
    assert solver.variable(0).VType == GRB.CONTINUOUS

    # MIP: the user's types come back, the columns are integer; x1 = x2 = 1 costs 4.
    assert solver.solve_integer() == layer.SolverStatus.Optimal
    assert solver.objective() == pytest.approx(4.0, abs=1e-6)
    assert integer.VType == GRB.INTEGER
    assert binary.VType == GRB.BINARY
    assert solver.variable(0).VType == GRB.INTEGER

    binary_params = layer.IntegerParams(column_type=layer.ColumnType.Binary)
    assert solver.solve_integer(binary_params) == layer.SolverStatus.Optimal
    assert solver.variable(2).VType == GRB.BINARY

    # And the next LP solve relaxes everything again.
    assert solver.solve_relaxation() == layer.SolverStatus.Optimal
    assert solver.objective() == pytest.approx(3.75, abs=TOLERANCE)
    assert integer.VType == GRB.CONTINUOUS
    assert solver.variable(2).VType == GRB.CONTINUOUS


def test_a_relaxed_binary_stays_within_zero_and_one() -> None:
    # A binary of the user's, in no row and worth 1. Gurobi stores an upper bound of 1 when a
    # binary is created, but keeps one set later: once continuous, the variable would make the
    # LP unbounded.
    model = new_model()
    binary = model.addVar(0.0, 1.0, -1.0, GRB.BINARY)
    model.update()
    binary.UB = GRB.INFINITY
    solver = lp_solver(model)

    assert solver.solve_relaxation() == layer.SolverStatus.Optimal
    assert solver.objective() == pytest.approx(3.75 - 1.0, abs=TOLERANCE)

    # Its own bounds come back with its type.
    solver.restore_user_model()
    model.update()
    assert binary.UB == math.inf  # gurobipy reads Gurobi's infinity as inf
    assert binary.VType == GRB.BINARY


def test_the_tolerance_and_the_user_costs_come_from_the_model() -> None:
    model = new_model()
    model.Params.OptimalityTol = 1e-7

    # A starting column of the user's, costlier than every generated one.
    model.addVar(0.0, GRB.INFINITY, -40.0, GRB.CONTINUOUS)
    solver = layer.GurobipyMasterSolver(model, lp_rows(model))
    assert solver.largest_user_cost() == 40.0

    # The generated columns do not count, however costly.
    solver.add_columns(column(0, 100.0, [(0, 1.0)]))
    assert solver.largest_user_cost() == 40.0
    assert solver.dual_tolerance() == 1e-7

    # No variable of the user's: 0.
    assert layer.GurobipyMasterSolver(new_model()).largest_user_cost() == 0.0
    assert layer.GurobipyMasterSolver(new_model()).dual_tolerance() == 1e-6


def test_a_variable_the_user_adds_during_the_run_is_relaxed_too() -> None:
    model = new_model()
    rows = lp_rows(model)
    solver = layer.GurobipyMasterSolver(model, rows)
    solver.add_columns(lp_columns())
    assert solver.solve_relaxation() == layer.SolverStatus.Optimal

    # An integer variable of the user's, added between two LP solves: in the `>=` row, too
    # dear to be used.
    integer = model.addVar(0.0, 5.0, 10.0, GRB.INTEGER, column=gp.Column([1.0], [rows[1]]))

    # Still an LP solve, with duals.
    assert solver.solve_relaxation() == layer.SolverStatus.Optimal
    assert solver.duals()[0] == pytest.approx(1.5, abs=TOLERANCE)
    assert integer.VType == GRB.CONTINUOUS

    # The integer solve gives the user's type back.
    assert solver.solve_integer() == layer.SolverStatus.Optimal
    assert integer.VType == GRB.INTEGER


def test_adds_rows_by_sense_with_names() -> None:
    model = new_model()
    solver = layer.GurobipyMasterSolver(model, [])
    assert solver.capabilities().add_rows
    assert solver.capabilities().integer_solve

    lower = [1.0, 2.0, -math.inf, -math.inf]
    upper = [1.0, math.inf, 3.0, math.inf]
    solver.add_rows(layer.RowBatch(lower, upper, ["equal", "greater", "less", "free"]))

    assert solver.number_of_rows() == 4
    lower_out, upper_out = solver.row_bounds()
    assert lower_out.tolist() == lower
    assert upper_out.tolist() == upper
    assert [row.ConstrName for row in model.getConstrs()] == ["equal", "greater", "less", "free"]
    assert [row.Sense for row in model.getConstrs()] == ["=", ">", "<", "<"]

    # Rows without names, after others.
    solver.add_rows(layer.RowBatch([0.0], [math.inf]))
    assert solver.number_of_rows() == 5
    assert solver.row_bounds()[0].tolist()[4] == 0.0


def test_refused_rows_add_nothing() -> None:
    model = new_model()
    solver = layer.GurobipyMasterSolver(model, [])

    # A range, then bounds no Gurobi row holds; each after a valid row.
    for lower, upper in (
        (1.0, 2.0),
        (math.inf, math.inf),
        (-math.inf, -math.inf),
        (math.inf, 1.0),
        (1.0, -math.inf),
        (math.nan, 1.0),
        (-math.inf, math.nan),
    ):
        with pytest.raises(ValueError):
            solver.add_rows(layer.RowBatch([0.0, lower], [0.0, upper]))

    model.update()
    assert solver.number_of_rows() == 0
    assert model.NumConstrs == 0


def test_unknown_ids_and_rows_raise() -> None:
    model = new_model()
    solver = layer.GurobipyMasterSolver(model, lp_rows(model))
    solver.add_columns(one_entry_columns([0], [0]))

    # The same id again, a row that does not exist, an id never received.
    with pytest.raises(ValueError, match="already used"):
        solver.add_columns(one_entry_columns([0], [0]))

    with pytest.raises(IndexError, match="does not exist"):
        solver.add_columns(one_entry_columns([1], [7]))

    with pytest.raises(KeyError, match="no column has the id 42"):
        solver.variable(42)

    with pytest.raises(KeyError):
        solver.primal(np.array([42], dtype=np.uint64))

    assert solver.primal(np.zeros(0, dtype=np.uint64)).tolist() == []


def test_a_refused_batch_adds_no_column() -> None:
    model = new_model()
    solver = layer.GurobipyMasterSolver(model, lp_rows(model))

    # The second column is refused: it repeats the first's id, enters a row that does not
    # exist, has a cost that is not finite, or bounds no column holds.
    with pytest.raises(ValueError):
        solver.add_columns(one_entry_columns([3, 3], [0, 0]))

    with pytest.raises(IndexError):
        solver.add_columns(one_entry_columns([2, 4], [0, 7]))

    batch = one_entry_columns([2, 4], [0, 1])

    with pytest.raises(ValueError, match="not finite"):
        solver.add_columns(
            layer.ColumnBatch(
                ids=batch.ids,
                cost=[1.0, math.nan],
                lower=batch.lower,
                upper=batch.upper,
                starts=batch.starts,
                row_indices=batch.row_indices,
                values=batch.values,
            )
        )

    with pytest.raises(ValueError, match="bounds"):
        solver.add_columns(
            layer.ColumnBatch(
                ids=batch.ids,
                cost=batch.cost,
                lower=[0.0, 2.0],
                upper=[1.0, 1.0],
                starts=batch.starts,
                row_indices=batch.row_indices,
                values=batch.values,
            )
        )

    model.update()
    assert model.NumVars == 0

    with pytest.raises(KeyError):
        solver.variable(2)

    # The ids are still free.
    solver.add_columns(one_entry_columns([2, 3], [0, 1]))
    assert model.NumVars == 2


def test_the_bounds_and_costs_of_the_columns_reach_the_model() -> None:
    model = new_model()
    solver = layer.GurobipyMasterSolver(model, lp_rows(model))
    solver.add_columns(
        layer.ColumnBatch(
            ids=[5, 6],
            cost=[2.5, -1.0],
            lower=[-math.inf, 1.0],
            upper=[4.0, math.inf],
            starts=[0, 2, 3],
            row_indices=[2, 0, 1],
            values=[0.5, 2.0, 1.5],
        )
    )
    model.update()

    first, second = solver.variable(5), solver.variable(6)
    assert (first.LB, first.UB, first.Obj) == (-math.inf, 4.0, 2.5)
    assert (second.LB, second.UB, second.Obj) == (1.0, math.inf, -1.0)
    rows = model.getConstrs()
    assert model.getCoeff(rows[2], first) == 0.5 and model.getCoeff(rows[0], first) == 2.0
    assert model.getCoeff(rows[1], second) == 1.5 and model.getCoeff(rows[0], second) == 0.0


def test_options_by_solver_name() -> None:
    model = new_model()
    solver = layer.GurobipyMasterSolver(model, [])

    solver.set_option("Threads", 2)
    solver.set_option("MIPGap", 0.05)
    solver.set_option("OutputFlag", False)
    solver.set_option("method", "1")  # Gurobi's names ignore case; text reads as the type
    solver.set_option("LogFile", "")

    assert model.Params.Threads == 2
    assert model.Params.MIPGap == 0.05
    assert model.Params.OutputFlag == 0
    assert model.Params.Method == GRB.METHOD_DUAL

    # A bool for a number, as gurobipy takes it; an int for a double.
    solver.set_option("OutputFlag", True)
    solver.set_option("MIPGap", 1)
    assert model.Params.OutputFlag == 1 and model.Params.MIPGap == 1.0

    # A name or a value Gurobi refuses.
    with pytest.raises(ValueError, match="NoSuchParameter"):
        solver.set_option("NoSuchParameter", 1)

    with pytest.raises(ValueError, match="Threads"):
        solver.set_option("Threads", "many")

    with pytest.raises(ValueError, match="Threads"):
        solver.set_option("Threads", -5)

    # A number or a bool for a text parameter: gurobipy fails on it with another error.
    for value in (True, 1):
        with pytest.raises(ValueError, match="LogFile"):
            solver.set_option("LogFile", value)


# MARK: Importing


def test_importing_the_layer_loads_no_gurobipy() -> None:
    # A new process: this one has loaded it already.
    code = (
        "import sys, rcspp.python_column_generation as layer;"
        "layer.GurobipyMasterSolver;"
        "print('gurobipy' in sys.modules)"
    )
    loaded = subprocess.run(
        [sys.executable, "-c", code],
        capture_output=True,
        text=True,
        check=True,
        env={**os.environ, "PYTHONPATH": os.pathsep.join(sys.path)},
    )
    assert loaded.stdout.strip() == "False"


def test_a_master_solver_without_gurobipy_names_the_extra(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setitem(sys.modules, "gurobipy", None)  # as if it were not installed

    with pytest.raises(ImportError, match=r"pip install rcspp\[gurobipy\]"):
        layer.GurobipyMasterSolver(object())  # type: ignore[arg-type]


def test_an_interrupted_solve_raises_keyboard_interrupt_and_puts_the_options_back() -> None:
    # An assignment LP that primal simplex needs a few seconds for. Gurobi answers an
    # interruption (Ctrl-C, or terminate from another thread) with a status, not an exception.
    size = 600
    model = new_model()
    model.Params.Presolve = 0
    x = model.addMVar((size, size), obj=np.random.default_rng(0).random((size, size)))
    model.addConstr(x.sum(axis=0) == 1)
    model.addConstr(x.sum(axis=1) == 1)
    solver = layer.GurobipyMasterSolver(model)

    # From the start and often, whatever the speed of the machine: a call before the solve
    # starts may be lost, and the solve lasts far longer than the interval.
    done = threading.Event()

    def terminate() -> None:
        while not done.is_set():
            model.terminate()
            done.wait(0.01)

    terminator = threading.Thread(target=terminate)
    terminator.start()

    try:
        with pytest.raises(KeyboardInterrupt):
            solver.solve_relaxation(layer.RelaxationParams(timeout_s=60.0, threads=1))
    finally:
        done.set()
        terminator.join()

    assert model.Status == GRB.INTERRUPTED
    assert model.Params.TimeLimit == math.inf and model.Params.Threads == 0
    assert model.Params.Method == GRB.METHOD_AUTO


def test_an_interrupted_second_solve_raises_keyboard_interrupt() -> None:
    # The solve that tells "infeasible or unbounded" apart is interrupted too.
    model = new_model()
    lp_rows(model)
    scripted = ScriptedSolves(model, [GRB.INF_OR_UNBD, GRB.INTERRUPTED])
    solver = layer.GurobipyMasterSolver(scripted)  # type: ignore[arg-type]

    with pytest.raises(KeyboardInterrupt):
        solver.solve_relaxation()

    assert scripted.solves == 2
    assert model.Params.DualReductions == 1 and model.Params.TimeLimit == math.inf
