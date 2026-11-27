#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""How runs converge: their objective and best bound, iteration after iteration:

    python analysis.py convergence results/my_benchmark --where graph.ng_size=8 --gap

A figure per instance, the runs of that instance on it: each run's objective as a solid line,
its best bound so far as a dashed one of the same colour (an iteration records its own bound;
the best is the highest so far), against the loop's time (``--x time``, the default) or the
iterations. ``--gap`` adds a panel below with the relative gap, (objective - best bound) /
|objective|, on a log scale.

A run is drawn from one round: the round whose loop took the median time (the lower median
for an even count; ``--rounds all`` draws every round). A round that did not finish (killed at
its timeout, crashed) is drawn to its last iteration, ended with a cross. The legend names a
run by the parts of its name that the other runs of its figure do not all have (``exact``,
``heuristics-first-single-customer``): a run's name is made of its presets' names.

The y axis shows, by default, the end of the convergence: from half the lowest last objective
to twice the highest (for objectives near 0, the same distances in their largest size). An
artificial start's first objectives are orders of magnitude above the end, and its first bounds
as far below: the gap panel shows the whole run, on a log scale. ``--y-range full`` shows
everything, ``--y-range LOW,HIGH`` a range of one's own.

It needs the runs' iterations: ``statistics.iterations`` (any groups; an empty list writes no
file). The runs without them are left out, with a warning; none at all is an error. Each
figure comes with its data, ``convergence-<instance>.csv``.
"""

from __future__ import annotations

import argparse
import math
import sys
from collections.abc import Sequence
from pathlib import Path
from typing import TYPE_CHECKING, Any

from column_generation_benchmark import run_folder
from column_generation_benchmark.folder import FolderError

from ..core import analysis_folder, command_line
from ..core.command_line import CommandError
from ..core.selection import SelectionError, select

if TYPE_CHECKING:
    import pandas as pd
    from column_generation_benchmark.folder import Folder

NAME = "convergence"
HELP = "The objective and the best bound of runs, iteration after iteration."

#: The fields of a curve's data, as its CSV has them.
DATA_COLUMNS = (
    "benchmark",
    "run",
    "round",
    "iteration",
    "elapsed_seconds",
    "objective",
    "lower_bound",
    "best_bound",
    "gap",
)

#: The outcomes of a round that finished.
FINISHED = (run_folder.FINISHED, run_folder.CHECK_FAILED)


def y_range(value: str) -> str | tuple[float, float]:
    """``auto``, ``full``, or ``LOW,HIGH``."""
    if value in ("auto", "full"):
        return value

    low, separator, high = value.partition(",")

    try:
        found = (float(low), float(high))
    except ValueError:
        found = None

    if not separator or found is None or not all(map(math.isfinite, found)) or found[0] >= found[1]:
        raise argparse.ArgumentTypeError(f"not auto, full, or LOW,HIGH with LOW < HIGH: {value}")

    return found


def add_arguments(parser: argparse.ArgumentParser) -> None:
    command_line.add_run_arguments(parser)
    parser.add_argument(
        "--x",
        choices=("time", "iteration"),
        default="time",
        help="the x axis: the loop's time in seconds, or the iteration (default time)",
    )
    parser.add_argument(
        "--gap",
        action="store_true",
        help="add a panel with the relative gap, (objective - best bound) / |objective|",
    )
    parser.add_argument(
        "--y-range",
        type=y_range,
        default="auto",
        metavar="RANGE",
        help="auto (from half the lowest last objective to twice the highest), full, or "
        "LOW,HIGH (default auto)",
    )
    parser.add_argument(
        "--rounds",
        choices=("median", "all"),
        default="median",
        help="draw each run's median round by the loop's time, or every round (default median)",
    )
    command_line.add_output_arguments(parser)


def run(parsed: argparse.Namespace, arguments: Sequence[str]) -> int:
    """Draws the figures `parsed` asks for into an analysis folder; returns the exit
    code."""
    try:
        folders = select(parsed.runs, parsed.where)
        curves, warnings = read_curves(folders, parsed.rounds == "all")
        folder = command_line.output_folder(parsed, NAME)
    except (
        SelectionError,
        FolderError,
        CommandError,
        analysis_folder.AnalysisFolderError,
    ) as error:
        print(f"analysis.py {NAME}: {error}", file=sys.stderr)
        return 2

    if parsed.gap and all(curve["frame"]["best_bound"].isna().all() for curve in curves):
        warnings.append("no run has a bound: the gap panel is empty (see instance.vehicles)")

    outputs: list[str] = []

    for instance, found in by_instance(curves).items():
        name = f"{NAME}-{instance}"
        outputs += draw(folder, name, found, parsed, command_line.formats(parsed))
        write_data(folder / f"{name}.csv", found)
        outputs.append(f"{name}.csv")

    command_line.warn(NAME, warnings)
    analysis_folder.write_record(
        folder, NAME, arguments, command_line.options_of(parsed), folders, outputs, warnings
    )
    print(f"{len(outputs)} files in {folder}")
    return 0


# MARK: Curves


def read_curves(
    folders: Sequence[Folder], every_round: bool
) -> tuple[list[dict[str, Any]], list[str]]:
    """A curve per run, or per round with `every_round`: its key, instance, whether it
    finished, and its iterations (``frame``, with ``best_bound`` and ``gap``); and the
    warnings.

    Raises:
        CommandError: When no run recorded its iterations.
    """
    from ..core import tables

    runs = tables.runs_table(folders)
    iterations, _, _ = tables.iteration_tables(folders)

    if iterations.empty:
        raise CommandError(
            "no run recorded its iterations: run them with statistics.iterations listing groups "
            '(statistics.iterations = ["times"], say; an empty list writes none)'
        )

    warnings: list[str] = []
    recorded = set(zip(iterations["benchmark"], iterations["run"], strict=True))
    without = sorted(
        {
            run
            for benchmark, run in zip(runs["benchmark"], runs["run"], strict=True)
            if (benchmark, run) not in recorded
        }
    )

    if without:
        warnings.append("no iterations recorded, left out: " + ", ".join(without))

    outcomes = {
        (row["benchmark"], row["run"], none_if_missing(row["round"])): row
        for row in runs.to_dict("records")
    }
    curves: list[dict[str, Any]] = []

    for (benchmark_key, run_key), frame in iterations.groupby(["benchmark", "run"], sort=False):
        benchmark, run = str(benchmark_key), str(run_key)
        rounds = [found for _, found in frame.groupby("round", sort=True, dropna=False)]
        chosen = rounds if every_round else [median_round(rounds)]

        for found in chosen:
            number = none_if_missing(found["round"].iloc[0])
            row = outcomes[(benchmark, run, number)]
            outcome = str(row["outcome"])
            instance = row.get("instance.name")

            if outcome not in FINISHED:
                which = run if number is None else f"{run} round {number}"
                warnings.append(f"{which} ended {outcome}: its curve stops at its last iteration")

            curves.append(
                {
                    "benchmark": benchmark,
                    "run": run,
                    "round": number,
                    "instance": str(instance) if isinstance(instance, str) else run,
                    "finished": outcome in FINISHED,
                    "frame": with_bounds(found),
                }
            )

    return curves, warnings


def none_if_missing(value: Any) -> int | None:
    """A round's number, None for a run folder alone (pandas' missing value)."""
    import pandas as pd

    return None if pd.isna(value) else int(value)


def median_round(rounds: Sequence[pd.DataFrame]) -> pd.DataFrame:
    """The round whose loop took the median time (its last iteration's
    ``elapsed_seconds``); the lower median for an even count."""
    ordered = sorted(rounds, key=lambda frame: float(frame["elapsed_seconds"].iloc[-1]))
    return ordered[(len(ordered) - 1) // 2]


def with_bounds(frame: pd.DataFrame) -> pd.DataFrame:
    """The round's iterations in order, with ``best_bound``, the highest bound so far,
    and ``gap``, (objective - best bound) / |objective|."""
    found = frame.sort_values("iteration").reset_index(drop=True)
    bounds = found["lower_bound"].astype(float)
    found["best_bound"] = bounds.cummax().ffill()
    objectives = found["objective"].astype(float)
    found["gap"] = (objectives - found["best_bound"]) / objectives.abs()
    return found


def by_instance(curves: Sequence[dict[str, Any]]) -> dict[str, list[dict[str, Any]]]:
    found: dict[str, list[dict[str, Any]]] = {}

    for curve in curves:
        found.setdefault(curve["instance"], []).append(curve)

    return found


def labels(curves: Sequence[dict[str, Any]]) -> list[str]:
    """Each curve's legend: the parts of its run's name (between dashes) that not every
    run of its figure has, in the name's order, else the whole name; the benchmark
    folder too when that leaves two alike."""
    names = [str(curve["run"]).split("-") for curve in curves]
    shared = set(names[0]).intersection(*names[1:])
    found = [
        "-".join(part for part in parts if part not in shared) or curve["run"]
        for curve, parts in zip(curves, names, strict=True)
    ]

    if len(set(zip(found, (curve["round"] for curve in curves), strict=True))) < len(curves):
        found = [
            f"{curve['benchmark']}: {label}" for curve, label in zip(curves, found, strict=True)
        ]

    return found


# MARK: Figures


def draw(
    folder: Path,
    name: str,
    curves: Sequence[dict[str, Any]],
    parsed: argparse.Namespace,
    figure_formats: Sequence[str],
) -> list[str]:
    """One instance's figure, in each format; returns the files' names."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    x_field = "elapsed_seconds" if parsed.x == "time" else "iteration"
    panels = 2 if parsed.gap else 1
    figure, axes = plt.subplots(
        panels, 1, sharex=True, figsize=(7.0, 4.5 + 2.5 * (panels - 1)), squeeze=False
    )
    top = axes[0][0]
    colors: dict[tuple[str, str], str] = {}
    labelled: set[tuple[str, str]] = set()
    any_gap = False

    for curve, label in zip(curves, labels(curves), strict=True):
        key = (curve["benchmark"], curve["run"])
        colour = colors.setdefault(key, f"C{len(colors) % 10}")
        frame = curve["frame"]
        x = frame[x_field]
        width = 1.5 if parsed.rounds == "median" else 0.9
        shown = label if key not in labelled else None
        labelled.add(key)

        top.plot(x, frame["objective"], color=colour, linewidth=width, label=shown)
        top.plot(x, frame["best_bound"], color=colour, linewidth=width, linestyle="--")

        if not curve["finished"]:
            top.plot(x.iloc[-1], frame["objective"].iloc[-1], marker="x", color=colour)

        if parsed.gap:
            # A log scale shows only the positive gaps: 0 once the bound meets the objective.
            gap = frame["gap"].where(frame["gap"] > 0)
            axes[1][0].plot(x, gap, color=colour, linewidth=width)
            any_gap = any_gap or bool(gap.notna().any())

    top.set_title(curves[0]["instance"])
    top.set_ylabel("objective (solid), best bound (dashed)")
    top.grid(alpha=0.3)
    top.legend(fontsize="small")
    limits = y_limits(curves, parsed.y_range)

    if limits is not None:
        top.set_ylim(*limits)

    if any_gap:
        axes[1][0].set_yscale("log")

    if parsed.gap:
        axes[1][0].set_ylabel("relative gap")
        axes[1][0].grid(alpha=0.3)

    axes[-1][0].set_xlabel("seconds" if parsed.x == "time" else "iteration")
    figure.tight_layout()
    written: list[str] = []

    for figure_format in figure_formats:
        file_name = f"{name}.{figure_format}"
        # Matplotlib's stubs leave savefig's keyword arguments untyped.
        figure.savefig(folder / file_name, dpi=150)  # pyright: ignore[reportUnknownMemberType]
        written.append(file_name)

    plt.close(figure)
    return written


def y_limits(
    curves: Sequence[dict[str, Any]], asked: str | tuple[float, float]
) -> tuple[float, float] | None:
    """The y axis's range: None for ``full``, the range asked for, or for ``auto`` the
    end of the convergence: from half the lowest last objective to twice the highest, the
    distances in the objectives' largest size (1 when they are all 0)."""
    if asked == "full":
        return None

    if not isinstance(asked, str):
        return asked

    last = [
        float(curve["frame"]["objective"].dropna().iloc[-1])
        for curve in curves
        if curve["frame"]["objective"].notna().any()
    ]

    if not last:
        return None

    low, high = min(last), max(last)
    size = max(abs(low), abs(high)) or 1.0
    return low - 0.5 * size, high + size


def write_data(path: Path, curves: Sequence[dict[str, Any]]) -> None:
    """The figure's data: each curve's iterations, its key first."""
    import pandas as pd

    frames = [
        curve["frame"]
        .reindex(columns=list(DATA_COLUMNS))
        .assign(benchmark=curve["benchmark"], run=curve["run"], round=curve["round"])
        for curve in curves
    ]
    pd.concat(frames, ignore_index=True).to_csv(path, index=False)
