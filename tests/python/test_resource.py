#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

import os
import sys

import pytest

sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "python", "src")
)

from rcspp.resource import (  # noqa: E402
    ContainDominanceFunction,
    InclusionDominanceFunction,
    IntersectionExtensionFunction,
    SizeFeasibilityFunction,
    SubtractExtensionFunction,
    TimeWindowExtensionFunction,
    TimeWindowFeasibilityFunction,
    TrivialCostFunction,
    TrivialFeasibilityFunction,
    UnionExtensionFunction,
)


def test_get_fn_unknown_type_raises():
    with pytest.raises(TypeError, match="not available for resource type"):
        UnionExtensionFunction().create("not_a_type")


def test_time_window_extension_no_default():
    fn = TimeWindowExtensionFunction({0: (0.0, 10.0), 1: (0.0, 20.0)})
    result = fn.create("real")
    assert result is not None


def test_time_window_extension_with_default():
    fn = TimeWindowExtensionFunction({0: (0.0, 10.0)}, default_max_value=100.0)
    result = fn.create("real")
    assert result is not None


def test_time_window_feasibility_no_default():
    fn = TimeWindowFeasibilityFunction({0: (0.0, 10.0)})
    result = fn.create("real")
    assert result is not None


def test_time_window_feasibility_with_default():
    fn = TimeWindowFeasibilityFunction({0: (0.0, 10.0)}, default_max_value=100.0)
    result = fn.create("real")
    assert result is not None


def test_union_extension_function():
    result = UnionExtensionFunction().create("uint_bitset")
    assert result is not None


def test_intersection_extension_function():
    result = IntersectionExtensionFunction().create("uint_bitset")
    assert result is not None


def test_subtract_extension_function():
    result = SubtractExtensionFunction().create("uint_bitset")
    assert result is not None


def test_inclusion_dominance_function():
    result = InclusionDominanceFunction().create("uint_bitset")
    assert result is not None


def test_contain_dominance_function():
    result = ContainDominanceFunction().create("uint_bitset")
    assert result is not None


def test_size_feasibility_function_init_and_create():
    fn = SizeFeasibilityFunction(2, 5)
    assert fn.min_size == 2
    assert fn.max_size == 5
    result = fn.create("uint_bitset")
    assert result is not None


def test_trivial_feasibility_bitset():
    result = TrivialFeasibilityFunction().create("uint_bitset")
    assert result is not None


def test_trivial_cost_bitset():
    result = TrivialCostFunction().create("uint_bitset")
    assert result is not None
