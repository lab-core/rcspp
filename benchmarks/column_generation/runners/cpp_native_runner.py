#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""One run of the C++ column generation example in a C++ program, without Python,
written to its run folder.

    python runners/cpp_native_runner.py --instance C101_25 --master highs
    python runners/cpp_native_runner.py --file results/<run name>/run.toml --output results/again

The program, ``cpp-native-solver`` (``cpp_native/``), is built with the example: it builds the
instance as ``instances.py`` does, its master over HiGHS or Gurobi from C++, the graph pricer
with the run's stages, the start, the loop and the integer master, and prints its result as a
line ``RESULT <json>``. This runner expands the run into the program's flags, starts it (first
with ``--check``, so that wrong options stop before the folder is touched) and writes the folder
with the steps the other runners share; the instance's check of the integer solution stays
here. Its masters are ``highs`` and ``gurobi`` (Gurobi's C++ API). :func:`main` runs the steps
in order.
"""

from __future__ import annotations

import json
import math
import subprocess  # nosec B404: this tree's own program, with arguments built here
import sys
from datetime import UTC, datetime
from pathlib import Path
from typing import Any, cast

# The code the runners share is in the folder above.
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from column_generation_benchmark import runner, source_tree  # noqa: E402
from column_generation_benchmark.instances import SolomonInstance, make_instance  # noqa: E402
from column_generation_benchmark.options import (  # noqa: E402
    OptionsError,
    Run,
    make_algorithm_params,
)

LAYER = "cpp-native"

#: The program's result fields that ``result.json`` holds as they are.
RESULT_FIELDS = (
    "status",
    "objective",
    "lower_bound",
    "iterations",
    "columns",
    "artificial_columns",
    "master_seconds",
    "pricing_seconds",
    "other_seconds",
    "solve_seconds",
    "lp_solved_again_seconds",
)


class ProgramError(Exception):
    """The program stopped with an error; its message."""


# MARK: Flags


def flag_value(value: Any) -> str:
    """A value as the program reads it: ``true``/``false``, ``inf``, a set as a comma
    list."""
    if isinstance(value, bool):
        return "true" if value else "false"

    if isinstance(value, float) and math.isinf(value):
        return "inf" if value > 0 else "-inf"

    if isinstance(value, list | tuple | set):
        items = cast("list[Any] | tuple[Any, ...] | set[Any]", value)
        return ",".join(str(item) for item in sorted(items))

    return str(value)


def option_kind(value: Any) -> str:
    """The kind of a master parameter's value: ``bool``, ``int``, ``double`` or
    ``string``."""
    if isinstance(value, bool):
        return "bool"

    if isinstance(value, int):
        return "int"

    if isinstance(value, float):
        return "double"

    return "string"


def program_arguments(run: Run, instance: SolomonInstance) -> list[str]:
    """The program's flags for the run."""
    arguments = ["--instance", str(instance.path), "--master", run.master.type]

    for name, value in run.master.parameters.items():
        arguments += ["--master-option", name, option_kind(value), flag_value(value)]

    for stage in run.pricing.stages:
        arguments += ["--stage", str(stage["algorithm"])]

        for name, value in stage.items():
            if name != "algorithm":
                arguments += ["--stage-param", name, flag_value(value)]

    if run.pricing.preprocess is False:
        arguments.append("--no-preprocess")

    arguments += ["--start", run.start.type]

    if run.instance.vehicles is not None:
        arguments += ["--vehicles", str(run.instance.vehicles)]

    if run.start.type == "artificial":
        assert run.start.cost_factor is not None
        arguments += ["--artificial-cost-factor", repr(float(run.start.cost_factor))]

    settings = run.column_generation

    if settings.max_iterations is not None:
        arguments += ["--max-iterations", str(settings.max_iterations)]

    if settings.timeout_s is not None:
        arguments += ["--timeout", flag_value(float(settings.timeout_s))]

    if settings.tolerances is not None:
        tolerances = dict(settings.tolerances)
        arguments += ["--tolerances", str(tolerances.pop("type"))]

        for name, value in tolerances.items():
            arguments += ["--tolerance", name, repr(float(value))]

    if settings.threads is not None:
        arguments += ["--threads", str(settings.threads)]

    if settings.absolute_gap is not None:
        arguments += ["--absolute-gap", flag_value(float(settings.absolute_gap))]

    if settings.relative_gap is not None:
        arguments += ["--relative-gap", flag_value(float(settings.relative_gap))]

    if settings.cutoff is not None:
        arguments += ["--cutoff", flag_value(float(settings.cutoff))]

    if settings.integer_objective:
        arguments += ["--integer-objective"]

    integer = run.integer

    if integer.solve:
        arguments += ["--integer", "--integer-timeout", repr(float(integer.timeout_s))]
        column_type = "binary" if integer.column_type == "binary" else "integer"
        arguments += ["--integer-column-type", column_type]

        if integer.relative_gap is not None:
            arguments += ["--integer-gap", repr(float(integer.relative_gap))]

        if integer.threads is not None:
            arguments += ["--integer-threads", str(integer.threads)]

    if run.statistics.solution:
        arguments.append("--solution")

    return arguments


# MARK: Running the program


def execute(program: Path, arguments: list[str]) -> tuple[int, dict[str, Any] | None, str]:
    """Runs the program: its exit code, its result (None without a ``RESULT`` line) and
    what it wrote on its error stream."""
    completed = subprocess.run(  # nosec B603: this tree's own program, arguments built here
        [str(program), *arguments], capture_output=True, text=True
    )
    found = None

    for line in completed.stdout.splitlines():
        if line.startswith("RESULT "):
            found = json.loads(line.removeprefix("RESULT "))

    return completed.returncode, found, completed.stderr.strip()


def build(run: Run, build_folder: Path) -> tuple[SolomonInstance, Path, list[str]]:
    """The instance, the program and its flags, once the program built the run's objects
    without solving (``--check``).

    Raises:
        OptionsError: For options the program refuses, or that the stages cannot take.
        SourceTreeError: When the build has not the program.
    """
    instance = make_instance(run.instance.to_table())

    if not isinstance(instance, SolomonInstance):
        raise OptionsError(f"layer {LAYER} reads Solomon instances only")

    # The stages' options are checked as the other layers check them, with their messages.
    for stage in run.pricing.stages:
        make_algorithm_params(stage)

    program = source_tree.cpp_native_program(build_folder)
    arguments = program_arguments(run, instance)
    code, _, errors = execute(program, [*arguments, "--check"])

    if code != 0:
        raise OptionsError(errors or f"{program.name} stopped with exit code {code}")

    return instance, program, arguments


def solve(program: Path, arguments: list[str]) -> dict[str, Any]:
    """The program's result for the run.

    Raises:
        ProgramError: When the program stops with an error or prints no result.
    """
    code, found, errors = execute(program, arguments)

    if code != 0 or found is None:
        raise ProgramError(errors or f"{program.name} stopped with exit code {code}")

    return found


def program_report(program: Path, found: dict[str, Any]) -> runner.ProgramReport:
    """What the program recorded and measured, for the run's record and statistics."""
    status = program.stat()
    code = {
        "path": str(program),
        "bytes": status.st_size,
        "modified": datetime.fromtimestamp(status.st_mtime, UTC).isoformat(),
        **found["build"],
    }
    process = found["process"]
    return runner.ProgramReport(
        versions=found["versions"],
        code=code,
        process={
            "process_peak_memory_bytes": process["peak_memory_bytes"],
            "process_processor_seconds": process["processor_seconds"],
        },
        processor_seconds=process["processor_seconds"],
    )


# MARK: Running


def main(arguments: list[str] | None = None) -> int:
    """One run: its options read, the program's flags built and checked, the program run,
    the folder written.

    Returns:
        The exit code: 0; 1 when the integer solution fails the instance's check, or the
        run stopped after its folder was started; 2 when the run cannot start.
    """
    arguments = sys.argv[1:] if arguments is None else arguments
    script = Path(__file__)
    parsed = runner.parse_arguments(__doc__, arguments)
    folder = None

    try:
        run, build_folder = runner.read_run(parsed, LAYER)

        # Built first: wrong options stop the runner before its folder is touched.
        instance, program, program_flags = build(run, build_folder)

        if parsed.check:
            print(f"{run.name}: built")
            return 0

        folder = runner.start_folder(parsed, run, [str(script.resolve()), *arguments])
        found = solve(program, program_flags)
        result = {name: found[name] for name in RESULT_FIELDS}
        solution: dict[str, Any] | None = found["solution"]
        fields, parts = runner.integer_results(instance, result, found["integer"])
        result.update(fields)

        if solution is not None:
            solution.update(parts)

        report = program_report(program, found)
        runner.finish_folder(folder, parsed, run, build_folder, result, solution, report)
    except (*runner.ERRORS, ProgramError) as error:
        return runner.stop(script.name, error, folder)

    return runner.report(script.name, run, result, parts)


if __name__ == "__main__":
    sys.exit(main())
