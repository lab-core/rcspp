#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Column generation: a master problem, its pricers and the loop that runs them.

The loop runs in C++. A master solver, a pricer, tolerances or an initialization
written in Python is a subclass of :class:`MasterSolver`, :class:`Pricer`,
:class:`Tolerances` or :class:`MasterInitialization`, called by the loop with the GIL
held. :func:`master_solver_self_test` checks a master solver, e.g. one written in
Python, against an LP whose answers are known. Arrays
crossing the boundary (duals, column values, the arrays of a :class:`ColumnBatch`) are
numpy copies, so a Python object may keep them after the call. A :class:`GraphPricer`
prices a :class:`~rcspp.graph.ResourceGraph` in C++, without Python, and a
:class:`HighsMasterSolver` (when rcspp is built with HiGHS) solves the master in C++ too.
"""

from .._core.column_generation import (
    AbsoluteTolerances,
    ArtificialColumns,
    Capabilities,
    ColumnBatch,
    ColumnGeneration,
    ColumnGenerationParams,
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
from .graph_pricer import GraphPricer, PricingStage

# Built with USE_HIGHS only, as the wheel is.
try:
    from .._core.column_generation import HighsMasterSolver
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
    "HighsMasterSolver",
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
]
