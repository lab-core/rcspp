#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

from . import _core as _ext

# ── Generic resource-function descriptors ─────────────────────────────────────
# These are resolved to the correct typed C++ object when add_real_resource /
# add_int_resource is called on a ResourceGraph.


class _GenericFunctionDescriptor:
    """Base marker for generic (type-unspecialized) resource function descriptors."""

    def create(self, resource_type: str):
        raise NotImplementedError


class AdditionExtensionFunction(_GenericFunctionDescriptor):
    def create(self, resource_type: str):
        if resource_type == "real":
            return _ext.resource.RealAdditionExtensionFunction()
        return _ext.resource.IntAdditionExtensionFunction()


class ValueCostFunction(_GenericFunctionDescriptor):
    def create(self, resource_type: str):
        if resource_type == "real":
            return _ext.resource.RealValueCostFunction()
        return _ext.resource.IntValueCostFunction()


class ValueDominanceFunction(_GenericFunctionDescriptor):
    def create(self, resource_type: str):
        if resource_type == "real":
            return _ext.resource.RealValueDominanceFunction()
        return _ext.resource.IntValueDominanceFunction()


class TrivialFeasibilityFunction(_GenericFunctionDescriptor):
    def create(self, resource_type: str):
        if resource_type == "real":
            return _ext.resource.RealTrivialFeasibilityFunction()
        return _ext.resource.IntTrivialFeasibilityFunction()


class MinMaxFeasibilityFunction(_GenericFunctionDescriptor):
    def __init__(self, min_value, max_value):
        self.min_value = min_value
        self.max_value = max_value

    def create(self, resource_type: str):
        if resource_type == "real":
            return _ext.resource.MinMaxFeasibilityFunction(
                float(self.min_value), float(self.max_value)
            )
        return _ext.resource.IntMinMaxFeasibilityFunction(int(self.min_value), int(self.max_value))


# Re-export typed C++ names and everything else from the resource submodule,
# except names we've overridden above with generic wrappers.
_overridden = {
    "AdditionExtensionFunction",
    "ValueCostFunction",
    "ValueDominanceFunction",
    "TrivialFeasibilityFunction",
    "MinMaxFeasibilityFunction",
}

for _k in dir(_ext.resource):
    if not _k.startswith("_") and _k not in _overridden:
        globals()[_k] = getattr(_ext.resource, _k)
