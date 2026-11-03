#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The pricer: what the loop asks of it and what it returns."""

import math
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from enum import Enum, auto

from .._core.graph import Solution
from .types import FloatArray


class PricingStatus(Enum):
    """How a pricing call ended."""

    Optimal = auto()
    SolverLimit = auto()
    Infeasible = auto()
    NotApplicable = auto()
    Unknown = auto()


@dataclass(frozen=True, kw_only=True)
class PricingRequest:
    """What the loop asks of one pricing call."""

    #: Return only columns whose reduced cost (without the convexity dual) is below this value.
    reduced_cost_threshold: float = 0.0
    timeout_s: float = math.inf


@dataclass(eq=False, kw_only=True)
class PricingResult:
    """What one pricing call returns."""

    #: The new columns; each ``Solution.column`` holds its cost and rows.
    columns: list[Solution] = field(default_factory=list[Solution])
    status: PricingStatus = PricingStatus.Unknown
    #: A lower bound on the best reduced cost (``-inf`` when unknown); not read by the loop yet.
    min_reduced_cost_bound: float = -math.inf
    #: Only an exact call that is Optimal or Infeasible, or a call answering NotApplicable,
    #: proves that no improving column exists.
    exact_stage: bool = False


@dataclass(frozen=True, kw_only=True)
class PricingChanges:
    """What changed in the pricing problem."""

    costs: bool = False
    bounds: bool = False
    structure: bool = False


class Pricer(ABC):
    """Finds columns with a negative reduced cost for one block of the master.

    Subclass it to price in Python: implement :meth:`price`, returning a
    :class:`PricingResult`; :meth:`update` and :meth:`cost_scale` are optional.
    """

    @abstractmethod
    def price(self, duals: FloatArray, request: PricingRequest = PricingRequest()) -> PricingResult:
        """Prices the master's duals: one per master row, normalized as a
        :class:`~rcspp.python_column_generation.MasterSolver` gives them.

        Args:
            duals: The duals.
            request: The threshold and the time limit.

        Returns:
            The columns found, the status and a bound.
        """

    def update(self, changes: PricingChanges) -> None:
        """Tells the pricer its problem changed, so it can refresh cached state."""

    def cost_scale(self) -> float:
        """The typical magnitude of the costs its columns are made of, the scale of
        automatic artificial costs and relative tolerances (1 by default)."""
        return 1.0
