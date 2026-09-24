"""One call per resource kind, registering a coherent set of four function objects.

Each preset is sugar over the four-object ``add_<type>_resource`` form and documents what it
expands to. :func:`add_cost_resource` must be called first, since ``ResourceGraph`` requires the
first resource to be a ``"real"`` cost resource.

There is no ng-path preset: its extension and feasibility functions have no Python descriptors.
"""

from __future__ import annotations

from collections.abc import Mapping
from typing import TYPE_CHECKING, Literal

from .resource import (
    AdditionExtensionFunction,
    BudgetExtensionFunction,
    MinMaxFeasibilityFunction,
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
    "add_budget_resource",
    "add_cost_resource",
    "add_window_resource",
]

#: The resource types a threshold preset accepts: backward extension subtracts, so it needs a
#: signed type.
SignedResourceType = Literal["real", "int"]


def add_cost_resource(graph: ResourceGraph, resource_type: Literal["real"] = "real") -> None:
    """Register the cost slot: an unbounded accumulation whose value *is* the objective.

    Expands to ``graph.add_<resource_type>_resource(AdditionExtensionFunction(),
    TrivialFeasibilityFunction(), ValueCostFunction(), ValueDominanceFunction())``.

    Must be called **first**, and with ``"real"``: ``ResourceGraph`` rejects any other type as the
    first registered resource.

    Args:
        graph: The graph to register on.
        resource_type: The resource type slot; ``"real"`` in every current use.
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
    """Register a scalar resource with a per-node window: a *threshold*.

    Expands to ``graph.add_<resource_type>_resource(TimeWindowExtensionFunction(windows),
    TimeWindowFeasibilityFunction(windows), ValueCostFunction(), ValueDominanceFunction())``.

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
        ValueCostFunction(),
        ValueDominanceFunction(),
    )


def add_budget_resource(
    graph: ResourceGraph, resource_type: SignedResourceType, capacity: float
) -> None:
    """Register a bounded accumulation -- capacity, duration, any budget.

    Expands to ``graph.add_<resource_type>_resource(BudgetExtensionFunction(capacity),
    MinMaxFeasibilityFunction(0, capacity), TrivialCostFunction(), ValueDominanceFunction())``.

    Uniform capacity only; per-node budgets need the C++ ``rcspp::presets::add_budget_resource``.
    The minimum is 0, which a bidirectional solve checks only while loads are non-negative, so
    they must be: it refuses a negative arc load at setup.

    Call :func:`add_cost_resource` before this one.

    Args:
        graph: The graph to register on.
        resource_type: The resource type slot; must be signed (``"real"`` or ``"int"``).
        capacity: The upper bound, at every node.
    """
    add = getattr(graph, f"add_{resource_type}_resource")
    add(
        BudgetExtensionFunction(capacity),
        MinMaxFeasibilityFunction(0, capacity),
        TrivialCostFunction(),
        ValueDominanceFunction(),
    )
