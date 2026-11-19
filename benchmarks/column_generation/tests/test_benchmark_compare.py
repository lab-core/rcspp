#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The comparison of two runs, on benchmark folders written by hand: how a side is
named, the fields side by side, the spread, the time rule, the records, and the
script."""

from pathlib import Path
from typing import Any

import compare
import pytest
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
from column_generation_benchmark.compare import (
    EQUIVALENT,
    NO_VERDICT,
    SLOWER,
    CompareError,
    Comparison,
    TimeRule,
)
from column_generation_benchmark.compare import compare as compare_runs
from column_generation_benchmark.compare import (
    comparison_text,
    read_side,
)
from column_generation_benchmark.summary import SummaryError

OTHER = RUN.replace("package", "cpp-bindings")


def write_pair(
    path: Path,
    results_a: list[dict[str, Any]],
    results_b: list[dict[str, Any]],
    options_b: dict[str, Any] | None = None,
) -> None:
    """A benchmark of two runs, A the package and B the bindings, a finished round per
    result."""
    runs = {RUN: options(), OTHER: options_b or options(layer="cpp-bindings")}
    write_benchmark(path, runs, rounds=max(len(results_a), len(results_b)))

    for name, results in ((RUN, results_a), (OTHER, results_b)):
        for number, found in enumerate(results, start=1):
            write_round(round_folder(path, name, number), found, metadata())


def pair(path: Path, rule: TimeRule | None = None) -> Comparison:
    return compare_runs(f"{path}:{RUN}", f"{path}:{OTHER}", rule)


def row(comparison: Comparison, field: str) -> dict[str, Any]:
    return next(found for found in comparison.rows if found["field"] == field)


def times(*values: float) -> list[dict[str, Any]]:
    return [result(solve_seconds=value) for value in values]


# MARK: Naming a side


def test_a_side_is_a_run_of_a_benchmark_by_its_name_or_a_prefix(tmp_path: Path):
    write_pair(tmp_path, times(1.0, 2.0), times(1.0))

    side = read_side(f"{tmp_path}:{RUN}")
    assert side.name == f"{tmp_path.name} / {RUN}" and len(side.cell.rounds) == 2
    # The folder is an absolute path: on Windows, its drive letter has a colon too.
    assert read_side(f"{tmp_path}:C101_25-cpp").cell.run == OTHER

    with pytest.raises(CompareError, match="names several runs") as error:
        read_side(f"{tmp_path}:C101_25")

    assert RUN in str(error.value) and OTHER in str(error.value)

    with pytest.raises(CompareError, match="names no run"):
        read_side(f"{tmp_path}:R201")

    with pytest.raises(CompareError, match="holds 2 runs; name one as"):
        read_side(str(tmp_path))

    with pytest.raises(CompareError, match="neither a folder nor"):
        read_side(str(tmp_path / "absent"))

    with pytest.raises(CompareError, match="neither a folder nor"):
        read_side(f"{tmp_path}:{run_folder.RUNS_FOLDER}/{RUN}-r1")


def test_a_side_is_a_round_a_run_folder_or_a_benchmark_of_one_run(tmp_path: Path):
    write_rounds(tmp_path / "one", RUN, times(1.0, 3.0))

    side = read_side(str(tmp_path / "one"))
    assert side.name == f"one / {RUN}" and len(side.cell.counted) == 2

    side = read_side(str(round_folder(tmp_path / "one", RUN, 2)))
    assert side.name == f"{RUN}-r2" and side.cell.counted == [result(solve_seconds=3.0)]

    write_round(tmp_path / "alone", result(), None, options())
    assert read_side(str(tmp_path / "alone")).name == "alone"

    (tmp_path / "empty").mkdir()

    with pytest.raises(SummaryError, match="holds no run"):
        read_side(str(tmp_path / "empty"))


# MARK: Fields


def test_equal_runs_side_by_side(tmp_path: Path):
    write_pair(tmp_path, times(1.0, 1.2), times(1.1, 1.3))
    comparison = pair(tmp_path)

    assert comparison.options == ["layer: package -> cpp-bindings"]
    assert comparison.records == [] and comparison.warnings == []
    assert comparison.verdict is None
    assert row(comparison, "status")["judgement"] == "equal"
    assert row(comparison, "objective")["judgement"] == "equal"

    solve = row(comparison, "solve_seconds")
    assert (solve["a"], solve["b"]) == pytest.approx((1.1, 1.2))
    assert (solve["a_min"], solve["a_max"], solve["b_min"], solve["b_max"]) == (1.0, 1.2, 1.1, 1.3)
    assert solve["difference"] == pytest.approx(-0.1)
    assert solve["percent"] == pytest.approx(-100 / 12)
    assert solve["judgement"] == "within spread"

    per_iteration = row(comparison, "seconds_per_iteration")
    assert per_iteration["difference"] == pytest.approx(-0.1 / 16)

    text = comparison_text(comparison)
    assert f"- A: {tmp_path.name} / {RUN}" in text and "2 / 2 rounds finished" in text
    assert "- Options that differ: layer: package -> cpp-bindings" in text
    assert "- Records that differ: none" in text
    assert "| field " in text and "| A - B " in text
    solve_line = "| 1.100 [1.000..1.200] | 1.200 [1.100..1.300] | -0.100 (-8.3 %), within spread |"
    assert solve_line in text
    assert "| master_seconds            | 0.040" in text
    assert "| per iteration             | 68.75 ms" in text and "-6.3 ms (-8.3 %)" in text
    assert "| 64.0 MiB" in text and "+0.0 MiB (+0.0 %)" in text
    assert "Verdict" not in text


def test_fields_that_differ_and_rounds_that_disagree(tmp_path: Path):
    write_pair(
        tmp_path,
        [result(iterations=18, solve_seconds=3.0, gap=0.25)] * 2,
        [result(columns=600), result(columns=601)],
    )
    comparison = pair(tmp_path)

    iterations = row(comparison, "iterations")
    assert iterations["judgement"] == "differs"
    assert (iterations["difference"], iterations["percent"]) == (2, 12.5)
    columns = row(comparison, "columns")
    assert columns["judgement"] == "B's rounds differ" and columns["b"] == "600..601"
    assert row(comparison, "solve_seconds")["judgement"] == "beyond spread"
    assert row(comparison, "lp_solved_again_seconds")["judgement"] is None
    assert comparison.warnings == ["B: rounds differ: columns"]

    text = comparison_text(comparison)
    assert "| differs: +2 (+12.5 %)" in text and "| B's rounds differ" in text
    # The gap is 0 in B: a difference without a percent.
    assert "| differs: +0.25 " in text
    assert "| 600..601 " in text
    # Other measures show their difference, not their spread.
    assert "| +0.000 (+0.0 %) " in text and "+2.000 (+200.0 %), beyond spread |" in text


def test_one_round_has_no_spread(tmp_path: Path):
    write_pair(tmp_path, times(1.0), times(1.0, 2.0))
    comparison = pair(tmp_path)

    assert row(comparison, "solve_seconds")["judgement"] == "1 round, no spread"
    assert "| 1.000 " in comparison_text(comparison)


def test_runners_with_other_fields_and_options(tmp_path: Path):
    other_options = options(layer="cpp-bindings")
    other_options["graph"] = {"ng_size": 8}
    first = result(labels=10)
    del first["gap"]
    second = result(replay_seconds=2.0)
    write_pair(tmp_path, [first], [second], other_options)
    comparison = pair(tmp_path)

    assert comparison.options == [
        "graph: {} -> -",
        "graph.ng_size: - -> 8",
        "layer: package -> cpp-bindings",
    ]
    fields = [found["field"] for found in comparison.rows]
    assert fields.index("labels") < fields.index("gap") < fields.index("replay_seconds")
    assert row(comparison, "labels")["judgement"] == "only in A"
    assert row(comparison, "gap")["judgement"] == "only in B"
    replay = row(comparison, "replay_seconds")
    assert (replay["a"], replay["b"], replay["judgement"]) == (None, 2.0, "only in B")

    text = comparison_text(comparison)
    assert "| labels                    | 10" in text and "| only in A" in text
    assert "| replay_seconds            | -" in text


# MARK: Rounds and records


def test_rounds_not_finished_are_said(tmp_path: Path):
    write_benchmark(tmp_path, {RUN: options(), OTHER: options(layer="cpp-bindings")}, rounds=3)
    write_round(round_folder(tmp_path, RUN, 1), result(), metadata())
    write_round(
        round_folder(tmp_path, RUN, 2), None, metadata(outcome=run_folder.CRASHED, exit_code=1)
    )
    # A's round 3 and every round of B have no folder.
    comparison = pair(tmp_path, TimeRule())
    text = comparison_text(comparison)

    assert "1 / 3 rounds finished (1 crashed, 1 missing)" in text
    assert "0 / 3 rounds finished (3 missing)" in text
    assert comparison.verdict == (NO_VERDICT, "B has no finished round")
    assert row(comparison, "status")["judgement"] is None
    assert (row(comparison, "status")["a"], row(comparison, "status")["b"]) == ("Optimal", None)
    assert "B has no record" in comparison.warnings


def test_records_and_solvers_that_differ(tmp_path: Path):
    write_rounds(tmp_path / "old", RUN, [result()])
    write_benchmark(tmp_path / "new", {RUN: options()}, rounds=2)
    newer = metadata(source={"commit": "b" * 40, "uncommitted_changes": True, "branch": "main"})
    newer["versions"].update(highspy="1.16.0", highs="1.16.0")
    write_round(round_folder(tmp_path / "new", RUN, 1), result(), newer)
    write_round(round_folder(tmp_path / "new", RUN, 2), result(), metadata())
    comparison = compare_runs(f"{tmp_path / 'old'}:{RUN}", f"{tmp_path / 'new'}:{RUN}")

    assert comparison.options == []
    assert comparison.records == [
        f"source.commit: {COMMIT} -> {'b' * 40}",
        "source.uncommitted_changes: no -> yes",
        "versions.highspy: 1.15.1 -> 1.16.0",
        "versions.highs: 1.15.1 -> 1.16.0",
    ]
    assert "B's rounds have 2 records: its most common is compared" in comparison.warnings
    assert "B: highspy versions differ: 1.16.0 (1 rounds), 1.15.1 (1 rounds)" in (
        comparison.warnings
    )


def test_warnings_both_sides_share_are_said_once(tmp_path: Path):
    write_benchmark(
        tmp_path, {RUN: options(), OTHER: options(layer="cpp-bindings")}, rounds=1, processes=4
    )
    write_round(round_folder(tmp_path, RUN, 1), result(), metadata())
    load = {"other_processor_percent_mean": 5.0, "on_battery": True}
    write_round(round_folder(tmp_path, OTHER, 1), result(), metadata(load=load))
    warnings = pair(tmp_path).warnings

    assert warnings[0].startswith("4 runs at a time: each run shared the machine")
    assert warnings[1:] == ["B: 1 rounds ran on battery", "B: on battery"]


# MARK: The time rule


@pytest.mark.parametrize(
    ("time_a", "time_b", "rule", "verdict", "reason"),
    [
        (10.4, 10.0, TimeRule(), EQUIVALENT, "+0.400 s <= max(5 % of 10.000, 0.25) = 0.500 s"),
        (10.6, 10.0, TimeRule(), SLOWER, "+0.600 s > max(5 % of 10.000, 0.25) = 0.500 s"),
        (0.3, 0.1, TimeRule(), EQUIVALENT, "+0.200 s <= max(5 % of 0.100, 0.25) = 0.250 s"),
        (0.3, 0.1, TimeRule(5.0, 0.1), SLOWER, "+0.200 s > max(5 % of 0.100, 0.1) = 0.100 s"),
        (5.0, 10.0, TimeRule(0.0, 0.0), EQUIVALENT, "-5.000 s <= max(0 % of 10.000, 0) = 0.000 s"),
    ],
)
def test_the_time_rule_judges_a_median_solve_time(
    tmp_path: Path, time_a: float, time_b: float, rule: TimeRule, verdict: str, reason: str
):
    write_pair(tmp_path, times(time_a, 99.0, 0.0), times(time_b))
    found = pair(tmp_path, rule)

    assert found.verdict == (verdict, f"both Optimal, same objective; {reason}")
    last = comparison_text(found).splitlines()[-1]
    assert last == f"Verdict: {verdict} (both Optimal, same objective; {reason})"


@pytest.mark.parametrize(
    ("results_a", "reason"),
    [
        ([result(objective=191.9)], "the objectives differ: 191.9 and 191.814"),
        ([result(objective=191.8136 * (1 + 1e-9))], None),
        ([result(status="TimeLimit")], "A is TimeLimit, not Optimal"),
        ([result(), result(status="TimeLimit")], "A's rounds differ in status"),
        ([result(), result(objective=191.0)], "A's rounds differ in objective"),
        ([result(solve_seconds=None)], "A has no objective or no solve_seconds"),
        ([{"status": "Optimal", "objective": 1.0}], "A has no objective or no solve_seconds"),
    ],
)
def test_the_rule_needs_both_optimal_with_the_same_objective(
    tmp_path: Path, results_a: list[dict[str, Any]], reason: str | None
):
    write_pair(tmp_path, results_a, [result()])
    verdict = pair(tmp_path, TimeRule()).verdict

    if reason is None:
        assert verdict is not None and verdict[0] == EQUIVALENT
    else:
        assert verdict == (NO_VERDICT, reason)


def test_a_timeout_leaves_the_time_unknown(tmp_path: Path):
    write_pair(tmp_path, [result()], [result()])
    timed_out = round_folder(tmp_path, OTHER, 2)
    write_round(timed_out, run_folder.timeout_result(9.0), metadata(outcome=run_folder.TIMEOUT))

    assert pair(tmp_path, TimeRule()).verdict == (
        NO_VERDICT,
        "B has 1 timeouts, so its time is not known",
    )


# MARK: The script


def test_the_script_prints_writes_and_exits_by_the_verdict(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    write_pair(tmp_path / "pair", times(10.4), times(10.0))
    first, second = f"{tmp_path / 'pair'}:{RUN}", f"{tmp_path / 'pair'}:{OTHER}"
    outputs = ["--output", str(tmp_path / "out.md"), "--output", str(tmp_path / "out.csv")]

    assert compare.main([first, second, *outputs]) == 0
    printed = capsys.readouterr().out
    assert "Verdict" not in printed
    assert (tmp_path / "out.md").read_text() == printed

    rows = read_csv(tmp_path / "out.csv")
    assert list(rows[0]) == list(compare.compare.COLUMNS)
    solve = next(found for found in rows if found["field"] == "solve_seconds")
    assert (solve["kind"], solve["a"], solve["b"]) == ("measured", "10.4", "10.0")

    assert compare.main([first, second, "--time-within", "5%"]) == 0
    assert "Verdict: equivalent" in capsys.readouterr().out
    assert compare.main([first, second, "--time-within", "3"]) == 1
    assert "+0.400 s > max(3 % of 10.000, 0.25) = 0.300 s" in capsys.readouterr().out
    assert compare.main([first, second, "--floor", "0.5"]) == 0
    assert "max(5 % of 10.000, 0.5) = 0.500 s" in capsys.readouterr().out
    assert compare.main([second, first, "--floor", "0"]) == 0

    write_pair(tmp_path / "other", [result(status="TimeLimit")], [result()])
    other = [f"{tmp_path / 'other'}:{RUN}", f"{tmp_path / 'other'}:{OTHER}"]
    assert compare.main([*other, "--floor", "0.25"]) == 3
    assert compare.main(other) == 0


def test_the_script_refuses_wrong_arguments(tmp_path: Path, capsys: pytest.CaptureFixture[str]):
    write_pair(tmp_path, times(1.0), times(1.0))
    first, second = f"{tmp_path}:{RUN}", f"{tmp_path}:{OTHER}"

    assert compare.main([first, second, "--output", str(tmp_path / "out.txt")]) == 2
    assert "is neither .md nor .csv" in capsys.readouterr().err
    assert compare.main([first, f"{tmp_path}:R201"]) == 2
    assert "names no run" in capsys.readouterr().err
    (tmp_path / "empty").mkdir()
    assert compare.main([first, str(tmp_path / "empty")]) == 2
    assert "holds no run" in capsys.readouterr().err

    for wrong in (
        ["--time-within", "five"],
        ["--time-within", "-1%"],
        ["--floor", "a second"],
        ["--floor", "-1"],
    ):
        with pytest.raises(SystemExit) as error:
            compare.main([first, second, *wrong])

        assert error.value.code == 2
