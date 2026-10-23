"""
Solution pool classes
"""

from __future__ import annotations

import collections.abc
import typing

import numpy
import numpy.typing

import rcspp._core.graph

__all__: list[str] = [
    "ColumnActivity",
    "DEFAULT_PRICE_THRESHOLD",
    "FilteredSolutionPool",
    "PricedColumn",
    "SolutionPool",
]

class ColumnActivity:
    last_was_negative: bool
    def __init__(self) -> None: ...
    def usage_rate(self) -> float: ...
    @property
    def age(self) -> int: ...
    @age.setter
    def age(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    @property
    def created_at(self) -> int: ...
    @created_at.setter
    def created_at(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    @property
    def last_reduced_cost(self) -> float: ...
    @last_reduced_cost.setter
    def last_reduced_cost(self, arg0: typing.SupportsFloat | typing.SupportsIndex) -> None: ...
    @property
    def priced_count(self) -> int: ...
    @priced_count.setter
    def priced_count(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    @property
    def use_count(self) -> int: ...
    @use_count.setter
    def use_count(self, arg0: typing.SupportsInt | typing.SupportsIndex) -> None: ...

class PricedColumn:
    @property
    def id(self) -> int: ...
    @property
    def reduced_cost(self) -> float: ...
    @property
    def solution(self) -> rcspp._core.graph.Solution: ...

class FilteredSolutionPool:
    @staticmethod
    def make_filter(
        compulsory_rows: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex] = [],
        forbidden_rows: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex] = [],
        compulsory_arc_ids: collections.abc.Sequence[
            typing.SupportsInt | typing.SupportsIndex
        ] = [],
        forbidden_arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex] = [],
    ) -> collections.abc.Callable[[rcspp._core.graph.Solution], bool]: ...
    def __len__(self) -> int: ...
    @typing.overload
    def add(self, solution: rcspp._core.graph.Solution, check_filter: bool = True) -> int: ...
    @typing.overload
    def add(
        self,
        solutions: collections.abc.Sequence[rcspp._core.graph.Solution],
        check_filter: bool = True,
    ) -> list[int]: ...
    def add_filter(
        self,
        filter: collections.abc.Callable | None = None,
        compulsory_rows: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex] = [],
        forbidden_rows: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex] = [],
        compulsory_arc_ids: collections.abc.Sequence[
            typing.SupportsInt | typing.SupportsIndex
        ] = [],
        forbidden_arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex] = [],
    ) -> None: ...
    def cleanup(self) -> None: ...
    def get(
        self, column_id: typing.SupportsInt | typing.SupportsIndex
    ) -> rcspp._core.graph.Solution | None: ...
    def get_activity(
        self, column_id: typing.SupportsInt | typing.SupportsIndex
    ) -> ColumnActivity | None: ...
    def get_all(self) -> list[tuple[int, rcspp._core.graph.Solution, ColumnActivity]]: ...
    def get_column_ids(self) -> numpy.typing.NDArray[numpy.uint64]: ...
    def get_entry(
        self, column_id: typing.SupportsInt | typing.SupportsIndex
    ) -> tuple[int, rcspp._core.graph.Solution, ColumnActivity] | None: ...
    def global_remove_if(self, pred: collections.abc.Callable) -> list[int]: ...
    def global_remove_if_arc_present(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex
    ) -> list[int]: ...
    def global_remove_stale(
        self,
        max_age: typing.SupportsInt | typing.SupportsIndex,
        min_usage_rate: typing.SupportsFloat | typing.SupportsIndex = 0.0,
    ) -> list[int]: ...
    def new_filter(
        self,
        filter: collections.abc.Callable | None = None,
        compulsory_rows: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex] = [],
        forbidden_rows: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex] = [],
        compulsory_arc_ids: collections.abc.Sequence[
            typing.SupportsInt | typing.SupportsIndex
        ] = [],
        forbidden_arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex] = [],
        min_usage_rate: typing.SupportsFloat | typing.SupportsIndex | None = None,
        max_age: typing.SupportsInt | typing.SupportsIndex | None = None,
        max_last_rc: typing.SupportsFloat | typing.SupportsIndex | None = None,
    ) -> FilteredSolutionPool: ...
    def pool(self) -> SolutionPool: ...
    def price(
        self,
        duals: collections.abc.Sequence[typing.SupportsFloat | typing.SupportsIndex],
        threshold: typing.SupportsFloat | typing.SupportsIndex = -1e-09,
    ) -> list[PricedColumn]: ...
    def price_numpy(
        self,
        duals: typing.Annotated[numpy.typing.ArrayLike, numpy.float64],
        threshold: typing.SupportsFloat | typing.SupportsIndex = -1e-09,
    ) -> tuple[numpy.typing.NDArray[numpy.uint64], numpy.typing.NDArray[numpy.float64]]: ...
    def pricing_count(self) -> int: ...
    def remove_if(self, pred: collections.abc.Callable) -> list[int]: ...
    def remove_if_arc_present(
        self, arc_id: typing.SupportsInt | typing.SupportsIndex
    ) -> list[int]: ...
    def remove_stale(
        self,
        max_age: typing.SupportsInt | typing.SupportsIndex,
        min_usage_rate: typing.SupportsFloat | typing.SupportsIndex = 0.0,
    ) -> list[int]: ...
    def sort_by_lp_index(self) -> None: ...
    def update_activity(
        self, basis_column_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex]
    ) -> None: ...

class SolutionPool:
    NO_ID: typing.ClassVar[int] = 0
    @staticmethod
    def make_filter(
        compulsory_rows: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex] = [],
        forbidden_rows: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex] = [],
        compulsory_arc_ids: collections.abc.Sequence[
            typing.SupportsInt | typing.SupportsIndex
        ] = [],
        forbidden_arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex] = [],
    ) -> collections.abc.Callable[[rcspp._core.graph.Solution], bool]: ...
    def __init__(self) -> None: ...
    def get_lp_arrays(
        self,
    ) -> tuple[
        numpy.typing.NDArray[numpy.float64],
        numpy.typing.NDArray[numpy.uint32],
        numpy.typing.NDArray[numpy.uint32],
        numpy.typing.NDArray[numpy.float64],
    ]: ...
    def new_filter(
        self,
        filter: collections.abc.Callable | None = None,
        compulsory_rows: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex] = [],
        forbidden_rows: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex] = [],
        compulsory_arc_ids: collections.abc.Sequence[
            typing.SupportsInt | typing.SupportsIndex
        ] = [],
        forbidden_arc_ids: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex] = [],
        min_usage_rate: typing.SupportsFloat | typing.SupportsIndex | None = None,
        max_age: typing.SupportsInt | typing.SupportsIndex | None = None,
        max_last_rc: typing.SupportsFloat | typing.SupportsIndex | None = None,
    ) -> FilteredSolutionPool: ...

DEFAULT_PRICE_THRESHOLD: float = -1e-09
