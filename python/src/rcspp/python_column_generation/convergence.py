#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""What the convergence hook sees and decides."""

from dataclasses import dataclass
from enum import Enum, auto

from .master_problem import MasterProblem
from .types import FloatArray


class ConvergenceAction(Enum):
    """What the convergence hook decides."""

    #: Finish the run.
    Stop = auto()
    #: The hook changed something: solve the master and price again.
    Continue = auto()


@dataclass(frozen=True, eq=False, kw_only=True)
class ConvergenceContext:
    """What the convergence hook sees and may change: the master (its columns and pricers,
    e.g. to call :meth:`Pricer.update`), the iterations so far, the value of the converged
    master LP, its duals (one per row) and the value of every column, indexed like
    :attr:`MasterProblem.columns`."""

    master: MasterProblem
    iteration: int
    objective: float
    duals: FloatArray
    values: FloatArray
