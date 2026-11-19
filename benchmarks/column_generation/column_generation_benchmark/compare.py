#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The comparison of two runs: A, the run checked, against B, the reference.

A side is one run over its rounds, read with :mod:`.folder` from a round's folder, a run
folder, or ``<benchmark folder>:<run name>``. The comparison lists the options and the
records that differ and what to know about the timings, then every result field side by
side, over each side's finished rounds:

- a measured field (its name ends in ``_seconds`` or ``_bytes``): each side's median, A - B
  and its percent of B, and whether the two sides' ranges (min..max) overlap, that is,
  whether the difference is within the rounds' spread;
- any other field should match: equal (numbers to 1e-6 relative) or differs.

The time rule, when asked for, judges A: every round of both sides finished, both
``Optimal`` with the same objective (to 1e-6 relative), then A's median solve time may exceed
B's by at most max(margin x B, floor).
"""

from collections import Counter
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from . import run_folder
from .folder import (
    MAIN_TIME,
    MEBIBYTE,
    SOLVER_VERSIONS,
    Cell,
    Folder,
    agreed,
    aligned_table,
    cell_row,
    counts,
    csv_text,
    disagreement,
    first_appearance,
    is_measure,
    machine_flags,
    measure,
    numbers,
    read_folder,
    record_differences,
    records,
    solver_flags,
    solver_versions,
    text,
)

OPTIMAL = "Optimal"
PER_ITERATION = "seconds_per_iteration"

#: The kinds of field.
MEASURED = "measured"
MATCHED = "should match"

#: How a measured field's difference compares with the rounds' spread.
WITHIN_SPREAD = "within spread"
BEYOND_SPREAD = "beyond spread"
NO_SPREAD = "1 round, no spread"

#: The time rule's verdicts.
EQUIVALENT = "equivalent"
SLOWER = "slower"
NO_VERDICT = "no verdict"

#: The columns of a comparison's CSV, one row per field.
COLUMNS = (
    "field",
    "kind",
    "a",
    "b",
    "a_min",
    "a_max",
    "b_min",
    "b_max",
    "difference",
    "percent",
    "judgement",
)


class CompareError(Exception):
    """An argument that names no single run."""


@dataclass(frozen=True)
class Side:
    """One side of the comparison: its name, its run over its rounds, the folder it was
    read from with this run alone, and the run's row of figures."""

    name: str
    cell: Cell
    folder: Folder
    row: dict[str, Any]


@dataclass(frozen=True)
class TimeRule:
    """A's median solve time may exceed B's by at most max(margin x B, floor)."""

    margin_percent: float = 5.0
    floor_seconds: float = 0.25

    def allowed(self, reference: float) -> float:
        return max(self.margin_percent / 100 * reference, self.floor_seconds)


@dataclass(frozen=True)
class Comparison:
    """What the comparison found; `verdict` is the time rule's verdict and its reason,
    None without the rule."""

    a: Side
    b: Side
    options: list[str]
    records: list[str]
    warnings: list[str]
    rows: list[dict[str, Any]]
    verdict: tuple[str, str] | None


# MARK: Reading


def read_side(argument: str) -> Side:
    """The run an argument names: a folder holding one run, or ``<folder>:<run name>``, the
    name in full or a prefix that only it has.

    Raises:
        CompareError: When the argument names no single run.
        FolderError: When its folder holds no run.
    """
    path = Path(argument)

    # An existing folder first: a Windows path has a colon after its drive letter.
    if path.is_dir():
        folder = read_folder(path)

        if len(folder.cells) != 1:
            raise CompareError(
                f"{argument} holds {len(folder.cells)} runs; name one as {argument}:<run name>"
                + "".join(f"\n  {cell.run}" for cell in folder.cells)
            )

        cell = folder.cells[0]
        own = (path / run_folder.RUN_FILE).is_file()
        # Resolved: "." has no name.
        name = path.resolve().name if own else f"{path.resolve().name} / {cell.run}"
        return Side(name, cell, Folder(folder.path, folder.metadata, [cell]), cell_row(cell))

    base, separator, run = argument.rpartition(":")

    if not separator or not run or "/" in run or "\\" in run or not Path(base).is_dir():
        raise CompareError(f"{argument} is neither a folder nor <benchmark folder>:<run name>")

    folder = read_folder(Path(base))
    cell = find_run(folder, run)
    return Side(
        f"{Path(base).resolve().name} / {cell.run}",
        cell,
        Folder(folder.path, folder.metadata, [cell]),
        cell_row(cell),
    )


def find_run(folder: Folder, run: str) -> Cell:
    """The run of that name, else the only one whose name starts with it."""
    found = [cell for cell in folder.cells if cell.run == run] or [
        cell for cell in folder.cells if cell.run.startswith(run)
    ]

    if len(found) == 1:
        return found[0]

    what = "several runs" if found else "no run"
    raise CompareError(
        f"{run} names {what} of {folder.path}"
        + "".join(f"\n  {cell.run}" for cell in found or folder.cells)
    )


# MARK: Fields


def difference(first: Any, second: Any) -> dict[str, Any]:
    """A - B and its percent of B, when both are numbers."""
    if len(numbers([first, second])) < 2:
        return {}

    found = first - second
    return {"difference": found, "percent": found / second * 100 if second else None}


def matched_row(
    field: str, first: Sequence[Mapping[str, Any]], second: Sequence[Mapping[str, Any]]
) -> dict[str, Any]:
    """A field that should match, each side's value if its rounds agree."""
    same_a, value_a = agreed([result.get(field) for result in first])
    same_b, value_b = agreed([result.get(field) for result in second])
    row: dict[str, Any] = {"field": field, "kind": MATCHED, "a": value_a, "b": value_b}

    if not first or not second:
        return row

    if not same_a or not same_b:
        if not same_a:
            row["a"] = disagreement(first, field)

        if not same_b:
            row["b"] = disagreement(second, field)

        if not same_a and not same_b:
            row["judgement"] = "both sides' rounds differ"
        else:
            row["judgement"] = "A's rounds differ" if not same_a else "B's rounds differ"

        return row

    if agreed([value_a, value_b])[0]:
        row["judgement"] = "equal"
        return row

    row["judgement"] = "differs"
    row.update(difference(value_a, value_b))
    return row


def measured_row(field: str, values_a: Sequence[Any], values_b: Sequence[Any]) -> dict[str, Any]:
    """A measured field: each side's median, min and max, A - B, and whether it is within
    the rounds' spread."""
    first, second = measure(values_a), measure(values_b)
    row: dict[str, Any] = {
        "field": field,
        "kind": MEASURED,
        "a": first.median,
        "b": second.median,
        "a_min": first.min,
        "a_max": first.max,
        "b_min": second.min,
        "b_max": second.max,
    }

    if first.median is None or second.median is None:
        return row

    row.update(difference(first.median, second.median))
    found_a, found_b = numbers(values_a), numbers(values_b)

    if len(found_a) < 2 or len(found_b) < 2:
        row["judgement"] = NO_SPREAD
    elif min(found_a) <= max(found_b) and min(found_b) <= max(found_a):
        # The ranges min..max overlap.
        row["judgement"] = WITHIN_SPREAD
    else:
        row["judgement"] = BEYOND_SPREAD

    return row


def field_rows(a: Side, b: Side) -> list[dict[str, Any]]:
    """Every result field of either side, A's order first, then the time per iteration,
    the median of each round's solve time over its iterations."""
    first, second = a.cell.results, b.cell.results
    rows: list[dict[str, Any]] = []

    for field in first_appearance([*first, *second]):
        values_a = [result.get(field) for result in first]
        values_b = [result.get(field) for result in second]

        if is_measure(field):
            row = measured_row(field, values_a, values_b)
        else:
            row = matched_row(field, first, second)

        in_a = any(field in result for result in first)
        in_b = any(field in result for result in second)

        if first and second and not (in_a and in_b):
            row["judgement"] = "only in A" if in_a else "only in B"

        rows.append(row)

    per_iteration_a = a.row[PER_ITERATION]
    per_iteration_b = b.row[PER_ITERATION]
    row = {"field": PER_ITERATION, "kind": MEASURED, "a": per_iteration_a, "b": per_iteration_b}
    row.update(difference(per_iteration_a, per_iteration_b))
    rows.append(row)
    return [{name: row.get(name) for name in COLUMNS} for row in rows]


# MARK: Options and records


def record_lines(a: Side, b: Side) -> tuple[list[str], list[str]]:
    """The records' differences, the solvers' versions among them when both sides
    recorded them; and the warnings: a side without a record, or with several."""
    differences: list[str] = []
    warnings: list[str] = []
    found_a, found_b = records(a.folder), records(b.folder)

    for label, found in (("A", found_a), ("B", found_b)):
        if not found:
            warnings.append(f"{label} has no record")

        if len(found) > 1:
            warnings.append(
                f"{label}'s rounds have {len(found)} records: its most common is compared"
            )

    if found_a and found_b:
        differences += record_differences(found_a[0][0], found_b[0][0], text)

    versions_a, versions_b = solver_versions(a.folder), solver_versions(b.folder)

    for name in SOLVER_VERSIONS:
        if name in versions_a and name in versions_b:
            old = versions_a[name].most_common(1)[0][0]
            new = versions_b[name].most_common(1)[0][0]

            if old != new:
                differences.append(f"versions.{name}: {old} -> {new}")

    return differences, warnings


def side_flags(side: Side) -> list[str]:
    """What to know about a side's timings: the machine, the solvers, the run's own
    flags."""
    flags = machine_flags(side.folder) + solver_flags(side.folder)
    return flags + ([side.row["flags"]] if side.row["flags"] else [])


def flag_lines(a: Side, b: Side) -> list[str]:
    """Both sides' flags; one both share is said once."""
    found: dict[str, list[str]] = {}

    for label, side in (("A", a), ("B", b)):
        for flag in side_flags(side):
            found.setdefault(flag, []).append(label)

    return [flag if len(labels) == 2 else f"{labels[0]}: {flag}" for flag, labels in found.items()]


# MARK: The time rule


def side_figures(side: Side, label: str) -> tuple[tuple[float, float] | None, str]:
    """A side's objective and median solve time, else None and why the rule cannot use
    them.

    A round that did not finish (a timeout, a crash, a missing round) leaves the side's
    time unknown: the finished rounds alone would make it look faster.
    """
    results = side.cell.results
    found = counts(side.cell)

    if not results:
        return None, f"{label} has no finished round"

    if found["timeouts"]:
        return None, f"{label} has {found['timeouts']} timeouts, so its time is not known"

    if found["not_finished"]:
        return (
            None,
            f"{label} has {found['not_finished']} rounds not finished, so its time is not known",
        )

    same, status = agreed([result.get("status") for result in results])

    if not same:
        return None, f"{label}'s rounds differ in status"

    if status != OPTIMAL:
        return None, f"{label} is {text(status)}, not {OPTIMAL}"

    same, objective = agreed([result.get("objective") for result in results])

    if not same:
        return None, f"{label}'s rounds differ in objective"

    time = measure(result.get(MAIN_TIME) for result in results).median

    if not numbers([objective]) or time is None:
        return None, f"{label} has no objective or no {MAIN_TIME}"

    return (float(objective), time), ""


def time_verdict(a: Side, b: Side, rule: TimeRule) -> tuple[str, str]:
    """The time rule's verdict on A against B, and its reason."""
    first, why = side_figures(a, "A")

    if first is None:
        return NO_VERDICT, why

    second, why = side_figures(b, "B")

    if second is None:
        return NO_VERDICT, why

    (objective_a, time_a), (objective_b, time_b) = first, second

    if not agreed([objective_a, objective_b])[0]:
        return NO_VERDICT, f"the objectives differ: {text(objective_a)} and {text(objective_b)}"

    excess = time_a - time_b
    allowed = rule.allowed(time_b)
    bound = (
        f"max({text(rule.margin_percent)} % of {time_b:.3f}, {text(rule.floor_seconds)})"
        f" = {allowed:.3f} s"
    )
    figures = f"both {OPTIMAL}, same objective; {excess:+.3f} s"

    if excess <= allowed:
        return EQUIVALENT, f"{figures} <= {bound}"

    return SLOWER, f"{figures} > {bound}"


# MARK: Comparing


def compare(a_argument: str, b_argument: str, rule: TimeRule | None = None) -> Comparison:
    """The comparison of the runs the two arguments name (:func:`read_side`).

    Raises:
        CompareError: When an argument names no single run.
        FolderError: When its folder holds no run.
    """
    a, b = read_side(a_argument), read_side(b_argument)
    options = record_differences(a.cell.options, b.cell.options, text)
    differences, warnings = record_lines(a, b)
    return Comparison(
        a,
        b,
        options,
        differences,
        warnings + flag_lines(a, b),
        field_rows(a, b),
        None if rule is None else time_verdict(a, b, rule),
    )


# MARK: Text


def header(side: Side, label: str, width: int) -> str:
    """A side's name and how its rounds ended."""
    found = counts(side.cell)
    others = Counter(item.outcome for item in side.cell.rounds if not item.in_figures)
    line = (
        f"- {label}: {side.name.ljust(width)}  "
        f"{found['finished']} / {found['rounds']} rounds finished"
    )

    if others:
        line += " (" + ", ".join(f"{count} {outcome}" for outcome, count in others.items()) + ")"

    return line


def value_text(row: Mapping[str, Any], side: str) -> str:
    """A side's value: a measure in seconds, MiB, or milliseconds per iteration; the
    main time with its range."""
    value = row[side]

    if row["kind"] == MATCHED or not numbers([value]):
        return text(value)

    if row["field"].endswith("_bytes"):
        return f"{value / MEBIBYTE:.1f} MiB"

    if row["field"] == PER_ITERATION:
        return f"{value * 1000:.2f} ms"

    found = f"{value:.3f}"
    low, high = row[f"{side}_min"], row[f"{side}_max"]

    if row["field"] == MAIN_TIME and low is not None and high is not None and low < high:
        found += f" [{low:.3f}..{high:.3f}]"

    return found


def difference_text(row: Mapping[str, Any]) -> str:
    """A - B: equal or differs for a field that should match, the difference and its
    percent for a measure; the spread for the main time only."""
    found = row["difference"]
    judgement = row["judgement"]
    spread = judgement in (WITHIN_SPREAD, BEYOND_SPREAD, NO_SPREAD)

    if found is None:
        return "-" if judgement is None or spread else judgement

    if row["field"] == PER_ITERATION:
        number = f"{found * 1000:+.1f} ms"
    elif row["field"].endswith("_bytes"):
        number = f"{found / MEBIBYTE:+.1f} MiB"
    elif row["kind"] == MEASURED and found and abs(found) < 0.001:
        # Under a millisecond, seconds to 3 places would read as no difference.
        number = f"{found * 1000:+.3f} ms"
    elif row["kind"] == MEASURED:
        number = f"{found:+.3f}"
    elif isinstance(found, int):
        number = f"{found:+d}"
    else:
        number = f"{found:+.6g}"

    if row["percent"] is not None:
        number += f" ({row['percent']:+.1f} %)"

    if row["kind"] == MATCHED:
        return f"{judgement}: {number}"

    if judgement is not None and (not spread or row["field"] == MAIN_TIME):
        return f"{number}, {judgement}"

    return number


def comparison_text(comparison: Comparison) -> str:
    """The comparison in markdown."""
    width = max(len(comparison.a.name), len(comparison.b.name))
    lines = [header(comparison.a, "A", width), header(comparison.b, "B", width)]
    lines.append(f"- Options that differ: {'; '.join(comparison.options) or 'none'}")
    lines.append(f"- Records that differ: {'; '.join(comparison.records) or 'none'}")
    lines += [f"- Warning: {warning}" for warning in comparison.warnings]
    table = [
        [
            "per iteration" if row["field"] == PER_ITERATION else row["field"],
            value_text(row, "a"),
            value_text(row, "b"),
            difference_text(row),
        ]
        for row in comparison.rows
    ]
    lines += ["", *aligned_table(("field", "A", "B", "A - B"), table)]

    if comparison.verdict is not None:
        verdict, reason = comparison.verdict
        lines += ["", f"Verdict: {verdict} ({reason})"]

    return "\n".join(lines) + "\n"


def comparison_csv(comparison: Comparison) -> str:
    """The fields of the comparison, one row each, in the units of the results."""
    return csv_text(comparison.rows)
