#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""A benchmark: its own options, and the runs it makes.

A benchmark file has ``[benchmark]`` (its own options), named blocks under an option's name
(``[master.one-thread]``), and runs: ``[defaults]`` is a run every other one is put over,
``[matrix]`` is a run whose options are lists (one run per combination), and each
``[[runs]]`` entry is a run, which may start from a run file with ``from``. The presets
(``presets.toml``) hold only named blocks and ``[defaults]``, under every benchmark and run.
"""

from __future__ import annotations

import itertools
import math
import tomllib
from dataclasses import dataclass, field, replace
from pathlib import Path
from typing import Any, cast

from .blocks import BLOCKS, OptionsError, boolean, number, string
from .run import MasterNotOnLayerError, Names, Run, RunOptions, digest

#: The presets shipped with the benchmark.
PRESETS_FILE = Path(__file__).parent.parent / "presets.toml"

#: The sections of a benchmark file besides its named blocks.
SECTIONS = ("benchmark", "defaults", "matrix", "runs")

#: The options of a benchmark file's ``[benchmark]``.
BENCHMARK_OPTIONS = ("processes", "repeats", "timeout", "machine", "logs")


def read_toml(path: str | Path) -> dict[str, Any]:
    """A TOML file's tables; raises OptionsError for a missing or malformed file."""
    try:
        with open(path, "rb") as file:
            return tomllib.load(file)
    except (OSError, tomllib.TOMLDecodeError) as error:
        raise OptionsError(f"{path}: {error}") from None


@dataclass(frozen=True)
class Benchmark:
    """A benchmark file read: its options, its names, and its runs, not yet put together.

    `timeout` is the seconds after which a run's process is killed (None: never), `machine`
    the machine's label for the records, `logs` whether each run keeps its output.
    """

    processes: int = 1
    repeats: int = 1
    timeout: float | None = None
    machine: str | None = None
    logs: bool = True
    names: Names = field(default_factory=dict[str, dict[str, dict[str, Any]]])
    defaults: RunOptions = field(default_factory=RunOptions)
    matrix: list[RunOptions] = field(default_factory=list[RunOptions])
    runs: list[RunOptions] = field(default_factory=list[RunOptions])

    @classmethod
    def from_table(
        cls, given: dict[str, Any], where: str = "benchmark file", folder: Path | None = None
    ) -> Benchmark:
        """The benchmark a file's tables describe, over the presets; `folder` is where
        its ``from`` paths start."""
        presets = read_presets()
        names = named_blocks(presets, str(PRESETS_FILE))

        for option, named in named_blocks(given, where).items():
            names[option].update(named)

        for name in given:
            if name not in BLOCKS and name not in SECTIONS:
                raise OptionsError(f"{where}: unknown section {name!r}")

        own = benchmark_options(given.get("benchmark", {}), where)
        preset_defaults = RunOptions.from_table(presets["defaults"], "presets: [defaults]", names)
        defaults = RunOptions.from_table(given.get("defaults", {}), f"{where}: [defaults]", names)
        points = matrix_points(given["matrix"], where) if "matrix" in given else []
        matrix = [
            RunOptions.from_table(point, f"{where}: matrix {point}", names) for point in points
        ]
        entries = given.get("runs", [])

        if not isinstance(entries, list):
            raise OptionsError(f"{where}: runs is a list of tables ([[runs]])")

        runs = [
            run_entry(entry, f"{where}: runs[{index}]", names, folder)
            for index, entry in enumerate(cast(list[Any], entries))
        ]
        return cls(
            **own,
            names=names,
            defaults=defaults.over(preset_defaults),
            matrix=matrix,
            runs=runs,
        )

    def expand(self, command_line: RunOptions | None = None) -> Expansion:
        """Every run, complete and named: each matrix point and ``[[runs]]`` entry over
        the defaults, the command line over all (without either, one run of the
        defaults).

        A matrix point whose layer has not its master, wherever the master comes from
        (the command line included), is dropped and listed; a ``[[runs]]`` entry or the
        defaults alone giving one are refused, and so is a matrix with every point dropped.
        Runs that come out the same are kept once. With more than one process, a run that
        leaves ``column_generation.threads`` unset gets 1, before it is named: runs side by
        side must not share the cores.

        Raises:
            OptionsError: For a run that cannot be made, or no run left.
        """
        command_line = command_line or RunOptions()
        entries = [(point, True) for point in self.matrix] + [(run, False) for run in self.runs]
        defaults = preset_defaults().complete_blocks()
        runs: list[Run] = []
        dropped: list[str] = []

        for entry, from_matrix in entries or [(RunOptions(), False)]:
            where = place(entry.where or self.defaults.where, command_line)

            try:
                run = command_line.over(entry.over(self.defaults)).complete(where)
            except MasterNotOnLayerError as error:
                if not from_matrix:
                    raise

                dropped.append(str(error))
                continue

            if self.processes > 1 and run.column_generation.threads is None:
                run = replace(run, column_generation=replace(run.column_generation, threads=1))

            if run not in runs:
                runs.append(run.named(self.names, defaults))

        if not runs:
            points = "1 matrix point" if len(dropped) == 1 else f"{len(dropped)} matrix points"
            raise OptionsError(f"no run left: {points} dropped ({dropped[0]})")

        return Expansion(
            self.processes,
            self.repeats,
            unique_names(runs),
            dropped,
            self.timeout,
            self.machine,
            self.logs,
        )


@dataclass(frozen=True)
class Expansion:
    """A benchmark's runs, complete and named, the matrix points dropped, and the
    benchmark's options."""

    processes: int
    repeats: int
    runs: list[Run]
    dropped: list[str]
    timeout: float | None = None
    machine: str | None = None
    logs: bool = True

    def rounds(self) -> list[tuple[int, Run]]:
        """Every run of every round, round by round; rounds count from 1."""
        return [(number, run) for number in range(1, self.repeats + 1) for run in self.runs]


def folder_name(run: Run, round_number: int) -> str:
    """The folder of a run's round: ``<name>-r<round>``."""
    return f"{run.name}-r{round_number}"


def read_presets(path: Path = PRESETS_FILE) -> dict[str, Any]:
    """The presets' tables: named blocks and ``[defaults]``, nothing else."""
    presets = read_toml(path)

    for name in presets:
        if name not in BLOCKS and name != "defaults":
            raise OptionsError(f"{path}: {name!r}: presets hold named blocks and [defaults]")

    return presets


def preset_names() -> Names:
    """The presets' named blocks: the names a run file and a runner's command line can
    use."""
    return named_blocks(read_presets(), str(PRESETS_FILE))


def preset_defaults() -> RunOptions:
    """The presets' ``[defaults]``: under every run, and what a run's name is compared
    with."""
    return RunOptions.from_table(read_presets()["defaults"], "presets: [defaults]", preset_names())


def expand_run(given: dict[str, Any] | None, command_line: RunOptions | None = None) -> Run:
    """One run, from a run file's table and the command line, over the presets'
    defaults: what a runner takes."""
    command_line = command_line or RunOptions()
    names = preset_names()
    defaults = preset_defaults()
    run = RunOptions.from_table(given or {}, "run file", names)
    where = place("run file" if given else "", command_line)
    complete = command_line.over(run.over(defaults)).complete(where)
    return complete.named(names, defaults.complete_blocks())


# MARK: Helpers


def benchmark_options(given: Any, where: str) -> dict[str, Any]:
    """The benchmark options a ``[benchmark]`` table, or the command line, gives,
    checked.

    Raises:
        OptionsError: For an unknown option or a wrong value.
    """
    if not isinstance(given, dict):
        raise OptionsError(f"{where}: [benchmark] is a table of options")

    options = cast(dict[str, Any], given)
    unknown = set(options) - set(BENCHMARK_OPTIONS)

    if unknown:
        raise OptionsError(f"{where}: unknown benchmark option {sorted(unknown)[0]!r}")

    checked: dict[str, Any] = {}

    if "processes" in options:
        checked["processes"] = positive(options["processes"], "processes", where)

    if "repeats" in options:
        checked["repeats"] = positive(options["repeats"], "repeats", where)

    if "timeout" in options:
        checked["timeout"] = positive_seconds(options["timeout"], where)

    if "machine" in options:
        checked["machine"] = string(options["machine"], f"{where}: machine")

    if "logs" in options:
        checked["logs"] = boolean(options["logs"], f"{where}: logs")

    return checked


def place(where: str, command_line: RunOptions) -> str:
    """Where a run's options come from, for its errors: `where`, and the command line
    when it gives any."""
    if command_line.layer is None and not command_line.blocks:
        return where or "presets"

    return f"{where}, with the command line" if where else "command line"


def named_blocks(tables: dict[str, Any], where: str) -> Names:
    """The named blocks of a file: ``[master.one-thread]`` and the like."""
    names: Names = {option: {} for option in BLOCKS}

    for option in BLOCKS:
        named = tables.get(option, {})

        if not isinstance(named, dict):
            raise OptionsError(f"{where}: [{option}] holds named {option} tables")

        for name, block in cast(dict[str, Any], named).items():
            if not isinstance(block, dict):
                raise OptionsError(f"{where}: [{option}] holds named {option} tables")

            names[option][name] = cast(dict[str, Any], block)

    return names


def matrix_points(matrix: Any, where: str) -> list[dict[str, Any]]:
    """Every combination of the matrix's lists, the last option varying fastest."""
    if not isinstance(matrix, dict):
        raise OptionsError(f"{where}: [matrix] is a table of lists")

    lists = cast(dict[str, Any], matrix)

    for name, values in lists.items():
        if not isinstance(values, list) or not values:
            raise OptionsError(
                f"{where}: matrix {name!r} must be a non-empty list (quote a dotted name: "
                '"start.cost_factor" = [...])'
            )

    return [dict(zip(lists, point)) for point in itertools.product(*lists.values())]


def run_entry(entry: Any, where: str, names: Names, folder: Path | None) -> RunOptions:
    """A ``[[runs]]`` entry: its options, over the run file it names with ``from``."""
    if not isinstance(entry, dict) or "from" not in entry:
        return RunOptions.from_table(entry, where, names)

    given = cast(dict[str, Any], entry)

    if not isinstance(given["from"], str):
        raise OptionsError(f"{where}: from is the path of a run file, not {given['from']!r}")

    own = {name: value for name, value in given.items() if name != "from"}
    path = Path(given["from"])
    path = path if path.is_absolute() or folder is None else folder / path
    return RunOptions.from_table(own, where, names).over(
        RunOptions.from_table(read_toml(path), str(path), names)
    )


def positive(value: Any, name: str, where: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value < 1:
        raise OptionsError(f"{where}: {name} must be a whole number of at least 1")

    return value


def positive_seconds(value: Any, where: str) -> float | None:
    seconds = number(value, f"{where}: timeout")

    if seconds is not None and not 0 < seconds < math.inf:
        raise OptionsError(f"{where}: timeout must be a positive number of seconds")

    return seconds


def unique_names(runs: list[Run]) -> list[Run]:
    """The runs, a fingerprint of their options added to every name more than one has,
    whatever its case: Windows and macOS take ``Fast`` and ``fast`` for one folder."""
    counts: dict[str, int] = {}

    for run in runs:
        counts[run.name.casefold()] = counts.get(run.name.casefold(), 0) + 1

    return [
        (
            replace(run, name=f"{run.name}-{digest(run.to_table())}")
            if counts[run.name.casefold()] > 1
            else run
        )
        for run in runs
    ]
