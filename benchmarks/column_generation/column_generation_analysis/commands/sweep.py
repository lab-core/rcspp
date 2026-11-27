#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""One option swept: result fields against the option's values, a line per instance:

    python analysis.py sweep results/ng --option graph.ng_size
    python analysis.py sweep results/ng --option graph.relaxation --option graph.ng_size --relative

A point is a run over its finished rounds, at its value of the swept option (``--option``,
dotted; several give one label per combination, ``unset`` for a run without the option). The
runs whose other options are the same make a line: a line per instance, or per instance and
any other option that varies. A line is named by the parts of its runs' names that the other
lines' do not all have.

A field (``--field``, by default ``objective`` and ``solve_seconds``) is any field of
``result.json``, or ``seconds_per_iteration``. A measured field (its name ends in ``_seconds``
or ``_bytes``) is the rounds' median, drawn with their min and max; any other is the value the
rounds agree on, as the summary has them. ``--relative`` divides each line by its value at
its first point.

``--log`` puts the fields' axis on a log scale.

A figure per field, ``sweep-<field>.<format>``, with its data, ``sweep-<field>.csv`` (a point
per row), and a table, ``sweep-<field>.md`` (a line per row, a value per column). A run with
no finished round, or whose rounds disagree on a field that should match, is left out of that
field with a warning.
"""

from __future__ import annotations

import argparse
import math
import sys
from collections.abc import Sequence
from pathlib import Path
from typing import Any

from column_generation_benchmark import run_folder
from column_generation_benchmark.folder import (
    Cell,
    Folder,
    FolderError,
    agreed,
    aligned_table,
    csv_text,
    flatten,
    is_measure,
    measure,
    numbers,
    text,
)

from ..core import analysis_folder, command_line
from ..core.command_line import CommandError
from ..core.selection import SelectionError, benchmark_name, select

NAME = "sweep"
HELP = "One option swept: result fields against its values, a line per instance."

DEFAULT_FIELDS = ("objective", "solve_seconds")
PER_ITERATION = "seconds_per_iteration"

#: A run's value of a swept option it does not have.
UNSET = "unset"

#: The columns of a field's data, a point per row.
DATA_COLUMNS = ("line", "value", "benchmark", "run", "rounds", "median", "min", "max")


def add_arguments(parser: argparse.ArgumentParser) -> None:
    command_line.add_run_arguments(parser)
    parser.add_argument(
        "--option",
        action="append",
        required=True,
        metavar="OPTION",
        help="the dotted option swept (graph.ng_size); several make one label per combination",
    )
    parser.add_argument(
        "--field",
        action="append",
        metavar="FIELD",
        help="a field of result.json, or seconds_per_iteration; may be given more than once "
        f"(default {' and '.join(DEFAULT_FIELDS)})",
    )
    parser.add_argument(
        "--relative",
        action="store_true",
        help="divide each line by its value at its first point",
    )
    parser.add_argument(
        "--log",
        action="store_true",
        help="a log scale for the fields' axis (times that span orders of magnitude)",
    )
    command_line.add_output_arguments(parser)


def run(parsed: argparse.Namespace, arguments: Sequence[str]) -> int:
    """Draws a figure and writes a table per field into an analysis folder; returns the
    exit code."""
    fields: list[str] = list(dict.fromkeys(parsed.field or DEFAULT_FIELDS))

    try:
        folders = select(parsed.runs, parsed.where)
        points, values, warnings = read_points(folders, parsed.option, fields)
        folder = command_line.output_folder(parsed, NAME)
    except (
        SelectionError,
        FolderError,
        CommandError,
        analysis_folder.AnalysisFolderError,
    ) as error:
        print(f"analysis.py {NAME}: {error}", file=sys.stderr)
        return 2

    outputs: list[str] = []

    for field in fields:
        found = [point for point in points if point["field"] == field]

        if parsed.relative:
            found = relative(found, values, warnings)

        name = f"{NAME}-{field}"
        outputs += draw(folder, name, field, found, values, parsed)
        run_folder.write_atomically(folder / f"{name}.csv", data_text(found))
        run_folder.write_atomically(folder / f"{name}.md", table_text(field, found, values))
        outputs += [f"{name}.csv", f"{name}.md"]

    command_line.warn(NAME, warnings)
    analysis_folder.write_record(
        folder, NAME, arguments, command_line.options_of(parsed), folders, outputs, warnings
    )
    print(f"{len(outputs)} files in {folder}")
    return 0


# MARK: Points


def read_points(
    folders: Sequence[Folder], swept: Sequence[str], fields: Sequence[str]
) -> tuple[list[dict[str, Any]], list[str], list[str]]:
    """Each run's point for each field (its ``line``, ``value`` of the swept options,
    ``median``, ``min`` and ``max``); the values in their order on the x axis; the
    warnings.

    Raises:
        CommandError: When no run has a swept option or a field.
    """
    cells = [(benchmark_name(folder), cell) for folder in folders for cell in folder.cells]
    options = [flatten(cell.options) for _, cell in cells]
    warnings: list[str] = []

    for option in swept:
        if not any(option in found for found in options):
            raise CommandError(f"--option {option}: no run has that option")

    for field in fields:
        has_field = any(field in result for _, cell in cells for result in cell.results)

        if field != PER_ITERATION and not has_field:
            raise CommandError(f"--field {field}: no finished run has that field")

    labels = [value_label([found.get(option, UNSET) for option in swept]) for found in options]
    values = x_order(labels, [[found.get(option) for option in swept] for found in options])

    if len(values) < 2:
        warnings.append(f"{', '.join(swept)} takes one value only: {values[0]}")

    # The runs whose other options are the same make a line.
    # The other options, an empty block (graph = {}) left out: it is as no block.
    others = [
        {key: value for key, value in found.items() if key not in swept and value != {}}
        for found in options
    ]
    keys = [repr(sorted((key, repr(value)) for key, value in found.items())) for found in others]
    names = line_names(keys, [cell.run for _, cell in cells], others)
    points: list[dict[str, Any]] = []
    seen: dict[tuple[str, str], str] = {}

    for (benchmark, cell), key, label in zip(cells, keys, labels, strict=True):
        if (key, label) in seen:
            warnings.append(
                f"{cell.run} is at the same point as {seen[(key, label)]}: the first is drawn"
            )
            continue

        seen[(key, label)] = cell.run

        if not cell.results:
            warnings.append(f"{cell.run} has no finished round: left out")
            continue

        for field in fields:
            figures = field_figures(cell, field)

            if figures is None:
                warnings.append(f"{cell.run}: its rounds disagree on {field}, left out of it")
                continue

            points.append(
                {
                    "field": field,
                    "line": names[key],
                    "value": label,
                    "benchmark": benchmark,
                    "run": cell.run,
                    "rounds": len(cell.results),
                    **figures,
                }
            )

    return points, values, warnings


def value_label(values: Sequence[Any]) -> str:
    """A point's value of the swept options: each one's, by spaces; ``unset`` once when
    the run has none of them."""
    if all(value == UNSET for value in values):
        return UNSET

    return " ".join(text(value) for value in values)


def x_order(labels: Sequence[str], values: Sequence[Sequence[Any]]) -> list[str]:
    """The values on the x axis: by number when one option is swept and each value is a
    number (``unset`` first), else in the order the runs come."""
    found = list(dict.fromkeys(labels))
    by_label = dict(zip(labels, values, strict=True))

    if all(
        len(by_label[label]) == 1 and len(numbers(by_label[label])) == 1
        for label in found
        if label != UNSET
    ):
        return sorted(found, key=lambda label: (label != UNSET, number_of(by_label[label])))

    return found


def number_of(values: Sequence[Any]) -> float:
    found = numbers(values)
    return found[0] if found else 0.0


def line_names(
    keys: Sequence[str], runs: Sequence[str], options: Sequence[dict[str, Any]]
) -> dict[str, str]:
    """Each line's name: the parts of its runs' names (between dashes) that all its runs
    have and not every line does, in the name's order; ``all runs`` for a single line.

    Lines left with one name, or none, get the other options that differ between them
    (``graph.relaxation=ng``).
    """
    parts: dict[str, list[str]] = {}
    shared: dict[str, set[str]] = {}
    own_options: dict[str, dict[str, Any]] = {}

    for key, run, found in zip(keys, runs, options, strict=True):
        split = run.split("-")
        parts.setdefault(key, split)
        own_options.setdefault(key, found)
        shared[key] = shared.get(key, set(split)) & set(split)

    if len(shared) == 1:
        return {key: "all runs" for key in shared}

    everywhere = set(parts[keys[0]])

    for found in shared.values():
        everywhere &= found

    names = {
        key: "-".join(part for part in parts[key] if part in shared[key] - everywhere)
        for key in shared
    }

    for name in set(names.values()):
        alike = [key for key in names if names[key] == name]

        if len(alike) < 2 and name:
            continue

        every_option = sorted({option for key in alike for option in own_options[key]})
        differ = [
            option
            for option in every_option
            if len({repr(own_options[key].get(option)) for key in alike}) > 1
        ]

        for key in alike:
            values = ", ".join(
                f"{option}={text(own_options[key].get(option))}" for option in differ
            )
            names[key] = ", ".join(found for found in (name, values) if found) or key

    return names


def field_figures(cell: Cell, field: str) -> dict[str, Any] | None:
    """A field over a run's finished rounds: a measure's median, min and max; else the
    value they agree on (min and max empty); None when they do not agree."""
    results = cell.results

    if field == PER_ITERATION:
        found = measure(
            result["solve_seconds"] / result["iterations"]
            for result in results
            if numbers([result.get("solve_seconds")])
            and isinstance(result.get("iterations"), int)
            and result["iterations"] > 0
        )
        return {"median": found.median, "min": found.min, "max": found.max}

    if is_measure(field):
        found = measure(result.get(field) for result in results)
        return {"median": found.median, "min": found.min, "max": found.max}

    same, value = agreed([result.get(field) for result in results])
    return {"median": value, "min": None, "max": None} if same else None


def relative(
    points: Sequence[dict[str, Any]], values: Sequence[str], warnings: list[str]
) -> list[dict[str, Any]]:
    """Each line's points divided by its point at its first value; a line whose first
    point is not a number, or 0, left out with a warning."""
    found: list[dict[str, Any]] = []

    for line in dict.fromkeys(point["line"] for point in points):
        own = sorted(
            (point for point in points if point["line"] == line),
            key=lambda point: values.index(point["value"]),
        )
        base = own[0]["median"]

        if not numbers([base]) or base == 0:
            warnings.append(f"{line}: its first point is {text(base)}, left out of --relative")
            continue

        for point in own:
            divided = {
                name: None if point[name] is None else point[name] / base
                for name in ("median", "min", "max")
            }
            found.append({**point, **divided})

    return found


# MARK: Output


def draw(
    folder: Path,
    name: str,
    field: str,
    points: Sequence[dict[str, Any]],
    values: Sequence[str],
    parsed: argparse.Namespace,
) -> list[str]:
    """A field's figure, in each format; returns the files' names."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    numeric = numeric_axis(values)
    figure, grid = plt.subplots(1, 1, figsize=(7.0, 4.5), squeeze=False)
    axes = grid[0][0]

    for index, line in enumerate(dict.fromkeys(point["line"] for point in points)):
        own = sorted(
            (point for point in points if point["line"] == line and numbers([point["median"]])),
            key=lambda point: values.index(point["value"]),
        )

        if not own:
            continue

        x = [
            to_number(point["value"]) if numeric else values.index(point["value"]) for point in own
        ]
        y = [point["median"] for point in own]
        below = [point["median"] - (point["min"] or point["median"]) for point in own]
        above = [(point["max"] or point["median"]) - point["median"] for point in own]
        axes.errorbar(
            x, y, yerr=[below, above], color=f"C{index % 10}", marker="o", capsize=3, label=line
        )

    if not numeric:
        axes.set_xticks(range(len(values)), list(values))

    axes.set_xlabel(", ".join(parsed.option))
    axes.set_ylabel(f"{field} / its first value" if parsed.relative else field)
    axes.grid(alpha=0.3)
    axes.legend(fontsize="small")

    if parsed.log:
        axes.set_yscale("log")

    figure.tight_layout()
    written: list[str] = []

    for figure_format in command_line.formats(parsed):
        file_name = f"{name}.{figure_format}"
        # Matplotlib's stubs leave savefig's keyword arguments untyped.
        figure.savefig(folder / file_name, dpi=150)  # pyright: ignore[reportUnknownMemberType]
        written.append(file_name)

    plt.close(figure)
    return written


def numeric_axis(values: Sequence[str]) -> bool:
    """Whether the x axis is a number axis: every value one finite number."""
    return all(math.isfinite(to_number(value)) for value in values)


def to_number(label: str) -> float:
    try:
        return float(label)
    except ValueError:
        return float("nan")


def data_text(points: Sequence[dict[str, Any]]) -> str:
    """A field's data: a point per row."""
    rows = [{name: point[name] for name in DATA_COLUMNS} for point in points]
    return csv_text(rows, first=DATA_COLUMNS)


def table_text(field: str, points: Sequence[dict[str, Any]], values: Sequence[str]) -> str:
    """A field's table: a line per row, a value of the swept options per column; a
    measure's median, ``-`` where a line has no point."""
    lines = list(dict.fromkeys(point["line"] for point in points))
    by_point = {(point["line"], point["value"]): point["median"] for point in points}
    rows = [[line, *(text(by_point.get((line, value))) for value in values)] for line in lines]
    return f"# {field}\n\n" + "\n".join(aligned_table(["line", *values], rows)) + "\n"
