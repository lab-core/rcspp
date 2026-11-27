#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The column generation loop: solve the master LP, price its duals, add columns."""

import logging
import math
import time
from collections.abc import Generator
from contextlib import contextmanager
from dataclasses import dataclass, replace
from enum import Enum, auto

import numpy as np

from .convergence import ConvergenceAction, ConvergenceContext
from .master_initialization import MasterInitialization
from .master_problem import MasterProblem
from .params import ColumnGenerationParams
from .pricer import PricingRequest, PricingResult, PricingStatus
from .registry import make_initialization, make_tolerances
from .statistics import IterationInfo, PricerIterationInfo, Statistics, StatisticsParams
from .tolerances import ToleranceContext, Tolerances
from .types import FloatArray, IdArray, IntegerParams, SolverStatus

#: Where the progress log goes (``ColumnGenerationParams.log_frequency``).
logger = logging.getLogger("rcspp.column_generation")


class ColumnGenerationStatus(Enum):
    """How a run ended."""

    #: Proven: no improving column exists, and no artificial column is used.
    Optimal = auto()
    #: No new column, but some pricer did not prove it found the best ones.
    NotProven = auto()
    #: Converged with an artificial column still in use: no solution exists at the artificial
    #: cost used (``Statistics.artificial_cost``). A solution whose columns cost more than that
    #: per row could still exist: see :class:`ArtificialColumns`.
    Infeasible = auto()
    #: The master LP, with the columns it has so far, has no feasible solution. This does not
    #: prove the problem infeasible. Add starting columns, or use :class:`ArtificialColumns`.
    RestrictedMasterInfeasible = auto()
    #: The master LP is unbounded: some column can be used without limit at a negative cost.
    #: More columns cannot bound it, so the problem itself is unbounded; look for a missing
    #: convexity row or bound.
    Unbounded = auto()
    #: Stopped by ``ColumnGenerationParams.timeout_s``, in LP or pricing.
    TimeLimit = auto()
    #: Stopped by ``ColumnGenerationParams.max_iterations``.
    IterationLimit = auto()
    #: Stopped by ``ColumnGenerationParams.cutoff``: the lower bound reached the cutoff, so no
    #: solution here costs less than it. Checked first of the early stops.
    Cutoff = auto()
    #: Stopped by ``ColumnGenerationParams.absolute_gap``: the LP value minus the lower bound is
    #: at most the gap. Checked after ``Cutoff``.
    AbsoluteGapLimit = auto()
    #: Stopped by ``ColumnGenerationParams.relative_gap``: the LP value minus the lower bound is
    #: at most the gap times the LP value's magnitude. Checked after ``AbsoluteGapLimit``.
    RelativeGapLimit = auto()
    #: Stopped by ``ColumnGenerationParams.integer_objective``: the lower bound rounded up
    #: reached the LP value rounded up, so more iterations cannot raise what it proves about an
    #: integer solution. Checked last.
    IntegerObjectiveGap = auto()
    #: An LP solve failed for another reason (numerical trouble...).
    SolverError = auto()


@dataclass(frozen=True, eq=False, kw_only=True)
class ColumnGenerationResult:
    """Result of a run."""

    #: How the run ended.
    status: ColumnGenerationStatus
    #: Value of the last master LP solved (``None`` when none was).
    objective: float | None
    #: A proven lower bound on the master LP optimum: the objective when ``Optimal``, else the
    #: best Lagrangian bound of the run's iterations (``-inf`` when none is known; see
    #: :attr:`IterationInfo.lower_bound`).
    lower_bound: float
    #: Number of iterations (LP solves).
    iterations: int
    #: Counters and times.
    statistics: Statistics


# How a run ends when the master LP was not solved to optimality.
_STATUS_AFTER_FAILED_SOLVE = {
    SolverStatus.Infeasible: ColumnGenerationStatus.RestrictedMasterInfeasible,
    SolverStatus.Unbounded: ColumnGenerationStatus.Unbounded,
    SolverStatus.TimeLimit: ColumnGenerationStatus.TimeLimit,
    SolverStatus.Other: ColumnGenerationStatus.SolverError,
}


def _checked(status: object) -> SolverStatus:
    """The status of an LP solve, checked: a master solver written in Python may answer
    anything, which would fail far from its cause."""
    if not isinstance(status, SolverStatus):
        raise TypeError(
            f"MasterSolver.solve_relaxation: expected a SolverStatus, got {type(status).__name__}"
        )

    return status


class _Stopwatch:
    """Adds up the time of the blocks it measures; reads no clock when disabled."""

    def __init__(self, enabled: bool) -> None:
        self.seconds = 0.0
        self._enabled = enabled

    @contextmanager
    def measure(self) -> Generator[None, None, None]:
        if not self._enabled:
            yield
            return

        start = time.perf_counter()
        yield
        self.seconds += time.perf_counter() - start


class ColumnGeneration:
    """The column generation loop: solve the master LP, price its duals, add columns.

    Holds the master: the caller may keep using it. :meth:`run` may be called again, e.g. after
    the master or a pricer changed. The master is initialized once, by the first run of any
    loop over it.

    Args:
        master: The master, with its rows, pricers and any starting columns.
        params: Options; a tolerances or initialization name is built here.

    Raises:
        TypeError: When ``master`` is not a :class:`MasterProblem` or ``params`` not
            :class:`ColumnGenerationParams`.
        TypeError: When ``params.tolerances`` or ``params.initialization`` is neither an
            object of its kind nor a name.
        ValueError: For an unknown tolerances or initialization name.
    """

    def __init__(
        self, master: MasterProblem, params: ColumnGenerationParams = ColumnGenerationParams()
    ) -> None:
        # Checked: a wrong argument would otherwise fail deep inside the first run.
        arguments: list[tuple[object, type, str]] = [
            (master, MasterProblem, "a MasterProblem"),
            (params, ColumnGenerationParams, "ColumnGenerationParams"),
        ]

        for argument, expected, name in arguments:
            if not isinstance(argument, expected):
                raise TypeError(f"ColumnGeneration: expected {name}")

        self._master = master
        tolerances, initialization = params.tolerances, params.initialization
        self._tolerances = (
            make_tolerances(tolerances) if isinstance(tolerances, str) else tolerances
        )
        self._initialization = (
            make_initialization(initialization)
            if isinstance(initialization, str)
            else initialization
        )
        components: list[tuple[object, type, str]] = [
            (self._tolerances, Tolerances, "tolerances must be Tolerances"),
            (
                self._initialization,
                MasterInitialization,
                "initialization must be a MasterInitialization",
            ),
        ]

        for component, expected, rule in components:
            if not isinstance(component, expected):
                raise TypeError(f"ColumnGeneration: {rule} or a name")

        self._params = replace(
            params, tolerances=self._tolerances, initialization=self._initialization
        )
        self._context = ToleranceContext()
        self._initialized = False

    def run(self) -> ColumnGenerationResult:
        """Runs column generation until convergence or a limit, then has the master
        solver restore the user's model (:meth:`MasterSolver.restore_user_model`), also
        when the run ends with an exception, Ctrl-C included.

        Raises:
            ValueError: When the master solver gives a number of duals other than the rows.
            TypeError: When an LP solve of the master solver answers anything but a
                :class:`SolverStatus`.
        """
        try:
            return self._iterate()
        finally:
            self._master.solver.restore_user_model()

    def solve_restricted_master_integer(
        self, params: IntegerParams = IntegerParams()
    ) -> SolverStatus:
        """Solves the master as a MIP over the columns it holds (a heuristic: the best
        plan may need columns that column generation never produced).

        Args:
            params: Time limit, gap and column type.

        Returns:
            The master solver's status.
        """
        return self._master.solver.solve_integer(params)

    @property
    def params(self) -> ColumnGenerationParams:
        """The options, with the components built from their names."""
        return self._params

    @property
    def tolerance_context(self) -> ToleranceContext:
        """The cost scale and other inputs of the tolerances, set by the first run."""
        return self._context

    # MARK: Inside

    def _iterate(self) -> ColumnGenerationResult:
        """The loop itself: solve, price, add columns, until convergence or a limit."""
        params, master = self._params, self._master
        solver = master.solver
        start = time.perf_counter()
        relaxing = _Stopwatch(params.statistics.timers)
        pricing = _Stopwatch(params.statistics.timers)

        self._initialize()

        # Never below the LP solver's own tolerance, or a column it leaves out would come back
        # forever.
        reduced_cost_tolerance = max(
            self._tolerances.reduced_cost(self._context), solver.dual_tolerance()
        )

        status = ColumnGenerationStatus.SolverError
        objective: float | None = None
        lower_bound = -math.inf
        iterations = generated_columns = 0
        paths_allowed = self._paths_allowed()
        statistics = params.statistics
        wants_info = params.on_iteration is not None or params.log_frequency > 0
        last_info: IterationInfo | None = None

        while True:
            # Limits.
            if iterations >= params.max_iterations:
                status = ColumnGenerationStatus.IterationLimit
                break

            remaining = params.timeout_s - (time.perf_counter() - start)

            if remaining <= 0.0:
                status = ColumnGenerationStatus.TimeLimit
                break

            # Solve the master LP, within the time left.
            relaxation = replace(
                params.relaxation, timeout_s=min(params.relaxation.timeout_s, remaining)
            )

            relaxation_before = relaxing.seconds

            with relaxing.measure():
                solver_status = _checked(solver.solve_relaxation(relaxation))

            if solver_status != SolverStatus.Optimal:
                status = _STATUS_AFTER_FAILED_SOLVE[solver_status]
                break

            # Read its value and duals.
            iterations += 1
            objective = solver.objective()
            duals = np.asarray(solver.duals(), dtype=np.float64)

            if duals.shape != (master.number_of_rows,):
                raise ValueError(
                    f"MasterSolver.duals: expected {master.number_of_rows} values, got "
                    f"{duals.size}"
                )

            # The LP solution, read before the new columns change the model.
            master_values = (
                self._nonzero_values() if wants_info and statistics.master_values else None
            )

            # Price the duals and add the columns found.
            columns_before = master.number_of_columns
            relaxation_seconds = relaxing.seconds - relaxation_before
            pricing_before = pricing.seconds

            with pricing.measure():
                new_columns, proven, bound_terms, pricers = self._price(
                    duals, start, reduced_cost_tolerance, paths_allowed, wants_info
                )

            generated_columns += new_columns
            iteration_bound = objective + bound_terms
            lower_bound = max(lower_bound, iteration_bound)

            if wants_info:
                timers = statistics.timers
                last_info = IterationInfo(
                    iteration=iterations,
                    objective=objective,
                    lower_bound=iteration_bound,
                    new_columns=new_columns,
                    total_columns=master.number_of_columns,
                    elapsed_seconds=time.perf_counter() - start,
                    relaxation_seconds=relaxation_seconds if timers else None,
                    pricing_seconds=pricing.seconds - pricing_before if timers else None,
                    pricers=pricers,
                    duals=duals.copy() if statistics.duals else None,
                    master_values=master_values,
                    added_columns=(
                        tuple(master.columns[columns_before:]) if statistics.columns else None
                    ),
                )

                if params.log_frequency > 0 and iterations % params.log_frequency == 0:
                    _log_iteration(last_info)

                if params.on_iteration is not None:
                    params.on_iteration(last_info)

            # New columns: solve again, unless the bound is already close enough.
            if new_columns > 0:
                early_stop = self._early_stop(objective, iteration_bound, reduced_cost_tolerance)

                if early_stop is not None:
                    status = early_stop
                    break

                continue

            # Unproven: the run's time limit may have cut pricing short.
            if not proven:
                status = (
                    ColumnGenerationStatus.TimeLimit
                    if time.perf_counter() - start >= params.timeout_s
                    else ColumnGenerationStatus.NotProven
                )
                break

            # Proven convergence: the hook may change a pricer and ask for more.
            if params.on_converged is not None:
                context = ConvergenceContext(
                    master=master,
                    iteration=iterations,
                    objective=objective,
                    duals=duals,
                    values=self._column_values(),
                )

                # The hook may have changed the problem: the bounds found so far may not hold.
                if params.on_converged(context) == ConvergenceAction.Continue:
                    lower_bound = -math.inf
                    paths_allowed = self._paths_allowed()
                    continue

            # An artificial column still in use means no real solution exists.
            if self._uses_artificial_columns():
                status = ColumnGenerationStatus.Infeasible
            else:
                status = ColumnGenerationStatus.Optimal
                lower_bound = objective

            break

        result = ColumnGenerationResult(
            status=status,
            objective=objective,
            lower_bound=lower_bound,
            iterations=iterations,
            statistics=Statistics(
                generated_columns=generated_columns,
                artificial_columns=len(master.artificial_column_ids),
                artificial_cost=master.artificial_cost,
                total_seconds=time.perf_counter() - start,
                relaxation_seconds=relaxing.seconds,
                pricing_seconds=pricing.seconds,
            ),
        )

        if params.log_frequency > 0:
            # The last iteration, unless its line is out already, then how the run ended.
            if last_info is not None and last_info.iteration % params.log_frequency != 0:
                _log_iteration(last_info)

            _log_end(result)

        return result

    def _initialize(self) -> None:
        """On this loop's first run: sets the tolerances' inputs, and initializes the
        master unless another loop did."""
        if self._initialized:
            return

        self._context = self._compute_tolerance_context()

        if not self._master.initialized:
            self._initialization.initialize(self._master, self._context)
            self._master.mark_initialized()

        self._initialized = True

    def _price(
        self,
        duals: FloatArray,
        start: float,
        reduced_cost_tolerance: float,
        paths_allowed: list[float],
        wants_info: bool,
    ) -> tuple[int, bool, float, tuple[PricerIterationInfo, ...] | None]:
        """Prices every pricer once and adds what they find: the number of columns added,
        whether pricing proved that no column improves, what the pricers add to the LP
        value to give the Lagrangian bound (``-inf`` when unknown), and what each pricer did
        when the run collects it (``None`` otherwise).

        The bound: for any solution of the full master, cost = LP value + the sum over the
        columns of reduced cost * value. A pricer's columns cost at least its best reduced
        cost each, and its convexity row limits how many a solution uses (``paths_allowed``),
        so each pricer adds (paths allowed) * (its best reduced cost) when that is negative.
        """
        master = self._master
        statistics = self._params.statistics
        collect = wants_info and statistics.pricers
        pricers: list[PricerIterationInfo] = []
        new_columns = 0
        proven = True
        bound_terms = 0.0

        for index, entry in enumerate(master.pricers):
            # A column improves when its reduced cost, convexity dual included, is negative.
            convexity = entry.convexity_row
            convexity_dual = float(duals[convexity]) if convexity is not None else 0.0
            pricing_start = time.perf_counter()
            request = PricingRequest(
                reduced_cost_threshold=convexity_dual - reduced_cost_tolerance,
                timeout_s=max(0.0, self._params.timeout_s - (pricing_start - start)),
                search_details=collect,
            )

            priced = entry.pricer.price(duals, request)

            if collect:
                seconds = time.perf_counter() - pricing_start
                pricers.append(_pricer_info(priced, convexity_dual, seconds, statistics))

            # Only an exact stage that finished proves there is no improving column. No path at
            # all (Infeasible) proves it too: no column of any cost exists. A pricer with
            # nothing to price holds no improving column.
            finished = priced.status in (PricingStatus.Optimal, PricingStatus.Infeasible)
            nothing_to_price = priced.status == PricingStatus.NotApplicable
            proved = nothing_to_price or (priced.exact_stage and finished)
            proven = proven and proved
            new_columns += len(priced.columns)
            master.add_columns(index, priced.columns)

            # Nothing to add to the bound when the pricer proved it has no improving column (its
            # bound may be the threshold, a tolerance below zero), or when it is already unknown.
            if (proved and not priced.columns) or bound_terms == -math.inf:
                continue

            # Its best reduced cost, convexity dual included; never above a column it returned.
            best = priced.min_reduced_cost_bound

            if priced.columns and best > -math.inf:
                best = min(best, min(column.cost for column in priced.columns))

            best -= convexity_dual

            # An unknown best (-inf) or no limit on its paths (inf) makes the bound unknown; a
            # row that allows no path adds nothing.
            paths = paths_allowed[index]

            if best < 0.0 and paths > 0.0:
                bound_terms += paths * best

        return new_columns, proven, bound_terms, tuple(pricers) if collect else None

    def _paths_allowed(self) -> list[float]:
        """How many paths each pricer's convexity row allows: its upper bound, ``inf``
        without a row.

        Read once per run and after a restart, as a master solver may copy its whole
        model to answer.
        """
        master = self._master
        rows = [entry.convexity_row for entry in master.pricers]

        if all(row is None for row in rows):
            return [math.inf] * len(rows)

        upper = master.solver.row_bounds()[1]
        return [math.inf if row is None else float(upper[row]) for row in rows]

    def _early_stop(
        self, objective: float, bound: float, tolerance: float
    ) -> ColumnGenerationStatus | None:
        """The status of a stop on the cutoff or the gap, or ``None`` to go on."""
        params = self._params

        if bound == -math.inf:
            return None

        # Nothing here costs less than the bound; with integer costs, than the bound rounded up.
        if params.integer_objective:
            lowest_cost = math.ceil(bound - tolerance)
        else:
            lowest_cost = bound + tolerance

        if lowest_cost >= params.cutoff:
            return ColumnGenerationStatus.Cutoff

        gap = objective - bound

        if params.absolute_gap > 0.0 and gap <= params.absolute_gap:
            return ColumnGenerationStatus.AbsoluteGapLimit

        if params.relative_gap > 0.0 and gap <= params.relative_gap * abs(objective):
            return ColumnGenerationStatus.RelativeGapLimit

        # More iterations cannot raise the bound above the LP value rounded up.
        if params.integer_objective and lowest_cost >= math.ceil(objective - tolerance):
            return ColumnGenerationStatus.IntegerObjectiveGap

        return None

    def _compute_tolerance_context(self) -> ToleranceContext:
        """At least 1: the largest pricer cost scale, starting column cost and cost of the
        user's own variables."""
        master = self._master
        scale = 1.0

        for entry in master.pricers:
            scale = max(scale, entry.pricer.cost_scale())

        for column in master.columns:
            scale = max(scale, abs(column.solution.column.cost))

        # Starting columns the user built in their own model.
        scale = max(scale, master.solver.largest_user_cost())

        return ToleranceContext(cost_scale=scale)

    def _column_values(self) -> FloatArray:
        """The value of every column, indexed like ``MasterProblem.columns``."""
        ids = np.array([column.id for column in self._master.columns], dtype=np.uint64)
        return np.asarray(self._master.solver.primal(ids), dtype=np.float64)

    def _nonzero_values(self) -> tuple[IdArray, FloatArray]:
        """The ids and values of the columns the last LP solution uses."""
        ids = np.arange(self._master.number_of_columns, dtype=np.uint64)
        values = np.asarray(self._master.solver.primal(ids), dtype=np.float64)
        used = values != 0.0
        return ids[used], values[used]

    def _uses_artificial_columns(self) -> bool:
        """Whether the last LP solution uses an artificial column."""
        ids = self._master.artificial_column_ids

        if len(ids) == 0:
            return False

        values = np.asarray(self._master.solver.primal(ids), dtype=np.float64)
        return bool((values > self._tolerances.feasibility(self._context)).any())


# MARK: Statistics and the log


def _pricer_info(
    priced: PricingResult, convexity_dual: float, seconds: float, statistics: StatisticsParams
) -> PricerIterationInfo:
    """What a pricer did, from its answer; the engine's counts dropped unless
    collected."""
    details = priced.search_details

    if not statistics.search:
        details = tuple(
            replace(stage, extended_labels=None, bidirectional=None) for stage in details
        )

    return PricerIterationInfo(
        status=priced.status,
        new_columns=len(priced.columns),
        min_reduced_cost=(
            min(column.cost for column in priced.columns) - convexity_dual
            if priced.columns
            else None
        ),
        seconds=seconds,
        search_details=details,
    )


def _log_iteration(info: IterationInfo) -> None:
    """One line of the progress log."""
    known = info.lower_bound > -math.inf
    gap = info.objective - info.lower_bound
    relative = f" ({gap / abs(info.objective):.2%})" if info.objective != 0.0 else ""
    logger.info(
        "iteration %d  %.2f s  LP %s  pricing %s  columns +%d / %d  LP value %.6g  bound %s  "
        "gap %s",
        info.iteration,
        info.elapsed_seconds,
        _seconds(info.relaxation_seconds),
        _seconds(info.pricing_seconds),
        info.new_columns,
        info.total_columns,
        info.objective,
        f"{info.lower_bound:.6g}" if known else "-",
        f"{gap:.6g}{relative}" if known else "-",
    )


def _log_end(result: ColumnGenerationResult) -> None:
    """The last line of the progress log: how the run ended."""
    statistics = result.statistics
    logger.info(
        "%s after %d iterations, %.2f s (LP %.2f s, pricing %.2f s), %d columns generated, "
        "LP value %s, bound %s",
        result.status.name,
        result.iterations,
        statistics.total_seconds,
        statistics.relaxation_seconds,
        statistics.pricing_seconds,
        statistics.generated_columns,
        "-" if result.objective is None else f"{result.objective:.6g}",
        f"{result.lower_bound:.6g}" if result.lower_bound > -math.inf else "-",
    )


def _seconds(seconds: float | None) -> str:
    """A time of the log; ``-`` when not measured."""
    return "-" if seconds is None else f"{seconds:.3f} s"
