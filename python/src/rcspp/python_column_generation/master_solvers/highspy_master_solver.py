#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The master solver over the user's highspy model, called by the loop from Python."""

from __future__ import annotations

import math
import signal
import threading
import time
import warnings
from collections.abc import Callable, Generator, Sequence
from contextlib import contextmanager
from types import FrameType
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

# highspy is imported where it is used, so that importing rcspp does not load it.
if TYPE_CHECKING:
    import highspy


def _status(highs_status: highspy.HighsModelStatus) -> SolverStatus:
    """Our status for a HiGHS model status."""
    import highspy

    statuses = {
        highspy.HighsModelStatus.kOptimal: SolverStatus.Optimal,
        highspy.HighsModelStatus.kInfeasible: SolverStatus.Infeasible,
        highspy.HighsModelStatus.kUnbounded: SolverStatus.Unbounded,
        highspy.HighsModelStatus.kTimeLimit: SolverStatus.TimeLimit,
    }
    return statuses.get(highs_status, SolverStatus.Other)


def _option(model: highspy.Highs, name: str) -> Any:
    """The value of the HiGHS option ``name``."""
    return model.getOptionValue(name)[1]


@contextmanager
def _mip_deadline(
    model: highspy.Highs, timeout_s: float, ctrl_c: Callable[[], bool]
) -> Generator[Callable[[], bool], None, None]:
    """Interrupts the MIP solves of the block once ``timeout_s`` has passed, or once
    ``ctrl_c()`` is true, and yields whether the time was up.

    HiGHS's MIP solver compares the time limit with a clock of its own, and overruns a
    limit set from the clock that counts all the model's solves: a handler of HiGHS's
    interrupt callback keeps the limit instead.
    """
    if math.isinf(timeout_s):
        yield lambda: False
        return

    deadline = time.perf_counter() + max(0.0, timeout_s)
    interrupted = False

    # The callback's input, which holds the interrupt request, once the handler made one.
    requested: Any = None

    def interrupt(event: Any) -> None:
        nonlocal interrupted, requested

        # The handler only ever makes the request: a false one would take back a request of
        # another handler of the user's.
        interrupted = interrupted or time.perf_counter() >= deadline

        if interrupted or ctrl_c():
            event.interrupt()
            requested = event.data_in

    model.cbMipInterrupt.subscribe(interrupt)

    try:
        yield lambda: interrupted
    finally:
        model.cbMipInterrupt.unsubscribe(interrupt)

        # HiGHS keeps the request after the solve, and would stop the model's next solves at
        # their first callback.
        if requested is not None:
            requested.user_interrupt = False


@contextmanager
def _ctrl_c_held_back() -> Generator[Callable[[], bool], None, None]:
    """Holds Ctrl-C back during the block, and yields whether it came; the program's own
    Ctrl-C handler gets it once the block has ended.

    Python raises Ctrl-C in the next Python code that runs, which during a HiGHS solve is a
    callback handler, ours or the user's: an exception that leaves a handler breaks the model
    for good (every later run fails). Only the main thread receives Ctrl-C and can change its
    handler: elsewhere, or when the program ignores Ctrl-C, the block runs as is.
    """
    previous = signal.getsignal(signal.SIGINT)

    if threading.current_thread() is not threading.main_thread() or not callable(previous):
        yield lambda: False
        return

    frames: list[FrameType | None] = []

    def hold(signal_number: int, frame: FrameType | None) -> None:
        frames.append(frame)

    signal.signal(signal.SIGINT, hold)

    try:
        yield lambda: bool(frames)
    finally:
        signal.signal(signal.SIGINT, previous)

    if frames:
        previous(signal.SIGINT, frames[0])


@contextmanager
def _temporary_options(model: highspy.Highs, **values: Any) -> Generator[None, None, None]:
    """Sets options for the duration of the block, then puts the old values back."""
    old_values = {name: _option(model, name) for name in values}

    try:
        for name, value in values.items():
            model.setOptionValue(name, value)

        yield
    finally:
        for name, value in old_values.items():
            model.setOptionValue(name, value)


class HighspyMasterSolver(MasterSolver):
    """A master solver over the user's ``highspy.Highs`` model.

    The user builds the master in HiGHS (its rows, and any columns of their own, such as
    slacks or starting columns), or passes an empty model. The master solver adds the
    generated columns to that model and reads its duals: when the run ends, the model holds
    the generated columns. During a run the model is the master solver's: what it holds
    between two solves must not be relied on. When the run ends, what the solves changed
    comes back (:meth:`restore_user_model`). Rows must not be deleted from the model during
    the run: that would shift the indices. Needs highspy (``pip install rcspp[highspy]``).

    - **Sense:** a maximizing model works too: the master solver negates the costs it adds,
      the objective and the duals, so that the master is seen as a minimization.
    - **Integrality:** LP solves set HiGHS's ``solve_relaxation`` option, so the model's
      integer columns stay integer and are solved as continuous; the integer solve unsets it
      and gives the generated columns the requested type (a binary column is an integer one
      with an upper bound of at most 1 and at most its own, until the next LP solve or the end
      of the run, which put its own back). Semi-continuous and semi-integer columns are
      refused.
    - **Solver options:** every solve sets ``time_limit``, ``threads``, ``output_flag``,
      ``solve_relaxation`` (and ``mip_rel_gap`` for the integer solve) from its parameters, and
      puts the user's values back after it. HiGHS counts its time limit over all the model's
      solves, and highspy cannot zero that clock: a solve sets the limit to the time already
      run plus its own. HiGHS's MIP solver overruns that limit, so the integer solve also
      interrupts itself from HiGHS's MIP interrupt callback once its time is up (a model whose
      callbacks were disabled stops later, at HiGHS's own limit). HiGHS fixes its thread
      count at its first solve in the process and refuses a run asking for another: the
      master solver then runs with the count in force and warns. The simplex method is left
      to HiGHS, which takes primal simplex after added columns.
    - **Status:** "infeasible or unbounded" is solved again once without presolve to tell the
      two apart. An LP solve that ends with an error or an unknown status, or that HiGHS
      refused to run, is solved again once from scratch on a cleared solver, within what is
      left of its time limit (:attr:`retried_solves` counts them): a warm-started dual simplex
      can fail on large costs where a solve from scratch does not.
    - **Ctrl-C:** HiGHS does not stop on Ctrl-C. The master solver holds it back during a
      solve, so that it never reaches a callback handler (an exception that leaves one breaks
      the model for good), and passes it to the program's handler (``KeyboardInterrupt`` by
      default) once the solve has returned. An
      integer solve with a time limit stops at once, from the same callback as the limit;
      other solves run to their end. Only in the main thread, which is where Python delivers
      Ctrl-C.
    - **Errors:** an option HiGHS refuses raises ``ValueError``.

    Args:
        model: The user's master.
        rows: The indices of its rows that pricing columns enter, in master row order; every
            row by default.

    Raises:
        ValueError: For a semi-continuous or semi-integer column.
        IndexError: For a row index that is not a row of the model.
    """

    def __init__(self, model: highspy.Highs, rows: Sequence[int] | None = None) -> None:
        try:
            import highspy
        except ImportError as error:
            raise ImportError(
                "HighspyMasterSolver needs highspy: pip install rcspp[highspy]"
            ) from error

        super().__init__()
        lp = model.getLp()

        if any(
            kind in (highspy.HighsVarType.kSemiContinuous, highspy.HighsVarType.kSemiInteger)
            for kind in lp.integrality_
        ):
            raise ValueError(
                "HighspyMasterSolver: semi-continuous and semi-integer columns are not supported"
            )

        row_list = list(rows) if rows is not None else list(range(lp.num_row_))

        if any(row < 0 or row >= lp.num_row_ for row in row_list):
            raise IndexError("HighspyMasterSolver: a row index is not a row of the model")

        self._model = model

        # The model row of each master row.
        self._rows: np.ndarray[Any, np.dtype[np.int32]] = np.array(row_list, dtype=np.int32)

        # The HiGHS column of each column id, in the order they were added.
        self._columns: dict[int, int] = {}

        # The generated columns made binary by the last integer solve, and their upper bounds
        # to put back.
        self._binary_columns: np.ndarray[Any, np.dtype[np.int32]] | None = None
        self._upper_bounds: FloatArray = np.zeros(0)

        # The thread count the current solve asks for, and the last one warned about.
        self._requested_threads = 0
        self._warned_threads = 0
        self._retried_solves = 0

    @property
    def model(self) -> highspy.Highs:
        """The user's model."""
        return self._model

    @property
    def retried_solves(self) -> int:
        """How many LP solves failed and were solved again from scratch."""
        return self._retried_solves

    def column(self, column_id: int) -> int:
        """The HiGHS column of column ``column_id``: its index in the user's model.

        Raises:
            KeyError: For an id this master solver did not receive.
        """
        try:
            return self._columns[column_id]
        except KeyError:
            raise KeyError(f"HighspyMasterSolver: no column has the id {column_id}") from None

    # MARK: The rows

    def number_of_rows(self) -> int:
        return len(self._rows)

    def row_bounds(self) -> tuple[FloatArray, FloatArray]:
        # HiGHS takes bounds beyond infinite_bound as infinite.
        lp = self._model.getLp()
        infinite_bound = _option(self._model, "infinite_bound")
        lower = np.asarray(lp.row_lower_, dtype=np.float64)[self._rows]
        upper = np.asarray(lp.row_upper_, dtype=np.float64)[self._rows]
        lower[lower <= -infinite_bound] = -math.inf
        upper[upper >= infinite_bound] = math.inf
        return lower, upper

    def add_rows(self, rows: RowBatch) -> None:
        """Appends rows to the model, ranges included.

        Raises:
            ValueError: For bounds that no row holds (NaN, ``lower = +inf``, ``upper = -inf``,
                ``lower > upper``). A refused batch adds no row.
        """
        import highspy

        # Check every row first, so that a refused batch adds nothing.
        if not bounds_can_be_held(rows.lower, rows.upper).all():
            raise ValueError(
                "HighspyMasterSolver.add_rows: row bounds must be numbers, with lower <= upper, "
                "lower < +inf and upper > -inf"
            )

        first = self._model.getNumRow()
        count = len(rows)

        if self._model.addRows(count, rows.lower, rows.upper, 0, [], [], []) == (
            highspy.HighsStatus.kError
        ):
            raise ValueError("HighspyMasterSolver.add_rows: HiGHS refused the rows")

        for offset, name in enumerate(rows.names):
            if name:
                self._model.passRowName(first + offset, name)

        self._rows = np.concatenate([self._rows, np.arange(first, first + count, dtype=np.int32)])

    # MARK: The columns

    def add_columns(self, columns: ColumnBatch) -> None:
        """Appends columns to the model.

        Raises:
            ValueError: Also when HiGHS refuses the batch (e.g. a row listed twice in a
                column); the batch then adds nothing.
        """
        import highspy

        if len(columns) == 0:
            return

        check_columns(columns, len(self._rows), self._columns)

        # HiGHS refuses some batches the shared checks let through; it then adds none of the
        # columns, and none is recorded.
        first = self._model.getNumCol()
        status = self._model.addCols(
            len(columns),
            self._sense_sign() * columns.cost,
            columns.lower,
            columns.upper,
            len(columns.values),
            columns.starts[:-1].astype(np.int32),
            self._rows[columns.row_indices],
            columns.values,
        )

        if status == highspy.HighsStatus.kError:
            raise ValueError("HighspyMasterSolver.add_columns: HiGHS refused the columns")

        for offset, column_id in enumerate(columns.ids.tolist()):
            self._columns[column_id] = first + offset

    # MARK: The solves

    def solve_relaxation(self, params: RelaxationParams = RelaxationParams()) -> SolverStatus:
        self._restore_upper_bounds()

        with (
            _ctrl_c_held_back(),
            self._solve_options(params.timeout_s, params.threads, params.verbose, True),
        ):
            self._requested_threads = params.threads
            return self._run(integer=False)

    def solve_integer(self, params: IntegerParams = IntegerParams()) -> SolverStatus:
        import highspy

        self._restore_upper_bounds()

        # Ctrl-C stops a solve with a time limit, as that limit does; otherwise it waits for
        # the solve's end.
        with (
            _ctrl_c_held_back() as ctrl_c,
            self._solve_options(params.timeout_s, params.threads, params.verbose, False),
            _temporary_options(self._model, mip_rel_gap=params.relative_gap),
            _mip_deadline(self._model, params.timeout_s, ctrl_c) as interrupted,
        ):
            self._requested_threads = params.threads

            generated = np.array(list(self._columns.values()), dtype=np.int32)

            if len(generated) > 0:
                kinds = np.full(len(generated), highspy.HighsVarType.kInteger.value, np.uint8)
                self._model.changeColsIntegrality(len(generated), generated, kinds)

                # HiGHS has no binary type: an integer column with an upper bound of at
                # most 1, and of at most its own, which comes back afterwards.
                if params.column_type == ColumnType.Binary:
                    lp = self._model.getLp()
                    lower = np.asarray(lp.col_lower_, dtype=np.float64)[generated]
                    upper = np.asarray(lp.col_upper_, dtype=np.float64)[generated]
                    self._model.changeColsBounds(
                        len(generated), generated, lower, np.minimum(upper, 1.0)
                    )
                    self._binary_columns = generated
                    self._upper_bounds = upper

            status = self._run(integer=True)

            if interrupted() and (
                self._model.getModelStatus() == highspy.HighsModelStatus.kInterrupt
            ):
                return SolverStatus.TimeLimit

            return status

    def objective(self) -> float:
        objective: float = self._model.getInfo().objective_function_value
        return self._sense_sign() * objective

    def duals(self) -> FloatArray:
        """The duals of the last LP solve.

        Raises:
            RuntimeError: When the last solve gave no duals.
        """
        solution = self._model.getSolution()

        if not solution.dual_valid:
            raise RuntimeError("HighspyMasterSolver.duals: the last solve gave no duals")

        duals: FloatArray = np.asarray(solution.row_dual, dtype=np.float64)[self._rows]
        return self._sense_sign() * duals

    def primal(self, ids: IdArray) -> FloatArray:
        """The values of the given columns in the last solve.

        Raises:
            KeyError: For an id this master solver did not receive.
            RuntimeError: When the last solve gave no values, or a column was added after it.
        """
        positions = np.array([self.column(int(column_id)) for column_id in ids], dtype=int)
        solution = self._model.getSolution()

        if len(positions) == 0:
            return np.zeros(0)

        if not solution.value_valid:
            raise RuntimeError("HighspyMasterSolver.primal: the last solve gave no values")

        values = np.asarray(solution.col_value, dtype=np.float64)

        if np.max(positions) >= len(values):
            raise RuntimeError(
                "HighspyMasterSolver.primal: a column was added after the last solve"
            )

        result: FloatArray = values[positions]
        return result

    # MARK: The rest of the interface

    def set_option(self, name: str, value: OptionValue) -> None:
        """Sets the HiGHS option ``name``.

        Raises:
            ValueError: When HiGHS refuses the name or the value.
        """
        import highspy

        # highspy takes an int for a double option, and refuses a bool for an int option and an
        # int that HiGHS's integers cannot hold.
        if self._model.setOptionValue(name, value) == highspy.HighsStatus.kError:
            raise ValueError(
                f"HighspyMasterSolver.set_option: HiGHS refused the option {name!r} or its value"
            )

    def capabilities(self) -> Capabilities:
        return Capabilities(add_rows=True, integer_solve=True)

    def dual_tolerance(self) -> float:
        """HiGHS's ``dual_feasibility_tolerance``, read from the model."""
        tolerance: float = _option(self._model, "dual_feasibility_tolerance")
        return tolerance

    def largest_user_cost(self) -> float:
        """The largest absolute cost of the model's columns that this master solver did
        not add."""
        costs = np.abs(np.asarray(self._model.getLp().col_cost_, dtype=np.float64))
        costs[np.array(list(self._columns.values()), dtype=np.intp)] = 0.0
        return float(np.max(costs)) if len(costs) > 0 else 0.0

    def restore_user_model(self) -> None:
        """Gives the generated columns their upper bounds back, after a binary integer
        solve; the solve options already come back after each solve."""
        self._restore_upper_bounds()

    # MARK: Inside

    def _sense_sign(self) -> float:
        """+1 for a minimizing model, -1 for a maximizing one: turns its values into
        ours."""
        import highspy

        sense = self._model.getObjectiveSense()[1]
        return -1.0 if sense == highspy.ObjSense.kMaximize else 1.0

    @contextmanager
    def _solve_options(
        self, timeout_s: float, threads: int, verbose: bool, relaxation: bool
    ) -> Generator[None, None, None]:
        """The options every solve sets from its parameters, for the duration of the
        solve: the user's values come back after it.

        HiGHS compares its time limit with a clock that counts all the model's solves,
        so the limit is what has run already plus this solve's.
        """
        with _temporary_options(
            self._model,
            time_limit=self._model.getRunTime() + max(0.0, timeout_s),
            threads=threads,
            output_flag=verbose,
            solve_relaxation=relaxation,
        ):
            yield

    def _restore_upper_bounds(self) -> None:
        """Gives the generated columns their upper bounds back, after an integer solve
        made them binary."""
        if self._binary_columns is None:
            return

        columns = self._binary_columns
        lower = np.asarray(self._model.getLp().col_lower_, dtype=np.float64)[columns]
        self._model.changeColsBounds(len(columns), columns, lower, self._upper_bounds)
        self._binary_columns = None

    def _run_once(self) -> highspy.HighsModelStatus | None:
        """Runs HiGHS once; no status for a run HiGHS refused (it keeps the previous
        one)."""
        import highspy

        if self._model.run() == highspy.HighsStatus.kError:
            return None

        status: highspy.HighsModelStatus = self._model.getModelStatus()
        return status

    def _run(self, integer: bool) -> SolverStatus:
        """Runs HiGHS; a run refused for its thread count runs again with the count in
        force, an LP that failed again from scratch, and an "infeasible or unbounded"
        answer without presolve."""
        import highspy

        outcome = self._run_once()

        # HiGHS fixes its thread count at its first solve in the process and refuses a run
        # asking for another; 0 runs with the count in force.
        if outcome is None and self._requested_threads > 0:
            if self._warned_threads != self._requested_threads:
                self._warned_threads = self._requested_threads
                warnings.warn(
                    "HighspyMasterSolver: HiGHS keeps the thread count of its first solve in "
                    f"this process; this solve asked for {self._requested_threads} threads and "
                    "runs with the count in force instead",
                    RuntimeWarning,
                    stacklevel=3,
                )

            self._model.setOptionValue("threads", 0)
            outcome = self._run_once()

        # A run HiGHS refused is a solve error, never the previous solve's status.
        status = outcome if outcome is not None else highspy.HighsModelStatus.kSolveError

        # A warm-started dual simplex can fail on large costs ("excessive dual values") where the
        # same LP solved from scratch, on a cleared solver, does not. An unknown status (HiGHS
        # also gives one when simplex wrongly detects cycling after many rounds of added
        # columns) and a refused run are solved again the same way. The retry has what is left
        # of the solve's time limit.
        if not integer and status in (
            highspy.HighsModelStatus.kSolveError,
            highspy.HighsModelStatus.kUnknown,
            highspy.HighsModelStatus.kNotset,
        ):
            self._retried_solves += 1
            self._model.clearSolver()
            outcome = self._run_once()
            status = outcome if outcome is not None else highspy.HighsModelStatus.kSolveError

        if status == highspy.HighsModelStatus.kUnboundedOrInfeasible:
            with _temporary_options(self._model, presolve="off"):
                outcome = self._run_once()

            status = outcome if outcome is not None else highspy.HighsModelStatus.kSolveError

        return _status(status)
