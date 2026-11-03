#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The master solver over the user's gurobipy model, called by the loop from Python."""

from __future__ import annotations

import math
from collections.abc import Generator, Sequence
from contextlib import contextmanager
from typing import TYPE_CHECKING, Any

import numpy as np

from ..master_solver import MasterSolver
from ..master_solver_support import bounds_can_be_held, check_columns
from ..types import (
    Capabilities,
    ColumnBatch,
    ColumnType,
    FloatArray,
    IdArray,
    IntegerParams,
    OptionValue,
    RelaxationParams,
    RowBatch,
    SolverStatus,
)

# gurobipy is imported where it is used, so that importing rcspp does not load it.
if TYPE_CHECKING:
    import gurobipy as gp


def _status(gurobi_status: int) -> SolverStatus:
    """Our status for a Gurobi status."""
    from gurobipy import GRB

    statuses = {
        GRB.OPTIMAL: SolverStatus.Optimal,
        GRB.INFEASIBLE: SolverStatus.Infeasible,
        GRB.UNBOUNDED: SolverStatus.Unbounded,
        GRB.TIME_LIMIT: SolverStatus.TimeLimit,
    }
    return statuses.get(gurobi_status, SolverStatus.Other)


@contextmanager
def _temporary_parameters(model: gp.Model, **values: Any) -> Generator[None, None, None]:
    """Sets parameters for the duration of the block, then puts the old values back."""
    old_values = {name: model.getParamInfo(name)[2] for name in values}

    try:
        for name, value in values.items():
            model.setParam(name, value)

        yield
    finally:
        for name, value in old_values.items():
            model.setParam(name, value)


class GurobipyMasterSolver(MasterSolver):
    """A master solver over the user's ``gurobipy.Model``.

    The user builds the master in Gurobi (its rows, and any variables of their own, such as
    slacks or starting columns). The master solver adds the generated columns to that model
    and reads its duals: when the run ends, the model holds the generated columns,
    continuous, or of the integer solve's type when that solve came last. During a run the
    model is the master solver's: what it holds between two solves must not be relied on.
    When the run ends, the user's variable types come back (:meth:`restore_user_model`).
    Needs gurobipy (``pip install rcspp[gurobipy]``).

    - **Sense:** a maximizing model works too: the master solver negates the costs it adds,
      the objective and the duals, so that the master is seen as a minimization.
    - **Integrality:** every LP solve relaxes the model's integer variables in place, those
      the user adds during the run included, a binary kept within 0 and 1 by bounds whatever
      its own; the integer solve restores them and gives the generated columns the requested
      type. The model must become an LP once relaxed (no SOS or general constraints). The
      types restored are those the variables had when first seen, and variables must not be
      deleted during the run.
    - **Solver options:** every solve sets ``TimeLimit``, ``Threads`` and ``OutputFlag``
      (and ``MIPGap`` for the integer solve) from its parameters, and puts the user's values
      back after it. LP solves use primal simplex while ``Method`` is automatic.
    - **Errors:** Gurobi's own errors are raised as ``gurobipy.GurobiError``, except a
      refused option (``ValueError``, as for every master solver).

    Args:
        model: The user's master.
        rows: Its constraints that columns enter: ``=``, ``>=`` or ``<=`` linear
            constraints (not ranges), in master row order; every linear constraint by
            default.
    """

    def __init__(self, model: gp.Model, rows: Sequence[gp.Constr] | None = None) -> None:
        try:
            import gurobipy  # noqa: F401  # pyright: ignore[reportUnusedImport]
        except ImportError as error:
            raise ImportError(
                "GurobipyMasterSolver needs gurobipy: pip install rcspp[gurobipy]"
            ) from error

        super().__init__()

        # Rows created just before are pending until an update.
        model.update()
        self._model = model
        self._rows: list[gp.Constr] = list(rows) if rows is not None else model.getConstrs()

        # The Gurobi variable of each column id, in the order they came.
        self._columns: dict[int, gp.Var] = {}

        # The user's non-continuous variables and their types, and how many of the model's
        # variables have been looked at.
        self._user_types: list[tuple[gp.Var, str]] = []
        self._recorded_variables = 0

        # Every variable is continuous (an LP solve came last).
        self._relaxed = False

        # The user's binaries made continuous, with their own bounds.
        self._relaxed_binaries: list[tuple[gp.Var, float, float]] = []

    @property
    def model(self) -> gp.Model:
        """The user's model."""
        return self._model

    def variable(self, column_id: int) -> gp.Var:
        """The Gurobi variable of column ``column_id``.

        Raises:
            KeyError: For an id this master solver did not receive.
        """
        try:
            return self._columns[column_id]
        except KeyError:
            raise KeyError(f"GurobipyMasterSolver: no column has the id {column_id}") from None

    # MARK: The rows

    def number_of_rows(self) -> int:
        return len(self._rows)

    def row_bounds(self) -> tuple[FloatArray, FloatArray]:
        from gurobipy import GRB

        if not self._rows:
            return np.zeros(0), np.zeros(0)

        # gurobipy reads Gurobi's infinity as an infinity.
        senses = np.array(self._model.getAttr(GRB.Attr.Sense, self._rows))
        sides = np.array(self._model.getAttr(GRB.Attr.RHS, self._rows), dtype=np.float64)
        lower = np.where(senses == GRB.LESS_EQUAL, -math.inf, sides)
        upper = np.where(senses == GRB.GREATER_EQUAL, math.inf, sides)
        return lower, upper

    def add_rows(self, rows: RowBatch) -> None:
        """Appends ``=``, ``>=``, ``<=`` or free rows to the model.

        Raises:
            ValueError: For a row with two different finite bounds (a range: build it in
                the model instead), or bounds no Gurobi row holds. A refused batch adds no
                row.
        """
        import gurobipy as gp
        from gurobipy import GRB

        # Check every row first, so that a refused batch adds nothing.
        if not bounds_can_be_held(rows.lower, rows.upper).all():
            raise ValueError(
                "GurobipyMasterSolver.add_rows: row bounds must be numbers, with lower <= upper, "
                "lower < +inf and upper > -inf"
            )

        ranges = np.isfinite(rows.lower) & np.isfinite(rows.upper) & (rows.lower != rows.upper)

        if ranges.any():
            raise ValueError(
                "GurobipyMasterSolver.add_rows: a row with two different finite bounds is a "
                "range; build it in the model instead"
            )

        names = rows.names or [""] * len(rows)

        for lower, upper, name in zip(rows.lower.tolist(), rows.upper.tolist(), names):
            if lower == upper:
                sense, side = GRB.EQUAL, lower
            elif math.isfinite(lower):
                sense, side = GRB.GREATER_EQUAL, lower
            else:
                # A free row is a <= row with an infinite side.
                sense, side = GRB.LESS_EQUAL, upper

            self._rows.append(self._model.addLConstr(gp.LinExpr(), sense, side, name))

        self._model.update()

    # MARK: The columns

    def add_columns(self, columns: ColumnBatch) -> None:
        import gurobipy as gp
        from gurobipy import GRB

        check_columns(columns, len(self._rows), self._columns)

        # Gurobi has no batch call for columns: one variable each.
        costs = (self._sense_sign() * columns.cost).tolist()
        lowers = columns.lower.tolist()
        uppers = columns.upper.tolist()
        starts = columns.starts.tolist()
        row_indices: list[int] = columns.row_indices.tolist()
        values = columns.values.tolist()

        for position, column_id in enumerate(columns.ids.tolist()):
            begin, end = starts[position], starts[position + 1]
            entries = gp.Column(
                values[begin:end], [self._rows[row] for row in row_indices[begin:end]]
            )
            self._columns[column_id] = self._model.addVar(
                lb=lowers[position],
                ub=uppers[position],
                obj=costs[position],
                vtype=GRB.CONTINUOUS,
                column=entries,
            )

        self._model.update()

    # MARK: The solves

    def solve_relaxation(self, params: RelaxationParams = RelaxationParams()) -> SolverStatus:
        from gurobipy import GRB

        self._relax()

        with self._solve_options(params.timeout_s, params.threads, params.verbose):
            # After columns are added the last basis stays primal feasible, so primal simplex
            # resumes from it; a method the user chose is kept.
            if self._model.Params.Method == GRB.METHOD_AUTO:
                with _temporary_parameters(self._model, Method=GRB.METHOD_PRIMAL):
                    return self._optimize()

            return self._optimize()

    def solve_integer(self, params: IntegerParams = IntegerParams()) -> SolverStatus:
        from gurobipy import GRB

        self._restore(GRB.BINARY if params.column_type == ColumnType.Binary else GRB.INTEGER)

        with (
            self._solve_options(params.timeout_s, params.threads, params.verbose),
            _temporary_parameters(self._model, MIPGap=params.relative_gap),
        ):
            return self._optimize()

    def objective(self) -> float:
        objective: float = self._model.ObjVal
        return self._sense_sign() * objective

    def duals(self) -> FloatArray:
        from gurobipy import GRB

        if not self._rows:
            return np.zeros(0)

        duals = np.array(self._model.getAttr(GRB.Attr.Pi, self._rows), dtype=np.float64)
        return self._sense_sign() * duals

    def primal(self, ids: IdArray) -> FloatArray:
        from gurobipy import GRB

        variables = [self.variable(int(column_id)) for column_id in ids]

        if not variables:
            return np.zeros(0)

        return np.array(self._model.getAttr(GRB.Attr.X, variables), dtype=np.float64)

    # MARK: The rest of the interface

    def set_option(self, name: str, value: OptionValue) -> None:
        """Sets the Gurobi parameter ``name`` (case-insensitive, as in Gurobi); a text
        value is read as the parameter's own type.

        Raises:
            ValueError: When Gurobi refuses the name or the value.
        """
        import gurobipy as gp

        try:
            # gurobipy takes a bool for a number, but not a text; nor anything but a text for a
            # text parameter.
            kind = self._model.getParamInfo(name)[1]

            if isinstance(value, str) and kind is not str:
                value = kind(value)
            elif kind is str and not isinstance(value, str):
                raise TypeError("a text parameter takes a text")

            self._model.setParam(name, value)
        except (gp.GurobiError, TypeError, ValueError) as error:
            raise ValueError(
                f"GurobipyMasterSolver.set_option: Gurobi refused the parameter '{name}' or its "
                f"value: {error}"
            ) from None

    def capabilities(self) -> Capabilities:
        return Capabilities(add_rows=True, integer_solve=True)

    def dual_tolerance(self) -> float:
        """Gurobi's ``OptimalityTol``, read from the model."""
        tolerance: float = self._model.Params.OptimalityTol
        return tolerance

    def largest_user_cost(self) -> float:
        """The largest absolute objective coefficient of the model's variables that this
        master solver did not add."""
        from gurobipy import GRB

        # Variables added just before are pending until an update.
        self._model.update()
        costs = np.abs(
            np.array(self._model.getAttr(GRB.Attr.Obj, self._model.getVars()), dtype=np.float64)
        )
        costs[[variable.index for variable in self._columns.values()]] = 0.0
        return float(np.max(costs)) if len(costs) > 0 else 0.0

    def restore_user_model(self) -> None:
        """Puts back the types of the user's variables, after an LP solve relaxed them;
        the generated columns stay continuous, or of the integer solve's type when that
        solve came last.

        Gurobi applies the types at the model's next update (or optimize): until then,
        the last solve's values can still be read.
        """
        from gurobipy import GRB

        if self._relaxed:
            self._restore(GRB.CONTINUOUS)

    # MARK: Inside

    def _sense_sign(self) -> float:
        """+1 for a minimizing model, -1 for a maximizing one: turns its values into
        ours."""
        from gurobipy import GRB

        return -1.0 if self._model.ModelSense == GRB.MAXIMIZE else 1.0

    @contextmanager
    def _solve_options(
        self, timeout_s: float, threads: int, verbose: bool
    ) -> Generator[None, None, None]:
        """The options every solve sets from its parameters, for the duration of the
        solve: the user's values come back after it."""
        with _temporary_parameters(
            self._model,
            TimeLimit=max(0.0, timeout_s),
            Threads=threads,
            OutputFlag=verbose,
        ):
            yield

    def _relax(self) -> None:
        """Makes every variable continuous: the generated columns, and the user's
        variables that are not (recorded with their types, to put back)."""
        from gurobipy import GRB

        recorded_before = len(self._user_types)
        self._record_new_user_types()

        # Not relaxed yet: the model holds the types of the user's variables, and the
        # generated columns hold those of the last integer solve. Relaxed already: everything
        # is continuous but the variables the user added since, recorded just now.
        if self._relaxed:
            to_relax: list[gp.Var] = []
            user_types = self._user_types[recorded_before:]
        else:
            to_relax = list(self._columns.values())
            user_types = self._user_types

        binaries: list[gp.Var] = []

        for variable, kind in user_types:
            to_relax.append(variable)

            if kind == GRB.BINARY:
                binaries.append(variable)

        self._relaxed = True

        if not to_relax:
            return

        self._model.setAttr(GRB.Attr.VType, to_relax, [GRB.CONTINUOUS] * len(to_relax))
        self._bound_binaries(binaries)

    def _bound_binaries(self, binaries: list[gp.Var]) -> None:
        """Bounds the user's binaries within 0 and 1 while they are continuous, keeping
        their own bounds to put back."""
        from gurobipy import GRB

        if not binaries:
            return

        # Gurobi keeps a binary within 0 and 1 by its type alone, whatever bounds the user set
        # after creating it: once continuous, it needs them as bounds.
        lower = self._model.getAttr(GRB.Attr.LB, binaries)
        upper = self._model.getAttr(GRB.Attr.UB, binaries)
        self._relaxed_binaries.extend(zip(binaries, lower, upper))
        self._model.setAttr(GRB.Attr.LB, binaries, [max(value, 0.0) for value in lower])
        self._model.setAttr(GRB.Attr.UB, binaries, [min(value, 1.0) for value in upper])

    def _restore(self, kind: str) -> None:
        """Gives back the user's types, and ``kind`` to the generated columns."""
        from gurobipy import GRB

        self._record_new_user_types()

        variables = list(self._columns.values())
        kinds = [kind] * len(variables)

        for variable, user_kind in self._user_types:
            variables.append(variable)
            kinds.append(user_kind)

        if variables:
            self._model.setAttr(GRB.Attr.VType, variables, kinds)

        # The user's binaries get their own bounds back.
        for variable, lower, upper in self._relaxed_binaries:
            variable.LB = lower
            variable.UB = upper

        self._relaxed_binaries.clear()
        self._relaxed = False

    def _record_new_user_types(self) -> None:
        """Records the user's non-continuous variables added since the last call; called
        before changing any type."""
        from gurobipy import GRB

        # Variables the user added are pending until an update.
        self._model.update()
        variables = self._model.getVars()[self._recorded_variables :]

        if not variables:
            return

        # A generated column is continuous whenever this runs: added so, and made integer only
        # by the integer solve, after this. Every other type belongs to a variable of the user.
        kinds = self._model.getAttr(GRB.Attr.VType, variables)
        self._user_types.extend(
            (variable, kind) for variable, kind in zip(variables, kinds) if kind != GRB.CONTINUOUS
        )
        self._recorded_variables += len(variables)

    def _optimize(self) -> SolverStatus:
        """Optimizes; an "infeasible or unbounded" answer is solved again without dual
        reductions, to tell the two apart, within what is left of the time limit."""
        from gurobipy import GRB

        self._model.optimize()
        status = self._model.Status

        if status == GRB.INF_OR_UNBD:
            # The second solve gets what is left of the time limit, not a limit of its own.
            time_left = max(0.0, self._model.Params.TimeLimit - self._model.Runtime)

            with _temporary_parameters(self._model, TimeLimit=time_left, DualReductions=0):
                self._model.optimize()

            status = self._model.Status

        return _status(status)
