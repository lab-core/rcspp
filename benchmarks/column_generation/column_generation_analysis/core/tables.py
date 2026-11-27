#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The run folders as tables, one row per round, or per round and iteration.

Every table starts with the same key: ``benchmark`` (the folder, as it was given), ``run`` and
``round`` (empty for a run folder a runner wrote alone). The tables keep the rounds apart:
combining them is a command's job.

- :func:`runs_table`: a round per row, as ``runs.csv`` (the summary's code builds it).
- :func:`iteration_tables`: from ``iterations.jsonl``, the iterations, their pricers, and the
  pricers' stages.
- :func:`duals_table`, :func:`master_values_table`, :func:`added_columns_table`: the
  iterations' long lists, read only when asked for.
- :func:`solution_columns_table` and :func:`resolved_solutions`: from ``solution.json``.

A field a round did not record is left empty: a missing file or group gives no rows, never an
error. Each command checks that it has the data it needs.
"""

import json
from collections.abc import Iterator, Mapping, Sequence
from typing import Any

import pandas as pd
from column_generation_benchmark import run_folder
from column_generation_benchmark.folder import Folder, Round, first_appearance, flatten
from column_generation_benchmark.summary import round_parts

from .selection import benchmark_name

KEY = ("benchmark", "run", "round")

#: An iteration's fields that are lists, each read into its own table.
LIST_FIELDS = ("pricers", "duals", "master_values", "added_columns")


# MARK: Runs


def runs_table(folders: Sequence[Folder]) -> pd.DataFrame:
    """A round per row: the key, the folder and the outcome, the run's options as dotted
    columns, every field of ``result.json``, then the metadata's (``runs.csv``'s
    columns)."""
    parts: list[list[dict[str, Any]]] = []

    for folder in folders:
        for item in folder.rounds:
            found = round_parts(item, folder.path)
            found[0] = {"benchmark": benchmark_name(folder), **found[0]}
            parts.append(found)

    # The columns by group, whatever round first has one, as runs.csv has them.
    columns: list[str] = []

    for index in range(4):
        found = first_appearance(part[index] for part in parts)
        columns += [name for name in found if name not in columns]

    rows = [{name: value for part in found for name, value in part.items()} for found in parts]
    return keyed(pd.DataFrame(rows, columns=columns))


# MARK: Iterations


def iteration_tables(
    folders: Sequence[Folder],
) -> tuple[pd.DataFrame, pd.DataFrame, pd.DataFrame]:
    """The iterations, the pricers and the stages of every round that recorded its
    iterations.

    - iterations: the key, then a line's fields that are not lists (``iteration``,
      ``objective``, ``lower_bound``, ..., and the ``times`` group's);
    - pricers (group ``pricers``): the key, ``iteration``, ``pricer`` (its place in the
      line), then its fields but its stages;
    - stages: the key, ``iteration``, ``pricer``, then the stage's fields, the ``search``
      group's included (``bidirectional.half_way_used``, ...).
    """
    iterations: list[dict[str, Any]] = []
    pricers: list[dict[str, Any]] = []
    stages: list[dict[str, Any]] = []

    for key, line in iteration_lines(folders):
        row = {**key, **{name: value for name, value in line.items() if name not in LIST_FIELDS}}
        iterations.append(row)
        at = {**key, "iteration": line.get("iteration")}

        for position, pricer in enumerate(listed(line, "pricers")):
            fields = {name: value for name, value in pricer.items() if name != "stages"}
            pricers.append({**at, "pricer": position, **fields})

            for stage in listed(pricer, "stages"):
                stages.append({**at, "pricer": position, **flatten(stage)})

    return (
        table(iterations, [*KEY, "iteration"]),
        table(pricers, [*KEY, "iteration", "pricer"]),
        table(stages, [*KEY, "iteration", "pricer", "stage"]),
    )


def duals_table(folders: Sequence[Folder]) -> pd.DataFrame:
    """Each iteration's duals (group ``duals``): the key, ``iteration``, ``row``,
    ``dual``."""
    rows = [
        {**key, "iteration": line.get("iteration"), "row": row, "dual": dual}
        for key, line in iteration_lines(folders)
        for row, dual in enumerate(listed(line, "duals"))
    ]
    return table(rows, [*KEY, "iteration", "row", "dual"])


def master_values_table(folders: Sequence[Folder]) -> pd.DataFrame:
    """The values of the columns each iteration's LP uses (group ``master_values``): the
    key, ``iteration``, ``column``, ``value``."""
    rows = [
        {**key, "iteration": line.get("iteration"), "column": column, "value": value}
        for key, line in iteration_lines(folders)
        for column, value in listed(line, "master_values")
    ]
    return table(rows, [*KEY, "iteration", "column", "value"])


def added_columns_table(folders: Sequence[Folder]) -> pd.DataFrame:
    """The columns each iteration added (group ``columns``): the key, ``iteration``,
    ``column`` (its id), ``cost``, ``path`` (node ids) and ``rows`` (``[index,
    coefficient]``), the last two lists."""
    rows = [
        {
            **key,
            "iteration": line.get("iteration"),
            "column": column.get("id"),
            "cost": column.get("cost"),
            "path": column.get("path"),
            "rows": column.get("rows"),
        }
        for key, line in iteration_lines(folders)
        for column in listed(line, "added_columns")
    ]
    return table(rows, [*KEY, "iteration", "column", "cost", "path", "rows"])


def iteration_lines(folders: Sequence[Folder]) -> Iterator[tuple[dict[str, Any], dict[str, Any]]]:
    """Each round's key with each line of its ``iterations.jsonl``, whatever its
    outcome: a run killed at its timeout keeps the iterations it did."""
    for folder in folders:
        for item in folder.rounds:
            key = round_key(folder, item)

            for line in read_iterations(item):
                yield key, line


def read_iterations(item: Round) -> list[dict[str, Any]]:
    """The lines of a round's ``iterations.jsonl``, none without the file.

    A last line that cannot be read is left out: the run was killed while writing it.

    Raises:
        ValueError: When another line cannot be read.
    """
    path = item.folder / run_folder.ITERATIONS_FILE

    if not path.is_file():
        return []

    texts = path.read_text(encoding="utf-8").splitlines()
    lines: list[dict[str, Any]] = []

    for number, text in enumerate(texts, start=1):
        if not text.strip():
            continue

        try:
            lines.append(json.loads(text))
        except json.JSONDecodeError:
            if number == len(texts):
                break

            raise ValueError(f"{path}, line {number}: not a JSON object") from None

    return lines


# MARK: Solutions


def solution_columns_table(folders: Sequence[Folder]) -> pd.DataFrame:
    """The columns of each round's last LP and integer solution (``solution.json``): the
    key, ``kind`` (``lp``, ``integer``), ``column`` (its id), ``value``, ``cost``,
    ``path``, ``rows`` and ``artificial``; an artificial column has no cost, path or
    rows."""
    rows: list[dict[str, Any]] = []

    for folder in folders:
        for item in folder.rounds:
            solution = read_solution(item)

            for kind in ("lp", "integer"):
                part: dict[str, Any] = solution.get(kind) or {}
                at = {**round_key(folder, item), "kind": kind}

                for column in listed(part, "columns"):
                    rows.append(
                        {
                            **at,
                            "column": column.get("id"),
                            "value": column.get("value"),
                            "cost": column.get("cost"),
                            "path": column.get("path"),
                            "rows": column.get("rows"),
                            "artificial": False,
                        }
                    )

                for column in listed(part, "artificial_columns"):
                    rows.append(
                        {
                            **at,
                            "column": column.get("id"),
                            "value": column.get("value"),
                            "cost": None,
                            "path": None,
                            "rows": None,
                            "artificial": True,
                        }
                    )

    return table(rows, [*KEY, "kind", "column", "value", "cost", "path", "rows", "artificial"])


def resolved_solutions(
    folders: Sequence[Folder],
) -> dict[tuple[str, str, int | None], dict[str, Any]]:
    """Each round's resolved solution (``solution.json``'s ``resolved``: for Solomon,
    the routes, vehicles, distance and errors), by key; rounds without one left out."""
    found: dict[tuple[str, str, int | None], dict[str, Any]] = {}

    for folder in folders:
        for item in folder.rounds:
            resolved = read_solution(item).get("resolved")

            if resolved:
                found[(benchmark_name(folder), item.run, item.number)] = resolved

    return found


def read_solution(item: Round) -> dict[str, Any]:
    """A round's ``solution.json``, empty without one."""
    path = item.folder / run_folder.SOLUTION_FILE
    return run_folder.read_json(path) if path.is_file() else {}


# MARK: Shared


def listed(table: Mapping[str, Any], name: str) -> list[Any]:
    """A list in a JSON object: empty when the object does not have it, or has null."""
    found: list[Any] = table.get(name) or []
    return found


def round_key(folder: Folder, item: Round) -> dict[str, Any]:
    return {"benchmark": benchmark_name(folder), "run": item.run, "round": item.number}


def table(rows: list[dict[str, Any]], first: Sequence[str]) -> pd.DataFrame:
    """`rows` as a table, `first` its first columns, which it has even without rows."""
    columns = [*first, *(name for name in first_appearance(rows) if name not in first)]
    return keyed(pd.DataFrame(rows, columns=columns))


def keyed(frame: pd.DataFrame) -> pd.DataFrame:
    """The round as a whole number that may be empty: a run folder alone has none."""
    frame["round"] = frame["round"].astype("Int64")
    return frame
