#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The summary, on benchmark folders written by hand: the figures over the finished
rounds, how timeouts, crashes and failed checks count, rounds that disagree, the flags,
the records, ``runs.csv``, and folders of run folders without a benchmark."""

import json
import re
from pathlib import Path

import pytest
import summary
from benchmark_folders import (
    COMMIT,
    RUN,
    metadata,
    options,
    read_csv,
    result,
    round_folder,
    write_benchmark,
    write_round,
    write_rounds,
)
from column_generation_benchmark import run_folder
from column_generation_benchmark.folder import (
    MISSING,
    UNFINISHED,
    UNREADABLE,
    FolderError,
    Measure,
    agreed,
    cell_row,
    measure,
    read_folder,
)
from column_generation_benchmark.summary import write_summary


def summary_text(path: Path) -> str:
    """``summary.md``, its tables' padding squeezed to one space."""
    return re.sub(r" +", " ", (path / "summary.md").read_text())


@pytest.fixture
def three_rounds(tmp_path: Path) -> Path:
    """One run, three finished rounds of 1, 2 and 4 seconds."""
    write_rounds(tmp_path, RUN, [result(solve_seconds=seconds) for seconds in (1.0, 2.0, 4.0)])
    return tmp_path


# MARK: Figures


def test_measure_gives_median_mean_min_max_and_spread():
    assert measure([1.0, 4.0, 1.0, None]) == Measure(
        median=1.0, mean=2.0, min=1.0, max=4.0, spread_percent=300.0
    )
    assert measure([None]) == Measure()
    assert measure([0.0, 0.0]).spread_percent is None


def test_values_agree_within_the_tolerance():
    assert agreed([1.0, 1.0 + 1e-9]) == (True, 1.0)
    assert agreed([1.0, 1.001]) == (False, None)
    assert agreed(["Optimal", "Optimal"]) == (True, "Optimal")
    assert agreed(["Optimal", "TimeLimit"])[0] is False
    assert agreed([None, 1.0])[0] is False
    assert agreed([True, 1])[0] is False
    assert agreed([]) == (True, None)


def test_a_cell_has_the_figures_of_its_rounds(three_rounds: Path):
    [cell] = read_folder(three_rounds).cells
    row = cell_row(cell)

    assert (row["run"], row["layer"], row["instance"], row["master"]) == (
        RUN,
        "package",
        "C101_25",
        "highspy",
    )
    assert (row["rounds"], row["finished"], row["timeouts"], row["not_finished"]) == (3, 3, 0, 0)
    assert row["solve_seconds_median"] == 2.0
    assert (row["solve_seconds_min"], row["solve_seconds_max"]) == (1.0, 4.0)
    assert row["solve_seconds_mean"] == 7.0 / 3
    assert row["solve_seconds_spread_percent"] == 150.0
    assert row["process_peak_memory_bytes_median"] == 64 * 1024 * 1024
    assert row["objective"] == 191.8136 and row["iterations"] == 16
    assert row["seconds_per_iteration"] == 2.0 / 16
    assert row["wall_seconds_median"] == 2.0 and row["processor_seconds_median"] == 1.5
    assert row["other_processor_percent"] == 5.0 and row["on_battery"] is False
    assert row["flags"] == ""


def test_timeouts_and_crashes_count_apart_from_the_figures(tmp_path: Path):
    write_benchmark(tmp_path, {RUN: options()}, rounds=5)
    write_round(round_folder(tmp_path, RUN, 1), result(solve_seconds=1.0), metadata())
    write_round(
        round_folder(tmp_path, RUN, 2),
        run_folder.timeout_result(600.0),
        metadata(outcome=run_folder.TIMEOUT, exit_code=None),
    )
    write_round(
        round_folder(tmp_path, RUN, 3), None, metadata(outcome=run_folder.CRASHED, exit_code=1)
    )
    write_round(round_folder(tmp_path, RUN, 4), None, None)  # cut off
    # Round 5 has no folder.

    folder = read_folder(tmp_path)
    [cell] = folder.cells
    outcomes = [item.outcome for item in cell.rounds]
    assert outcomes == ["finished", run_folder.TIMEOUT, run_folder.CRASHED, UNFINISHED, MISSING]

    row = cell_row(cell)
    assert (row["finished"], row["timeouts"], row["not_finished"]) == (1, 1, 3)
    assert row["solve_seconds_median"] == 1.0
    assert row["flags"] == "1 timeouts; 3 not finished"

    write_summary(tmp_path)
    text = summary_text(tmp_path)
    assert "## Rounds not in the figures" in text
    assert f"| {RUN}-r2 | timeout | - |" in text
    assert f"| {RUN}-r3 | crashed | 1 |" in text
    assert f"| {RUN}-r5 | missing | - |" in text
    assert f"- {RUN}: 1 timeouts; 3 not finished" in text


def test_a_failed_check_is_in_the_figures_and_flagged(tmp_path: Path):
    write_benchmark(tmp_path, {RUN: options()}, rounds=2)
    write_round(round_folder(tmp_path, RUN, 1), result(solve_seconds=1.0), metadata())
    write_round(
        round_folder(tmp_path, RUN, 2),
        result(solve_seconds=3.0, solution_check="failed"),
        metadata(outcome=run_folder.CHECK_FAILED, exit_code=1),
    )

    row = cell_row(read_folder(tmp_path).cells[0])
    assert (row["finished"], row["failed_checks"]) == (2, 1)
    assert row["solve_seconds_median"] == 2.0
    assert row["solution_check"] is None
    assert row["flags"] == "1 failed checks; rounds differ: solution_check"


def test_rounds_that_differ_are_flagged_and_shown(tmp_path: Path):
    write_benchmark(tmp_path, {RUN: options()}, rounds=3)
    write_round(round_folder(tmp_path, RUN, 1), result(iterations=16), metadata())
    write_round(
        round_folder(tmp_path, RUN, 2), result(iterations=18, solve_seconds=0.9), metadata()
    )
    write_round(
        round_folder(tmp_path, RUN, 3), result(iterations=18, status="TimeLimit"), metadata()
    )

    row = cell_row(read_folder(tmp_path).cells[0])
    assert row["iterations"] is None and row["status"] is None
    # Each round's own average: 1 / 16, 0.05 and 1 / 18.
    assert row["seconds_per_iteration"] == 1.0 / 18
    assert row["flags"] == "rounds differ: status, iterations"

    write_summary(tmp_path)
    text = summary_text(tmp_path)
    assert "| Optimal x2, TimeLimit x1 | 191.814 | 16..18 |" in text


def test_fields_a_runner_adds_are_summarised_too(tmp_path: Path):
    write_benchmark(tmp_path, {RUN: options()}, rounds=2)
    write_round(
        round_folder(tmp_path, RUN, 1),
        {"status": "done", "labels": 10, "replay_seconds": 1.0},
        metadata(),
    )
    write_round(
        round_folder(tmp_path, RUN, 2),
        {"status": "done", "labels": 10, "replay_seconds": 3.0},
        metadata(),
    )

    row = cell_row(read_folder(tmp_path).cells[0])
    assert row["labels"] == 10 and row["replay_seconds_median"] == 2.0

    write_summary(tmp_path)
    text = summary_text(tmp_path)
    assert "| run | finished | status | labels |" in text
    assert "| run | replay_seconds median | min | max | spread % |" in text


# MARK: Flags and records


def test_the_header_and_the_last_event(three_rounds: Path):
    events = [{"time": "t0", "event": "started"}, {"time": "t1", "event": "stopped"}]
    write_benchmark(three_rounds, {RUN: options()}, rounds=3, events=events)
    # The benchmark's own record gives the solvers no round recorded.
    folder_metadata = run_folder.read_json(three_rounds / run_folder.METADATA_FILE)
    folder_metadata["record"] = {"versions": {"gurobipy": "13.0.1", "gurobi": "13.0.1"}}
    run_folder.write_json(three_rounds / run_folder.METADATA_FILE, folder_metadata)
    write_summary(three_rounds)
    text = summary_text(three_rounds)

    assert text.startswith(f"# Summary of {three_rounds.name}\n")
    assert "- 1 runs, 3 rounds, 1 processes" in text
    assert "- 3 / 3 rounds finished (0 failed checks), 0 timeouts, 0 not finished" in text
    assert "- last event: stopped at t1" in text
    assert "## Flags\n\nNone." in text
    assert f"- source.commit: {COMMIT}" in text
    assert "- versions: python 3.14.0, rcspp 0.0.2, numpy 2.5.3" in text
    assert "- solvers: highspy 1.15.1, highs 1.15.1, gurobipy 13.0.1, gurobi 13.0.1" in text


def test_many_processes_and_battery_are_flagged(tmp_path: Path):
    write_benchmark(tmp_path, {RUN: options()}, rounds=1, processes=2)
    load = {"other_processor_percent_mean": 50.0, "on_battery": True}
    write_round(round_folder(tmp_path, RUN, 1), result(), metadata(load=load))
    write_summary(tmp_path)
    text = summary_text(tmp_path)

    assert "- 2 runs at a time: each run shared the machine" in text
    assert "- 1 rounds ran on battery" in text
    # Said once, for the folder.
    assert f"- {RUN}: on battery" not in text


def test_mixed_records_are_listed_with_their_rounds(tmp_path: Path):
    write_benchmark(tmp_path, {RUN: options()}, rounds=3)
    write_round(round_folder(tmp_path, RUN, 1), result(), metadata())
    write_round(round_folder(tmp_path, RUN, 2), result(), metadata())
    source = {"commit": "b" * 40, "uncommitted_changes": True, "branch": "main"}
    write_round(round_folder(tmp_path, RUN, 3), result(), metadata(source=source))
    write_summary(tmp_path)
    text = summary_text(tmp_path)

    assert "- mixed: 2 records (below)" in text
    assert "- 2 rounds have it" in text
    assert f"Record 2, 1 rounds: source.commit: '{COMMIT}' -> '{'b' * 40}'; " in text
    assert "source.uncommitted_changes: False -> True" in text
    assert f"- {RUN}-r3" in text


def test_layers_must_share_their_solvers_versions(tmp_path: Path):
    bindings = RUN.replace("package", "cpp-bindings")
    native = RUN.replace("package-highspy", "cpp-bindings-highs")
    runs = {
        RUN: options(),
        bindings: options(layer="cpp-bindings"),
        native: options(layer="cpp-bindings", master="highs"),
    }
    write_benchmark(tmp_path, runs, rounds=1)
    write_round(round_folder(tmp_path, RUN, 1), result(), metadata())
    older = metadata()
    older["versions"].update(highspy="1.14.0", highs="1.14.0")
    write_round(round_folder(tmp_path, bindings, 1), result(), older)
    write_round(round_folder(tmp_path, native, 1), result(), metadata(master="highs"))
    write_summary(tmp_path)
    text = summary_text(tmp_path)

    assert "- highspy versions differ: 1.15.1 (1 rounds), 1.14.0 (1 rounds)" in text
    assert "- highs versions differ: 1.15.1 (1 rounds), 1.14.0 (1 rounds)" in text
    assert "- solvers: highspy 1.15.1 / 1.14.0, highs 1.15.1 / 1.14.0" in text
    assert "- solver version not recorded: cpp-bindings with master highs (1 rounds)" in text
    # The solvers' versions are not part of the record every round shares.
    assert "mixed" not in text


# MARK: Files


def test_the_three_files(three_rounds: Path):
    written = write_summary(three_rounds)
    assert [path.name for path in written] == ["summary.md", "summary.csv", "runs.csv"]

    [cell] = read_csv(three_rounds / "summary.csv")
    assert cell["run"] == RUN and cell["solve_seconds_median"] == "2.0"
    assert cell["lp_solved_again_seconds_median"] == "" and cell["flags"] == ""

    rounds = read_csv(three_rounds / "runs.csv")
    assert [line["round"] for line in rounds] == ["1", "2", "3"]
    first = rounds[0]
    assert first["folder"] == f"runs/{RUN}-r1" and first["outcome"] == "finished"
    assert first["instance.name"] == "C101_25" and first["master.type"] == "highspy"
    assert json.loads(first["pricing.stages"]) == [{"algorithm": "simple"}]
    assert first["solve_seconds"] == "1.0" and first["wall_seconds"] == "2.0"
    assert first["load.other_processor_percent_mean"] == "5.0"
    assert first["benchmark.processes"] == "1" and first["source.commit"] == COMMIT
    assert first["versions.highs"] == "1.15.1"
    assert "load" not in first


def test_run_folders_without_a_benchmark(tmp_path: Path):
    other = "R201_25-package-gurobipy-exact-artificial"
    for name, number in ((RUN, 1), (RUN, 2), (other, 1)):
        run_options = options(master="gurobipy", instance="R201_25") if name == other else options()
        master = "gurobipy" if name == other else "highspy"
        write_round(
            tmp_path / f"{name}-r{number}",
            result(),
            metadata(outcome=None, master=master),
            run_options,
        )

    (tmp_path / "notes").mkdir()
    folder = read_folder(tmp_path)

    assert [cell.run for cell in folder.cells] == [RUN, other]
    assert [len(cell.rounds) for cell in folder.cells] == [2, 1]
    assert folder.cells[1].options["master"]["type"] == "gurobipy"
    assert {item.outcome for item in folder.rounds} == {"finished"}

    # A runner's folder alone: a timeout or a failed check from its result.
    write_round(tmp_path / "alone", result(solution_check="failed"), None, options())
    [cell] = read_folder(tmp_path / "alone").cells
    assert cell.run == "alone" and cell.rounds[0].outcome == run_folder.CHECK_FAILED
    write_round(tmp_path / "killed", run_folder.timeout_result(5.0), None, options())
    assert read_folder(tmp_path / "killed").rounds[0].outcome == run_folder.TIMEOUT

    with pytest.raises(FolderError, match="holds no run"):
        read_folder(tmp_path / "notes")


def test_the_script(three_rounds: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]):
    assert summary.main([str(three_rounds)]) == 0
    assert capsys.readouterr().out.splitlines()[0].endswith("summary.md")

    (tmp_path / "empty").mkdir()
    assert summary.main([str(tmp_path / "empty")]) == 2
    assert "holds no run" in capsys.readouterr().err
    assert summary.main([str(tmp_path / "absent")]) == 2
    assert "is not a folder" in capsys.readouterr().err


# MARK: Odd folders


def test_rounds_of_one_name_with_other_options_are_flagged(tmp_path: Path):
    write_round(tmp_path / f"{RUN}-r1", result(), metadata(outcome=None), options())
    other = options(master="gurobipy")
    write_round(tmp_path / f"{RUN}-r2", result(), metadata(outcome=None), other)

    row = cell_row(read_folder(tmp_path).cells[0])
    assert "options differ between rounds" in row["flags"]


def test_the_columns_keep_their_groups_whatever_the_first_run(tmp_path: Path):
    other = "R201_25-package-highspy-exact-artificial"
    write_benchmark(tmp_path, {RUN: options(), other: options(instance="R201_25")}, rounds=1)
    write_round(round_folder(tmp_path, RUN, 1), result(), metadata())
    write_round(round_folder(tmp_path, other, 1), result(labels=3), metadata())
    write_summary(tmp_path)

    columns = list(read_csv(tmp_path / "summary.csv")[0])
    assert columns.index("labels") < columns.index("seconds_per_iteration") < columns.index("flags")
    assert columns[-1] == "flags"

    columns = list(read_csv(tmp_path / "runs.csv")[0])
    assert columns.index("labels") < columns.index("exit_code")
    assert columns.index("graph") < columns.index("status")


def test_a_result_field_never_hides_a_column_of_the_summary(tmp_path: Path):
    write_rounds(tmp_path, RUN, [result(run="other", outcome="odd", rounds=7)])
    row = cell_row(read_folder(tmp_path).cells[0])

    assert (row["run"], row["rounds"]) == (RUN, 1)
    assert (row["result.run"], row["result.rounds"]) == ("other", 7)

    write_summary(tmp_path)
    [line] = read_csv(tmp_path / "runs.csv")
    assert (line["outcome"], line["result.outcome"]) == ("finished", "odd")


def test_an_outcome_without_its_result_is_not_finished(tmp_path: Path):
    write_benchmark(tmp_path, {RUN: options()}, rounds=2)
    write_round(round_folder(tmp_path, RUN, 1), result(), metadata())
    write_round(round_folder(tmp_path, RUN, 2), None, metadata())

    row = cell_row(read_folder(tmp_path).cells[0])
    assert (row["finished"], row["not_finished"]) == (1, 1)


@pytest.mark.parametrize("outcome", [run_folder.INTERRUPTED, run_folder.NOT_STARTED])
def test_interrupted_and_not_started_rounds_are_not_finished(tmp_path: Path, outcome: str):
    write_benchmark(tmp_path, {RUN: options()}, rounds=2)
    write_round(round_folder(tmp_path, RUN, 1), result(), metadata())
    write_round(round_folder(tmp_path, RUN, 2), None, metadata(outcome=outcome))

    row = cell_row(read_folder(tmp_path).cells[0])
    assert (row["finished"], row["not_finished"]) == (1, 1)


def test_a_bar_in_a_value_stays_in_its_cell(tmp_path: Path):
    write_rounds(tmp_path, RUN, [result(status="a|b\nc")])
    write_summary(tmp_path)

    assert "| a\\|b c |" in summary_text(tmp_path)


def test_an_unreadable_round_is_said_and_an_unreadable_benchmark_refused(tmp_path: Path):
    write_rounds(tmp_path, RUN, [result(), result()])
    (round_folder(tmp_path, RUN, 2) / run_folder.RESULT_FILE).write_text("{ not json")

    assert [item.outcome for item in read_folder(tmp_path).rounds] == ["finished", UNREADABLE]

    (tmp_path / run_folder.BENCHMARK_FILE).write_text("[runs\n")

    with pytest.raises(FolderError, match="benchmark.toml"):
        read_folder(tmp_path)


def test_a_benchmark_without_runs_holds_no_run(tmp_path: Path):
    write_benchmark(tmp_path, {}, rounds=1)

    with pytest.raises(FolderError, match="holds no run"):
        read_folder(tmp_path)


def test_rounds_beyond_the_count_and_without_metadata_are_read(tmp_path: Path):
    write_rounds(tmp_path, RUN, [result(), result()])
    # A resume with fewer rounds leaves the third; without the folder's metadata, the
    # benchmark file's repeats count.
    write_round(round_folder(tmp_path, RUN, 3), result(), metadata(), options())
    (tmp_path / run_folder.METADATA_FILE).unlink()

    assert [item.number for item in read_folder(tmp_path).rounds] == [1, 2, 3]


def test_flags_with_commas_read_back_from_the_csv(tmp_path: Path):
    write_benchmark(tmp_path, {RUN: options()}, rounds=2)
    write_round(round_folder(tmp_path, RUN, 1), result(iterations=1), metadata())
    write_round(round_folder(tmp_path, RUN, 2), result(iterations=2, status="x"), metadata())
    write_summary(tmp_path)

    [line] = read_csv(tmp_path / "summary.csv")
    assert line["flags"] == "rounds differ: status, iterations"
