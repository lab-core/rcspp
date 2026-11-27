#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The sweep command, on benchmark folders written by hand: the points, the values on
the x axis, the lines, the relative values, the figures and their tables."""

from pathlib import Path
from typing import Any

import analysis
import pytest
from benchmark_folders import (
    metadata,
    options,
    read_csv,
    result,
    round_folder,
    write_benchmark,
    write_round,
)
from column_generation_analysis.commands import sweep
from column_generation_analysis.core.command_line import CommandError
from column_generation_analysis.core.selection import select
from column_generation_benchmark import run_folder

BASE = "package-highspy-exact-artificial"


def name_of(instance: str, size: int | None) -> str:
    return f"{instance}-{BASE}" + ("" if size is None else f"-graph_ng{size}")


def write_sweep(
    path: Path,
    sizes: tuple[int | None, ...] = (4, 8, 16),
    instances: tuple[str, ...] = ("C101_25", "R201_25"),
    rounds: int = 2,
) -> dict[str, dict[str, Any]]:
    """A benchmark of the package on `instances` for each ng size (None: without ng, no
    graph option): the objective 100 + the size (0 without), the solve time 1 + size /
    8, plus 0.1 a round."""
    runs: dict[str, dict[str, Any]] = {}

    for instance in instances:
        for size in sizes:
            run_options = options(instance=instance)

            if size is not None:
                run_options["graph"] = {"relaxation": "ng", "ng_size": size}

            runs[name_of(instance, size)] = run_options

    write_benchmark(path, runs, rounds=rounds)

    for name, run_options in runs.items():
        size = run_options["graph"].get("ng_size", 0)

        for number in range(1, rounds + 1):
            found = result(objective=100.0 + size, solve_seconds=1.0 + size / 8 + number * 0.1)
            write_round(round_folder(path, name, number), found, metadata(), run_options)

    return runs


def points_of(path: Path, swept: list[str], fields: list[str]) -> Any:
    return sweep.read_points(select([str(path)]), swept, fields)


def by_line(points: list[dict[str, Any]], field: str) -> dict[str, list[Any]]:
    found: dict[str, list[Any]] = {}

    for point in points:
        if point["field"] == field:
            found.setdefault(point["line"], []).append((point["value"], point["median"]))

    return found


# MARK: Points


def test_a_line_per_instance_a_point_per_value(tmp_path: Path):
    write_sweep(tmp_path)

    points, values, warnings = points_of(tmp_path, ["graph.ng_size"], ["objective"])

    assert values == ["4", "8", "16"]
    assert by_line(points, "objective") == {
        "C101_25": [("4", 104.0), ("8", 108.0), ("16", 116.0)],
        "R201_25": [("4", 104.0), ("8", 108.0), ("16", 116.0)],
    }
    assert warnings == []


def test_runs_without_the_option_are_unset_and_their_own_line_when_others_differ(tmp_path: Path):
    write_sweep(tmp_path, sizes=(None, 8, 16), instances=("C101_25",))

    alone, values, _ = points_of(tmp_path, ["graph.ng_size"], ["objective"])
    both, labels, _ = points_of(tmp_path, ["graph.relaxation", "graph.ng_size"], ["objective"])

    # graph.relaxation varies too: the runs without ng are a line of their own.
    assert values == ["unset", "8", "16"]
    assert by_line(alone, "objective") == {
        "graph.relaxation=-": [("unset", 100.0)],
        "graph.relaxation=ng": [("8", 108.0), ("16", 116.0)],
    }
    assert labels == ["unset", "ng 8", "ng 16"]
    assert by_line(both, "objective") == {
        "all runs": [("unset", 100.0), ("ng 8", 108.0), ("ng 16", 116.0)]
    }


def test_a_measured_field_is_the_median_of_the_rounds_with_their_range(tmp_path: Path):
    write_sweep(tmp_path, sizes=(8,), instances=("C101_25",), rounds=3)

    points, _, _ = points_of(tmp_path, ["graph.ng_size"], ["solve_seconds", "objective"])

    seconds, objective = points
    assert (seconds["median"], seconds["min"], seconds["max"]) == pytest.approx((2.2, 2.1, 2.3))
    assert (objective["median"], objective["min"], objective["max"]) == (108.0, None, None)
    assert seconds["rounds"] == 3


def test_seconds_per_iteration_is_each_rounds_own(tmp_path: Path):
    write_sweep(tmp_path, sizes=(8,), instances=("C101_25",), rounds=1)

    (point,), _, _ = points_of(tmp_path, ["graph.ng_size"], ["seconds_per_iteration"])

    assert point["median"] == pytest.approx(2.1 / 16)


def test_several_options_make_one_label_in_the_runs_order(tmp_path: Path):
    write_sweep(tmp_path, sizes=(16, None, 8), instances=("C101_25",))

    _, values, _ = points_of(tmp_path, ["graph.relaxation", "graph.ng_size"], ["objective"])

    assert values == ["ng 16", "unset", "ng 8"]


def test_a_single_value_warns(tmp_path: Path):
    write_sweep(tmp_path, sizes=(8,))

    _, values, warnings = points_of(tmp_path, ["graph.ng_size"], ["objective"])

    assert values == ["8"]
    assert warnings == ["graph.ng_size takes one value only: 8"]


def test_a_single_line_is_all_runs(tmp_path: Path):
    write_sweep(tmp_path, instances=("C101_25",))

    points, _, _ = points_of(tmp_path, ["graph.ng_size"], ["objective"])

    assert {point["line"] for point in points} == {"all runs"}


def test_rounds_that_disagree_or_did_not_finish_are_left_out(tmp_path: Path):
    write_sweep(tmp_path, instances=("C101_25",))
    differs = name_of("C101_25", 8)
    run_folder.write_json(
        round_folder(tmp_path, differs, 2) / run_folder.RESULT_FILE, result(objective=1.0)
    )
    crashed = name_of("C101_25", 16)

    for number in (1, 2):
        folder = round_folder(tmp_path, crashed, number)
        (folder / run_folder.RESULT_FILE).unlink()
        run_folder.write_json(folder / run_folder.METADATA_FILE, metadata(outcome="crashed"))

    points, _, warnings = points_of(tmp_path, ["graph.ng_size"], ["objective", "solve_seconds"])

    assert [point["value"] for point in points if point["field"] == "objective"] == ["4"]
    assert [point["value"] for point in points if point["field"] == "solve_seconds"] == ["4", "8"]
    assert warnings == [
        f"{differs}: its rounds disagree on objective, left out of it",
        f"{crashed} has no finished round: left out",
    ]


@pytest.mark.parametrize(
    ("swept", "fields", "message"),
    [
        (["graph.ng_sise"], ["objective"], "--option graph.ng_sise: no run has that option"),
        (["graph.ng_size"], ["objectif"], "--field objectif: no finished run has that field"),
    ],
)
def test_an_option_or_a_field_no_run_has_is_an_error(
    tmp_path: Path, swept: list[str], fields: list[str], message: str
):
    write_sweep(tmp_path)

    with pytest.raises(CommandError, match=message):
        points_of(tmp_path, swept, fields)


def test_relative_divides_each_line_by_its_first_point(tmp_path: Path):
    write_sweep(tmp_path, instances=("C101_25",), rounds=1)
    points, values, _ = points_of(tmp_path, ["graph.ng_size"], ["objective", "solve_seconds"])
    warnings: list[str] = []

    found = sweep.relative(
        [point for point in points if point["field"] == "objective"], values, warnings
    )
    seconds = sweep.relative(
        [point for point in points if point["field"] == "solve_seconds"], values, warnings
    )

    assert [point["median"] for point in found] == pytest.approx([1.0, 108 / 104, 116 / 104])
    assert [point["min"] for point in seconds] == pytest.approx([1.0, 2.1 / 1.6, 3.1 / 1.6])
    assert warnings == []


def test_the_x_axis_is_a_number_axis_only_for_numbers():
    assert sweep.numeric_axis(["2", "8", "16"])
    assert not sweep.numeric_axis(["unset", "8"])
    assert not sweep.numeric_axis(["ng 2", "ng 8"])
    assert not sweep.numeric_axis(["nan", "8"])


# MARK: The command


def test_the_command_writes_a_figure_data_and_table_per_field(tmp_path: Path):
    write_sweep(tmp_path / "benchmark")
    output = tmp_path / "analysis"
    arguments = ["sweep", str(tmp_path / "benchmark"), "--option", "graph.ng_size"]

    assert analysis.main([*arguments, "--format", "png", "--output-folder", str(output)]) == 0

    assert sorted(path.name for path in output.iterdir()) == [
        "analysis.json",
        "command.txt",
        "sweep-objective.csv",
        "sweep-objective.md",
        "sweep-objective.png",
        "sweep-solve_seconds.csv",
        "sweep-solve_seconds.md",
        "sweep-solve_seconds.png",
    ]
    rows = read_csv(output / "sweep-solve_seconds.csv")
    assert list(rows[0]) == list(sweep.DATA_COLUMNS)
    assert (rows[0]["line"], rows[0]["value"], rows[0]["rounds"]) == ("C101_25", "4", "2")
    assert (rows[0]["median"], rows[0]["min"], rows[0]["max"]) == ("1.65", "1.6", "1.7")

    table = (output / "sweep-objective.md").read_text()
    assert table.splitlines()[0] == "# objective"
    assert "| C101_25 | 104 | 108 | 116 |" in table

    record = run_folder.read_json(output / "analysis.json")
    assert record["options"]["option"] == ["graph.ng_size"]
    assert record["options"]["field"] is None


def test_the_command_with_where_and_relative(tmp_path: Path):
    write_sweep(tmp_path / "benchmark")
    output = tmp_path / "analysis"
    arguments = [
        "sweep",
        str(tmp_path / "benchmark"),
        "--option",
        "graph.ng_size",
        "--where",
        "instance.name=R201_25",
        "--field",
        "objective",
        "--relative",
        "--format",
        "png",
        "--output-folder",
        str(output),
    ]

    assert analysis.main(arguments) == 0

    rows = read_csv(output / "sweep-objective.csv")
    assert {row["line"] for row in rows} == {"all runs"}
    assert [float(row["median"]) for row in rows] == pytest.approx([1.0, 108 / 104, 116 / 104])


def test_the_command_refuses_an_option_no_run_has(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    write_sweep(tmp_path / "benchmark")
    output = tmp_path / "analysis"
    arguments = ["sweep", str(tmp_path / "benchmark"), "--option", "graph.size"]

    assert analysis.main([*arguments, "--output-folder", str(output)]) == 2

    assert "no run has that option" in capsys.readouterr().err
    assert not output.exists()
