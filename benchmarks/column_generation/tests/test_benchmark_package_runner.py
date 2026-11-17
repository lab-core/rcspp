#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The package runner: a run fills its folder, refuses one that holds a run, writes the LP
solution, and runs the same again from its ``run.toml`` and from its ``command.txt``."""

import shlex
import subprocess  # nosec B404: the runner's own recorded command
import sys
from pathlib import Path
from typing import Any

import package_runner
import pytest
from column_generation_benchmark import options, record, run_folder, runner

pytestmark = pytest.mark.usefixtures("build")

#: What must come out the same when a run runs again.
SAME = ("status", "objective", "iterations", "columns", "artificial_columns")


def run(build: Path, folder: Path, *arguments: str) -> dict[str, Any]:
    """The result of a run of the package runner into `folder`."""
    assert package_runner.main(["--build", str(build), "--output", str(folder), *arguments]) == 0
    result = run_folder.read_result(folder)
    assert result is not None
    return result


def same(result: dict[str, Any]) -> dict[str, Any]:
    return {name: result[name] for name in SAME}


def rerun_command(folder: Path) -> dict[str, Any]:
    """The result of the command a folder's ``command.txt`` holds, run again into the
    same folder (with ``--overwrite``, which it needs there)."""
    command = (folder / run_folder.COMMAND_FILE).read_text(encoding="utf-8").strip()
    command += " --overwrite"
    arguments = command if sys.platform == "win32" else shlex.split(command)
    subprocess.run(arguments, check=True, capture_output=True)  # nosec B603
    result = run_folder.read_result(folder)
    assert result is not None
    return result


# MARK: A run


def test_a_run_fills_its_folder(build: Path, tmp_path: Path):
    result = run(build, tmp_path, "--instance", "C101_5", "--machine", "test")

    assert run_folder.result_problems(result) == []
    assert result["status"] == "Optimal"
    assert result["iterations"] > 0 and result["columns"] > 0
    assert "process_peak_memory_bytes" in result

    metadata = run_folder.read_json(tmp_path / run_folder.METADATA_FILE)
    assert {"start", "end", *record.RECORD_GROUPS} <= set(metadata)
    assert metadata["machine"]["label"] == "test"
    assert "highspy" in metadata["versions"]

    expected = options.expand_run({"instance": "C101_5"})
    assert run_folder.read_run_file(tmp_path) == expected.to_table()
    assert "--instance C101_5" in (tmp_path / run_folder.COMMAND_FILE).read_text()


def test_a_run_goes_to_the_results_folder_by_default(
    build: Path, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
):
    monkeypatch.setattr(runner, "RESULTS", tmp_path)

    assert package_runner.main(["--build", str(build), "--instance", "C101_5"]) == 0
    assert run_folder.read_result(tmp_path / "C101_5-package-highspy-exact-artificial")


@pytest.mark.parametrize(
    "arguments",
    [
        ("--start", "single-customer"),
        ("--stage", "tabu:max_solutions=5,max_iterations=100", "--stage", "simple"),
        ("--column_generation.max_iterations", "3"),
        ("--column_generation.tolerances", "relative", "--master.parameters", "{ threads = 1 }"),
    ],
)
def test_the_run_options_reach_the_package(build: Path, tmp_path: Path, arguments: tuple[str]):
    result = run(build, tmp_path, "--instance", "C101_5", *arguments)

    assert run_folder.result_problems(result) == []

    if "--column_generation.max_iterations" in arguments:
        assert (result["status"], result["iterations"]) == ("IterationLimit", 3)
    else:
        assert result["status"] == "Optimal"


def test_a_build_check_builds_without_writing(
    build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    arguments = ["--build", str(build), "--instance", "C101_5", "--check", "build"]

    assert package_runner.main(arguments) == 0
    assert capsys.readouterr().out.strip() == "C101_5-package-highspy-exact-artificial: built"
    assert list(tmp_path.iterdir()) == []


@pytest.mark.parametrize(
    ("arguments", "message"),
    [
        (["--instance", "C101_5", "--layer", "cpp-bindings"], "runs layer package"),
        (["--instance", "MISSING"], "MISSING"),
        (["--instance", "C101_5", "--master.parameters", "{ nope = 1 }"], "highspy refuses"),
        (["--instance", "C101_5", "--stage", "simple:nope=1"], "nope"),
    ],
)
def test_wrong_options_stop_before_the_run(
    build: Path,
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
    arguments: list[str],
    message: str,
):
    folder = tmp_path / "run"

    assert package_runner.main(["--build", str(build), "--output", str(folder), *arguments]) == 2
    assert message in capsys.readouterr().err
    assert run_folder.read_result(folder) is None


# MARK: Collisions


def test_a_folder_that_holds_a_run_is_refused(
    build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    run_folder.write_json(tmp_path / run_folder.METADATA_FILE, {"exit_code": 1})
    arguments = ["--build", str(build), "--output", str(tmp_path), "--instance", "C101_5"]

    assert package_runner.main(arguments) == 2
    assert "--overwrite" in capsys.readouterr().err
    assert [path.name for path in tmp_path.iterdir()] == [run_folder.METADATA_FILE]


def test_overwrite_removes_the_folder_first(build: Path, tmp_path: Path):
    run_folder.write_json(tmp_path / run_folder.METADATA_FILE, {"exit_code": 1})
    (tmp_path / "notes.txt").write_text("old")
    run(build, tmp_path, "--instance", "C101_5", "--statistics.process", "false", "--overwrite")

    assert "exit_code" not in run_folder.read_json(tmp_path / run_folder.METADATA_FILE)
    assert "process_peak_memory_bytes" not in (run_folder.read_result(tmp_path) or {})
    assert not (tmp_path / "notes.txt").exists()


def test_a_folder_without_run_files_is_kept(build: Path, tmp_path: Path):
    (tmp_path / "log.txt").write_text("a benchmark's log")
    run(build, tmp_path, "--instance", "C101_5")

    assert (tmp_path / "log.txt").read_text() == "a benchmark's log"


# MARK: Solution


@pytest.mark.parametrize("start", ["artificial", "single-customer"])
def test_the_solution_covers_every_customer_once(build: Path, tmp_path: Path, start: str):
    result = run(build, tmp_path, "--instance", "C101_10", "--start", start)
    lp = run_folder.read_json(tmp_path / run_folder.SOLUTION_FILE)["lp"]

    assert lp["objective"] == result["objective"]
    assert result["solution_columns"] == len(lp["columns"])
    assert lp["artificial_columns"] == []

    # Rows 1..10 are the customers, each covered exactly once; row 0 is free.
    covered = [0.0] * 11

    for column in lp["columns"]:
        assert column["path"][0] == 0 and column["path"][-1] == 11

        for index, coefficient in column["rows"]:
            covered[index] += coefficient * column["value"]

    assert covered[1:] == pytest.approx([1.0] * 10)
    assert sum(c["cost"] * c["value"] for c in lp["columns"]) == pytest.approx(lp["objective"])

    # Strong duality: the duals times the right-hand sides give the objective.
    assert len(lp["duals"]) == 11
    assert sum(lp["duals"][1:]) == pytest.approx(lp["objective"])


def test_the_solution_can_be_left_out(build: Path, tmp_path: Path):
    result = run(build, tmp_path, "--instance", "C101_5", "--statistics.solution", "false")

    assert "solution_columns" not in result
    assert not (tmp_path / run_folder.SOLUTION_FILE).exists()


def test_artificial_columns_in_use_are_listed_apart(build: Path, tmp_path: Path):
    # Artificial columns cheaper than any route: the LP keeps them.
    result = run(build, tmp_path, "--instance", "C101_10", "--start.cost_factor", "1e-4")
    lp = run_folder.read_json(tmp_path / run_folder.SOLUTION_FILE)["lp"]

    assert result["status"] == "Infeasible"
    assert lp["artificial_columns"]
    assert all(set(column) == {"id", "value"} for column in lp["artificial_columns"])


def test_a_run_stopped_after_pricing_has_no_lp_solution(build: Path, tmp_path: Path):
    result = run(
        build, tmp_path, "--instance", "C101_10", "--column_generation.max_iterations", "1"
    )
    solution = run_folder.read_json(tmp_path / run_folder.SOLUTION_FILE)

    assert result["status"] == "IterationLimit"
    assert result["solution_columns"] is None
    assert solution["lp"] is None
    assert "columns were added after the last LP solve" in solution["missing"]


# MARK: Runs again


def test_a_run_from_the_command_line_runs_again(build: Path, tmp_path: Path):
    first = run(build, tmp_path / "first", "--instance", "C101_5", "--start", "single-customer")
    again = run(build, tmp_path / "again", "--file", str(tmp_path / "first" / "run.toml"))

    assert same(again) == same(first)
    assert same(rerun_command(tmp_path / "first")) == same(first)


def test_a_run_from_a_run_file_runs_again(build: Path, tmp_path: Path):
    path = tmp_path / "options.toml"
    path.write_text(
        'instance = "C101_5"\n'
        'pricing = { stages = [{ algorithm = "tabu", max_solutions = 5, max_iterations = 100 }, '
        '{ algorithm = "simple" }] }\n'
        '"start.cost_factor" = 1e3\n'
    )
    first = run(
        build, tmp_path / "first", "--file", str(path), "--master.parameters", "{ threads = 1 }"
    )
    again = run(build, tmp_path / "again", "--file", str(tmp_path / "first" / "run.toml"))

    assert run_folder.read_run_file(tmp_path / "again") == run_folder.read_run_file(
        tmp_path / "first"
    )
    assert same(again) == same(first)
    assert same(rerun_command(tmp_path / "first")) == same(first)
