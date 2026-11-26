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
from dataclasses import dataclass, fields, replace
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
    """The problem: ``solomon``, a Solomon VRPTW file of the tree's ``instances/``.

    `vehicles` adds the master row "at most that many routes", the pricer's convexity row:
    the bound the column generation's lower bound needs. None: no such row, any number of
    routes. As many vehicles as customers or more leaves the LP value as it is, though not
    always the iterations: the row can be tight at a degenerate LP.
    """

    type: str
    name: str
    vehicles: int | None = None

    TYPES: ClassVar = ("solomon",)

    @classmethod
    def from_table(cls, given: dict[str, Any], where: str) -> Self:
        only_keys(given, ("type", "name", "vehicles"), "instance", where)
        name = string(given.get("name"), where)
        vehicles = integer(given.get("vehicles"), where)

        if name is None:
            raise OptionsError(f"{where}: instance.name is missing")

        if vehicles is not None and vehicles < 1:
            raise OptionsError(f"{where}: instance.vehicles must be 1 or more")

        return cls(kind(given, cls.TYPES, "instance", where), name, vehicles)


#: The solver parameters every master solve sets from the layer's own, by solver (Gurobi's
#: names in lower case: it ignores their case); where a run option sets them instead.
SET_BY_EVERY_SOLVE = {
    "highs": {
        "threads": "column_generation.threads",
        "time_limit": "column_generation.timeout_s",
        "output_flag": None,
        "solve_relaxation": None,
        "mip_rel_gap": "integer.relative_gap",
    },
    "gurobi": {
        "threads": "column_generation.threads",
        "timelimit": "column_generation.timeout_s",
        "outputflag": None,
        "mipgap": "integer.relative_gap",
    },
}


@dataclass(frozen=True)
class Master(Block):
    """The master solver; `parameters` go to the solver as they are, except those every
    solve sets (:data:`SET_BY_EVERY_SOLVE`), which are refused."""

    type: str
    parameters: dict[str, Any]

    TYPES: ClassVar = ("highspy", "gurobipy", "highs", "gurobi")

    @classmethod
    def from_table(cls, given: dict[str, Any], where: str) -> Self:
        only_keys(given, ("type", "parameters"), "master", where)
        master = kind(given, cls.TYPES, "master", where)
        parameters = table(given.get("parameters", {}), where)
        gurobi = master in ("gurobipy", "gurobi")
        set_by_every_solve = SET_BY_EVERY_SOLVE["gurobi" if gurobi else "highs"]

        for name in parameters:
            key = name.lower() if gurobi else name

            if key in set_by_every_solve:
                option = set_by_every_solve[key]
                instead = f": use {option}" if option else ""
                raise OptionsError(
                    f"{where}: master.parameters.{name} is set by every solve, from the layer's "
                    f"own parameters{instead}"
                )

        return cls(master, parameters)


@dataclass(frozen=True)
class Pricing(Block):
    """The pricing: its stages, in order; each is ``algorithm`` and any field of
    ``rcspp.graph.AlgorithmParams``. `preprocess`: whether each search reduces the graph
    first; None for the layer's default (on)."""

    stages: list[dict[str, Any]]
    preprocess: bool | None = None

    @classmethod
    def from_table(cls, given: dict[str, Any], where: str) -> Self:
        only_keys(given, ("stages", "preprocess"), "pricing", where)
        preprocess = boolean(given.get("preprocess"), where)
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

        return cls(cast(list[dict[str, Any]], copy.deepcopy(items)), preprocess)


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
    """The routes pricing searches.

    `relaxation`: ``none`` (None too: routes may visit a customer again), ``ng``
    (ng-routes: no return to a customer the route still remembers, each customer's
    `ng_size` nearest remembered) or ``elementary`` (no customer twice).
    """

    relaxation: str | None = None
    ng_size: int | None = None

    RELAXATIONS: ClassVar = ("none", "ng", "elementary")

    @classmethod
    def from_table(cls, given: dict[str, Any], where: str) -> Self:
        only_keys(given, ("relaxation", "ng_size"), "graph", where)
        relaxation = string(given.get("relaxation"), where)
        ng_size = integer(given.get("ng_size"), where)

        if relaxation is not None and relaxation not in cls.RELAXATIONS:
            raise OptionsError(
                f"{where}: relaxation {relaxation!r} is not one of {', '.join(cls.RELAXATIONS)}"
            )

        if relaxation == "ng" and ng_size is None:
            raise OptionsError(f"{where}: relaxation ng needs graph.ng_size")

        if relaxation != "ng" and ng_size is not None:
            raise OptionsError(f"{where}: graph.ng_size is for relaxation ng only")

        # A neighbourhood of the customer alone forgets it as soon as the route leaves it.
        if ng_size is not None and ng_size < 2:
            raise OptionsError(
                f"{where}: graph.ng_size must be at least 2 (1 gives the routes of "
                'relaxation = "none")'
            )

        return cls(relaxation, ng_size)


@dataclass(frozen=True)
class ColumnGeneration(Block):
    """The layer's own parameters, None for the layer's default.

    `tolerances` is a table with a ``type``, or a name alone, read as the type. `threads`
    goes to every master solve; 0 lets the solver choose. `absolute_gap`, `relative_gap`,
    `cutoff` and `integer_objective` stop the run early on its lower bound.
    """

    max_iterations: int | None = None
    timeout_s: float | None = None
    tolerances: dict[str, Any] | None = None
    threads: int | None = None
    absolute_gap: float | None = None
    relative_gap: float | None = None
    cutoff: float | None = None
    integer_objective: bool | None = None

    @classmethod
    def from_table(cls, given: dict[str, Any], where: str) -> Self:
        names = (
            "max_iterations",
            "timeout_s",
            "tolerances",
            "threads",
            "absolute_gap",
            "relative_gap",
            "cutoff",
            "integer_objective",
        )
        only_keys(given, names, "column_generation", where)
        max_iterations = integer(given.get("max_iterations"), where)
        timeout_s = number(given.get("timeout_s"), where)
        threads = integer(given.get("threads"), where)
        absolute_gap = number(given.get("absolute_gap"), where)
        relative_gap = number(given.get("relative_gap"), where)
        cutoff = number(given.get("cutoff"), where)
        integer_objective = boolean(given.get("integer_objective"), where)

        if max_iterations is not None and max_iterations < 0:
            raise OptionsError(f"{where}: column_generation.max_iterations must be 0 or more")

        # Written "not >= 0" so that nan is refused too; inf is no limit.
        if timeout_s is not None and not timeout_s >= 0:
            raise OptionsError(f"{where}: column_generation.timeout_s must be 0 or more")

        if threads is not None and threads < 0:
            raise OptionsError(f"{where}: column_generation.threads must be 0 or more")

        for name, gap in (("absolute_gap", absolute_gap), ("relative_gap", relative_gap)):
            if gap is not None and not gap >= 0:
                raise OptionsError(f"{where}: column_generation.{name} must be 0 or more")

        # Any cost, inf included (no cutoff); not nan.
        if cutoff is not None and cutoff != cutoff:
            raise OptionsError(f"{where}: column_generation.cutoff must be a number")

        tolerances = given.get("tolerances")

        if isinstance(tolerances, str):
            tolerances = {"type": tolerances}

        if tolerances is not None:
            tolerances = table(tolerances, where)
            check_tolerances(tolerances, where)

        return cls(
            max_iterations,
            timeout_s,
            tolerances,
            threads,
            absolute_gap,
            relative_gap,
            cutoff,
            integer_objective,
        )


@dataclass(frozen=True)
class Integer(Block):
    """The integer master over the generated columns, solved once column generation is
    over: a feasible solution and an upper bound, not the integer optimum (no branching).

    `solve` and `column_type` are None for the instance type's own (:meth:`for_instance`);
    `relative_gap` None for the layer's; `threads` None for ``column_generation.threads``.
    `timeout_s` is always finite: with many columns the integer solve can take far longer
    than the LP.
    """

    solve: bool | None = None
    timeout_s: float = 60.0
    relative_gap: float | None = None
    column_type: str | None = None
    threads: int | None = None

    COLUMN_TYPES: ClassVar = ("integer", "binary")

    @classmethod
    def from_table(cls, given: dict[str, Any], where: str) -> Self:
        names = ("solve", "timeout_s", "relative_gap", "column_type", "threads")
        only_keys(given, names, "integer", where)
        timeout_s = number(given.get("timeout_s"), where)
        relative_gap = number(given.get("relative_gap"), where)
        column_type = string(given.get("column_type"), where)
        threads = integer(given.get("threads"), where)

        if timeout_s is not None and not 0 < timeout_s < float("inf"):
            raise OptionsError(f"{where}: integer.timeout_s must be a positive, finite number")

        if relative_gap is not None and not relative_gap >= 0:
            raise OptionsError(f"{where}: integer.relative_gap must be 0 or more")

        if column_type is not None and column_type not in cls.COLUMN_TYPES:
            raise OptionsError(
                f"{where}: integer.column_type {column_type!r} is not one of "
                f"{', '.join(cls.COLUMN_TYPES)}"
            )

        if threads is not None and threads < 0:
            raise OptionsError(f"{where}: integer.threads must be 0 or more")

        return cls(
            boolean(given.get("solve"), where),
            cls.timeout_s if timeout_s is None else timeout_s,
            relative_gap,
            column_type,
            threads,
        )

    def for_instance(self, defaults: dict[str, Any]) -> Integer:
        """This block with the instance type's `defaults` (``solve``, ``column_type``)
        where it leaves them out."""
        return replace(
            self,
            solve=defaults["solve"] if self.solve is None else self.solve,
            column_type=defaults["column_type"] if self.column_type is None else self.column_type,
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
    "integer": Integer,
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

    # Each would fail at every pricing call, not at the start: the engine's tabu search
    # refuses the default iterations, and the heuristics search forward only.
    if algorithm == "tabu" and params.max_iterations >= AlgorithmParams().max_iterations:
        raise OptionsError(f"{where}: a tabu stage needs a finite max_iterations")

    if algorithm in ("greedy", "tabu") and params.direction != SearchDirection.Forward:
        raise OptionsError(f"{where}: a {algorithm} stage searches forward only")

    return algorithm, params


def check_tolerances(tolerances: dict[str, Any], where: str) -> None:
    """Raises OptionsError unless the package's registry builds these tolerances."""
    from rcspp.column_generation import registry

    names = registry.tolerance_names()
    name = tolerances.get("type")

    if name not in names:
        raise OptionsError(f"{where}: tolerances {name!r} is not one of {', '.join(names)}")

    parameters = {name: value for name, value in tolerances.items() if name != "type"}

    try:
        registry.make_tolerances(name, **parameters)
    except (TypeError, ValueError) as error:
        raise OptionsError(f"{where}: {error}") from None
