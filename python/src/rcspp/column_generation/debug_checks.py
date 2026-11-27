#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Checks that a run's master solver and pricers agree with each other, for debugging a
master solver or a pricer: each is off by default, and a failed one raises
:class:`DebugCheckError`."""

from collections.abc import Sequence
from dataclasses import dataclass, field
from typing import Any, cast

import numpy as np

from .._core.graph import Solution
from .master_problem import MasterProblem
from .types import FloatArray

IntArray = np.ndarray[Any, np.dtype[np.int64]]


@dataclass(frozen=True, kw_only=True)
class DebugChecks:
    """Which checks a run makes. Each is off by default; a failed check raises
    :class:`DebugCheckError`.

    They all use the run's reduced-cost tolerance (``tol`` below): the larger of
    ``Tolerances.reduced_cost`` and ``MasterSolver.dual_tolerance``.
    """

    #: Each column a pricer returns: its reduced cost (``Solution.cost``) is its column's cost
    #: minus the duals of its rows, the convexity row's left out, within ``tol`` times
    #: ``max(1, |cost| + sum |dual * coefficient|)``. Costs the new columns' entries.
    pricer_reduced_costs: bool = False
    #: The duals of each LP solve against their rows' bounds: at most ``tol`` on a ``<=`` row,
    #: at least ``-tol`` on a ``>=`` row, within ``tol`` of 0 on a row with no bound. A row
    #: with two different finite bounds is not checked: its sign depends on the bound it
    #: reaches. Costs a pass over the rows.
    dual_signs: bool = False
    #: The reduced cost of every column the master holds, from its cost, its entries and the
    #: duals, after each LP solve: at least ``-tol`` (scaled as for ``pricer_reduced_costs``),
    #: and within ``tol`` of 0 for a column the LP solution uses. Variables of the user's own
    #: model are not seen. Costs every entry of the master and a read of every column's value.
    master_reduced_costs: bool = False
    #: The LP value equals the cost of its columns' values, within ``tol`` times
    #: ``max(1, sum |cost * value|)``. Checked only when the user's own variables cost
    #: nothing (``MasterSolver.largest_user_cost`` is 0); an objective constant in the
    #: user's model fails it. Costs a read of every column's value.
    objective: bool = False


class DebugCheckError(RuntimeError):
    """A debug check failed (:class:`DebugChecks`): the master solver or a pricer gave
    something the others contradict.

    Args:
        check: The check's name, a field of :class:`DebugChecks`.
        iteration: The iteration it failed at.
        message: What failed.
    """

    def __init__(self, check: str, iteration: int, message: str) -> None:
        super().__init__(f"debug check {check} failed at iteration {iteration}: {message}")
        self.check = check
        self.iteration = iteration
        self.message = message


# MARK: The checks


def check_pricer_reduced_costs(
    columns: Sequence[Solution], duals: FloatArray, tolerance: float, iteration: int, pricer: int
) -> None:
    """Checks the reduced cost each of a pricer's columns reports.

    A column whose row does not exist is left to ``MasterProblem.add_columns``, which
    refuses it.
    """
    failures: list[str] = []

    for position, solution in enumerate(columns):
        cost, rows, values = _column_arrays(solution)

        if rows.size > 0 and (int(rows.min()) < 0 or int(rows.max()) >= duals.size):
            continue

        terms = values * duals[rows]
        recomputed = cost - float(terms.sum())
        scale = max(1.0, abs(cost) + float(np.abs(terms).sum()))

        if not abs(solution.cost - recomputed) <= tolerance * scale:
            failures.append(
                f"pricer {pricer}'s column {position} reports the reduced cost "
                f"{float(solution.cost)!r}, its cost and rows give {recomputed!r}"
            )

    _raise_if_any("pricer_reduced_costs", iteration, failures, tolerance)


def check_dual_signs(
    duals: FloatArray, lower: FloatArray, upper: FloatArray, tolerance: float, iteration: int
) -> None:
    """Checks the sign of each row's dual against the row's bounds (minimization, ``dual
    = d objective / d bound``)."""
    no_lower = lower == -np.inf
    no_upper = upper == np.inf
    wrong = (
        (no_lower & ~no_upper & (duals > tolerance))
        | (~no_lower & no_upper & (duals < -tolerance))
        | (no_lower & no_upper & (np.abs(duals) > tolerance))
    )
    failures = [
        f"row {row} ({_bounds(float(lower[row]), float(upper[row]))}) has the dual "
        f"{float(duals[row])!r}"
        for row in np.flatnonzero(wrong).tolist()
    ]
    _raise_if_any("dual_signs", iteration, failures, tolerance)


def check_master_reduced_costs(
    kept: "KeptColumns",
    duals: FloatArray,
    values: FloatArray,
    tolerance: float,
    feasibility: float,
    iteration: int,
) -> None:
    """Checks that no column of the master has a negative reduced cost, and that the
    columns the LP solution uses have none but 0."""
    count = kept.costs.size
    terms = kept.values * duals[kept.rows]
    reduced_costs = kept.costs - np.bincount(kept.owners, weights=terms, minlength=count)
    margins = tolerance * np.maximum(
        1.0, np.abs(kept.costs) + np.bincount(kept.owners, weights=np.abs(terms), minlength=count)
    )
    negative = reduced_costs < -margins
    used = (values > feasibility) & (np.abs(reduced_costs) > margins)
    failures = [
        f"column {column} has the reduced cost {float(reduced_costs[column])!r}"
        for column in np.flatnonzero(negative).tolist()
    ]
    failures += [
        f"column {column}, of value {float(values[column])!r}, has the reduced cost "
        f"{float(reduced_costs[column])!r}"
        for column in np.flatnonzero(used & ~negative).tolist()
    ]
    _raise_if_any("master_reduced_costs", iteration, failures, tolerance)


def check_objective(
    kept: "KeptColumns", values: FloatArray, objective: float, tolerance: float, iteration: int
) -> None:
    """Checks the LP value against the cost of its columns' values."""
    costs = kept.costs * values
    recomputed = float(costs.sum())
    scale = max(1.0, float(np.abs(costs).sum()))
    failures: list[str] = []

    if not abs(objective - recomputed) <= tolerance * scale:
        failures.append(
            f"the LP value is {float(objective)!r}, its columns' values cost {recomputed!r}"
        )

    _raise_if_any("objective", iteration, failures, tolerance)


def _raise_if_any(check: str, iteration: int, failures: list[str], tolerance: float) -> None:
    """Raises for the first failure, telling how many more there are."""
    if not failures:
        return

    more = f" ({len(failures) - 1} more)" if len(failures) > 1 else ""
    raise DebugCheckError(check, iteration, f"{failures[0]}{more}; tolerance {tolerance:g}")


def _bounds(lower: float, upper: float) -> str:
    """A row's bounds, for a message."""
    if lower == -np.inf and upper == np.inf:
        return "no bound"

    if lower == -np.inf:
        return f"<= {upper!r}"

    if upper == np.inf:
        return f">= {lower!r}"

    return f"{lower!r} ... {upper!r}"


# MARK: The master's columns


@dataclass(kw_only=True)
class KeptColumns:
    """The master's columns as arrays, for the checks over the whole master: one cost
    per column, and per entry its row, its value and its column (the column's id)."""

    costs: FloatArray = field(default_factory=lambda: np.empty(0))
    rows: IntArray = field(default_factory=lambda: np.empty(0, dtype=np.int64))
    values: FloatArray = field(default_factory=lambda: np.empty(0))
    owners: IntArray = field(default_factory=lambda: np.empty(0, dtype=np.int64))


def keep_new_columns(kept: KeptColumns, master: MasterProblem) -> None:
    """Appends the columns the master gained since the last call: each column's own
    entries, then a 1 in its pricer's convexity row (as ``MasterProblem.add_columns``
    sends it)."""
    first = kept.costs.size
    new = master.columns[first:]

    if not new:
        return

    convexity_rows = [entry.convexity_row for entry in master.pricers]
    costs = np.empty(len(new))
    row_parts: list[IntArray] = [kept.rows]
    value_parts: list[FloatArray] = [kept.values]
    owner_parts: list[IntArray] = [kept.owners]

    for position, column in enumerate(new):
        cost, rows, values = _column_arrays(column.solution)
        convexity = None if column.pricer is None else convexity_rows[column.pricer]

        if convexity is not None:
            rows = np.append(rows, convexity)
            values = np.append(values, 1.0)

        costs[position] = cost
        row_parts.append(rows.astype(np.int64))
        value_parts.append(values.astype(np.float64))
        owner_parts.append(np.full(rows.size, first + position, dtype=np.int64))

    kept.costs = np.concatenate([kept.costs, costs])
    kept.rows = np.concatenate(row_parts)
    kept.values = np.concatenate(value_parts)
    kept.owners = np.concatenate(owner_parts)


def _column_arrays(solution: Solution) -> tuple[float, IntArray, FloatArray]:
    """A solution's column as arrays: its cost, its rows and their coefficients (the
    binding returns a tuple the generated stub leaves untyped)."""
    arrays: tuple[float, IntArray, FloatArray] = cast(Any, solution.column).to_arrays()
    return arrays
