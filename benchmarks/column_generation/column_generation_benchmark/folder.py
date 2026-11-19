#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""A folder of runs, read: what the summary and the comparison share.

The reader turns a folder into cells: a cell is one run over its rounds, a round one run
folder, with the run's options, its ``result.json``, its ``metadata.json`` and how it ended.
It needs only the run folders: ``benchmark.toml`` gives the runs and their order when the
folder has one, and the benchmark's ``metadata.json`` and its events are read when present.

The figures are over the rounds in the figures: the finished ones, a failed check included.
A field whose name ends in ``_seconds`` or ``_bytes`` is a measure: its median, mean, min, max
and spread. Any other field is one value when the rounds agree, else the cell is flagged.
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
from .options import OptionsError, read_toml

#: A round's folder name: the run's name, then ``-r<round>``.
ROUND = re.compile(r"^(?P<run>.+)-r(?P<number>\d+)$")

#: How a round ended, besides the benchmark's outcomes: its folder has no result and no
#: outcome (in progress, or cut off), it has no folder, or a file of it cannot be read.
UNFINISHED = "unfinished"
MISSING = "missing"
UNREADABLE = "unreadable"

#: The outcomes of the rounds in the figures, with their result.
IN_FIGURES = (run_folder.FINISHED, run_folder.CHECK_FAILED)

#: How close two numbers of the rounds must be to agree.
RELATIVE_TOLERANCE = 1e-6

#: The time the tables show the min, max and spread of, when the results have it.
MAIN_TIME = "solve_seconds"

#: The solvers whose versions every round of a master type must share, by master type.
SOLVERS = {"highspy": "highs", "highs": "highs", "gurobipy": "gurobi", "gurobi": "gurobi"}

#: The record's groups that must be equal in every round of a folder; versions apart from
#: the solvers', which a round records only for the masters it uses.
RECORD_GROUPS = ("source", "code", "versions", "machine")
SOLVER_VERSIONS = ("highspy", "highs", "gurobipy", "gurobi")

#: A cell's first columns, before its result fields' figures, and its last ones.
CELL_COLUMNS = (
    "run",
    "layer",
    "instance",
    "master",
    "rounds",
    "finished",
    "failed_checks",
    "timeouts",
    "not_finished",
)
CELL_LAST_COLUMNS = (
    "seconds_per_iteration",
    "wall_seconds_median",
    "processor_seconds_median",
    "other_processor_percent",
    "on_battery",
    "flags",
)

MEBIBYTE = 1024 * 1024


class FolderError(Exception):
    """A folder that holds no run, or whose benchmark files cannot be read."""


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
    def in_figures(self) -> bool:
        """Whether the round is in the figures: it finished, and has its result."""
        return self.outcome in IN_FIGURES and self.result is not None


@dataclass
class Cell:
    """One run over its rounds; `options_differ` when its rounds' options are not the
    same (a folder of run folders, which names the runs by folder)."""

    run: str
    options: dict[str, Any]
    rounds: list[Round]
    options_differ: bool = False

    @property
    def results(self) -> list[dict[str, Any]]:
        """The results of the rounds in the figures."""
        return [item.result for item in self.rounds if item.in_figures and item.result is not None]


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
        FolderError: When `path` holds no run, or its ``benchmark.toml`` or
            ``metadata.json`` cannot be read.
    """
    metadata_path = path / run_folder.METADATA_FILE
    benchmark_file = path / run_folder.BENCHMARK_FILE

    if benchmark_file.is_file():
        try:
            table = read_toml(benchmark_file)
            metadata = run_folder.read_json(metadata_path) if metadata_path.is_file() else {}
        except (OptionsError, OSError, ValueError) as error:
            raise FolderError(f"{path}: cannot read its benchmark files: {error}") from None

        cells = cells_of_benchmark(path, table, metadata)

        if not cells:
            raise FolderError(f"{path} holds no run: its {run_folder.BENCHMARK_FILE} has none")

        return Folder(path, metadata, cells)

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
        raise FolderError(f"{path} holds no run")

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
    ``metadata.json`` names (else the file's repeats) and any other the folder holds."""
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
        round_numbers = sorted(set(range(1, rounds + 1)) | present.get(name, set()))
        found = [
            read_round(runs / f"{name}-r{number}", name, number, options)
            for number in round_numbers
        ]
        cells.append(Cell(name, options, found))

    return cells


def read_round(folder: Path, run: str, number: int | None, options: dict[str, Any] | None) -> Round:
    """A round from its folder; `options` None: from its ``run.toml``.

    A file that cannot be read makes the round unreadable, not the folder.
    """
    if not folder.is_dir():
        return Round(run, number, folder, options or {}, MISSING, None, {})

    try:
        metadata_path = folder / run_folder.METADATA_FILE
        metadata = run_folder.read_json(metadata_path) if metadata_path.is_file() else {}
        result = run_folder.read_result(folder)

        if options is None:
            has_file = (folder / run_folder.RUN_FILE).is_file()
            options = run_folder.read_run_file(folder) if has_file else {}
    except (OSError, ValueError):
        # JSON's and TOML's decoding errors are ValueErrors.
        return Round(run, number, folder, options or {}, UNREADABLE, None, {})

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
    """Rounds into cells by run name, in the order of their first round; a cell whose
    rounds' options differ says so."""
    cells: dict[str, Cell] = {}

    for item in rounds:
        if item.run not in cells:
            cells[item.run] = Cell(item.run, item.options, [])
        elif item.options != cells[item.run].options:
            cells[item.run].options_differ = True

        cells[item.run].rounds.append(item)

    for cell in cells.values():
        cell.rounds.sort(key=lambda item: item.number or 0)

    return list(cells.values())


# MARK: Figures


def is_measure(field: str) -> bool:
    """Whether a field is measured (its rounds vary) rather than found."""
    return field.endswith(("_seconds", "_bytes"))


def first_appearance(tables: Iterable[Mapping[str, Any]]) -> list[str]:
    """The keys of the tables, in the order they first appear."""
    keys: dict[str, None] = {}

    for table in tables:
        keys.update(dict.fromkeys(table))

    return list(keys)


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
    """How a cell's rounds ended: finished means in the figures."""
    finished = [item for item in cell.rounds if item.in_figures]
    timeouts = sum(item.outcome == run_folder.TIMEOUT for item in cell.rounds)
    return {
        "rounds": len(cell.rounds),
        "finished": len(finished),
        "failed_checks": sum(item.outcome == run_folder.CHECK_FAILED for item in finished),
        "timeouts": timeouts,
        "not_finished": len(cell.rounds) - len(finished) - timeouts,
    }


def load_values(cell: Cell, name: str) -> list[Any]:
    return [item.metadata.get("load", {}).get(name) for item in cell.rounds if item.in_figures]


def cell_row(cell: Cell) -> dict[str, Any]:
    """A cell's row of ``summary.csv``: its options' main parts and how its rounds ended
    (:data:`CELL_COLUMNS`), each result field's figures, then the time per iteration,
    the run's process, the machine's load and its flags (:data:`CELL_LAST_COLUMNS`).

    A result
    field named as one of those columns is written ``result.<field>``.
    """
    results = cell.results
    row: dict[str, Any] = {
        "run": cell.run,
        "layer": cell.options.get("layer"),
        "instance": cell.options.get("instance", {}).get("name"),
        "master": cell.options.get("master", {}).get("type"),
        **counts(cell),
    }
    taken = {*CELL_COLUMNS, *CELL_LAST_COLUMNS}
    differ: list[str] = []

    for field in first_appearance(results):
        values = [result.get(field) for result in results]
        column = f"result.{field}" if field in taken else field

        if is_measure(field):
            for name, value in asdict(measure(values)).items():
                row[f"{column}_{name}"] = value

            continue

        same, value = agreed(values)
        row[column] = value

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
            item.metadata.get(field) for item in cell.rounds if item.in_figures
        ).median

    row["other_processor_percent"] = measure(
        load_values(cell, "other_processor_percent_mean")
    ).median
    battery = [value for value in load_values(cell, "on_battery") if value is not None]
    row["on_battery"] = any(battery) if battery else None
    row["flags"] = "; ".join(cell_flags(row, differ, cell.options_differ))
    return row


def cell_flags(row: Mapping[str, Any], differ: Sequence[str], options_differ: bool) -> list[str]:
    """What to know about a cell's figures.

    The battery is the folder's flag
    (:func:`machine_flags`), said once there.
    """
    flags: list[str] = []

    if row["timeouts"]:
        flags.append(f"{row['timeouts']} timeouts")

    if row["failed_checks"]:
        flags.append(f"{row['failed_checks']} failed checks")

    if row["not_finished"]:
        flags.append(f"{row['not_finished']} not finished")

    if differ:
        flags.append(f"rounds differ: {', '.join(differ)}")

    if options_differ:
        flags.append("options differ between rounds: the first round's are shown")

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


# MARK: Text


def text(value: Any) -> str:
    """A value in a markdown table's cell: a ``|`` escaped, a line break a space."""
    if value is None:
        return "-"

    if isinstance(value, bool):
        return "yes" if value else "no"

    if isinstance(value, float):
        return f"{value:.6g}"

    return str(value).replace("|", "\\|").replace("\n", " ")


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


def aligned_table(header: Sequence[str], lines: Sequence[Sequence[str]]) -> list[str]:
    """A markdown table with its columns padded, to read on screen too."""
    widths = [max(len(line[index]) for line in [header, *lines]) for index in range(len(header))]

    def joined(cells: Sequence[str]) -> str:
        return "| " + " | ".join(cell.ljust(width) for cell, width in zip(cells, widths)) + " |"

    return [
        joined(header),
        "|" + "|".join("-" * (width + 2) for width in widths) + "|",
        *(joined(line) for line in lines),
    ]


def csv_text(
    lines: Sequence[Mapping[str, Any]], first: Sequence[str] = (), last: Sequence[str] = ()
) -> str:
    """CSV of `lines`: the columns `first`, then every other key in the order it first
    appears, then `last`; so that a column a later line adds does not land after
    `last`."""
    middle = [name for name in first_appearance(lines) if name not in first and name not in last]
    buffer = io.StringIO()
    writer = csv.DictWriter(buffer, [*first, *middle, *last], restval="", lineterminator="\n")
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
