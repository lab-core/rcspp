#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The options of a column generation run, with components by name."""

import math
from collections.abc import Callable
from dataclasses import dataclass, field

from .convergence import ConvergenceAction, ConvergenceContext
from .debug_checks import DebugChecks
from .master_initialization import ArtificialColumns, MasterInitialization
from .statistics import IterationInfo, StatisticsParams
from .tolerances import AbsoluteTolerances, Tolerances
from .types import RelaxationParams


@dataclass(frozen=True, kw_only=True)
class ColumnGenerationParams:
    """Options of a run.

    ``tolerances`` and ``initialization`` take an object or a name (see
    :mod:`rcspp.column_generation.registry`), built by :class:`ColumnGeneration`.

    Args:
        max_iterations: Maximum number of iterations (LP solves); no limit by default.
        timeout_s: Wall-clock limit of the run, in seconds.
        relaxation: Options of every LP solve; its time limit is capped by what is left of
            ``timeout_s``.
        tolerances: The tolerances, or their name.
        initialization: How the first LP becomes solvable, or its name.
        statistics: Which statistics to collect.
        absolute_gap: Stop (``AbsoluteGapLimit``) when the LP value minus the lower bound
            (:attr:`IterationInfo.lower_bound`) is at most this; 0 (the default) is off.
        relative_gap: Stop (``RelativeGapLimit``) when the LP value minus the lower bound is at most
            this times the LP value's magnitude; 0 (the default) is off.
        cutoff: The cost of a solution already known: stop (``Cutoff``) when the lower bound
            reaches it, as nothing here costs less; ``inf`` (the default) is off.
        integer_objective: Every solution has an integer cost: the lower bound is rounded
            up, for the cutoff and for a stop (``IntegerObjectiveGap``) when it reaches the LP value
            rounded up.
        log_frequency: Log a line every this many iterations, the last one too, and one
            when the run ends, to the logger ``rcspp.column_generation`` at level INFO; 0 (the
            default) logs nothing.
        on_iteration: Called after every iteration with an :class:`IterationInfo`.
        on_converged: Called after each proven convergence with a
            :class:`ConvergenceContext`; returns a :class:`ConvergenceAction`.
        debug_checks: Which checks of the master solver and the pricers to make, all off by
            default; a failed one raises :class:`DebugCheckError`.

    The stops on the gap and the cutoff are checked after an iteration that added columns
    and has a lower bound, in this order: the cutoff, the absolute gap, the relative gap, the
    integer objective; the first one reached gives the status. They compare with the
    reduced-cost tolerance's margin.

    Raises:
        ValueError: When a gap is negative or not a number, or ``log_frequency`` negative.
        TypeError: When ``log_frequency`` is not an integer.
    """

    max_iterations: float = math.inf
    timeout_s: float = math.inf
    relaxation: RelaxationParams = field(default_factory=RelaxationParams)
    tolerances: Tolerances | str = field(default_factory=AbsoluteTolerances)
    initialization: MasterInitialization | str = field(default_factory=ArtificialColumns)
    statistics: StatisticsParams = field(default_factory=StatisticsParams)
    absolute_gap: float = 0.0
    relative_gap: float = 0.0
    cutoff: float = math.inf
    integer_objective: bool = False
    log_frequency: int = 0
    on_iteration: Callable[[IterationInfo], None] | None = None
    on_converged: Callable[[ConvergenceContext], ConvergenceAction] | None = None
    debug_checks: DebugChecks = field(default_factory=DebugChecks)

    def __post_init__(self) -> None:
        # A negative gap would silently never stop the run.
        for name, gap in (("absolute_gap", self.absolute_gap), ("relative_gap", self.relative_gap)):
            if not gap >= 0.0:
                raise ValueError(f"ColumnGenerationParams: {name} must be at least 0")

        # A float would never divide the iteration count evenly.
        frequency = self.log_frequency

        if isinstance(frequency, bool) or not isinstance(
            frequency, int  # pyright: ignore[reportUnnecessaryIsInstance]
        ):
            raise TypeError("ColumnGenerationParams: log_frequency must be an integer")

        if self.log_frequency < 0:
            raise ValueError("ColumnGenerationParams: log_frequency must be at least 0")
