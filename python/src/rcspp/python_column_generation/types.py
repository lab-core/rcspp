#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The vocabulary of a master solver: statuses, options, capabilities and the batches of rows
and columns it receives."""

import math
from dataclasses import dataclass
from enum import Enum, auto
from typing import Any, Sequence, TypeAlias

import numpy as np

# What a method receives or returns: duals, values, bounds; column ids.
FloatArray: TypeAlias = np.ndarray[Any, np.dtype[np.float64]]
IdArray: TypeAlias = np.ndarray[Any, np.dtype[np.uint64]]
IndexArray: TypeAlias = np.ndarray[Any, np.dtype[np.uint32]]
# What an argument accepts: anything numpy turns into an array of numbers.
ArrayLike: TypeAlias = Sequence[float] | np.ndarray[Any, Any]
IntArrayLike: TypeAlias = Sequence[int] | np.ndarray[Any, Any]
# The value of a solver-specific option.
OptionValue: TypeAlias = bool | int | float | str

#: Default relative optimality gap of the final integer solve.
DEFAULT_RELATIVE_GAP = 1e-4


class SolverStatus(Enum):
    """Outcome of an LP or MIP solve by a MasterSolver."""

    Optimal = auto()
    Infeasible = auto()
    Unbounded = auto()
    TimeLimit = auto()
    Other = auto()


class RowSense(Enum):
    """Sense of a master row: a·x = rhs, a·x ≥ rhs or a·x ≤ rhs."""

    Equal = auto()
    GreaterEqual = auto()
    LessEqual = auto()


class ColumnType(Enum):
    """Type of the generated columns in the integer solve."""

    Integer = auto()
    Binary = auto()


@dataclass(frozen=True, kw_only=True)
class Capabilities:
    """What a master solver can do beyond the required."""

    add_rows: bool = False
    integer_solve: bool = False
    farkas: bool = False
    basis: bool = False


@dataclass(frozen=True, kw_only=True)
class RelaxationParams:
    """Options of one LP solve."""

    timeout_s: float = math.inf
    #: Number of threads; 0: the solver's.
    threads: int = 0
    verbose: bool = False


@dataclass(frozen=True, kw_only=True)
class IntegerParams:
    """Options of the integer solve of the master."""

    timeout_s: float = math.inf
    relative_gap: float = DEFAULT_RELATIVE_GAP
    column_type: ColumnType = ColumnType.Integer
    threads: int = 0
    verbose: bool = False


def _array(values: Any, dtype: Any, what: str) -> Any:
    """``values`` as a read-only one-dimensional numpy array of ``dtype``; ``what``
    names it in the error.

    A batch is a record: its arrays cannot be written through. The result is a view, so
    the caller's own array, which it may share memory with, stays writable.
    """
    array = np.asarray(values, dtype=dtype)
    if array.ndim != 1:
        raise ValueError(f"{what}: expected a one-dimensional array")
    view = np.ascontiguousarray(array).view()
    view.flags.writeable = False
    return view


def _name(name: object) -> str:
    """``name``, checked: a row name is a string."""
    if not isinstance(name, str):
        raise TypeError("RowBatch.names: expected strings")
    return name


class RowBatch:
    """Rows to add, as ranges lower ≤ a·x ≤ upper (numpy arrays).

    Args:
        lower: The lower bound of each row.
        upper: The upper bound of each row; as many as ``lower``.
        names: The names of the rows: none, or one per row.

    Raises:
        ValueError: If ``lower``, ``upper`` and ``names`` (when given) differ in size.
        TypeError: If ``names`` is a string, or holds anything but strings.
    """

    __slots__ = ("_lower", "_upper", "_names")

    def __init__(self, lower: ArrayLike, upper: ArrayLike, names: Sequence[str] = ()) -> None:
        self._lower: FloatArray = _array(lower, np.float64, "RowBatch.lower")
        self._upper: FloatArray = _array(upper, np.float64, "RowBatch.upper")
        # A string is a sequence of its characters, which are not names.
        if isinstance(names, str):  # pyright: ignore[reportUnnecessaryIsInstance]
            raise TypeError("RowBatch.names: expected a sequence of names, not a string")
        self._names = tuple(_name(name) for name in names)
        if len(self._upper) != len(self._lower) or (
            self._names and len(self._names) != len(self._lower)
        ):
            raise ValueError(
                "RowBatch: lower, upper and names (when given) must have the same size"
            )

    @property
    def lower(self) -> FloatArray:
        return self._lower

    @property
    def upper(self) -> FloatArray:
        return self._upper

    @property
    def names(self) -> tuple[str, ...]:
        return self._names

    def __len__(self) -> int:
        return len(self._lower)

    def __repr__(self) -> str:
        return f"RowBatch({len(self)} rows{', named' if self._names else ''})"


class ColumnBatch:
    """Columns to add, in compressed sparse column form (numpy arrays): column j has the
    entries ``row_indices[starts[j]:starts[j + 1]]`` with the matching ``values``.

    Args:
        ids: The id of each column, assigned by the master.
        cost: The objective coefficient of each column.
        lower: The lower bound of each column.
        upper: The upper bound of each column.
        starts: The start of each column's entries, plus one end marker (one more than the
            columns).
        row_indices: The row of each entry.
        values: The coefficient of each entry.

    Raises:
        ValueError: If the arrays differ in size as above, or ``starts`` does not rise from 0
            to the number of entries.
    """

    __slots__ = ("_ids", "_cost", "_lower", "_upper", "_starts", "_row_indices", "_values")

    def __init__(
        self,
        ids: IntArrayLike,
        cost: ArrayLike,
        lower: ArrayLike,
        upper: ArrayLike,
        starts: IntArrayLike,
        row_indices: IntArrayLike,
        values: ArrayLike,
    ) -> None:
        self._ids: IdArray = _array(ids, np.uint64, "ColumnBatch.ids")
        self._cost: FloatArray = _array(cost, np.float64, "ColumnBatch.cost")
        self._lower: FloatArray = _array(lower, np.float64, "ColumnBatch.lower")
        self._upper: FloatArray = _array(upper, np.float64, "ColumnBatch.upper")
        self._starts: IndexArray = _array(starts, np.uint32, "ColumnBatch.starts")
        self._row_indices: IndexArray = _array(row_indices, np.uint32, "ColumnBatch.row_indices")
        self._values: FloatArray = _array(values, np.float64, "ColumnBatch.values")

        size = len(self._ids)
        if (
            len(self._cost) != size
            or len(self._lower) != size
            or len(self._upper) != size
            or len(self._starts) != size + 1
            or len(self._values) != len(self._row_indices)
        ):
            raise ValueError(
                "ColumnBatch: cost, lower and upper need one value per id, starts one more, "
                "and values one per row index"
            )
        if (
            self._starts[0] != 0
            or self._starts[-1] != len(self._row_indices)
            or (np.diff(self._starts.astype(np.int64)) < 0).any()
        ):
            raise ValueError("ColumnBatch: starts must rise from 0 to the number of entries")

    @property
    def ids(self) -> IdArray:
        return self._ids

    @property
    def cost(self) -> FloatArray:
        return self._cost

    @property
    def lower(self) -> FloatArray:
        return self._lower

    @property
    def upper(self) -> FloatArray:
        return self._upper

    @property
    def starts(self) -> IndexArray:
        return self._starts

    @property
    def row_indices(self) -> IndexArray:
        return self._row_indices

    @property
    def values(self) -> FloatArray:
        return self._values

    def __len__(self) -> int:
        return len(self._ids)

    def __repr__(self) -> str:
        return f"ColumnBatch({len(self)} columns, {len(self._row_indices)} entries)"
