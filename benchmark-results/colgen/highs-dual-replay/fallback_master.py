"""A pure-Python master solver over highspy that solves again from scratch when HiGHS fails (fix A), and the
artificial start with a smaller automatic factor (fix B). A prototype for measurements, LP solves only.

Fix A: HiGHS stops with "excessive dual values" on a warm-started dual simplex when the costs are large; the same LP
solved from scratch (cleared solver, presolve on) is optimal. Fix B: an automatic artificial cost of 1e4 times the
cost scale instead of 1e6, below the limit where that failure starts (7.7e5 times the cost scale on R201_25 and
RC201_12).
"""

import math
import time

import highspy
import numpy as np
from rcspp.column_generation import ArtificialColumns, MasterInitialization, MasterSolver, SolverStatus
from rcspp.column_generation import Capabilities

STATUSES = {
    highspy.HighsModelStatus.kOptimal: SolverStatus.Optimal,
    highspy.HighsModelStatus.kInfeasible: SolverStatus.Infeasible,
    highspy.HighsModelStatus.kUnbounded: SolverStatus.Unbounded,
    highspy.HighsModelStatus.kTimeLimit: SolverStatus.TimeLimit,
}

# Statuses a solve from scratch can turn into a result.
RETRIED = {
    highspy.HighsModelStatus.kSolveError,
    highspy.HighsModelStatus.kUnknown,
    highspy.HighsModelStatus.kNotset,
}


class FallbackHighspyMasterSolver(MasterSolver):
    """A minimization master over its own ``highspy.Highs``; a failed solve is solved again from scratch."""

    def __init__(self, row_lower, row_upper, retry=True, progress=None):
        super().__init__()
        self._highs = highspy.Highs()
        self._highs.setOptionValue("output_flag", False)
        self._highs.addRows(len(row_lower), np.asarray(row_lower, float), np.asarray(row_upper, float), 0, [], [], [])
        self._retry = retry
        # The HiGHS column of each column id.
        self._columns = {}
        # Solves that failed and were solved again, by solve number.
        self.retries = []
        self.solves = 0
        # A file that gets one line per solve, to follow a long run.
        self._progress = progress
        self._start = time.perf_counter()

    def number_of_rows(self):
        return self._highs.getNumRow()

    def row_bounds(self):
        lp = self._highs.getLp()
        return np.array(lp.row_lower_, dtype=np.float64), np.array(lp.row_upper_, dtype=np.float64)

    def add_rows(self, rows):
        lower, upper = np.asarray(rows.lower, float), np.asarray(rows.upper, float)
        self._highs.addRows(len(lower), lower, upper, 0, [], [], [])

    def add_columns(self, columns):
        if not np.isfinite(columns.cost).all() or not np.isfinite(columns.values).all():
            raise ValueError("add_columns: a column has a cost or a coefficient that is not finite")
        first = self._highs.getNumCol()
        count = len(columns.ids)
        self._highs.addCols(
            count,
            np.asarray(columns.cost, float),
            np.asarray(columns.lower, float),
            np.asarray(columns.upper, float),
            len(columns.values),
            np.asarray(columns.starts[:-1], np.int32),
            np.asarray(columns.row_indices, np.int32),
            np.asarray(columns.values, float),
        )
        for offset, column_id in enumerate(columns.ids):
            self._columns[int(column_id)] = first + offset

    def solve_relaxation(self, params=None):
        if params is not None:
            self._highs.setOptionValue("time_limit", max(0.0, params.timeout_s) if math.isfinite(params.timeout_s) else math.inf)
        self.solves += 1
        self._highs.run()
        status = self._highs.getModelStatus()

        # A warm-started dual simplex can fail on large costs where a solve from scratch does not.
        if self._retry and status in RETRIED:
            self.retries.append(self.solves)
            self._highs.clearSolver()
            self._highs.run()
            status = self._highs.getModelStatus()

        if self._progress is not None:
            with open(self._progress, "a") as log:
                print(
                    f"{time.perf_counter() - self._start:9.1f} s  solve {self.solves:4d}  "
                    f"{status.name:12s} objective {self.objective():.6g}  "
                    f"columns {self._highs.getNumCol()}  retries {len(self.retries)}",
                    file=log,
                )
        return STATUSES.get(status, SolverStatus.Other)

    def solve_integer(self, params=None):
        raise NotImplementedError("LP solves only")

    def objective(self):
        return self._highs.getInfo().objective_function_value

    def duals(self):
        return np.array(self._highs.getSolution().row_dual, dtype=np.float64)

    def primal(self, ids):
        values = self._highs.getSolution().col_value
        return np.array([values[self._columns[int(column_id)]] for column_id in ids], dtype=np.float64)

    def farkas(self):
        return None

    def set_option(self, name, value):
        self._highs.setOptionValue(name, value)

    def capabilities(self):
        return Capabilities(add_rows=True)

    def dual_tolerance(self):
        return self._highs.getOptions().dual_feasibility_tolerance

    def largest_user_cost(self):
        return 0.0

    def restore_user_model(self):
        pass


class SmallerArtificialColumns(MasterInitialization):
    """Artificial columns at ``factor`` times the cost scale (the library's automatic factor is 1e6)."""

    def __init__(self, factor):
        super().__init__()
        self._factor = factor

    def initialize(self, master, context):
        ArtificialColumns(cost=self._factor * context.cost_scale).initialize(master, context)
