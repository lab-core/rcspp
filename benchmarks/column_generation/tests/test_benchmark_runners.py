#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The runners: a run fills its folder, refuses one that holds a run, writes the LP solution,
solves the integer master and checks its routes, and runs the same again from its ``run.toml``
and from its ``command.txt``; every layer gives the same column generation. The
``cpp-bindings`` runner's tests skip when the build has no column generation example."""

import shlex
import subprocess  # nosec B404: the runner's own recorded command
import sys
from pathlib import Path
from typing import Any

import cpp_bindings_runner
import package_runner
import pytest
from column_generation_benchmark import options, record, run_folder, runner, source_tree
from column_generation_benchmark.instances import SolomonInstance

#: The runner of each layer.
RUNNERS = {"package": package_runner, "cpp-bindings": cpp_bindings_runner}

#: What must come out the same when a run runs again on the same layer.
SAME = (
    "status",
    "objective",
    "iterations",
    "columns",
    "artificial_columns",
    "integer_status",
    "integer_objective",
    "solution_check",
)

#: What every layer gives the same: the column generation. The integer master's result can
#: differ, as its solver may take another of several optimal solutions.
SAME_COLUMN_GENERATION = SAME[:5]

#: The integer solve's fields of ``result.json``: null when it is off.
INTEGER_FIELDS = ("integer_status", "integer_objective", "integer_seconds", "gap", "solution_check")


def has_example() -> bool:
    try:
        source_tree.use_example()
    except source_tree.SourceTreeError:
        return False

    return True


def needs_example(layer: str) -> None:
    if layer == "cpp-bindings" and not has_example():
        pytest.skip("the build has no column generation example")


@pytest.fixture(params=list(RUNNERS))
def layer(request: pytest.FixtureRequest, build: Path) -> str:
    """Each layer whose runner the build can run."""
    needs_example(request.param)
    return request.param


def run(layer: str, build: Path, folder: Path, *arguments: str) -> dict[str, Any]:
    """The result of a run of `layer`'s runner into `folder`."""
    main = RUNNERS[layer].main
    assert main(["--build", str(build), "--output", str(folder), *arguments]) == 0
    result = run_folder.read_result(folder)
    assert result is not None
    return result


def same(result: dict[str, Any], names: tuple[str, ...] = SAME) -> dict[str, Any]:
    return {name: result[name] for name in names}


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


def test_a_run_fills_its_folder(layer: str, build: Path, tmp_path: Path):
    result = run(layer, build, tmp_path, "--instance", "C101_5", "--machine", "test")

    assert run_folder.result_problems(result) == []
    assert result["status"] == "Optimal"
    assert result["iterations"] > 0 and result["columns"] > 0
    assert "process_peak_memory_bytes" in result

    metadata = run_folder.read_json(tmp_path / run_folder.METADATA_FILE)
    assert {"start", "end", *record.RECORD_GROUPS} <= set(metadata)
    assert metadata["machine"]["label"] == "test"
    assert "highspy" in metadata["versions"]

    expected = options.expand_run({"instance": "C101_5", "layer": layer})
    assert run_folder.read_run_file(tmp_path) == expected.to_table()
    assert "--instance C101_5" in (tmp_path / run_folder.COMMAND_FILE).read_text()


def test_a_run_goes_to_the_results_folder_by_default(
    layer: str, build: Path, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
):
    monkeypatch.setattr(runner, "RESULTS", tmp_path)

    assert RUNNERS[layer].main(["--build", str(build), "--instance", "C101_5"]) == 0
    assert run_folder.read_result(tmp_path / f"C101_5-{layer}-highspy-exact-artificial")


# HiGHS keeps the thread count of the first solve in a process: the tests' process has solved
# before, a runner's own process has not.
@pytest.mark.filterwarnings("ignore:HighspyMasterSolver. HiGHS keeps the thread count")
@pytest.mark.parametrize(
    "arguments",
    [
        ("--start", "single-customer"),
        ("--stage", "tabu:max_solutions=5,max_iterations=100", "--stage", "simple"),
        ("--column_generation.max_iterations", "3"),
        ("--column_generation.threads", "1"),
        (
            "--column_generation.tolerances",
            "relative",
            "--master.parameters",
            "{ simplex_strategy = 1 }",
        ),
    ],
)
def test_the_run_options_reach_the_layer(
    layer: str, build: Path, tmp_path: Path, arguments: tuple[str]
):
    result = run(layer, build, tmp_path, "--instance", "C101_5", *arguments)

    assert run_folder.result_problems(result) == []

    if "--column_generation.threads" in arguments:
        metadata = run_folder.read_json(tmp_path / run_folder.METADATA_FILE)
        assert metadata["benchmark"]["solver_threads"] == 1

    if "--column_generation.max_iterations" in arguments:
        assert (result["status"], result["iterations"]) == ("IterationLimit", 3)
    else:
        assert result["status"] == "Optimal"


def test_a_build_check_builds_without_writing(
    layer: str, build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    arguments = ["--build", str(build), "--instance", "C101_5", "--check", "build"]

    assert RUNNERS[layer].main(arguments) == 0
    assert capsys.readouterr().out.strip() == f"C101_5-{layer}-highspy-exact-artificial: built"
    assert list(tmp_path.iterdir()) == []


@pytest.mark.parametrize(
    ("arguments", "message"),
    [
        (["--instance", "MISSING"], "MISSING"),
        (["--instance", "C101_5", "--master.parameters", "{ nope = 1 }"], "highspy refuses"),
        (["--instance", "C101_5", "--stage", "simple:nope=1"], "nope"),
    ],
)
def test_wrong_options_stop_before_the_run(
    layer: str,
    build: Path,
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
    arguments: list[str],
    message: str,
):
    folder = tmp_path / "run"

    assert RUNNERS[layer].main(["--build", str(build), "--output", str(folder), *arguments]) == 2
    assert message in capsys.readouterr().err
    assert not folder.exists()


def test_a_run_of_another_layer_is_refused(
    layer: str, build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    other = next(name for name in RUNNERS if name != layer)
    arguments = ["--build", str(build), "--output", str(tmp_path / "run"), "--instance", "C101_5"]

    assert RUNNERS[layer].main([*arguments, "--layer", other]) == 2
    assert f"runs layer {layer}" in capsys.readouterr().err

    path = tmp_path / "options.toml"
    path.write_text(f'instance = "C101_5"\nlayer = "{other}"\n')

    assert RUNNERS[layer].main([*arguments, "--file", str(path)]) == 2
    assert f"runs layer {layer}" in capsys.readouterr().err


# MARK: Collisions


def test_a_folder_that_holds_a_run_is_refused(
    layer: str, build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    run_folder.write_json(tmp_path / run_folder.METADATA_FILE, {"exit_code": 1})
    arguments = ["--build", str(build), "--output", str(tmp_path), "--instance", "C101_5"]

    assert RUNNERS[layer].main(arguments) == 2
    assert "--overwrite" in capsys.readouterr().err
    assert [path.name for path in tmp_path.iterdir()] == [run_folder.METADATA_FILE]


def test_overwrite_removes_the_folder_first(layer: str, build: Path, tmp_path: Path):
    run_folder.write_json(tmp_path / run_folder.METADATA_FILE, {"exit_code": 1})
    (tmp_path / "notes.txt").write_text("old")
    run(
        layer,
        build,
        tmp_path,
        *("--instance", "C101_5", "--statistics.process", "false", "--overwrite"),
    )

    assert "exit_code" not in run_folder.read_json(tmp_path / run_folder.METADATA_FILE)
    assert "process_peak_memory_bytes" not in (run_folder.read_result(tmp_path) or {})
    assert not (tmp_path / "notes.txt").exists()


def test_a_folder_without_run_files_is_kept(layer: str, build: Path, tmp_path: Path):
    (tmp_path / "log.txt").write_text("a benchmark's log")
    run(layer, build, tmp_path, "--instance", "C101_5")

    assert (tmp_path / "log.txt").read_text() == "a benchmark's log"


# MARK: Solution


@pytest.mark.parametrize("start", ["artificial", "single-customer"])
def test_the_solution_covers_every_customer_once(
    layer: str, build: Path, tmp_path: Path, start: str
):
    result = run(layer, build, tmp_path, "--instance", "C101_10", "--start", start)
    lp = run_folder.read_json(tmp_path / run_folder.SOLUTION_FILE)["lp"]

    assert lp["objective"] == result["objective"]
    assert not lp["solved_again"] and result["lp_solved_again_seconds"] is None
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


def test_the_solution_can_be_left_out(layer: str, build: Path, tmp_path: Path):
    result = run(layer, build, tmp_path, "--instance", "C101_5", "--statistics.solution", "false")

    assert "solution_columns" not in result
    assert not (tmp_path / run_folder.SOLUTION_FILE).exists()


def test_artificial_columns_in_use_are_listed_apart(layer: str, build: Path, tmp_path: Path):
    # Artificial columns cheaper than any route: the LP keeps them.
    result = run(layer, build, tmp_path, "--instance", "C101_10", "--start.cost_factor", "1e-4")
    lp = run_folder.read_json(tmp_path / run_folder.SOLUTION_FILE)["lp"]

    assert result["status"] == "Infeasible"
    assert lp["artificial_columns"]
    assert all(set(column) == {"id", "value"} for column in lp["artificial_columns"])


def test_a_run_stopped_after_pricing_solves_the_lp_again(layer: str, build: Path, tmp_path: Path):
    # Pricing added columns after the last LP: the master solvers dropped its values.
    result = run(
        layer, build, tmp_path, "--instance", "C101_10", "--column_generation.max_iterations", "1"
    )
    lp = run_folder.read_json(tmp_path / run_folder.SOLUTION_FILE)["lp"]

    assert result["status"] == "IterationLimit"
    assert lp["solved_again"] and result["lp_solved_again_seconds"] >= 0
    assert result["solution_columns"] == len(lp["columns"])
    # Over more columns than the loop's last LP: never worse.
    assert lp["objective"] <= result["objective"] + 1e-9
    assert len(lp["duals"]) == 11
    # The loop's LP is no lower bound: no gap.
    assert result["integer_status"] == "Optimal" and result["gap"] is None


def test_a_run_without_an_lp_has_no_objective(layer: str, build: Path, tmp_path: Path):
    result = run(
        layer, build, tmp_path, "--instance", "C101_5", "--column_generation.max_iterations", "0"
    )
    solution = run_folder.read_json(tmp_path / run_folder.SOLUTION_FILE)

    assert (result["status"], result["iterations"], result["objective"]) == (
        "IterationLimit",
        0,
        None,
    )
    assert (solution["lp"], solution["lp_missing"]) == (None, "no master LP was solved")
    assert result["lp_solved_again_seconds"] is None

    # The start's artificial columns are all the master has: the integer solve takes them.
    assert solution["integer"]["artificial_columns"]
    assert solution["resolved"] is None
    assert solution["resolved_missing"] == "the integer solution uses artificial columns"
    assert result["solution_check"] is None


# MARK: Integer solve


@pytest.mark.parametrize("column_type", ["binary", "integer"])
def test_the_integer_solve_gives_routes_that_pass_the_check(
    layer: str, build: Path, tmp_path: Path, column_type: str
):
    arguments = ("--instance", "C101_10", "--integer.column_type", column_type)
    result = run(layer, build, tmp_path, *arguments)
    solution = run_folder.read_json(tmp_path / run_folder.SOLUTION_FILE)
    integer, resolved = solution["integer"], solution["resolved"]
    objective = result["integer_objective"]

    assert (result["integer_status"], integer["status"]) == ("Optimal", "Optimal")
    assert integer["objective"] == objective
    assert objective >= result["objective"] - 1e-6
    assert result["gap"] == pytest.approx((objective - result["objective"]) / objective)
    assert result["integer_seconds"] > 0
    assert integer["artificial_columns"] == []
    assert [round(column["value"]) for column in integer["columns"]] == [1] * len(
        resolved["routes"]
    )

    assert result["solution_check"] == "passed" and resolved["errors"] == []
    assert sorted(customer for route in resolved["routes"] for customer in route) == list(
        range(1, 11)
    )
    assert resolved["vehicles"] == len(resolved["routes"])
    assert resolved["distance"] == pytest.approx(objective)


def test_the_integer_solve_can_be_left_out(layer: str, build: Path, tmp_path: Path):
    result = run(layer, build, tmp_path, "--instance", "C101_10", "--integer.solve", "false")
    solution = run_folder.read_json(tmp_path / run_folder.SOLUTION_FILE)

    assert run_folder.result_problems(result) == []
    assert [result[name] for name in INTEGER_FIELDS] == [None] * len(INTEGER_FIELDS)
    assert (solution["integer"], solution["resolved"]) == (None, None)
    assert solution["resolved_missing"] == "no integer solve"


def test_the_check_runs_without_the_solution_file(layer: str, build: Path, tmp_path: Path):
    result = run(layer, build, tmp_path, "--instance", "C101_10", "--statistics.solution", "false")

    assert result["solution_check"] == "passed"
    assert not (tmp_path / run_folder.SOLUTION_FILE).exists()


def test_a_failed_check_is_an_error_status(
    layer: str,
    build: Path,
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
):
    def broken(self: SolomonInstance, solution: dict[str, Any], objective: float) -> list[str]:
        return ["customer 3 is served 2 times"]

    monkeypatch.setattr(SolomonInstance, "check_solution", broken)
    arguments = ["--build", str(build), "--output", str(tmp_path), "--instance", "C101_10"]

    # The folder is written all the same: the run finished, its solution is wrong.
    assert RUNNERS[layer].main(arguments) == 1
    assert "fails the instance's check: customer 3 is served 2 times" in capsys.readouterr().err
    assert (run_folder.read_result(tmp_path) or {})["solution_check"] == "failed"
    resolved = run_folder.read_json(tmp_path / run_folder.SOLUTION_FILE)["resolved"]
    assert resolved["errors"] == ["customer 3 is served 2 times"]


# MARK: Runs again


def test_a_run_from_the_command_line_runs_again(layer: str, build: Path, tmp_path: Path):
    first = run(
        layer, build, tmp_path / "first", "--instance", "C101_5", "--start", "single-customer"
    )
    again = run(layer, build, tmp_path / "again", "--file", str(tmp_path / "first" / "run.toml"))

    assert same(again) == same(first)
    assert same(rerun_command(tmp_path / "first")) == same(first)


def test_a_run_from_a_run_file_runs_again(layer: str, build: Path, tmp_path: Path):
    path = tmp_path / "options.toml"
    path.write_text(
        'instance = "C101_5"\n'
        'pricing = { stages = [{ algorithm = "tabu", max_solutions = 5, max_iterations = 100 }, '
        '{ algorithm = "simple" }] }\n'
        '"start.cost_factor" = 1e3\n'
    )
    first = run(
        layer,
        build,
        tmp_path / "first",
        *("--file", str(path), "--master.parameters", "{ simplex_strategy = 1 }"),
    )
    again = run(layer, build, tmp_path / "again", "--file", str(tmp_path / "first" / "run.toml"))

    assert run_folder.read_run_file(tmp_path / "again") == run_folder.read_run_file(
        tmp_path / "first"
    )
    assert same(again) == same(first)
    assert same(rerun_command(tmp_path / "first")) == same(first)


# MARK: The C++ example's masters


@pytest.fixture
def example(build: Path) -> None:
    needs_example("cpp-bindings")


@pytest.mark.usefixtures("example")
@pytest.mark.parametrize("master", ["highs", "highspy"])
def test_each_cpp_master_runs(build: Path, tmp_path: Path, master: str):
    result = run("cpp-bindings", build, tmp_path, "--instance", "C101_5", "--master", master)
    metadata = run_folder.read_json(tmp_path / run_folder.METADATA_FILE)

    assert result["status"] == "Optimal"
    assert run_folder.read_run_file(tmp_path)["master"]["type"] == master
    assert "highs" in metadata["versions"]


@pytest.mark.usefixtures("example")
def test_the_cpp_highs_master_takes_parameters(
    build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    arguments = ["--instance", "C101_5", "--master", "highs"]
    result = run(
        "cpp-bindings",
        build,
        tmp_path,
        *arguments,
        "--master.parameters",
        "{ simplex_strategy = 1 }",
    )

    assert result["status"] == "Optimal"

    wrong = ["--build", str(build), "--output", str(tmp_path / "wrong"), *arguments]

    assert cpp_bindings_runner.main([*wrong, "--master.parameters", "{ nope = 1 }"]) == 2
    assert "highs refuses nope = 1" in capsys.readouterr().err


# MARK: Drift


@pytest.mark.parametrize("start", ["artificial", "single-customer"])
def test_every_layer_gives_the_same_run(build: Path, tmp_path: Path, start: str):
    """Every runner the build has, over HiGHS, gives the same objective, iterations and
    columns: the layers do the same column generation."""
    runs = [("package", "highspy")]

    if has_example():
        runs += [("cpp-bindings", "highspy"), ("cpp-bindings", "highs")]

    results = {
        (layer, master): run(
            layer,
            build,
            tmp_path / f"{layer}-{master}",
            *("--instance", "C101_25", "--master", master, "--start", start),
        )
        for layer, master in runs
    }
    first = results[runs[0]]

    for result in results.values():
        assert result["status"] == "Optimal"
        assert result["objective"] == pytest.approx(first["objective"], rel=1e-9)
        assert (result["iterations"], result["columns"]) == (first["iterations"], first["columns"])


def test_a_package_run_runs_on_the_cpp_bindings(build: Path, tmp_path: Path):
    """A run file of one layer runs on another with ``--layer``, to the same result."""
    needs_example("cpp-bindings")
    first = run("package", build, tmp_path / "package", "--instance", "C101_10")
    again = run(
        "cpp-bindings",
        build,
        tmp_path / "cpp-bindings",
        *("--file", str(tmp_path / "package" / "run.toml"), "--layer", "cpp-bindings"),
    )

    assert run_folder.read_run_file(tmp_path / "cpp-bindings")["layer"] == "cpp-bindings"
    assert same(again, SAME_COLUMN_GENERATION) == same(first, SAME_COLUMN_GENERATION)
