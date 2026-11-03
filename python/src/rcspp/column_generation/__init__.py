#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Column generation in pure Python: the features of the C++ column generation example
(``cpp_column_generation``), with the loop, the master problem and the master solvers written in
Python.

The same classes as in the example, with an API written for Python: abstract base classes,
properties and frozen option records. This package imports nothing of the example. A
:class:`HighspyMasterSolver` works on the user's ``highspy`` model itself; it imports highspy
when it is used, so that importing rcspp does not load it.

A master solver or a pricer written in Python is a subclass of :class:`MasterSolver` or
:class:`Pricer`.
"""

from .master_solver import MasterSolver
from .master_solvers import HighspyMasterSolver
from .pricer import Pricer, PricingChanges, PricingRequest, PricingResult, PricingStatus
from .types import (
    DEFAULT_RELATIVE_GAP,
    Capabilities,
    ColumnBatch,
    ColumnType,
    IntegerParams,
    RelaxationParams,
    RowBatch,
    RowSense,
    SolverStatus,
)

__all__ = [
    "Capabilities",
    "ColumnBatch",
    "ColumnType",
    "HighspyMasterSolver",
    "IntegerParams",
    "MasterSolver",
    "Pricer",
    "PricingChanges",
    "PricingRequest",
    "PricingResult",
    "PricingStatus",
    "RelaxationParams",
    "RowBatch",
    "RowSense",
    "SolverStatus",
    "DEFAULT_RELATIVE_GAP",
]
