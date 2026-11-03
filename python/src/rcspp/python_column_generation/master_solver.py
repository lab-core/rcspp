#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The adapter to the LP/MIP solver that holds the master."""

from abc import ABC, abstractmethod

from .types import (
    Capabilities,
    ColumnBatch,
    FloatArray,
    IdArray,
    IntegerParams,
    OptionValue,
    RelaxationParams,
    RowBatch,
    SolverStatus,
)


class MasterSolver(ABC):
    """Adapter to the LP/MIP solver holding the master, seen as a minimization with
    normalized duals (≥ row: dual ≥ 0, ≤ row: dual ≤ 0).

    Duals follow one convention: ``π_i = ∂objective/∂rhs_i``, so a ``≥`` row has ``π ≥ 0``, a
    ``≤`` row ``π ≤ 0`` and an ``=`` row a free sign. A master solver wrapping a maximizing
    model negates its costs and duals.

    Rows are numbered ``0 … number_of_rows() − 1``. Columns are named by the id the master
    assigns; the master solver maps ids to its own indices. The loop calls these methods a few
    times per iteration, never inside pricing.

    Subclass it to use another solver: implement :meth:`number_of_rows`, :meth:`row_bounds`
    (returns ``(lower, upper)``), :meth:`add_columns`, :meth:`solve_relaxation`,
    :meth:`objective`, :meth:`duals` (returns one value per row), :meth:`primal` (returns one
    value per id), :meth:`solve_integer`, :meth:`set_option` and :meth:`capabilities`;
    :meth:`add_rows`, :meth:`farkas` (returns a ray or None), :meth:`dual_tolerance`,
    :meth:`largest_user_cost` and :meth:`restore_user_model` are optional (:meth:`add_rows`
    raises ``NotImplementedError`` unless overridden: the solver cannot add rows).
    """

    @abstractmethod
    def number_of_rows(self) -> int:
        """The number of rows of the master."""

    @abstractmethod
    def row_bounds(self) -> tuple[FloatArray, FloatArray]:
        """The bounds ``lower ≤ a·x ≤ upper`` of every row: ``(lower, upper)``."""

    def add_rows(self, rows: RowBatch) -> None:
        """Appends rows. Optional: only master solvers that set
        :attr:`Capabilities.add_rows` override it; a master solver wrapping a user's
        model, whose rows exist, does not.

        New rows get the next row numbers. A master solver may refuse rows its solver cannot
        hold, e.g. ranges: the batch then adds nothing.

        Raises:
            NotImplementedError: Unless overridden.
            ValueError: For a row the solver cannot hold.
        """
        raise NotImplementedError("MasterSolver.add_rows: this master solver cannot add rows")

    @abstractmethod
    def add_columns(self, columns: ColumnBatch) -> None:
        """Appends columns. A batch the solver cannot hold is refused as a whole and
        adds nothing.

        Raises:
            ValueError: For an id already used, or a number the solver refuses.
            IndexError: For a column entering a row that does not exist.
        """

    @abstractmethod
    def solve_relaxation(self, params: RelaxationParams = RelaxationParams()) -> SolverStatus:
        """Solves the LP relaxation; integer variables are relaxed in place.

        Args:
            params: The time limit, threads and log options.

        Returns:
            The solver's status.
        """

    @abstractmethod
    def objective(self) -> float:
        """The objective value of the last solve (minimization).

        This and the other accessors of a solution read the last solve, which must have
        returned ``SolverStatus.Optimal``; after another status a master solver may raise.
        """

    @abstractmethod
    def duals(self) -> FloatArray:
        """The duals of the last LP solve, normalized as described above: one per
        row."""

    @abstractmethod
    def primal(self, ids: IdArray) -> FloatArray:
        """The values of the given columns in the last solve: one per id."""

    @abstractmethod
    def solve_integer(self, params: IntegerParams = IntegerParams()) -> SolverStatus:
        """Restores integrality, makes the generated columns of ``params.column_type``,
        solves.

        Returns:
            The solver's status.
        """

    def farkas(self) -> FloatArray | None:
        """The Farkas ray of an infeasible LP, when :attr:`Capabilities.farkas` is set;
        ``None`` when the master solver gives no ray (the default)."""
        return None

    @abstractmethod
    def set_option(self, name: str, value: OptionValue) -> None:
        """Sets a solver-specific option by name.

        Raises:
            ValueError: When the solver refuses the name or the value.
        """

    @abstractmethod
    def capabilities(self) -> Capabilities:
        """What this master solver supports."""

    def dual_tolerance(self) -> float:
        """The LP solver's own tolerance on reduced costs.

        E.g. Gurobi's ``OptimalityTol``: a column whose reduced cost is above minus this is
        priced out for the solver. The loop never asks pricers for less. 0 by default.
        """
        return 0.0

    def largest_user_cost(self) -> float:
        """The largest absolute cost among the variables the master did not add: the
        user's own, e.g. starting columns built in the model. 0 when there are none (the
        default).

        With the pricers' :meth:`Pricer.cost_scale`, it sets the scale of the automatic
        artificial cost and of relative tolerances.
        """
        return 0.0

    def restore_user_model(self) -> None:
        """Puts back what the LP solves of a run changed in the user's model (relaxed
        variable types, solver options…). ``ColumnGeneration.run`` calls it when the run
        ends, however it ends.

        During a run the model is in the master solver's hands: what it holds between
        two solves must not be relied on. Optional: a master solver that changes nothing
        keeps this default, which does nothing.
        """
