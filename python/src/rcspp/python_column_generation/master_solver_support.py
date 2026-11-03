#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""What every master solver checks whatever its solver: the bounds of the rows and the columns
it receives. Written once, so that master solvers cannot drift apart."""

from typing import Any, Container, TypeAlias

import numpy as np

from .types import ColumnBatch, FloatArray

BoolArray: TypeAlias = np.ndarray[Any, np.dtype[np.bool_]]


def bounds_can_be_held(lower: FloatArray, upper: FloatArray) -> BoolArray:
    """Whether each row or column ``lower ≤ a·x ≤ upper`` can have its bounds.

    Bounds can be held when they are numbers, with ``lower < +∞``, ``upper > −∞`` and
    ``lower ≤ upper``.
    """
    held: BoolArray = (
        ~np.isnan(lower) & ~np.isnan(upper) & (lower != np.inf) & (upper != -np.inf)
    ) & (lower <= upper)
    return held


def check_columns(batch: ColumnBatch, rows: int, known_ids: Container[int]) -> None:
    """Checks a batch before anything of it is added, so that a refused batch adds
    nothing.

    Args:
        batch: The columns to add.
        rows: The number of rows of the master solver.
        known_ids: The ids of the columns the master solver holds.

    Raises:
        ValueError: When an id is already used, or used twice in the batch, or when a cost or a
            coefficient is not finite, or the bounds are not ones a column can hold (the rule
            of :func:`bounds_can_be_held`).
        IndexError: When a column enters a row that does not exist.
    """
    ids = batch.ids.tolist()
    if len(set(ids)) != len(ids) or any(column_id in known_ids for column_id in ids):
        raise ValueError("add_columns: a column id is already used")
    if len(batch.row_indices) > 0 and int(np.max(batch.row_indices)) >= rows:
        raise IndexError("add_columns: a column enters a row that does not exist")

    # A NaN or an infinite cost would poison the LP without an error.
    if not np.isfinite(batch.cost).all() or not np.isfinite(batch.values).all():
        raise ValueError("add_columns: a column has a cost or a coefficient that is not finite")
    if not bounds_can_be_held(batch.lower, batch.upper).all():
        raise ValueError("add_columns: a column has bounds that no column can hold")
