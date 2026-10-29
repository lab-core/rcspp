#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Components by name, for configurations and benchmark grids.

A name stands for a component built with its defaults, or with the params given:
``make_tolerances("relative", reduced_cost=1e-6)``. :class:`ColumnGenerationParams`
takes a name wherever it takes a tolerances or initialization object, and a
:class:`~rcspp.column_generation.PricingStage` takes an algorithm by name. Master solvers
have no name: each wraps a model, so a name alone cannot build one.
"""

from typing import Any, Callable

from .._core.column_generation import (
    AbsoluteTolerances,
    ArtificialColumns,
    MasterInitialization,
    NoInitialization,
    RelativeTolerances,
    Tolerances,
)
from ..graph import ALGORITHMS

_TOLERANCES: dict[str, Callable[..., Tolerances]] = {
    "absolute": AbsoluteTolerances,
    "relative": RelativeTolerances,
}
_INITIALIZATIONS: dict[str, Callable[..., MasterInitialization]] = {
    "artificial_columns": ArtificialColumns,
    "none": NoInitialization,
}


def _register(kind: str, names: dict[str, Any], name: str, factory: Any) -> None:
    if name in names:
        raise ValueError(f"{kind} {name!r} is already registered")
    names[name] = factory


def _make(
    kind: str, names: dict[str, Callable[..., Any]], name: str, params: dict[str, Any]
) -> Any:
    factory = names.get(name)
    if factory is None:
        raise ValueError(f"Unknown {kind} {name!r}. Choose from: {', '.join(names)}")
    return factory(**params)


def register_tolerances(name: str, factory: Callable[..., Tolerances]) -> None:
    """Gives `factory` a name; it is called with the params given to
    :func:`make_tolerances`.

    Raises:
        ValueError: When the name is taken.
    """
    _register("tolerances", _TOLERANCES, name, factory)


def register_initialization(name: str, factory: Callable[..., MasterInitialization]) -> None:
    """Gives `factory` a name; it is called with the params given to
    :func:`make_initialization`.

    Raises:
        ValueError: When the name is taken.
    """
    _register("initialization", _INITIALIZATIONS, name, factory)


def make_tolerances(name: str, **params: Any) -> Tolerances:
    """The tolerances named `name`, built with `params`.

    Raises:
        ValueError: For an unknown name.
    """
    tolerances: Tolerances = _make("tolerances", _TOLERANCES, name, params)
    return tolerances


def make_initialization(name: str, **params: Any) -> MasterInitialization:
    """The initialization named `name`, built with `params`.

    Raises:
        ValueError: For an unknown name.
    """
    initialization: MasterInitialization = _make("initialization", _INITIALIZATIONS, name, params)
    return initialization


def tolerance_names() -> list[str]:
    """Every tolerances name, the registered ones included."""
    return list(_TOLERANCES)


def initialization_names() -> list[str]:
    """Every initialization name, the registered ones included."""
    return list(_INITIALIZATIONS)


def algorithm_names() -> list[str]:
    """Every name a :class:`~rcspp.column_generation.PricingStage` takes as its
    algorithm."""
    return list(ALGORITHMS)
