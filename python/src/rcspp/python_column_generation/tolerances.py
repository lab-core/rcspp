#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The tolerances of a column generation run, as a swappable component."""

from abc import ABC, abstractmethod
from dataclasses import dataclass

#: Default reduced-cost tolerance: a column improves when its reduced cost is below minus this.
DEFAULT_REDUCED_COST_TOLERANCE = 1e-9
#: Default feasibility tolerance: smaller values count as zero.
DEFAULT_FEASIBILITY_TOLERANCE = 1e-6
#: Default gap between the bound and the LP value at which the loop may stop early (not used
#: yet: reserved for an early stop on the gap).
DEFAULT_OPTIMALITY_GAP = 1e-6


@dataclass(frozen=True, kw_only=True)
class ToleranceContext:
    """What a tolerance may depend on."""

    #: Typical magnitude of the column costs (at least 1).
    cost_scale: float = 1.0


class Tolerances(ABC):
    """The tolerances of a run. Subclass it to compute them in Python: implement
    :meth:`reduced_cost`, :meth:`feasibility` and :meth:`optimality_gap`, each taking a
    :class:`ToleranceContext`.

    The loop never uses a reduced-cost tolerance below the master solver's own
    (:meth:`MasterSolver.dual_tolerance`): the LP solver would leave a column improving by less
    out of its basis, and pricing would return it again and again. LP solvers apply their
    tolerance to a scaled model, so this is a strong guard, not a guarantee: for costs far from
    1, use :class:`RelativeTolerances` or a larger tolerance.
    """

    @abstractmethod
    def reduced_cost(self, context: ToleranceContext) -> float:
        """A column improves the master when its reduced cost is below minus this
        value."""

    @abstractmethod
    def feasibility(self, context: ToleranceContext) -> float:
        """Values below this are zero (artificial columns, integrality)."""

    @abstractmethod
    def optimality_gap(self, context: ToleranceContext) -> float:
        """Gap between the bound and the LP value at which the loop may stop early (not read
        by the loop yet: reserved for an early stop on the gap)."""


class _GivenTolerances(Tolerances):
    """Tolerances given as three numbers; the subclasses say how the context scales
    them.

    Two of them are equal when they are of the same class with the same numbers, and their
    ``repr`` is the call that builds them.
    """

    def __init__(
        self,
        reduced_cost: float = DEFAULT_REDUCED_COST_TOLERANCE,
        feasibility: float = DEFAULT_FEASIBILITY_TOLERANCE,
        optimality_gap: float = DEFAULT_OPTIMALITY_GAP,
    ) -> None:
        self._reduced_cost = reduced_cost
        self._feasibility = feasibility
        self._optimality_gap = optimality_gap

    def _numbers(self) -> tuple[float, float, float]:
        return self._reduced_cost, self._feasibility, self._optimality_gap

    def __repr__(self) -> str:
        return (
            f"{type(self).__name__}(reduced_cost={self._reduced_cost!r}, "
            f"feasibility={self._feasibility!r}, optimality_gap={self._optimality_gap!r})"
        )

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, _GivenTolerances) or type(other) is not type(self):
            return NotImplemented
        return self._numbers() == other._numbers()

    def __hash__(self) -> int:
        return hash((type(self), self._numbers()))

    def feasibility(self, context: ToleranceContext) -> float:
        return self._feasibility


class AbsoluteTolerances(_GivenTolerances):
    """Fixed tolerances, whatever the scale of the costs (the default).

    Args:
        reduced_cost: See :meth:`Tolerances.reduced_cost`.
        feasibility: See :meth:`Tolerances.feasibility`.
        optimality_gap: See :meth:`Tolerances.optimality_gap`.
    """

    def reduced_cost(self, context: ToleranceContext) -> float:
        return self._reduced_cost

    def optimality_gap(self, context: ToleranceContext) -> float:
        return self._optimality_gap


class RelativeTolerances(_GivenTolerances):
    """Tolerances on costs scaled by :attr:`ToleranceContext.cost_scale`.

    The reduced-cost and gap tolerances are multiplied by the cost scale; the feasibility
    tolerance applies to column values, not costs, so it is not scaled.

    Args:
        reduced_cost: Reduced-cost tolerance per unit of cost scale.
        feasibility: See :meth:`Tolerances.feasibility`.
        optimality_gap: Gap tolerance per unit of cost scale.
    """

    def reduced_cost(self, context: ToleranceContext) -> float:
        return self._reduced_cost * context.cost_scale

    def optimality_gap(self, context: ToleranceContext) -> float:
        return self._optimality_gap * context.cost_scale
