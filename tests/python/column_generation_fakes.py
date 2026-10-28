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

sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "python", "src")
)

from rcspp._core.graph import Column, Row, Solution  # noqa: E402
from rcspp.column_generation import (  # noqa: E402
    Capabilities,
    MasterSolver,
    Pricer,
    PricingResult,
    PricingStatus,
    SolverStatus,
)


def make_path(cost, rows, reduced_cost):
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


def proven_none():
    """No column, proven by an exact stage."""
    return PricingResult(status=PricingStatus.Optimal, min_reduced_cost_bound=0.0, exact_stage=True)


class ScriptedMasterSolver(MasterSolver):
    """A master solver that records what it is given and answers what the test
    scripts."""

    def __init__(self):
        super().__init__()
        # Rows.
        self.lower = []
        self.upper = []
        self.names = []
        # What was received: (id, cost, lower, upper, [(row, value)]) per column.
        self.columns = []
        self.add_columns_calls = 0
        self.relaxation_calls = []
        self.integer_calls = []
        self.options = []
        self.restore_calls = 0
        # The script: called by every LP solve, it sets the answers below.
        self.on_solve = None
        self.objective_value = 0.0
        self.dual_values = []
        self.values = {}
        self.capabilities_value = Capabilities(add_rows=True, integer_solve=True)
        self.dual_tolerance_value = 0.0
        self.largest_user_cost_value = 0.0

    def number_of_rows(self):
        return len(self.lower)

    def row_bounds(self):
        return self.lower, self.upper

    def add_rows(self, rows):
        self.lower.extend(rows.lower.tolist())
        self.upper.extend(rows.upper.tolist())
        self.names.extend(rows.names if rows.names else [""] * len(rows))

    def add_columns(self, columns):
        self.add_columns_calls += 1
        for column in range(len(columns)):
            entries = range(columns.starts[column], columns.starts[column + 1])
            self.columns.append(
                (
                    int(columns.ids[column]),
                    float(columns.cost[column]),
                    float(columns.lower[column]),
                    float(columns.upper[column]),
                    [(int(columns.row_indices[e]), float(columns.values[e])) for e in entries],
                )
            )

    def solve_relaxation(self, params):
        self.relaxation_calls.append(params)
        return self.on_solve(self) if self.on_solve else SolverStatus.Optimal

    def objective(self):
        return self.objective_value

    def duals(self):
        rows = self.number_of_rows()
        return [
            self.dual_values[row] if row < len(self.dual_values) else 0.0 for row in range(rows)
        ]

    def primal(self, ids):
        return [self.values.get(int(column_id), 0.0) for column_id in ids]

    def solve_integer(self, params):
        self.integer_calls.append(params)
        return SolverStatus.Optimal

    def set_option(self, name, value):
        self.options.append((name, value))

    def capabilities(self):
        return self.capabilities_value

    def restore_user_model(self):
        self.restore_calls += 1

    def dual_tolerance(self):
        return self.dual_tolerance_value

    def largest_user_cost(self):
        return self.largest_user_cost_value


class WrappedModelMasterSolver(MasterSolver):
    """The smallest master solver: a wrapped model whose rows already exist, so it keeps the
    base class's add_rows, farkas and optional methods, and reports no capability."""

    def number_of_rows(self):
        return 0

    def row_bounds(self):
        return [], []

    def add_columns(self, columns):
        pass

    def solve_relaxation(self, params):
        return SolverStatus.Optimal

    def objective(self):
        return 0.0

    def duals(self):
        return []

    def primal(self, ids):
        return [0.0] * len(ids)

    def solve_integer(self, params):
        return SolverStatus.Optimal

    def set_option(self, name, value):
        pass

    def capabilities(self):
        return Capabilities()


class ScriptedPricer(Pricer):
    """A pricer that records each call and answers what the test scripts.

    Keeps the base class's update and cost_scale.
    """

    def __init__(self, respond=None):
        super().__init__()
        self.respond = respond
        self.requests = []
        self.seen_duals = []

    def price(self, duals, request):
        self.requests.append(request)
        self.seen_duals.append(duals)
        if self.respond:
            return self.respond(duals, request)
        return proven_none()


class ScaledPricer(Pricer):
    """A pricer that finds nothing, with a chosen cost scale, and counts updates."""

    def __init__(self, scale):
        super().__init__()
        self.scale = scale
        self.updates = []

    def price(self, duals, request):
        return proven_none()

    def update(self, changes):
        self.updates.append(changes)

    def cost_scale(self):
        return self.scale
