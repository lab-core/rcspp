#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Where options come from: TOML files and the command line, read into plain dicts.

A **benchmark file** has sections: ``[benchmark]``, ``[defaults]``, ``[matrix]``,
``[[runs]]``, and definitions of named blocks under an option's name
(``[master.one-thread]``). A **run file**, such as a run folder's ``run.toml``, holds one
run's options at its top level, and nothing else.

On the command line, ``--name value`` (or ``--name=value``) sets an option; the value is
read as a TOML value, or as text when it is not one. Fixed flags (``--file``, ``--output``,
...) are a runner's own, declared with argparse; :func:`split_command_line` hands it the
rest.
"""

import argparse
import copy
import tomllib
from collections.abc import Mapping, Sequence
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, cast

from .schema import BLOCKS, OptionsError, describe

#: The presets shipped with the benchmark, in the format of a benchmark file.
PRESETS_FILE = Path(__file__).parent.parent / "presets.toml"

#: The sections of a benchmark file besides the definitions of blocks.
SECTIONS = ("benchmark", "defaults", "matrix", "runs")


# MARK: Files


def read_toml(path: str | Path) -> dict[str, Any]:
    """A TOML file's tables; raises OptionsError for a missing or malformed file."""
    try:
        with open(path, "rb") as file:
            return tomllib.load(file)
    except (OSError, tomllib.TOMLDecodeError) as error:
        raise OptionsError(f"{path}: {error}") from None


@dataclass
class Definitions:
    """Named blocks, by option: the presets', then a file's, which may replace them."""

    named: dict[str, dict[str, dict[str, Any]]] = field(
        default_factory=lambda: {option: {} for option in BLOCKS}
    )

    def add(self, option: str, tables: Any, where: str) -> None:
        if not isinstance(tables, dict):
            raise OptionsError(f"{where}: [{option}] holds named {option} tables")

        for name, given in cast(dict[str, Any], tables).items():
            if not isinstance(given, dict):
                raise OptionsError(f"{where}: {option} {name!r} is not a table")

            self.named[option][name] = cast(dict[str, Any], given)

    def resolve(self, option: str, value: Any, where: str) -> dict[str, Any]:
        """The table a value gives for a block: a table as it is; a name looked up as a
        definition, then a type, then the block's shorthand."""
        if isinstance(value, dict):
            return copy.deepcopy(cast(dict[str, Any], value))

        if not isinstance(value, str):
            raise OptionsError(f"{where}: {option} is a name or a table, not {value!r}")

        if value in self.named[option]:
            return copy.deepcopy(self.named[option][value])

        block = BLOCKS[option]

        if value in block.types:
            return {"type": value}

        if block.shorthand is not None:
            kind, parameter = block.shorthand
            return {"type": kind, parameter: value}

        known = sorted(self.named[option]) + list(block.types)
        raise OptionsError(f"{where}: unknown {option} {value!r} (known: {', '.join(known)})")


@dataclass
class BenchmarkFile:
    """A benchmark file read: its sections, and its definitions added to the presets'."""

    definitions: Definitions
    benchmark: dict[str, Any] = field(default_factory=dict[str, Any])
    defaults: dict[str, Any] = field(default_factory=dict[str, Any])
    matrix: dict[str, Any] | None = None
    runs: list[Any] = field(default_factory=list[Any])
    folder: Path | None = None


def read_benchmark_file(
    tables: Mapping[str, Any], definitions: Definitions, where: str, folder: Path | None = None
) -> BenchmarkFile:
    """A benchmark file's tables sorted into sections; its definitions added to
    `definitions`.

    `folder` is where the file's ``from`` paths start.
    """
    read = BenchmarkFile(definitions, folder=folder)

    for name, value in tables.items():
        if name in BLOCKS:
            definitions.add(name, value, where)
        elif name == "matrix":
            read.matrix = value
        elif name == "runs":
            if not isinstance(value, list):
                raise OptionsError(f"{where}: runs is a list of tables ([[runs]])")

            read.runs = cast(list[Any], value)
        elif name in SECTIONS:
            setattr(read, name, value)
        else:
            known = ", ".join([*SECTIONS, *BLOCKS])
            raise OptionsError(f"{where}: unknown section {name!r} (sections: {known})")

    return read


def load_presets() -> BenchmarkFile:
    """The presets: their definitions and their defaults."""
    return read_benchmark_file(read_toml(PRESETS_FILE), Definitions(), str(PRESETS_FILE))


# MARK: Command line


def parse_value(text: str) -> Any:
    """`text` read as a TOML value (``8``, ``true``, ``{ type = "gurobipy" }``), else as
    text."""
    try:
        return tomllib.loads(f"value = {text}")["value"]
    except tomllib.TOMLDecodeError:
        return text


def parse_stage(text: str) -> dict[str, Any]:
    """A stage written ``tabu:max_solutions=20,max_iterations=1000``."""
    algorithm, _, rest = text.partition(":")
    stage: dict[str, Any] = {"algorithm": algorithm}

    for item in filter(None, rest.split(",")):
        name, equals, value = item.partition("=")

        if not equals:
            raise OptionsError(f"--stage {text}: {item!r} is not name=value")

        stage[name.strip()] = parse_value(value.strip())

    return stage


def parse_assignments(arguments: Sequence[str]) -> dict[str, Any]:
    """Options given as ``--name value`` or ``--name=value``; repeated ``--stage``
    options make the pricing's stages.

    Raises:
        OptionsError: For an argument that is not an option, or an option without a value
            (a value starting with ``--`` is the next option, not a value).
    """
    assignments: dict[str, Any] = {}
    stages: list[dict[str, Any]] = []
    position = 0

    while position < len(arguments):
        argument = arguments[position]

        if not argument.startswith("--") or argument == "--":
            raise OptionsError(f"command line: {argument!r} is not an option")

        name, equals, text = argument[2:].partition("=")

        if not equals:
            if position + 1 == len(arguments) or arguments[position + 1].startswith("--"):
                raise OptionsError(f"command line: --{name} needs a value")

            position += 1
            text = arguments[position]

        if name == "stage":
            stages.append(parse_stage(text))
        else:
            assignments[name] = parse_value(text)

        position += 1

    if stages:
        if "pricing" in assignments:
            raise OptionsError("command line: give --pricing or --stage, not both")

        assignments["pricing"] = {"stages": stages}

    return assignments


def split_command_line(
    parser: argparse.ArgumentParser, arguments: Sequence[str] | None = None
) -> tuple[argparse.Namespace, dict[str, Any]]:
    """A runner's own flags, parsed by `parser`, and the options given after them.

    The parser's ``--help`` lists every option and its parameters.
    """
    parser.epilog = describe()
    parser.formatter_class = argparse.RawDescriptionHelpFormatter
    flags, rest = parser.parse_known_args(arguments)
    return flags, parse_assignments(rest)
