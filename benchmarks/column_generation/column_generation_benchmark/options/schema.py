#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""What a run's options may hold, and the checks that enforce it.

Words used across the ``options`` package:

- an **option** is a top-level name of a run: ``instance``, ``layer``, ``master``, ...;
- a **block** is the table an option holds (every option but ``layer``);
- a **parameter** is a key of a block: ``cost_factor`` in ``start``.

A typed block picks its object with ``type`` (``master = { type = "gurobipy" }``) and has
parameters per type; an untyped block has one set of parameters.
"""

import copy
from collections.abc import Callable, Mapping
from dataclasses import dataclass, field
from typing import Any, cast


class OptionsError(Exception):
    """Options that cannot make a run: unknown names, wrong values, a missing option."""


# MARK: Value checks

#: A check: the value, and where it was given for the error; returns the value to keep.
Check = Callable[[Any, str], Any]


def string(value: Any, where: str) -> str:
    if not isinstance(value, str):
        raise OptionsError(f"{where}: {value!r} is not a string")

    return value


def boolean(value: Any, where: str) -> bool:
    if not isinstance(value, bool):
        raise OptionsError(f"{where}: {value!r} is not true or false")

    return value


def integer(value: Any, where: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool):
        raise OptionsError(f"{where}: {value!r} is not a whole number")

    return value


def number(value: Any, where: str) -> float:
    if not isinstance(value, int | float) or isinstance(value, bool):
        raise OptionsError(f"{where}: {value!r} is not a number")

    return float(value)


def table(value: Any, where: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise OptionsError(f"{where}: {value!r} is not a table")

    return copy.deepcopy(cast(dict[str, Any], value))


def make_algorithm_params(stage: Mapping[str, Any], where: str = "stage") -> tuple[str, Any]:
    """The algorithm's name and the ``AlgorithmParams`` of a pricing stage:
    ``algorithm`` and any field of ``rcspp.graph.AlgorithmParams``; ``direction`` by
    name, a set as a list.

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
    fields = {
        name
        for name, member in vars(AlgorithmParams).items()
        if isinstance(member, property) and member.fset is not None
    }

    for name, value in stage.items():
        if name == "algorithm":
            continue

        if name not in fields:
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


def stages(value: Any, where: str) -> list[dict[str, Any]]:
    """A non-empty list of pricing stages, each checked by building its
    ``AlgorithmParams``."""
    if not isinstance(value, list) or not value:
        raise OptionsError(f"{where}: stages must be a non-empty list of tables")

    items = cast(list[Any], value)

    for index, stage in enumerate(items):
        if not isinstance(stage, dict):
            raise OptionsError(f"{where}[{index}]: a stage is a table")

        make_algorithm_params(cast(dict[str, Any], stage), f"{where}[{index}]")

    return cast(list[dict[str, Any]], copy.deepcopy(items))


def tolerances(value: Any, where: str) -> dict[str, Any]:
    """Tolerances by name (``"relative"``) or as a table (``{ type = "relative", ...

    }``), checked by building them with the package's registry.
    """
    from rcspp.column_generation import registry

    if isinstance(value, str):
        value = {"type": value}

    if not isinstance(value, dict):
        raise OptionsError(f"{where}: tolerances are a name or a table with a type")

    given = cast(dict[str, Any], value)

    if not isinstance(given.get("type"), str):
        raise OptionsError(f"{where}: tolerances are a name or a table with a type")

    names = registry.tolerance_names()

    if given["type"] not in names:
        raise OptionsError(
            f"{where}: tolerances {given['type']!r} is not one of {', '.join(names)}"
        )

    parameters = {name: value for name, value in given.items() if name != "type"}

    try:
        registry.make_tolerances(given["type"], **parameters)
    except (TypeError, ValueError) as error:
        raise OptionsError(f"{where}: {error}") from None

    return copy.deepcopy(given)


# MARK: Blocks


@dataclass(frozen=True)
class Parameter:
    """A parameter of a block: its check, and its default (None: no default, absent when
    not given)."""

    check: Check
    default: Any = None
    required: bool = False


@dataclass(frozen=True)
class Block:
    """A block: `types` for a typed block (its parameters by type), else `parameters`.

    `shorthand` reads a name that is neither a definition nor a type as a table: an
    instance's ``"C101_25"`` is ``{ type = "solomon", name = "C101_25" }``.
    """

    types: Mapping[str, Mapping[str, Parameter]] = field(
        default_factory=dict[str, Mapping[str, Parameter]]
    )
    parameters: Mapping[str, Parameter] = field(default_factory=dict[str, Parameter])
    shorthand: tuple[str, str] | None = None

    @property
    def typed(self) -> bool:
        return bool(self.types)

    def parameters_of(self, kind: str | None) -> Mapping[str, Parameter]:
        return self.types[kind] if kind is not None else self.parameters


#: The masters, by the layers that have them; a master's name is one implementation.
LAYERS: dict[str, tuple[str, ...]] = {
    "package": ("highspy", "gurobipy"),
    "cpp-bindings": ("highs", "highspy", "gurobipy"),
}

SOLVER = {"parameters": Parameter(table, default={})}

BLOCKS: dict[str, Block] = {
    "instance": Block(
        types={"solomon": {"name": Parameter(string, required=True)}},
        shorthand=("solomon", "name"),
    ),
    "master": Block(types={"highspy": SOLVER, "gurobipy": SOLVER, "highs": SOLVER}),
    "pricing": Block(parameters={"stages": Parameter(stages, required=True)}),
    "start": Block(
        types={
            "artificial": {"cost_factor": Parameter(number, default=1e4)},
            "single-customer": {},
        }
    ),
    "graph": Block(),
    "column_generation": Block(
        parameters={
            "max_iterations": Parameter(integer),
            "timeout_s": Parameter(number),
            "tolerances": Parameter(tolerances),
        }
    ),
    "statistics": Block(parameters={"process": Parameter(boolean, default=True)}),
}

#: Every option of a run, in the order a run file lists them.
RUN_OPTIONS = ("instance", "layer", *(name for name in BLOCKS if name != "instance"))

#: The options of a benchmark, with their defaults: runs at a time, rounds of the whole list.
BENCHMARK_OPTIONS: dict[str, int] = {"processes": 1, "repeats": 1}


# MARK: Checking


def check_block(option: str, given: Mapping[str, Any], where: str) -> dict[str, Any]:
    """`given` checked as the block of `option`: its type, its parameters, their values;
    defaults filled in.

    Raises:
        OptionsError: For an unknown type or parameter, a missing one or a wrong value.
    """
    block = BLOCKS[option]
    kind = given.get("type")

    if block.typed and kind not in block.types:
        raise OptionsError(f"{where}: {option} needs a type, one of {', '.join(block.types)}")

    if not block.typed and "type" in given:
        raise OptionsError(f"{where}: {option} has no type")

    parameters = block.parameters_of(kind)
    checked: dict[str, Any] = {"type": kind} if block.typed else {}

    for name in given:
        if name != "type" and name not in parameters:
            known = ", ".join(parameters) or "none"
            raise OptionsError(f"{where}: {name!r} is not a parameter of {option} ({known})")

    for name, parameter in parameters.items():
        if name in given:
            checked[name] = parameter.check(given[name], f"{where}: {option}.{name}")
        elif parameter.required:
            raise OptionsError(f"{where}: {option}.{name} is missing")
        elif parameter.default is not None:
            checked[name] = copy.deepcopy(parameter.default)

    return checked


class MasterNotOnLayerError(OptionsError):
    """A master the run's layer does not have: dropped from a matrix, refused
    elsewhere."""


def check_run(given: Mapping[str, Any], where: str) -> dict[str, Any]:
    """A run's options, every block checked, in :data:`RUN_OPTIONS` order.

    Raises:
        OptionsError: For a missing option, an unknown layer, a master the layer does not
            have, or any wrong block.
    """
    options: dict[str, Any] = {}

    for option in RUN_OPTIONS:
        if option not in given:
            raise OptionsError(f"{where}: {option} is missing")

        if option == "layer":
            options[option] = check_layer(given[option], where)
        else:
            options[option] = check_block(option, given[option], where)

    master = options["master"]["type"]

    if master not in LAYERS[options["layer"]]:
        raise MasterNotOnLayerError(
            f"{where}: layer {options['layer']} has no master {master} "
            f"(it has {', '.join(LAYERS[options['layer']])})"
        )

    return options


def check_layer(value: Any, where: str) -> str:
    if value not in LAYERS:
        raise OptionsError(f"{where}: layer {value!r} is not one of {', '.join(LAYERS)}")

    return value


def check_benchmark(given: Mapping[str, Any], where: str) -> dict[str, int]:
    """A benchmark's options: whole numbers of at least 1."""
    checked: dict[str, int] = {}

    for name, value in given.items():
        if name not in BENCHMARK_OPTIONS:
            known = ", ".join(BENCHMARK_OPTIONS)
            raise OptionsError(f"{where}: unknown benchmark option {name!r} ({known})")

        if not isinstance(value, int) or isinstance(value, bool) or value < 1:
            raise OptionsError(f"{where}: {name} must be a whole number of at least 1")

        checked[name] = value

    return checked


def describe() -> str:
    """Every option and its parameters, for a runner's ``--help``."""
    lines = [
        "run options (--name value, or name = value in a file):",
        "  layer: " + ", ".join(LAYERS),
    ]

    for option, block in BLOCKS.items():
        if block.typed:
            for kind, parameters in block.types.items():
                lines.append(f"  {option} type {kind}: {', '.join(parameters) or 'no parameters'}")
        else:
            lines.append(f"  {option}: {', '.join(block.parameters) or 'no parameters'}")

    return "\n".join(lines)
