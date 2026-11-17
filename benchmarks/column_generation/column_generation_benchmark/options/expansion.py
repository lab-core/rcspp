#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""From sources to runs: stacking the options a run is given, then checking them.

A run's options come from a stack of **sources**, lowest first:

1. the presets' ``[defaults]``;
2. the benchmark file's ``[defaults]``;
3. the run file an entry names with ``from``, if any;
4. the run's own entry: one point of ``[matrix]``, or one ``[[runs]]`` entry;
5. the command line.

Each source is applied in two passes. First its whole values: a name or a table, resolved
to a table, then **merged** into what the lower sources gave (:func:`merge`). Then its
dotted names (``"start.cost_factor" = 1e3``), each setting one parameter.

Expanding gives every run its blocks as full tables, without names, so that a run reads the
same after a preset changes.
"""

import copy
import itertools
import json
from collections.abc import Mapping
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, cast

from .naming import run_name, unique_names
from .schema import (
    BENCHMARK_OPTIONS,
    RUN_OPTIONS,
    MasterNotOnLayerError,
    OptionsError,
    check_benchmark,
    check_block,
    check_run,
)
from .sources import BenchmarkFile, Definitions, load_presets, read_benchmark_file, read_toml


@dataclass(frozen=True)
class Source:
    """Options given in one place, and that place, for errors."""

    options: Mapping[str, Any]
    where: str


@dataclass(frozen=True)
class Run:
    """One run: its name, unique in its benchmark, and its options, expanded."""

    name: str
    options: dict[str, Any]


@dataclass(frozen=True)
class Expansion:
    """A benchmark expanded: its options, its runs, and the matrix points dropped because
    their layer has not their master."""

    benchmark: dict[str, int]
    runs: list[Run]
    dropped: list[str] = field(default_factory=list[str])

    def rounds(self) -> list[tuple[int, Run]]:
        """Every run of every round, round by round; rounds count from 1."""
        return [
            (number, run) for number in range(1, self.benchmark["repeats"] + 1) for run in self.runs
        ]


# MARK: Stacking


def merge(lower: Any, upper: Any) -> Any:
    """`upper` given over `lower`: a table merges into a table, key by key, unless it
    names another ``type``; anything else replaces."""
    if not isinstance(lower, dict) or not isinstance(upper, dict):
        return copy.deepcopy(upper)

    below, above = cast(dict[str, Any], lower), cast(dict[str, Any], upper)

    if "type" in above and above["type"] != below.get("type"):
        return copy.deepcopy(above)

    merged = copy.deepcopy(below)

    for name, value in above.items():
        merged[name] = merge(merged.get(name), value)

    return merged


def split_option_name(name: str, where: str) -> tuple[str, list[str]]:
    """An option and, for a dotted name (``start.cost_factor``), the path to a
    parameter."""
    option, *path = name.split(".")

    if option not in RUN_OPTIONS:
        raise OptionsError(f"{where}: unknown option {name!r} (options: {', '.join(RUN_OPTIONS)})")

    if path and option == "layer":
        raise OptionsError(f"{where}: layer has no parameters")

    return option, path


def apply(stack: dict[str, Any], source: Source, definitions: Definitions) -> None:
    """`source` applied to `stack`: whole values merged, then dotted names set."""
    dotted: list[tuple[str, list[str], Any]] = []

    for name, value in source.options.items():
        option, path = split_option_name(name, source.where)

        if path:
            dotted.append((option, path, value))
        elif option == "layer":
            stack[option] = value
        else:
            given = definitions.resolve(option, value, source.where)
            stack[option] = merge(stack.get(option), given)

    for option, path, value in dotted:
        inner: dict[str, Any] = stack.setdefault(option, {})

        for name in path[:-1]:
            nested = inner.setdefault(name, {})

            if not isinstance(nested, dict):
                raise OptionsError(f"{source.where}: {option}.{name} is not a table")

            inner = cast(dict[str, Any], nested)

        inner[path[-1]] = copy.deepcopy(value)


def stack_sources(definitions: Definitions, sources: list[Source]) -> dict[str, Any]:
    """The options the sources give, unchecked."""
    stack: dict[str, Any] = {}

    for source in sources:
        if not isinstance(source.options, dict):
            raise OptionsError(f"{source.where}: a table of options is needed")

        apply(stack, source, definitions)

    return stack


def checked_defaults(definitions: Definitions, sources: list[Source]) -> dict[str, Any]:
    """Each option the defaults give, checked on its own (those that do not check are
    left out); a run's name lists the blocks that differ from them."""
    checked: dict[str, Any] = {}

    for option, value in stack_sources(definitions, sources).items():
        try:
            checked[option] = value if option == "layer" else check_block(option, value, "defaults")
        except OptionsError:
            continue

    return checked


# MARK: Benchmark


def matrix_points(matrix: Any, where: str) -> list[dict[str, Any]]:
    """Every combination of the matrix's lists, the last name varying fastest."""
    if not isinstance(matrix, dict):
        raise OptionsError(f"{where}: [matrix] is a table of lists")

    lists = cast(dict[str, Any], matrix)

    for name, values in lists.items():
        if not isinstance(values, list) or not values:
            raise OptionsError(
                f"{where}: matrix {name!r} must be a non-empty list (quote a dotted name: "
                '"start.cost_factor" = [...])'
            )

    names = list(lists)
    return [dict(zip(names, point)) for point in itertools.product(*lists.values())]


def entry_sources(read: BenchmarkFile, index: int, entry: Any, where: str) -> list[Source]:
    """The sources of a ``[[runs]]`` entry: the run file it names with ``from``, then
    its own options."""
    place = f"{where}: runs[{index}]"

    if not isinstance(entry, dict):
        raise OptionsError(f"{place}: a run is a table of options")

    own = dict(cast(dict[str, Any], entry))
    sources: list[Source] = []

    if "from" in own:
        path = Path(own.pop("from"))
        path = path if path.is_absolute() or read.folder is None else read.folder / path
        sources.append(Source(read_toml(path), str(path)))

    sources.append(Source(own, place))
    return sources


def expand_benchmark(
    tables: Mapping[str, Any] | None,
    command_line: Mapping[str, Any],
    where: str = "benchmark file",
    folder: Path | None = None,
) -> Expansion:
    """A benchmark file's tables and the command line's options, expanded to runs.

    The runs are the matrix's points, then the ``[[runs]]``; without either, one run of the
    defaults. A matrix point whose layer has not its master is dropped and listed; a
    ``[[runs]]`` entry or a command line giving one is refused. Runs that expand to the same
    options are kept once.

    Raises:
        OptionsError: For any option that cannot make a run.
    """
    presets = load_presets()
    read = read_benchmark_file(tables or {}, presets.definitions, where, folder)
    run_command_line = {k: v for k, v in command_line.items() if k not in BENCHMARK_OPTIONS}
    benchmark = dict(BENCHMARK_OPTIONS)
    benchmark.update(check_benchmark(read.benchmark, f"{where}: [benchmark]"))
    benchmark.update(
        check_benchmark(
            {k: v for k, v in command_line.items() if k in BENCHMARK_OPTIONS}, "command line"
        )
    )
    defaults = [
        Source(presets.defaults, "presets: [defaults]"),
        Source(read.defaults, f"{where}: [defaults]"),
    ]
    entries: list[tuple[list[Source], bool]] = []

    if read.matrix is not None:
        entries += [
            ([Source(point, f"{where}: matrix {point}")], True)
            for point in matrix_points(read.matrix, where)
        ]

    entries += [(entry_sources(read, i, entry, where), False) for i, entry in enumerate(read.runs)]
    entries = entries or [([], False)]
    default_options = checked_defaults(read.definitions, defaults)
    runs: list[Run] = []
    dropped: list[str] = []
    seen: set[str] = set()

    for own, from_matrix in entries:
        sources = [*defaults, *own, Source(run_command_line, "command line")]
        place = own[-1].where if own else where

        try:
            options = check_run(stack_sources(read.definitions, sources), place)
        except MasterNotOnLayerError as error:
            if not from_matrix:
                raise

            dropped.append(str(error))
            continue

        fingerprint = json.dumps(options, sort_keys=True, default=str)

        if fingerprint in seen:
            continue

        seen.add(fingerprint)
        runs.append(Run(run_name(sources, options, default_options), options))

    return Expansion(benchmark, [Run(*pair) for pair in unique_names(runs)], dropped)


def expand_run(tables: Mapping[str, Any] | None, command_line: Mapping[str, Any]) -> Run:
    """One run, from a run file's options (its top level) and the command line's: what a
    runner takes."""
    presets = load_presets()

    for name in command_line:
        if name in BENCHMARK_OPTIONS:
            raise OptionsError(f"command line: --{name} is an option of a benchmark, not of a run")

    defaults = [Source(presets.defaults, "presets: [defaults]")]
    sources = [*defaults, Source(tables or {}, "run file"), Source(command_line, "command line")]
    options = check_run(stack_sources(presets.definitions, sources), "run")
    default_options = checked_defaults(presets.definitions, defaults)
    return Run(run_name(sources, options, default_options), options)
