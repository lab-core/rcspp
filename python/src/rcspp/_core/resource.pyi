"""
Resource-related classes
"""

from __future__ import annotations

import collections.abc
import typing

__all__: list[str] = [
    "AdditionExtensionFunction_int",
    "AdditionExtensionFunction_real",
    "CapacityExtensionFunction_int",
    "CapacityExtensionFunction_real",
    "CostFunction_bitset",
    "CostFunction_int",
    "CostFunction_int_set",
    "CostFunction_real",
    "CostFunction_real_set",
    "DifferenceExtensionFunction_bitset",
    "DifferenceExtensionFunction_int_set",
    "DifferenceExtensionFunction_real_set",
    "DominanceFunction_bitset",
    "DominanceFunction_int",
    "DominanceFunction_int_set",
    "DominanceFunction_real",
    "DominanceFunction_real_set",
    "ExtensionFunction_bitset",
    "ExtensionFunction_int",
    "ExtensionFunction_int_set",
    "ExtensionFunction_real",
    "ExtensionFunction_real_set",
    "FeasibilityFunction_bitset",
    "FeasibilityFunction_int",
    "FeasibilityFunction_int_set",
    "FeasibilityFunction_real",
    "FeasibilityFunction_real_set",
    "IntersectionExtensionFunction_bitset",
    "IntersectionExtensionFunction_int_set",
    "IntersectionExtensionFunction_real_set",
    "IntersectionFeasibilityFunction_bitset",
    "IntersectionFeasibilityFunction_int_set",
    "IntersectionFeasibilityFunction_real_set",
    "MinMaxFeasibilityFunction_int",
    "MinMaxFeasibilityFunction_real",
    "NgPathExtensionFunction_bitset",
    "NgPathExtensionFunction_int_set",
    "NgPathExtensionFunction_real_set",
    "SizeFeasibilityFunction_bitset",
    "SizeFeasibilityFunction_int_set",
    "SizeFeasibilityFunction_real_set",
    "SubsetDominanceFunction_bitset",
    "SubsetDominanceFunction_int_set",
    "SubsetDominanceFunction_real_set",
    "SupersetDominanceFunction_bitset",
    "SupersetDominanceFunction_int_set",
    "SupersetDominanceFunction_real_set",
    "TimeWindowExtensionFunction_int",
    "TimeWindowExtensionFunction_real",
    "TimeWindowFeasibilityFunction_int",
    "TimeWindowFeasibilityFunction_real",
    "TrivialCostFunction_bitset",
    "TrivialCostFunction_int",
    "TrivialCostFunction_int_set",
    "TrivialCostFunction_real",
    "TrivialCostFunction_real_set",
    "TrivialFeasibilityFunction_bitset",
    "TrivialFeasibilityFunction_int",
    "TrivialFeasibilityFunction_int_set",
    "TrivialFeasibilityFunction_real",
    "TrivialFeasibilityFunction_real_set",
    "UnionExtensionFunction_bitset",
    "UnionExtensionFunction_int_set",
    "UnionExtensionFunction_real_set",
    "ValueCostFunction_int",
    "ValueCostFunction_real",
    "ValueDominanceFunction_int",
    "ValueDominanceFunction_real",
]

class ExtensionFunction_real:
    pass

class FeasibilityFunction_real:
    pass

class CostFunction_real:
    pass

class DominanceFunction_real:
    pass

class ExtensionFunction_int:
    pass

class FeasibilityFunction_int:
    pass

class CostFunction_int:
    pass

class DominanceFunction_int:
    pass

class ExtensionFunction_real_set:
    pass

class FeasibilityFunction_real_set:
    pass

class CostFunction_real_set:
    pass

class DominanceFunction_real_set:
    pass

class ExtensionFunction_int_set:
    pass

class FeasibilityFunction_int_set:
    pass

class CostFunction_int_set:
    pass

class DominanceFunction_int_set:
    pass

class ExtensionFunction_bitset:
    pass

class FeasibilityFunction_bitset:
    pass

class CostFunction_bitset:
    pass

class DominanceFunction_bitset:
    pass

class AdditionExtensionFunction_real(ExtensionFunction_real):
    def __init__(self) -> None: ...

class ValueCostFunction_real(CostFunction_real):
    def __init__(self) -> None: ...

class TrivialCostFunction_real(CostFunction_real):
    def __init__(self) -> None: ...

class ValueDominanceFunction_real(DominanceFunction_real):
    def __init__(self) -> None: ...

class MinMaxFeasibilityFunction_real(FeasibilityFunction_real):
    def __init__(
        self,
        min_value: typing.SupportsFloat | typing.SupportsIndex,
        max_value: typing.SupportsFloat | typing.SupportsIndex,
    ) -> None: ...

class TrivialFeasibilityFunction_real(FeasibilityFunction_real):
    def __init__(self) -> None: ...

class TimeWindowExtensionFunction_real(ExtensionFunction_real):
    def __init__(
        self,
        tw_by_node: dict,
        default_max_value: typing.SupportsFloat | typing.SupportsIndex = 8.988465674311579e307,
    ) -> None: ...

class TimeWindowFeasibilityFunction_real(FeasibilityFunction_real):
    def __init__(
        self,
        tw_by_node: dict,
        default_max_value: typing.SupportsFloat | typing.SupportsIndex = 8.988465674311579e307,
    ) -> None: ...

class AdditionExtensionFunction_int(ExtensionFunction_int):
    def __init__(self) -> None: ...

class ValueCostFunction_int(CostFunction_int):
    def __init__(self) -> None: ...

class TrivialCostFunction_int(CostFunction_int):
    def __init__(self) -> None: ...

class ValueDominanceFunction_int(DominanceFunction_int):
    def __init__(self) -> None: ...

class MinMaxFeasibilityFunction_int(FeasibilityFunction_int):
    def __init__(
        self,
        min_value: typing.SupportsInt | typing.SupportsIndex,
        max_value: typing.SupportsInt | typing.SupportsIndex,
    ) -> None: ...

class TrivialFeasibilityFunction_int(FeasibilityFunction_int):
    def __init__(self) -> None: ...

class TimeWindowExtensionFunction_int(ExtensionFunction_int):
    def __init__(
        self,
        tw_by_node: dict,
        default_max_value: typing.SupportsInt | typing.SupportsIndex = 1073741823,
    ) -> None: ...

class TimeWindowFeasibilityFunction_int(FeasibilityFunction_int):
    def __init__(
        self,
        tw_by_node: dict,
        default_max_value: typing.SupportsInt | typing.SupportsIndex = 1073741823,
    ) -> None: ...

class CapacityExtensionFunction_real(ExtensionFunction_real):
    def __init__(self, capacity: typing.SupportsFloat | typing.SupportsIndex) -> None: ...

class CapacityExtensionFunction_int(ExtensionFunction_int):
    def __init__(self, capacity: typing.SupportsInt | typing.SupportsIndex) -> None: ...

class TrivialCostFunction_real_set(CostFunction_real_set):
    def __init__(self) -> None: ...

class TrivialFeasibilityFunction_real_set(FeasibilityFunction_real_set):
    def __init__(self) -> None: ...

class SubsetDominanceFunction_real_set(DominanceFunction_real_set):
    def __init__(self) -> None: ...

class SupersetDominanceFunction_real_set(DominanceFunction_real_set):
    def __init__(self) -> None: ...

class UnionExtensionFunction_real_set(ExtensionFunction_real_set):
    def __init__(self) -> None: ...

class IntersectionExtensionFunction_real_set(ExtensionFunction_real_set):
    def __init__(self) -> None: ...

class DifferenceExtensionFunction_real_set(ExtensionFunction_real_set):
    def __init__(self) -> None: ...

class SizeFeasibilityFunction_real_set(FeasibilityFunction_real_set):
    def __init__(
        self,
        min_size: typing.SupportsInt | typing.SupportsIndex,
        max_size: typing.SupportsInt | typing.SupportsIndex,
    ) -> None: ...

class NgPathExtensionFunction_real_set(ExtensionFunction_real_set):
    def __init__(
        self,
        neighborhoods: collections.abc.Mapping[
            typing.SupportsInt | typing.SupportsIndex,
            collections.abc.Set[typing.SupportsFloat | typing.SupportsIndex],
        ],
    ) -> None: ...

class IntersectionFeasibilityFunction_real_set(FeasibilityFunction_real_set):
    def __init__(
        self,
        values_by_node: collections.abc.Mapping[
            typing.SupportsInt | typing.SupportsIndex,
            collections.abc.Set[typing.SupportsFloat | typing.SupportsIndex],
        ],
        forbidden: bool = True,
    ) -> None: ...

class TrivialCostFunction_int_set(CostFunction_int_set):
    def __init__(self) -> None: ...

class TrivialFeasibilityFunction_int_set(FeasibilityFunction_int_set):
    def __init__(self) -> None: ...

class SubsetDominanceFunction_int_set(DominanceFunction_int_set):
    def __init__(self) -> None: ...

class SupersetDominanceFunction_int_set(DominanceFunction_int_set):
    def __init__(self) -> None: ...

class UnionExtensionFunction_int_set(ExtensionFunction_int_set):
    def __init__(self) -> None: ...

class IntersectionExtensionFunction_int_set(ExtensionFunction_int_set):
    def __init__(self) -> None: ...

class DifferenceExtensionFunction_int_set(ExtensionFunction_int_set):
    def __init__(self) -> None: ...

class SizeFeasibilityFunction_int_set(FeasibilityFunction_int_set):
    def __init__(
        self,
        min_size: typing.SupportsInt | typing.SupportsIndex,
        max_size: typing.SupportsInt | typing.SupportsIndex,
    ) -> None: ...

class NgPathExtensionFunction_int_set(ExtensionFunction_int_set):
    def __init__(
        self,
        neighborhoods: collections.abc.Mapping[
            typing.SupportsInt | typing.SupportsIndex,
            collections.abc.Set[typing.SupportsInt | typing.SupportsIndex],
        ],
    ) -> None: ...

class IntersectionFeasibilityFunction_int_set(FeasibilityFunction_int_set):
    def __init__(
        self,
        values_by_node: collections.abc.Mapping[
            typing.SupportsInt | typing.SupportsIndex,
            collections.abc.Set[typing.SupportsInt | typing.SupportsIndex],
        ],
        forbidden: bool = True,
    ) -> None: ...

class TrivialCostFunction_bitset(CostFunction_bitset):
    def __init__(self) -> None: ...

class TrivialFeasibilityFunction_bitset(FeasibilityFunction_bitset):
    def __init__(self) -> None: ...

class SubsetDominanceFunction_bitset(DominanceFunction_bitset):
    def __init__(self) -> None: ...

class SupersetDominanceFunction_bitset(DominanceFunction_bitset):
    def __init__(self) -> None: ...

class UnionExtensionFunction_bitset(ExtensionFunction_bitset):
    def __init__(self) -> None: ...

class IntersectionExtensionFunction_bitset(ExtensionFunction_bitset):
    def __init__(self) -> None: ...

class DifferenceExtensionFunction_bitset(ExtensionFunction_bitset):
    def __init__(self) -> None: ...

class SizeFeasibilityFunction_bitset(FeasibilityFunction_bitset):
    def __init__(
        self,
        min_size: typing.SupportsInt | typing.SupportsIndex,
        max_size: typing.SupportsInt | typing.SupportsIndex,
    ) -> None: ...

class NgPathExtensionFunction_bitset(ExtensionFunction_bitset):
    def __init__(
        self,
        neighborhoods: collections.abc.Mapping[
            typing.SupportsInt | typing.SupportsIndex,
            collections.abc.Set[typing.SupportsInt | typing.SupportsIndex],
        ],
    ) -> None: ...

class IntersectionFeasibilityFunction_bitset(FeasibilityFunction_bitset):
    def __init__(
        self,
        values_by_node: collections.abc.Mapping[
            typing.SupportsInt | typing.SupportsIndex,
            collections.abc.Set[typing.SupportsInt | typing.SupportsIndex],
        ],
        forbidden: bool = True,
    ) -> None: ...
