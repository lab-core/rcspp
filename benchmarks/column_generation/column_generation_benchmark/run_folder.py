#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The folder of one run, which its runner fills.

- ``run.toml``: the run's expanded options; ``<runner> --file run.toml`` runs it again.
- ``command.txt``: the command line that started the run.
- ``metadata.json``: start, end and the record of the run (:mod:`.record`); the benchmark adds
  the exit code and the load.
- ``solution.json``: the last LP solution, when the ``solution`` statistic is on.
- ``result.json``: the fields of :data:`RESULT_FIELDS`, and those of the collectors that ran.
  Infinite numbers are written as JSON's ``Infinity``, which Python reads back.

``result.json`` is written last, and whole: a folder that has one holds a finished run. A
folder that holds any of these files holds a run: a runner refuses it unless told to remove
it first (:func:`prepare_folder`).
"""

import json
import os
import shlex
import shutil
import subprocess  # nosec B404: only to quote a command line on Windows
import sys
import tempfile
from collections.abc import Mapping, Sequence
from pathlib import Path
from typing import Any

from .collectors import COLLECTORS
from .options import OptionsError, read_toml

RUN_FILE = "run.toml"
COMMAND_FILE = "command.txt"
METADATA_FILE = "metadata.json"
RESULT_FILE = "result.json"
SOLUTION_FILE = "solution.json"

#: The files a runner writes: a folder with any of them holds a run.
RUN_FILES = (RUN_FILE, COMMAND_FILE, METADATA_FILE, SOLUTION_FILE, RESULT_FILE)

#: The fields of every ``result.json``, with their kinds; ``objective`` is null when no master
#: LP was solved, the integer solve's fields when it is off.
RESULT_FIELDS: dict[str, str] = {
    "status": "string",
    "objective": "number or null",
    "iterations": "integer",
    "columns": "integer",
    "artificial_columns": "integer",
    "master_seconds": "number",
    "pricing_seconds": "number",
    "other_seconds": "number",
    "solve_seconds": "number",
    "lp_solved_again_seconds": "number or null",
    "integer_status": "string or null",
    "integer_objective": "number or null",
    "integer_seconds": "number or null",
    "gap": "number or null",
    "solution_check": "string or null",
}

#: The fields a collector may add, with their kinds.
COLLECTOR_FIELDS: dict[str, str] = {
    name: kind for collector in COLLECTORS.values() for name, kind in collector.fields.items()
}


class RunFolderError(Exception):
    """An output folder that already holds a run."""


# MARK: Writing


def prepare_folder(folder: Path, overwrite: bool = False) -> None:
    """Makes `folder` ready for a run: made when missing; when it holds a run, refused,
    or removed first with `overwrite`. A folder without run files is kept as it is.

    Raises:
        RunFolderError: When `folder` holds a run and `overwrite` is not set.
    """
    found = [name for name in RUN_FILES if (folder / name).exists()]

    if found and not overwrite:
        raise RunFolderError(
            f"{folder} already holds a run ({', '.join(found)}): choose another folder, or "
            "remove it first with --overwrite"
        )

    if found:
        shutil.rmtree(folder)

    folder.mkdir(parents=True, exist_ok=True)


def write_atomically(path: Path, text: str) -> None:
    """Writes `text` to `path` through a temporary file, so that `path` is never half
    written."""
    path.parent.mkdir(parents=True, exist_ok=True)
    handle, temporary = tempfile.mkstemp(dir=path.parent, prefix=f".{path.name}.")

    try:
        with os.fdopen(handle, "w", encoding="utf-8") as file:
            file.write(text)

        os.replace(temporary, path)
    except BaseException:
        os.unlink(temporary)
        raise


def write_json(path: Path, value: Mapping[str, Any]) -> None:
    write_atomically(path, json.dumps(value, indent=2) + "\n")


def read_json(path: Path) -> dict[str, Any]:
    with open(path, encoding="utf-8") as file:
        value: dict[str, Any] = json.load(file)
        return value


def write_run_file(folder: Path, options: Mapping[str, Any]) -> None:
    """``run.toml``: the run's expanded options at its top level."""
    import tomli_w

    write_atomically(folder / RUN_FILE, tomli_w.dumps(dict(options)))


def read_run_file(folder: Path) -> dict[str, Any]:
    """The options of a run folder's ``run.toml``, as a runner's ``--file`` reads
    them."""
    return read_toml(folder / RUN_FILE)


def command_line(arguments: Sequence[str]) -> str:
    """`arguments` quoted for this system's shell."""
    if sys.platform == "win32":
        return subprocess.list2cmdline(list(arguments))

    return shlex.join(arguments)


def write_command(folder: Path, arguments: Sequence[str]) -> None:
    """``command.txt``: the command line that started the run, the interpreter first."""
    write_atomically(folder / COMMAND_FILE, command_line(arguments) + "\n")


def update_metadata(folder: Path, fields: Mapping[str, Any]) -> dict[str, Any]:
    """Adds `fields` to ``metadata.json`` (made when missing); returns all of it."""
    path = folder / METADATA_FILE
    metadata = read_json(path) if path.is_file() else {}
    metadata.update(fields)
    write_json(path, metadata)
    return metadata


# MARK: Results


def kind_matches(kind: str, value: Any) -> bool:
    """Whether `value` is of `kind`, as :data:`RESULT_FIELDS` names kinds."""
    if kind.endswith(" or null"):
        if value is None:
            return True

        kind = kind.removesuffix(" or null")

    if kind == "string":
        return isinstance(value, str)

    if isinstance(value, bool):
        return False

    if kind == "integer":
        return isinstance(value, int)

    return isinstance(value, int | float)


def result_problems(result: Mapping[str, Any]) -> list[str]:
    """What is wrong with a ``result.json``: missing or unknown fields, wrong kinds."""
    known = {**RESULT_FIELDS, **COLLECTOR_FIELDS}
    problems = [f"{name} is missing" for name in RESULT_FIELDS if name not in result]

    for name, value in result.items():
        if name not in known:
            problems.append(f"{name} is not a result field")
        elif not kind_matches(known[name], value):
            problems.append(f"{name} = {value!r} is not {known[name]}")

    return problems


def write_result(folder: Path, result: Mapping[str, Any]) -> None:
    """``result.json``, checked against the fields.

    Raises:
        OptionsError: When a field is missing, unknown or of the wrong kind.
    """
    problems = result_problems(result)

    if problems:
        raise OptionsError(f"{folder / RESULT_FILE}: {'; '.join(problems)}")

    write_json(folder / RESULT_FILE, result)


def write_solution(folder: Path, solution: Mapping[str, Any]) -> None:
    """``solution.json``: the solution as the runner read it from its master."""
    write_json(folder / SOLUTION_FILE, solution)


def read_result(folder: Path) -> dict[str, Any] | None:
    """The run's result, or None when it has not finished."""
    path = folder / RESULT_FILE
    return read_json(path) if path.is_file() else None
