#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Generic resource-function descriptors for the Python RCSPP API.

Each descriptor class is a lightweight, type-agnostic wrapper that defers
instantiation of the underlying typed C++ object until the resource type is
known (i.e. when ``add_xxx_resource`` is called on a ``ResourceGraph``).
C++ classes are registered as ``FunctionName_<resource_type>``
(e.g. ``ValueCostFunction_real``, ``UnionExtensionFunction_real_set``).
"""

from . import _core as _ext

# ── Generic resource-function descriptors ─────────────────────────────────────
# These are resolved to the correct typed C++ object when add_xxx_resource is
# called on a ResourceGraph. Naming convention: C++ classes are registered as
# FunctionName_<resource_type> (e.g. ValueCostFunction_real, UnionExtensionFunction_real_set).


class _GenericFunctionDescriptor:
    """Base marker for generic (type-unspecialized) resource function descriptors.

    Subclasses implement ``create`` to instantiate the appropriate typed C++
    object once the concrete resource type is known.
    """

    def create(self, resource_type: str):
        """Instantiate the typed C++ function object for *resource_type*.

        Args:
            resource_type: Resource type string (e.g. ``"real"``, ``"int_set"``).

        Raises:
            NotImplementedError: Must be overridden by subclasses.
        """
        raise NotImplementedError


def _get_fn(fn_name: str, resource_type: str):
    """Look up a typed C++ class by name and resource type.

    Args:
        fn_name: Base name of the C++ class (e.g. ``"ValueCostFunction"``).
        resource_type: Resource type suffix (e.g. ``"real"``, ``"int_set"``).

    Returns:
        The C++ class registered as ``{fn_name}_{resource_type}``.

    Raises:
        TypeError: If no matching class is found in the C++ extension module.
    """
    cls = getattr(_ext.resource, f"{fn_name}_{resource_type}", None)
    if cls is None:
        raise TypeError(f"{fn_name} is not available for resource type '{resource_type}'")
    return cls


# ── Numerical + container: trivial ───────────────────────────────────────────


class TrivialFeasibilityFunction(_GenericFunctionDescriptor):
    """Feasibility function that always returns feasible.

    Applicable to both numerical and container resource types.
    """

    def create(self, resource_type: str):
        """Instantiate a TrivialFeasibilityFunction for *resource_type*.

        Args:
            resource_type: Resource type string (e.g. ``"real"``, ``"int_set"``).

        Returns:
            A typed C++ TrivialFeasibilityFunction instance.
        """
        return _get_fn("TrivialFeasibilityFunction", resource_type)()


class TrivialCostFunction(_GenericFunctionDescriptor):
    """Cost function that always returns zero cost.

    Applicable to both numerical and container resource types.
    """

    def create(self, resource_type: str):
        """Instantiate a TrivialCostFunction for *resource_type*.

        Args:
            resource_type: Resource type string (e.g. ``"real"``, ``"int_set"``).

        Returns:
            A typed C++ TrivialCostFunction instance.
        """
        return _get_fn("TrivialCostFunction", resource_type)()


# ── Numerical only ────────────────────────────────────────────────────────────


class AdditionExtensionFunction(_GenericFunctionDescriptor):
    """Extension function that adds the arc consumption to the resource value.

    Applicable to numerical resource types only.
    """

    def create(self, resource_type: str):
        """Instantiate an AdditionExtensionFunction for *resource_type*.

        Args:
            resource_type: Numerical resource type string (e.g. ``"real"``,
                ``"int"``).

        Returns:
            A typed C++ AdditionExtensionFunction instance.
        """
        return _get_fn("AdditionExtensionFunction", resource_type)()


class ValueCostFunction(_GenericFunctionDescriptor):
    """Cost function that uses the resource value directly as cost.

    Applicable to numerical resource types only.
    """

    def create(self, resource_type: str):
        """Instantiate a ValueCostFunction for *resource_type*.

        Args:
            resource_type: Numerical resource type string (e.g. ``"real"``,
                ``"int"``).

        Returns:
            A typed C++ ValueCostFunction instance.
        """
        return _get_fn("ValueCostFunction", resource_type)()


class ValueDominanceFunction(_GenericFunctionDescriptor):
    """Dominance function that compares resource values directly.

    A label dominates another if its resource value is less than or equal to the
    other's. Applicable to numerical resource types only.
    """

    def create(self, resource_type: str):
        """Instantiate a ValueDominanceFunction for *resource_type*.

        Args:
            resource_type: Numerical resource type string (e.g. ``"real"``,
                ``"int"``).

        Returns:
            A typed C++ ValueDominanceFunction instance.
        """
        return _get_fn("ValueDominanceFunction", resource_type)()


class MinMaxFeasibilityFunction(_GenericFunctionDescriptor):
    """Feasibility function that enforces a [min, max] bound on a resource value.

    A label is feasible when its resource value lies within the closed interval
    ``[min_value, max_value]``. Applicable to numerical resource types only.
    """

    def __init__(self, min_value, max_value):
        """Initialize the feasibility bounds.

        Args:
            min_value: Lower bound (inclusive) on the resource value.
            max_value: Upper bound (inclusive) on the resource value.
        """
        self.min_value = min_value
        self.max_value = max_value

    def create(self, resource_type: str):
        """Instantiate a MinMaxFeasibilityFunction for *resource_type*.

        Args:
            resource_type: Numerical resource type string (e.g. ``"real"``,
                ``"int"``).

        Returns:
            A typed C++ MinMaxFeasibilityFunction instance.
        """
        return _get_fn("MinMaxFeasibilityFunction", resource_type)(self.min_value, self.max_value)


class TimeWindowExtensionFunction(_GenericFunctionDescriptor):
    """Extension function that advances the resource to the earliest feasible time.

    At each node the resource value is extended by the arc consumption and then clamped
    to the node's time-window lower bound (i.e. waiting is allowed). Applicable to
    numerical resource types only.
    """

    def __init__(self, tw_by_node: dict, default_max_value=None):
        """Initialize the time-window extension function.

        Args:
            tw_by_node: Mapping from node identifier to ``(earliest, latest)``
                time-window tuples.
            default_max_value: Fallback upper bound used for nodes not present
                in *tw_by_node*. When ``None`` the C++ default is applied.
        """
        self.tw_by_node = tw_by_node
        self.default_max_value = default_max_value

    def create(self, resource_type: str):
        """Instantiate a TimeWindowExtensionFunction for *resource_type*.

        Args:
            resource_type: Numerical resource type string (e.g. ``"real"``,
                ``"int"``).

        Returns:
            A typed C++ TimeWindowExtensionFunction instance.
        """
        fn = _get_fn("TimeWindowExtensionFunction", resource_type)
        if self.default_max_value is None:
            return fn(self.tw_by_node)
        return fn(self.tw_by_node, self.default_max_value)


class TimeWindowFeasibilityFunction(_GenericFunctionDescriptor):
    """Feasibility function that checks whether a resource value lies within a time
    window.

    A label is feasible at a node when its resource value does not exceed the node's
    latest time. Applicable to numerical resource types only.
    """

    def __init__(self, tw_by_node: dict, default_max_value=None):
        """Initialize the time-window feasibility function.

        Args:
            tw_by_node: Mapping from node identifier to ``(earliest, latest)``
                time-window tuples.
            default_max_value: Fallback upper bound used for nodes not present
                in *tw_by_node*. When ``None`` the C++ default is applied.
        """
        self.tw_by_node = tw_by_node
        self.default_max_value = default_max_value

    def create(self, resource_type: str):
        """Instantiate a TimeWindowFeasibilityFunction for *resource_type*.

        Args:
            resource_type: Numerical resource type string (e.g. ``"real"``,
                ``"int"``).

        Returns:
            A typed C++ TimeWindowFeasibilityFunction instance.
        """
        fn = _get_fn("TimeWindowFeasibilityFunction", resource_type)
        if self.default_max_value is None:
            return fn(self.tw_by_node)
        return fn(self.tw_by_node, self.default_max_value)


# ── Container only ────────────────────────────────────────────────────────────


class UnionExtensionFunction(_GenericFunctionDescriptor):
    """Extension function that unions the arc consumption set into the resource.

    Applicable to container resource types only.
    """

    def create(self, resource_type: str):
        """Instantiate a UnionExtensionFunction for *resource_type*.

        Args:
            resource_type: Container resource type string
                (e.g. ``"real_set"``, ``"int_set"``, ``"bitset"``).

        Returns:
            A typed C++ UnionExtensionFunction instance.
        """
        return _get_fn("UnionExtensionFunction", resource_type)()


class IntersectionExtensionFunction(_GenericFunctionDescriptor):
    """Extension function that intersects the resource with the arc consumption set.

    Applicable to container resource types only.
    """

    def create(self, resource_type: str):
        """Instantiate an IntersectionExtensionFunction for *resource_type*.

        Args:
            resource_type: Container resource type string
                (e.g. ``"real_set"``, ``"int_set"``, ``"bitset"``).

        Returns:
            A typed C++ IntersectionExtensionFunction instance.
        """
        return _get_fn("IntersectionExtensionFunction", resource_type)()


class SubtractExtensionFunction(_GenericFunctionDescriptor):
    """Extension function that removes the arc consumption set from the resource.

    Applicable to container resource types only.
    """

    def create(self, resource_type: str):
        """Instantiate a SubtractExtensionFunction for *resource_type*.

        Args:
            resource_type: Container resource type string
                (e.g. ``"real_set"``, ``"int_set"``, ``"bitset"``).

        Returns:
            A typed C++ SubtractExtensionFunction instance.
        """
        return _get_fn("SubtractExtensionFunction", resource_type)()


class InclusionDominanceFunction(_GenericFunctionDescriptor):
    """Dominance function based on set inclusion.

    A label dominates another if its resource set is a subset of the other's. Applicable
    to container resource types only.
    """

    def create(self, resource_type: str):
        """Instantiate an InclusionDominanceFunction for *resource_type*.

        Args:
            resource_type: Container resource type string
                (e.g. ``"real_set"``, ``"int_set"``, ``"bitset"``).

        Returns:
            A typed C++ InclusionDominanceFunction instance.
        """
        return _get_fn("InclusionDominanceFunction", resource_type)()


class ContainDominanceFunction(_GenericFunctionDescriptor):
    """Dominance function based on superset containment.

    A label dominates another if its resource set is a superset of the other's.
    Applicable to container resource types only.
    """

    def create(self, resource_type: str):
        """Instantiate a ContainDominanceFunction for *resource_type*.

        Args:
            resource_type: Container resource type string
                (e.g. ``"real_set"``, ``"int_set"``, ``"bitset"``).

        Returns:
            A typed C++ ContainDominanceFunction instance.
        """
        return _get_fn("ContainDominanceFunction", resource_type)()


class SizeFeasibilityFunction(_GenericFunctionDescriptor):
    """Feasibility function that enforces a cardinality bound on a container resource.

    A label is feasible when the size of its resource set lies within the
    closed interval ``[min_size, max_size]``. Applicable to container resource
    types only.
    """

    def __init__(self, min_size: int, max_size: int):
        """Initialize the size feasibility bounds.

        Args:
            min_size: Minimum required cardinality of the resource set.
            max_size: Maximum allowed cardinality of the resource set.
        """
        self.min_size = min_size
        self.max_size = max_size

    def create(self, resource_type: str):
        """Instantiate a SizeFeasibilityFunction for *resource_type*.

        Args:
            resource_type: Container resource type string
                (e.g. ``"real_set"``, ``"int_set"``, ``"bitset"``).

        Returns:
            A typed C++ SizeFeasibilityFunction instance.
        """
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
    "TimeWindowExtensionFunction",
    "TimeWindowFeasibilityFunction",
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
