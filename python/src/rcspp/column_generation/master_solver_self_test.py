#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""A check that a master solver follows the conventions of :class:`MasterSolver`."""

import math
from dataclasses import dataclass

import numpy as np

from .master_solver import MasterSolver
from .types import (
    ArrayLike,
    ColumnBatch,
    ColumnType,
    IntArrayLike,
    IntegerParams,
    RelaxationParams,
    RowBatch,
    SolverStatus,
)

#: Default absolute tolerance of :func:`master_solver_self_test` on the values it compares.
DEFAULT_SELF_TEST_TOLERANCE = 1e-6


@dataclass(frozen=True)
class MasterSolverSelfTestReport:
    """Outcome of :func:`master_solver_self_test`."""

    #: One line per failed check: the model, the step, what was checked, the expected value and
    #: the value the master solver gave.
    failures: tuple[str, ...] = ()

    @property
    def passed(self) -> bool:
        """Whether every check passed."""
        return not self.failures


# The rows of the LP described in master_solver_self_test.
_ROW_LOWER = [2.0, 1.0, -math.inf]
_ROW_UPPER = [2.0, math.inf, 1.5]
_ROW_NAMES = ["row 0 (=)", "row 1 (>=)", "row 2 (<=)"]

# The optimum of the first LP, and of the LP with x4.
_FIRST_OBJECTIVE = 3.75
_FIRST_DUALS = [1.5, 1.5, -0.5]
_ADDED_OBJECTIVE = 1.0
_ADDED_DUALS = [0.5, 0.0, 0.0]
_ADDED_X4 = 2.0
_INTEGER_OBJECTIVE = 1.0
_BINARY_OBJECTIVE = 1.5


def _batch(
    ids: IntArrayLike, costs: ArrayLike, starts: IntArrayLike, rows: IntArrayLike, values: ArrayLike
) -> ColumnBatch:
    """Columns bounded by 0 and infinity."""
    return ColumnBatch(
        ids=ids,
        cost=costs,
        lower=[0.0] * len(costs),
        upper=[math.inf] * len(costs),
        starts=starts,
        row_indices=rows,
        values=values,
    )


# x1, x2, x3, under ids that are neither their positions nor sorted.
_FIRST_COLUMNS = _batch([7, 3, 5], [1.0, 3.0, 1.0], [0, 2, 4, 6], [0, 2, 0, 1, 1, 2], [1.0] * 6)

# x4: cost 0.5 in rows 0 and 1.
_ADDED_COLUMN = _batch([9], [0.5], [0, 2], [0, 1], [1.0, 1.0])

# x5: cost 0.2, and 4 in row 0, added before the integer solve. The LP optimum becomes 0.55 at
# x4 = 1, x5 = 0.25, while the integer optimum stays 1.0 at x4 = 2: a master solver that solves
# the relaxation instead is caught.
_FRACTIONAL_COLUMN = _batch([11], [0.2], [0, 1], [0], [4.0])

# The values are read in an order that differs from the order the columns were added in.
_FIRST_READ = ([5, 7, 3], ["x3", "x1", "x2"], [0.25, 1.25, 0.75])
_ADDED_READ = ([9, 7], ["x4", "x1"], [_ADDED_X4, 0.0])
_INTEGER_READ = ([11, 9, 7], ["x5", "x4", "x1"], [0.0, _ADDED_X4, 0.0])
_BINARY_READ = ([11, 9, 7], ["x5", "x4", "x1"], [0.0, 1.0, 1.0])


class _Checker:
    """Records the failed checks of one master solver, each line prefixed by the model
    and the step."""

    def __init__(self, model: str, tolerance: float, failures: list[str]) -> None:
        self._model = model
        self._tolerance = tolerance
        self._failures = failures
        self.step = ""

    def near(self, what: str, expected: float, actual: float) -> None:
        # Equal values pass too, so that infinite bounds compare.
        if expected == actual or abs(expected - actual) <= self._tolerance:
            return

        self.fail(f"{what}: expected {expected!r}, got {float(actual)!r}")

    def optimal(self, status: SolverStatus) -> bool:
        """Whether the solve ended optimal; reports it when not."""
        if status == SolverStatus.Optimal:
            return True

        self.fail(f"status: expected Optimal, got {status.name}")
        return False

    def sized(self, what: str, values: ArrayLike, expected: int) -> bool:
        """Whether the master solver answered ``expected`` values; reports it when
        not."""
        if len(values) == expected:
            return True

        self.fail(f"{what}: expected {expected} values, got {len(values)}")
        return False

    def fail(self, line: str) -> None:
        self._failures.append(f"{self._model}: {self.step}: {line}")


def _check_duals(check: _Checker, solver: MasterSolver, expected: list[float]) -> bool:
    """Checks the duals; whether they were of the right size."""
    duals = solver.duals()

    if not check.sized("duals", duals, len(expected)):
        return False

    for row, (want, got) in enumerate(zip(expected, duals)):
        check.near(f"dual of {_ROW_NAMES[row]}", want, got)

    return True


def _check_values(
    check: _Checker, solver: MasterSolver, read: tuple[list[int], list[str], list[float]]
) -> bool:
    """Checks the values of the columns read; whether they were of the right size."""
    ids, names, expected = read
    values = solver.primal(np.array(ids, dtype=np.uint64))

    if not check.sized("values", values, len(ids)):
        return False

    for column_id, name, want, got in zip(ids, names, expected, values):
        check.near(f"value of {name} (id {column_id})", want, got)

    return True


def _check_master_solver(
    solver: MasterSolver, model: str, tolerance: float, failures: list[str]
) -> None:
    check = _Checker(model, tolerance, failures)

    # 1. The rows, read back.
    check.step = "rows"
    solver.add_rows(RowBatch(_ROW_LOWER, _ROW_UPPER))
    lower, upper = solver.row_bounds()

    if not (check.sized("lower bounds", lower, 3) and check.sized("upper bounds", upper, 3)):
        return

    for row in range(3):
        check.near(f"lower bound of {_ROW_NAMES[row]}", _ROW_LOWER[row], lower[row])
        check.near(f"upper bound of {_ROW_NAMES[row]}", _ROW_UPPER[row], upper[row])

    # 2. x1, x2, x3.
    check.step = "first LP"
    solver.add_columns(_FIRST_COLUMNS)

    if not check.optimal(solver.solve_relaxation(RelaxationParams())):
        return

    check.near("objective", _FIRST_OBJECTIVE, solver.objective())

    if not (
        _check_duals(check, solver, _FIRST_DUALS) and _check_values(check, solver, _FIRST_READ)
    ):
        return

    # 3. x4, added after a solve.
    check.step = "LP after adding x4"
    solver.add_columns(_ADDED_COLUMN)

    if not check.optimal(solver.solve_relaxation(RelaxationParams())):
        return

    check.near("objective", _ADDED_OBJECTIVE, solver.objective())

    if not (
        _check_duals(check, solver, _ADDED_DUALS) and _check_values(check, solver, _ADDED_READ)
    ):
        return

    # 4. The integer problem, when the master solver solves it.
    if not solver.capabilities().integer_solve:
        return

    check.step = "integer solve"
    solver.add_columns(_FRACTIONAL_COLUMN)

    if not check.optimal(solver.solve_integer(IntegerParams())):
        return

    check.near("objective", _INTEGER_OBJECTIVE, solver.objective())

    if not _check_values(check, solver, _INTEGER_READ):
        return

    # 5. Binary columns: x4 can no longer be 2.
    check.step = "binary solve"

    if not check.optimal(solver.solve_integer(IntegerParams(column_type=ColumnType.Binary))):
        return

    check.near("objective", _BINARY_OBJECTIVE, solver.objective())

    if not _check_values(check, solver, _BINARY_READ):
        return

    # 6. Integer columns again: the bounds of the binary solve are gone.
    check.step = "integer solve after the binary one"

    if not check.optimal(solver.solve_integer(IntegerParams())):
        return

    check.near("objective", _INTEGER_OBJECTIVE, solver.objective())
    _check_values(check, solver, _INTEGER_READ)


def _require_empty_and_extensible(solver: MasterSolver, model: str) -> None:
    if solver.number_of_rows() != 0 or not solver.capabilities().add_rows:
        raise ValueError(
            f"master_solver_self_test: the {model} master solver must have no rows and be able to "
            "add rows"
        )


def master_solver_self_test(
    minimizing: MasterSolver,
    maximizing: MasterSolver,
    tolerance: float = DEFAULT_SELF_TEST_TOLERANCE,
) -> MasterSolverSelfTestReport:
    """Checks that a master solver follows the conventions of :class:`MasterSolver`.

    The column generation loop trusts the answers of its master solver: a dual with the wrong
    sign gives wrong reduced costs without any error. This test solves an LP whose answers are
    known, through the interface only, so it works for any master solver::

        min  x1 + 3 x2 + x3
        s.t. x1 + x2      = 2      (row 0)
             x2 + x3      >= 1      (row 1)
             x1 + x3      <= 1.5    (row 2)
             x >= 0

    Its optimum is 3.75 at ``x = (1.25, 0.75, 0.25)``, with the duals ``(1.5, 1.5, -0.5)``: one
    row of each kind, each dual of its expected sign. The test then adds a column ``x4`` of cost
    0.5 in rows 0 and 1 and solves again: 1.0 at ``x4 = 2``, with the duals ``(0.5, 0, 0)``.
    Both optima are unique, primal and dual. The steps, on each of the two master solvers:

    1. add the three rows, and read their bounds back;
    2. add ``x1``, ``x2``, ``x3`` under ids that are neither their positions nor sorted, solve,
       and check the objective, the duals and the values read by id in another order;
    3. add ``x4``, solve again, and check the new optimum;
    4. when ``Capabilities.integer_solve`` is set, add ``x5`` of cost 0.2 with 4 in row 0, which
       makes the LP optimum fractional (0.55 at ``x4 = 1``, ``x5 = 0.25``), and solve the
       integer problem: 1.0 at ``x4 = 2``, ``x5 = 0``. A master solver that solves the
       relaxation is caught;
    5. solve it with binary columns (``ColumnType.Binary``): 1.5 at ``x1 = x4 = 1``;
    6. solve it with integer columns again: 1.0 at ``x4 = 2``, so that a master solver that
       keeps the bounds of the binary solve is caught.

    A step whose solve does not end ``SolverStatus.Optimal`` is reported and ends the checks on
    that master solver, and so is an answer of the wrong size.

    Args:
        minimizing: A master solver over an empty minimizing model, able to add rows.
        maximizing: A master solver of the same kind over an empty maximizing model: it must
            give the same answers, as the master is always seen as a minimization.
        tolerance: Absolute tolerance on the values compared.

    Returns:
        The failed checks of both master solvers.

    Raises:
        ValueError: When a master solver already has rows or cannot add rows.
    """
    _require_empty_and_extensible(minimizing, "minimizing")
    _require_empty_and_extensible(maximizing, "maximizing")

    failures: list[str] = []
    _check_master_solver(minimizing, "minimizing model", tolerance, failures)
    _check_master_solver(maximizing, "maximizing model", tolerance, failures)
    return MasterSolverSelfTestReport(tuple(failures))
