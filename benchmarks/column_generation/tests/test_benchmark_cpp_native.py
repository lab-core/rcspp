#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The ``cpp-native`` runner: its program builds and solves the run in C++, the runner
writes the folder as the other runners do, with the program's record and statistics.

The tests skip when the build has not the program (the column generation example and a
master solver).
"""

from pathlib import Path
from typing import Any

import cpp_native_runner
import pytest
from column_generation_benchmark import options, run_folder, source_tree
from column_generation_benchmark.instances import SolomonInstance, make_instance
from test_benchmark_runners import has_gurobi, rerun_command, same


@pytest.fixture
def program(build: Path) -> Path:
    try:
        return source_tree.cpp_native_program(build)
    except source_tree.SourceTreeError:
        pytest.skip("the build has not the cpp-native program")


@pytest.fixture
def gurobi(program: Path) -> None:
    instance = str(source_tree.TREE / "instances" / "C101_5.txt")
    flags = ["--instance", instance, "--master", "gurobi", "--start", "single-customer", "--check"]
    code, _, _ = cpp_native_runner.execute(program, flags)

    if code != 0 or not has_gurobi():
        pytest.skip("the program has no Gurobi master, or Gurobi has no licence")


def flags_of(table: dict[str, Any]) -> list[str]:
    """The program's flags for the run `table` expands to."""
    run = options.expand_run(table)
    instance = make_instance(run.instance.to_table())
    assert isinstance(instance, SolomonInstance)
    return cpp_native_runner.program_arguments(run, instance)


def run(build: Path, folder: Path, *arguments: str) -> dict[str, Any]:
    """The result of a run of the cpp-native runner into `folder`, master highs unless
    given."""
    if "--master" not in arguments and "--file" not in arguments:
        arguments = (*arguments, "--master", "highs")

    command = ["--build", str(build), "--output", str(folder), *arguments]
    assert cpp_native_runner.main(command) == 0
    result = run_folder.read_result(folder)
    assert result is not None
    return result


# MARK: A run


@pytest.mark.usefixtures("program")
def test_a_run_fills_its_folder(build: Path, tmp_path: Path):
    result = run(build, tmp_path, "--instance", "C101_5")
    metadata = run_folder.read_json(tmp_path / run_folder.METADATA_FILE)
    solution = run_folder.read_json(tmp_path / run_folder.SOLUTION_FILE)

    assert result["status"] == "Optimal"
    assert result["solution_check"] == "passed"
    assert result["solve_seconds"] >= result["master_seconds"] + result["pricing_seconds"] - 1e-9
    assert run_folder.read_run_file(tmp_path)["layer"] == "cpp-native"
    # The program's own record and statistics, not the runner's.
    assert metadata["versions"]["highs"].count(".") == 2
    assert metadata["code"]["program"]["path"].endswith(
        ("cpp-native-solver", "cpp-native-solver.exe")
    )
    assert metadata["code"]["program"]["compiler"]
    assert result["process_peak_memory_bytes"] > 0
    assert solution["lp"]["objective"] == pytest.approx(result["objective"])
    assert len(solution["lp"]["duals"]) == 6


@pytest.mark.usefixtures("program")
def test_the_run_options_reach_the_program(build: Path, tmp_path: Path):
    result = run(
        build,
        tmp_path,
        *("--instance", "C101_25", "--start", "single-customer"),
        *("--stage", "tabu:max_solutions=20,max_iterations=1000", "--stage", "simple"),
        *("--column_generation.tolerances", "relative", "--column_generation.threads", "1"),
        *("--master.parameters", "{ simplex_strategy = 1 }", "--integer.column_type", "integer"),
    )

    assert result["status"] == "Optimal"
    assert result["objective"] == pytest.approx(191.8136197786562, rel=1e-9)
    assert result["artificial_columns"] == 0


@pytest.mark.usefixtures("program")
def test_a_run_stopped_at_its_iteration_limit_solves_the_lp_again(build: Path, tmp_path: Path):
    result = run(
        build, tmp_path, "--instance", "C101_10", "--column_generation.max_iterations", "1"
    )
    solution = run_folder.read_json(tmp_path / run_folder.SOLUTION_FILE)

    assert result["status"] == "IterationLimit"
    assert result["lp_solved_again_seconds"] is not None
    assert solution["lp"]["solved_again"] is True


@pytest.mark.usefixtures("program")
def test_the_integer_solve_and_the_solution_can_be_left_out(build: Path, tmp_path: Path):
    result = run(
        build,
        tmp_path,
        *("--instance", "C101_5", "--integer.solve", "false", "--statistics.solution", "false"),
    )

    assert result["integer_status"] is None
    assert "solution_columns" not in result
    assert not (tmp_path / run_folder.SOLUTION_FILE).exists()


@pytest.mark.usefixtures("program")
def test_a_build_check_builds_without_writing(build: Path, tmp_path: Path):
    arguments = ["--build", str(build), "--output", str(tmp_path / "run"), "--check", "build"]

    assert cpp_native_runner.main([*arguments, "--instance", "C101_5", "--master", "highs"]) == 0
    assert not (tmp_path / "run").exists()


@pytest.mark.usefixtures("program")
@pytest.mark.parametrize(
    "wrong",
    [
        ["--master", "highspy"],
        ["--master", "highs", "--stage", "simple:nope=1"],
        ["--master", "highs", "--master.parameters", "{ nope = 1 }"],
        ["--master", "highs", "--master.parameters", "{ threads = 2 }"],
    ],
)
def test_wrong_options_stop_before_the_run(build: Path, tmp_path: Path, wrong: list[str]):
    folder = tmp_path / "run"
    arguments = ["--build", str(build), "--output", str(folder), "--instance", "C101_5", *wrong]

    assert cpp_native_runner.main(arguments) == 2
    assert not folder.exists()


@pytest.mark.usefixtures("program")
def test_a_run_runs_again_from_its_files(build: Path, tmp_path: Path):
    first = run(build, tmp_path / "first", "--instance", "C101_10")
    again = run(build, tmp_path / "again", "--file", str(tmp_path / "first" / "run.toml"))

    assert same(again) == same(first)
    assert same(rerun_command(tmp_path / "first")) == same(first)


@pytest.mark.usefixtures("gurobi")
def test_a_gurobi_run_fills_its_folder(build: Path, tmp_path: Path):
    result = run(
        build,
        tmp_path,
        *("--instance", "C101_10", "--master", "gurobi", "--master.parameters", "{ Presolve = 1 }"),
    )
    metadata = run_folder.read_json(tmp_path / run_folder.METADATA_FILE)

    assert result["status"] == "Optimal"
    assert result["solution_check"] == "passed"
    assert metadata["versions"]["gurobi"].count(".") == 2


@pytest.mark.usefixtures("build")
def test_preprocessing_off_is_one_flag():
    given = {"instance": "C101_5", "layer": "cpp-native", "master": "highs"}

    for preprocess, flags in [(None, 0), (True, 0), (False, 1)]:
        table = given if preprocess is None else {**given, "pricing.preprocess": preprocess}
        assert flags_of(table).count("--no-preprocess") == flags


@pytest.mark.usefixtures("build")
def test_the_graph_options_are_flags():
    given = {"instance": "C101_5", "layer": "cpp-native", "master": "highs"}

    for graph, flags in [
        ({}, ""),
        ({"relaxation": "ng", "ng_size": 8}, "--relaxation ng --ng-size 8"),
        ({"relaxation": "elementary"}, "--relaxation elementary"),
    ]:
        arguments = flags_of({**given, "graph": graph})
        assert flags in " ".join(arguments)
        assert ("--relaxation" in arguments) == bool(flags)


@pytest.mark.parametrize(
    ("flags", "message"),
    [
        (["--relaxation", "ng"], "--relaxation ng needs --ng-size, at least 2"),
        (["--relaxation", "ng", "--ng-size", "1"], "--relaxation ng needs --ng-size, at least 2"),
        (["--ng-size", "8"], "--ng-size is for --relaxation ng only"),
        (["--relaxation", "full"], "relaxation 'full' is not none, ng or elementary"),
        (
            ["--stage", "simple", "--stage-param", "bucket_resource", "x"],
            "bucket_resource 'x' is not cost, time or load",
        ),
        (
            ["--stage", "simple", "--stage-param", "bucket_width", "0"],
            "bucket_width must be at least 1",
        ),
        (
            ["--stage", "greedy", "--stage-param", "sort_resource", "time"],
            "a greedy stage keeps no label buckets",
        ),
        (["--stage", "tabu"], "a tabu stage needs a finite max_iterations"),
        (
            ["--stage", "greedy", "--stage-param", "direction", "backward"],
            "a greedy stage searches forward only",
        ),
        (
            ["--stage", "tabu", "--stage-param", "max_iterations", "10"]
            + ["--stage-param", "direction", "bidirectional"],
            "a tabu stage searches forward only",
        ),
    ],
)
def test_the_program_refuses_wrong_flags(program: Path, flags: list[str], message: str):
    instance = str(source_tree.TREE / "instances" / "C101_5.txt")
    code, _, error = cpp_native_runner.execute(
        program, ["--instance", instance, "--master", "highs", *flags, "--check"]
    )

    assert code == 2
    assert message in error


def test_the_flags_write_values_as_the_program_reads_them():
    assert cpp_native_runner.flag_value(True) == "true"
    assert cpp_native_runner.flag_value(float("inf")) == "inf"
    assert cpp_native_runner.flag_value([26, 0]) == "0,26"
    assert cpp_native_runner.flag_value(1e-7) == "1e-07"
    assert [cpp_native_runner.option_kind(value) for value in (True, 1, 1.5, "on")] == [
        "bool",
        "int",
        "double",
        "string",
    ]
