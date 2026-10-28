#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Scripted master solvers and pricers for the column generation tests, written in
Python.

They mirror the C++ fakes of tests/cpp/test_column_generation_fakes.hpp: each records
what it is given and answers what the test scripts, so the tests reach the C++ loop
through the Python subclasses of MasterSolver and Pricer.
"""

import os
import sys
from typing import Any, Callable, NamedTuple, Sequence, TypeAlias

import numpy as np

sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "python", "src")
)

from rcspp._core.graph import Column, Row, Solution  # noqa: E402
from rcspp.column_generation import (  # noqa: E402
    Capabilities,
    ColumnBatch,
    IntegerParams,
    MasterSolver,
    Pricer,
    PricingChanges,
    PricingRequest,
    PricingResult,
    PricingStatus,
    RelaxationParams,
    RowBatch,
    SolverStatus,
)

FloatArray: TypeAlias = np.ndarray[Any, np.dtype[np.float64]]
IdArray: TypeAlias = np.ndarray[Any, np.dtype[np.uint64]]
OptionValue: TypeAlias = bool | int | float | str
# A pricer's answer to the duals and the request it receives.
Respond: TypeAlias = Callable[[FloatArray, PricingRequest], PricingResult]


def make_path(cost: float, rows: Sequence[tuple[int, float]], reduced_cost: float) -> Solution:
    """A pricer's solution: a path of reduced cost `reduced_cost` whose column has
    `cost` and the (row, coefficient) entries `rows`."""
    column = Column()
    column.cost = cost
    column.rows = [Row(index=row, coefficient=coefficient) for row, coefficient in rows]
    solution = Solution()
    solution.cost = reduced_cost
    solution.path_node_ids = [0, 1]
    solution.path_arc_ids = [0]
    solution.column = column
    return solution


def proven_none() -> PricingResult:
    """No column, proven by an exact stage."""
    return PricingResult(status=PricingStatus.Optimal, min_reduced_cost_bound=0.0, exact_stage=True)


class ReceivedColumn(NamedTuple):
    """A column as the master solver received it."""

    id: int
    cost: float
    lower: float
    upper: float
    entries: list[tuple[int, float]]


class ScriptedMasterSolver(MasterSolver):
    """A master solver that records what it is given and answers what the test
    scripts."""

    def __init__(self) -> None:
        super().__init__()
        # Rows.
        self.lower: list[float] = []
        self.upper: list[float] = []
        self.names: list[str] = []
        # What was received.
        self.columns: list[ReceivedColumn] = []
        self.add_columns_calls = 0
        self.relaxation_calls: list[RelaxationParams] = []
        self.integer_calls: list[IntegerParams] = []
        self.options: list[tuple[str, OptionValue]] = []
        self.restore_calls = 0
        # The script: called by every LP solve, it sets the answers below.
        self.on_solve: Callable[[ScriptedMasterSolver], SolverStatus] | None = None
        self.objective_value = 0.0
        self.dual_values: list[float] = []
        self.values: dict[int, float] = {}
        self.capabilities_value = Capabilities(add_rows=True, integer_solve=True)
        self.dual_tolerance_value = 0.0
        self.largest_user_cost_value = 0.0

    def number_of_rows(self) -> int:
        return len(self.lower)

    def row_bounds(self) -> tuple[FloatArray, FloatArray]:
        return np.array(self.lower, dtype=np.float64), np.array(self.upper, dtype=np.float64)

    def add_rows(self, rows: RowBatch) -> None:
        self.lower.extend(float(value) for value in rows.lower)
        self.upper.extend(float(value) for value in rows.upper)
        self.names.extend(rows.names if rows.names else [""] * len(rows))

    def add_columns(self, columns: ColumnBatch) -> None:
        self.add_columns_calls += 1
        starts = [int(start) for start in columns.starts]
        for column in range(len(columns)):
            entries = range(starts[column], starts[column + 1])
            self.columns.append(
                ReceivedColumn(
                    id=int(columns.ids[column]),
                    cost=float(columns.cost[column]),
                    lower=float(columns.lower[column]),
                    upper=float(columns.upper[column]),
                    entries=[
                        (int(columns.row_indices[entry]), float(columns.values[entry]))
                        for entry in entries
                    ],
                )
            )

    def solve_relaxation(self, params: RelaxationParams = RelaxationParams()) -> SolverStatus:
        self.relaxation_calls.append(params)
        return self.on_solve(self) if self.on_solve else SolverStatus.Optimal

    def objective(self) -> float:
        return self.objective_value

    def duals(self) -> FloatArray:
        rows = self.number_of_rows()
        duals = [
            self.dual_values[row] if row < len(self.dual_values) else 0.0 for row in range(rows)
        ]
        return np.array(duals, dtype=np.float64)

    def primal(self, ids: IdArray) -> FloatArray:
        return np.array(
            [self.values.get(int(column_id), 0.0) for column_id in ids], dtype=np.float64
        )

    def solve_integer(self, params: IntegerParams = IntegerParams()) -> SolverStatus:
        self.integer_calls.append(params)
        return SolverStatus.Optimal

    def set_option(self, name: str, value: OptionValue) -> None:
        self.options.append((name, value))

    def capabilities(self) -> Capabilities:
        return self.capabilities_value

    def restore_user_model(self) -> None:
        self.restore_calls += 1

    def dual_tolerance(self) -> float:
        return self.dual_tolerance_value

    def largest_user_cost(self) -> float:
        return self.largest_user_cost_value


class WrappedModelMasterSolver(MasterSolver):
    """The smallest master solver: a wrapped model whose rows already exist, so it keeps
    the base class's add_rows, farkas and optional methods, and reports no capability."""

    def number_of_rows(self) -> int:
        return 0

    def row_bounds(self) -> tuple[FloatArray, FloatArray]:
        return np.zeros(0), np.zeros(0)

    def add_columns(self, columns: ColumnBatch) -> None:
        pass

    def solve_relaxation(self, params: RelaxationParams = RelaxationParams()) -> SolverStatus:
        return SolverStatus.Optimal

    def objective(self) -> float:
        return 0.0

    def duals(self) -> FloatArray:
        return np.zeros(0)

    def primal(self, ids: IdArray) -> FloatArray:
        return np.zeros(len(ids))

    def solve_integer(self, params: IntegerParams = IntegerParams()) -> SolverStatus:
        return SolverStatus.Optimal

    def set_option(self, name: str, value: OptionValue) -> None:
        pass

    def capabilities(self) -> Capabilities:
        return Capabilities()


class ScriptedPricer(Pricer):
    """A pricer that records each call and answers what the test scripts.

    Keeps the base class's update and cost_scale.
    """

    def __init__(self, respond: Respond | None = None) -> None:
        super().__init__()
        self.respond = respond
        self.requests: list[PricingRequest] = []
        self.seen_duals: list[FloatArray] = []

    def price(self, duals: FloatArray, request: PricingRequest = PricingRequest()) -> PricingResult:
        self.requests.append(request)
        self.seen_duals.append(duals)
        if self.respond:
            return self.respond(duals, request)
        return proven_none()


class ScaledPricer(Pricer):
    """A pricer that finds nothing, with a chosen cost scale, and records updates."""

    def __init__(self, scale: float) -> None:
        super().__init__()
        self.scale = scale
        self.updates: list[PricingChanges] = []

    def price(self, duals: FloatArray, request: PricingRequest = PricingRequest()) -> PricingResult:
        return proven_none()

    def update(self, changes: PricingChanges) -> None:
        self.updates.append(changes)

    def cost_scale(self) -> float:
        return self.scale
