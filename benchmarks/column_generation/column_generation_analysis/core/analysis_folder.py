#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The folder an analysis writes, so that its result can be found and made again.

- ``analysis.json``: the command and its arguments as typed, the options it resolved, when, the
  source tree (commit, uncommitted changes and their digest, branch), the versions of Python,
  numpy, pandas and matplotlib, the inputs, the outputs and the warnings;
- ``command.txt``: the command line that makes it again, the interpreter first;
- the outputs: tables, plots, and the data each plot was drawn from.

The inputs are referenced, not copied: each folder as given and resolved, and each round read
with a digest of each of its files, which tells when a run folder changed since (a resumed
benchmark, a run done again).

By default the folder is ``results/analysis/<command>-<date>-<time>`` in the benchmark's
folder, which git ignores. A folder that holds an analysis is refused, or, with ``overwrite``,
that analysis's files are removed first; a folder that holds anything else is refused.
"""

import hashlib
import platform
import sys
from collections.abc import Mapping, Sequence
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

from column_generation_benchmark import record, run_folder
from column_generation_benchmark.folder import Folder

from .selection import benchmark_name

RECORD_FILE = "analysis.json"
COMMAND_FILE = "command.txt"

#: Where the analyses go by default: the benchmark's results, which git ignores.
RESULTS = Path(__file__).resolve().parents[2] / "results" / "analysis"

#: The packages whose versions an analysis records.
PACKAGES = ("numpy", "pandas", "matplotlib")


class AnalysisFolderError(Exception):
    """An output folder that cannot take the analysis."""


def default_folder(command: str, now: datetime | None = None) -> Path:
    """``results/analysis/<command>-<date>-<time>``, in local time."""
    now = now or datetime.now()
    return RESULTS / f"{command}-{now:%Y%m%d-%H%M%S}"


def prepare_folder(folder: Path, overwrite: bool = False) -> None:
    """Makes `folder` ready for an analysis: made when missing; when it holds one,
    refused, or that analysis's files removed first with `overwrite`; other files are
    kept.

    Raises:
        AnalysisFolderError: When `folder` holds an analysis and `overwrite` is not set,
            or holds files but no analysis: `overwrite` never removes what an analysis did
            not write.
    """
    record_path = folder / RECORD_FILE

    if record_path.is_file():
        if not overwrite:
            raise AnalysisFolderError(
                f"{folder} already holds an analysis: choose another folder, or replace it "
                "with --overwrite"
            )

        try:
            outputs = run_folder.read_json(record_path).get("outputs", [])
        except (OSError, ValueError) as error:
            raise AnalysisFolderError(f"{record_path} cannot be read: {error}") from None

        inside = folder.resolve()

        for name in [*outputs, RECORD_FILE, COMMAND_FILE]:
            path = (folder / name).resolve()

            # Only the analysis's own files, inside its folder.
            if path.is_relative_to(inside):
                path.unlink(missing_ok=True)
    elif folder.is_dir() and any(folder.iterdir()):
        raise AnalysisFolderError(f"{folder} holds files but no analysis: choose another folder")

    folder.mkdir(parents=True, exist_ok=True)


def write_record(
    folder: Path,
    command: str,
    arguments: Sequence[str],
    options: Mapping[str, Any],
    inputs: Sequence[Folder],
    outputs: Sequence[str],
    warnings: Sequence[str],
) -> None:
    """``analysis.json`` and ``command.txt``; `arguments` are the command line's, the
    script first, without the interpreter; `outputs` the files written, relative to
    `folder`."""
    found = {
        "command": command,
        "arguments": list(arguments),
        "options": dict(options),
        "written": datetime.now(UTC).isoformat(timespec="seconds"),
        "source": record.source_record(),
        "versions": {
            "python": platform.python_version(),
            **{name: record.package_version(name) for name in PACKAGES},
        },
        "inputs": inputs_record(inputs),
        "outputs": list(outputs),
        "warnings": list(warnings),
    }
    run_folder.write_json(folder / RECORD_FILE, found)
    run_folder.write_atomically(
        folder / COMMAND_FILE, run_folder.command_line([sys.executable, *arguments]) + "\n"
    )


def inputs_record(inputs: Sequence[Folder]) -> list[dict[str, Any]]:
    """Each folder as given and resolved, with each round read and its files'
    digests."""
    found: list[dict[str, Any]] = []

    for folder in inputs:
        rounds: list[dict[str, Any]] = []

        for item in folder.rounds:
            files = {
                name: digest(item.folder / name)
                for name in run_folder.RUN_FILES
                if (item.folder / name).is_file()
            }
            where = item.folder.relative_to(folder.path) if item.folder != folder.path else Path()
            rounds.append(
                {"run": item.run, "round": item.number, "folder": where.as_posix(), "files": files}
            )

        found.append(
            {
                "benchmark": benchmark_name(folder),
                "path": folder.path.resolve().as_posix(),
                "rounds": rounds,
            }
        )

    return found


def digest(path: Path) -> str:
    """The first 12 hexadecimal digits of the file's SHA-256, as the source record's
    digest."""
    return hashlib.sha256(path.read_bytes()).hexdigest()[:12]
