#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The command line's options, declared with argparse from the blocks' dataclasses.

Every option is ``--<option> <value>`` and every parameter ``--<option>.<parameter> <value>``
(one level: a deeper one is given as a table, ``--master.parameters "{ Presolve = 1 }"``); a
value is read as a TOML value (``8``, ``true``, ``{ Presolve = 1 }``), else as text.
``--stage`` gives the pricing's stages, in order. A runner adds its own flags to the parser.
"""

from __future__ import annotations

import argparse
import tomllib
from dataclasses import fields, replace
from typing import Any

from .benchmark import BENCHMARK_OPTIONS, Benchmark, benchmark_options, preset_names
from .blocks import BLOCKS, OptionsError
from .run import RUN_OPTIONS, Names, RunOptions


def toml_value(text: str) -> Any:
    """`text` read as one TOML value, else as text."""
    try:
        document = tomllib.loads(f"value = {text}")
    except tomllib.TOMLDecodeError:
        return text

    # "1\nx = 2" reads as two keys: it is not one value.
    return document["value"] if len(document) == 1 else text


def stage(text: str) -> dict[str, Any]:
    """A stage written ``tabu:max_solutions=20,tabu_exempt_node_ids=[0,26]``."""
    algorithm, _, rest = text.partition(":")
    parsed: dict[str, Any] = {"algorithm": algorithm.strip()}

    for item in split_outside_brackets(rest):
        name, equals, value = item.partition("=")

        if not equals:
            raise argparse.ArgumentTypeError(f"{item!r} is not name=value")

        parsed[name.strip()] = toml_value(value.strip())

    return parsed


def split_outside_brackets(text: str) -> list[str]:
    """The parts of `text` between the commas outside brackets and braces, blank ones
    left out: ``a=1,b=[2,3]`` gives ``a=1`` and ``b=[2,3]``."""
    parts: list[str] = []
    depth = start = 0

    for index, character in enumerate(text):
        if character in "[{":
            depth += 1
        elif character in "]}":
            depth -= 1
        elif character == "," and depth == 0:
            parts.append(text[start:index])
            start = index + 1

    parts.append(text[start:])
    return [part for part in parts if part.strip()]


def add_run_options(parser: argparse.ArgumentParser) -> None:
    """Declares every run option and every block parameter on `parser`."""
    group = parser.add_argument_group("run options (a name, or a TOML table for a block)")

    for option in RUN_OPTIONS:
        group.add_argument(f"--{option}", dest=option, type=toml_value, metavar="VALUE")

    group = parser.add_argument_group("block parameters")

    for option, block in BLOCKS.items():
        for item in fields(block):
            if item.name not in ("type", "stages"):
                name = f"{option}.{item.name}"
                group.add_argument(f"--{name}", dest=name, type=toml_value, metavar="VALUE")

    group.add_argument(
        "--stage",
        dest="stages",
        action="append",
        type=stage,
        metavar="ALGORITHM[:NAME=VALUE,...]",
        help="a pricing stage; repeat it for the next ones",
    )


def add_benchmark_options(parser: argparse.ArgumentParser) -> None:
    """Declares a benchmark's own options on `parser`."""
    group = parser.add_argument_group("benchmark options")
    group.add_argument("--processes", type=int, help="runs at a time")
    group.add_argument("--repeats", type=int, help="rounds of the whole list of runs")
    group.add_argument(
        "--timeout", type=float, help="seconds after which a run's process is killed"
    )
    group.add_argument("--machine", help="the machine's label, for the records")
    group.add_argument(
        "--no-logs",
        dest="logs",
        action="store_false",
        default=None,
        help="do not keep each run's output in its log.txt",
    )


def benchmark_from_arguments(benchmark: Benchmark, arguments: argparse.Namespace) -> Benchmark:
    """`benchmark` with the benchmark options the command line gives over its file's.

    Raises:
        OptionsError: For a value its file would not take.
    """
    given = {
        name: getattr(arguments, name)
        for name in BENCHMARK_OPTIONS
        if getattr(arguments, name, None) is not None
    }
    return replace(benchmark, **benchmark_options(given, "command line"))


def run_from_arguments(arguments: argparse.Namespace, names: Names | None = None) -> RunOptions:
    """The run options the command line gives; a name can be a preset's, or one of
    `names` (a benchmark file's, which include the presets')."""
    names = names if names is not None else preset_names()
    given = {
        name: value
        for name, value in vars(arguments).items()
        if value is not None and name.partition(".")[0] in RUN_OPTIONS
    }

    if arguments.stages:
        if "pricing" in given:
            raise OptionsError("command line: give --pricing or --stage, not both")

        given["pricing"] = {"stages": arguments.stages}

    return RunOptions.from_table(given, "command line", names)
