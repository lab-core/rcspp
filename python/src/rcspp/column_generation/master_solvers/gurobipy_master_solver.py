#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The master solver over a gurobipy model, called by the loop from Python."""

import math
from contextlib import contextmanager
from typing import Any, Iterator, Sequence, TypeAlias

import gurobipy as gp
import numpy as np
from gurobipy import GRB

from ..._core.column_generation import (
    Capabilities,
    ColumnBatch,
    ColumnType,
    IntegerParams,
    MasterSolver,
    RelaxationParams,
    RowBatch,
    SolverStatus,
)

# What a method receives or returns: duals, values, bounds; column ids.
FloatArray: TypeAlias = np.ndarray[Any, np.dtype[np.float64]]
IdArray: TypeAlias = np.ndarray[Any, np.dtype[np.uint64]]

_STATUSES = {
    GRB.OPTIMAL: SolverStatus.Optimal,
    GRB.INFEASIBLE: SolverStatus.Infeasible,
    GRB.UNBOUNDED: SolverStatus.Unbounded,
    GRB.TIME_LIMIT: SolverStatus.TimeLimit,
}


# Gurobi writes infinity as ±GRB.INFINITY (1e100).
def _to_gurobi(value: float) -> float:
    return min(max(value, -GRB.INFINITY), GRB.INFINITY)


def _from_gurobi(value: float) -> float:
    if value >= GRB.INFINITY:
        return math.inf
    if value <= -GRB.INFINITY:
        return -math.inf
    return value


def _bounds_can_be_held(lower: float, upper: float) -> bool:
    """Whether a row or a column can have these bounds: numbers, with lower < +inf,
    upper > −inf and lower ≤ upper."""
    return (
        not math.isnan(lower)
        and not math.isnan(upper)
        and lower != math.inf
        and upper != -math.inf
        and lower <= upper
    )


def _gurobi_row(lower: float, upper: float) -> tuple[str, float]:
    """The Gurobi row (sense, side) for lower ≤ a·x ≤ upper: ``=`` for equal finite
    bounds, ``≥`` or ``≤`` for one finite bound, ``≤ ∞`` for a free row."""
    if not _bounds_can_be_held(lower, upper):
        raise ValueError(
            "GurobipyMasterSolver.add_rows: row bounds must be numbers, with lower < +inf and "
            "upper > -inf"
        )

    finite_lower = math.isfinite(lower)
    finite_upper = math.isfinite(upper)
    if finite_lower and finite_upper:
        if lower != upper:
            raise ValueError(
                "GurobipyMasterSolver.add_rows: a row with two different finite bounds is a "
                "range; build it in the model instead"
            )
        return GRB.EQUAL, lower
    if finite_lower:
        return GRB.GREATER_EQUAL, lower
    if finite_upper:
        return GRB.LESS_EQUAL, upper
    return GRB.LESS_EQUAL, GRB.INFINITY


@contextmanager
def _temporary_parameters(model: gp.Model, **values: Any) -> Iterator[None]:
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
    """A master solver over the user's ``gurobipy.Model``, a Python subclass the loop
    calls with the GIL held (the solves themselves release it).

    The user builds the master in Gurobi (its rows, and any variables of their own, such as
    slacks or starting columns). The master solver adds the generated columns to that model
    and reads its duals: when the run ends, the model holds the generated columns,
    continuous. During a run the model is the master solver's: what it holds between two
    solves must not be relied on. When the run ends, the user's variable types come back
    (:meth:`restore_user_model`). Available when gurobipy is installed.

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
        rows: Its constraints that columns enter: ``=``, ``≥`` or ``≤`` linear
            constraints (not ranges), in master row order; every linear constraint by
            default.
    """

    def __init__(self, model: gp.Model, rows: Sequence[gp.Constr] | None = None) -> None:
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
            IndexError: For an id this master solver did not receive.
        """
        try:
            return self._columns[column_id]
        except KeyError:
            raise IndexError(f"GurobipyMasterSolver: no column has the id {column_id}") from None

    # ── The rows ──────────────────────────────────────────────────────────────

    def number_of_rows(self) -> int:
        return len(self._rows)

    def row_bounds(self) -> tuple[FloatArray, FloatArray]:
        senses = self._model.getAttr(GRB.Attr.Sense, self._rows) if self._rows else []
        sides = self._model.getAttr(GRB.Attr.RHS, self._rows) if self._rows else []
        lower = [
            -math.inf if sense == GRB.LESS_EQUAL else _from_gurobi(side)
            for sense, side in zip(senses, sides)
        ]
        upper = [
            math.inf if sense == GRB.GREATER_EQUAL else _from_gurobi(side)
            for sense, side in zip(senses, sides)
        ]
        return np.array(lower, dtype=np.float64), np.array(upper, dtype=np.float64)

    def add_rows(self, rows: RowBatch) -> None:
        """Appends ``=``, ``≥``, ``≤`` or free rows to the model.

        Raises:
            ValueError: For a row with two different finite bounds (a range: build it in
                the model instead), or bounds no Gurobi row holds. A refused batch adds no
                row.
        """
        # Convert every row first, so that a refused batch adds nothing.
        converted = [
            _gurobi_row(float(lower), float(upper)) for lower, upper in zip(rows.lower, rows.upper)
        ]
        names = rows.names if rows.names else [""] * len(converted)

        for (sense, side), name in zip(converted, names):
            self._rows.append(self._model.addLConstr(gp.LinExpr(), sense, side, name))

        self._model.update()

    # ── The columns ───────────────────────────────────────────────────────────

    def add_columns(self, columns: ColumnBatch) -> None:
        ids = [int(column_id) for column_id in columns.ids]
        self._check(columns, ids)

        cost_sign = self._sense_sign()
        starts = columns.starts.tolist()
        row_indices = columns.row_indices.tolist()
        values = columns.values.tolist()
        for column, column_id in enumerate(ids):
            begin, end = starts[column], starts[column + 1]
            entries = gp.Column(
                values[begin:end], [self._rows[row] for row in row_indices[begin:end]]
            )
            self._columns[column_id] = self._model.addVar(
                lb=_to_gurobi(float(columns.lower[column])),
                ub=_to_gurobi(float(columns.upper[column])),
                obj=cost_sign * float(columns.cost[column]),
                vtype=GRB.CONTINUOUS,
                column=entries,
            )

        self._model.update()

    def _check(self, columns: ColumnBatch, ids: list[int]) -> None:
        """Checks a batch before anything of it is added, so that a refused batch adds
        nothing: the checks every master solver makes."""
        if len(set(ids)) != len(ids) or any(column_id in self._columns for column_id in ids):
            raise ValueError("add_columns: a column id is already used")
        if any(row >= len(self._rows) for row in columns.row_indices.tolist()):
            raise IndexError("add_columns: a column enters a row that does not exist")

        # A NaN or an infinite cost would poison the LP without an error.
        if not np.isfinite(columns.cost).all() or not np.isfinite(columns.values).all():
            raise ValueError("add_columns: a column has a cost or a coefficient that is not finite")
        if not all(
            _bounds_can_be_held(float(lower), float(upper))
            for lower, upper in zip(columns.lower, columns.upper)
        ):
            raise ValueError("add_columns: a column has bounds that no column can hold")

    # ── The solves ────────────────────────────────────────────────────────────

    def solve_relaxation(self, params: RelaxationParams = RelaxationParams()) -> SolverStatus:
        self._relax()
        with self._solve_options(params.timeout_s, params.threads, params.verbose):
            # After columns are added the last basis stays primal feasible, so primal simplex
            # resumes from it; a method the user chose is kept.
            if self._model.Params.Method == GRB.METHOD_AUTO:
                with _temporary_parameters(self._model, Method=GRB.METHOD_PRIMAL):
                    return self._optimize()
            return self._optimize()

    def solve_integer(self, params: IntegerParams = IntegerParams()) -> SolverStatus:
        self._restore(GRB.BINARY if params.column_type == ColumnType.Binary else GRB.INTEGER)
        with self._solve_options(params.timeout_s, params.threads, params.verbose):
            with _temporary_parameters(self._model, MIPGap=params.relative_gap):
                return self._optimize()

    def objective(self) -> float:
        objective: float = self._model.ObjVal
        return self._sense_sign() * objective

    def duals(self) -> FloatArray:
        if not self._rows:
            return np.zeros(0)
        duals = np.array(self._model.getAttr(GRB.Attr.Pi, self._rows), dtype=np.float64)
        return self._sense_sign() * duals

    def primal(self, ids: IdArray) -> FloatArray:
        variables = [self.variable(int(column_id)) for column_id in ids]
        if not variables:
            return np.zeros(0)
        return np.array(self._model.getAttr(GRB.Attr.X, variables), dtype=np.float64)

    # ── The rest of the interface ─────────────────────────────────────────────

    def set_option(self, name: str, value: bool | int | float | str) -> None:
        """Sets the Gurobi parameter ``name`` (case-insensitive, as in Gurobi); a text
        value is read as the parameter's own type.

        Raises:
            ValueError: When Gurobi refuses the name or the value.
        """
        try:
            kind = self._model.getParamInfo(name)[1]
            if isinstance(value, bool) or (isinstance(value, str) and kind is not str):
                value = kind(value)
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
        # Variables added just before are pending until an update.
        self._model.update()
        generated = {variable.index for variable in self._columns.values()}
        variables = [
            variable for variable in self._model.getVars() if variable.index not in generated
        ]
        if not variables:
            return 0.0
        return float(np.abs(self._model.getAttr(GRB.Attr.Obj, variables)).max())

    def restore_user_model(self) -> None:
        """Puts back the types of the user's variables, after an LP solve relaxed them;
        the generated columns stay continuous.

        Gurobi applies the types at the model's next update (or optimize): until then,
        the last solve's values can still be read.
        """
        if self._relaxed:
            self._restore(GRB.CONTINUOUS)

    # ── Inside ────────────────────────────────────────────────────────────────

    def _sense_sign(self) -> float:
        """+1 for a minimizing model, −1 for a maximizing one: turns its values into
        ours."""
        return -1.0 if self._model.ModelSense == GRB.MAXIMIZE else 1.0

    @contextmanager
    def _solve_options(self, timeout_s: float, threads: int, verbose: bool) -> Iterator[None]:
        """The options every solve sets from its parameters, for the duration of the
        solve: the user's values come back after it."""
        with _temporary_parameters(
            self._model,
            TimeLimit=_to_gurobi(max(0.0, timeout_s)),
            Threads=threads,
            OutputFlag=1 if verbose else 0,
        ):
            yield

    def _relax(self) -> None:
        """Makes every variable continuous."""
        known = len(self._user_types)
        self._record_new_user_types()

        # Relaxed already: only the user's variables added since are not.
        variables = [] if self._relaxed else list(self._columns.values())
        binaries = []
        for variable, kind in self._user_types[known if self._relaxed else 0 :]:
            variables.append(variable)
            if kind == GRB.BINARY:
                binaries.append(variable)
        self._relaxed = True
        if not variables:
            return

        self._model.setAttr(GRB.Attr.VType, variables, [GRB.CONTINUOUS] * len(variables))
        self._bound_binaries(binaries)

    def _bound_binaries(self, binaries: list[gp.Var]) -> None:
        """Bounds the user's binaries within 0 and 1 while they are continuous, keeping
        their own bounds to put back."""
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
        self._model.optimize()
        status = self._model.Status

        if status == GRB.INF_OR_UNBD:
            # The second solve gets what is left of the time limit, not a limit of its own.
            time_left = max(0.0, self._model.Params.TimeLimit - self._model.Runtime)
            with _temporary_parameters(self._model, TimeLimit=time_left, DualReductions=0):
                self._model.optimize()
            status = self._model.Status
        return _STATUSES.get(status, SolverStatus.Other)
