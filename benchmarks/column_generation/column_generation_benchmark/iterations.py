#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Each iteration of a run, as ``iterations.jsonl`` records it: one JSON object per
line, written and flushed as the run goes, so that a run killed half-way keeps the
iterations it did.

A line always has ``iteration``, ``objective``, ``lower_bound`` (null when unknown),
``new_columns``, ``total_columns`` and ``elapsed_seconds``; then the fields of each group the
run's ``statistics.iterations`` lists (:data:`GROUPS`). The package and the C++ example's
bindings give the same iteration objects, so both Python runners write their lines here;
``cpp_native/cpp_native_solver.cpp`` writes the same keys in C++.
"""

from __future__ import annotations

import json
import math
from collections.abc import Sequence
from pathlib import Path
from typing import Any, TextIO

from .options.blocks import Statistics
from .run_folder import ITERATIONS_FILE

#: The groups a run may record, in the order a line holds them.
GROUPS = Statistics.ITERATION_GROUPS


def statistics_switches(groups: Sequence[str]) -> dict[str, bool]:
    """The layer's ``StatisticsParams`` switches that `groups` need (``times`` needs
    none:

    the runners keep the timers on).
    """
    return {name: name in groups for name in GROUPS if name != "times"}


def finite(value: float) -> float | None:
    """A number as a line holds it: null when it is not finite."""
    return float(value) if math.isfinite(value) else None


def stage_record(stage: Any, search: bool) -> dict[str, Any]:
    """What one stage of a pricing call did; with the engine's counts when `search`."""
    record: dict[str, Any] = {
        "stage": stage.stage,
        "status": stage.status.name,
        "new_columns": stage.new_columns,
        "seconds": stage.seconds,
    }

    if search:
        bidirectional = stage.bidirectional
        record["extended_labels"] = stage.extended_labels
        record["bidirectional"] = (
            None
            if bidirectional is None
            else {
                "half_way_used": bidirectional.half_way_used,
                "half_way_off_reason": bidirectional.half_way_off_reason,
                "number_of_joined_paths": bidirectional.number_of_joined_paths,
            }
        )

    return record


def iteration_record(info: Any, groups: Sequence[str]) -> dict[str, Any]:
    """One line of ``iterations.jsonl``: the layer's ``IterationInfo`` with the groups
    the run records."""
    record: dict[str, Any] = {
        "iteration": info.iteration,
        "objective": info.objective,
        "lower_bound": finite(info.lower_bound),
        "new_columns": info.new_columns,
        "total_columns": info.total_columns,
        "elapsed_seconds": info.elapsed_seconds,
    }

    if "times" in groups:
        record["relaxation_seconds"] = info.relaxation_seconds
        record["pricing_seconds"] = info.pricing_seconds

    if "pricers" in groups:
        search = "search" in groups
        record["pricers"] = [
            {
                "status": pricer.status.name,
                "new_columns": pricer.new_columns,
                "min_reduced_cost": pricer.min_reduced_cost,
                "seconds": pricer.seconds,
                "stages": [stage_record(stage, search) for stage in pricer.search_details],
            }
            for pricer in info.pricers
        ]

    if "duals" in groups:
        record["duals"] = [float(dual) for dual in info.duals]

    if "master_values" in groups:
        ids, values = info.master_values
        record["master_values"] = [
            [int(column), float(value)] for column, value in zip(ids, values, strict=True)
        ]

    if "columns" in groups:
        record["added_columns"] = [
            {
                "id": column.id,
                "cost": column.solution.column.cost,
                "path": list(column.solution.path_node_ids),
                "rows": [[row.index, row.coefficient] for row in column.solution.column.rows],
            }
            for column in info.added_columns
        ]

    return record


class IterationFile:
    """``iterations.jsonl`` of a run: its hook is passed to the loop when the loop is
    built, before the run's folder exists; it writes once :meth:`open` has opened the
    file."""

    def __init__(self, groups: Sequence[str]) -> None:
        self.groups = tuple(groups)
        self.file: TextIO | None = None

    def open(self, folder: Path) -> None:
        """Opens the file in the run's `folder`."""
        self.file = (folder / ITERATIONS_FILE).open("w", encoding="utf-8", newline="\n")

    def write(self, info: Any) -> None:
        """Writes the line of an iteration, and flushes it."""
        if self.file is None:
            return

        self.file.write(json.dumps(iteration_record(info, self.groups)) + "\n")
        self.file.flush()

    def close(self) -> None:
        if self.file is not None:
            self.file.close()
            self.file = None
