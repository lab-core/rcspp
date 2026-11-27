#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""What a column generation run collects and what its hooks see."""

from dataclasses import dataclass

from .master_problem import MasterColumn
from .pricer import PricingStageInfo, PricingStatus
from .types import FloatArray, IdArray


@dataclass(frozen=True, kw_only=True)
class StatisticsParams:
    """Which statistics a run collects.

    The result itself is always filled, and so are the fields of :class:`IterationInfo` that
    name no switch. Each switch fills its fields of :class:`IterationInfo`, ``None`` when off.

    Raises:
        ValueError: When ``search`` is on without ``pricers``.
    """

    #: Time spent in LP solves and in pricing, over the run and at each iteration (two clock
    #: reads per iteration for each).
    timers: bool = True
    #: What each pricer did at each iteration (:attr:`IterationInfo.pricers`): one clock read
    #: per pricer, and the stages a pricer that has them ran.
    pricers: bool = False
    #: The search engine's counts in each stage's details (extended labels, the bidirectional
    #: search's half-way point and join); needs ``pricers``.
    search: bool = False
    #: The duals of each iteration (:attr:`IterationInfo.duals`): one copy per iteration.
    duals: bool = False
    #: The nonzero values of each iteration's LP solution
    #: (:attr:`IterationInfo.master_values`): one read of every column's value per iteration.
    master_values: bool = False
    #: The columns each iteration added (:attr:`IterationInfo.added_columns`).
    columns: bool = False

    def __post_init__(self) -> None:
        # The counts sit in the pricers' details: alone, they would silently give nothing.
        if self.search and not self.pricers:
            raise ValueError("StatisticsParams: search needs pricers")


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
class PricerIterationInfo:
    """What one pricer did at one iteration, measured by the loop around its call."""

    #: How its call ended.
    status: PricingStatus
    #: Columns it returned.
    new_columns: int
    #: The most negative reduced cost of the columns it returned, its convexity dual included;
    #: ``None`` when it returned none.
    min_reduced_cost: float | None
    #: Time of its call, in seconds.
    seconds: float
    #: Its own account of the call (:attr:`PricingResult.search_details`): for a
    #: :class:`GraphPricer`, the stages it ran; empty from a pricer without stages.
    search_details: tuple[PricingStageInfo, ...] = ()


@dataclass(frozen=True, eq=False, kw_only=True)
class IterationInfo:
    """What the iteration hook sees."""

    #: Iteration number, from 1.
    iteration: int
    #: Value of the master LP at this iteration.
    objective: float
    #: The Lagrangian bound at this iteration's duals: a lower bound on the LP value of the
    #: master with every column. ``-inf`` when unknown: a pricer without a bound (a heuristic
    #: stage, a search stopped early) or without a convexity row that limits its paths.
    lower_bound: float
    #: Columns the pricers added at this iteration.
    new_columns: int
    #: Columns in the master after this iteration, artificial ones included.
    total_columns: int
    #: Time since the run started, in seconds.
    elapsed_seconds: float
    #: Time of this iteration's LP solve, in seconds (``StatisticsParams.timers``).
    relaxation_seconds: float | None = None
    #: Time of this iteration's pricing, the columns' addition to the master included, in
    #: seconds (``StatisticsParams.timers``).
    pricing_seconds: float | None = None
    #: One entry per pricer, in the master's order (``StatisticsParams.pricers``).
    pricers: tuple[PricerIterationInfo, ...] | None = None
    #: The LP's duals, one per row, as the pricers received them (``StatisticsParams.duals``).
    duals: FloatArray | None = None
    #: The LP solution's nonzero values: the columns' ids in the master solver (their positions
    #: in :attr:`MasterProblem.columns`, artificial ones included) and their values, read
    #: before this iteration's columns were added (``StatisticsParams.master_values``).
    master_values: tuple[IdArray, FloatArray] | None = None
    #: The columns this iteration added, each with its id, pricer and solution
    #: (``StatisticsParams.columns``).
    added_columns: tuple[MasterColumn, ...] | None = None
