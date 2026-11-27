#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""What every runner does around its column generation calls.

A runner's ``main`` holds its layer's calls, in order, and calls these steps around them: read
the flags and the run (:func:`parse_arguments`, :func:`read_run`), fill the run folder before
the solve (:func:`start_folder`) and after it (:func:`finish_folder`), with what the instance
makes of the integer solution (:func:`integer_results`), then say how the run ended
(:func:`report`, or :func:`stop` on an error). Its flags:

- ``--file``: a run file, under the command line's run options;
- ``--output``: the run folder, ``results/<run name>`` in the benchmark's folder by default;
- ``--overwrite``: remove the run's files from the folder first when it already holds a run;
- ``--build``: a build of this tree to import rcspp from, when it has several;
- ``--machine``: the machine's label, for the record;
- ``--check build``: build the run's objects without solving, and write nothing.
"""

from __future__ import annotations

import argparse
import math
import sys
from collections.abc import Mapping, Sequence
from dataclasses import dataclass, field, replace
from datetime import UTC, datetime
from pathlib import Path
from typing import TYPE_CHECKING, Any

from . import run_folder, source_tree
from .collectors import collect
from .instances import Instance, InstanceError
from .options import (
    OptionsError,
    Run,
    add_run_options,
    expand_run,
    read_toml,
    run_from_arguments,
)
from .record import make_record

if TYPE_CHECKING:
    import gurobipy

#: Where a run's folder goes when no ``--output`` is given; git ignores it.
RESULTS = Path(__file__).resolve().parents[1] / "results"

#: What stops a runner with a message instead of a traceback.
ERRORS = (OptionsError, InstanceError, source_tree.SourceTreeError, run_folder.RunFolderError)

#: The statuses after which the master solver still holds the last LP's solution: that LP
#: finished, and pricing then found no column. After any other (a limit, a failed solve),
#: pricing may have added columns, which drop the values, or the limit stopped an LP solve:
#: the runner solves the LP again instead of reading it.
LAST_LP_FINISHED = ("Optimal", "NotProven", "Infeasible")

#: The masters over HiGHS, which keeps the thread count of a process's first solve.
HIGHS_MASTERS = ("highspy", "highs")


# MARK: A program


@dataclass(frozen=True)
class ProgramReport:
    """What a runner's program measured and recorded, for a layer that solves in a
    program of its own (``cpp-native``): the record and the process statistics are then
    that program's, not the runner's."""

    #: The solvers' versions the program links, over the runner's (``highs``, ``gurobi``).
    versions: dict[str, Any] = field(default_factory=dict[str, Any])
    #: The program: its path, size and time, configuration and compiler; ``code.program``.
    code: dict[str, Any] = field(default_factory=dict[str, Any])
    #: Its ``process`` statistic's fields, over the runner's.
    process: dict[str, Any] = field(default_factory=dict[str, Any])
    #: Its processor time, added to the runner's in ``metadata.json``.
    processor_seconds: float = 0.0


# MARK: Master rows


def gurobipy_model(lower: Sequence[float], upper: Sequence[float]) -> gurobipy.Model:
    """A gurobipy model of the master's rows, without columns, its log off."""
    import gurobipy
    from gurobipy import GRB

    model = gurobipy.Model()
    model.Params.OutputFlag = 0

    for low, up in zip(lower, upper, strict=True):
        if low == up:
            model.addLConstr(gurobipy.LinExpr(), GRB.EQUAL, low)
        elif math.isinf(up):
            # A free row (an instance's row 0): a right-hand side no column total reaches.
            model.addLConstr(gurobipy.LinExpr(), GRB.GREATER_EQUAL, max(low, -1e30))
        elif math.isinf(low):
            model.addLConstr(gurobipy.LinExpr(), GRB.LESS_EQUAL, up)
        else:
            raise InstanceError(
                f"gurobipy: a row with two different finite bounds ({low}, {up}), which this "
                "model does not hold"
            )

    return model


# MARK: Steps


def parse_arguments(description: str | None, arguments: Sequence[str]) -> argparse.Namespace:
    """A runner's flags and run options."""
    parser = argparse.ArgumentParser(
        description=description,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        allow_abbrev=False,
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
    flags.add_argument(
        "--build",
        help="a build of this tree to import rcspp from (relative to the tree's root)",
    )
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

    lp_threads = run.column_generation.threads or 0
    integer_threads = run.integer.threads

    if run.master.type in HIGHS_MASTERS and integer_threads not in (None, lp_threads):
        raise OptionsError(
            f"HiGHS keeps the thread count of a process's first solve: integer.threads "
            f"{integer_threads} cannot differ from column_generation.threads {lp_threads}"
        )

    return run, build


def now() -> str:
    return datetime.now(UTC).isoformat(timespec="milliseconds")


def processor_seconds() -> float:
    """The processor time this process has used so far."""
    import psutil

    times = psutil.Process().cpu_times()
    return times.user + times.system


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


def relative_gap(status: str, objective: float | None, integer: float | None) -> float | None:
    """(integer - LP) / |integer|, the solvers' MIP gap; None unless column generation
    ended ``Optimal``: only then is the LP a lower bound."""
    if status != "Optimal" or objective is None or integer is None:
        return None

    if integer == 0.0:
        return 0.0 if objective == 0.0 else None

    return (integer - objective) / abs(integer)


def integer_results(
    instance: Instance, result: Mapping[str, Any], integer: Mapping[str, Any] | None
) -> tuple[dict[str, Any], dict[str, Any]]:
    """The integer solve's fields of ``result.json``, and its parts of
    ``solution.json``: the raw solution, and the instance's solution resolved from it
    with the errors its check found.

    `integer` is what the runner read after the integer solve (None when it is off):
    ``status``, ``seconds``, ``objective`` (None without a solution), ``columns`` and
    ``artificial_columns``. The check's outcome is ``solution_check``: ``passed``,
    ``failed``, or null when there is no resolved solution.
    """
    if integer is None:
        fields = dict.fromkeys(
            ("integer_status", "integer_objective", "integer_seconds", "gap", "solution_check")
        )
        parts = {"integer": None, "resolved": None, "resolved_missing": "no integer solve"}
        return fields, parts

    raw = {name: value for name, value in integer.items() if name != "seconds"}
    objective = integer["objective"]
    fields: dict[str, Any] = {
        "integer_status": integer["status"],
        "integer_objective": objective,
        "integer_seconds": integer["seconds"],
        "gap": relative_gap(result["status"], result["objective"], objective),
        "solution_check": None,
    }

    if objective is None:
        return fields, {"integer": raw, "resolved": None, "resolved_missing": "no integer solution"}

    if integer["artificial_columns"]:
        missing = "the integer solution uses artificial columns"
        return fields, {"integer": raw, "resolved": None, "resolved_missing": missing}

    resolved = instance.resolve_solution(integer["columns"])
    resolved["errors"] = instance.check_solution(resolved, objective)
    fields["solution_check"] = "failed" if resolved["errors"] else "passed"
    return fields, {"integer": raw, "resolved": resolved}


def finish_folder(
    folder: Path,
    arguments: argparse.Namespace,
    run: Run,
    build: Path,
    result: Mapping[str, Any],
    solution: Mapping[str, Any] | None,
    program: ProgramReport | None = None,
) -> None:
    """The end, the process's processor time and the record in ``metadata.json``, the
    solution in ``solution.json`` when it was read, then ``result.json`` with the
    collectors' fields; with `program`, what the runner's program measured and recorded
    goes over the runner's own.

    The processor time is the benchmark's: it cannot read it once the process has ended.
    """
    fields = {**result, **collect(run.statistics.to_table(), solution)}
    lp_threads = run.column_generation.threads
    integer_threads = lp_threads if run.integer.threads is None else run.integer.threads
    record = make_record(
        build, arguments.machine, solver_threads=lp_threads, integer_threads=integer_threads
    )
    seconds = processor_seconds()

    if program is not None:
        fields.update({name: value for name, value in program.process.items() if name in fields})
        record["versions"].update(program.versions)
        record["code"]["program"] = program.code
        seconds += program.processor_seconds

    end = {"end": now(), "processor_seconds": seconds}
    run_folder.update_metadata(folder, {**end, **record})

    if solution is not None:
        run_folder.write_solution(folder, solution)

    run_folder.write_result(folder, fields)


def report(script: str, run: Run, result: Mapping[str, Any], parts: Mapping[str, Any]) -> int:
    """Prints how the run ended; the runner's exit code: 1 when the integer solution
    fails the instance's check (its folder is written all the same), else 0."""
    line = f"{run.name}: {result['status']}, objective {result['objective']}"

    if result["integer_status"] is not None:
        line += f"; integer {result['integer_status']}, objective {result['integer_objective']}"

    print(line)

    if result["solution_check"] == "failed":
        errors = "; ".join(parts["resolved"]["errors"])
        print(
            f"{script}: the integer solution fails the instance's check: {errors}", file=sys.stderr
        )
        return 1

    return 0


def stop(script: str, error: Exception, folder: Path | None) -> int:
    """Prints why the run stopped; the runner's exit code: 2 when it stopped before its
    folder was started (a benchmark then calls it not started), else 1: its folder holds
    a start but no result."""
    if folder is None:
        print(f"{script}: {error}", file=sys.stderr)
        return 2

    print(f"{script}: the run stopped after its folder was started: {error}", file=sys.stderr)
    return 1
