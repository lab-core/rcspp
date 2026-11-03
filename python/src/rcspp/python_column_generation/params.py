#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The options of a column generation run, with components by name."""

import math
from dataclasses import dataclass, field
from typing import Callable

from .convergence import ConvergenceAction, ConvergenceContext
from .master_initialization import ArtificialColumns, MasterInitialization
from .statistics import IterationInfo, StatisticsParams
from .tolerances import AbsoluteTolerances, Tolerances
from .types import RelaxationParams


@dataclass(frozen=True, kw_only=True)
class ColumnGenerationParams:
    """Options of a run.

    ``tolerances`` and ``initialization`` take an object or a name (see
    :mod:`rcspp.python_column_generation.registry`), built by :class:`ColumnGeneration`.

    Args:
        max_iterations: Maximum number of iterations (LP solves); no limit by default.
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

    max_iterations: float = math.inf
    timeout_s: float = math.inf
    relaxation: RelaxationParams = field(default_factory=RelaxationParams)
    tolerances: Tolerances | str = field(default_factory=AbsoluteTolerances)
    initialization: MasterInitialization | str = field(default_factory=ArtificialColumns)
    statistics: StatisticsParams = field(default_factory=StatisticsParams)
    on_iteration: Callable[[IterationInfo], None] | None = None
    on_converged: Callable[[ConvergenceContext], ConvergenceAction] | None = None
