#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The analysis's core, on benchmark folders written by hand: which runs the arguments
and conditions select, the tables read from their folders, and the folder an analysis
writes."""

import json
from datetime import datetime
from pathlib import Path
from typing import Any

import pytest
from benchmark_folders import (
    COMMIT,
    RUN,
    iteration_line,
    metadata,
    options,
    result,
    round_folder,
    write_benchmark,
    write_iterations,
    write_round,
    write_rounds,
)
from column_generation_analysis.core import analysis_folder, tables
from column_generation_analysis.core.selection import SelectionError, select
from column_generation_benchmark import run_folder
from column_generation_benchmark.folder import FolderError

OTHER = RUN.replace("package", "cpp-bindings")
GUROBI = RUN.replace("highspy", "gurobipy")


def write_three_runs(path: Path) -> None:
    """A benchmark of three runs, two rounds each: the package and the C++ example's
    bindings over highspy, the package over gurobipy."""
    runs = {
        RUN: options(),
        OTHER: options(layer="cpp-bindings"),
        GUROBI: options(master="gurobipy"),
    }
    write_benchmark(path, runs, rounds=2)

    for name, run_options in runs.items():
        for number in (1, 2):
            folder = round_folder(path, name, number)
            write_round(folder, result(), metadata(), run_options)


def names(folders: list[Any]) -> list[str]:
    return [cell.run for folder in folders for cell in folder.cells]


# MARK: Selection


def test_a_folder_selects_every_run(tmp_path: Path) -> None:
    write_three_runs(tmp_path)

    assert names(select([str(tmp_path)])) == [RUN, OTHER, GUROBI]


def test_a_prefix_selects_every_run_it_starts(tmp_path: Path) -> None:
    write_three_runs(tmp_path)

    assert names(select([f"{tmp_path}:C101_25-package"])) == [RUN, GUROBI]


def test_a_full_name_selects_that_run_alone(tmp_path: Path) -> None:
    write_three_runs(tmp_path)
    longer = RUN + "-more"
    write_rounds(tmp_path / "two", RUN, [result()])
    write_round(round_folder(tmp_path / "two", longer, 1), result(), metadata(), options())

    assert names(select([f"{tmp_path / 'two'}:{RUN}"])) == [RUN]


def test_a_folder_named_twice_is_read_once(tmp_path: Path) -> None:
    write_three_runs(tmp_path)

    found = select([f"{tmp_path}:{GUROBI}", str(tmp_path), f"{tmp_path}:{RUN}"])

    assert len(found) == 1
    assert names(found) == [GUROBI, RUN, OTHER]


def test_a_run_folder_alone_is_one_run_without_a_round(tmp_path: Path) -> None:
    folder = tmp_path / "alone"
    write_round(folder, result(), metadata(outcome=None), options())

    found = select([str(folder)])

    assert names(found) == ["alone"]
    assert found[0].rounds[0].number is None


def test_where_keeps_the_runs_whose_option_has_the_value(tmp_path: Path) -> None:
    write_three_runs(tmp_path)

    assert names(select([str(tmp_path)], ["master.type=gurobipy"])) == [GUROBI]
    assert names(select([str(tmp_path)], ['layer="package"'])) == [RUN, GUROBI]
    assert names(select([str(tmp_path)], ["start.cost_factor=10000"])) == [RUN, OTHER, GUROBI]
    assert names(select([str(tmp_path)], ["layer=package", "master.type=highspy"])) == [RUN]


@pytest.mark.parametrize(
    ("arguments", "where", "message"),
    [
        (["{path}:nothing"], [], "nothing names no run"),
        (["{path}/missing"], [], "neither a folder nor"),
        (["{path}"], ["layer"], "not <dotted option>=<value>"),
        (["{path}"], ["graph.ng_sise=8"], "no run has that option"),
        (["{path}"], ["master.type=gurobi"], "no run meets"),
    ],
)
def test_a_selection_of_no_run_is_an_error(
    tmp_path: Path, arguments: list[str], where: list[str], message: str
) -> None:
    write_three_runs(tmp_path)

    with pytest.raises(SelectionError, match=message):
        select([argument.format(path=tmp_path) for argument in arguments], where)


def test_a_folder_without_runs_is_an_error(tmp_path: Path) -> None:
    with pytest.raises(FolderError):
        select([str(tmp_path)])


# MARK: Runs table


def test_the_runs_table_has_a_row_per_round(tmp_path: Path) -> None:
    write_three_runs(tmp_path)

    runs = tables.runs_table(select([str(tmp_path)]))

    assert list(runs.columns[:5]) == ["benchmark", "run", "round", "folder", "outcome"]
    assert len(runs) == 6
    assert list(runs["round"]) == [1, 2, 1, 2, 1, 2]
    assert set(runs["benchmark"]) == {tmp_path.as_posix()}
    assert list(runs["master.type"]) == ["highspy"] * 4 + ["gurobipy"] * 2
    assert list(runs["objective"]) == [191.8136] * 6
    assert list(runs["source.commit"]) == [COMMIT] * 6


def test_the_runs_table_has_the_rounds_without_a_result(tmp_path: Path) -> None:
    write_rounds(tmp_path, RUN, [result()])
    write_round(round_folder(tmp_path, RUN, 2), None, metadata(outcome="crashed"), options())

    runs = tables.runs_table(select([str(tmp_path)]))

    assert list(runs["outcome"]) == ["finished", "crashed"]
    assert runs["objective"].isna().tolist() == [False, True]


def test_two_folders_are_told_apart(tmp_path: Path) -> None:
    write_rounds(tmp_path / "one", RUN, [result()])
    write_rounds(tmp_path / "two", RUN, [result(objective=200.0)])

    runs = tables.runs_table(select([str(tmp_path / "one"), str(tmp_path / "two")]))

    assert list(runs["benchmark"]) == [(tmp_path / name).as_posix() for name in ("one", "two")]
    assert list(runs["objective"]) == [191.8136, 200.0]


# MARK: Iteration tables


def stage(number: int, **changes: Any) -> dict[str, Any]:
    found: dict[str, Any] = {"stage": number, "status": "COMPLETE", "new_columns": 5}
    found.update(changes)
    return found


def test_the_iteration_tables_split_iterations_pricers_and_stages(tmp_path: Path) -> None:
    write_rounds(tmp_path, RUN, [result()])
    pricers = [
        {
            "status": "Found",
            "new_columns": 10,
            "min_reduced_cost": -5.0,
            "seconds": 0.01,
            "stages": [
                stage(0, extended_labels=100, bidirectional=None),
                stage(
                    1,
                    extended_labels=200,
                    bidirectional={"half_way_used": 50.0, "number_of_joined_paths": 3},
                ),
            ],
        }
    ]
    lines = [
        iteration_line(1, relaxation_seconds=0.01, pricers=pricers, duals=[1.0, 2.0]),
        iteration_line(2, relaxation_seconds=0.02, pricers=pricers, duals=[3.0, 4.0]),
    ]
    write_iterations(round_folder(tmp_path, RUN, 1), lines)

    iterations, pricer_table, stage_table = tables.iteration_tables(select([str(tmp_path)]))

    assert list(iterations["iteration"]) == [1, 2]
    assert list(iterations["relaxation_seconds"]) == [0.01, 0.02]
    assert "pricers" not in iterations.columns
    assert "duals" not in iterations.columns
    assert list(pricer_table.columns[:5]) == ["benchmark", "run", "round", "iteration", "pricer"]
    assert list(pricer_table["min_reduced_cost"]) == [-5.0, -5.0]
    assert "stages" not in pricer_table.columns
    assert list(stage_table["stage"]) == [0, 1, 0, 1]
    assert list(stage_table["extended_labels"]) == [100, 200, 100, 200]
    assert stage_table["bidirectional.half_way_used"].tolist()[1] == 50.0


def test_a_round_without_iterations_gives_no_rows(tmp_path: Path) -> None:
    write_rounds(tmp_path, RUN, [result(), result()])
    write_iterations(round_folder(tmp_path, RUN, 2), [iteration_line(1)])

    iterations, pricer_table, stage_table = tables.iteration_tables(select([str(tmp_path)]))

    assert list(iterations["round"]) == [2]
    assert pricer_table.empty
    assert list(pricer_table.columns) == ["benchmark", "run", "round", "iteration", "pricer"]
    assert stage_table.empty


def test_a_run_killed_keeps_its_iterations_but_a_line_cut_off(tmp_path: Path) -> None:
    write_rounds(tmp_path, RUN, [result(status="timeout", objective=None)])
    folder = round_folder(tmp_path, RUN, 1)
    write_iterations(folder, [iteration_line(1), iteration_line(2)], tail='{"iteration": 3, "obj')

    iterations, _, _ = tables.iteration_tables(select([str(tmp_path)]))

    assert list(iterations["iteration"]) == [1, 2]


def test_a_line_that_cannot_be_read_before_the_last_is_an_error(tmp_path: Path) -> None:
    write_rounds(tmp_path, RUN, [result()])
    folder = round_folder(tmp_path, RUN, 1)
    text = json.dumps(iteration_line(1)) + "\nnot json\n" + json.dumps(iteration_line(3)) + "\n"
    (folder / run_folder.ITERATIONS_FILE).write_text(text, encoding="utf-8")

    with pytest.raises(ValueError, match="line 2"):
        tables.iteration_tables(select([str(tmp_path)]))


def test_the_long_lists_have_a_row_per_item(tmp_path: Path) -> None:
    write_rounds(tmp_path, RUN, [result()])
    added = [
        {"id": 30, "cost": 12.5, "path": [0, 3, 26], "rows": [[3, 1.0]]},
        {"id": 31, "cost": 20.0, "path": [0, 4, 5, 26], "rows": [[4, 1.0], [5, 1.0]]},
    ]
    lines = [
        iteration_line(1, duals=[1.0, 2.0, 3.0], master_values=[[0, 1.0]], added_columns=added),
        iteration_line(2, duals=[4.0, 5.0, 6.0], master_values=[[0, 0.5], [30, 0.5]]),
    ]
    write_iterations(round_folder(tmp_path, RUN, 1), lines)
    folders = select([str(tmp_path)])

    duals = tables.duals_table(folders)
    values = tables.master_values_table(folders)
    columns = tables.added_columns_table(folders)

    assert list(duals["iteration"]) == [1, 1, 1, 2, 2, 2]
    assert list(duals["row"]) == [0, 1, 2, 0, 1, 2]
    assert list(duals["dual"]) == [1.0, 2.0, 3.0, 4.0, 5.0, 6.0]
    assert list(values["column"]) == [0, 0, 30]
    assert list(values["value"]) == [1.0, 0.5, 0.5]
    assert list(columns["column"]) == [30, 31]
    assert list(columns["path"]) == [[0, 3, 26], [0, 4, 5, 26]]
    assert list(columns["rows"])[1] == [[4, 1.0], [5, 1.0]]


# MARK: Solution tables


def solution_file(folder: Path) -> None:
    solution = {
        "lp": {
            "objective": 30.0,
            "columns": [
                {"id": 30, "value": 0.5, "cost": 10.0, "path": [0, 1, 3], "rows": [[1, 1.0]]},
                {"id": 31, "value": 0.5, "cost": 20.0, "path": [0, 2, 3], "rows": [[2, 1.0]]},
            ],
            "artificial_columns": [{"id": 1, "value": 0.25}],
            "duals": [0.0, 10.0, 20.0],
            "solved_again": False,
        },
        "integer": {
            "status": "Optimal",
            "objective": 30.0,
            "columns": [
                {"id": 30, "value": 1.0, "cost": 10.0, "path": [0, 1, 3], "rows": [[1, 1.0]]}
            ],
            "artificial_columns": [],
        },
        "resolved": {"routes": [[1], [2]], "vehicles": 2, "distance": 30.0, "errors": []},
    }
    run_folder.write_json(folder / run_folder.SOLUTION_FILE, solution)


def test_the_solution_columns_have_their_kind(tmp_path: Path) -> None:
    write_rounds(tmp_path, RUN, [result(), result()])
    solution_file(round_folder(tmp_path, RUN, 1))

    columns = tables.solution_columns_table(select([str(tmp_path)]))

    assert list(columns["kind"]) == ["lp", "lp", "lp", "integer"]
    assert list(columns["column"]) == [30, 31, 1, 30]
    assert list(columns["artificial"]) == [False, False, True, False]
    assert list(columns["path"])[0] == [0, 1, 3]
    assert columns["cost"].isna().tolist() == [False, False, True, False]
    assert list(columns["round"]) == [1, 1, 1, 1]


def test_the_resolved_solutions_by_round(tmp_path: Path) -> None:
    write_rounds(tmp_path, RUN, [result(), result()])
    solution_file(round_folder(tmp_path, RUN, 2))

    found = tables.resolved_solutions(select([str(tmp_path)]))

    assert list(found) == [(tmp_path.as_posix(), RUN, 2)]
    assert found[(tmp_path.as_posix(), RUN, 2)]["routes"] == [[1], [2]]


# MARK: Analysis folder


def test_the_default_folder_is_named_after_the_command_and_the_time() -> None:
    folder = analysis_folder.default_folder("convergence", datetime(2026, 11, 27, 15, 30, 12))

    assert folder.name == "convergence-20261127-153012"
    assert folder.parent.name == "analysis"
    assert folder.parent.parent.name == "results"


def test_a_new_or_empty_folder_takes_an_analysis(tmp_path: Path) -> None:
    analysis_folder.prepare_folder(tmp_path / "new" / "one")
    (tmp_path / "empty").mkdir()
    analysis_folder.prepare_folder(tmp_path / "empty")

    assert (tmp_path / "new" / "one").is_dir()


def write_analysis(folder: Path, outputs: list[str]) -> None:
    analysis_folder.prepare_folder(folder)

    for name in outputs:
        (folder / name).write_text("output")

    analysis_folder.write_record(folder, "sweep", ["analysis.py", "sweep"], {}, [], outputs, [])


def test_an_analysis_is_replaced_only_with_overwrite(tmp_path: Path) -> None:
    write_analysis(tmp_path, ["sweep.png", "sweep.csv"])
    (tmp_path / "notes.txt").write_text("mine")

    with pytest.raises(analysis_folder.AnalysisFolderError, match="already holds an analysis"):
        analysis_folder.prepare_folder(tmp_path)

    analysis_folder.prepare_folder(tmp_path, overwrite=True)

    assert sorted(path.name for path in tmp_path.iterdir()) == ["notes.txt"]


def test_a_folder_of_other_files_is_refused_even_with_overwrite(tmp_path: Path) -> None:
    write_rounds(tmp_path, RUN, [result()])

    with pytest.raises(analysis_folder.AnalysisFolderError, match="holds files but no analysis"):
        analysis_folder.prepare_folder(tmp_path, overwrite=True)

    assert (tmp_path / run_folder.BENCHMARK_FILE).is_file()


def test_the_record_has_the_command_the_source_the_inputs_and_the_outputs(
    tmp_path: Path,
) -> None:
    benchmark = tmp_path / "benchmark"
    write_rounds(benchmark, RUN, [result(), result()])
    write_iterations(round_folder(benchmark, RUN, 1), [iteration_line(1)])
    output = tmp_path / "analysis"
    analysis_folder.prepare_folder(output)
    arguments = ["analysis.py", "convergence", str(benchmark)]

    analysis_folder.write_record(
        output,
        "convergence",
        arguments,
        {"x": "time"},
        select([str(benchmark)]),
        ["convergence-C101_25.png"],
        ["a warning"],
    )

    found = run_folder.read_json(output / analysis_folder.RECORD_FILE)
    assert found["command"] == "convergence"
    assert found["arguments"] == arguments
    assert found["options"] == {"x": "time"}
    assert set(found["source"]) >= {"commit", "uncommitted_changes", "changes_digest"}
    assert set(found["versions"]) == {"python", "numpy", "pandas", "matplotlib"}
    assert found["outputs"] == ["convergence-C101_25.png"]
    assert found["warnings"] == ["a warning"]

    (inputs,) = found["inputs"]
    assert inputs["benchmark"] == benchmark.as_posix()
    assert inputs["path"] == benchmark.resolve().as_posix()
    first, second = inputs["rounds"]
    assert (first["run"], first["round"], first["folder"]) == (RUN, 1, f"runs/{RUN}-r1")
    assert set(first["files"]) == {"run.toml", "metadata.json", "result.json", "iterations.jsonl"}
    assert "iterations.jsonl" not in second["files"]
    assert len(first["files"]["result.json"]) == 12

    command = (output / analysis_folder.COMMAND_FILE).read_text()
    assert "convergence" in command
    assert command.endswith("\n")


def test_a_changed_input_changes_its_digest(tmp_path: Path) -> None:
    write_rounds(tmp_path, RUN, [result()])
    before = analysis_folder.inputs_record(select([str(tmp_path)]))
    run_folder.write_json(
        round_folder(tmp_path, RUN, 1) / run_folder.RESULT_FILE, result(objective=1.0)
    )

    after = analysis_folder.inputs_record(select([str(tmp_path)]))

    files_before = before[0]["rounds"][0]["files"]
    files_after = after[0]["rounds"][0]["files"]
    assert files_before["result.json"] != files_after["result.json"]
    assert files_before["run.toml"] == files_after["run.toml"]
