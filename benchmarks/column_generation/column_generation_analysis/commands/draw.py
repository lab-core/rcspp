#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""An instance drawn, alone or with a run's solution:

    python analysis.py draw --instance C101_25
    python analysis.py draw results/my_benchmark:C101_25 --solution lp

``--instance NAME`` draws a Solomon instance of the tree (``instances/<NAME>.txt``): the depot,
the customers by demand, their numbers up to 30 customers (``--labels on`` or ``off`` to
choose). Runs instead draw each run's instance with its solution, from its first round that
wrote ``solution.json`` (the statistic ``solution``, on by default):

- ``integer`` (by default when the run has one): the integer solution's routes, a colour each,
  the customers no route visits marked with a red cross (the integer master left them to
  artificial columns), and a second figure of the routes' time windows, a row per route;
- ``lp``: the last LP's flow on each arc, the values of the columns that take it summed, by
  width and shade;
- ``none``: the instance alone.

Each run gives ``draw-<run>.<format>``, with ``draw-<run>-windows.<format>`` and its data
``draw-<run>-routes.csv`` (each visit's window, arrival and service) for routes, or
``draw-<run>-arcs.csv`` (each arc's flow) for an LP. A run without ``solution.json``, or
without the solution asked for, is left out with a warning; none at all is an error.
"""

from __future__ import annotations

import argparse
import sys
from collections.abc import Sequence
from pathlib import Path
from typing import TYPE_CHECKING, Any

from column_generation_benchmark import run_folder
from column_generation_benchmark.folder import Cell, FolderError, csv_text
from column_generation_benchmark.instances import (
    InstanceError,
    SolomonInstance,
    make_instance,
)

from ..core import analysis_folder, command_line
from ..core.command_line import CommandError
from ..core.selection import SelectionError, select
from ..vrp import drawing

if TYPE_CHECKING:
    from column_generation_benchmark.folder import Folder

NAME = "draw"
HELP = "An instance drawn, alone or with a run's solution and its time windows."

SOLUTIONS = ("auto", "integer", "lp", "none")


def add_arguments(parser: argparse.ArgumentParser) -> None:
    command_line.add_run_arguments(parser, required=False)
    parser.add_argument(
        "--instance",
        metavar="NAME",
        help="draw this Solomon instance of the tree alone (instances/NAME.txt), without runs",
    )
    parser.add_argument(
        "--solution",
        choices=SOLUTIONS,
        default="auto",
        help="the runs' solution drawn: integer (routes and their time windows), lp (the flow "
        "on each arc) or none; auto, the default, the integer one when the run has it, else "
        "the LP",
    )
    parser.add_argument(
        "--labels",
        choices=("auto", "on", "off"),
        default="auto",
        help=f"the customers' numbers: auto, the default, up to {drawing.LABELS_UP_TO} customers",
    )
    command_line.add_output_arguments(parser)


def run(parsed: argparse.Namespace, arguments: Sequence[str]) -> int:
    """Draws the instance or the runs `parsed` names into an analysis folder; returns
    the exit code."""
    if bool(parsed.runs) == (parsed.instance is not None):
        print(f"analysis.py {NAME}: give runs or --instance, one of the two", file=sys.stderr)
        return 2

    labels = {"auto": None, "on": True, "off": False}[parsed.labels]
    folders: list[Folder] = []
    warnings: list[str] = []

    drawings: list[tuple[str, SolomonInstance, str, dict[str, Any]]] = []

    try:
        if parsed.instance is not None:
            block = {"type": "solomon", "name": parsed.instance}
            drawings = [(f"{NAME}-{parsed.instance}", solomon(block), "none", {})]
        else:
            folders = select(parsed.runs, parsed.where)
            drawings = read_drawings(folders, parsed.solution, warnings)

        folder = command_line.output_folder(parsed, NAME)
    except (
        SelectionError,
        FolderError,
        CommandError,
        InstanceError,
        analysis_folder.AnalysisFolderError,
    ) as error:
        print(f"analysis.py {NAME}: {error}", file=sys.stderr)
        return 2

    outputs: list[str] = []

    for name, instance, kind, part in drawings:
        outputs += draw(folder, name, instance, kind, part, labels, command_line.formats(parsed))

        if kind == "integer":
            left = drawing.unvisited(instance, routes_of(part))

            if left:
                numbers = ", ".join(map(str, left))
                warnings.append(f"{name}: no route visits customers {numbers} (artificial columns)")

    command_line.warn(NAME, warnings)
    analysis_folder.write_record(
        folder, NAME, arguments, command_line.options_of(parsed), folders, outputs, warnings
    )
    print(f"{len(outputs)} files in {folder}")
    return 0


# MARK: Reading


def solomon(block: dict[str, Any]) -> SolomonInstance:
    """The Solomon instance an instance block names.

    Raises:
        CommandError: When the block is of another instance type.
        InstanceError: When its file cannot be read.
    """
    if block.get("type") != "solomon":
        raise CommandError(f"instance {block.get('name')}: only Solomon instances are drawn")

    instance = make_instance(block)
    assert isinstance(instance, SolomonInstance)
    return instance


def read_drawings(
    folders: Sequence[Folder], asked: str, warnings: list[str]
) -> list[tuple[str, SolomonInstance, str, dict[str, Any]]]:
    """Each run's drawing: its file name, its instance, the solution drawn (``integer``,
    ``lp``, ``none``) and that part of ``solution.json``.

    Raises:
        CommandError: When no run wrote ``solution.json`` and a solution is asked for.
    """
    found: list[tuple[str, SolomonInstance, str, dict[str, Any]]] = []
    names: set[str] = set()
    any_solution = False

    for folder in folders:
        for cell in folder.cells:
            solution = first_solution(cell)
            any_solution = any_solution or solution is not None
            kind = chosen_kind(asked, solution)

            if kind is None:
                what = (
                    "solution.json"
                    if solution is None
                    else "integer nor LP solution" if asked == "auto" else f"{asked} solution"
                )
                warnings.append(f"{cell.run} has no {what}: left out")
                continue

            name = f"{NAME}-{cell.run}"

            if name in names:
                name = f"{name}-{len(names) + 1}"

            names.add(name)
            part: dict[str, Any] = {} if kind == "none" or solution is None else solution[kind]
            found.append((name, solomon(cell.options["instance"]), kind, part))

    if not any_solution and asked != "none":
        raise CommandError(
            "no run wrote solution.json: run them with the statistic solution on (the default)"
        )

    return found


def first_solution(cell: Cell) -> dict[str, Any] | None:
    """The ``solution.json`` of the run's first round that wrote one."""
    for item in cell.rounds:
        path = item.folder / run_folder.SOLUTION_FILE

        if path.is_file():
            return run_folder.read_json(path)

    return None


def columns_of(part: dict[str, Any] | None) -> list[dict[str, Any]]:
    """A solution's columns (``lp`` or ``integer``), none when it is null."""
    found: list[dict[str, Any]] = (part or {}).get("columns") or []
    return found


def chosen_kind(asked: str, solution: dict[str, Any] | None) -> str | None:
    """The solution drawn: the one asked for when the run has it, ``auto`` the integer
    one, else the LP; None when the run has not what was asked for."""
    if asked == "none":
        return "none"

    has = {
        kind: solution is not None and bool(columns_of(solution.get(kind)))
        for kind in ("integer", "lp")
    }

    if asked == "auto":
        return next((kind for kind in ("integer", "lp") if has[kind]), None)

    return asked if has[asked] else None


def routes_of(part: dict[str, Any]) -> list[list[int]]:
    """The integer solution's routes as node paths, a column once per unit of its
    value."""
    return [
        list(column["path"]) for column in columns_of(part) for _ in range(round(column["value"]))
    ]


# MARK: Drawing


def draw(
    folder: Path,
    name: str,
    instance: SolomonInstance,
    kind: str,
    part: dict[str, Any],
    labels: bool | None,
    figure_formats: Sequence[str],
) -> list[str]:
    """A drawing's figures and data; returns the files' names."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    written: list[str] = []
    figure, grid = plt.subplots(1, 1, figsize=(7.0, 7.0), squeeze=False)
    axes = grid[0][0]
    drawing.draw_instance(axes, instance, labels)
    routes: list[list[int]] = []

    if kind == "integer":
        routes = routes_of(part)
        drawing.draw_paths(
            axes, instance, routes, [f"route {row + 1}" for row in range(len(routes))]
        )
        left = drawing.unvisited(instance, routes)
        drawing.draw_node_marks(axes, instance, left, "no route" if left else None)
    elif kind == "lp":
        values = drawing.arc_values(columns_of(part))
        drawing.draw_arc_values(axes, instance, values)
        rows = [
            {"from": start, "to": end, "value": value} for (start, end), value in values.items()
        ]
        run_folder.write_atomically(folder / f"{name}-arcs.csv", csv_text(rows))
        written.append(f"{name}-arcs.csv")

    written += save(figure, folder, name, figure_formats)
    plt.close(figure)

    if routes:
        customers = [route[1:-1] for route in routes]
        figure, grid = plt.subplots(1, 1, figsize=(9.0, 1.5 + 0.45 * len(customers)), squeeze=False)
        drawing.draw_windows(grid[0][0], instance, customers)
        written += save(figure, folder, f"{name}-windows", figure_formats)
        plt.close(figure)
        rows = [
            {"route": row + 1, "position": position + 1, **visit}
            for row, route in enumerate(customers)
            for position, visit in enumerate(drawing.visits(instance, route))
        ]
        run_folder.write_atomically(folder / f"{name}-routes.csv", csv_text(rows))
        written.append(f"{name}-routes.csv")

    return written


def save(figure: Any, folder: Path, name: str, figure_formats: Sequence[str]) -> list[str]:
    figure.tight_layout()
    written: list[str] = []

    for figure_format in figure_formats:
        file_name = f"{name}.{figure_format}"
        figure.savefig(folder / file_name, dpi=150)
        written.append(file_name)

    return written
