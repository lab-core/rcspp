#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""A run, given in parts, then complete.

:class:`RunOptions` is what one place gives: a run file, a benchmark's ``[defaults]``, one
``[[runs]]`` entry, one point of its ``[matrix]``, the command line. It holds each block as
a table, so that :meth:`RunOptions.over` merges one over another. :meth:`RunOptions.complete`
then builds a :class:`Run`: every block a checked dataclass, nothing missing.
"""

from __future__ import annotations

import copy
import hashlib
import json
from dataclasses import dataclass, field, replace
from typing import Any, cast

from ..instances import INSTANCE_TYPES
from .blocks import (
    BLOCKS,
    ColumnGeneration,
    Graph,
    Instance,
    Integer,
    Master,
    OptionsError,
    Pricing,
    Start,
    Statistics,
    merge_tables,
)

#: The masters, by the layers that have them.
LAYERS: dict[str, tuple[str, ...]] = {
    "package": ("highspy", "gurobipy"),
    "cpp-bindings": ("highs", "highspy", "gurobipy"),
    "cpp-native": ("highs", "gurobi"),
}

#: Every option of a run, in the order a run file lists them.
RUN_OPTIONS = ("instance", "layer", *(option for option in BLOCKS if option != "instance"))

#: The options every run's name shows, in that order.
NAMED_OPTIONS = ("instance", "layer", "master", "pricing", "start")

#: Named blocks, by option, then name: the presets' and a benchmark file's definitions.
Names = dict[str, dict[str, dict[str, Any]]]


class MasterNotOnLayerError(OptionsError):
    """A master the run's layer has not: dropped from a matrix, refused elsewhere."""


# MARK: Run


@dataclass(frozen=True)
class Run:
    """A complete run: what a runner reads."""

    instance: Instance
    layer: str
    master: Master
    pricing: Pricing
    start: Start
    graph: Graph
    column_generation: ColumnGeneration
    integer: Integer
    statistics: Statistics
    name: str = field(default="", compare=False)

    def to_table(self) -> dict[str, Any]:
        """The run's options, as its ``run.toml`` holds them."""
        return {
            option: self.layer if option == "layer" else getattr(self, option).to_table()
            for option in RUN_OPTIONS
        }

    def named(self, names: Names, defaults: dict[str, Any]) -> Run:
        """This run with its name: the parts of :data:`NAMED_OPTIONS`, then of every other
        block that differs from `defaults` (the presets' complete blocks, by option), joined
        by ``-``.

        The name depends only on the run's options, not on how they were written nor on
        which file's ``[defaults]`` gave them.
        """
        instance_type = self.instance.type
        parts = [
            name_part(option, getattr(self, option), names, instance_type)
            for option in NAMED_OPTIONS
        ]

        for option in RUN_OPTIONS:
            default = defaults.get(option)

            # The integer block's defaults depend on the run's instance type.
            if isinstance(default, Integer):
                default = default.for_instance(integer_defaults(instance_type))

            if option not in NAMED_OPTIONS and getattr(self, option) != default:
                parts.append(name_part(option, getattr(self, option), names, instance_type))

        return replace(self, name="-".join(parts))


def stops_without_fleet(run: Run) -> list[str]:
    """The column generation stops a run sets without ``instance.vehicles``: without the
    row that limits the routes, the lower bound stays unknown until the end, so they
    never stop the run (they change nothing)."""
    if run.instance.vehicles is not None:
        return []

    settings = run.column_generation
    stops = {
        "absolute_gap": settings.absolute_gap,
        "relative_gap": settings.relative_gap,
        "cutoff": settings.cutoff,
        "integer_objective": settings.integer_objective,
    }
    return [f"column_generation.{name}" for name, value in stops.items() if value is not None]


#: What a run with :func:`stops_without_fleet` is warned of.
STOPS_WITHOUT_FLEET = (
    "without instance.vehicles, the row that limits the routes, the lower bound is unknown "
    "until the end: these stops cannot stop the run"
)


def name_part(option: str, value: Any, names: Names, instance_type: str) -> str:
    """How an option shows in a run's name.

    The layer and the instance by their names. A block by the name of the definition or
    preset it equals (``one-thread``), else by its type when it is that type with its
    defaults (``highspy``), else by its type, or its option, and a fingerprint of its table
    (``artificial_3fa2c1``).
    """
    if option == "layer":
        return value

    if option == "instance":
        return value.name if value.vehicles is None else f"{value.name}-vehicles_{value.vehicles}"

    for name, table in names.get(option, {}).items():
        if complete_alone(option, table, instance_type) == value:
            return name

    kind = getattr(value, "type", None)

    if kind is not None and complete_alone(option, {"type": kind}, instance_type) == value:
        return kind

    return f"{kind or option}_{digest(value.to_table())}"


def complete_alone(option: str, table: dict[str, Any], instance_type: str | None = None) -> Any:
    """The block a table makes on its own, or None when it is not complete alone; an
    integer block with `instance_type`'s defaults, when given, as a run's has them."""
    try:
        block = BLOCKS[option].from_table(table, option)
    except OptionsError:
        return None

    if isinstance(block, Integer) and instance_type is not None:
        return block.for_instance(integer_defaults(instance_type))

    return block


# MARK: Run options


@dataclass(frozen=True)
class RunOptions:
    """The options one place gives: the layer, and blocks as tables; any may be absent."""

    layer: str | None = None
    blocks: dict[str, dict[str, Any]] = field(default_factory=dict[str, dict[str, Any]])
    where: str = ""

    @classmethod
    def from_table(cls, given: Any, where: str, names: Names | None = None) -> RunOptions:
        """The options a TOML table gives.

        A block is a table or a name: a named block of `names`, a type, or an instance's
        name. A quoted dotted name (``"start.cost_factor"``) sets one parameter, after the
        whole blocks.
        """
        if not isinstance(given, dict):
            raise OptionsError(f"{where}: a run is a table of options")

        given = cast(dict[str, Any], given)
        names = names or {}
        layer = None
        blocks: dict[str, dict[str, Any]] = {}

        for key, value in given.items():
            option, _, path = key.partition(".")

            if option not in RUN_OPTIONS:
                raise OptionsError(f"{where}: unknown option {key!r} ({', '.join(RUN_OPTIONS)})")

            if path:
                continue

            if option == "layer":
                if not isinstance(value, str) or value not in LAYERS:
                    raise OptionsError(
                        f"{where}: layer {value!r} is not one of {', '.join(LAYERS)}"
                    )

                layer = value
            else:
                blocks[option] = resolve(option, value, names, where)

        tolerances_as_table(blocks)

        # Dotted names after whole blocks: "master.parameters.Presolve" = 1 sets one key.
        for key, value in given.items():
            option, _, path = key.partition(".")

            if not path:
                continue

            if option == "layer":
                raise OptionsError(f"{where}: layer has no parameters")

            *outer, last = path.split(".")
            inner = blocks.setdefault(option, {})

            for name in outer:
                nested = inner.setdefault(name, {})

                if not isinstance(nested, dict):
                    raise OptionsError(f"{where}: {option}.{name} is not a table")

                inner = cast(dict[str, Any], nested)

            inner[last] = copy.deepcopy(value)

        tolerances_as_table(blocks)
        return cls(layer, blocks, where)

    def over(self, lower: RunOptions) -> RunOptions:
        """These options given over `lower`'s: a block merges into lower's, unless it
        names another ``type``."""
        blocks = dict(lower.blocks)

        for option, table in self.blocks.items():
            blocks[option] = merge_tables(lower.blocks.get(option), table)

        return RunOptions(self.layer or lower.layer, blocks, self.where or lower.where)

    def complete(self, where: str | None = None) -> Run:
        """The run these options make: every block built and checked.

        Raises:
            OptionsError: For a missing option or a wrong block.
            MasterNotOnLayerError: For a master the run's layer has not.
        """
        where = where or self.where or "run"

        if self.layer is None:
            raise OptionsError(f"{where}: layer is missing")

        for option in BLOCKS:
            if option not in self.blocks:
                raise OptionsError(f"{where}: {option} is missing")

        instance = Instance.from_table(self.blocks["instance"], where)
        integer = Integer.from_table(self.blocks["integer"], where)
        run = Run(
            instance=instance,
            layer=self.layer,
            master=Master.from_table(self.blocks["master"], where),
            pricing=Pricing.from_table(self.blocks["pricing"], where),
            start=Start.from_table(self.blocks["start"], where),
            graph=Graph.from_table(self.blocks["graph"], where),
            column_generation=ColumnGeneration.from_table(self.blocks["column_generation"], where),
            integer=integer.for_instance(integer_defaults(instance.type)),
            statistics=Statistics.from_table(self.blocks["statistics"], where),
        )

        if run.master.type not in LAYERS[run.layer]:
            raise MasterNotOnLayerError(
                f"{where}: layer {run.layer} has no master {run.master.type} "
                f"(it has {', '.join(LAYERS[run.layer])})"
            )

        return run

    def complete_blocks(self) -> dict[str, Any]:
        """Each block that is complete on its own, by option: what a run's name is
        compared with."""
        complete: dict[str, Any] = {}

        for option, table in self.blocks.items():
            block = complete_alone(option, table)

            if block is not None:
                complete[option] = block

        return complete


# MARK: Helpers


def integer_defaults(instance_type: str) -> dict[str, Any]:
    """Whether an instance type's runs solve the integer master, and with which column
    type, unless a run says otherwise."""
    return INSTANCE_TYPES[instance_type].INTEGER


def resolve(option: str, value: Any, names: Names, where: str) -> dict[str, Any]:
    """The table a block's value gives: a table as it is; a name as the named block of
    `names`, else as a type, else (for an instance) as a Solomon instance's name."""
    if isinstance(value, dict):
        return copy.deepcopy(cast(dict[str, Any], value))

    if not isinstance(value, str):
        raise OptionsError(f"{where}: {option} is a name or a table, not {value!r}")

    if value in names.get(option, {}):
        return copy.deepcopy(names[option][value])

    types: tuple[str, ...] = getattr(BLOCKS[option], "TYPES", ())

    if value in types:
        return {"type": value}

    if option == "instance":
        return {"type": "solomon", "name": value}

    known = [*names.get(option, {}), *types]
    raise OptionsError(f"{where}: unknown {option} {value!r} (known: {', '.join(known)})")


def tolerances_as_table(blocks: dict[str, dict[str, Any]]) -> None:
    """Turns tolerances given by name into a table of that type, so that a table given
    over them merges into it."""
    column_generation = blocks.get("column_generation", {})
    tolerances = column_generation.get("tolerances")

    if isinstance(tolerances, str):
        column_generation["tolerances"] = {"type": tolerances}


def digest(value: Any) -> str:
    """A short fingerprint of a value."""
    text = json.dumps(value, sort_keys=True, default=str)
    return hashlib.sha256(text.encode()).hexdigest()[:6]
