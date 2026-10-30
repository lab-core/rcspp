#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The master solver over a highspy model: a copy of the model, solved in C++."""

from typing import TYPE_CHECKING, Any, Sequence

import numpy as np

from ..._core.column_generation import ColumnType, HighsMasterSolver, MasterProblem

if TYPE_CHECKING:
    import highspy


class HighspyMasterSolver(HighsMasterSolver):
    """A master solver over a copy of the user's ``highspy.Highs`` model.

    The model (costs, bounds, matrix, integrality, sense and offset) is copied when the
    master solver is built, and a run solves the copy in C++, without Python. Changes made
    to the model afterwards are not seen, and its options (time limit, threads…) are not
    copied: :meth:`set_option` sets those of the copy. :meth:`write_back` adds the
    generated columns to the model.

    Args:
        model: The user's master: its rows, and any columns of their own (slacks,
            starting columns). Semi-continuous and semi-integer columns are refused.
        rows: The indices of its rows that pricing columns enter, in master row order;
            every row by default.
    """

    def __init__(self, model: "highspy.Highs", rows: Sequence[int] | None = None) -> None:
        try:
            import highspy
        except ImportError as error:
            raise ImportError(
                "HighspyMasterSolver needs highspy: pip install rcspp[highspy]"
            ) from error

        lp = model.getLp()
        integrality = list(lp.integrality_)
        if any(
            kind in (highspy.HighsVarType.kSemiContinuous, highspy.HighsVarType.kSemiInteger)
            for kind in integrality
        ):
            raise ValueError(
                "HighspyMasterSolver: semi-continuous and semi-integer columns are not supported"
            )

        starts, row_indices, values = _column_wise(lp.a_matrix_, lp.num_row_, lp.num_col_)
        super().__init__(
            column_cost=lp.col_cost_,
            column_lower=lp.col_lower_,
            column_upper=lp.col_upper_,
            integer=[kind == highspy.HighsVarType.kInteger for kind in integrality],
            row_lower=lp.row_lower_,
            row_upper=lp.row_upper_,
            starts=starts,
            row_indices=row_indices,
            values=values,
            rows=list(rows) if rows is not None else None,
            maximize=lp.sense_ == highspy.ObjSense.kMaximize,
            offset=lp.offset_,
        )

        self._model = model
        # The rows of the model, which the copy's first rows match.
        self._model_row_count = lp.num_row_
        # The first column id not written back yet.
        self._next_id = 0

    @property
    def model(self) -> "highspy.Highs":
        """The user's model."""
        return self._model

    def write_back(
        self, master: MasterProblem, column_type: ColumnType | None = None
    ) -> dict[int, int]:
        """Adds to the user's model what the runs added to the copy since the last call:
        the rows the master created (e.g. convexity rows), then the generated columns.

        Artificial columns are left out: they only make the first LP solvable. The columns
        come with their cost, bounds and entries as the copy holds them; their values are
        those the master solver gives (:meth:`primal`).

        Args:
            master: The master problem over this master solver.
            column_type: The type of the written columns: continuous by default, so the
                model is the LP the run solved; ``ColumnType.Integer`` or
                ``ColumnType.Binary`` (integer, with an upper bound of at most 1) to solve the
                integer problem in the model.

        Returns:
            The column of the user's model of each written column, by column id.

        Raises:
            ValueError: If ``master`` runs on another master solver, or if rows were added
                to or removed from the model since it was copied.
        """
        import highspy

        if master.solver() is not self:
            raise ValueError("HighspyMasterSolver.write_back: master runs on another solver")
        if self._model.getNumRow() != self._model_row_count:
            raise ValueError(
                "HighspyMasterSolver.write_back: rows were added to or removed from the model "
                "since it was copied"
            )

        # An integer solve may have tightened the generated columns' bounds in the copy.
        self.restore_user_model()

        lower, upper, names = self.model_rows(self._model_row_count)
        if len(names) > 0:
            self._model.addRows(len(names), lower, upper, 0, [], [], [])
            for offset, name in enumerate(names):
                if name:
                    self._model.passRowName(self._model_row_count + offset, name)
            self._model_row_count += len(names)

        artificial = set(master.artificial_column_ids().tolist())
        ids = [
            column_id
            for column_id in range(self._next_id, master.number_of_columns())
            if column_id not in artificial
        ]
        self._next_id = master.number_of_columns()
        if not ids:
            return {}

        cost, lower, upper, starts, model_rows, values = self.model_columns(ids)
        if column_type == ColumnType.Binary:
            upper = np.minimum(upper, 1.0)
        first = self._model.getNumCol()
        self._model.addCols(
            len(ids),
            cost,
            lower,
            upper,
            len(values),
            starts[:-1].astype(np.int32),
            model_rows.astype(np.int32),
            values,
        )

        columns = np.arange(first, first + len(ids), dtype=np.int32)
        if column_type is not None:
            self._model.changeColsIntegrality(
                len(ids), columns, np.full(len(ids), highspy.HighsVarType.kInteger)
            )
        return dict(zip(ids, columns.tolist()))


def _column_wise(matrix: Any, row_count: int, column_count: int) -> tuple[Any, Any, Any]:
    """The entries of a HiGHS matrix in compressed sparse column form: (starts,
    row_indices, values)."""
    import highspy

    starts = np.asarray(matrix.start_, dtype=np.int64)
    indices = np.asarray(matrix.index_, dtype=np.int64)
    values = np.asarray(matrix.value_, dtype=np.float64)
    if matrix.format_ == highspy.MatrixFormat.kColwise:
        entries = starts[column_count]
        return starts[: column_count + 1], indices[:entries], values[:entries]

    # Row-wise: sorting the entries by column, stably, keeps each column's rows in order.
    entries = starts[row_count]
    rows = np.repeat(np.arange(row_count, dtype=np.int64), np.diff(starts[: row_count + 1]))
    columns = indices[:entries]
    order = np.argsort(columns, kind="stable")
    column_starts = np.zeros(column_count + 1, dtype=np.int64)
    np.cumsum(np.bincount(columns, minlength=column_count), out=column_starts[1:])
    return column_starts, rows[order], values[:entries][order]
