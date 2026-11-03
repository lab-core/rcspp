#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""What a column generation run collects and what its hooks see."""

from dataclasses import dataclass


@dataclass(frozen=True, kw_only=True)
class StatisticsParams:
    """Which statistics a run collects.

    The result itself is always filled.
    """

    #: Time spent in LP solves and in pricing (two clock reads per iteration for each).
    timers: bool = True


@dataclass(frozen=True, eq=False, kw_only=True)
class Statistics:
    """Counters and times of one run (times only when ``StatisticsParams.timers`` is
    set)."""

    #: Columns added by the pricers during the run.
    generated_columns: int
    #: Artificial columns in the master.
    artificial_columns: int
    #: Cost of each artificial column (``None`` when there is none): an ``Infeasible`` run found
    #: no solution at this cost.
    artificial_cost: float | None
    #: Wall-clock time of the run, in seconds.
    total_seconds: float
    #: Time spent in LP solves, in seconds (0 when timers are off).
    relaxation_seconds: float
    #: Time spent in pricing, in seconds (0 when timers are off).
    pricing_seconds: float


@dataclass(frozen=True, eq=False, kw_only=True)
class IterationInfo:
    """What the iteration hook sees."""

    #: Iteration number, from 1.
    iteration: int
    #: Value of the master LP at this iteration.
    objective: float
    #: Columns the pricers added at this iteration.
    new_columns: int
    #: Columns in the master after this iteration, artificial ones included.
    total_columns: int
    #: Time since the run started, in seconds.
    elapsed_seconds: float
