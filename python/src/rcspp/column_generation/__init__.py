#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Column generation: a master problem, its pricers and the loop that runs them.

The loop runs in C++. A :class:`GraphPricer` prices a :class:`~rcspp.graph.ResourceGraph`
in C++, and a :class:`HighsMasterSolver` (when rcspp is built with HiGHS) solves the master
in C++, so a run over them never calls Python. A :class:`HighspyMasterSolver` is one over a
copy of the user's ``highspy`` model. A :class:`GurobipyMasterSolver` works on the user's
``gurobipy`` model itself, from Python. Each imports its LP solver's package when it is
used, so that importing rcspp does not load it.

A master solver, a pricer, tolerances or an initialization written in Python is a subclass
of :class:`MasterSolver`, :class:`Pricer`, :class:`Tolerances` or
:class:`MasterInitialization`, called by the loop with the GIL held.
:func:`master_solver_self_test` checks a master solver, e.g. one written in Python, against
an LP whose answers are known. Arrays crossing the boundary (duals, column values, the
arrays of a :class:`ColumnBatch`) are numpy copies, so a Python object may keep them after
the call.

Tolerances and initializations can also be given by name
(:mod:`rcspp.column_generation.registry`).
"""

from .._core.column_generation import (
    AbsoluteTolerances,
    ArtificialColumns,
    Capabilities,
    ColumnBatch,
    ColumnGeneration,
    ColumnGenerationResult,
    ColumnGenerationStatus,
    ColumnTag,
    ColumnType,
    ConvergenceAction,
    ConvergenceContext,
    ConvexityRow,
    IntegerParams,
    IterationInfo,
    MasterColumn,
    MasterInitialization,
    MasterProblem,
    MasterSolver,
    MasterSolverSelfTestReport,
    NoInitialization,
    Pricer,
    PricingChanges,
    PricingRequest,
    PricingResult,
    PricingStatus,
    RelativeTolerances,
    RelaxationParams,
    RowBatch,
    RowRange,
    RowSense,
    SolverStatus,
    Statistics,
    StatisticsParams,
    ToleranceContext,
    Tolerances,
    default_feasibility_tolerance,
    default_optimality_gap,
    default_reduced_cost_tolerance,
    default_relative_gap,
    default_self_test_tolerance,
    master_solver_self_test,
)
from . import registry
from .graph_pricer import GraphPricer, PricingStage
from .master_solvers import GurobipyMasterSolver
from .params import ColumnGenerationParams
from .registry import (
    algorithm_names,
    initialization_names,
    make_initialization,
    make_tolerances,
    register_initialization,
    register_tolerances,
    tolerance_names,
)

# Built with USE_COLUMN_GENERATION_HIGHS only, as the wheel is.
try:
    from .._core.column_generation import HighsMasterSolver
    from .master_solvers import HighspyMasterSolver
except ImportError:  # pragma: no cover
    pass

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
    "HighsMasterSolver",
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
    "SolverStatus",
    "Statistics",
    "StatisticsParams",
    "ToleranceContext",
    "Tolerances",
    "default_feasibility_tolerance",
    "default_optimality_gap",
    "default_reduced_cost_tolerance",
    "default_relative_gap",
    "default_self_test_tolerance",
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
