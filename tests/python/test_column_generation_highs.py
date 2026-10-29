#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The HiGHS master solver from Python, over a model built from arrays (rcspp built with
HiGHS)."""

import math
import os
import sys
from typing import Any

import numpy as np
import pytest

sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "python", "src")
)

import rcspp.column_generation as column_generation  # noqa: E402
from rcspp.column_generation import (  # noqa: E402
    ColumnBatch,
    IntegerParams,
    RelaxationParams,
    RowBatch,
    SolverStatus,
    master_solver_self_test,
)

pytestmark = pytest.mark.skipif(
    not hasattr(column_generation, "HighsMasterSolver"), reason="rcspp built without HiGHS"
)

# The self-test's LP: min x1 + 3 x2 + x3 s.t. x1 + x2 = 2, x2 + x3 ≥ 1, x1 + x3 ≤ 1.5, x ≥ 0;
# its optimum is 3.75 with the duals (1.5, 1.5, −0.5).
SELF_TEST_LP: dict[str, Any] = dict(
    column_cost=[1.0, 3.0, 1.0],
    column_lower=[0.0, 0.0, 0.0],
    column_upper=[math.inf, math.inf, math.inf],
    row_lower=[2.0, 1.0, -math.inf],
    row_upper=[2.0, math.inf, 1.5],
    starts=[0, 2, 4, 6],
    row_indices=[0, 2, 0, 1, 1, 2],
    values=[1.0, 1.0, 1.0, 1.0, 1.0, 1.0],
)


def test_passes_the_master_solver_self_test() -> None:
    report = master_solver_self_test(
        column_generation.HighsMasterSolver(), column_generation.HighsMasterSolver(maximize=True)
    )
    assert report.failures == []


def test_a_model_from_arrays() -> None:
    solver = column_generation.HighsMasterSolver(**SELF_TEST_LP)
    assert solver.number_of_rows() == 3
    assert solver.largest_user_cost() == 3.0
    assert solver.solve_relaxation(RelaxationParams()) == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(3.75)
    assert solver.duals() == pytest.approx([1.5, 1.5, -0.5])


def test_a_maximizing_model_is_seen_as_a_minimization() -> None:
    model = SELF_TEST_LP | {"column_cost": [-1.0, -3.0, -1.0]}
    solver = column_generation.HighsMasterSolver(**model, maximize=True, offset=1.0)
    assert solver.solve_relaxation() == SolverStatus.Optimal
    # max −3.75 + 1 is min 3.75 − 1; the duals are those of the minimization.
    assert solver.objective() == pytest.approx(2.75)
    assert solver.duals() == pytest.approx([1.5, 1.5, -0.5])


def test_the_rows_that_columns_enter() -> None:
    solver = column_generation.HighsMasterSolver(**SELF_TEST_LP, rows=[2, 0])
    lower, upper = solver.row_bounds()
    assert lower.tolist() == [-math.inf, 2.0]
    assert upper.tolist() == [1.5, 2.0]
    # A column entering master row 0 enters model row 2.
    solver.add_columns(
        ColumnBatch(
            ids=[7],
            cost=[0.5],
            lower=[0.0],
            upper=[math.inf],
            starts=[0, 1],
            row_indices=[0],
            values=[1.0],
        )
    )
    assert solver.column(7) == 3
    with pytest.raises(IndexError):
        solver.column(8)


def test_the_columns_and_rows_of_the_model() -> None:
    solver = column_generation.HighsMasterSolver(**SELF_TEST_LP, rows=[2, 0])
    solver.add_rows(RowBatch(lower=[-math.inf], upper=[4.0], names=["fleet"]))
    solver.add_columns(
        ColumnBatch(
            ids=[0, 1],
            cost=[0.5, 2.0],
            lower=[0.0, 0.0],
            upper=[math.inf, 3.0],
            starts=[0, 2, 3],
            row_indices=[0, 2, 1],
            values=[1.0, 1.0, 2.0],
        )
    )

    # Column 1 first; the entries are in the model's rows: master row 0 is model row 2,
    # and the added master row 2 is model row 3.
    cost, lower, upper, starts, row_indices, values = solver.model_columns([1, 0])
    assert cost.tolist() == [2.0, 0.5]
    assert lower.tolist() == [0.0, 0.0]
    assert upper.tolist() == [3.0, math.inf]
    assert starts.tolist() == [0, 1, 3]
    assert row_indices.tolist() == [0, 2, 3]
    assert values.tolist() == [2.0, 1.0, 1.0]

    lower, upper, names = solver.model_rows(2)
    assert lower.tolist() == [-math.inf, -math.inf]
    assert upper.tolist() == [1.5, 4.0]
    assert names == ["", "fleet"]
    assert len(solver.model_rows()[2]) == 4
    assert solver.model_rows(4)[2] == []
    for first in (-1, 5):
        with pytest.raises(IndexError, match="first must be between"):
            solver.model_rows(first)


def test_integer_columns_are_relaxed_by_lp_solves_only() -> None:
    # min −x s.t. 2 x ≤ 3, x ≥ 0 integer: the LP gives −1.5, the integer solve −1.
    solver = column_generation.HighsMasterSolver(
        column_cost=[-1.0],
        column_lower=[0.0],
        column_upper=[math.inf],
        integer=[True],
        row_lower=[-math.inf],
        row_upper=[3.0],
        starts=[0, 1],
        row_indices=[0],
        values=[2.0],
    )
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(-1.5)
    assert solver.solve_integer(IntegerParams()) == SolverStatus.Optimal
    assert solver.objective() == pytest.approx(-1.0)


def test_options_by_highs_name() -> None:
    solver = column_generation.HighsMasterSolver(**SELF_TEST_LP)
    solver.set_option("simplex_strategy", 4)
    solver.set_option("primal_feasibility_tolerance", 1e-8)
    assert solver.solve_relaxation() == SolverStatus.Optimal
    assert solver.dual_tolerance() > 0.0
    with pytest.raises(ValueError):
        solver.set_option("no_such_option", True)


def test_refused_models() -> None:
    for change in (
        {"column_lower": [0.0]},
        {"integer": [True]},
        {"row_upper": [2.0]},
        {"starts": [0, 2, 4]},
        {"values": [1.0]},
    ):
        with pytest.raises(ValueError, match="one value per column"):
            column_generation.HighsMasterSolver(**(SELF_TEST_LP | change))
    with pytest.raises(ValueError, match="starts must rise"):
        column_generation.HighsMasterSolver(**(SELF_TEST_LP | {"starts": [0, 4, 2, 6]}))
    with pytest.raises(ValueError, match="one-dimensional"):
        column_generation.HighsMasterSolver(column_cost=np.zeros((1, 1)))
    with pytest.raises(IndexError, match="not a row of the model"):
        column_generation.HighsMasterSolver(**SELF_TEST_LP, rows=[3])
