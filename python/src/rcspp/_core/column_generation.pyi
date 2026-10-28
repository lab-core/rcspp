from __future__ import annotations

from enum import Enum
from typing import Any, Callable, Sequence, TypeAlias, overload

import numpy as np

from .graph import Algorithm, SearchDirection, Solution

# What a method receives or returns: duals, values, bounds.
FloatArray: TypeAlias = np.ndarray[Any, np.dtype[np.float64]]
# Column ids, as a Python master solver receives them in primal.
IdArray: TypeAlias = np.ndarray[Any, np.dtype[np.uint64]]
# What an argument accepts: anything numpy turns into an array of numbers.
ArrayLike: TypeAlias = Sequence[float] | np.ndarray[Any, Any]
IntArrayLike: TypeAlias = Sequence[int] | np.ndarray[Any, Any]

class SolverStatus(Enum):
    Optimal: SolverStatus
    Infeasible: SolverStatus
    Unbounded: SolverStatus
    TimeLimit: SolverStatus
    Other: SolverStatus

class RowSense(Enum):
    Equal: RowSense
    GreaterEqual: RowSense
    LessEqual: RowSense

class ColumnType(Enum):
    Integer: ColumnType
    Binary: ColumnType

class PricingStatus(Enum):
    Optimal: PricingStatus
    SolverLimit: PricingStatus
    Infeasible: PricingStatus
    NotApplicable: PricingStatus
    Unknown: PricingStatus

class ColumnTag(Enum):
    Real: ColumnTag
    Artificial: ColumnTag

class ConvergenceAction(Enum):
    Stop: ConvergenceAction
    Continue: ConvergenceAction

class ColumnGenerationStatus(Enum):
    Optimal: ColumnGenerationStatus
    NotProven: ColumnGenerationStatus
    Infeasible: ColumnGenerationStatus
    RestrictedMasterInfeasible: ColumnGenerationStatus
    Unbounded: ColumnGenerationStatus
    TimeLimit: ColumnGenerationStatus
    IterationLimit: ColumnGenerationStatus
    SolverError: ColumnGenerationStatus

default_relative_gap: float
default_reduced_cost_tolerance: float
default_feasibility_tolerance: float
default_optimality_gap: float
default_self_test_tolerance: float

class Capabilities:
    add_rows: bool
    integer_solve: bool
    farkas: bool
    basis: bool
    def __init__(
        self,
        *,
        add_rows: bool = False,
        integer_solve: bool = False,
        farkas: bool = False,
        basis: bool = False,
    ) -> None: ...

class RelaxationParams:
    timeout_s: float
    threads: int
    verbose: bool
    def __init__(
        self, *, timeout_s: float = ..., threads: int = 0, verbose: bool = False
    ) -> None: ...

class IntegerParams:
    timeout_s: float
    relative_gap: float
    column_type: ColumnType
    threads: int
    verbose: bool
    def __init__(
        self,
        *,
        timeout_s: float = ...,
        relative_gap: float = ...,
        column_type: ColumnType = ...,
        threads: int = 0,
        verbose: bool = False,
    ) -> None: ...

class RowBatch:
    """Rows to add, as ranges lower ≤ a·x ≤ upper (numpy copies)."""

    @property
    def lower(self) -> FloatArray: ...
    @property
    def upper(self) -> FloatArray: ...
    @property
    def names(self) -> list[str]: ...
    def __init__(self, lower: ArrayLike, upper: ArrayLike, names: Sequence[str] = ...) -> None: ...
    def __len__(self) -> int: ...

class ColumnBatch:
    """Columns to add, in compressed sparse column form (numpy copies).

    Column j has the entries row_indices[starts[j]:starts[j + 1]] with the matching values.
    """

    @property
    def ids(self) -> IdArray: ...
    @property
    def cost(self) -> FloatArray: ...
    @property
    def lower(self) -> FloatArray: ...
    @property
    def upper(self) -> FloatArray: ...
    @property
    def starts(self) -> np.ndarray[Any, np.dtype[np.uint32]]: ...
    @property
    def row_indices(self) -> np.ndarray[Any, np.dtype[np.uint32]]: ...
    @property
    def values(self) -> FloatArray: ...
    def __init__(
        self,
        ids: IntArrayLike,
        cost: ArrayLike,
        lower: ArrayLike,
        upper: ArrayLike,
        starts: IntArrayLike,
        row_indices: IntArrayLike,
        values: ArrayLike,
    ) -> None: ...
    def __len__(self) -> int: ...

class MasterSolver:
    """Adapter to the LP/MIP solver holding the master, seen as a minimization with normalized
    duals.

    Subclass it to use another solver from Python: an override keeps the signature below,
    defaults included, and returns numpy arrays where this class does.
    """

    def __init__(self) -> None: ...
    def number_of_rows(self) -> int: ...
    def row_bounds(self) -> tuple[FloatArray, FloatArray]: ...
    def add_rows(self, rows: RowBatch) -> None: ...
    def add_columns(self, columns: ColumnBatch) -> None: ...
    def solve_relaxation(self, params: RelaxationParams = ...) -> SolverStatus: ...
    def objective(self) -> float: ...
    def duals(self) -> FloatArray: ...
    def primal(self, ids: IdArray) -> FloatArray: ...
    def solve_integer(self, params: IntegerParams = ...) -> SolverStatus: ...
    def farkas(self) -> FloatArray | None: ...
    def set_option(self, name: str, value: bool | int | float | str) -> None: ...
    def capabilities(self) -> Capabilities: ...
    def dual_tolerance(self) -> float: ...
    def largest_user_cost(self) -> float: ...
    def restore_user_model(self) -> None: ...

class PricingRequest:
    reduced_cost_threshold: float
    timeout_s: float
    def __init__(self, *, reduced_cost_threshold: float = 0.0, timeout_s: float = ...) -> None: ...

class PricingResult:
    columns: list[Solution]
    status: PricingStatus
    min_reduced_cost_bound: float
    exact_stage: bool
    def __init__(
        self,
        *,
        columns: Sequence[Solution] = ...,
        status: PricingStatus = ...,
        min_reduced_cost_bound: float = ...,
        exact_stage: bool = False,
    ) -> None: ...

class PricingChanges:
    costs: bool
    bounds: bool
    structure: bool
    def __init__(
        self, *, costs: bool = False, bounds: bool = False, structure: bool = False
    ) -> None: ...

class Pricer:
    """Finds columns with a negative reduced cost for one block of the master.

    Subclass it to price in Python: an override keeps the signature below, defaults
    included.
    """

    def __init__(self) -> None: ...
    def price(self, duals: FloatArray, request: PricingRequest = ...) -> PricingResult: ...
    def update(self, changes: PricingChanges) -> None: ...
    def cost_scale(self) -> float: ...

class GraphPricer(Pricer):
    """A pricer over a bound resource graph, run in C++.

    Built by rcspp.column_generation.GraphPricer.
    """

    def __init__(
        self,
        graph: Any,
        stages: list[tuple[Algorithm, Any, SearchDirection, bool]],
        cost_index: int = 0,
        preprocess: bool = True,
    ) -> None: ...

class HighsMasterSolver(MasterSolver):
    """A master solver over a HiGHS model it owns, built from arrays: empty by default, or
    the user's model, its columns in compressed sparse column form.

    rows are the rows that pricing columns enter, in master row order; every row by default.
    Available when rcspp is built with HiGHS.
    """

    def __init__(
        self,
        *,
        column_cost: ArrayLike = ...,
        column_lower: ArrayLike = ...,
        column_upper: ArrayLike = ...,
        integer: Sequence[bool] | np.ndarray[Any, Any] = ...,
        row_lower: ArrayLike = ...,
        row_upper: ArrayLike = ...,
        starts: IntArrayLike = ...,
        row_indices: IntArrayLike = ...,
        values: ArrayLike = ...,
        rows: Sequence[int] | None = None,
        maximize: bool = False,
        offset: float = 0.0,
    ) -> None: ...
    def column(self, id: int) -> int: ...

class ToleranceContext:
    cost_scale: float
    def __init__(self, *, cost_scale: float = 1.0) -> None: ...

class Tolerances:
    """The tolerances of a run.

    Subclass it to compute them in Python: override the three methods below.
    """

    def __init__(self) -> None: ...
    def reduced_cost(self, context: ToleranceContext) -> float: ...
    def feasibility(self, context: ToleranceContext) -> float: ...
    def optimality_gap(self, context: ToleranceContext) -> float: ...

class AbsoluteTolerances(Tolerances):
    def __init__(
        self,
        reduced_cost: float = ...,
        feasibility: float = ...,
        optimality_gap: float = ...,
    ) -> None: ...

class RelativeTolerances(Tolerances):
    def __init__(
        self,
        reduced_cost: float = ...,
        feasibility: float = ...,
        optimality_gap: float = ...,
    ) -> None: ...

class RowRange:
    @property
    def first(self) -> int: ...
    @property
    def count(self) -> int: ...

class ConvexityRow:
    existing_row: int | None
    max_paths: float
    def __init__(self, *, existing_row: int | None = None, max_paths: float = ...) -> None: ...

class MasterColumn:
    @property
    def id(self) -> int: ...
    @property
    def pricer(self) -> int: ...
    @property
    def tag(self) -> ColumnTag: ...
    @property
    def solution(self) -> Solution: ...

class MasterProblem:
    no_pricer: int
    def __init__(self, solver: MasterSolver) -> None: ...
    @overload
    def add_rows(self, count: int, sense: RowSense, right_hand_side: float) -> RowRange: ...
    @overload
    def add_rows(
        self, lower: ArrayLike, upper: ArrayLike, names: Sequence[str] = ...
    ) -> RowRange: ...
    def add_pricer(self, pricer: Pricer, convexity: ConvexityRow = ...) -> int: ...
    def add_columns(self, pricer: int, columns: Sequence[Solution]) -> None: ...
    def add_artificial_columns(
        self, rows: Sequence[int], coefficients: ArrayLike, cost: float
    ) -> None: ...
    def number_of_rows(self) -> int: ...
    def number_of_columns(self) -> int: ...
    def number_of_pricers(self) -> int: ...
    def pricer(self, index: int) -> Pricer: ...
    def convexity_row(self, index: int) -> int | None: ...
    def columns(self) -> list[MasterColumn]: ...
    def artificial_column_ids(self) -> IdArray: ...
    def artificial_cost(self) -> float: ...
    def initialized(self) -> bool: ...
    def mark_initialized(self) -> None: ...
    def solver(self) -> MasterSolver: ...

class MasterInitialization:
    """Makes the first master LP solvable.

    Subclass it to do so in Python: override initialize, called once before the
    first LP solve.
    """

    def __init__(self) -> None: ...
    def initialize(self, master: MasterProblem, context: ToleranceContext) -> None: ...

class ArtificialColumns(MasterInitialization):
    automatic_cost_factor: float
    def __init__(self, cost: float = 0.0) -> None: ...
    def cost(self, context: ToleranceContext) -> float: ...

class NoInitialization(MasterInitialization):
    def __init__(self) -> None: ...

class StatisticsParams:
    timers: bool
    def __init__(self, *, timers: bool = True) -> None: ...

class Statistics:
    @property
    def generated_columns(self) -> int: ...
    @property
    def artificial_columns(self) -> int: ...
    @property
    def artificial_cost(self) -> float: ...
    @property
    def total_seconds(self) -> float: ...
    @property
    def relaxation_seconds(self) -> float: ...
    @property
    def pricing_seconds(self) -> float: ...

class IterationInfo:
    @property
    def iteration(self) -> int: ...
    @property
    def objective(self) -> float: ...
    @property
    def new_columns(self) -> int: ...
    @property
    def total_columns(self) -> int: ...
    @property
    def elapsed_seconds(self) -> float: ...

class ConvergenceContext:
    """What the convergence hook sees and may change; valid only during the call."""

    @property
    def master(self) -> MasterProblem: ...
    @property
    def iteration(self) -> int: ...
    @property
    def objective(self) -> float: ...
    @property
    def duals(self) -> FloatArray: ...
    @property
    def values(self) -> FloatArray: ...

class ColumnGenerationParams:
    max_iterations: int
    timeout_s: float
    relaxation: RelaxationParams
    tolerances: Tolerances | None
    initialization: MasterInitialization | None
    statistics: StatisticsParams
    on_iteration: Callable[[IterationInfo], None] | None
    on_converged: Callable[[ConvergenceContext], ConvergenceAction] | None
    def __init__(
        self,
        *,
        max_iterations: int = ...,
        timeout_s: float = ...,
        relaxation: RelaxationParams = ...,
        tolerances: Tolerances | None = None,
        initialization: MasterInitialization | None = None,
        statistics: StatisticsParams = ...,
        on_iteration: Callable[[IterationInfo], None] | None = None,
        on_converged: Callable[[ConvergenceContext], ConvergenceAction] | None = None,
    ) -> None: ...

class ColumnGenerationResult:
    @property
    def status(self) -> ColumnGenerationStatus: ...
    @property
    def objective(self) -> float: ...
    @property
    def lower_bound(self) -> float: ...
    @property
    def iterations(self) -> int: ...
    @property
    def statistics(self) -> Statistics: ...

class ColumnGeneration:
    """The column generation loop: solve the master LP, price its duals, add columns."""

    def __init__(self, master: MasterProblem, params: ColumnGenerationParams = ...) -> None: ...
    def run(self) -> ColumnGenerationResult: ...
    def solve_restricted_master_integer(self, params: IntegerParams = ...) -> SolverStatus: ...
    def params(self) -> ColumnGenerationParams: ...
    def tolerance_context(self) -> ToleranceContext: ...

class MasterSolverSelfTestReport:
    """Outcome of master_solver_self_test."""

    @property
    def failures(self) -> list[str]: ...
    def passed(self) -> bool: ...

def master_solver_self_test(
    minimizing: MasterSolver, maximizing: MasterSolver, tolerance: float = ...
) -> MasterSolverSelfTestReport:
    """Checks that a master solver follows the conventions of MasterSolver, on an LP whose
    answers are known.

    minimizing and maximizing are two master solvers of the same kind over empty models,
    able to add rows. Raises ValueError when one already has rows or cannot add rows.
    """
