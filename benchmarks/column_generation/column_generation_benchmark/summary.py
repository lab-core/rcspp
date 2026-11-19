#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The summary of a benchmark folder, and the reader it shares with other tools.

The reader turns a folder into cells: a cell is one run over its rounds, a round one run
folder, with the run's options, its ``result.json``, its ``metadata.json`` and how it ended.
It needs only the run folders: ``benchmark.toml`` gives the runs and their order when the
folder has one, and the benchmark's ``metadata.json`` and its events are read when present.

The summary writes three files into the folder:

- ``summary.md``: the folder's state, its flags, the records of its rounds, then per cell
  the results, the times, and the rounds that did not finish;
- ``summary.csv``: one row per cell;
- ``runs.csv``: one row per round, the options as dotted columns, for plots and other tools.

The figures are over the finished rounds, a failed check included. A field whose name ends
in ``_seconds`` or ``_bytes`` is a measure: its median, mean, min, max and spread. Any other field
is one value when the rounds agree, else the cell is flagged.
"""

import csv
import io
import json
import math
import re
import statistics
from collections import Counter
from collections.abc import Callable, Iterable, Mapping, Sequence
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, cast

from . import run_folder
from .options import read_toml

SUMMARY_FILE = "summary.md"
SUMMARY_TABLE = "summary.csv"
RUNS_TABLE = "runs.csv"

#: A round's folder name: the run's name, then ``-r<round>``.
ROUND = re.compile(r"^(?P<run>.+)-r(?P<number>\d+)$")

#: How a round ended, besides the benchmark's outcomes: its folder has no result and no
#: outcome (in progress, or cut off), or it has no folder.
UNFINISHED = "unfinished"
MISSING = "missing"

#: The outcomes counted in the figures.
COUNTED = (run_folder.FINISHED, run_folder.CHECK_FAILED)

#: How close two numbers of the rounds must be to agree.
RELATIVE_TOLERANCE = 1e-6

#: The time the times table shows the min, max and spread of, when the results have it.
MAIN_TIME = "solve_seconds"

#: The solvers whose versions every round of a master type must share, by master type.
SOLVERS = {"highspy": "highs", "highs": "highs", "gurobipy": "gurobi", "gurobi": "gurobi"}

#: The record's groups that must be equal in every round of a folder; versions apart from
#: the solvers', which a round records only for the masters it uses.
RECORD_GROUPS = ("source", "code", "versions", "machine")
SOLVER_VERSIONS = ("highspy", "highs", "gurobipy", "gurobi")

#: What ``runs.csv`` takes from a round's ``metadata.json``, besides the outcome.
METADATA_COLUMNS = ("exit_code", "started", "wall_seconds", "processor_seconds")
METADATA_GROUPS = ("benchmark", "load", "source", "versions")

MEBIBYTE = 1024 * 1024


class SummaryError(Exception):
    """A folder that holds no run."""


@dataclass
class Round:
    """One run folder: what it holds, and how it ended."""

    run: str
    number: int | None
    folder: Path
    options: dict[str, Any]
    outcome: str
    result: dict[str, Any] | None
    metadata: dict[str, Any]

    @property
    def counted(self) -> bool:
        """Whether the round is in the figures: it finished."""
        return self.outcome in COUNTED and self.result is not None


@dataclass
class Cell:
    """One run over its rounds."""

    run: str
    options: dict[str, Any]
    rounds: list[Round]

    @property
    def counted(self) -> list[dict[str, Any]]:
        """The results of the finished rounds."""
        return [item.result for item in self.rounds if item.counted and item.result is not None]


@dataclass
class Folder:
    """A benchmark folder: its own ``metadata.json`` (empty without one), and its cells."""

    path: Path
    metadata: dict[str, Any]
    cells: list[Cell]

    @property
    def rounds(self) -> list[Round]:
        return [item for cell in self.cells for item in cell.rounds]


# MARK: Reading


def read_folder(path: Path) -> Folder:
    """The cells of a benchmark folder, or of a folder of run folders.

    Raises:
        SummaryError: When `path` holds no run.
    """
    metadata_path = path / run_folder.METADATA_FILE
    benchmark_file = path / run_folder.BENCHMARK_FILE

    if benchmark_file.is_file():
        metadata = run_folder.read_json(metadata_path) if metadata_path.is_file() else {}
        return Folder(path, metadata, cells_of_benchmark(path, read_toml(benchmark_file), metadata))

    if (path / run_folder.RUN_FILE).is_file():
        # One run folder.
        return Folder(path, {}, group([read_round(path, path.name, None, None)]))

    runs = path / run_folder.RUNS_FOLDER
    parent = runs if runs.is_dir() else path
    folders = (
        sorted(item for item in parent.iterdir() if item.is_dir() and holds_run(item))
        if parent.is_dir()
        else []
    )

    if not folders:
        raise SummaryError(f"{path} holds no run")

    found: list[Round] = []

    for folder in folders:
        match = ROUND.match(folder.name)
        name = match["run"] if match else folder.name
        number = int(match["number"]) if match else None
        found.append(read_round(folder, name, number, None))

    return Folder(path, {}, group(found))


def holds_run(folder: Path) -> bool:
    return any((folder / name).exists() for name in run_folder.RUN_FILES)


def cells_of_benchmark(
    path: Path, table: Mapping[str, Any], metadata: Mapping[str, Any]
) -> list[Cell]:
    """The cells of ``benchmark.toml``, in its order, each with every round the folder's
    ``metadata.json`` names and any other the folder holds."""
    runs = path / run_folder.RUNS_FOLDER
    present: dict[str, set[int]] = {}

    if runs.is_dir():
        for folder in runs.iterdir():
            match = ROUND.match(folder.name)

            if match:
                present.setdefault(match["run"], set()).add(int(match["number"]))

    rounds = int(metadata.get("rounds") or table.get("benchmark", {}).get("repeats", 1))
    cells: list[Cell] = []

    for entry in table.get("runs", []):
        name = entry["name"]
        options = {key: value for key, value in entry.items() if key != "name"}
        numbers = sorted(set(range(1, rounds + 1)) | present.get(name, set()))
        found = [
            read_round(runs / f"{name}-r{number}", name, number, options) for number in numbers
        ]
        cells.append(Cell(name, options, found))

    return cells


def read_round(folder: Path, run: str, number: int | None, options: dict[str, Any] | None) -> Round:
    """A round from its folder; `options` None: from its ``run.toml``."""
    if not folder.is_dir():
        return Round(run, number, folder, options or {}, MISSING, None, {})

    metadata_path = folder / run_folder.METADATA_FILE
    metadata = run_folder.read_json(metadata_path) if metadata_path.is_file() else {}
    result = run_folder.read_result(folder)

    if options is None:
        has_file = (folder / run_folder.RUN_FILE).is_file()
        options = run_folder.read_run_file(folder) if has_file else {}

    return Round(run, number, folder, options, outcome_of(metadata, result), result, metadata)


def outcome_of(metadata: Mapping[str, Any], result: Mapping[str, Any] | None) -> str:
    """The benchmark's outcome, or, for a run folder its runner wrote alone, one from
    its result."""
    outcome = metadata.get("outcome")

    if outcome is not None:
        return str(outcome)

    if result is None:
        return UNFINISHED

    if result.get("status") == run_folder.TIMEOUT:
        return run_folder.TIMEOUT

    return (
        run_folder.CHECK_FAILED if result.get("solution_check") == "failed" else run_folder.FINISHED
    )


def group(rounds: Iterable[Round]) -> list[Cell]:
    """Rounds into cells by run name, in the order of their first round."""
    cells: dict[str, Cell] = {}

    for item in rounds:
        if item.run not in cells:
            cells[item.run] = Cell(item.run, item.options, [])

        cells[item.run].rounds.append(item)

    for cell in cells.values():
        cell.rounds.sort(key=lambda item: item.number or 0)

    return list(cells.values())


# MARK: Figures


def is_measure(field: str) -> bool:
    """Whether a field is measured (its rounds vary) rather than found."""
    return field.endswith(("_seconds", "_bytes"))


def result_fields(results: Iterable[Mapping[str, Any]]) -> list[str]:
    """The fields of the results, in the order they first appear."""
    fields: dict[str, None] = {}

    for result in results:
        fields.update(dict.fromkeys(result))

    return list(fields)


def numbers(values: Iterable[Any]) -> list[float]:
    return [
        float(value)
        for value in values
        if isinstance(value, int | float) and not isinstance(value, bool)
    ]


@dataclass(frozen=True)
class Measure:
    """The figures of a measured field over the rounds; None without any value.

    The
    spread is max - min, in percent of the median.
    """

    median: float | None = None
    mean: float | None = None
    min: float | None = None
    max: float | None = None
    spread_percent: float | None = None


def measure(values: Iterable[Any]) -> Measure:
    """The figures of the numbers among `values`."""
    found = numbers(values)

    if not found:
        return Measure()

    median = statistics.median(found)
    spread = (max(found) - min(found)) / median * 100 if median > 0 else None
    return Measure(median, statistics.fmean(found), min(found), max(found), spread)


def agreed(values: Sequence[Any]) -> tuple[bool, Any]:
    """Whether the values agree (numbers to :data:`RELATIVE_TOLERANCE`), and the value
    they agree on (None without values)."""
    if not values:
        return True, None

    first = values[0]

    for value in values[1:]:
        both_numbers = len(numbers([first, value])) == 2

        if both_numbers and not math.isclose(
            first, value, rel_tol=RELATIVE_TOLERANCE, abs_tol=1e-12
        ):
            return False, None

        if not both_numbers and (value != first or type(value) is not type(first)):
            return False, None

    return True, first


def counts(cell: Cell) -> dict[str, int]:
    """How a cell's rounds ended."""
    outcomes = Counter(item.outcome for item in cell.rounds)
    finished = outcomes[run_folder.FINISHED] + outcomes[run_folder.CHECK_FAILED]
    timeouts = outcomes[run_folder.TIMEOUT]
    return {
        "rounds": len(cell.rounds),
        "finished": finished,
        "failed_checks": outcomes[run_folder.CHECK_FAILED],
        "timeouts": timeouts,
        "not_finished": len(cell.rounds) - finished - timeouts,
    }


def load_values(cell: Cell, name: str) -> list[Any]:
    return [item.metadata.get("load", {}).get(name) for item in cell.rounds if item.counted]


def cell_row(cell: Cell) -> dict[str, Any]:
    """A cell's row of ``summary.csv``: its options' main parts, how its rounds ended,
    each result field's figures, the run's process and the machine's load, and its
    flags."""
    results = cell.counted
    row: dict[str, Any] = {
        "run": cell.run,
        "layer": cell.options.get("layer"),
        "instance": cell.options.get("instance", {}).get("name"),
        "master": cell.options.get("master", {}).get("type"),
        **counts(cell),
    }
    differ: list[str] = []

    for field in result_fields(results):
        values = [result.get(field) for result in results]

        if is_measure(field):
            for name, value in asdict(measure(values)).items():
                row[f"{field}_{name}"] = value

            continue

        same, value = agreed(values)
        row[field] = value

        if not same:
            differ.append(field)

    # Each round's own average, so that rounds with other iteration counts have one too.
    row["seconds_per_iteration"] = measure(
        result[MAIN_TIME] / result["iterations"]
        for result in results
        if numbers([result.get(MAIN_TIME)])
        and isinstance(result.get("iterations"), int)
        and result["iterations"] > 0
    ).median

    for field in ("wall_seconds", "processor_seconds"):
        row[f"{field}_median"] = measure(
            item.metadata.get(field) for item in cell.rounds if item.counted
        ).median

    row["other_processor_percent"] = measure(
        load_values(cell, "other_processor_percent_mean")
    ).median
    battery = [value for value in load_values(cell, "on_battery") if value is not None]
    row["on_battery"] = any(battery) if battery else None
    row["flags"] = "; ".join(cell_flags(row, differ))
    return row


def cell_flags(row: Mapping[str, Any], differ: Sequence[str]) -> list[str]:
    flags: list[str] = []

    if row["timeouts"]:
        flags.append(f"{row['timeouts']} timeouts")

    if row["failed_checks"]:
        flags.append(f"{row['failed_checks']} failed checks")

    if row["not_finished"]:
        flags.append(f"{row['not_finished']} not finished")

    if differ:
        flags.append(f"rounds differ: {', '.join(differ)}")

    if row["on_battery"]:
        flags.append("on battery")

    return flags


# MARK: Records


def record_of(item: Round) -> dict[str, Any]:
    """The part of a round's record that every round of a folder must share."""
    record = {group: item.metadata.get(group) for group in RECORD_GROUPS}
    versions: dict[str, Any] | None = record.get("versions")

    if versions is not None:
        record["versions"] = {
            name: value for name, value in versions.items() if name not in SOLVER_VERSIONS
        }

    return record


def flatten(value: Any, prefix: str = "") -> dict[str, Any]:
    """Nested tables as dotted names; lists stay values."""
    if not isinstance(value, dict) or (not value and prefix):
        return {prefix: value} if prefix else {}

    table = cast(dict[str, Any], value)
    found: dict[str, Any] = {}

    for key, inner in table.items():
        found.update(flatten(inner, f"{prefix}.{key}" if prefix else str(key)))

    return found


def records(folder: Folder) -> list[tuple[dict[str, Any], list[Round]]]:
    """The records of the folder's rounds, each with the rounds that have it, the most
    common first."""
    found: dict[str, tuple[dict[str, Any], list[Round]]] = {}

    for item in folder.rounds:
        if not item.metadata:
            continue

        record = record_of(item)
        key = json.dumps(record, sort_keys=True)
        found.setdefault(key, (record, []))[1].append(item)

    return sorted(found.values(), key=lambda pair: -len(pair[1]))


def record_differences(
    first: Mapping[str, Any], other: Mapping[str, Any], shown: Callable[[Any], str] = repr
) -> list[str]:
    """The dotted names whose values differ, each with both values as `shown` writes
    them."""
    old, new = flatten(dict(first)), flatten(dict(other))
    return [
        f"{name}: {shown(old.get(name))} -> {shown(new.get(name))}"
        for name in sorted(set(old) | set(new))
        if old.get(name) != new.get(name)
    ]


def solver_versions(folder: Folder) -> dict[str, Counter[str]]:
    """Each solver's versions over the folder's rounds, with their counts of rounds."""
    versions: dict[str, Counter[str]] = {}

    for item in folder.rounds:
        recorded: dict[str, Any] = item.metadata.get("versions") or {}

        for name in SOLVER_VERSIONS:
            if recorded.get(name) is not None:
                versions.setdefault(name, Counter())[str(recorded[name])] += 1

    return versions


def solver_flags(folder: Folder) -> list[str]:
    """The solvers' versions: one per solver over the folder, so that layers compare on
    the same solver; and the rounds whose master's solver version was not recorded."""
    versions = solver_versions(folder)
    unknown: Counter[str] = Counter()

    for item in folder.rounds:
        if not item.metadata:
            continue

        recorded: dict[str, Any] = item.metadata.get("versions") or {}
        master = item.options.get("master", {}).get("type")
        solver = SOLVERS.get(master or "")

        if solver is not None and recorded.get(solver) is None:
            unknown[f"{item.options.get('layer')} with master {master}"] += 1

    flags = [
        f"{name} versions differ: "
        + ", ".join(f"{version} ({count} rounds)" for version, count in found.most_common())
        for name, found in versions.items()
        if len(found) > 1
    ]
    flags += [
        f"solver version not recorded: {what} ({count} rounds)" for what, count in unknown.items()
    ]
    return flags


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


def machine_flags(folder: Folder) -> list[str]:
    """How the machine was shared: runs at a time, and rounds on battery."""
    flags: list[str] = []
    processes = {
        item.metadata.get("benchmark", {}).get("processes")
        for item in folder.rounds
        if item.metadata
    }
    processes.add(folder.metadata.get("processes"))
    many = sorted(value for value in processes if isinstance(value, int) and value > 1)

    if many:
        flags.append(
            f"{', '.join(map(str, many))} runs at a time: each run shared the machine with others, "
            "and the runs that left column_generation.threads unset ran with 1 thread"
        )

    battery = sum(bool(item.metadata.get("load", {}).get("on_battery")) for item in folder.rounds)

    if battery:
        flags.append(f"{battery} rounds ran on battery")

    return flags


def rows(folder: Folder) -> list[tuple[Cell, dict[str, Any]]]:
    return [(cell, cell_row(cell)) for cell in folder.cells]


# MARK: Text


def text(value: Any) -> str:
    """A value in a markdown cell."""
    if value is None:
        return "-"

    if isinstance(value, bool):
        return "yes" if value else "no"

    if isinstance(value, float):
        return f"{value:.6g}"

    return str(value)


def seconds(value: Any) -> str:
    return "-" if value is None else f"{value:.3f}"


def disagreement(results: Sequence[Mapping[str, Any]], field: str) -> str:
    """How the rounds' values of a field differ: ``min..max`` for numbers, else each
    value with its count."""
    values = [result.get(field) for result in results]
    found = numbers(values)

    if len(found) == len(values):
        return f"{text(min(found))}..{text(max(found))}"

    return ", ".join(
        f"{text(value)} x{count}" for value, count in Counter(map(text, values)).items()
    )


def table(header: Sequence[str], lines: Iterable[Sequence[str]]) -> list[str]:
    found = [f"| {' | '.join(header)} |", f"|{'---|' * len(header)}"]
    found += [f"| {' | '.join(line)} |" for line in lines]
    return found


def summary_text(folder: Folder) -> str:
    """``summary.md``."""
    pairs = rows(folder)
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
    left = [item for item in folder.rounds if not item.counted]

    if left:
        lines += ["", "## Rounds not finished", ""]
        lines += table(
            ("round", "outcome", "exit code"),
            (
                (item.folder.name, item.outcome, text(item.metadata.get("exit_code")))
                for item in left
            ),
        )

    return "\n".join(lines) + "\n"


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
        for field in result_fields(result for cell, _ in pairs for result in cell.counted)
        if not is_measure(field)
    ]
    lines: list[list[str]] = []

    for cell, row in pairs:
        line = [cell.run, f"{row['finished']} / {row['rounds']}"]

        for field in fields:
            same, value = agreed([result.get(field) for result in cell.counted])
            line.append(text(value) if same else disagreement(cell.counted, field))

        lines.append(line)

    return table(("run", "finished", *fields), lines)


def times_table(pairs: Sequence[tuple[Cell, dict[str, Any]]]) -> list[str]:
    fields = [
        field
        for field in result_fields(result for cell, _ in pairs for result in cell.counted)
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
        line = [cell.run]

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
        *table(header, lines),
    ]


# MARK: Tables


def csv_text(lines: Sequence[Mapping[str, Any]]) -> str:
    """CSV of `lines`, their keys as the columns in the order they first appear."""
    columns = result_fields(lines)
    buffer = io.StringIO()
    writer = csv.DictWriter(buffer, columns, restval="", lineterminator="\n")
    writer.writeheader()

    for line in lines:
        writer.writerow({name: csv_value(value) for name, value in line.items()})

    return buffer.getvalue()


def csv_value(value: Any) -> Any:
    if value is None:
        return ""

    if isinstance(value, list | dict):
        return json.dumps(value)

    return value


def round_row(item: Round, root: Path) -> dict[str, Any]:
    """A round's row of ``runs.csv``."""
    folder = item.folder.relative_to(root) if item.folder.is_relative_to(root) else item.folder
    row: dict[str, Any] = {
        "run": item.run,
        "round": item.number,
        "folder": folder.as_posix(),
        "outcome": item.outcome,
    }
    row.update(flatten(item.options))
    row.update(item.result or {})
    row.update({name: item.metadata.get(name) for name in METADATA_COLUMNS})

    for group_name in METADATA_GROUPS:
        if item.metadata.get(group_name):
            row.update(flatten(item.metadata[group_name], group_name))

    return row


# MARK: Writing


def write_summary(path: Path) -> list[Path]:
    """Writes ``summary.md``, ``summary.csv`` and ``runs.csv`` into the folder; returns
    their paths.

    Raises:
        SummaryError: When the folder holds no run.
    """
    folder = read_folder(path)
    written = [path / SUMMARY_FILE, path / SUMMARY_TABLE, path / RUNS_TABLE]
    run_folder.write_atomically(written[0], summary_text(folder))
    run_folder.write_atomically(written[1], csv_text([row for _, row in rows(folder)]))
    run_folder.write_atomically(
        written[2], csv_text([round_row(item, path) for item in folder.rounds])
    )
    return written
