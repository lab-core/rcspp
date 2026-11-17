#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""What every runner does around its column generation calls.

A runner reads its flags and its run (:func:`parse_arguments`, :func:`read_run`), fills its run
folder before the solve (:func:`start_folder`) and after it (:func:`finish_folder`). Its flags:

- ``--file``: a run file, under the command line's run options;
- ``--output``: the run folder, ``results/<run name>`` in the benchmark's folder by default;
- ``--overwrite``: remove the run folder first when it already holds a run;
- ``--build``: the build folder of this tree to import rcspp from, when it has several;
- ``--machine``: the machine's label, for the record;
- ``--check build``: build the run's objects without solving, and write nothing.
"""

from __future__ import annotations

import argparse
import sys
from collections.abc import Mapping, Sequence
from dataclasses import replace
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

from . import run_folder, source_tree
from .collectors import collect
from .instances import InstanceError
from .options import (
    OptionsError,
    Run,
    add_run_options,
    expand_run,
    read_toml,
    run_from_arguments,
)
from .record import make_record

#: Where a run's folder goes when no ``--output`` is given; git ignores it.
RESULTS = Path(__file__).resolve().parents[1] / "results"

#: What stops a runner with a message instead of a traceback.
ERRORS = (OptionsError, InstanceError, source_tree.SourceTreeError, run_folder.RunFolderError)

#: The parameter that sets a master solver's threads, by master.
THREAD_PARAMETERS = {"highspy": "threads", "gurobipy": "Threads", "highs": "threads"}


def parse_arguments(description: str | None, arguments: Sequence[str]) -> argparse.Namespace:
    """A runner's flags and run options."""
    parser = argparse.ArgumentParser(
        description=description, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    flags = parser.add_argument_group("flags")
    flags.add_argument("--file", help="a run file; the command line's run options go over it")
    flags.add_argument(
        "--output", help="the run folder (default: results/<run name> in the benchmark's folder)"
    )
    flags.add_argument(
        "--overwrite",
        action="store_true",
        help="remove the run folder first when it already holds a run",
    )
    flags.add_argument("--build", help="the build folder of this tree to import rcspp from")
    flags.add_argument("--machine", help="the machine's label, for the record")
    flags.add_argument(
        "--check",
        choices=("build",),
        help="build the instance, graph, master, pricer and starting columns, without solving",
    )
    add_run_options(parser)
    return parser.parse_args(arguments)


def read_run(arguments: argparse.Namespace, layer: str) -> tuple[Run, Path]:
    """The run the flags and options give, and the build rcspp is imported from. The
    run's layer is the runner's unless the run file or the command line names one.

    Raises:
        OptionsError: For wrong options, or a run of another layer.
        SourceTreeError: When rcspp cannot come from a build of this tree.
    """
    build = source_tree.use_tree(arguments.build)
    given = read_toml(arguments.file) if arguments.file else None
    command_line = run_from_arguments(arguments)

    if command_line.layer is None and "layer" not in (given or {}):
        command_line = replace(command_line, layer=layer)

    run = expand_run(given, command_line)

    if run.layer != layer:
        raise OptionsError(f"this runner runs layer {layer}, not {run.layer}")

    return run, build


def now() -> str:
    return datetime.now(UTC).isoformat(timespec="milliseconds")


def start_folder(arguments: argparse.Namespace, run: Run, command: Sequence[str]) -> Path:
    """The run folder, with ``run.toml``, ``command.txt`` and the start in
    ``metadata.json``.

    Raises:
        RunFolderError: When the folder already holds a run and ``--overwrite`` is not given.
    """
    folder = Path(arguments.output) if arguments.output else RESULTS / run.name
    run_folder.prepare_folder(folder, arguments.overwrite)
    run_folder.write_run_file(folder, run.to_table())
    run_folder.write_command(folder, [sys.executable, *command])
    run_folder.update_metadata(folder, {"start": now()})
    return folder


def finish_folder(
    folder: Path,
    arguments: argparse.Namespace,
    run: Run,
    build: Path,
    result: Mapping[str, Any],
    solution: Mapping[str, Any] | None,
) -> None:
    """The end and the record in ``metadata.json``, the solution in ``solution.json``
    when it was read, then ``result.json`` with the collectors' fields."""
    fields = {**result, **collect(run.statistics.to_table(), solution)}
    threads = run.master.parameters.get(THREAD_PARAMETERS[run.master.type])
    record = make_record(build, arguments.machine, solver_threads=threads)
    run_folder.update_metadata(folder, {"end": now(), **record})

    if solution is not None:
        run_folder.write_solution(folder, solution)

    run_folder.write_result(folder, fields)
