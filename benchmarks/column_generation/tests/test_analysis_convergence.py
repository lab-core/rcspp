#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The convergence command, on benchmark folders written by hand: the curves, the round
drawn, the runs left out, the figures and their data."""

import csv
from pathlib import Path
from typing import Any

import analysis
import pytest
from benchmark_folders import (
    RUN,
    iteration_line,
    metadata,
    options,
    result,
    round_folder,
    write_benchmark,
    write_iterations,
    write_round,
)
from column_generation_analysis.commands import convergence
from column_generation_analysis.core.selection import select
from column_generation_benchmark import run_folder

GUROBI = RUN.replace("highspy", "gurobipy")
R201 = RUN.replace("C101_25", "R201_25")


def lines(count: int, seconds: float, bounds: list[float | None] | None = None) -> list[Any]:
    """`count` iterations over `seconds`, the objective falling to 100, with
    `bounds`."""
    found: list[float | None] = [None] * count if bounds is None else bounds
    return [
        iteration_line(
            number,
            objective=100.0 + 1000.0 / number**2,
            lower_bound=found[number - 1],
            elapsed_seconds=seconds * number / count,
        )
        for number in range(1, count + 1)
    ]


def write_runs(path: Path, runs: dict[str, dict[str, Any]], rounds: int = 1) -> None:
    write_benchmark(path, runs, rounds=rounds)

    for name, run_options in runs.items():
        for number in range(1, rounds + 1):
            write_round(round_folder(path, name, number), result(), metadata(), run_options)


def run_command(*arguments: str) -> int:
    return analysis.main(["convergence", *arguments, "--format", "png"])


def read_data(path: Path) -> list[dict[str, str]]:
    with open(path, newline="", encoding="utf-8") as file:
        return list(csv.DictReader(file))


# MARK: Curves


def test_the_best_bound_is_the_highest_so_far(tmp_path: Path):
    write_runs(tmp_path, {RUN: options()})
    bounds: list[float | None] = [None, 50.0, 40.0, 90.0, 80.0]
    write_iterations(round_folder(tmp_path, RUN, 1), lines(5, 1.0, bounds))

    curves, warnings = convergence.read_curves(select([str(tmp_path)]), every_round=False)

    (curve,) = curves
    best = curve["frame"]["best_bound"].tolist()
    assert best[0] != best[0]  # NaN: no bound yet
    assert best[1:] == [50.0, 50.0, 90.0, 90.0]
    assert curve["frame"]["gap"].iloc[-1] == pytest.approx((140.0 - 90.0) / 140.0)
    assert curve["instance"] == "C101_25"
    assert curve["finished"]
    assert warnings == []


def test_a_run_is_drawn_from_its_median_round(tmp_path: Path):
    write_runs(tmp_path, {RUN: options()}, rounds=4)

    for number, seconds in zip((1, 2, 3, 4), (4.0, 1.0, 3.0, 2.0), strict=True):
        write_iterations(round_folder(tmp_path, RUN, number), lines(3, seconds))

    folders = select([str(tmp_path)])
    median, _ = convergence.read_curves(folders, every_round=False)
    every, _ = convergence.read_curves(folders, every_round=True)

    # Times 1, 2, 3, 4: the lower median, 2 s, is round 4.
    assert [curve["round"] for curve in median] == [4]
    assert [curve["round"] for curve in every] == [1, 2, 3, 4]


def test_runs_without_iterations_are_left_out_with_a_warning(tmp_path: Path):
    write_runs(tmp_path, {RUN: options(), GUROBI: options(master="gurobipy")})
    write_iterations(round_folder(tmp_path, RUN, 1), lines(3, 1.0))

    curves, warnings = convergence.read_curves(select([str(tmp_path)]), every_round=False)

    assert [curve["run"] for curve in curves] == [RUN]
    assert warnings == [f"no iterations recorded, left out: {GUROBI}"]


def test_a_round_not_finished_is_drawn_with_a_warning(tmp_path: Path):
    write_benchmark(tmp_path, {RUN: options()}, rounds=1)
    folder = round_folder(tmp_path, RUN, 1)
    write_round(folder, result(status="timeout"), metadata(outcome="timeout"), options())
    write_iterations(folder, lines(3, 1.0))

    curves, warnings = convergence.read_curves(select([str(tmp_path)]), every_round=False)

    assert not curves[0]["finished"]
    assert warnings == [f"{RUN} round 1 ended timeout: its curve stops at its last iteration"]


def test_a_run_folder_alone_has_no_round(tmp_path: Path):
    folder = tmp_path / "alone"
    write_round(folder, result(), metadata(outcome=None), options())
    write_iterations(folder, lines(3, 1.0))

    curves, _ = convergence.read_curves(select([str(folder)]), every_round=False)

    assert [(curve["run"], curve["round"]) for curve in curves] == [("alone", None)]


def test_the_legend_names_the_parts_of_the_names_that_differ():
    def curve(run: str, benchmark: str = "a") -> dict[str, Any]:
        return {"benchmark": benchmark, "run": run, "round": 1}

    exact = curve("C101_25-package-highspy-exact-artificial")
    first = curve("C101_25-package-highspy-heuristics-first-single-customer")
    gurobi = curve("C101_25-package-gurobipy-exact-artificial")

    assert convergence.labels([exact, first]) == [
        "exact-artificial",
        "heuristics-first-single-customer",
    ]
    assert convergence.labels([exact, gurobi]) == ["highspy", "gurobipy"]
    assert convergence.labels([exact]) == [exact["run"]]
    assert convergence.labels([curve(exact["run"], "b"), exact]) == [
        f"b: {exact['run']}",
        f"a: {exact['run']}",
    ]


def test_the_automatic_range_shows_the_end_of_the_convergence():
    import pandas as pd

    def curve(objectives: list[float]) -> dict[str, Any]:
        return {"frame": pd.DataFrame({"objective": objectives, "best_bound": [-1e8] * 3})}

    curves = [curve([1e7, 500.0, 120.0]), curve([1e6, 300.0, 100.0])]

    assert convergence.y_limits(curves, "auto") == (100.0 - 60.0, 120.0 + 120.0)
    assert convergence.y_limits([curve([5.0, 1.0, 0.0])], "auto") == (-0.5, 1.0)
    assert convergence.y_limits(curves, "full") is None
    assert convergence.y_limits(curves, (1.0, 2.0)) == (1.0, 2.0)


# MARK: The command


def test_the_command_writes_a_figure_and_its_data_per_instance(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    benchmark = tmp_path / "benchmark"
    runs = {RUN: options(), GUROBI: options(master="gurobipy"), R201: options(instance="R201_25")}
    write_runs(benchmark, runs, rounds=2)

    for name in runs:
        for number in (1, 2):
            write_iterations(round_folder(benchmark, name, number), lines(4, number * 1.0))

    output = tmp_path / "analysis"

    assert run_command(str(benchmark), "--gap", "--output-folder", str(output)) == 0

    written = sorted(path.name for path in output.iterdir())
    assert written == [
        "analysis.json",
        "command.txt",
        "convergence-C101_25.csv",
        "convergence-C101_25.png",
        "convergence-R201_25.csv",
        "convergence-R201_25.png",
    ]
    data = read_data(output / "convergence-C101_25.csv")
    assert list(data[0]) == list(convergence.DATA_COLUMNS)
    assert {row["run"] for row in data} == {RUN, GUROBI}
    assert {row["round"] for row in data} == {"1"}
    assert len(data) == 8

    record = run_folder.read_json(output / "analysis.json")
    assert record["command"] == "convergence"
    assert record["options"]["gap"] is True
    assert record["options"]["format"] == ["png"]
    assert record["outputs"] == [
        "convergence-C101_25.png",
        "convergence-C101_25.csv",
        "convergence-R201_25.png",
        "convergence-R201_25.csv",
    ]
    assert "warning" in capsys.readouterr().err  # no bound: the gap panel is empty


def test_the_command_keeps_the_runs_where_asks(tmp_path: Path):
    benchmark = tmp_path / "benchmark"
    write_runs(benchmark, {RUN: options(), GUROBI: options(master="gurobipy")})

    for name in (RUN, GUROBI):
        write_iterations(round_folder(benchmark, name, 1), lines(3, 1.0))

    output = tmp_path / "analysis"
    arguments = [str(benchmark), "--where", "master.type=gurobipy", "--x", "iteration"]

    assert run_command(*arguments, "--output-folder", str(output)) == 0

    data = read_data(output / "convergence-C101_25.csv")
    assert {row["run"] for row in data} == {GUROBI}


def test_the_command_refuses_runs_without_iterations(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    write_runs(tmp_path / "benchmark", {RUN: options()})
    output = tmp_path / "analysis"

    assert run_command(str(tmp_path / "benchmark"), "--output-folder", str(output)) == 2

    assert "no run recorded its iterations" in capsys.readouterr().err
    assert not output.exists()


@pytest.mark.parametrize("wrong", ["1,1", "2,1", "low,high", "1", "nan,2"])
def test_the_command_refuses_a_wrong_range(tmp_path: Path, wrong: str):
    with pytest.raises(SystemExit) as error:
        run_command(str(tmp_path), "--y-range", wrong)

    assert error.value.code == 2
