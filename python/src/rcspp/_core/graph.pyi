from __future__ import annotations

from enum import Enum
from typing import Any, Sequence

import numpy as np

class Algorithm(Enum):
    Simple: Algorithm
    Pushing: Algorithm
    Pulling: Algorithm
    Greedy: Algorithm
    Tabu: Algorithm

class AlgorithmStatus(Enum):
    Complete: AlgorithmStatus
    Timeout: AlgorithmStatus
    MaxSolutions: AlgorithmStatus
    MaxPhases: AlgorithmStatus
    Interrupted: AlgorithmStatus
    MemoryLimit: AlgorithmStatus

class Row:
    index: int
    coefficient: float
    def __init__(self, index: int = 0, coefficient: float = 0.0) -> None: ...

class Column:
    cost: float
    rows: list[Row]
    def __init__(self) -> None: ...

class Solution:
    cost: float
    path_node_ids: list[int]
    path_arc_ids: list[int]
    column: Column
    def __init__(self) -> None: ...
    def to_arrays(
        self,
    ) -> tuple[
        float,
        np.ndarray[Any, np.dtype[np.int64]],
        np.ndarray[Any, np.dtype[np.int64]],
        np.ndarray[Any, np.dtype[np.float64]],
    ]: ...

class SolveResult:
    """Result of a solve() call.

    Behaves like a ``list[Solution]`` for backward compatibility
    (``len()``, indexing, iteration) while also exposing ``.status``.
    """

    solutions: list[Solution]
    status: AlgorithmStatus
    def __init__(self) -> None: ...
    def status_string(self) -> str: ...
    def __len__(self) -> int: ...
    def __iter__(self) -> Any: ...
    def __getitem__(self, index: int) -> Solution: ...
    def __bool__(self) -> bool: ...
    def __repr__(self) -> str: ...

class AlgorithmParams:
    stop_after_X_solutions: int
    return_dominated_solutions: bool
    use_pool: bool
    num_labels_to_extend_by_node: int
    num_max_phases: int
    max_iterations: int
    timeout_s: float
    tolerance: float
    release_after_solve: bool
    tabu_tenure: int
    forbidden_tabu: set[int]
    tabu_random_noise: bool
    seed: int
    max_memory_gb: float
    limit_to_available_ram: bool
    limit_to_total_ram: bool
    memory_limit_fraction: float
    memory_check_interval: int
    memory_pressure_fraction: float
    memory_pressure_max_labels_per_node: int
    def __init__(self) -> None: ...
    def check(self) -> None: ...
    def could_be_non_optimal(self) -> bool: ...

class BucketAlgorithmParams(AlgorithmParams):
    """Low-level C++ bucket params.  Prefer the Python wrapper in rcspp.graph."""

    range_buckets: int
    bucket_resource_index: int
    sort_resource_index: int
    bucket_resource_type: str
    def __init__(self) -> None: ...

class Node:
    id: int
    source: bool
    sink: bool
    in_arcs: list[Arc]
    out_arcs: list[Arc]
    resource: Any
    def pos(self) -> int: ...
    def __str__(self) -> str: ...
    def __repr__(self) -> str: ...

class Arc:
    id: int
    cost: float
    rows: list[Row]
    extender: Any
    def origin(self) -> Node: ...
    def destination(self) -> Node: ...
    def __str__(self) -> str: ...
    def __repr__(self) -> str: ...

class Graph:
    def add_node(self, id: int, source: bool = False, sink: bool = False) -> Node: ...
    def add_arc(
        self,
        origin_id: int,
        destination_id: int,
        cost: float = 0.0,
        rows: Sequence[Row] = (),
    ) -> Arc: ...
    def get_node(self, id: int) -> Node | None: ...
    def get_arc(self, id: int) -> Arc | None: ...
    def remove_arc(self, arc_id: int) -> bool: ...
    def restore_arc(self, arc_id: int) -> bool: ...
    def remove_arcs(self, arc_ids: Sequence[int]) -> list[int]: ...
    def restore_arcs(self, arc_ids: Sequence[int]) -> list[int]: ...
    def force_arc(self, arc_id: int) -> list[int]: ...
    def number_of_arcs(self) -> int: ...
    def number_of_nodes(self) -> int: ...
    def removed_arc_ids(self) -> list[int]: ...
    def next_arc_id(self) -> int: ...
    def update(self) -> None: ...

def check_interrupted() -> None: ...
