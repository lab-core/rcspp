#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The blocks of a complete run, one dataclass each.

A block is built from its table once every source of a run is merged
(:meth:`RunOptions.complete`): :meth:`from_table` checks the table, fills in the defaults
and refuses anything unknown. A field is None only where the run leaves it to the layer
(``column_generation.timeout_s``) or where it does not apply (``cost_factor`` on a
``single-customer`` start). :meth:`to_table` gives the block back as TOML holds it.
"""

from __future__ import annotations

import copy
from dataclasses import dataclass, fields
from typing import Any, ClassVar, Self, cast


class OptionsError(Exception):
    """Options that cannot make a run: unknown names, wrong values, a missing option."""


# MARK: Values


def string(value: Any, where: str) -> str | None:
    if value is not None and not isinstance(value, str):
        raise OptionsError(f"{where}: {value!r} is not a string")

    return value


def boolean(value: Any, where: str) -> bool | None:
    if value is not None and not isinstance(value, bool):
        raise OptionsError(f"{where}: {value!r} is not true or false")

    return value


def integer(value: Any, where: str) -> int | None:
    if value is not None and (not isinstance(value, int) or isinstance(value, bool)):
        raise OptionsError(f"{where}: {value!r} is not a whole number")

    return value


def number(value: Any, where: str) -> float | None:
    if value is None:
        return None

    if not isinstance(value, int | float) or isinstance(value, bool):
        raise OptionsError(f"{where}: {value!r} is not a number")

    return float(value)


def table(value: Any, where: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise OptionsError(f"{where}: {value!r} is not a table")

    return copy.deepcopy(cast(dict[str, Any], value))


def kind(given: dict[str, Any], choices: tuple[str, ...], block: str, where: str) -> str:
    """A typed block's ``type``, one of `choices`."""
    value = given.get("type")

    if value not in choices:
        raise OptionsError(f"{where}: {block} needs a type, one of {', '.join(choices)}")

    return value


def only_keys(given: dict[str, Any], keys: tuple[str, ...], block: str, where: str) -> None:
    for name in given:
        if name not in keys:
            raise OptionsError(f"{where}: {name!r} is not a parameter of {block}")


def merge_tables(lower: Any, upper: Any) -> Any:
    """`upper` over `lower`: tables merge key by key unless `upper` names another
    ``type``; anything else replaces."""
    if not isinstance(lower, dict) or not isinstance(upper, dict):
        return copy.deepcopy(upper)

    below, above = cast(dict[str, Any], lower), cast(dict[str, Any], upper)

    if "type" in above and above["type"] != below.get("type"):
        return copy.deepcopy(above)

    merged = copy.deepcopy(below)

    for name, value in above.items():
        merged[name] = merge_tables(below.get(name), value)

    return merged


# MARK: Blocks


@dataclass(frozen=True)
class Block:
    """What every block shares: writing it back as a table."""

    def to_table(self) -> dict[str, Any]:
        """The fields set, as a TOML table."""
        return {
            item.name: copy.deepcopy(getattr(self, item.name))
            for item in fields(self)
            if getattr(self, item.name) is not None
        }


@dataclass(frozen=True)
class Instance(Block):
    """The problem: ``solomon``, a Solomon VRPTW file of the tree's ``instances/``."""

    type: str
    name: str

    TYPES: ClassVar = ("solomon",)

    @classmethod
    def from_table(cls, given: dict[str, Any], where: str) -> Self:
        only_keys(given, ("type", "name"), "instance", where)
        name = string(given.get("name"), where)

        if name is None:
            raise OptionsError(f"{where}: instance.name is missing")

        return cls(kind(given, cls.TYPES, "instance", where), name)


@dataclass(frozen=True)
class Master(Block):
    """The master solver; `parameters` go to the solver as they are."""

    type: str
    parameters: dict[str, Any]

    TYPES: ClassVar = ("highspy", "gurobipy", "highs")

    @classmethod
    def from_table(cls, given: dict[str, Any], where: str) -> Self:
        only_keys(given, ("type", "parameters"), "master", where)
        return cls(
            kind(given, cls.TYPES, "master", where), table(given.get("parameters", {}), where)
        )


@dataclass(frozen=True)
class Pricing(Block):
    """The pricing: its stages, in order; each is ``algorithm`` and any field of
    ``rcspp.graph.AlgorithmParams``."""

    stages: list[dict[str, Any]]

    @classmethod
    def from_table(cls, given: dict[str, Any], where: str) -> Self:
        only_keys(given, ("stages",), "pricing", where)
        stages = given.get("stages")

        if stages is None:
            raise OptionsError(f"{where}: pricing.stages is missing")

        if not isinstance(stages, list) or not stages:
            raise OptionsError(f"{where}: stages must be a non-empty list of tables")

        items = cast(list[Any], stages)

        for index, stage in enumerate(items):
            if not isinstance(stage, dict):
                raise OptionsError(f"{where}: stages[{index}] is not a table")

            make_algorithm_params(cast(dict[str, Any], stage), f"{where}: stages[{index}]")

        return cls(cast(list[dict[str, Any]], copy.deepcopy(items)))


@dataclass(frozen=True)
class Start(Block):
    """The columns before the first master solve.

    ``artificial``: one costly artificial column per row (big-M), from the layer; each costs
    `cost_factor` times the largest arc cost (at least 1). It has no default here: the
    presets' ``artificial`` gives 1e4, the package's own factor. ``single-customer``: one
    route per customer, from the instance; no `cost_factor`.
    """

    type: str
    cost_factor: float | None = None

    TYPES: ClassVar = ("artificial", "single-customer")

    @classmethod
    def from_table(cls, given: dict[str, Any], where: str) -> Self:
        only_keys(given, ("type", "cost_factor"), "start", where)
        start = kind(given, cls.TYPES, "start", where)
        cost_factor = number(given.get("cost_factor"), where)

        if start == "artificial":
            if cost_factor is None:
                raise OptionsError(f"{where}: start.cost_factor is missing")

            return cls(start, cost_factor)

        if cost_factor is not None:
            raise OptionsError(f"{where}: start {start} has no cost_factor")

        return cls(start)


@dataclass(frozen=True)
class Graph(Block):
    """The pricing graph's options: none yet."""

    @classmethod
    def from_table(cls, given: dict[str, Any], where: str) -> Self:
        only_keys(given, (), "graph", where)
        return cls()


@dataclass(frozen=True)
class ColumnGeneration(Block):
    """The package's own parameters, None for the package's default; tolerances as ``{
    type = ..., ...

    }`` (a name alone is read as its type).
    """

    max_iterations: int | None = None
    timeout_s: float | None = None
    tolerances: dict[str, Any] | None = None

    @classmethod
    def from_table(cls, given: dict[str, Any], where: str) -> Self:
        only_keys(given, ("max_iterations", "timeout_s", "tolerances"), "column_generation", where)
        tolerances = given.get("tolerances")

        if isinstance(tolerances, str):
            tolerances = {"type": tolerances}

        if tolerances is not None:
            tolerances = table(tolerances, where)
            check_tolerances(tolerances, where)

        return cls(
            integer(given.get("max_iterations"), where),
            number(given.get("timeout_s"), where),
            tolerances,
        )


@dataclass(frozen=True)
class Statistics(Block):
    """What a run measures besides its totals: one parameter per collector."""

    process: bool = True
    solution: bool = True

    @classmethod
    def from_table(cls, given: dict[str, Any], where: str) -> Self:
        only_keys(given, ("process", "solution"), "statistics", where)
        process = boolean(given.get("process"), where)
        solution = boolean(given.get("solution"), where)
        return cls(True if process is None else process, True if solution is None else solution)


#: The block of each option.
BLOCKS: dict[str, Any] = {
    "instance": Instance,
    "master": Master,
    "pricing": Pricing,
    "start": Start,
    "graph": Graph,
    "column_generation": ColumnGeneration,
    "statistics": Statistics,
}


# MARK: Checks with rcspp


def make_algorithm_params(stage: dict[str, Any], where: str = "stage") -> tuple[str, Any]:
    """The algorithm's name and the ``AlgorithmParams`` of a pricing stage;
    ``direction`` by name, a set as a list.

    Raises:
        OptionsError: For an unknown algorithm or field, or a value the field refuses.
    """
    from rcspp._core.graph import AlgorithmParams, SearchDirection
    from rcspp.graph import ALGORITHMS

    algorithm = stage.get("algorithm")

    if algorithm not in ALGORITHMS:
        raise OptionsError(
            f"{where}: algorithm {algorithm!r} is not one of {', '.join(ALGORITHMS)}"
        )

    params = AlgorithmParams()
    settable = {
        name
        for name, member in vars(AlgorithmParams).items()
        if isinstance(member, property) and member.fset is not None
    }

    for name, value in stage.items():
        if name == "algorithm":
            continue

        if name not in settable:
            raise OptionsError(f"{where}: {name!r} is not a field of AlgorithmParams")

        if name == "direction" and isinstance(value, str):
            directions = {
                member.name.lower(): member for member in SearchDirection.__members__.values()
            }

            if value not in directions:
                raise OptionsError(
                    f"{where}: direction {value!r} is not one of {', '.join(directions)}"
                )

            value = directions[value]

        if isinstance(value, list) and isinstance(getattr(params, name), set):
            value = set(cast(list[Any], value))

        try:
            setattr(params, name, value)
        except (TypeError, ValueError) as error:
            raise OptionsError(f"{where}: {name} = {value!r} is refused ({error})") from None

    return algorithm, params


def check_tolerances(tolerances: dict[str, Any], where: str) -> None:
    """Raises OptionsError unless the package's registry builds these tolerances."""
    from rcspp.column_generation import registry

    names = registry.tolerance_names()
    kind = tolerances.get("type")

    if kind not in names:
        raise OptionsError(f"{where}: tolerances {kind!r} is not one of {', '.join(names)}")

    parameters = {name: value for name, value in tolerances.items() if name != "type"}

    try:
        registry.make_tolerances(kind, **parameters)
    except (TypeError, ValueError) as error:
        raise OptionsError(f"{where}: {error}") from None
