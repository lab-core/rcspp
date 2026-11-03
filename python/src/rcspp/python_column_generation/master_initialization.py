#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""How the first master LP becomes solvable, before any column is generated."""

import math
from abc import ABC, abstractmethod
from typing import ClassVar

import numpy as np

from .master_problem import MasterProblem
from .tolerances import ToleranceContext


class MasterInitialization(ABC):
    """Makes the first master LP solvable. Subclass it to do so in Python: implement
    :meth:`initialize`, called once before the first LP solve.

    A swappable component: :class:`ArtificialColumns` is the default; later alternatives
    include a two-phase method and Farkas pricing.
    """

    @abstractmethod
    def initialize(self, master: MasterProblem, context: ToleranceContext) -> None:
        """Prepares ``master``; called once, before the first LP solve.

        Args:
            master: The master, with its rows, pricers and starting columns.
            context: The cost scale of the run.
        """


class ArtificialColumns(MasterInitialization):
    """Single-phase big-M: one costly artificial column per row that ``x = 0`` violates (the
    default).

    Each artificial column alone satisfies its row (coefficient +1 when the row's lower bound
    is positive, −1 when its upper bound is negative). Pricing then brings cheaper real columns
    that push the artificial ones out. An artificial column still used once column generation
    has converged means the master is infeasible **at that cost**: a solution whose columns
    cost more than it per row they cover is missed. Without a cost, it is automatic:
    :attr:`AUTOMATIC_COST_FACTOR` times the cost scale, which is made of the pricers'
    :meth:`Pricer.cost_scale`, the starting columns' costs and the user's own variables'
    (:meth:`MasterSolver.largest_user_cost`). Give a cost explicitly, or a pricer's cost scale,
    when columns can cost more. An exact infeasibility proof needs another initialization (two
    phases, or Farkas pricing).

    The factor is 1e4: a larger artificial cost gives large duals, on which HiGHS's dual
    simplex, warm-started after columns are added, can fail. A big-M start still slows exact
    pricing, compared with a start from real columns: a tabu stage before the exact one is the
    suggested setup.

    Two of them are equal when they have the same cost, and their ``repr`` is the call that
    builds them.

    Args:
        cost: Cost of every artificial column; ``None`` for the automatic one.

    Raises:
        ValueError: When ``cost`` is not a positive finite number.
    """

    #: Factor of the automatic cost: ``AUTOMATIC_COST_FACTOR × cost_scale``.
    AUTOMATIC_COST_FACTOR: ClassVar[float] = 1e4

    def __init__(self, cost: float | None = None) -> None:
        # A free or negative artificial column would be used instead of the real ones.
        if cost is not None and not (math.isfinite(cost) and cost > 0.0):
            raise ValueError(f"ArtificialColumns: the cost must be positive and finite, got {cost}")
        self._cost = cost

    def __repr__(self) -> str:
        return "ArtificialColumns()" if self._cost is None else f"ArtificialColumns({self._cost!r})"

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, ArtificialColumns) or type(other) is not type(self):
            return NotImplemented
        return self._cost == other._cost

    def __hash__(self) -> int:
        return hash((type(self), self._cost))

    def initialize(self, master: MasterProblem, context: ToleranceContext) -> None:
        lower, upper = master.solver.row_bounds()

        # A row that x = 0 violates gets one column with the sign that satisfies it alone.
        positive = lower > 0.0
        rows = np.flatnonzero(positive | (upper < 0.0))
        if len(rows) > 0:
            master.add_artificial_columns(
                rows, np.where(positive[rows], 1.0, -1.0), self.cost(context)
            )

    def cost(self, context: ToleranceContext) -> float:
        """The cost given to artificial columns in a run with ``context``."""
        if self._cost is not None:
            return self._cost
        return self.AUTOMATIC_COST_FACTOR * context.cost_scale


class NoInitialization(MasterInitialization):
    """Leaves the master as it is: for a model that is already feasible, e.g. with starting
    columns or its own slack variables.

    Nothing is added to the user's model. If the first LP turns out to be infeasible, the run
    ends with ``ColumnGenerationStatus.RestrictedMasterInfeasible``.
    """

    def initialize(self, master: MasterProblem, context: ToleranceContext) -> None:
        pass

    def __repr__(self) -> str:
        return "NoInitialization()"

    def __eq__(self, other: object) -> bool:
        return type(other) is type(self) or NotImplemented

    def __hash__(self) -> int:
        return hash(type(self))
