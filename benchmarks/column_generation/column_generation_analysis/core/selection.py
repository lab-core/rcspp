#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The runs a command works on.

Each argument is a benchmark folder, a folder of run folders, a run folder, or
``<folder>:<run name>``: the runs whose name is that one, else every run whose name starts with
it. A ``--where`` condition, ``<dotted option>=<value>``, keeps the runs whose option has that
value; the value is read as TOML (``8``, ``true``, ``"ng"``), else taken as text.

The runs come back as the benchmark package's folders (:class:`Folder`), each holding only the
runs selected, so that the summary's and the comparison's code reads them as it reads its own.
"""

import tomllib
from collections.abc import Mapping, Sequence
from pathlib import Path
from typing import Any

from column_generation_benchmark.folder import Cell, Folder, flatten, read_folder


class SelectionError(Exception):
    """An argument or a condition that selects no run."""


def select(arguments: Sequence[str], where: Sequence[str] = ()) -> list[Folder]:
    """The runs the arguments name that meet every condition, by folder, in the
    arguments' order; a folder named twice is read once.

    Raises:
        SelectionError: When an argument names no run, a condition is not
            ``<name>=<value>`` or names an option no run has, or no run is left.
        FolderError: When a folder holds no run.
    """
    conditions = [read_condition(text) for text in where]
    folders: dict[Path, Folder] = {}

    for argument in arguments:
        found = read_argument(argument)
        key = found.path.resolve()

        if key in folders:
            # The same folder again: add the runs it did not select yet.
            known = folders[key]
            names = {cell.run for cell in known.cells}
            known.cells += [cell for cell in found.cells if cell.run not in names]
        else:
            folders[key] = found

    every_option: set[str] = set()

    for found in folders.values():
        for cell in found.cells:
            every_option.update(flatten(cell.options))

    for name, _ in conditions:
        if name not in every_option:
            raise SelectionError(f"--where {name}: no run has that option")

    selected: list[Folder] = []

    for found in folders.values():
        cells = [cell for cell in found.cells if meets(cell, conditions)]

        if cells:
            selected.append(Folder(found.path, found.metadata, cells))

    if not selected:
        raise SelectionError("no run meets every --where condition")

    return selected


def read_argument(argument: str) -> Folder:
    """The folder an argument names, with the runs it names."""
    path = Path(argument)

    # An existing folder first: a Windows path has a colon after its drive letter.
    if path.is_dir():
        return read_folder(path)

    base, separator, run = argument.rpartition(":")

    if not separator or not run or "/" in run or "\\" in run or not Path(base).is_dir():
        raise SelectionError(f"{argument} is neither a folder nor <folder>:<run name>")

    folder = read_folder(Path(base))
    cells = [cell for cell in folder.cells if cell.run == run] or [
        cell for cell in folder.cells if cell.run.startswith(run)
    ]

    if not cells:
        raise SelectionError(
            f"{run} names no run of {folder.path}"
            + "".join(f"\n  {cell.run}" for cell in folder.cells)
        )

    return Folder(folder.path, folder.metadata, cells)


def read_condition(text: str) -> tuple[str, Any]:
    """``<name>=<value>``: the dotted option's name and its value."""
    name, separator, value = text.partition("=")
    name = name.strip()

    if not separator or not name:
        raise SelectionError(f"--where {text}: not <dotted option>=<value>")

    try:
        return name, tomllib.loads(f"value = {value.strip()}")["value"]
    except tomllib.TOMLDecodeError:
        return name, value.strip()


def meets(cell: Cell, conditions: Sequence[tuple[str, Any]]) -> bool:
    options: Mapping[str, Any] = flatten(cell.options)
    return all(name in options and options[name] == value for name, value in conditions)


def benchmark_name(folder: Folder) -> str:
    """How the tables name a folder: its path as it was given."""
    return folder.path.as_posix()
