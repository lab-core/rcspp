#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The draw command, on the tree's C101_25 with solutions written by hand: the instance
alone, the integer routes and their time windows, the LP's arc flows, the runs left
out."""

from pathlib import Path
from typing import Any

import analysis
import pytest
from benchmark_folders import (
    RUN,
    metadata,
    options,
    read_csv,
    result,
    round_folder,
    write_benchmark,
    write_round,
)
from column_generation_analysis.commands import draw
from column_generation_analysis.vrp import drawing
from column_generation_benchmark import run_folder
from column_generation_benchmark.instances import SolomonInstance, distance_between, make_instance

GUROBI = RUN.replace("highspy", "gurobipy")
SINK = 26


def c101() -> SolomonInstance:
    instance = make_instance({"type": "solomon", "name": "C101_25"})
    assert isinstance(instance, SolomonInstance)
    return instance


def column(path: list[int], value: float = 1.0) -> dict[str, Any]:
    return {"id": path[1], "value": value, "cost": 1.0, "path": path, "rows": []}


def solution(integer: list[dict[str, Any]] | None, lp: list[dict[str, Any]]) -> dict[str, Any]:
    return {
        "lp": {"objective": 1.0, "columns": lp, "artificial_columns": [], "duals": []},
        "integer": (
            None
            if integer is None
            else {
                "status": "Optimal",
                "objective": 1.0,
                "columns": integer,
                "artificial_columns": [],
            }
        ),
        "resolved": None,
    }


def write_runs(path: Path, solutions: dict[str, dict[str, Any] | None]) -> None:
    """A benchmark of a run per solution on C101_25, one round each; None: no
    ``solution.json``."""
    write_benchmark(path, {name: options() for name in solutions}, rounds=1)

    for name, found in solutions.items():
        folder = round_folder(path, name, 1)
        write_round(folder, result(), metadata(), options())

        if found is not None:
            run_folder.write_json(folder / run_folder.SOLUTION_FILE, found)


ROUTES = [[0, 5, 3, 7, SINK], [0, 13, 17, SINK]]
LP = [column([0, 5, 3, SINK], 0.5), column([0, 5, 7, SINK], 0.5)]


# MARK: Drawing


def test_a_route_is_timed_as_the_check_times_it():
    instance = c101()
    depot, first, second = (instance.customers[number] for number in (0, 5, 3))

    found = drawing.visits(instance, [5, 3])

    arrival = depot.ready_time + distance_between(depot, first)
    start = max(first.ready_time, arrival)
    assert found[0] == {
        "customer": 5,
        "ready_time": first.ready_time,
        "due_time": first.due_time,
        "arrival": arrival,
        "start": start,
        "end": start + first.service_time,
    }
    assert found[1]["arrival"] == pytest.approx(
        start + first.service_time + distance_between(first, second)
    )


def test_the_arc_values_sum_the_columns_that_take_each_arc():
    assert drawing.arc_values(LP) == {
        (0, 5): 1.0,
        (5, 3): 0.5,
        (3, SINK): 0.5,
        (5, 7): 0.5,
        (7, SINK): 0.5,
    }


def test_the_customers_no_route_visits():
    assert drawing.unvisited(c101(), ROUTES)[:3] == [1, 2, 4]
    assert len(drawing.unvisited(c101(), ROUTES)) == 25 - 5


@pytest.mark.parametrize(
    ("asked", "found", "kind"),
    [
        ("auto", solution([column([0, 1, SINK])], LP), "integer"),
        ("auto", solution(None, LP), "lp"),
        ("auto", solution([], []), None),
        ("integer", solution(None, LP), None),
        ("lp", solution([column([0, 1, SINK])], LP), "lp"),
        ("none", None, "none"),
        ("auto", None, None),
    ],
)
def test_the_solution_drawn(asked: str, found: dict[str, Any] | None, kind: str | None):
    assert draw.chosen_kind(asked, found) == kind


# MARK: The command


def test_an_instance_alone(tmp_path: Path):
    output = tmp_path / "analysis"

    assert analysis.main(["draw", "--instance", "C101_25", "--output-folder", str(output)]) == 0

    assert sorted(path.name for path in output.iterdir()) == [
        "analysis.json",
        "command.txt",
        "draw-C101_25.pdf",
        "draw-C101_25.png",
    ]
    record = run_folder.read_json(output / "analysis.json")
    assert record["options"]["instance"] == "C101_25"
    assert record["inputs"] == []


def test_a_run_with_its_routes_and_their_windows(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    write_runs(tmp_path / "benchmark", {RUN: solution([column(path) for path in ROUTES], LP)})
    output = tmp_path / "analysis"
    arguments = [
        "draw",
        str(tmp_path / "benchmark"),
        "--format",
        "png",
        "--output-folder",
        str(output),
    ]

    assert analysis.main(arguments) == 0

    name = f"draw-{RUN}"
    assert sorted(path.name for path in output.iterdir()) == sorted(
        ["analysis.json", "command.txt", f"{name}.png", f"{name}-windows.png", f"{name}-routes.csv"]
    )
    rows = read_csv(output / f"{name}-routes.csv")
    assert [(row["route"], row["position"], row["customer"]) for row in rows] == [
        ("1", "1", "5"),
        ("1", "2", "3"),
        ("1", "3", "7"),
        ("2", "1", "13"),
        ("2", "2", "17"),
    ]
    # Most customers are on no route: the integer master left them to artificial columns.
    assert "no route visits customers 1, 2, 4," in capsys.readouterr().err


def test_a_run_with_its_lp(tmp_path: Path):
    write_runs(tmp_path / "benchmark", {RUN: solution(None, LP)})
    output = tmp_path / "analysis"
    arguments = ["draw", str(tmp_path / "benchmark"), "--solution", "lp", "--format", "png"]

    assert analysis.main([*arguments, "--output-folder", str(output)]) == 0

    rows = read_csv(output / f"draw-{RUN}-arcs.csv")
    assert {(row["from"], row["to"]): float(row["value"]) for row in rows}[("0", "5")] == 1.0
    assert not (output / f"draw-{RUN}-windows.png").exists()


def test_runs_without_a_solution_are_left_out_with_a_warning(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    write_runs(tmp_path / "benchmark", {RUN: solution(None, LP), GUROBI: None})
    output = tmp_path / "analysis"
    arguments = ["draw", str(tmp_path / "benchmark"), "--solution", "integer", "--format", "png"]

    assert analysis.main([*arguments, "--output-folder", str(output)]) == 0

    errors = capsys.readouterr().err
    assert f"{RUN} has no an integer solution: left out" not in errors
    assert f"{RUN} has no integer solution: left out" in errors
    assert f"{GUROBI} has no solution.json: left out" in errors
    assert [path.name for path in output.iterdir() if path.suffix == ".png"] == []


@pytest.mark.parametrize(
    ("arguments", "message"),
    [
        ([], "give runs or --instance"),
        (["{benchmark}", "--instance", "C101_25"], "give runs or --instance"),
        (["--instance", "C999_25"], "instance C999_25"),
        (["{benchmark}"], "no run wrote solution.json"),
    ],
)
def test_the_command_refuses(
    tmp_path: Path, capsys: pytest.CaptureFixture[str], arguments: list[str], message: str
):
    write_runs(tmp_path / "benchmark", {RUN: None})
    output = tmp_path / "analysis"
    given = [argument.format(benchmark=tmp_path / "benchmark") for argument in arguments]

    assert analysis.main(["draw", *given, "--output-folder", str(output)]) == 2

    assert message in capsys.readouterr().err
    assert not output.exists()
