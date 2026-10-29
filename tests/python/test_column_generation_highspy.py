#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The master solver over a copy of a highspy model (rcspp built with HiGHS, highspy
installed)."""

import math
import os
import sys
from typing import Any, Callable

import numpy as np
import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from test_column_generation_end_to_end import Toy, routing_graph  # noqa: E402

import rcspp.column_generation as column_generation  # noqa: E402
from rcspp.column_generation import (  # noqa: E402
    ColumnGeneration,
    ColumnGenerationStatus,
    ColumnType,
    ConvexityRow,
    GraphPricer,
    IntegerParams,
    MasterProblem,
    SolverStatus,
    master_solver_self_test,
)

highspy = pytest.importorskip("highspy")

pytestmark = pytest.mark.skipif(
    not hasattr(column_generation, "HighspyMasterSolver"), reason="rcspp built without HiGHS"
)

# Accuracy of the values checked: within what LP solvers return.
TOLERANCE = 1e-6


def new_model() -> Any:
    """An empty, quiet highspy model."""
    model = highspy.Highs()
    model.setOptionValue("output_flag", False)
    return model


def self_test_lp_by_rows() -> Any:
    """The self-test's LP, built row by row (a row-wise matrix).

    min x1 + 3 x2 + x3 subject to x1 + x2 = 2, x2 + x3 ≥ 1, x1 + x3 ≤ 1.5, x ≥ 0; its
    optimum is 3.75 with the duals (1.5, 1.5, −0.5).
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


def customer_rows(customers: int) -> Any:
    """The toy's user model: one row "customer i is visited once" per customer."""
    model = new_model()
    for _ in range(customers):
        model.addRow(1.0, 1.0, 0, [], [])
    return model


def toy_from_scratch(model: Any) -> tuple[MasterProblem, Any]:
    """Column generation on the toy over a copy of `model`, from no route at all."""
    solver = column_generation.HighspyMasterSolver(model)
    master = MasterProblem(solver)
    master.add_pricer(GraphPricer(routing_graph(Toy.capacity, Toy.demands, Toy.distances)))
    result = ColumnGeneration(master).run()
    assert result.status == ColumnGenerationStatus.Optimal
    assert result.objective == pytest.approx(Toy.lp, abs=TOLERANCE)
    return master, solver


def solve(model: Any) -> float:
    """The optimum of the user's model, solved by highspy."""
    model.run()
    assert model.getModelStatus() == highspy.HighsModelStatus.kOptimal
    objective: float = model.getInfo().objective_function_value
    return objective


# ── The copy ─────────────────────────────────────────────────────────────────


def test_passes_the_master_solver_self_test() -> None:
    maximizing = new_model()
    maximizing.changeObjectiveSense(highspy.ObjSense.kMaximize)
    report = master_solver_self_test(
        column_generation.HighspyMasterSolver(new_model()),
        column_generation.HighspyMasterSolver(maximizing),
    )
    assert report.failures == []


@pytest.mark.parametrize("build", [self_test_lp_by_rows, self_test_lp_by_columns])
def test_a_copy_of_the_users_model(build: Callable[[], Any]) -> None:
    model = build()
    solver = column_generation.HighspyMasterSolver(model)
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
    solver = column_generation.HighspyMasterSolver(model)
    assert solver.solve_relaxation() == SolverStatus.Optimal
    # max −3.75 + 1 is min 3.75 − 1; the duals are those of the minimization.
    assert solver.objective() == pytest.approx(2.75)
    assert solver.duals() == pytest.approx([1.5, 1.5, -0.5])


def test_the_rows_that_columns_enter() -> None:
    solver = column_generation.HighspyMasterSolver(self_test_lp_by_rows(), rows=[2, 0])
    lower, upper = solver.row_bounds()
    assert lower.tolist() == [-math.inf, 2.0]
    assert upper.tolist() == [1.5, 2.0]


def test_integer_columns_stay_integer() -> None:
    # min −x s.t. 2 x ≤ 3, x ≥ 0 integer: the LP gives −1.5, the integer solve −1.
    model = new_model()
    model.addVar(0.0, highspy.kHighsInf)
    model.changeColCost(0, -1.0)
    model.changeColIntegrality(0, highspy.HighsVarType.kInteger)
    model.addRow(-highspy.kHighsInf, 3.0, 1, np.array([0], dtype=np.int32), np.array([2.0]))
    solver = column_generation.HighspyMasterSolver(model)
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(-1.5)
    assert solver.solve_integer(IntegerParams()) == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(-1.0)


def test_semi_continuous_columns_are_refused() -> None:
    model = self_test_lp_by_rows()
    model.changeColIntegrality(1, highspy.HighsVarType.kSemiContinuous)
    with pytest.raises(ValueError, match="semi-continuous"):
        column_generation.HighspyMasterSolver(model)


def test_later_changes_to_the_model_are_not_seen() -> None:
    model = self_test_lp_by_rows()
    solver = column_generation.HighspyMasterSolver(model)
    model.changeColCost(0, 100.0)
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(3.75)


# ── Writing back ─────────────────────────────────────────────────────────────


def test_the_generated_columns_written_back() -> None:
    model = customer_rows(Toy.customers)
    master, solver = toy_from_scratch(model)
    artificial = master.artificial_column_ids().tolist()
    assert artificial  # the run started from artificial columns

    written = solver.write_back(master)

    real = [column.id for column in master.columns() if column.id not in artificial]
    assert sorted(written) == real
    assert sorted(written.values()) == list(range(len(real)))
    assert model.getNumCol() == len(real)
    assert solve(model) == pytest.approx(Toy.lp, abs=TOLERANCE)

    # The values are the master solver's: the user's LP takes the same columns.
    values = solver.primal(np.array(real, dtype=np.uint64))
    model_values = model.getSolution().col_value
    assert [model_values[written[column]] for column in real] == pytest.approx(
        values.tolist(), abs=TOLERANCE
    )


def test_written_back_columns_of_a_type() -> None:
    model = customer_rows(Toy.customers)
    master, solver = toy_from_scratch(model)
    written = solver.write_back(master, ColumnType.Integer)
    assert solve(model) == pytest.approx(Toy.best_plan, abs=TOLERANCE)
    assert all(
        model.getLp().integrality_[column] == highspy.HighsVarType.kInteger
        for column in written.values()
    )

    model = customer_rows(Toy.customers)
    master, solver = toy_from_scratch(model)
    written = solver.write_back(master, ColumnType.Binary)
    assert solve(model) == pytest.approx(Toy.best_plan, abs=TOLERANCE)
    assert all(model.getLp().col_upper_[column] <= 1.0 for column in written.values())


def test_the_bounds_of_an_integer_solve_are_not_written_back() -> None:
    model = customer_rows(Toy.customers)
    solver = column_generation.HighspyMasterSolver(model)
    master = MasterProblem(solver)
    master.add_pricer(GraphPricer(routing_graph(Toy.capacity, Toy.demands, Toy.distances)))
    loop = ColumnGeneration(master)
    loop.run()
    params = IntegerParams(column_type=ColumnType.Binary)
    assert loop.solve_restricted_master_integer(params) == SolverStatus.Optimal

    written = solver.write_back(master)
    assert all(model.getLp().col_upper_[column] == math.inf for column in written.values())


def test_writing_back_twice_adds_only_the_new_columns() -> None:
    model = customer_rows(Toy.customers)
    master, solver = toy_from_scratch(model)
    first = solver.write_back(master)
    assert solver.write_back(master) == {}
    assert model.getNumCol() == len(first)


def test_rows_the_master_created_are_written_back() -> None:
    model = customer_rows(Toy.customers)
    solver = column_generation.HighspyMasterSolver(model)
    master = MasterProblem(solver)
    graph = routing_graph(Toy.capacity, Toy.demands, Toy.distances)
    # A convexity row (unnamed) and a named row that no column enters.
    master.add_pricer(GraphPricer(graph), ConvexityRow(max_paths=2.0))
    master.add_rows([-math.inf], [5.0], ["spare"])
    result = ColumnGeneration(master).run()
    assert result.status == ColumnGenerationStatus.Optimal

    solver.write_back(master)
    lp = model.getLp()
    assert model.getNumRow() == Toy.customers + 2
    assert list(lp.row_upper_[Toy.customers :]) == [2.0, 5.0]
    assert lp.row_names_[Toy.customers :] == ["", "spare"]
    assert solve(model) == pytest.approx(Toy.lp, abs=TOLERANCE)


def test_write_back_refusals() -> None:
    model = customer_rows(Toy.customers)
    master, solver = toy_from_scratch(model)
    other = MasterProblem(column_generation.HighspyMasterSolver(customer_rows(Toy.customers)))
    with pytest.raises(ValueError, match="another solver"):
        solver.write_back(other)

    model.addRow(0.0, 1.0, 0, [], [])
    with pytest.raises(ValueError, match="rows were added to or removed"):
        solver.write_back(master)
