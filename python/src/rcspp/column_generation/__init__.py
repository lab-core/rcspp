#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Column generation in pure Python: the features of the C++ column generation example
(``cpp_column_generation``), with the loop, the master problem and the master solvers written in
Python.

The same classes as in the example, with an API written for Python: abstract base classes,
properties and frozen option records. This package imports nothing of the example. A
:class:`HighspyMasterSolver` and a :class:`GurobipyMasterSolver` work on the user's ``highspy``
or ``gurobipy`` model itself; each imports its LP solver's package when it is used, so that
importing rcspp does not load it.

A master solver or a pricer written in Python is a subclass of :class:`MasterSolver` or
:class:`Pricer`. A :class:`GraphPricer` prices a :class:`~rcspp.graph.ResourceGraph`, and a
:class:`ColumnGeneration` loop runs a :class:`MasterProblem` over its pricers and master
solver. Tolerances and initializations can also be given by name
(:mod:`rcspp.column_generation.registry`).
"""

from . import registry
from .column_generation_loop import ColumnGeneration, ColumnGenerationResult, ColumnGenerationStatus
from .convergence import ConvergenceAction, ConvergenceContext
from .graph_pricer import GraphPricer, PricingStage, Search, StageRequest
from .master_initialization import ArtificialColumns, MasterInitialization, NoInitialization
from .master_problem import (
    ColumnTag,
    ConvexityRow,
    MasterColumn,
    MasterProblem,
    PricerEntry,
    RowRange,
)
from .master_solver import MasterSolver
from .master_solver_self_test import (
    DEFAULT_SELF_TEST_TOLERANCE,
    MasterSolverSelfTestReport,
    master_solver_self_test,
)
from .master_solvers import GurobipyMasterSolver, HighspyMasterSolver
from .params import ColumnGenerationParams
from .pricer import Pricer, PricingChanges, PricingRequest, PricingResult, PricingStatus
from .registry import (
    algorithm_names,
    initialization_names,
    make_initialization,
    make_tolerances,
    register_initialization,
    register_tolerances,
    tolerance_names,
)
from .statistics import IterationInfo, Statistics, StatisticsParams
from .tolerances import (
    DEFAULT_FEASIBILITY_TOLERANCE,
    DEFAULT_OPTIMALITY_GAP,
    DEFAULT_REDUCED_COST_TOLERANCE,
    AbsoluteTolerances,
    RelativeTolerances,
    ToleranceContext,
    Tolerances,
)
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
    "AbsoluteTolerances",
    "ArtificialColumns",
    "Capabilities",
    "ColumnBatch",
    "ColumnGeneration",
    "ColumnGenerationParams",
    "ColumnGenerationResult",
    "ColumnGenerationStatus",
    "ColumnTag",
    "ColumnType",
    "ConvergenceAction",
    "ConvergenceContext",
    "ConvexityRow",
    "GraphPricer",
    "GurobipyMasterSolver",
    "HighspyMasterSolver",
    "IntegerParams",
    "IterationInfo",
    "MasterColumn",
    "MasterInitialization",
    "MasterProblem",
    "MasterSolverSelfTestReport",
    "MasterSolver",
    "NoInitialization",
    "Pricer",
    "PricerEntry",
    "PricingChanges",
    "PricingRequest",
    "PricingResult",
    "PricingStage",
    "PricingStatus",
    "RelativeTolerances",
    "RelaxationParams",
    "RowBatch",
    "RowRange",
    "RowSense",
    "Search",
    "SolverStatus",
    "StageRequest",
    "Statistics",
    "StatisticsParams",
    "ToleranceContext",
    "Tolerances",
    "DEFAULT_FEASIBILITY_TOLERANCE",
    "DEFAULT_OPTIMALITY_GAP",
    "DEFAULT_REDUCED_COST_TOLERANCE",
    "DEFAULT_RELATIVE_GAP",
    "DEFAULT_SELF_TEST_TOLERANCE",
    "master_solver_self_test",
    "algorithm_names",
    "initialization_names",
    "make_initialization",
    "make_tolerances",
    "register_initialization",
    "register_tolerances",
    "registry",
    "tolerance_names",
]
