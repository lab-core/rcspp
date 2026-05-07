#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

from . import _core as _ext

# ── Generic resource-function descriptors ─────────────────────────────────────
# These are resolved to the correct typed C++ object when add_xxx_resource is
# called on a ResourceGraph. Naming convention: C++ classes are registered as
# FunctionName_<resource_type> (e.g. ValueCostFunction_real, UnionExtensionFunction_real_set).


class _GenericFunctionDescriptor:
    """Base marker for generic (type-unspecialized) resource function descriptors."""

    def create(self, resource_type: str):
        raise NotImplementedError


def _get_fn(fn_name: str, resource_type: str):
    """Look up a typed C++ class by name and resource type, raising TypeError if
    absent."""
    cls = getattr(_ext.resource, f"{fn_name}_{resource_type}", None)
    if cls is None:
        raise TypeError(f"{fn_name} is not available for resource type '{resource_type}'")
    return cls


# ── Numerical + container: trivial ───────────────────────────────────────────


class TrivialFeasibilityFunction(_GenericFunctionDescriptor):
    def create(self, resource_type: str):
        return _get_fn("TrivialFeasibilityFunction", resource_type)()


class TrivialCostFunction(_GenericFunctionDescriptor):
    def create(self, resource_type: str):
        return _get_fn("TrivialCostFunction", resource_type)()


# ── Numerical only ────────────────────────────────────────────────────────────


class AdditionExtensionFunction(_GenericFunctionDescriptor):
    def create(self, resource_type: str):
        return _get_fn("AdditionExtensionFunction", resource_type)()


class ValueCostFunction(_GenericFunctionDescriptor):
    def create(self, resource_type: str):
        return _get_fn("ValueCostFunction", resource_type)()


class ValueDominanceFunction(_GenericFunctionDescriptor):
    def create(self, resource_type: str):
        return _get_fn("ValueDominanceFunction", resource_type)()


class MinMaxFeasibilityFunction(_GenericFunctionDescriptor):
    def __init__(self, min_value, max_value):
        self.min_value = min_value
        self.max_value = max_value

    def create(self, resource_type: str):
        return _get_fn("MinMaxFeasibilityFunction", resource_type)(self.min_value, self.max_value)


# ── Container only ────────────────────────────────────────────────────────────


class UnionExtensionFunction(_GenericFunctionDescriptor):
    def create(self, resource_type: str):
        return _get_fn("UnionExtensionFunction", resource_type)()


class IntersectionExtensionFunction(_GenericFunctionDescriptor):
    def create(self, resource_type: str):
        return _get_fn("IntersectionExtensionFunction", resource_type)()


class SubtractExtensionFunction(_GenericFunctionDescriptor):
    def create(self, resource_type: str):
        return _get_fn("SubtractExtensionFunction", resource_type)()


class InclusionDominanceFunction(_GenericFunctionDescriptor):
    def create(self, resource_type: str):
        return _get_fn("InclusionDominanceFunction", resource_type)()


class ContainDominanceFunction(_GenericFunctionDescriptor):
    def create(self, resource_type: str):
        return _get_fn("ContainDominanceFunction", resource_type)()


class SizeFeasibilityFunction(_GenericFunctionDescriptor):
    def __init__(self, min_size: int, max_size: int):
        self.min_size = min_size
        self.max_size = max_size

    def create(self, resource_type: str):
        return _get_fn("SizeFeasibilityFunction", resource_type)(self.min_size, self.max_size)


# ── Re-export typed C++ names from the resource submodule ────────────────────
# Names overridden above with generic Python wrappers are excluded.

_overridden = {
    "AdditionExtensionFunction",
    "ValueCostFunction",
    "ValueDominanceFunction",
    "TrivialFeasibilityFunction",
    "TrivialCostFunction",
    "MinMaxFeasibilityFunction",
    "UnionExtensionFunction",
    "IntersectionExtensionFunction",
    "SubtractExtensionFunction",
    "InclusionDominanceFunction",
    "ContainDominanceFunction",
    "SizeFeasibilityFunction",
}

for _k in dir(_ext.resource):
    if not _k.startswith("_") and _k not in _overridden:
        globals()[_k] = getattr(_ext.resource, _k)
