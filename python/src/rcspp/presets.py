"""One call per resource kind, registering a coherent set of four function objects.

Each preset is sugar over the four-object ``add_<type>_resource`` form and documents what it
expands to. :func:`add_cost_resource` must be called first, since ``ResourceGraph`` requires the
first resource to be a ``"real"`` cost resource.
"""

from __future__ import annotations

from collections.abc import Iterable, Mapping
from typing import TYPE_CHECKING, Literal

from .resource import (
    AdditionExtensionFunction,
    CapacityExtensionFunction,
    IntersectionFeasibilityFunction,
    MinMaxFeasibilityFunction,
    NgPathExtensionFunction,
    SubsetDominanceFunction,
    TimeWindowExtensionFunction,
    TimeWindowFeasibilityFunction,
    TrivialCostFunction,
    TrivialFeasibilityFunction,
    ValueCostFunction,
    ValueDominanceFunction,
)

if TYPE_CHECKING:
    from .graph import ResourceGraph

__all__ = [
    "add_capacity_resource",
    "add_cost_resource",
    "add_elementary_resource",
    "add_ng_path_resource",
    "add_window_resource",
]

#: The resource types a threshold preset accepts: backward extension subtracts, so it needs a
#: signed type.
SignedResourceType = Literal["real", "int"]

#: The resource types a memory preset accepts: a container. ``"bitset"`` is the compact one.
ContainerResourceType = Literal["bitset", "int_set", "real_set"]


def add_cost_resource(graph: ResourceGraph, resource_type: Literal["real"] = "real") -> None:
    """Register the cost slot: an unbounded accumulation whose value *is* the objective.

    Expands to ``graph.add_<resource_type>_resource(AdditionExtensionFunction(),
    TrivialFeasibilityFunction(), ValueCostFunction(), ValueDominanceFunction())``.

    Must be called **first**, and with ``"real"``: ``ResourceGraph`` rejects any other type as the
    first registered resource.

    Args:
        graph: The graph to register on.
        resource_type: The resource type; must be ``"real"``, because the graph requires its
            first resource, the cost, to be ``"real"`` (it raises ``ValueError`` otherwise).
    """
    add = getattr(graph, f"add_{resource_type}_resource")
    add(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )


def add_window_resource(
    graph: ResourceGraph,
    resource_type: SignedResourceType,
    windows: Mapping[int, tuple[float, float]],
    default_max: float | None = None,
) -> None:
    """Register a resource with a window ``[earliest, latest]`` at each node.

    Typically an arrival time: a path that arrives before ``earliest`` waits until then, and
    one that arrives after ``latest`` is infeasible.

    Expands to ``graph.add_<resource_type>_resource(TimeWindowExtensionFunction(windows,
    default_max), TimeWindowFeasibilityFunction(windows, default_max), TrivialCostFunction(),
    ValueDominanceFunction())``.

    The window costs nothing, as a capacity does: a model whose objective is the arrival time
    uses the four-object form.

    The window map is passed once, to both the extension and the feasibility function.

    Call :func:`add_cost_resource` before this one.

    Args:
        graph: The graph to register on.
        resource_type: The resource type slot; must be signed (``"real"`` or ``"int"``),
            because backward extension subtracts.
        windows: ``{node_id: (earliest, latest)}``.
        default_max: Bound used at nodes absent from the map; ``None`` keeps the C++ default.
    """
    add = getattr(graph, f"add_{resource_type}_resource")
    add(
        TimeWindowExtensionFunction(windows, default_max),
        TimeWindowFeasibilityFunction(windows, default_max),
        TrivialCostFunction(),
        ValueDominanceFunction(),
    )


def add_capacity_resource(
    graph: ResourceGraph, resource_type: SignedResourceType, capacity: float
) -> None:
    """Register a bounded accumulation -- capacity, duration, any limit.

    Expands to ``graph.add_<resource_type>_resource(CapacityExtensionFunction(capacity),
    MinMaxFeasibilityFunction(0, capacity), TrivialCostFunction(), ValueDominanceFunction())``.

    Uniform capacity only; per-node capacities need the C++
    ``rcspp::presets::add_capacity_resource``.
    The minimum is 0, and arc loads must be non-negative: a backward or bidirectional solve can
    only enforce the minimum when they are, so its model checks refuse a negative load before
    the search starts.

    Call :func:`add_cost_resource` before this one.

    Args:
        graph: The graph to register on.
        resource_type: The resource type slot; must be signed (``"real"`` or ``"int"``).
        capacity: The upper bound, at every node.
    """
    add = getattr(graph, f"add_{resource_type}_resource")
    add(
        CapacityExtensionFunction(capacity),
        MinMaxFeasibilityFunction(0, capacity),
        TrivialCostFunction(),
        ValueDominanceFunction(),
    )


def add_ng_path_resource(
    graph: ResourceGraph,
    resource_type: ContainerResourceType,
    neighborhoods: Mapping[int, Iterable[int]],
) -> None:
    """Register an ng-path memory: a path may not revisit a node it still remembers.

    Expands to ``graph.add_<resource_type>_resource(NgPathExtensionFunction(neighborhoods),
    IntersectionFeasibilityFunction({v: {v}}), TrivialCostFunction(),
    SubsetDominanceFunction())``, the forbidden set ``{v}`` being derived for every node the
    neighbourhoods mention, as a key or a member.

    The memory is read from the arcs' endpoints, so each arc's value for this resource is ignored:
    pass an empty set.

    Call :func:`add_cost_resource` before this one.

    Args:
        graph: The graph to register on.
        resource_type: A container resource type; ``"bitset"`` is the compact one.
        neighborhoods: ``{node_id: ng-neighbourhood}``. A node absent from the map forgets every
            previously visited node on arrival, so give every node a neighbourhood.
    """
    forbidden: dict[int, set[int]] = {}
    for node_id, neighborhood in neighborhoods.items():
        forbidden[node_id] = {node_id}
        for member in neighborhood:
            forbidden[member] = {member}
    add = getattr(graph, f"add_{resource_type}_resource")
    add(
        NgPathExtensionFunction(neighborhoods),
        IntersectionFeasibilityFunction(forbidden, forbidden=True),
        TrivialCostFunction(),
        SubsetDominanceFunction(),
    )


def add_elementary_resource(
    graph: ResourceGraph,
    resource_type: ContainerResourceType,
    node_ids: Iterable[int],
) -> None:
    """Register an elementary-path memory: no node may be visited twice.

    Expands to :func:`add_ng_path_resource` with every node's neighbourhood set to every node.

    Call :func:`add_cost_resource` before this one.

    Args:
        graph: The graph to register on.
        resource_type: A container resource type; ``"bitset"`` is the compact one.
        node_ids: Every node id of the model. A node left out gets no neighbourhood, and
            elementarity stops holding through it.
    """
    nodes = set(node_ids)
    add_ng_path_resource(graph, resource_type, dict.fromkeys(nodes, nodes))
