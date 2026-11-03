#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The restricted master problem: rows, pricers and the columns generated so far."""

import math
import operator
import warnings
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Any, Iterable, Sequence

import numpy as np

from .._core.graph import Solution
from .master_solver import MasterSolver
from .pricer import Pricer
from .types import (
    ArrayLike,
    ColumnBatch,
    FloatArray,
    IntArrayLike,
    RowBatch,
    RowSense,
)


class ColumnTag(Enum):
    """Where a master column comes from."""

    #: A column found by a pricer, or a starting column given for one.
    Real = auto()
    #: A placeholder that makes the first LP feasible (:class:`MasterInitialization`).
    Artificial = auto()


@dataclass(frozen=True)
class RowRange:
    """A block of consecutive rows: ``first … first + count − 1``."""

    #: Number of the first row.
    first: int
    #: Number of rows.
    count: int


@dataclass(frozen=True, eq=False)
class MasterColumn:
    """A column of the master and what the master knows about it."""

    #: Its id in the master solver.
    id: int
    #: Index of the pricer it belongs to; ``None`` for an artificial column.
    pricer: int | None
    tag: ColumnTag
    #: The pricer's solution: for a graph pricer, the path and its column; empty for an
    #: artificial column.
    solution: Solution = field(default_factory=Solution)


@dataclass(frozen=True, kw_only=True)
class ConvexityRow:
    """The convexity row of a pricer: a row of the user's model (``existing_row``), or one the
    master creates, ``Σ x ≤ max_paths``. Set at most one; neither: no convexity row.

    Raises:
        ValueError: When both are set.
    """

    #: A row the user already built, e.g. in a wrapped model.
    existing_row: int | None = None
    #: The master creates the row ``Σ x_p ≤ max_paths`` (needs ``Capabilities.add_rows``).
    max_paths: float | None = None

    def __post_init__(self) -> None:
        if self.existing_row is not None and self.max_paths is not None:
            raise ValueError("ConvexityRow: set either existing_row or max_paths, not both")


@dataclass(frozen=True, eq=False)
class PricerEntry:
    """A pricer of the master and its convexity row."""

    pricer: Pricer
    #: The pricer's convexity row; ``None`` when it has none.
    convexity_row: int | None


class MasterProblem:
    """The restricted master problem: rows, pricers and the columns generated so far.

    Columns get consecutive ids from 0; columns are never removed for now, so the id of a
    column is also its position in :attr:`columns`.

    Args:
        solver: The LP solver holding the master. Its existing rows are the master's rows. It
            may hold variables of its own (a user's slacks or starting columns): the master
            ignores them.
    """

    def __init__(self, solver: MasterSolver) -> None:
        self._solver = solver
        self._pricers: list[PricerEntry] = []
        self._columns: list[MasterColumn] = []
        self._artificial_column_ids: list[int] = []
        self._artificial_cost: float | None = None
        self._warned_repeated_rows = False
        self._initialized = False

    # ── The rows ──────────────────────────────────────────────────────────────

    def add_rows(self, count: int, sense: RowSense, right_hand_side: float) -> RowRange:
        """Appends ``count`` rows with the same sense and right-hand side.

        Returns:
            The rows created.

        Raises:
            NotImplementedError: When the master solver cannot add rows.
            TypeError: When ``count`` is not an integer, or ``sense`` not a :class:`RowSense`.
            ValueError: When ``count`` is negative.
        """
        # Any other value, e.g. the C++-backed layer's RowSense, would make equality rows.
        if not isinstance(sense, RowSense):  # pyright: ignore[reportUnnecessaryIsInstance]
            raise TypeError("MasterProblem.add_rows: sense must be a RowSense")
        try:
            count = operator.index(count)
        except TypeError:
            raise TypeError("MasterProblem.add_rows: count must be an integer") from None
        if count < 0:
            raise ValueError("MasterProblem.add_rows: count must not be negative")
        lower = -math.inf if sense == RowSense.LessEqual else right_hand_side
        upper = math.inf if sense == RowSense.GreaterEqual else right_hand_side
        return self.add_rows_between(np.full(count, lower), np.full(count, upper))

    def add_rows_between(
        self, lower: ArrayLike, upper: ArrayLike, names: Sequence[str] = ()
    ) -> RowRange:
        """Appends rows ``lower ≤ a·x ≤ upper``.

        Args:
            lower: Lower bound of each row.
            upper: Upper bound of each row.
            names: Optional names, one per row, for error messages and debugging.

        Returns:
            The rows created.

        Raises:
            NotImplementedError: When the master solver cannot add rows.
            ValueError: When the sizes differ.
        """
        if not self._solver.capabilities().add_rows:
            raise NotImplementedError(
                "MasterProblem: this master solver cannot add rows; build them in the "
                "wrapped model instead"
            )

        lower, upper = np.asarray(lower, dtype=np.float64), np.asarray(upper, dtype=np.float64)
        if lower.shape != upper.shape or (len(names) > 0 and len(names) != len(lower)):
            raise ValueError(
                "MasterProblem.add_rows_between: lower, upper and names must have the same size"
            )

        rows = RowBatch(lower, upper, names)
        first = self._solver.number_of_rows()
        self._solver.add_rows(rows)
        return RowRange(first, len(rows))

    # ── The pricers ───────────────────────────────────────────────────────────

    def add_pricer(self, pricer: Pricer, convexity: ConvexityRow = ConvexityRow()) -> int:
        """Registers a pricer and its convexity row.

        Args:
            pricer: The pricer.
            convexity: Its convexity row, if any.

        Returns:
            The pricer's index ``k``.

        Raises:
            IndexError: When ``ConvexityRow.existing_row`` is not a row.
        """
        row = convexity.existing_row
        if convexity.max_paths is not None:
            row = self.add_rows(1, RowSense.LessEqual, convexity.max_paths).first
        elif row is not None and not 0 <= row < self.number_of_rows:
            raise IndexError("MasterProblem.add_pricer: existing_row is not a row")

        self._pricers.append(PricerEntry(pricer, row))
        return len(self._pricers) - 1

    # ── The columns ───────────────────────────────────────────────────────────

    def add_columns(self, pricer: int, columns: Iterable[Solution]) -> None:
        """Adds columns of pricer ``pricer``: starting columns or generated ones.

        Each column takes the cost and rows of ``Solution.column``, plus a 1 in the pricer's
        convexity row. A row listed more than once in a column gets the sum of its entries (with
        one warning per master: it is usually a mistake). The master solver receives the columns
        as one batch.

        Args:
            pricer: Index of the pricer the columns belong to.
            columns: The pricer's solutions.

        Raises:
            IndexError: When ``pricer`` or a row index does not exist.
            ValueError: When a column already has an entry in the convexity row.
        """
        if not 0 <= pricer < len(self._pricers):
            raise IndexError("MasterProblem.add_columns: no such pricer")
        columns = list(columns)
        if len(columns) == 0:
            return

        convexity = self._pricers[pricer].convexity_row
        count = len(columns)
        costs = np.empty(count)
        lengths = np.empty(count, dtype=np.int64)
        row_parts: list[Any] = []
        value_parts: list[Any] = []
        for position, solution in enumerate(columns):
            costs[position], rows, values = solution.column.to_arrays()
            lengths[position] = len(rows)
            row_parts.append(rows)
            value_parts.append(values)
        row_indices = np.concatenate(row_parts)
        values = np.concatenate(value_parts)

        if row_indices.size > 0 and np.max(row_indices) >= self._solver.number_of_rows():
            raise IndexError(
                "MasterProblem.add_columns: a column has a row index that is not a row of the "
                "master"
            )
        if convexity is not None and (row_indices == convexity).any():
            raise ValueError(
                "MasterProblem.add_columns: a column already has an entry in its pricer's "
                "convexity row"
            )

        row_indices, values, lengths = self._sorted_without_repeats(row_indices, values, lengths)
        if convexity is not None:
            row_indices, values, lengths = _with_entry_at_end(
                row_indices, values, lengths, convexity
            )

        self._send(costs, row_indices, values, lengths)
        for solution in columns:
            self._columns.append(MasterColumn(len(self._columns), pricer, ColumnTag.Real, solution))

    def add_artificial_columns(
        self, rows: IntArrayLike, coefficients: ArrayLike, cost: float
    ) -> None:
        """Adds artificial columns: one per row, each with a single entry.

        Args:
            rows: The row of each column.
            coefficients: Its coefficient in that row.
            cost: The cost of every column.

        Raises:
            TypeError: When the rows are not integers.
            ValueError: When the sizes differ.
            IndexError: When a row does not exist.
        """
        row_indices = np.asarray(rows)
        if row_indices.size > 0 and (
            row_indices.dtype == np.bool_ or not np.issubdtype(row_indices.dtype, np.integer)
        ):
            raise TypeError("MasterProblem.add_artificial_columns: rows must be integers")
        row_indices = row_indices.astype(np.int64)
        values = np.asarray(coefficients, dtype=np.float64)
        if row_indices.shape != values.shape or row_indices.ndim != 1:
            raise ValueError(
                "MasterProblem.add_artificial_columns: rows and coefficients must have the same "
                "size"
            )
        if len(row_indices) == 0:
            return
        if np.min(row_indices) < 0 or np.max(row_indices) >= self._solver.number_of_rows():
            raise IndexError("MasterProblem.add_artificial_columns: not a row of the master")

        count = len(row_indices)
        self._send(np.full(count, cost), row_indices, values, np.ones(count, dtype=np.int64))
        self._artificial_column_ids.extend(range(len(self._columns), len(self._columns) + count))
        self._artificial_cost = cost
        for _ in range(count):
            self._columns.append(MasterColumn(len(self._columns), None, ColumnTag.Artificial))

    # ── What the master holds ─────────────────────────────────────────────────

    @property
    def number_of_rows(self) -> int:
        """Number of rows."""
        return self._solver.number_of_rows()

    @property
    def number_of_columns(self) -> int:
        """Number of columns, artificial ones included."""
        return len(self._columns)

    @property
    def pricers(self) -> tuple[PricerEntry, ...]:
        """Every pricer and its convexity row, in the order they were added: the
        position of a pricer is the index :meth:`add_pricer` returned."""
        return tuple(self._pricers)

    @property
    def columns(self) -> list[MasterColumn]:
        """Every column, in id order (a copy)."""
        return list(self._columns)

    @property
    def artificial_column_ids(self) -> np.ndarray[Any, np.dtype[np.uint64]]:
        """Ids of the artificial columns."""
        return np.array(self._artificial_column_ids, dtype=np.uint64)

    @property
    def artificial_cost(self) -> float | None:
        """Cost of the artificial columns; ``None`` when there are none."""
        return self._artificial_cost

    @property
    def initialized(self) -> bool:
        """Whether a loop has initialized this master (e.g. added artificial
        columns)."""
        return self._initialized

    def mark_initialized(self) -> None:
        """Records that the master is initialized, so no other loop does it again."""
        self._initialized = True

    @property
    def solver(self) -> MasterSolver:
        """The master solver."""
        return self._solver

    # ── Inside ────────────────────────────────────────────────────────────────

    def _sorted_without_repeats(
        self, row_indices: Any, values: FloatArray, lengths: Any
    ) -> tuple[Any, FloatArray, Any]:
        """The entries of the columns with each column's rows in order, a row listed
        twice in a column given the sum of its entries.

        A graph pricer gives each row once, in order, which is the one case checked
        cheaply here; the other columns are sorted, and an LP solver would read a
        repeated row the same way.
        """
        owner = np.repeat(np.arange(len(lengths)), lengths)
        in_order = (owner[1:] != owner[:-1]) | (row_indices[1:] > row_indices[:-1])
        if in_order.all():
            return row_indices, values, lengths

        order = np.lexsort((row_indices, owner))
        row_indices, values, owner = row_indices[order], values[order], owner[order]
        first_of_row = np.ones(len(row_indices), dtype=bool)
        first_of_row[1:] = (owner[1:] != owner[:-1]) | (row_indices[1:] != row_indices[:-1])
        if not first_of_row.all():
            if not self._warned_repeated_rows:
                self._warned_repeated_rows = True
                warnings.warn(
                    "MasterProblem.add_columns: a column lists a row more than once; its "
                    "entries are added together",
                    RuntimeWarning,
                    stacklevel=3,
                )
            values = np.add.reduceat(values, np.flatnonzero(first_of_row))
            row_indices, owner = row_indices[first_of_row], owner[first_of_row]
        return row_indices, values, np.bincount(owner, minlength=len(lengths))

    def _send(self, costs: FloatArray, row_indices: Any, values: FloatArray, lengths: Any) -> None:
        """Sends columns of cost ``costs`` and ``lengths`` entries each, bounded by 0
        and infinity, in one master solver call; they get the next ids."""
        count = len(costs)
        first = len(self._columns)
        starts = np.zeros(count + 1, dtype=np.uint32)
        starts[1:] = np.cumsum(lengths)
        self._solver.add_columns(
            ColumnBatch(
                ids=np.arange(first, first + count, dtype=np.uint64),
                cost=costs,
                lower=np.zeros(count),
                upper=np.full(count, math.inf),
                starts=starts,
                row_indices=row_indices.astype(np.uint32),
                values=values,
            )
        )


def _with_entry_at_end(
    row_indices: Any, values: FloatArray, lengths: Any, row: int
) -> tuple[Any, FloatArray, Any]:
    """The entries of the columns with a 1 in ``row`` after each column's own."""
    lengths = lengths + 1
    ends = np.cumsum(lengths)
    added = np.zeros(ends[-1], dtype=bool)
    added[ends - 1] = True

    all_rows = np.empty(ends[-1], dtype=row_indices.dtype)
    all_rows[added] = row
    all_rows[~added] = row_indices
    all_values = np.ones(ends[-1])
    all_values[~added] = values
    return all_rows, all_values, lengths
