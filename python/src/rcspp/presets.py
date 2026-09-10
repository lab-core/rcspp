"""One call per resource *kind*, for the shapes this library actually has.

A resource is four function objects that have to agree with each other. Individually each can be
legal while the quadruple is incoherent -- the recorded example is an accumulating extension
paired with a feasibility function that seeds its backward label at a ceiling, which is coherent
component by component and silently finds nothing backward. A preset makes that pairing
unwritable.

The four-object ``add_<type>_resource`` form remains normative. These are sugar over it: each
preset's docstring names exactly what it expands to, so stepping down to the general form is
obvious the moment a model needs something a preset does not offer.

**Registration order.** ``ResourceGraph`` requires that the first registered resource be a cost
resource of type ``"real"``, so :func:`add_cost_resource` must come first. The C++ side has no
such rule; this is a Python-side contract and it is why every other preset here says so.

**No new bindings.** These are plain Python functions over the existing ``add_<type>_resource``
wrappers, which already resolve descriptors from :mod:`rcspp.resource` to typed C++ classes. No
file under ``python/bindings/`` is involved.

There is deliberately **no** ``add_ng_path_resource`` here, unlike the C++ ``rcspp::presets``.
``NgPathExtensionFunction`` and ``IntersectionFeasibilityFunction`` have no Python descriptors, so
an ng-path model cannot be assembled from Python at all today -- with or without a preset. Adding
those descriptors is a separate piece of work; a preset cannot paper over their absence.
"""

from __future__ import annotations

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

__all__ = [
    "add_budget_resource",
    "add_cost_resource",
    "add_window_resource",
]


def add_cost_resource(graph, resource_type: str = "real") -> None:
    """Register the cost slot: an unbounded accumulation whose value *is* the objective.

    Expands to ``graph.add_<resource_type>_resource(AdditionExtensionFunction(),
    TrivialFeasibilityFunction(), ValueCostFunction(), ValueDominanceFunction())``.

    Must be called **first**, and with ``"real"``: ``ResourceGraph`` rejects any other type as the
    first registered resource.

    :param graph: The :class:`rcspp.ResourceGraph` to register on.
    :param resource_type: The resource type slot; ``"real"`` in every current use.
    """
    add = getattr(graph, f"add_{resource_type}_resource")
    add(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )


def add_window_resource(
    graph, resource_type: str, windows: dict, default_max=None
) -> None:
    """Register a scalar resource with a per-node window: a *threshold*.

    Expands to ``graph.add_<resource_type>_resource(TimeWindowExtensionFunction(windows),
    TimeWindowFeasibilityFunction(windows), ValueCostFunction(), ValueDominanceFunction())``.

    The window map is passed **once**; by hand it has to be given to both the extension and the
    feasibility function.

    Call :func:`add_cost_resource` before this one.

    :param graph: The :class:`rcspp.ResourceGraph` to register on.
    :param resource_type: The resource type slot; must be signed (``"real"`` or ``"int"``),
        because backward extension subtracts.
    :param windows: ``{node_id: (earliest, latest)}``.
    :param default_max: Bound used at nodes absent from the map; ``None`` keeps the C++ default.
    """
    add = getattr(graph, f"add_{resource_type}_resource")
    add(
        TimeWindowExtensionFunction(windows, default_max),
        TimeWindowFeasibilityFunction(windows, default_max),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )


def add_budget_resource(
    graph, resource_type: str, capacity, per_node: dict | None = None, minimum=0
) -> None:
    """Register a bounded accumulation -- capacity, duration, any budget.

    Expands to ``graph.add_<resource_type>_resource(BudgetExtensionFunction(per_node, capacity),
    MinMaxFeasibilityFunction(minimum, capacity), TrivialCostFunction(),
    ValueDominanceFunction())``.

    This is the preset that exists because the mistake it prevents actually happened. Pairing
    ``AdditionExtensionFunction`` with ``MinMaxFeasibilityFunction`` gives a backward label that
    starts at the ceiling while the extension *adds*, so it leaves its range on the first arc and
    the backward search silently finds nothing. A bounded accumulation is a **threshold**, which
    is what ``BudgetExtensionFunction`` is for.

    Call :func:`add_cost_resource` before this one.

    :param graph: The :class:`rcspp.ResourceGraph` to register on.
    :param resource_type: The resource type slot; must be signed (``"real"`` or ``"int"``).
    :param capacity: The global upper bound.
    :param per_node: Optional per-node capacities; ``None`` means uniform.
    :param minimum: The global lower bound, normally zero.
    """
    add = getattr(graph, f"add_{resource_type}_resource")
    add(
        BudgetExtensionFunction(per_node, capacity),
        MinMaxFeasibilityFunction(minimum, capacity),
        TrivialCostFunction(),
        ValueDominanceFunction(),
    )
