#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The summary of a benchmark folder, read with :mod:`.folder`. It writes three files into
the folder:

- ``summary.md``: the folder's state, its flags, the records of its rounds, then per cell
  the results, the times, and the rounds not in the figures;
- ``summary.csv``: one row per cell (:func:`.folder.cell_row`);
- ``runs.csv``: one row per round, for plots and other tools: the round, its options as
  dotted columns, its result, then what its ``metadata.json`` holds.
"""

from collections import Counter
from collections.abc import Sequence
from pathlib import Path
from typing import Any

from . import run_folder
from .folder import (
    CELL_COLUMNS,
    CELL_LAST_COLUMNS,
    MAIN_TIME,
    MEBIBYTE,
    SOLVER_VERSIONS,
    Cell,
    Folder,
    Round,
    agreed,
    aligned_table,
    cell_row,
    csv_text,
    disagreement,
    first_appearance,
    flatten,
    is_measure,
    machine_flags,
    read_folder,
    record_differences,
    records,
    solver_flags,
    solver_versions,
    text,
)

SUMMARY_FILE = "summary.md"
SUMMARY_TABLE = "summary.csv"
RUNS_TABLE = "runs.csv"

#: What ``runs.csv`` takes from a round's ``metadata.json``, besides the outcome.
METADATA_COLUMNS = ("exit_code", "started", "wall_seconds", "processor_seconds")
METADATA_GROUPS = ("benchmark", "load", "source", "versions")

#: A round's first columns in ``runs.csv``.
ROUND_COLUMNS = ("run", "round", "folder", "outcome")


# MARK: Text


def seconds(value: Any) -> str:
    return "-" if value is None else f"{value:.3f}"


def summary_text(folder: Folder, pairs: Sequence[tuple[Cell, dict[str, Any]]]) -> str:
    """``summary.md``, from the folder and each cell with its row."""
    total = Counter[str]()

    for _, row in pairs:
        total.update(
            {
                name: row[name]
                for name in ("rounds", "finished", "failed_checks", "timeouts", "not_finished")
            }
        )

    lines = [f"# Summary of {folder.path.name}", ""]
    metadata = folder.metadata
    lines.append(
        f"- {len(folder.cells)} runs, {metadata.get('rounds', '-')} rounds, "
        f"{metadata.get('processes', '-')} processes"
    )
    lines.append(
        f"- {total['finished']} / {total['rounds']} rounds finished "
        f"({total['failed_checks']} failed checks), "
        f"{total['timeouts']} timeouts, {total['not_finished']} not finished"
    )
    events: list[dict[str, Any]] = metadata.get("events") or []

    if events:
        last = events[-1]
        lines.append(f"- last event: {last.get('event')} at {last.get('time')}")

    lines += ["", "## Flags", ""]
    flags = folder_flags(folder, pairs)
    lines += [f"- {flag}" for flag in flags] if flags else ["None."]
    lines += ["", *records_text(folder)]
    lines += ["", "## Results", "", *results_table(pairs)]
    lines += ["", "## Times", "", *times_table(pairs)]
    left = [item for item in folder.rounds if not item.in_figures]

    if left:
        # Timeouts included: their time is a limit, not a measure.
        lines += ["", "## Rounds not in the figures", ""]
        lines += aligned_table(
            ("round", "outcome", "exit code"),
            [
                [text(item.folder.name), text(item.outcome), text(item.metadata.get("exit_code"))]
                for item in left
            ],
        )

    return "\n".join(lines) + "\n"


def folder_flags(folder: Folder, pairs: Sequence[tuple[Cell, dict[str, Any]]]) -> list[str]:
    """What to know before reading the folder's figures: the folder's, then each
    cell's."""
    flags = machine_flags(folder)
    found = records(folder)

    if len(found) > 1:
        flags.append(f"mixed: {len(found)} records (below)")

    flags += solver_flags(folder)
    flags += [f"{cell.run}: {row['flags']}" for cell, row in pairs if row["flags"]]
    return flags


def records_text(folder: Folder) -> list[str]:
    found = records(folder)

    if not found:
        return []

    lines = ["## Records", ""]
    first, rounds = found[0]
    flat = flatten(first)
    shown = (
        "source.commit",
        "source.uncommitted_changes",
        "source.branch",
        "machine.label",
        "machine.processor",
    )
    lines += [f"- {name}: {text(flat.get(name))}" for name in shown]
    versions: dict[str, Any] = first.get("versions") or {}
    lines.append(f"- versions: {', '.join(f'{name} {value}' for name, value in versions.items())}")
    solvers = solver_versions(folder)
    # A solver no round recorded, from the benchmark's own record: it imports every master.
    benchmark: dict[str, Any] = folder.metadata.get("record", {}).get("versions") or {}

    for name in SOLVER_VERSIONS:
        if name not in solvers and benchmark.get(name) is not None:
            solvers[name] = Counter([str(benchmark[name])])

    if solvers:
        listed = [
            f"{name} {' / '.join(solvers[name])}" for name in SOLVER_VERSIONS if name in solvers
        ]
        lines.append(f"- solvers: {', '.join(listed)}")

    if len(found) > 1:
        lines.append(f"- {len(rounds)} rounds have it")

    for number, (record, others) in enumerate(found[1:], start=2):
        lines += [
            "",
            f"Record {number}, {len(others)} rounds: "
            + "; ".join(record_differences(first, record)),
        ]
        lines += [f"- {item.folder.name}" for item in others]

    return lines


def results_table(pairs: Sequence[tuple[Cell, dict[str, Any]]]) -> list[str]:
    fields = [
        field
        for field in first_appearance(result for cell, _ in pairs for result in cell.results)
        if not is_measure(field)
    ]
    lines: list[list[str]] = []

    for cell, row in pairs:
        line = [text(cell.run), f"{row['finished']} / {row['rounds']}"]

        for field in fields:
            same, value = agreed([result.get(field) for result in cell.results])
            line.append(text(value) if same else disagreement(cell.results, field))

        lines.append(line)

    return aligned_table(("run", "finished", *fields), lines)


def times_table(pairs: Sequence[tuple[Cell, dict[str, Any]]]) -> list[str]:
    fields = [
        field
        for field in first_appearance(result for cell, _ in pairs for result in cell.results)
        if is_measure(field)
    ]
    main = (
        MAIN_TIME
        if MAIN_TIME in fields
        else next((field for field in fields if field.endswith("_seconds")), None)
    )
    header = ["run"]

    if main is not None:
        header += [f"{main} median", "min", "max", "spread %"]

    others = [field for field in fields if field != main]
    header += [
        f"{field.removesuffix('_bytes')} MiB" if field.endswith("_bytes") else field
        for field in others
    ]
    header += ["ms / iteration", "other CPU %"]
    lines: list[list[str]] = []

    for cell, row in pairs:
        line = [text(cell.run)]

        if main is not None:
            spread = row.get(f"{main}_spread_percent")
            line += [seconds(row.get(f"{main}_{name}")) for name in ("median", "min", "max")]
            line.append("-" if spread is None else f"{spread:.1f}")

        for field in others:
            value = row.get(f"{field}_median")

            if field.endswith("_bytes"):
                line.append("-" if value is None else f"{value / MEBIBYTE:.1f}")
            else:
                line.append(seconds(value))

        per_iteration = row.get("seconds_per_iteration")
        load = row.get("other_processor_percent")
        line.append("-" if per_iteration is None else f"{per_iteration * 1000:.2f}")
        line.append("-" if load is None else f"{load:.0f}")
        lines.append(line)

    return [
        "Medians over the finished rounds, in seconds unless said; spread: (max - min) / median.",
        "",
        *aligned_table(header, lines),
    ]


# MARK: Tables


def round_parts(item: Round, root: Path) -> list[dict[str, Any]]:
    """A round's row of ``runs.csv``, in its groups: the round, its options, its result,
    its metadata.

    A result field named as another column is written ``result.<field>``.
    """
    folder = item.folder.relative_to(root) if item.folder.is_relative_to(root) else item.folder
    identity = {
        "run": item.run,
        "round": item.number,
        "folder": folder.as_posix(),
        "outcome": item.outcome,
    }
    options = flatten(item.options)
    metadata: dict[str, Any] = {name: item.metadata.get(name) for name in METADATA_COLUMNS}

    for group_name in METADATA_GROUPS:
        if item.metadata.get(group_name):
            metadata.update(flatten(item.metadata[group_name], group_name))

    taken = {*identity, *options, *metadata}
    result = {
        (f"result.{name}" if name in taken else name): value
        for name, value in (item.result or {}).items()
    }
    return [identity, options, result, metadata]


def runs_csv(folder: Folder, root: Path) -> str:
    """``runs.csv``: its columns group by group, whatever round first has a column."""
    parts = [round_parts(item, root) for item in folder.rounds]
    columns: list[str] = []

    # The round, its options, its result, its metadata.
    for index in range(4):
        found = first_appearance(part[index] for part in parts)
        columns += [name for name in found if name not in columns]

    lines = [{name: value for part in found for name, value in part.items()} for found in parts]
    return csv_text(lines, first=columns)


# MARK: Writing


def write_summary(path: Path) -> list[Path]:
    """Writes ``summary.md``, ``summary.csv`` and ``runs.csv`` into the folder; returns
    their paths.

    Raises:
        FolderError: When the folder holds no run, or its benchmark files cannot be read.
    """
    folder = read_folder(path)
    pairs = [(cell, cell_row(cell)) for cell in folder.cells]
    written = [path / SUMMARY_FILE, path / SUMMARY_TABLE, path / RUNS_TABLE]
    run_folder.write_atomically(written[0], summary_text(folder, pairs))
    run_folder.write_atomically(
        written[1], csv_text([row for _, row in pairs], CELL_COLUMNS, CELL_LAST_COLUMNS)
    )
    run_folder.write_atomically(written[2], runs_csv(folder, path))
    return written
