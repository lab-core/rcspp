#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The column generation loop: solve the master LP, price its duals, add columns."""

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
from .pricer import PricingRequest, PricingStatus
from .registry import make_initialization, make_tolerances
from .statistics import IterationInfo, Statistics
from .tolerances import ToleranceContext, Tolerances
from .types import FloatArray, IntegerParams, SolverStatus


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
    #: An LP solve failed for another reason (numerical trouble...).
    SolverError = auto()


@dataclass(frozen=True, eq=False, kw_only=True)
class ColumnGenerationResult:
    """Result of a run."""

    #: How the run ended.
    status: ColumnGenerationStatus
    #: Value of the last master LP solved (``None`` when none was).
    objective: float | None
    #: A proven lower bound on the master LP optimum (``-inf`` when none).
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

            # Price the duals and add the columns found.
            with pricing.measure():
                new_columns, proven = self._price(duals, start, reduced_cost_tolerance)

            generated_columns += new_columns

            if params.on_iteration is not None:
                params.on_iteration(
                    IterationInfo(
                        iteration=iterations,
                        objective=objective,
                        new_columns=new_columns,
                        total_columns=master.number_of_columns,
                        elapsed_seconds=time.perf_counter() - start,
                    )
                )

            # New columns: solve again. None: converged, proven or not.
            if new_columns > 0:
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

                if params.on_converged(context) == ConvergenceAction.Continue:
                    continue

            # An artificial column still in use means no real solution exists.
            if self._uses_artificial_columns():
                status = ColumnGenerationStatus.Infeasible
            else:
                status = ColumnGenerationStatus.Optimal
                lower_bound = objective

            break

        return ColumnGenerationResult(
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
        self, duals: FloatArray, start: float, reduced_cost_tolerance: float
    ) -> tuple[int, bool]:
        """Prices every pricer once and adds what they find: the number of columns added,
        and whether pricing proved that no column improves."""
        master = self._master
        new_columns = 0
        proven = True

        for index, entry in enumerate(master.pricers):
            # A column improves when its reduced cost, convexity dual included, is negative.
            convexity = entry.convexity_row
            convexity_dual = float(duals[convexity]) if convexity is not None else 0.0
            request = PricingRequest(
                reduced_cost_threshold=convexity_dual - reduced_cost_tolerance,
                timeout_s=max(0.0, self._params.timeout_s - (time.perf_counter() - start)),
            )

            priced = entry.pricer.price(duals, request)

            # Only an exact stage that finished proves there is no improving column. No path at
            # all (Infeasible) proves it too: no column of any cost exists. A pricer with
            # nothing to price holds no improving column.
            finished = priced.status in (PricingStatus.Optimal, PricingStatus.Infeasible)
            nothing_to_price = priced.status == PricingStatus.NotApplicable
            proven = proven and (nothing_to_price or (priced.exact_stage and finished))
            new_columns += len(priced.columns)
            master.add_columns(index, priced.columns)

        return new_columns, proven

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

    def _uses_artificial_columns(self) -> bool:
        """Whether the last LP solution uses an artificial column."""
        ids = self._master.artificial_column_ids

        if len(ids) == 0:
            return False

        values = np.asarray(self._master.solver.primal(ids), dtype=np.float64)
        return bool((values > self._tolerances.feasibility(self._context)).any())
