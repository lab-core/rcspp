#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The options the commands share: the runs (folders or ``<folder>:<run>``, then ``--where``
conditions), and where the output goes (``--output-folder``, ``--overwrite``,
``--format``)."""

import argparse
import sys
from collections.abc import Sequence
from pathlib import Path
from typing import Any

from . import analysis_folder

#: The figures' formats a command can write; the first ones by default.
FORMATS = ("png", "pdf", "svg")
DEFAULT_FORMATS = ("png", "pdf")


class CommandError(Exception):
    """A command that cannot do what it was asked: the data it needs is missing."""


def add_run_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "runs",
        nargs="+",
        metavar="RUNS",
        help="a benchmark folder, a folder of run folders, a run folder, or <folder>:<run name> "
        "(a run's name or its start)",
    )
    parser.add_argument(
        "--where",
        action="append",
        default=[],
        metavar="OPTION=VALUE",
        help="keep the runs whose dotted option has that value (graph.ng_size=8, "
        "master.type=highspy); may be given more than once",
    )


def add_output_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--output-folder",
        metavar="FOLDER",
        help="the analysis folder (default results/analysis/<command>-<date>-<time>)",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="replace the analysis the output folder already holds",
    )
    parser.add_argument(
        "--format",
        action="append",
        choices=FORMATS,
        metavar="FORMAT",
        help=f"a figure's format, {', '.join(FORMATS)}; may be given more than once "
        f"(default {' and '.join(DEFAULT_FORMATS)})",
    )


def output_folder(parsed: argparse.Namespace, command: str) -> Path:
    """The analysis folder, ready: the one asked for, else a new one under ``results``.

    Raises:
        AnalysisFolderError: When the folder cannot take the analysis.
    """
    folder = (
        analysis_folder.default_folder(command)
        if parsed.output_folder is None
        else Path(parsed.output_folder)
    )
    analysis_folder.prepare_folder(folder, parsed.overwrite)
    return folder


def formats(parsed: argparse.Namespace) -> list[str]:
    found: list[str] = parsed.format or list(DEFAULT_FORMATS)
    return list(dict.fromkeys(found))


def options_of(parsed: argparse.Namespace) -> dict[str, Any]:
    """The options a command resolved, for its record: every argument but the runs and
    the output's."""
    left_out = {"run", "command", "runs", "output_folder", "overwrite"}
    return {name: value for name, value in vars(parsed).items() if name not in left_out}


def warn(command: str, warnings: Sequence[str]) -> None:
    for text in warnings:
        print(f"analysis.py {command}: warning: {text}", file=sys.stderr)
