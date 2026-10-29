#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The master solver over a gurobipy model, on small models built as a user would build
them (the cases of tests/cpp/gurobi/test_gurobi_master_solver.cpp).

Skipped without gurobipy or a Gurobi license.
"""

import math
import os
import sys
from typing import Any

import numpy as np
import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from column_generation_fakes import ScriptedPricer, make_path  # noqa: E402

import rcspp.column_generation as column_generation  # noqa: E402
from rcspp._core.graph import Solution  # noqa: E402
from rcspp.column_generation import (  # noqa: E402
    ColumnBatch,
    ColumnType,
    ConvexityRow,
    IntegerParams,
    MasterProblem,
    RelaxationParams,
    SolverStatus,
    master_solver_self_test,
)

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

    min x1 + 3 x2 + x3 subject to x1 + x2 = 2, x2 + x3 ≥ 1, x1 + x3 ≤ 1.5, x ≥ 0. Its
    optimum 3.75 is at (1.25, 0.75, 0.25), every row tight, with the duals (1.5, 1.5,
    −0.5).
    """
    return [
        model.addLConstr(gp.LinExpr(), GRB.EQUAL, 2.0),
        model.addLConstr(gp.LinExpr(), GRB.GREATER_EQUAL, 1.0),
        model.addLConstr(gp.LinExpr(), GRB.LESS_EQUAL, 1.5),
    ]


def covering(cost: float, rows: list[int]) -> Solution:
    """A path of cost `cost` with a coefficient 1 in each of `rows`."""
    return make_path(cost, [(row, 1.0) for row in rows], 0.0)


def lp_columns() -> list[Solution]:
    """X1, x2, x3; they get the ids 0, 1, 2."""
    return [covering(1.0, [0, 2]), covering(3.0, [0, 1]), covering(1.0, [1, 2])]


def add_lp_columns(solver: Any) -> None:
    """Adds the LP's columns through a master over `solver`."""
    master = MasterProblem(solver)
    master.add_columns(master.add_pricer(ScriptedPricer()), lp_columns())


def one_entry_columns(ids: list[int], rows: list[int]) -> ColumnBatch:
    """Columns of cost 1, each with a coefficient 1 in its row."""
    return ColumnBatch(
        ids=ids,
        cost=[1.0] * len(ids),
        lower=[0.0] * len(ids),
        upper=[math.inf] * len(ids),
        starts=list(range(len(ids) + 1)),
        row_indices=rows,
        values=[1.0] * len(ids),
    )


def test_row_bounds_from_the_model() -> None:
    model = new_model()
    solver = column_generation.GurobipyMasterSolver(model, lp_rows(model))
    lower, upper = solver.row_bounds()

    assert solver.number_of_rows() == 3
    assert lower.tolist() == [2.0, 1.0, -math.inf]
    assert upper.tolist() == [2.0, math.inf, 1.5]
    assert solver.model is model


def test_gurobis_infinity_is_read_as_infinite() -> None:
    # A row a·x ≥ −∞, which holds whatever x.
    model = new_model()
    row = model.addLConstr(gp.LinExpr(), GRB.GREATER_EQUAL, -GRB.INFINITY)
    lower, upper = column_generation.GurobipyMasterSolver(model, [row]).row_bounds()
    assert lower.tolist() == [-math.inf]
    assert upper.tolist() == [math.inf]


def test_a_master_with_no_row() -> None:
    solver = column_generation.GurobipyMasterSolver(new_model())
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.duals().tolist() == []
    lower, upper = solver.row_bounds()
    assert lower.tolist() == [] and upper.tolist() == []


def test_every_constraint_by_default() -> None:
    model = new_model()
    lp_rows(model)
    assert column_generation.GurobipyMasterSolver(model).number_of_rows() == 3


def test_passes_the_master_solver_self_test() -> None:
    maximizing = new_model()
    maximizing.ModelSense = GRB.MAXIMIZE
    report = master_solver_self_test(
        column_generation.GurobipyMasterSolver(new_model()),
        column_generation.GurobipyMasterSolver(maximizing),
    )
    assert report.failures == []


def test_a_maximizing_model_is_seen_as_a_minimization() -> None:
    model = new_model()
    model.ModelSense = GRB.MAXIMIZE
    solver = column_generation.GurobipyMasterSolver(model, lp_rows(model))
    add_lp_columns(solver)

    assert solver.solve_relaxation(RelaxationParams()) == SolverStatus.Optimal

    # Gurobi maximizes the negated costs; the master solver reports the minimization.
    assert solver.variable(1).Obj == pytest.approx(-3.0, abs=TOLERANCE)
    assert model.ObjVal == pytest.approx(-3.75, abs=TOLERANCE)
    assert solver.objective() == pytest.approx(3.75, abs=TOLERANCE)
    assert solver.duals().tolist() == pytest.approx([1.5, 1.5, -0.5], abs=TOLERANCE)


def test_infeasible_and_unbounded_are_told_apart() -> None:
    # x ≥ 2 and x ≤ 1.
    model = new_model()
    rows = [
        model.addLConstr(gp.LinExpr(), GRB.GREATER_EQUAL, 2.0),
        model.addLConstr(gp.LinExpr(), GRB.LESS_EQUAL, 1.0),
    ]
    infeasible = column_generation.GurobipyMasterSolver(model, rows)
    master = MasterProblem(infeasible)
    pricer = ScriptedPricer()
    master.add_columns(master.add_pricer(pricer), [covering(1.0, [0, 1])])
    assert infeasible.solve_relaxation() == SolverStatus.Infeasible

    # min −y with y ≥ 1 only.
    unbounded_model = new_model()
    unbounded = column_generation.GurobipyMasterSolver(
        unbounded_model, [unbounded_model.addLConstr(gp.LinExpr(), GRB.GREATER_EQUAL, 1.0)]
    )
    unbounded_master = MasterProblem(unbounded)
    unbounded_master.add_columns(unbounded_master.add_pricer(pricer), [covering(-1.0, [0])])
    assert unbounded.solve_relaxation() == SolverStatus.Unbounded

    # The re-solve that tells them apart leaves the user's setting as it was.
    assert model.Params.DualReductions == 1
    assert unbounded_model.Params.DualReductions == 1


def test_zero_time_left_is_a_time_limit() -> None:
    model = new_model()
    solver = column_generation.GurobipyMasterSolver(model, lp_rows(model))
    add_lp_columns(solver)

    assert solver.solve_relaxation(RelaxationParams(timeout_s=0.0)) == SolverStatus.TimeLimit


def test_the_user_options_come_back_after_each_solve() -> None:
    model = new_model()
    model.Params.TimeLimit = 50.0
    model.Params.Threads = 3
    model.Params.MIPGap = 0.2
    solver = column_generation.GurobipyMasterSolver(model, lp_rows(model))
    add_lp_columns(solver)

    params = RelaxationParams(timeout_s=100.0, threads=1, verbose=False)
    assert solver.solve_relaxation(params) == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(3.75, abs=TOLERANCE)  # outlives the options
    assert model.Params.TimeLimit == 50.0
    assert model.Params.Threads == 3
    assert model.Params.Method == GRB.METHOD_AUTO
    assert model.Params.OutputFlag == 0

    assert solver.solve_integer(IntegerParams(timeout_s=100.0, threads=1)) == SolverStatus.Optimal
    assert model.Params.MIPGap == 0.2
    assert model.Params.Threads == 3


def test_the_user_model_is_restored_after_a_run() -> None:
    model = new_model()
    rows = lp_rows(model)
    integer = model.addVar(0.0, 5.0, 10.0, GRB.INTEGER)
    model.update()
    model.chgCoeff(rows[1], integer, 1.0)
    solver = column_generation.GurobipyMasterSolver(model, rows)
    add_lp_columns(solver)
    assert solver.solve_relaxation() == SolverStatus.Optimal
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
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert integer.VType == GRB.CONTINUOUS


def test_a_method_the_user_chose_is_kept() -> None:
    model = new_model()
    model.Params.Method = GRB.METHOD_BARRIER
    solver = column_generation.GurobipyMasterSolver(model, lp_rows(model))
    add_lp_columns(solver)

    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(3.75, abs=1e-6)
    assert model.Params.Method == GRB.METHOD_BARRIER


def test_resolves_from_the_last_basis_after_adding_a_column() -> None:
    model = new_model()
    solver = column_generation.GurobipyMasterSolver(model, lp_rows(model))
    master = MasterProblem(solver)
    index = master.add_pricer(ScriptedPricer())
    master.add_columns(index, lp_columns())
    assert solver.solve_relaxation() == SolverStatus.Optimal

    # x4 covers the first two rows at cost 0.5: reduced cost 0.5 − 1.5 − 1.5 < 0.
    master.add_columns(index, [covering(0.5, [0, 1])])
    assert solver.solve_relaxation() == SolverStatus.Optimal

    assert solver.objective() == pytest.approx(1.0, abs=TOLERANCE)
    assert model.IterCount <= 3.0


def test_integrality_is_relaxed_then_restored() -> None:
    model = new_model()
    rows = lp_rows(model)
    # The user's own variables: an integer one in the `≥` row, a binary one in no row.
    integer = model.addVar(0.0, 5.0, 10.0, GRB.INTEGER)
    binary = model.addVar(0.0, 1.0, 0.0, GRB.BINARY)
    model.update()
    model.chgCoeff(rows[1], integer, 1.0)
    solver = column_generation.GurobipyMasterSolver(model, rows)
    add_lp_columns(solver)

    # LP: everything continuous, same optimum (the integer variable costs too much).
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(3.75, abs=TOLERANCE)
    assert integer.VType == GRB.CONTINUOUS
    assert binary.VType == GRB.CONTINUOUS
    assert solver.variable(0).VType == GRB.CONTINUOUS

    # MIP: the user's types come back, the columns are integer; x1 = x2 = 1 costs 4.
    assert solver.solve_integer() == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(4.0, abs=1e-6)
    assert integer.VType == GRB.INTEGER
    assert binary.VType == GRB.BINARY
    assert solver.variable(0).VType == GRB.INTEGER

    binary_params = IntegerParams(column_type=ColumnType.Binary)
    assert solver.solve_integer(binary_params) == SolverStatus.Optimal
    assert solver.variable(2).VType == GRB.BINARY

    # And the next LP solve relaxes everything again.
    assert solver.solve_relaxation() == SolverStatus.Optimal
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
    solver = column_generation.GurobipyMasterSolver(model, lp_rows(model))
    add_lp_columns(solver)

    assert solver.solve_relaxation() == SolverStatus.Optimal
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
    solver = column_generation.GurobipyMasterSolver(model, lp_rows(model))
    assert solver.largest_user_cost() == 40.0

    # The generated columns do not count, however costly.
    master = MasterProblem(solver)
    master.add_columns(master.add_pricer(ScriptedPricer()), [covering(100.0, [0])])
    assert solver.largest_user_cost() == 40.0
    assert solver.dual_tolerance() == 1e-7

    # No variable of the user's: 0.
    assert column_generation.GurobipyMasterSolver(new_model()).largest_user_cost() == 0.0


def test_a_variable_the_user_adds_during_the_run_is_relaxed_too() -> None:
    model = new_model()
    rows = lp_rows(model)
    solver = column_generation.GurobipyMasterSolver(model, rows)
    add_lp_columns(solver)
    assert solver.solve_relaxation() == SolverStatus.Optimal

    # An integer variable of the user's, added between two LP solves: in the `≥` row, too
    # dear to be used.
    integer = model.addVar(0.0, 5.0, 10.0, GRB.INTEGER, column=gp.Column([1.0], [rows[1]]))

    # Still an LP solve, with duals.
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.duals()[0] == pytest.approx(1.5, abs=TOLERANCE)
    assert integer.VType == GRB.CONTINUOUS

    # The integer solve gives the user's type back.
    assert solver.solve_integer() == SolverStatus.Optimal
    assert integer.VType == GRB.INTEGER


def test_adds_rows_by_sense_with_names() -> None:
    model = new_model()
    solver = column_generation.GurobipyMasterSolver(model, [])
    assert solver.capabilities().add_rows
    assert solver.capabilities().integer_solve

    master = MasterProblem(solver)
    lower = [1.0, 2.0, -math.inf, -math.inf]
    upper = [1.0, math.inf, 3.0, math.inf]
    rows = master.add_rows(lower, upper, ["equal", "greater", "less", "free"])

    assert rows.count == 4
    lower_out, upper_out = solver.row_bounds()
    assert lower_out.tolist() == lower
    assert upper_out.tolist() == upper
    assert model.getConstrs()[2].ConstrName == "less"

    # A convexity row created by the master, unnamed.
    master.add_pricer(ScriptedPricer(), ConvexityRow(max_paths=2.0))
    assert solver.number_of_rows() == 5
    assert model.getConstrs()[4].Sense == GRB.LESS_EQUAL


def test_refused_rows_add_nothing() -> None:
    model = new_model()
    solver = column_generation.GurobipyMasterSolver(model, [])
    master = MasterProblem(solver)

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
            master.add_rows([0.0, lower], [0.0, upper])

    model.update()
    assert solver.number_of_rows() == 0
    assert model.NumConstrs == 0


def test_unknown_ids_and_rows_raise() -> None:
    model = new_model()
    solver = column_generation.GurobipyMasterSolver(model, lp_rows(model))
    solver.add_columns(one_entry_columns([0], [0]))

    # The same id again, a row that does not exist, an id never received.
    with pytest.raises(ValueError, match="already used"):
        solver.add_columns(one_entry_columns([0], [0]))
    with pytest.raises(IndexError, match="does not exist"):
        solver.add_columns(one_entry_columns([1], [7]))
    with pytest.raises(IndexError, match="no column has the id 42"):
        solver.variable(42)
    with pytest.raises(IndexError):
        solver.primal(np.array([42], dtype=np.uint64))
    assert solver.primal(np.zeros(0, dtype=np.uint64)).tolist() == []


def test_a_refused_batch_adds_no_column() -> None:
    model = new_model()
    solver = column_generation.GurobipyMasterSolver(model, lp_rows(model))

    # The second column is refused: it repeats the first's id, enters a row that does not
    # exist, has a cost that is not finite, or bounds no column holds.
    with pytest.raises(ValueError):
        solver.add_columns(one_entry_columns([3, 3], [0, 0]))
    with pytest.raises(IndexError):
        solver.add_columns(one_entry_columns([2, 4], [0, 7]))
    batch = one_entry_columns([2, 4], [0, 1])
    with pytest.raises(ValueError, match="not finite"):
        solver.add_columns(
            ColumnBatch(
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
            ColumnBatch(
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
    with pytest.raises(IndexError):
        solver.variable(2)

    # The ids are still free.
    solver.add_columns(one_entry_columns([2, 3], [0, 1]))
    assert model.NumVars == 2


def test_options_by_solver_name() -> None:
    model = new_model()
    solver = column_generation.GurobipyMasterSolver(model, [])

    solver.set_option("Threads", 2)
    solver.set_option("MIPGap", 0.05)
    solver.set_option("OutputFlag", False)
    solver.set_option("method", "1")  # Gurobi's names ignore case; text reads as the type
    solver.set_option("LogFile", "")

    assert model.Params.Threads == 2
    assert model.Params.MIPGap == 0.05
    assert model.Params.OutputFlag == 0
    assert model.Params.Method == GRB.METHOD_DUAL

    # A name or a value Gurobi refuses.
    with pytest.raises(ValueError, match="NoSuchParameter"):
        solver.set_option("NoSuchParameter", 1)
    with pytest.raises(ValueError, match="Threads"):
        solver.set_option("Threads", "many")
