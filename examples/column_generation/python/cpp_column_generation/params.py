#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The options of a column generation run, with components by name."""

from typing import Callable

from rcspp._core.cpp_column_generation import ColumnGenerationParams as _CppColumnGenerationParams
from rcspp._core.cpp_column_generation import (
    ConvergenceAction,
    ConvergenceContext,
    IterationInfo,
    MasterInitialization,
    RelaxationParams,
    StatisticsParams,
    Tolerances,
)

from .registry import make_initialization, make_tolerances


class ColumnGenerationParams(_CppColumnGenerationParams):
    """Options of a run.

    ``tolerances`` and ``initialization`` take an object or a name (see
    :mod:`cpp_column_generation.registry`); ``None`` means the defaults,
    :class:`AbsoluteTolerances` and :class:`ArtificialColumns`. An option left out keeps its
    default. After construction, the attributes take objects only.

    Args:
        max_iterations: Maximum number of iterations (LP solves).
        timeout_s: Wall-clock limit of the run, in seconds.
        relaxation: Options of every LP solve; its time limit is capped by what is left of
            ``timeout_s``.
        tolerances: The tolerances, or their name.
        initialization: How the first LP becomes solvable, or its name.
        statistics: Which statistics to collect.
        absolute_gap: Stop (``AbsoluteGapLimit``) when the LP value minus the lower bound
            (:attr:`IterationInfo.lower_bound`) is at most this; 0 (the default) is off.
        relative_gap: Stop (``RelativeGapLimit``) when the LP value minus the lower bound is
            at most this times the LP value's magnitude; 0 (the default) is off.
        cutoff: The cost of a solution already known: stop (``Cutoff``) when the lower bound
            reaches it, as nothing here costs less; ``inf`` (the default) is off.
        integer_objective: Every solution has an integer cost: the lower bound is rounded
            up, for the cutoff and for a stop (``IntegerObjectiveGap``) when it reaches the LP
            value rounded up.
        on_iteration: Called after every iteration with an :class:`IterationInfo`.
        on_converged: Called after each proven convergence with a
            :class:`ConvergenceContext`; returns a :class:`ConvergenceAction`.

    The stops on the gap and the cutoff are checked after an iteration that added columns
    and has a lower bound, in this order: the cutoff, the absolute gap, the relative gap, the
    integer objective; the first one reached gives the status. :class:`ColumnGeneration`
    refuses a negative gap (``ValueError``).
    """

    def __init__(
        self,
        *,
        max_iterations: int | None = None,
        timeout_s: float | None = None,
        relaxation: RelaxationParams | None = None,
        tolerances: Tolerances | str | None = None,
        initialization: MasterInitialization | str | None = None,
        statistics: StatisticsParams | None = None,
        absolute_gap: float | None = None,
        relative_gap: float | None = None,
        cutoff: float | None = None,
        integer_objective: bool | None = None,
        on_iteration: Callable[[IterationInfo], None] | None = None,
        on_converged: Callable[[ConvergenceContext], ConvergenceAction] | None = None,
    ) -> None:
        super().__init__()
        if max_iterations is not None:
            self.max_iterations = max_iterations
        if timeout_s is not None:
            self.timeout_s = timeout_s
        if relaxation is not None:
            self.relaxation = relaxation
        if statistics is not None:
            self.statistics = statistics
        if absolute_gap is not None:
            self.absolute_gap = absolute_gap
        if relative_gap is not None:
            self.relative_gap = relative_gap
        if cutoff is not None:
            self.cutoff = cutoff
        if integer_objective is not None:
            self.integer_objective = integer_objective
        self.tolerances = make_tolerances(tolerances) if isinstance(tolerances, str) else tolerances
        self.initialization = (
            make_initialization(initialization)
            if isinstance(initialization, str)
            else initialization
        )
        self.on_iteration = on_iteration
        self.on_converged = on_converged
