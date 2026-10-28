#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The options of a column generation run, with components by name."""

from typing import Callable

from .._core.column_generation import ColumnGenerationParams as _CppColumnGenerationParams
from .._core.column_generation import (
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
    :mod:`rcspp.column_generation.registry`); ``None`` means the defaults,
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
        on_iteration: Called after every iteration with an :class:`IterationInfo`.
        on_converged: Called after each proven convergence with a
            :class:`ConvergenceContext`; returns a :class:`ConvergenceAction`.
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
        self.tolerances = make_tolerances(tolerances) if isinstance(tolerances, str) else tolerances
        self.initialization = (
            make_initialization(initialization)
            if isinstance(initialization, str)
            else initialization
        )
        self.on_iteration = on_iteration
        self.on_converged = on_converged
