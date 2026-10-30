"""
Graph-related classes
"""

from __future__ import annotations

import collections.abc
import typing

import numpy
import numpy.typing

import rcspp._core.resource
from rcspp.graph import ResourceGraph

__all__: list[str] = [
    "Algorithm",
    "AlgorithmParams",
    "AlgorithmStatus",
    "Arc",
    "BidirectionalStats",
    "BucketAlgorithmParams",
    "Column",
    "Graph",
    "Node",
    "ResourceGraph",
    "Row",
    "SearchDirection",
    "Solution",
    "SolveResult",
    "check_interrupted",
]

class Solution:
    column: Column
    def __init__(self) -> None: ...
    def get_hash(self) -> int: ...
    def to_arrays(self) -> tuple:
        """
        Return (cost, node_ids, row_indices, row_coefficients) as numpy arrays. Avoids per-Row Python object overhead during column extraction.
        """

    @property
    def cost(self) -> float: ...
    @cost.setter
    def cost(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None: ...
    @property
    def path_arc_ids(self) -> list[int]: ...
    @path_arc_ids.setter
    def path_arc_ids(
        self, arg1: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> None: ...
    @property
    def path_node_ids(self) -> list[int]: ...
    @path_node_ids.setter
    def path_node_ids(
        self, arg0: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> None: ...

class Node:
    def __init__(
        self, arg0: typing.SupportsInt | typing.SupportsIndex, arg1: bool, arg2: bool
    ) -> None: ...
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    def pos(self) -> int: ...
    @property
    def id(self) -> int: ...
    @property
    def in_arcs(self) -> list[Arc]: ...
    @in_arcs.setter
    def in_arcs(self, arg0: collections.abc.Sequence[Arc]) -> None: ...
    @property
    def out_arcs(self) -> list[Arc]: ...
    @out_arcs.setter
    def out_arcs(self, arg0: collections.abc.Sequence[Arc]) -> None: ...
    @property
    def sink(self) -> bool: ...
    @property
    def source(self) -> bool: ...

class Arc:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    def destination(self) -> Node: ...
    def origin(self) -> Node: ...
    @property
    def cost(self) -> float: ...
    @cost.setter
    def cost(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None: ...
    @property
    def id(self) -> int: ...
    @property
    def rows(self) -> list[Row]: ...
    @rows.setter
    def rows(self, arg0: collections.abc.Sequence[Row]) -> None: ...

class Algorithm:
    """
    Members:

      Simple

      Pushing

      Pulling

      Greedy

      Tabu

      AStar
    """

    AStar: typing.ClassVar[Algorithm]
    Greedy: typing.ClassVar[Algorithm]
    Pulling: typing.ClassVar[Algorithm]
    Pushing: typing.ClassVar[Algorithm]
    Simple: typing.ClassVar[Algorithm]
    Tabu: typing.ClassVar[Algorithm]
    __members__: typing.ClassVar[dict[str, Algorithm]]
    def __eq__(self, other: typing.Any) -> bool: ...
    def __getstate__(self) -> int: ...
    def __hash__(self) -> int: ...
    def __index__(self) -> int: ...
    def __init__(self, value: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    def __int__(self) -> int: ...
    def __ne__(self, other: typing.Any) -> bool: ...
    def __repr__(self) -> str: ...
    def __setstate__(self, state: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    def __str__(self) -> str: ...
    @property
    def name(self) -> str: ...
    @property
    def value(self) -> int: ...

class SearchDirection:
    """
    Which way a search runs: Forward from the sources, Backward from the sinks, or Bidirectional (both at once, joined in the middle). Only the simple algorithm supports Backward and Bidirectional.

    Members:

      Forward

      Backward

      Bidirectional
    """

    Backward: typing.ClassVar[SearchDirection]
    Bidirectional: typing.ClassVar[SearchDirection]
    Forward: typing.ClassVar[SearchDirection]
    __members__: typing.ClassVar[dict[str, SearchDirection]]
    def __eq__(self, other: typing.Any) -> bool: ...
    def __getstate__(self) -> int: ...
    def __hash__(self) -> int: ...
    def __index__(self) -> int: ...
    def __init__(self, value: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    def __int__(self) -> int: ...
    def __ne__(self, other: typing.Any) -> bool: ...
    def __repr__(self) -> str: ...
    def __setstate__(self, state: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    def __str__(self) -> str: ...
    @property
    def name(self) -> str: ...
    @property
    def value(self) -> int: ...

class AlgorithmStatus:
    """
    Members:

      Complete

      Timeout

      MaxSolutions

      Truncated

      Interrupted

      MemoryLimit
    """

    Complete: typing.ClassVar[AlgorithmStatus]
    Interrupted: typing.ClassVar[AlgorithmStatus]
    MaxSolutions: typing.ClassVar[AlgorithmStatus]
    MemoryLimit: typing.ClassVar[AlgorithmStatus]
    Timeout: typing.ClassVar[AlgorithmStatus]
    Truncated: typing.ClassVar[AlgorithmStatus]
    __members__: typing.ClassVar[dict[str, AlgorithmStatus]]
    def __eq__(self, other: typing.Any) -> bool: ...
    def __getstate__(self) -> int: ...
    def __hash__(self) -> int: ...
    def __index__(self) -> int: ...
    def __init__(self, value: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    def __int__(self) -> int: ...
    def __ne__(self, other: typing.Any) -> bool: ...
    def __repr__(self) -> str: ...
    def __setstate__(self, state: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    def __str__(self) -> str: ...
    @property
    def name(self) -> str: ...
    @property
    def value(self) -> int: ...

class BidirectionalStats:
    """
    What a bidirectional solve reports: whether the half-way bound was in force, why it was off if it was, and how many paths the join produced. Read it from SolveResult.bidirectional.
    """

    def __repr__(self) -> str: ...
    @property
    def half_way_off_reason(self) -> str:
        """
        Why the half-way bound was off for this solve; empty when it was in force.
        """

    @property
    def half_way_used(self) -> bool:
        """
        Whether the half-way bound was in force. When it is off (see half_way_off_reason), both searches run in full and the solve is slower than a forward one, so check this rather than assume it.
        """

    @property
    def joined_paths(self) -> int:
        """
        How many distinct complete paths came only from the join, that is, paths neither search completed on its own. Zero means every solution came from the forward search reaching a sink or the backward one reaching a source.
        """

class SolveResult:
    status: AlgorithmStatus
    def __bool__(self) -> bool: ...
    def __getitem__(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> Solution: ...
    def __init__(self) -> None: ...
    def __iter__(self) -> collections.abc.Iterator[Solution]: ...
    def __len__(self) -> int: ...
    def __repr__(self) -> str: ...
    def status_string(self) -> str: ...
    @property
    def bidirectional(self) -> BidirectionalStats | None:
        """
        Statistics of a bidirectional solve (a BidirectionalStats): whether the half-way bound was in force, and what the join produced. None unless the search ran bidirectionally.
        """

    @property
    def memory_pressure_triggered(self) -> bool:
        """
        Whether memory pressure dropped labels during this solve. If so, the result may not be optimal, even when status is 'complete': 'complete' then means only that the labels that were kept were all processed. Reported by every algorithm.
        """

    @property
    def solutions(self) -> list[Solution]: ...
    @solutions.setter
    def solutions(self, arg0: collections.abc.Sequence[Solution]) -> None: ...

class Row:
    @typing.overload
    def __init__(self) -> None: ...
    @typing.overload
    def __init__(
        self,
        index: typing.SupportsInt | typing.SupportsIndex,
        coefficient: typing.SupportsFloat | typing.SupportsIndex,
    ) -> None: ...
    @property
    def coefficient(self) -> float: ...
    @coefficient.setter
    def coefficient(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None: ...
    @property
    def index(self) -> int: ...
    @index.setter
    def index(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None: ...

class AlgorithmParams:
    prune_by_incumbent: bool
    return_dominated_solutions: bool
    tabu_random_noise: bool
    def __init__(self) -> None: ...
    def check(self) -> None: ...
    def could_be_non_optimal(self) -> bool: ...
    @property
    def critical_resource_index(self) -> int:
        """
        Which resource is the bidirectional clock, given as its index among the resources of the cost's type (e.g. 1 for the second 'real' resource). The clock must never decrease along an arc and must be a time window or a capacity, not a plain sum and never the cost. If it is not, the half-way bound turns itself off: the answer stays correct, but the solve is slower. Used only by a bidirectional search (default 0).
        """

    @critical_resource_index.setter
    def critical_resource_index(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    @property
    def direction(self) -> SearchDirection:
        """
        Which way the search runs: SearchDirection.Forward (default), Backward or Bidirectional. Only the simple algorithm searches backward or bidirectionally; the others raise ValueError. ResourceGraph.solve's `direction` argument overrides it for that call only.
        """

    @direction.setter
    def direction(self, arg0: SearchDirection) -> None: ...
    @property
    def half_way_point(self) -> float:
        """
        The clock value H where the two searches meet (the clock is chosen by critical_resource_index). The forward search drops labels whose clock is above H, the backward search drops those below H, and the join combines the rest. Set H to about half the clock's largest value. 0 (the default) turns this bound off; it does not choose one. Both searches then run in full, which is correct but slower than a forward solve. result.bidirectional.half_way_used says whether it was in force.
        """

    @half_way_point.setter
    def half_way_point(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None: ...
    @property
    def join_after_early_stop(self) -> bool:
        """
        Bidirectional only: whether the join still runs after a timeout or the memory limit (default True). An interrupt always skips it.
        """

    @join_after_early_stop.setter
    def join_after_early_stop(self, arg0: bool) -> None: ...
    @property
    def limit_to_available_ram(self) -> bool:
        """
        Derive limit from currently-available system RAM.
        """

    @limit_to_available_ram.setter
    def limit_to_available_ram(self, arg0: bool) -> None: ...
    @property
    def limit_to_total_ram(self) -> bool:
        """
        Derive limit from total physical RAM.
        """

    @limit_to_total_ram.setter
    def limit_to_total_ram(self, arg0: bool) -> None: ...
    @property
    def max_extended_labels_per_node(self) -> int: ...
    @max_extended_labels_per_node.setter
    def max_extended_labels_per_node(
        self, arg0: typing.SupportsInt | typing.SupportsIndex
    ) -> None: ...
    @property
    def max_iterations(self) -> int: ...
    @max_iterations.setter
    def max_iterations(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    @property
    def max_memory_gb(self) -> float:
        """
        Hard cap on process RSS in GiB (0 = unlimited). The solver stops early and returns whatever solutions have been found.
        """

    @max_memory_gb.setter
    def max_memory_gb(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None: ...
    @property
    def max_phases(self) -> int: ...
    @max_phases.setter
    def max_phases(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    @property
    def max_solutions(self) -> int: ...
    @max_solutions.setter
    def max_solutions(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    @property
    def memory_check_interval(self) -> int:
        """
        Main-loop iterations between RSS checks (default 50 000).
        """

    @memory_check_interval.setter
    def memory_check_interval(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    @property
    def memory_limit_fraction(self) -> float:
        """
        Fraction of RAM to use as limit (default 0.9).
        """

    @memory_limit_fraction.setter
    def memory_limit_fraction(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None: ...
    @property
    def memory_pressure_fraction(self) -> float:
        """
        RSS/limit fraction that triggers queue pruning (default 0.8).
        """

    @memory_pressure_fraction.setter
    def memory_pressure_fraction(
        self, arg0: typing.SupportsFloat | typing.SupportsIndex
    ) -> None: ...
    @property
    def memory_pressure_max_labels_per_node(self) -> int:
        """
        Max labels per node when under memory pressure (default 200).
        """

    @memory_pressure_max_labels_per_node.setter
    def memory_pressure_max_labels_per_node(
        self, arg0: typing.SupportsInt | typing.SupportsIndex
    ) -> None: ...
    @property
    def release_after_solve(self) -> bool:
        """
        If true (default), release label memory after solve(). Set to false when the same algorithm is called repeatedly in a tight loop to avoid shrink_to_fit() overhead.
        """

    @release_after_solve.setter
    def release_after_solve(self, arg0: bool) -> None: ...
    @property
    def seed(self) -> int: ...
    @seed.setter
    def seed(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    @property
    def tabu_exempt_node_ids(self) -> set[int]: ...
    @tabu_exempt_node_ids.setter
    def tabu_exempt_node_ids(
        self, arg0: collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]
    ) -> None: ...
    @property
    def tabu_tenure(self) -> int: ...
    @tabu_tenure.setter
    def tabu_tenure(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    @property
    def timeout_s(self) -> float:
        """
        Wall-clock timeout in seconds; solve() returns early when elapsed >= timeout_s (default: inf).
        """

    @timeout_s.setter
    def timeout_s(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None: ...
    @property
    def tolerance(self) -> float:
        """
        Numerical tolerance for cost comparisons (default 1e-9).
        """

    @tolerance.setter
    def tolerance(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None: ...

class BucketAlgorithmParams(AlgorithmParams):
    bucket_resource_type: str
    def __init__(self) -> None: ...
    @property
    def bucket_resource_index(self) -> int: ...
    @bucket_resource_index.setter
    def bucket_resource_index(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    @property
    def bucket_width(self) -> int: ...
    @bucket_width.setter
    def bucket_width(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    @property
    def sort_resource_index(self) -> int: ...
    @sort_resource_index.setter
    def sort_resource_index(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None: ...

class Column:
    def __init__(self) -> None: ...
    def to_arrays(self) -> tuple:
        """
        Return (cost, row_indices, row_coefficients) as numpy arrays (int64 indices, float64 coefficients) — avoids per-Row Python overhead.
        """

    @property
    def cost(self) -> float: ...
    @cost.setter
    def cost(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None: ...
    @property
    def rows(self) -> list[Row]: ...
    @rows.setter
    def rows(self, arg0: collections.abc.Sequence[Row]) -> None: ...

class Graph:
    def __init__(self) -> None: ...
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    def _add_rows_bulk(self, rows: typing.Annotated[numpy.typing.ArrayLike, numpy.float64]) -> None:
        """
        Bulk-append rows from a (N, 3) float64 array [arc_id, row_index, coeff]. The array must be sorted by arc_id (column 0).
        """

    def add_arc(
        self,
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
        cost: typing.SupportsFloat | typing.SupportsIndex = 0.0,
        rows: collections.abc.Sequence[Row] = [],
    ) -> Arc: ...
    def add_node(
        self,
        node_id: typing.SupportsInt | typing.SupportsIndex,
        source: bool = False,
        sink: bool = False,
    ) -> Node: ...
    def add_rows_to_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex, rows: collections.abc.Sequence[Row]
    ) -> bool:
        """
        Append rows to an arc's rows. Returns false if arc_id is invalid.
        """

    def arc_ids(self) -> list[int]: ...
    @typing.overload
    def force_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> list[int]:
        """
        Remove all other out-arcs from the arc's origin and all other in-arcs to its destination. Returns the ids of the removed arcs.
        """

    @typing.overload
    def force_arc(self, arc: Arc) -> list[int]:
        """
        Remove all other out-arcs from the arc's origin and all other in-arcs to its destination. Returns the ids of the removed arcs.
        """

    def get_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> Arc: ...
    def get_arcs(
        self,
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
    ) -> list[Arc]: ...
    def get_node(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> Node: ...
    def get_removed_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> Arc: ...
    def is_sink(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    def is_source(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    def next_arc_id(self) -> int:
        """
        Return the next arc ID that will be assigned by add_arc().
        """

    def node_ids(self) -> list[int]: ...
    def number_of_arcs(self) -> int: ...
    def number_of_nodes(self) -> int: ...
    @typing.overload
    def remove_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    @typing.overload
    def remove_arc(self, arc: Arc) -> bool: ...
    def remove_arcs(
        self, arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> list[int]: ...
    def remove_arcs_if(self, pred: collections.abc.Callable) -> list[int]: ...
    def removed_arc_ids(self) -> list[int]: ...
    def reserve(
        self,
        n_nodes: typing.SupportsInt | typing.SupportsIndex,
        n_arcs: typing.SupportsInt | typing.SupportsIndex,
    ) -> None:
        """
        Pre-allocate hash-map buckets to avoid rehashing during bulk inserts.
        """

    @typing.overload
    def restore_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    @typing.overload
    def restore_arc(self, arc: Arc) -> bool: ...
    def restore_arcs(
        self, arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> list[int]: ...
    def restore_arcs_if(self, pred: collections.abc.Callable) -> list[int]: ...
    def sink_node_ids(self) -> list[int]: ...
    @typing.overload
    def sort_nodes(self) -> None: ...
    @typing.overload
    def sort_nodes(self, comp: collections.abc.Callable) -> None: ...
    def source_node_ids(self) -> list[int]: ...
    def to_string(self, print_arcs: bool = False) -> str: ...

class _real_resource_graph(Graph):
    def __init__(self) -> None: ...
    def _add_arcs_bulk(
        self,
        consumptions: collections.abc.Sequence[
            tuple[collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]]]
        ],
        origin_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex],
        destination_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex],
        costs: collections.abc.Sequence[typing.SupportsFloat | typing.SupportsIndex],
        rows: collections.abc.Sequence[collections.abc.Sequence[Row]],
    ) -> None: ...
    def _add_nodes_bulk(
        self,
        nodes: collections.abc.Sequence[
            tuple[typing.SupportsInt | typing.SupportsIndex, bool, bool]
        ],
    ) -> None: ...
    def add_arc(
        self,
        resource_consumption: tuple[
            collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]]
        ],
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
        cost: typing.SupportsFloat | typing.SupportsIndex = 0.0,
        rows: collections.abc.Sequence[Row] = [],
    ) -> Arc: ...
    def add_node(
        self,
        node_id: typing.SupportsInt | typing.SupportsIndex,
        source: bool = False,
        sink: bool = False,
    ) -> Node: ...
    def add_real_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_real,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_real,
        cost_function: rcspp._core.resource.CostFunction_real,
        dominance_function: rcspp._core.resource.DominanceFunction_real,
    ) -> None: ...
    def check_model(self, direction: SearchDirection = SearchDirection.Bidirectional) -> list[str]:
        """
        Run, without solving, the model checks that a search in `direction` needs. Returns one string per problem found; empty when the model is ready for that search.
        """

    def clone(
        self, include_rows: bool = True, clone_removed_arcs: bool = False
    ) -> _real_resource_graph:
        """
        Clone this ResourceGraph. Arc IDs are stable across the clone. The clone has an independent remove/restore state.
        """

    def is_connected(
        self,
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
    ) -> bool: ...
    def remove_infeasible_arcs(self) -> None: ...
    @typing.overload
    def solve(
        self,
        algorithm: Algorithm,
        upper_bound: typing.SupportsFloat | typing.SupportsIndex,
        params: BucketAlgorithmParams,
        preprocess: bool = True,
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> SolveResult: ...
    @typing.overload
    def solve(
        self,
        algorithm: Algorithm = Algorithm.Simple,
        upper_bound: typing.SupportsFloat | typing.SupportsIndex = ...,
        params: AlgorithmParams = ...,
        preprocess: bool = True,
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> SolveResult: ...
    def update_arc(
        self,
        arc: Arc,
        resource_consumption: tuple[
            collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]]
        ],
        cost: typing.SupportsFloat | typing.SupportsIndex | None = None,
    ) -> None: ...
    def update_reduced_costs(
        self,
        duals: typing.Annotated[numpy.typing.ArrayLike, numpy.float64],
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> None: ...

class _real_int_node:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    @property
    def id(self) -> int: ...
    @property
    def sink(self) -> bool: ...
    @property
    def source(self) -> bool: ...

class _real_int_arc:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    def destination(self) -> _real_int_node: ...
    def origin(self) -> _real_int_node: ...
    @property
    def cost(self) -> float: ...
    @cost.setter
    def cost(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None: ...
    @property
    def id(self) -> int: ...
    @property
    def rows(self) -> list[Row]: ...
    @rows.setter
    def rows(self, arg0: collections.abc.Sequence[Row]) -> None: ...

class _real_int_graph:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    def _add_rows_bulk(self, rows: typing.Annotated[numpy.typing.ArrayLike, numpy.float64]) -> None:
        """
        Bulk-append rows from a (N, 3) float64 array [arc_id, row_index, coeff]. The array must be sorted by arc_id (column 0).
        """

    def add_rows_to_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex, rows: collections.abc.Sequence[Row]
    ) -> bool:
        """
        Append rows to an arc's rows. Returns false if arc_id is invalid.
        """

    def arc_ids(self) -> list[int]: ...
    @typing.overload
    def force_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> list[int]:
        """
        Remove all other out-arcs from the arc's origin and all other in-arcs to its destination. Returns the ids of the removed arcs.
        """

    @typing.overload
    def force_arc(self, arc: _real_int_arc) -> list[int]:
        """
        Remove all other out-arcs from the arc's origin and all other in-arcs to its destination. Returns the ids of the removed arcs.
        """

    def get_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> _real_int_arc: ...
    def get_arcs(
        self,
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
    ) -> list[_real_int_arc]: ...
    def get_node(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> _real_int_node: ...
    def get_removed_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex
    ) -> _real_int_arc: ...
    def is_sink(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    def is_source(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    def next_arc_id(self) -> int:
        """
        Return the next arc ID that will be assigned by add_arc().
        """

    def node_ids(self) -> list[int]: ...
    def number_of_arcs(self) -> int: ...
    def number_of_nodes(self) -> int: ...
    @typing.overload
    def remove_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    @typing.overload
    def remove_arc(self, arc: _real_int_arc) -> bool: ...
    def remove_arcs(
        self, arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> list[int]: ...
    def remove_arcs_if(self, pred: collections.abc.Callable) -> list[int]: ...
    def removed_arc_ids(self) -> list[int]: ...
    def reserve(
        self,
        n_nodes: typing.SupportsInt | typing.SupportsIndex,
        n_arcs: typing.SupportsInt | typing.SupportsIndex,
    ) -> None:
        """
        Pre-allocate hash-map buckets to avoid rehashing during bulk inserts.
        """

    @typing.overload
    def restore_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    @typing.overload
    def restore_arc(self, arc: _real_int_arc) -> bool: ...
    def restore_arcs(
        self, arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> list[int]: ...
    def restore_arcs_if(self, pred: collections.abc.Callable) -> list[int]: ...
    def sink_node_ids(self) -> list[int]: ...
    @typing.overload
    def sort_nodes(self) -> None: ...
    @typing.overload
    def sort_nodes(self, comp: collections.abc.Callable) -> None: ...
    def source_node_ids(self) -> list[int]: ...
    def to_string(self, print_arcs: bool = False) -> str: ...

class _real_int_resource_graph(_real_int_graph):
    def __init__(self) -> None: ...
    def _add_arcs_bulk(
        self,
        consumptions: collections.abc.Sequence[
            tuple[
                collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
                collections.abc.Sequence[tuple[typing.SupportsInt | typing.SupportsIndex]],
            ]
        ],
        origin_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex],
        destination_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex],
        costs: collections.abc.Sequence[typing.SupportsFloat | typing.SupportsIndex],
        rows: collections.abc.Sequence[collections.abc.Sequence[Row]],
    ) -> None: ...
    def _add_nodes_bulk(
        self,
        nodes: collections.abc.Sequence[
            tuple[typing.SupportsInt | typing.SupportsIndex, bool, bool]
        ],
    ) -> None: ...
    def add_arc(
        self,
        resource_consumption: tuple[
            collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
            collections.abc.Sequence[tuple[typing.SupportsInt | typing.SupportsIndex]],
        ],
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
        cost: typing.SupportsFloat | typing.SupportsIndex = 0.0,
        rows: collections.abc.Sequence[Row] = [],
    ) -> _real_int_arc: ...
    def add_int_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_int,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_int,
        cost_function: rcspp._core.resource.CostFunction_int,
        dominance_function: rcspp._core.resource.DominanceFunction_int,
    ) -> None: ...
    def add_node(
        self,
        node_id: typing.SupportsInt | typing.SupportsIndex,
        source: bool = False,
        sink: bool = False,
    ) -> _real_int_node: ...
    def add_real_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_real,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_real,
        cost_function: rcspp._core.resource.CostFunction_real,
        dominance_function: rcspp._core.resource.DominanceFunction_real,
    ) -> None: ...
    def check_model(self, direction: SearchDirection = SearchDirection.Bidirectional) -> list[str]:
        """
        Run, without solving, the model checks that a search in `direction` needs. Returns one string per problem found; empty when the model is ready for that search.
        """

    def clone(
        self, include_rows: bool = True, clone_removed_arcs: bool = False
    ) -> _real_int_resource_graph:
        """
        Clone this ResourceGraph. Arc IDs are stable across the clone. The clone has an independent remove/restore state.
        """

    def is_connected(
        self,
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
    ) -> bool: ...
    def remove_infeasible_arcs(self) -> None: ...
    @typing.overload
    def solve(
        self,
        algorithm: Algorithm,
        upper_bound: typing.SupportsFloat | typing.SupportsIndex,
        params: BucketAlgorithmParams,
        preprocess: bool = True,
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> SolveResult: ...
    @typing.overload
    def solve(
        self,
        algorithm: Algorithm = Algorithm.Simple,
        upper_bound: typing.SupportsFloat | typing.SupportsIndex = ...,
        params: AlgorithmParams = ...,
        preprocess: bool = True,
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> SolveResult: ...
    def update_arc(
        self,
        arc: _real_int_arc,
        resource_consumption: tuple[
            collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
            collections.abc.Sequence[tuple[typing.SupportsInt | typing.SupportsIndex]],
        ],
        cost: typing.SupportsFloat | typing.SupportsIndex | None = None,
    ) -> None: ...
    def update_reduced_costs(
        self,
        duals: typing.Annotated[numpy.typing.ArrayLike, numpy.float64],
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> None: ...

class _real_real_set_node:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    @property
    def id(self) -> int: ...
    @property
    def sink(self) -> bool: ...
    @property
    def source(self) -> bool: ...

class _real_real_set_arc:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    def destination(self) -> _real_real_set_node: ...
    def origin(self) -> _real_real_set_node: ...
    @property
    def cost(self) -> float: ...
    @cost.setter
    def cost(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None: ...
    @property
    def id(self) -> int: ...
    @property
    def rows(self) -> list[Row]: ...
    @rows.setter
    def rows(self, arg0: collections.abc.Sequence[Row]) -> None: ...

class _real_real_set_graph:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    def _add_rows_bulk(self, rows: typing.Annotated[numpy.typing.ArrayLike, numpy.float64]) -> None:
        """
        Bulk-append rows from a (N, 3) float64 array [arc_id, row_index, coeff]. The array must be sorted by arc_id (column 0).
        """

    def add_rows_to_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex, rows: collections.abc.Sequence[Row]
    ) -> bool:
        """
        Append rows to an arc's rows. Returns false if arc_id is invalid.
        """

    def arc_ids(self) -> list[int]: ...
    @typing.overload
    def force_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> list[int]:
        """
        Remove all other out-arcs from the arc's origin and all other in-arcs to its destination. Returns the ids of the removed arcs.
        """

    @typing.overload
    def force_arc(self, arc: _real_real_set_arc) -> list[int]:
        """
        Remove all other out-arcs from the arc's origin and all other in-arcs to its destination. Returns the ids of the removed arcs.
        """

    def get_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> _real_real_set_arc: ...
    def get_arcs(
        self,
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
    ) -> list[_real_real_set_arc]: ...
    def get_node(
        self, node_id: typing.SupportsInt | typing.SupportsIndex
    ) -> _real_real_set_node: ...
    def get_removed_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex
    ) -> _real_real_set_arc: ...
    def is_sink(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    def is_source(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    def next_arc_id(self) -> int:
        """
        Return the next arc ID that will be assigned by add_arc().
        """

    def node_ids(self) -> list[int]: ...
    def number_of_arcs(self) -> int: ...
    def number_of_nodes(self) -> int: ...
    @typing.overload
    def remove_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    @typing.overload
    def remove_arc(self, arc: _real_real_set_arc) -> bool: ...
    def remove_arcs(
        self, arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> list[int]: ...
    def remove_arcs_if(self, pred: collections.abc.Callable) -> list[int]: ...
    def removed_arc_ids(self) -> list[int]: ...
    def reserve(
        self,
        n_nodes: typing.SupportsInt | typing.SupportsIndex,
        n_arcs: typing.SupportsInt | typing.SupportsIndex,
    ) -> None:
        """
        Pre-allocate hash-map buckets to avoid rehashing during bulk inserts.
        """

    @typing.overload
    def restore_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    @typing.overload
    def restore_arc(self, arc: _real_real_set_arc) -> bool: ...
    def restore_arcs(
        self, arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> list[int]: ...
    def restore_arcs_if(self, pred: collections.abc.Callable) -> list[int]: ...
    def sink_node_ids(self) -> list[int]: ...
    @typing.overload
    def sort_nodes(self) -> None: ...
    @typing.overload
    def sort_nodes(self, comp: collections.abc.Callable) -> None: ...
    def source_node_ids(self) -> list[int]: ...
    def to_string(self, print_arcs: bool = False) -> str: ...

class _real_real_set_resource_graph(_real_real_set_graph):
    def __init__(self) -> None: ...
    def _add_arcs_bulk(
        self,
        consumptions: collections.abc.Sequence[
            tuple[
                collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
                collections.abc.Sequence[
                    tuple[collections.abc.Set[typing.SupportsFloat | typing.SupportsIndex]]
                ],
            ]
        ],
        origin_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex],
        destination_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex],
        costs: collections.abc.Sequence[typing.SupportsFloat | typing.SupportsIndex],
        rows: collections.abc.Sequence[collections.abc.Sequence[Row]],
    ) -> None: ...
    def _add_nodes_bulk(
        self,
        nodes: collections.abc.Sequence[
            tuple[typing.SupportsInt | typing.SupportsIndex, bool, bool]
        ],
    ) -> None: ...
    def add_arc(
        self,
        resource_consumption: tuple[
            collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
            collections.abc.Sequence[
                tuple[collections.abc.Set[typing.SupportsFloat | typing.SupportsIndex]]
            ],
        ],
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
        cost: typing.SupportsFloat | typing.SupportsIndex = 0.0,
        rows: collections.abc.Sequence[Row] = [],
    ) -> _real_real_set_arc: ...
    def add_node(
        self,
        node_id: typing.SupportsInt | typing.SupportsIndex,
        source: bool = False,
        sink: bool = False,
    ) -> _real_real_set_node: ...
    def add_real_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_real,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_real,
        cost_function: rcspp._core.resource.CostFunction_real,
        dominance_function: rcspp._core.resource.DominanceFunction_real,
    ) -> None: ...
    def add_real_set_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_real_set,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_real_set,
        cost_function: rcspp._core.resource.CostFunction_real_set,
        dominance_function: rcspp._core.resource.DominanceFunction_real_set,
    ) -> None: ...
    def check_model(self, direction: SearchDirection = SearchDirection.Bidirectional) -> list[str]:
        """
        Run, without solving, the model checks that a search in `direction` needs. Returns one string per problem found; empty when the model is ready for that search.
        """

    def clone(
        self, include_rows: bool = True, clone_removed_arcs: bool = False
    ) -> _real_real_set_resource_graph:
        """
        Clone this ResourceGraph. Arc IDs are stable across the clone. The clone has an independent remove/restore state.
        """

    def is_connected(
        self,
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
    ) -> bool: ...
    def remove_infeasible_arcs(self) -> None: ...
    @typing.overload
    def solve(
        self,
        algorithm: Algorithm,
        upper_bound: typing.SupportsFloat | typing.SupportsIndex,
        params: BucketAlgorithmParams,
        preprocess: bool = True,
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> SolveResult: ...
    @typing.overload
    def solve(
        self,
        algorithm: Algorithm = Algorithm.Simple,
        upper_bound: typing.SupportsFloat | typing.SupportsIndex = ...,
        params: AlgorithmParams = ...,
        preprocess: bool = True,
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> SolveResult: ...
    def update_arc(
        self,
        arc: _real_real_set_arc,
        resource_consumption: tuple[
            collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
            collections.abc.Sequence[
                tuple[collections.abc.Set[typing.SupportsFloat | typing.SupportsIndex]]
            ],
        ],
        cost: typing.SupportsFloat | typing.SupportsIndex | None = None,
    ) -> None: ...
    def update_reduced_costs(
        self,
        duals: typing.Annotated[numpy.typing.ArrayLike, numpy.float64],
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> None: ...

class _real_int_set_node:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    @property
    def id(self) -> int: ...
    @property
    def sink(self) -> bool: ...
    @property
    def source(self) -> bool: ...

class _real_int_set_arc:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    def destination(self) -> _real_int_set_node: ...
    def origin(self) -> _real_int_set_node: ...
    @property
    def cost(self) -> float: ...
    @cost.setter
    def cost(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None: ...
    @property
    def id(self) -> int: ...
    @property
    def rows(self) -> list[Row]: ...
    @rows.setter
    def rows(self, arg0: collections.abc.Sequence[Row]) -> None: ...

class _real_int_set_graph:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    def _add_rows_bulk(self, rows: typing.Annotated[numpy.typing.ArrayLike, numpy.float64]) -> None:
        """
        Bulk-append rows from a (N, 3) float64 array [arc_id, row_index, coeff]. The array must be sorted by arc_id (column 0).
        """

    def add_rows_to_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex, rows: collections.abc.Sequence[Row]
    ) -> bool:
        """
        Append rows to an arc's rows. Returns false if arc_id is invalid.
        """

    def arc_ids(self) -> list[int]: ...
    @typing.overload
    def force_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> list[int]:
        """
        Remove all other out-arcs from the arc's origin and all other in-arcs to its destination. Returns the ids of the removed arcs.
        """

    @typing.overload
    def force_arc(self, arc: _real_int_set_arc) -> list[int]:
        """
        Remove all other out-arcs from the arc's origin and all other in-arcs to its destination. Returns the ids of the removed arcs.
        """

    def get_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> _real_int_set_arc: ...
    def get_arcs(
        self,
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
    ) -> list[_real_int_set_arc]: ...
    def get_node(
        self, node_id: typing.SupportsInt | typing.SupportsIndex
    ) -> _real_int_set_node: ...
    def get_removed_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex
    ) -> _real_int_set_arc: ...
    def is_sink(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    def is_source(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    def next_arc_id(self) -> int:
        """
        Return the next arc ID that will be assigned by add_arc().
        """

    def node_ids(self) -> list[int]: ...
    def number_of_arcs(self) -> int: ...
    def number_of_nodes(self) -> int: ...
    @typing.overload
    def remove_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    @typing.overload
    def remove_arc(self, arc: _real_int_set_arc) -> bool: ...
    def remove_arcs(
        self, arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> list[int]: ...
    def remove_arcs_if(self, pred: collections.abc.Callable) -> list[int]: ...
    def removed_arc_ids(self) -> list[int]: ...
    def reserve(
        self,
        n_nodes: typing.SupportsInt | typing.SupportsIndex,
        n_arcs: typing.SupportsInt | typing.SupportsIndex,
    ) -> None:
        """
        Pre-allocate hash-map buckets to avoid rehashing during bulk inserts.
        """

    @typing.overload
    def restore_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    @typing.overload
    def restore_arc(self, arc: _real_int_set_arc) -> bool: ...
    def restore_arcs(
        self, arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> list[int]: ...
    def restore_arcs_if(self, pred: collections.abc.Callable) -> list[int]: ...
    def sink_node_ids(self) -> list[int]: ...
    @typing.overload
    def sort_nodes(self) -> None: ...
    @typing.overload
    def sort_nodes(self, comp: collections.abc.Callable) -> None: ...
    def source_node_ids(self) -> list[int]: ...
    def to_string(self, print_arcs: bool = False) -> str: ...

class _real_int_set_resource_graph(_real_int_set_graph):
    def __init__(self) -> None: ...
    def _add_arcs_bulk(
        self,
        consumptions: collections.abc.Sequence[
            tuple[
                collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
                collections.abc.Sequence[
                    tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
                ],
            ]
        ],
        origin_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex],
        destination_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex],
        costs: collections.abc.Sequence[typing.SupportsFloat | typing.SupportsIndex],
        rows: collections.abc.Sequence[collections.abc.Sequence[Row]],
    ) -> None: ...
    def _add_nodes_bulk(
        self,
        nodes: collections.abc.Sequence[
            tuple[typing.SupportsInt | typing.SupportsIndex, bool, bool]
        ],
    ) -> None: ...
    def add_arc(
        self,
        resource_consumption: tuple[
            collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
            collections.abc.Sequence[
                tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
            ],
        ],
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
        cost: typing.SupportsFloat | typing.SupportsIndex = 0.0,
        rows: collections.abc.Sequence[Row] = [],
    ) -> _real_int_set_arc: ...
    def add_int_set_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_int_set,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_int_set,
        cost_function: rcspp._core.resource.CostFunction_int_set,
        dominance_function: rcspp._core.resource.DominanceFunction_int_set,
    ) -> None: ...
    def add_node(
        self,
        node_id: typing.SupportsInt | typing.SupportsIndex,
        source: bool = False,
        sink: bool = False,
    ) -> _real_int_set_node: ...
    def add_real_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_real,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_real,
        cost_function: rcspp._core.resource.CostFunction_real,
        dominance_function: rcspp._core.resource.DominanceFunction_real,
    ) -> None: ...
    def check_model(self, direction: SearchDirection = SearchDirection.Bidirectional) -> list[str]:
        """
        Run, without solving, the model checks that a search in `direction` needs. Returns one string per problem found; empty when the model is ready for that search.
        """

    def clone(
        self, include_rows: bool = True, clone_removed_arcs: bool = False
    ) -> _real_int_set_resource_graph:
        """
        Clone this ResourceGraph. Arc IDs are stable across the clone. The clone has an independent remove/restore state.
        """

    def is_connected(
        self,
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
    ) -> bool: ...
    def remove_infeasible_arcs(self) -> None: ...
    @typing.overload
    def solve(
        self,
        algorithm: Algorithm,
        upper_bound: typing.SupportsFloat | typing.SupportsIndex,
        params: BucketAlgorithmParams,
        preprocess: bool = True,
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> SolveResult: ...
    @typing.overload
    def solve(
        self,
        algorithm: Algorithm = Algorithm.Simple,
        upper_bound: typing.SupportsFloat | typing.SupportsIndex = ...,
        params: AlgorithmParams = ...,
        preprocess: bool = True,
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> SolveResult: ...
    def update_arc(
        self,
        arc: _real_int_set_arc,
        resource_consumption: tuple[
            collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
            collections.abc.Sequence[
                tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
            ],
        ],
        cost: typing.SupportsFloat | typing.SupportsIndex | None = None,
    ) -> None: ...
    def update_reduced_costs(
        self,
        duals: typing.Annotated[numpy.typing.ArrayLike, numpy.float64],
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> None: ...

class _real_bitset_node:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    @property
    def id(self) -> int: ...
    @property
    def sink(self) -> bool: ...
    @property
    def source(self) -> bool: ...

class _real_bitset_arc:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    def destination(self) -> _real_bitset_node: ...
    def origin(self) -> _real_bitset_node: ...
    @property
    def cost(self) -> float: ...
    @cost.setter
    def cost(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None: ...
    @property
    def id(self) -> int: ...
    @property
    def rows(self) -> list[Row]: ...
    @rows.setter
    def rows(self, arg0: collections.abc.Sequence[Row]) -> None: ...

class _real_bitset_graph:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    def _add_rows_bulk(self, rows: typing.Annotated[numpy.typing.ArrayLike, numpy.float64]) -> None:
        """
        Bulk-append rows from a (N, 3) float64 array [arc_id, row_index, coeff]. The array must be sorted by arc_id (column 0).
        """

    def add_rows_to_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex, rows: collections.abc.Sequence[Row]
    ) -> bool:
        """
        Append rows to an arc's rows. Returns false if arc_id is invalid.
        """

    def arc_ids(self) -> list[int]: ...
    @typing.overload
    def force_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> list[int]:
        """
        Remove all other out-arcs from the arc's origin and all other in-arcs to its destination. Returns the ids of the removed arcs.
        """

    @typing.overload
    def force_arc(self, arc: _real_bitset_arc) -> list[int]:
        """
        Remove all other out-arcs from the arc's origin and all other in-arcs to its destination. Returns the ids of the removed arcs.
        """

    def get_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> _real_bitset_arc: ...
    def get_arcs(
        self,
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
    ) -> list[_real_bitset_arc]: ...
    def get_node(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> _real_bitset_node: ...
    def get_removed_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex
    ) -> _real_bitset_arc: ...
    def is_sink(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    def is_source(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    def next_arc_id(self) -> int:
        """
        Return the next arc ID that will be assigned by add_arc().
        """

    def node_ids(self) -> list[int]: ...
    def number_of_arcs(self) -> int: ...
    def number_of_nodes(self) -> int: ...
    @typing.overload
    def remove_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    @typing.overload
    def remove_arc(self, arc: _real_bitset_arc) -> bool: ...
    def remove_arcs(
        self, arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> list[int]: ...
    def remove_arcs_if(self, pred: collections.abc.Callable) -> list[int]: ...
    def removed_arc_ids(self) -> list[int]: ...
    def reserve(
        self,
        n_nodes: typing.SupportsInt | typing.SupportsIndex,
        n_arcs: typing.SupportsInt | typing.SupportsIndex,
    ) -> None:
        """
        Pre-allocate hash-map buckets to avoid rehashing during bulk inserts.
        """

    @typing.overload
    def restore_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    @typing.overload
    def restore_arc(self, arc: _real_bitset_arc) -> bool: ...
    def restore_arcs(
        self, arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> list[int]: ...
    def restore_arcs_if(self, pred: collections.abc.Callable) -> list[int]: ...
    def sink_node_ids(self) -> list[int]: ...
    @typing.overload
    def sort_nodes(self) -> None: ...
    @typing.overload
    def sort_nodes(self, comp: collections.abc.Callable) -> None: ...
    def source_node_ids(self) -> list[int]: ...
    def to_string(self, print_arcs: bool = False) -> str: ...

class _real_bitset_resource_graph(_real_bitset_graph):
    def __init__(self) -> None: ...
    def _add_arcs_bulk(
        self,
        consumptions: collections.abc.Sequence[
            tuple[
                collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
                collections.abc.Sequence[
                    tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
                ],
            ]
        ],
        origin_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex],
        destination_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex],
        costs: collections.abc.Sequence[typing.SupportsFloat | typing.SupportsIndex],
        rows: collections.abc.Sequence[collections.abc.Sequence[Row]],
    ) -> None: ...
    def _add_nodes_bulk(
        self,
        nodes: collections.abc.Sequence[
            tuple[typing.SupportsInt | typing.SupportsIndex, bool, bool]
        ],
    ) -> None: ...
    def add_arc(
        self,
        resource_consumption: tuple[
            collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
            collections.abc.Sequence[
                tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
            ],
        ],
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
        cost: typing.SupportsFloat | typing.SupportsIndex = 0.0,
        rows: collections.abc.Sequence[Row] = [],
    ) -> _real_bitset_arc: ...
    def add_bitset_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_bitset,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_bitset,
        cost_function: rcspp._core.resource.CostFunction_bitset,
        dominance_function: rcspp._core.resource.DominanceFunction_bitset,
    ) -> None: ...
    def add_node(
        self,
        node_id: typing.SupportsInt | typing.SupportsIndex,
        source: bool = False,
        sink: bool = False,
    ) -> _real_bitset_node: ...
    def add_real_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_real,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_real,
        cost_function: rcspp._core.resource.CostFunction_real,
        dominance_function: rcspp._core.resource.DominanceFunction_real,
    ) -> None: ...
    def check_model(self, direction: SearchDirection = SearchDirection.Bidirectional) -> list[str]:
        """
        Run, without solving, the model checks that a search in `direction` needs. Returns one string per problem found; empty when the model is ready for that search.
        """

    def clone(
        self, include_rows: bool = True, clone_removed_arcs: bool = False
    ) -> _real_bitset_resource_graph:
        """
        Clone this ResourceGraph. Arc IDs are stable across the clone. The clone has an independent remove/restore state.
        """

    def is_connected(
        self,
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
    ) -> bool: ...
    def remove_infeasible_arcs(self) -> None: ...
    @typing.overload
    def solve(
        self,
        algorithm: Algorithm,
        upper_bound: typing.SupportsFloat | typing.SupportsIndex,
        params: BucketAlgorithmParams,
        preprocess: bool = True,
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> SolveResult: ...
    @typing.overload
    def solve(
        self,
        algorithm: Algorithm = Algorithm.Simple,
        upper_bound: typing.SupportsFloat | typing.SupportsIndex = ...,
        params: AlgorithmParams = ...,
        preprocess: bool = True,
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> SolveResult: ...
    def update_arc(
        self,
        arc: _real_bitset_arc,
        resource_consumption: tuple[
            collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
            collections.abc.Sequence[
                tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
            ],
        ],
        cost: typing.SupportsFloat | typing.SupportsIndex | None = None,
    ) -> None: ...
    def update_reduced_costs(
        self,
        duals: typing.Annotated[numpy.typing.ArrayLike, numpy.float64],
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> None: ...

class _real_int_real_set_node:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    @property
    def id(self) -> int: ...
    @property
    def sink(self) -> bool: ...
    @property
    def source(self) -> bool: ...

class _real_int_real_set_arc:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    def destination(self) -> _real_int_real_set_node: ...
    def origin(self) -> _real_int_real_set_node: ...
    @property
    def cost(self) -> float: ...
    @cost.setter
    def cost(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None: ...
    @property
    def id(self) -> int: ...
    @property
    def rows(self) -> list[Row]: ...
    @rows.setter
    def rows(self, arg0: collections.abc.Sequence[Row]) -> None: ...

class _real_int_real_set_graph:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    def _add_rows_bulk(self, rows: typing.Annotated[numpy.typing.ArrayLike, numpy.float64]) -> None:
        """
        Bulk-append rows from a (N, 3) float64 array [arc_id, row_index, coeff]. The array must be sorted by arc_id (column 0).
        """

    def add_rows_to_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex, rows: collections.abc.Sequence[Row]
    ) -> bool:
        """
        Append rows to an arc's rows. Returns false if arc_id is invalid.
        """

    def arc_ids(self) -> list[int]: ...
    @typing.overload
    def force_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> list[int]:
        """
        Remove all other out-arcs from the arc's origin and all other in-arcs to its destination. Returns the ids of the removed arcs.
        """

    @typing.overload
    def force_arc(self, arc: _real_int_real_set_arc) -> list[int]:
        """
        Remove all other out-arcs from the arc's origin and all other in-arcs to its destination. Returns the ids of the removed arcs.
        """

    def get_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex
    ) -> _real_int_real_set_arc: ...
    def get_arcs(
        self,
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
    ) -> list[_real_int_real_set_arc]: ...
    def get_node(
        self, node_id: typing.SupportsInt | typing.SupportsIndex
    ) -> _real_int_real_set_node: ...
    def get_removed_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex
    ) -> _real_int_real_set_arc: ...
    def is_sink(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    def is_source(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    def next_arc_id(self) -> int:
        """
        Return the next arc ID that will be assigned by add_arc().
        """

    def node_ids(self) -> list[int]: ...
    def number_of_arcs(self) -> int: ...
    def number_of_nodes(self) -> int: ...
    @typing.overload
    def remove_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    @typing.overload
    def remove_arc(self, arc: _real_int_real_set_arc) -> bool: ...
    def remove_arcs(
        self, arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> list[int]: ...
    def remove_arcs_if(self, pred: collections.abc.Callable) -> list[int]: ...
    def removed_arc_ids(self) -> list[int]: ...
    def reserve(
        self,
        n_nodes: typing.SupportsInt | typing.SupportsIndex,
        n_arcs: typing.SupportsInt | typing.SupportsIndex,
    ) -> None:
        """
        Pre-allocate hash-map buckets to avoid rehashing during bulk inserts.
        """

    @typing.overload
    def restore_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    @typing.overload
    def restore_arc(self, arc: _real_int_real_set_arc) -> bool: ...
    def restore_arcs(
        self, arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> list[int]: ...
    def restore_arcs_if(self, pred: collections.abc.Callable) -> list[int]: ...
    def sink_node_ids(self) -> list[int]: ...
    @typing.overload
    def sort_nodes(self) -> None: ...
    @typing.overload
    def sort_nodes(self, comp: collections.abc.Callable) -> None: ...
    def source_node_ids(self) -> list[int]: ...
    def to_string(self, print_arcs: bool = False) -> str: ...

class _real_int_real_set_resource_graph(_real_int_real_set_graph):
    def __init__(self) -> None: ...
    def _add_arcs_bulk(
        self,
        consumptions: collections.abc.Sequence[
            tuple[
                collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
                collections.abc.Sequence[tuple[typing.SupportsInt | typing.SupportsIndex]],
                collections.abc.Sequence[
                    tuple[collections.abc.Set[typing.SupportsFloat | typing.SupportsIndex]]
                ],
            ]
        ],
        origin_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex],
        destination_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex],
        costs: collections.abc.Sequence[typing.SupportsFloat | typing.SupportsIndex],
        rows: collections.abc.Sequence[collections.abc.Sequence[Row]],
    ) -> None: ...
    def _add_nodes_bulk(
        self,
        nodes: collections.abc.Sequence[
            tuple[typing.SupportsInt | typing.SupportsIndex, bool, bool]
        ],
    ) -> None: ...
    def add_arc(
        self,
        resource_consumption: tuple[
            collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
            collections.abc.Sequence[tuple[typing.SupportsInt | typing.SupportsIndex]],
            collections.abc.Sequence[
                tuple[collections.abc.Set[typing.SupportsFloat | typing.SupportsIndex]]
            ],
        ],
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
        cost: typing.SupportsFloat | typing.SupportsIndex = 0.0,
        rows: collections.abc.Sequence[Row] = [],
    ) -> _real_int_real_set_arc: ...
    def add_int_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_int,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_int,
        cost_function: rcspp._core.resource.CostFunction_int,
        dominance_function: rcspp._core.resource.DominanceFunction_int,
    ) -> None: ...
    def add_node(
        self,
        node_id: typing.SupportsInt | typing.SupportsIndex,
        source: bool = False,
        sink: bool = False,
    ) -> _real_int_real_set_node: ...
    def add_real_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_real,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_real,
        cost_function: rcspp._core.resource.CostFunction_real,
        dominance_function: rcspp._core.resource.DominanceFunction_real,
    ) -> None: ...
    def add_real_set_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_real_set,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_real_set,
        cost_function: rcspp._core.resource.CostFunction_real_set,
        dominance_function: rcspp._core.resource.DominanceFunction_real_set,
    ) -> None: ...
    def check_model(self, direction: SearchDirection = SearchDirection.Bidirectional) -> list[str]:
        """
        Run, without solving, the model checks that a search in `direction` needs. Returns one string per problem found; empty when the model is ready for that search.
        """

    def clone(
        self, include_rows: bool = True, clone_removed_arcs: bool = False
    ) -> _real_int_real_set_resource_graph:
        """
        Clone this ResourceGraph. Arc IDs are stable across the clone. The clone has an independent remove/restore state.
        """

    def is_connected(
        self,
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
    ) -> bool: ...
    def remove_infeasible_arcs(self) -> None: ...
    @typing.overload
    def solve(
        self,
        algorithm: Algorithm,
        upper_bound: typing.SupportsFloat | typing.SupportsIndex,
        params: BucketAlgorithmParams,
        preprocess: bool = True,
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> SolveResult: ...
    @typing.overload
    def solve(
        self,
        algorithm: Algorithm = Algorithm.Simple,
        upper_bound: typing.SupportsFloat | typing.SupportsIndex = ...,
        params: AlgorithmParams = ...,
        preprocess: bool = True,
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> SolveResult: ...
    def update_arc(
        self,
        arc: _real_int_real_set_arc,
        resource_consumption: tuple[
            collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
            collections.abc.Sequence[tuple[typing.SupportsInt | typing.SupportsIndex]],
            collections.abc.Sequence[
                tuple[collections.abc.Set[typing.SupportsFloat | typing.SupportsIndex]]
            ],
        ],
        cost: typing.SupportsFloat | typing.SupportsIndex | None = None,
    ) -> None: ...
    def update_reduced_costs(
        self,
        duals: typing.Annotated[numpy.typing.ArrayLike, numpy.float64],
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> None: ...

class _real_int_int_set_node:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    @property
    def id(self) -> int: ...
    @property
    def sink(self) -> bool: ...
    @property
    def source(self) -> bool: ...

class _real_int_int_set_arc:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    def destination(self) -> _real_int_int_set_node: ...
    def origin(self) -> _real_int_int_set_node: ...
    @property
    def cost(self) -> float: ...
    @cost.setter
    def cost(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None: ...
    @property
    def id(self) -> int: ...
    @property
    def rows(self) -> list[Row]: ...
    @rows.setter
    def rows(self, arg0: collections.abc.Sequence[Row]) -> None: ...

class _real_int_int_set_graph:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    def _add_rows_bulk(self, rows: typing.Annotated[numpy.typing.ArrayLike, numpy.float64]) -> None:
        """
        Bulk-append rows from a (N, 3) float64 array [arc_id, row_index, coeff]. The array must be sorted by arc_id (column 0).
        """

    def add_rows_to_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex, rows: collections.abc.Sequence[Row]
    ) -> bool:
        """
        Append rows to an arc's rows. Returns false if arc_id is invalid.
        """

    def arc_ids(self) -> list[int]: ...
    @typing.overload
    def force_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> list[int]:
        """
        Remove all other out-arcs from the arc's origin and all other in-arcs to its destination. Returns the ids of the removed arcs.
        """

    @typing.overload
    def force_arc(self, arc: _real_int_int_set_arc) -> list[int]:
        """
        Remove all other out-arcs from the arc's origin and all other in-arcs to its destination. Returns the ids of the removed arcs.
        """

    def get_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex
    ) -> _real_int_int_set_arc: ...
    def get_arcs(
        self,
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
    ) -> list[_real_int_int_set_arc]: ...
    def get_node(
        self, node_id: typing.SupportsInt | typing.SupportsIndex
    ) -> _real_int_int_set_node: ...
    def get_removed_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex
    ) -> _real_int_int_set_arc: ...
    def is_sink(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    def is_source(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    def next_arc_id(self) -> int:
        """
        Return the next arc ID that will be assigned by add_arc().
        """

    def node_ids(self) -> list[int]: ...
    def number_of_arcs(self) -> int: ...
    def number_of_nodes(self) -> int: ...
    @typing.overload
    def remove_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    @typing.overload
    def remove_arc(self, arc: _real_int_int_set_arc) -> bool: ...
    def remove_arcs(
        self, arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> list[int]: ...
    def remove_arcs_if(self, pred: collections.abc.Callable) -> list[int]: ...
    def removed_arc_ids(self) -> list[int]: ...
    def reserve(
        self,
        n_nodes: typing.SupportsInt | typing.SupportsIndex,
        n_arcs: typing.SupportsInt | typing.SupportsIndex,
    ) -> None:
        """
        Pre-allocate hash-map buckets to avoid rehashing during bulk inserts.
        """

    @typing.overload
    def restore_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    @typing.overload
    def restore_arc(self, arc: _real_int_int_set_arc) -> bool: ...
    def restore_arcs(
        self, arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> list[int]: ...
    def restore_arcs_if(self, pred: collections.abc.Callable) -> list[int]: ...
    def sink_node_ids(self) -> list[int]: ...
    @typing.overload
    def sort_nodes(self) -> None: ...
    @typing.overload
    def sort_nodes(self, comp: collections.abc.Callable) -> None: ...
    def source_node_ids(self) -> list[int]: ...
    def to_string(self, print_arcs: bool = False) -> str: ...

class _real_int_int_set_resource_graph(_real_int_int_set_graph):
    def __init__(self) -> None: ...
    def _add_arcs_bulk(
        self,
        consumptions: collections.abc.Sequence[
            tuple[
                collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
                collections.abc.Sequence[tuple[typing.SupportsInt | typing.SupportsIndex]],
                collections.abc.Sequence[
                    tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
                ],
            ]
        ],
        origin_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex],
        destination_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex],
        costs: collections.abc.Sequence[typing.SupportsFloat | typing.SupportsIndex],
        rows: collections.abc.Sequence[collections.abc.Sequence[Row]],
    ) -> None: ...
    def _add_nodes_bulk(
        self,
        nodes: collections.abc.Sequence[
            tuple[typing.SupportsInt | typing.SupportsIndex, bool, bool]
        ],
    ) -> None: ...
    def add_arc(
        self,
        resource_consumption: tuple[
            collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
            collections.abc.Sequence[tuple[typing.SupportsInt | typing.SupportsIndex]],
            collections.abc.Sequence[
                tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
            ],
        ],
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
        cost: typing.SupportsFloat | typing.SupportsIndex = 0.0,
        rows: collections.abc.Sequence[Row] = [],
    ) -> _real_int_int_set_arc: ...
    def add_int_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_int,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_int,
        cost_function: rcspp._core.resource.CostFunction_int,
        dominance_function: rcspp._core.resource.DominanceFunction_int,
    ) -> None: ...
    def add_int_set_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_int_set,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_int_set,
        cost_function: rcspp._core.resource.CostFunction_int_set,
        dominance_function: rcspp._core.resource.DominanceFunction_int_set,
    ) -> None: ...
    def add_node(
        self,
        node_id: typing.SupportsInt | typing.SupportsIndex,
        source: bool = False,
        sink: bool = False,
    ) -> _real_int_int_set_node: ...
    def add_real_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_real,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_real,
        cost_function: rcspp._core.resource.CostFunction_real,
        dominance_function: rcspp._core.resource.DominanceFunction_real,
    ) -> None: ...
    def check_model(self, direction: SearchDirection = SearchDirection.Bidirectional) -> list[str]:
        """
        Run, without solving, the model checks that a search in `direction` needs. Returns one string per problem found; empty when the model is ready for that search.
        """

    def clone(
        self, include_rows: bool = True, clone_removed_arcs: bool = False
    ) -> _real_int_int_set_resource_graph:
        """
        Clone this ResourceGraph. Arc IDs are stable across the clone. The clone has an independent remove/restore state.
        """

    def is_connected(
        self,
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
    ) -> bool: ...
    def remove_infeasible_arcs(self) -> None: ...
    @typing.overload
    def solve(
        self,
        algorithm: Algorithm,
        upper_bound: typing.SupportsFloat | typing.SupportsIndex,
        params: BucketAlgorithmParams,
        preprocess: bool = True,
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> SolveResult: ...
    @typing.overload
    def solve(
        self,
        algorithm: Algorithm = Algorithm.Simple,
        upper_bound: typing.SupportsFloat | typing.SupportsIndex = ...,
        params: AlgorithmParams = ...,
        preprocess: bool = True,
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> SolveResult: ...
    def update_arc(
        self,
        arc: _real_int_int_set_arc,
        resource_consumption: tuple[
            collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
            collections.abc.Sequence[tuple[typing.SupportsInt | typing.SupportsIndex]],
            collections.abc.Sequence[
                tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
            ],
        ],
        cost: typing.SupportsFloat | typing.SupportsIndex | None = None,
    ) -> None: ...
    def update_reduced_costs(
        self,
        duals: typing.Annotated[numpy.typing.ArrayLike, numpy.float64],
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> None: ...

class _real_int_set_bitset_node:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    @property
    def id(self) -> int: ...
    @property
    def sink(self) -> bool: ...
    @property
    def source(self) -> bool: ...

class _real_int_set_bitset_arc:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    def destination(self) -> _real_int_set_bitset_node: ...
    def origin(self) -> _real_int_set_bitset_node: ...
    @property
    def cost(self) -> float: ...
    @cost.setter
    def cost(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None: ...
    @property
    def id(self) -> int: ...
    @property
    def rows(self) -> list[Row]: ...
    @rows.setter
    def rows(self, arg0: collections.abc.Sequence[Row]) -> None: ...

class _real_int_set_bitset_graph:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    def _add_rows_bulk(self, rows: typing.Annotated[numpy.typing.ArrayLike, numpy.float64]) -> None:
        """
        Bulk-append rows from a (N, 3) float64 array [arc_id, row_index, coeff]. The array must be sorted by arc_id (column 0).
        """

    def add_rows_to_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex, rows: collections.abc.Sequence[Row]
    ) -> bool:
        """
        Append rows to an arc's rows. Returns false if arc_id is invalid.
        """

    def arc_ids(self) -> list[int]: ...
    @typing.overload
    def force_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> list[int]:
        """
        Remove all other out-arcs from the arc's origin and all other in-arcs to its destination. Returns the ids of the removed arcs.
        """

    @typing.overload
    def force_arc(self, arc: _real_int_set_bitset_arc) -> list[int]:
        """
        Remove all other out-arcs from the arc's origin and all other in-arcs to its destination. Returns the ids of the removed arcs.
        """

    def get_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex
    ) -> _real_int_set_bitset_arc: ...
    def get_arcs(
        self,
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
    ) -> list[_real_int_set_bitset_arc]: ...
    def get_node(
        self, node_id: typing.SupportsInt | typing.SupportsIndex
    ) -> _real_int_set_bitset_node: ...
    def get_removed_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex
    ) -> _real_int_set_bitset_arc: ...
    def is_sink(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    def is_source(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    def next_arc_id(self) -> int:
        """
        Return the next arc ID that will be assigned by add_arc().
        """

    def node_ids(self) -> list[int]: ...
    def number_of_arcs(self) -> int: ...
    def number_of_nodes(self) -> int: ...
    @typing.overload
    def remove_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    @typing.overload
    def remove_arc(self, arc: _real_int_set_bitset_arc) -> bool: ...
    def remove_arcs(
        self, arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> list[int]: ...
    def remove_arcs_if(self, pred: collections.abc.Callable) -> list[int]: ...
    def removed_arc_ids(self) -> list[int]: ...
    def reserve(
        self,
        n_nodes: typing.SupportsInt | typing.SupportsIndex,
        n_arcs: typing.SupportsInt | typing.SupportsIndex,
    ) -> None:
        """
        Pre-allocate hash-map buckets to avoid rehashing during bulk inserts.
        """

    @typing.overload
    def restore_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    @typing.overload
    def restore_arc(self, arc: _real_int_set_bitset_arc) -> bool: ...
    def restore_arcs(
        self, arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> list[int]: ...
    def restore_arcs_if(self, pred: collections.abc.Callable) -> list[int]: ...
    def sink_node_ids(self) -> list[int]: ...
    @typing.overload
    def sort_nodes(self) -> None: ...
    @typing.overload
    def sort_nodes(self, comp: collections.abc.Callable) -> None: ...
    def source_node_ids(self) -> list[int]: ...
    def to_string(self, print_arcs: bool = False) -> str: ...

class _real_int_set_bitset_resource_graph(_real_int_set_bitset_graph):
    def __init__(self) -> None: ...
    def _add_arcs_bulk(
        self,
        consumptions: collections.abc.Sequence[
            tuple[
                collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
                collections.abc.Sequence[
                    tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
                ],
                collections.abc.Sequence[
                    tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
                ],
            ]
        ],
        origin_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex],
        destination_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex],
        costs: collections.abc.Sequence[typing.SupportsFloat | typing.SupportsIndex],
        rows: collections.abc.Sequence[collections.abc.Sequence[Row]],
    ) -> None: ...
    def _add_nodes_bulk(
        self,
        nodes: collections.abc.Sequence[
            tuple[typing.SupportsInt | typing.SupportsIndex, bool, bool]
        ],
    ) -> None: ...
    def add_arc(
        self,
        resource_consumption: tuple[
            collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
            collections.abc.Sequence[
                tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
            ],
            collections.abc.Sequence[
                tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
            ],
        ],
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
        cost: typing.SupportsFloat | typing.SupportsIndex = 0.0,
        rows: collections.abc.Sequence[Row] = [],
    ) -> _real_int_set_bitset_arc: ...
    def add_bitset_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_bitset,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_bitset,
        cost_function: rcspp._core.resource.CostFunction_bitset,
        dominance_function: rcspp._core.resource.DominanceFunction_bitset,
    ) -> None: ...
    def add_int_set_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_int_set,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_int_set,
        cost_function: rcspp._core.resource.CostFunction_int_set,
        dominance_function: rcspp._core.resource.DominanceFunction_int_set,
    ) -> None: ...
    def add_node(
        self,
        node_id: typing.SupportsInt | typing.SupportsIndex,
        source: bool = False,
        sink: bool = False,
    ) -> _real_int_set_bitset_node: ...
    def add_real_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_real,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_real,
        cost_function: rcspp._core.resource.CostFunction_real,
        dominance_function: rcspp._core.resource.DominanceFunction_real,
    ) -> None: ...
    def check_model(self, direction: SearchDirection = SearchDirection.Bidirectional) -> list[str]:
        """
        Run, without solving, the model checks that a search in `direction` needs. Returns one string per problem found; empty when the model is ready for that search.
        """

    def clone(
        self, include_rows: bool = True, clone_removed_arcs: bool = False
    ) -> _real_int_set_bitset_resource_graph:
        """
        Clone this ResourceGraph. Arc IDs are stable across the clone. The clone has an independent remove/restore state.
        """

    def is_connected(
        self,
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
    ) -> bool: ...
    def remove_infeasible_arcs(self) -> None: ...
    @typing.overload
    def solve(
        self,
        algorithm: Algorithm,
        upper_bound: typing.SupportsFloat | typing.SupportsIndex,
        params: BucketAlgorithmParams,
        preprocess: bool = True,
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> SolveResult: ...
    @typing.overload
    def solve(
        self,
        algorithm: Algorithm = Algorithm.Simple,
        upper_bound: typing.SupportsFloat | typing.SupportsIndex = ...,
        params: AlgorithmParams = ...,
        preprocess: bool = True,
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> SolveResult: ...
    def update_arc(
        self,
        arc: _real_int_set_bitset_arc,
        resource_consumption: tuple[
            collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
            collections.abc.Sequence[
                tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
            ],
            collections.abc.Sequence[
                tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
            ],
        ],
        cost: typing.SupportsFloat | typing.SupportsIndex | None = None,
    ) -> None: ...
    def update_reduced_costs(
        self,
        duals: typing.Annotated[numpy.typing.ArrayLike, numpy.float64],
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> None: ...

class _real_int_real_set_int_set_bitset_node:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    @property
    def id(self) -> int: ...
    @property
    def sink(self) -> bool: ...
    @property
    def source(self) -> bool: ...

class _real_int_real_set_int_set_bitset_arc:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    def destination(self) -> _real_int_real_set_int_set_bitset_node: ...
    def origin(self) -> _real_int_real_set_int_set_bitset_node: ...
    @property
    def cost(self) -> float: ...
    @cost.setter
    def cost(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None: ...
    @property
    def id(self) -> int: ...
    @property
    def rows(self) -> list[Row]: ...
    @rows.setter
    def rows(self, arg0: collections.abc.Sequence[Row]) -> None: ...

class _real_int_real_set_int_set_bitset_graph:
    def __repr__(self) -> str: ...
    def __str__(self) -> str: ...
    def _add_rows_bulk(self, rows: typing.Annotated[numpy.typing.ArrayLike, numpy.float64]) -> None:
        """
        Bulk-append rows from a (N, 3) float64 array [arc_id, row_index, coeff]. The array must be sorted by arc_id (column 0).
        """

    def add_rows_to_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex, rows: collections.abc.Sequence[Row]
    ) -> bool:
        """
        Append rows to an arc's rows. Returns false if arc_id is invalid.
        """

    def arc_ids(self) -> list[int]: ...
    @typing.overload
    def force_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> list[int]:
        """
        Remove all other out-arcs from the arc's origin and all other in-arcs to its destination. Returns the ids of the removed arcs.
        """

    @typing.overload
    def force_arc(self, arc: _real_int_real_set_int_set_bitset_arc) -> list[int]:
        """
        Remove all other out-arcs from the arc's origin and all other in-arcs to its destination. Returns the ids of the removed arcs.
        """

    def get_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex
    ) -> _real_int_real_set_int_set_bitset_arc: ...
    def get_arcs(
        self,
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
    ) -> list[_real_int_real_set_int_set_bitset_arc]: ...
    def get_node(
        self, node_id: typing.SupportsInt | typing.SupportsIndex
    ) -> _real_int_real_set_int_set_bitset_node: ...
    def get_removed_arc(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex
    ) -> _real_int_real_set_int_set_bitset_arc: ...
    def is_sink(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    def is_source(self, node_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    def next_arc_id(self) -> int:
        """
        Return the next arc ID that will be assigned by add_arc().
        """

    def node_ids(self) -> list[int]: ...
    def number_of_arcs(self) -> int: ...
    def number_of_nodes(self) -> int: ...
    @typing.overload
    def remove_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    @typing.overload
    def remove_arc(self, arc: _real_int_real_set_int_set_bitset_arc) -> bool: ...
    def remove_arcs(
        self, arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> list[int]: ...
    def remove_arcs_if(self, pred: collections.abc.Callable) -> list[int]: ...
    def removed_arc_ids(self) -> list[int]: ...
    def reserve(
        self,
        n_nodes: typing.SupportsInt | typing.SupportsIndex,
        n_arcs: typing.SupportsInt | typing.SupportsIndex,
    ) -> None:
        """
        Pre-allocate hash-map buckets to avoid rehashing during bulk inserts.
        """

    @typing.overload
    def restore_arc(self, arc_id: typing.SupportsInt | typing.SupportsIndex) -> bool: ...
    @typing.overload
    def restore_arc(self, arc: _real_int_real_set_int_set_bitset_arc) -> bool: ...
    def restore_arcs(
        self, arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> list[int]: ...
    def restore_arcs_if(self, pred: collections.abc.Callable) -> list[int]: ...
    def sink_node_ids(self) -> list[int]: ...
    @typing.overload
    def sort_nodes(self) -> None: ...
    @typing.overload
    def sort_nodes(self, comp: collections.abc.Callable) -> None: ...
    def source_node_ids(self) -> list[int]: ...
    def to_string(self, print_arcs: bool = False) -> str: ...

class _real_int_real_set_int_set_bitset_resource_graph(_real_int_real_set_int_set_bitset_graph):
    def __init__(self) -> None: ...
    def _add_arcs_bulk(
        self,
        consumptions: collections.abc.Sequence[
            tuple[
                collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
                collections.abc.Sequence[tuple[typing.SupportsInt | typing.SupportsIndex]],
                collections.abc.Sequence[
                    tuple[collections.abc.Set[typing.SupportsFloat | typing.SupportsIndex]]
                ],
                collections.abc.Sequence[
                    tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
                ],
                collections.abc.Sequence[
                    tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
                ],
            ]
        ],
        origin_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex],
        destination_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex],
        costs: collections.abc.Sequence[typing.SupportsFloat | typing.SupportsIndex],
        rows: collections.abc.Sequence[collections.abc.Sequence[Row]],
    ) -> None: ...
    def _add_nodes_bulk(
        self,
        nodes: collections.abc.Sequence[
            tuple[typing.SupportsInt | typing.SupportsIndex, bool, bool]
        ],
    ) -> None: ...
    def add_arc(
        self,
        resource_consumption: tuple[
            collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
            collections.abc.Sequence[tuple[typing.SupportsInt | typing.SupportsIndex]],
            collections.abc.Sequence[
                tuple[collections.abc.Set[typing.SupportsFloat | typing.SupportsIndex]]
            ],
            collections.abc.Sequence[
                tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
            ],
            collections.abc.Sequence[
                tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
            ],
        ],
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
        cost: typing.SupportsFloat | typing.SupportsIndex = 0.0,
        rows: collections.abc.Sequence[Row] = [],
    ) -> _real_int_real_set_int_set_bitset_arc: ...
    def add_bitset_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_bitset,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_bitset,
        cost_function: rcspp._core.resource.CostFunction_bitset,
        dominance_function: rcspp._core.resource.DominanceFunction_bitset,
    ) -> None: ...
    def add_int_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_int,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_int,
        cost_function: rcspp._core.resource.CostFunction_int,
        dominance_function: rcspp._core.resource.DominanceFunction_int,
    ) -> None: ...
    def add_int_set_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_int_set,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_int_set,
        cost_function: rcspp._core.resource.CostFunction_int_set,
        dominance_function: rcspp._core.resource.DominanceFunction_int_set,
    ) -> None: ...
    def add_node(
        self,
        node_id: typing.SupportsInt | typing.SupportsIndex,
        source: bool = False,
        sink: bool = False,
    ) -> _real_int_real_set_int_set_bitset_node: ...
    def add_real_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_real,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_real,
        cost_function: rcspp._core.resource.CostFunction_real,
        dominance_function: rcspp._core.resource.DominanceFunction_real,
    ) -> None: ...
    def add_real_set_resource(
        self,
        extension_function: rcspp._core.resource.ExtensionFunction_real_set,
        feasibility_function: rcspp._core.resource.FeasibilityFunction_real_set,
        cost_function: rcspp._core.resource.CostFunction_real_set,
        dominance_function: rcspp._core.resource.DominanceFunction_real_set,
    ) -> None: ...
    def check_model(self, direction: SearchDirection = SearchDirection.Bidirectional) -> list[str]:
        """
        Run, without solving, the model checks that a search in `direction` needs. Returns one string per problem found; empty when the model is ready for that search.
        """

    def clone(
        self, include_rows: bool = True, clone_removed_arcs: bool = False
    ) -> _real_int_real_set_int_set_bitset_resource_graph:
        """
        Clone this ResourceGraph. Arc IDs are stable across the clone. The clone has an independent remove/restore state.
        """

    def is_connected(
        self,
        origin_node_id: typing.SupportsInt | typing.SupportsIndex,
        destination_node_id: typing.SupportsInt | typing.SupportsIndex,
    ) -> bool: ...
    def remove_infeasible_arcs(self) -> None: ...
    @typing.overload
    def solve(
        self,
        algorithm: Algorithm,
        upper_bound: typing.SupportsFloat | typing.SupportsIndex,
        params: BucketAlgorithmParams,
        preprocess: bool = True,
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> SolveResult: ...
    @typing.overload
    def solve(
        self,
        algorithm: Algorithm = Algorithm.Simple,
        upper_bound: typing.SupportsFloat | typing.SupportsIndex = ...,
        params: AlgorithmParams = ...,
        preprocess: bool = True,
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> SolveResult: ...
    def update_arc(
        self,
        arc: _real_int_real_set_int_set_bitset_arc,
        resource_consumption: tuple[
            collections.abc.Sequence[tuple[typing.SupportsFloat | typing.SupportsIndex]],
            collections.abc.Sequence[tuple[typing.SupportsInt | typing.SupportsIndex]],
            collections.abc.Sequence[
                tuple[collections.abc.Set[typing.SupportsFloat | typing.SupportsIndex]]
            ],
            collections.abc.Sequence[
                tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
            ],
            collections.abc.Sequence[
                tuple[collections.abc.Set[typing.SupportsInt | typing.SupportsIndex]]
            ],
        ],
        cost: typing.SupportsFloat | typing.SupportsIndex | None = None,
    ) -> None: ...
    def update_reduced_costs(
        self,
        duals: typing.Annotated[numpy.typing.ArrayLike, numpy.float64],
        cost_index: typing.SupportsInt | typing.SupportsIndex = 0,
    ) -> None: ...

def check_interrupted() -> None:
    """
    Raise KeyboardInterrupt if a SIGINT was received since the last solve.
    """
