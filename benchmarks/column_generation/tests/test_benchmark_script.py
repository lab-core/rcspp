#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The benchmark: the order of its runs, side by side or not, resuming, the timeout, the STOP
file, Ctrl-C and errors, the runners' exit codes, the checks before the first run, the example
file, folders refused, the events, and the load. A fake runner (``fake_runner.py``) stands for
the layers' runners, except in the last tests, which run a benchmark of the real ones and each
of its runs again."""

import json
import re
import shlex
import shutil
import signal
import subprocess  # nosec B404: the runs' own recorded commands
import sys
import time
import tomllib
from pathlib import Path
from typing import Any

import analysis
import benchmark
import psutil
import pytest
from column_generation_benchmark import run_folder
from column_generation_benchmark.load import Sample

FAKE_RUNNER = Path(__file__).resolve().parent / "fake_runner.py"

#: Two runs, one per instance; the fake runner's plan picks a run by these keys.
TWO_RUNS = '[matrix]\ninstance = ["C101_5", "C101_25"]\n'
FIRST, SECOND = "C101_5-", "C101_25-"


@pytest.fixture
def fake(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    """The fake runner in place of every layer's, with an empty plan."""
    monkeypatch.setenv("FAKE_RUNNER_PLAN", str(tmp_path / "plan.json"))
    monkeypatch.setenv("FAKE_RUNNER_LINES", str(tmp_path / "runner_lines"))
    monkeypatch.setattr(benchmark, "runner_command", fake_runner_command)
    plan(tmp_path, {})


def fake_runner_command(layer: str) -> list[str]:
    return [sys.executable, str(FAKE_RUNNER)]


def plan(tmp_path: Path, what: dict[str, dict[str, Any]]) -> None:
    """What the fake runner does, by a key of the run's folder name."""
    (tmp_path / "plan.json").write_text(json.dumps(what))


def log_lines(tmp_path: Path) -> list[str]:
    """The fake runners' lines, in the order they were written."""
    folder = tmp_path / "runner_lines"
    files = sorted(folder.iterdir()) if folder.exists() else []
    return [path.read_text(encoding="utf-8") for path in files]


def clear_log(tmp_path: Path) -> None:
    shutil.rmtree(tmp_path / "runner_lines")


def runner_log(tmp_path: Path) -> list[str]:
    """The fake runner's starts and ends, in order, without the run names' tails."""
    return [f"{line.split('-')[0]}-{line.rsplit('-', 1)[1]}" for line in log_lines(tmp_path)]


def building(tmp_path: Path) -> int:
    """How many fake runners have started building."""
    return len(list(tmp_path.glob("building-*")))


def processes_of(tmp_path: Path) -> list[int]:
    """The processes still running whose command line names `tmp_path`: the fake runners
    of a test."""
    found: list[int] = []

    for process in psutil.process_iter(["cmdline"]):
        command = " ".join(process.info["cmdline"] or [])

        if str(tmp_path) in command:
            found.append(process.pid)

    return found


def events(tmp_path: Path) -> list[dict[str, Any]]:
    return run_folder.read_json(tmp_path / "out" / benchmark.METADATA_FILE)["events"]


def run(build: Path, tmp_path: Path, text: str, *arguments: str) -> int:
    """The benchmark `text` describes, into ``tmp_path/out``; its exit code."""
    path = tmp_path / "benchmark.toml"
    path.write_text(text)
    output = tmp_path / "out"
    return benchmark.main([str(path), "--build", str(build), "--output", str(output), *arguments])


def folders(tmp_path: Path) -> dict[str, Path]:
    """The run folders, by their instance and round (``C101_5-r1``)."""
    found = sorted((tmp_path / "out" / benchmark.RUNS_FOLDER).iterdir())
    return {f"{path.name.split('-')[0]}-{path.name.rsplit('-', 1)[1]}": path for path in found}


def result_of(folder: Path) -> dict[str, Any]:
    result = run_folder.read_result(folder)
    assert result is not None
    return result


def metadata(folder: Path) -> dict[str, Any]:
    return run_folder.read_json(folder / run_folder.METADATA_FILE)


def status_lines(tmp_path: Path) -> list[str]:
    return (tmp_path / "out" / benchmark.STATUS_FILE).read_text().splitlines()


# MARK: Order


def test_stops_without_a_fleet_are_warned_of_once(
    build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    text = (
        TWO_RUNS
        + "[defaults]\ncolumn_generation = { relative_gap = 0.01, integer_objective = false }\n"
    )

    assert run(build, tmp_path, text, "--check", "options") == 0
    warnings = [line for line in capsys.readouterr().err.splitlines() if "warning" in line]

    assert len(warnings) == 1
    assert warnings[0].startswith("warning: 2 runs set a column generation stop (C101_5-")
    assert "these stops cannot stop the run" in warnings[0]


@pytest.mark.usefixtures("fake")
def test_runs_go_round_after_round(build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]):
    assert run(build, tmp_path, "[benchmark]\nrepeats = 2\n" + TWO_RUNS) == 0

    printed = capsys.readouterr().out
    assert "starting " in printed and ": 4 runs to run" in printed
    assert "[1/4] C101_5-package-highspy-exact-artificial-r1: finished, " in printed
    assert "[4/4] C101_25-package-highspy-exact-artificial-r2: finished, " in printed
    assert printed.splitlines()[-2].startswith("4 / 4 runs finished")
    assert printed.splitlines()[-1] == f"summary: {tmp_path / 'out' / 'summary.md'}"

    starts = [line for line in runner_log(tmp_path) if line.startswith("start")]
    assert starts == ["start C101_5-r1", "start C101_25-r1", "start C101_5-r2", "start C101_25-r2"]

    output = tmp_path / "out"
    table = tomllib.loads((output / benchmark.BENCHMARK_FILE).read_text())
    assert [entry["name"] for entry in table["runs"]] == [
        "C101_5-package-highspy-exact-artificial",
        "C101_25-package-highspy-exact-artificial",
    ]
    assert len(list((output / benchmark.RUN_FILES_FOLDER).iterdir())) == 2
    summary = (output / "summary.md").read_text()
    assert "- 4 / 4 rounds finished (0 failed checks), 0 timeouts, 0 not finished" in summary
    assert (output / "summary.csv").is_file() and (output / "runs.csv").is_file()
    assert status_lines(tmp_path)[0].split("  ", 1)[1].startswith("4 / 4 runs finished")
    assert status_lines(tmp_path)[-1].startswith("finished at")

    record = run_folder.read_json(output / benchmark.METADATA_FILE)
    assert (record["processes"], record["runs"], record["rounds"]) == (1, 2, 2)
    assert "repeats" not in record
    assert record["record"]["benchmark"]["processes"] == 1
    assert [event["event"] for event in record["events"]] == ["started", "finished"]
    assert (record["events"][0]["to_run"], record["events"][1]["finished"]) == (4, 4)

    for folder in folders(tmp_path).values():
        written = metadata(folder)
        assert (written["exit_code"], written["outcome"]) == (0, "finished")
        assert written["benchmark"] == {"solver_threads": None, "processes": 1}
        assert written["load"]["samples"] >= 2
        assert written["load"]["other_processor_percent_mean"] is not None
        assert (folder / run_folder.LOG_FILE).exists()
        assert (folder / run_folder.RUN_FILE).exists()


@pytest.mark.usefixtures("fake")
def test_processes_run_side_by_side_with_one_thread_each(build: Path, tmp_path: Path):
    plan(tmp_path, {FIRST: {"sleep": 1.0}, SECOND: {"sleep": 1.0}})

    assert run(build, tmp_path, TWO_RUNS, "--processes", "2", "--no-logs") == 0

    # Both start before either ends, in any order.
    assert sorted(runner_log(tmp_path)[:2]) == ["start C101_25-r1", "start C101_5-r1"]

    for path in (tmp_path / "out" / benchmark.RUN_FILES_FOLDER).iterdir():
        assert tomllib.loads(path.read_text())["column_generation"]["threads"] == 1

    for folder in folders(tmp_path).values():
        assert metadata(folder)["benchmark"]["processes"] == 2
        assert not (folder / run_folder.LOG_FILE).exists()


# MARK: Resume


@pytest.mark.usefixtures("fake")
def test_a_benchmark_resumes_with_its_unfinished_runs(
    build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    assert run(build, tmp_path, TWO_RUNS) == 0

    unfinished = folders(tmp_path)["C101_25-r1"]
    (unfinished / run_folder.RESULT_FILE).unlink()
    (unfinished / "left.txt").write_text("from the run cut off")
    clear_log(tmp_path)

    capsys.readouterr()
    assert run(build, tmp_path, TWO_RUNS) == 0

    assert runner_log(tmp_path) == ["start C101_25-r1", "end C101_25-r1"]
    assert not (unfinished / "left.txt").exists()
    assert run_folder.read_result(unfinished) is not None
    captured = capsys.readouterr()
    printed = captured.out
    assert (
        ": 1 / 2 finished (0 timeout, 0 check failed), 1 to run (1 unfinished, removed first)"
        in (printed)
    )
    # Loud: an existing folder is resumed, not started again.
    assert f"{tmp_path / 'out'} already holds this benchmark, started " in captured.err
    assert "RESUMING" in captured.err
    assert "[2/2] C101_25-package-highspy-exact-artificial-r1: finished" in printed
    resumed = events(tmp_path)[2]
    assert [event["event"] for event in events(tmp_path)] == [
        "started",
        "finished",
        "resumed",
        "finished",
    ]
    assert (resumed["total"], resumed["finished"], resumed["to_run"]) == (2, 1, 1)
    assert resumed["unfinished"] == 1
    assert "changed" not in resumed

    assert run(build, tmp_path, TWO_RUNS, "--check", "options") == 0
    assert "already holds 2 / 2 finished runs" in capsys.readouterr().out


@pytest.mark.usefixtures("fake")
def test_a_folder_of_another_benchmark_is_refused(
    build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    assert run(build, tmp_path, TWO_RUNS) == 0
    assert run(build, tmp_path, '[matrix]\ninstance = ["C101_5"]\n') == benchmark.NOT_STARTED
    assert (
        "another benchmark's runs (1 removed: C101_25-package-highspy-exact-artificial)"
        in capsys.readouterr().err
    )
    assert [event["event"] for event in events(tmp_path)] == ["started", "finished"]


@pytest.mark.usefixtures("fake")
def test_other_processes_make_other_runs(
    build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    assert run(build, tmp_path, TWO_RUNS) == 0
    assert run(build, tmp_path, TWO_RUNS, "--processes", "2") == benchmark.NOT_STARTED
    assert "resume with --processes 1, or choose another folder" in capsys.readouterr().err


@pytest.mark.usefixtures("fake")
def test_a_mixed_benchmark_is_refused_unless_allowed(
    build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    assert run(build, tmp_path, TWO_RUNS, "--machine", "laptop") == 0
    assert run(build, tmp_path, TWO_RUNS, "--machine", "desktop") == benchmark.NOT_STARTED
    assert "machine.label: 'laptop' -> 'desktop'" in capsys.readouterr().err

    assert run(build, tmp_path, TWO_RUNS, "--machine", "desktop", "--allow-mixed") == 0
    written = run_folder.read_json(tmp_path / "out" / benchmark.METADATA_FILE)
    assert written["record"]["machine"]["label"] == "laptop"
    assert written["other_records"][0]["machine"]["label"] == "desktop"
    assert written["events"][-2]["allow_mixed"] is True
    assert written["events"][-2]["changed"] == {"machine": ["laptop", "desktop"]}


# MARK: Timeout, STOP


@pytest.mark.usefixtures("fake")
def test_a_run_past_its_timeout_is_killed_and_kept(build: Path, tmp_path: Path):
    plan(tmp_path, {FIRST: {"sleep": 60.0}})
    started = time.monotonic()

    assert run(build, tmp_path, TWO_RUNS, "--timeout", "2") == benchmark.RUNS_FAILED

    assert time.monotonic() - started < 30.0
    killed = folders(tmp_path)["C101_5-r1"]
    result = run_folder.read_result(killed)
    assert result is not None and result["status"] == run_folder.TIMEOUT
    assert 2.0 <= result["solve_seconds"] < 30.0
    assert result["iterations"] is None
    assert (metadata(killed)["exit_code"], metadata(killed)["outcome"]) == (None, "timeout")
    assert (killed / run_folder.RUN_FILE).exists()
    assert result_of(folders(tmp_path)["C101_25-r1"])["status"] == "Optimal"

    # A timeout is a result: a resume keeps it, unless told to run it again.
    clear_log(tmp_path)
    assert run(build, tmp_path, TWO_RUNS, "--timeout", "2") == benchmark.RUNS_FAILED
    assert runner_log(tmp_path) == []

    plan(tmp_path, {})
    assert run(build, tmp_path, TWO_RUNS, "--retry-timeouts") == 0
    assert runner_log(tmp_path) == ["start C101_5-r1", "end C101_5-r1"]
    assert result_of(killed)["status"] == "Optimal"
    retried = events(tmp_path)[-2]
    assert (retried["event"], retried["retry_timeouts"]) == ("resumed", True)
    assert (retried["timeouts_to_run_again"], retried["unfinished"]) == (1, 0)
    assert retried["changed"] == {"timeout": [2.0, None]}


@pytest.mark.usefixtures("fake")
def test_a_stop_file_stops_after_the_runs_in_progress(build: Path, tmp_path: Path):
    plan(tmp_path, {FIRST: {"stop": True}})

    assert run(build, tmp_path, TWO_RUNS) == benchmark.STOPPED

    assert runner_log(tmp_path) == ["start C101_5-r1", "end C101_5-r1"]
    assert not (tmp_path / "out" / benchmark.STOP_FILE).exists()
    assert status_lines(tmp_path)[-1].startswith("stopped at")

    plan(tmp_path, {})
    assert run(build, tmp_path, TWO_RUNS) == 0
    assert runner_log(tmp_path)[2:] == ["start C101_25-r1", "end C101_25-r1"]
    kinds = [event["event"] for event in events(tmp_path)]
    assert kinds == ["started", "stopped", "resumed", "finished"]
    stopped = events(tmp_path)[1]
    assert (stopped["total"], stopped["finished"], stopped["runs_left"]) == (2, 1, 1)


# MARK: Ctrl-C and errors


@pytest.mark.usefixtures("fake")
@pytest.mark.parametrize(
    ("error", "code", "reason"),
    [
        (KeyboardInterrupt(), benchmark.INTERRUPTED, "Ctrl-C"),
        (RuntimeError("the benchmark broke"), benchmark.FAILED, "the benchmark broke"),
    ],
)
def test_ctrl_c_or_an_error_kills_the_runs_in_progress(
    build: Path,
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    error: BaseException,
    code: int,
    reason: str,
):
    plan(tmp_path, {FIRST: {"sleep": 60.0}})
    samples = 0
    take_sample = benchmark.take_sample

    def failing() -> Sample:
        # The first sample is the run's start, the next ones come every second.
        nonlocal samples
        samples += 1

        if samples == 3:
            raise error

        return take_sample()

    monkeypatch.setattr(benchmark, "take_sample", failing)
    started = time.monotonic()

    assert run(build, tmp_path, TWO_RUNS) == code

    assert time.monotonic() - started < 30.0
    assert processes_of(tmp_path) == []
    cut = folders(tmp_path)["C101_5-r1"]
    assert metadata(cut)["outcome"] == "interrupted"
    assert run_folder.read_result(cut) is None
    assert status_lines(tmp_path)[-1].startswith("interrupted at")
    last = events(tmp_path)[-1]
    assert (last["event"], last["killed"]) == ("interrupted", [cut.name])
    assert reason in last["reason"]

    monkeypatch.setattr(benchmark, "take_sample", take_sample)
    plan(tmp_path, {})
    assert run(build, tmp_path, TWO_RUNS) == 0
    assert result_of(cut)["status"] == "Optimal"


@pytest.mark.usefixtures("fake")
def test_ctrl_c_during_the_build_check_kills_the_runners(
    build: Path, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
):
    plan(tmp_path, {FIRST: {"build_sleep": 60.0}, SECOND: {"build_sleep": 60.0}})
    looks = 0
    wait = benchmark.wait

    def interrupted(futures: Any, timeout: float) -> Any:
        nonlocal looks
        looks += 1

        # Once both runners have started building.
        if building(tmp_path) == 2 or looks > 400:
            raise KeyboardInterrupt

        return wait(futures, timeout=timeout)

    monkeypatch.setattr(benchmark, "wait", interrupted)
    started = time.monotonic()

    assert run(build, tmp_path, TWO_RUNS, "--processes", "2") == benchmark.INTERRUPTED

    assert time.monotonic() - started < 30.0
    assert building(tmp_path) == 2
    assert processes_of(tmp_path) == []
    assert not (tmp_path / "out").exists()


# MARK: Exit codes


FOUR_RUNS = '[matrix]\ninstance = ["C101_5", "C101_25", "C102_25", "R201_25"]\n'


@pytest.mark.usefixtures("fake")
def test_each_exit_code_is_recorded_and_only_unfinished_runs_resume(build: Path, tmp_path: Path):
    plan(
        tmp_path,
        {FIRST: {"check_failed": True}, SECOND: {"crash": True}, "C102_25-": {"exit": 2}},
    )

    assert run(build, tmp_path, FOUR_RUNS) == benchmark.RUNS_FAILED

    found = folders(tmp_path)
    outcomes = {
        name: (metadata(path)["exit_code"], metadata(path)["outcome"])
        for name, path in found.items()
    }
    assert outcomes == {
        "C101_5-r1": (1, "check failed"),
        "C101_25-r1": (1, "crashed"),
        "C102_25-r1": (2, "not started"),
        "R201_25-r1": (0, "finished"),
    }
    assert run_folder.read_result(found["C101_5-r1"]) is not None
    assert run_folder.read_result(found["C101_25-r1"]) is None
    assert "the fake runner crashed" in (found["C101_25-r1"] / run_folder.LOG_FILE).read_text()
    assert "1 check failed), 2 crashed or not started" in status_lines(tmp_path)[0]

    # A failed check is a result; a crash and a run that did not start run again.
    plan(tmp_path, {})
    clear_log(tmp_path)
    assert run(build, tmp_path, FOUR_RUNS) == benchmark.RUNS_FAILED
    assert [line for line in runner_log(tmp_path) if line.startswith("start")] == [
        "start C101_25-r1",
        "start C102_25-r1",
    ]


# MARK: Checks


@pytest.mark.usefixtures("fake")
def test_a_failed_build_check_starts_no_run(
    build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    plan(tmp_path, {FIRST: {"build_fails": True}})

    assert run(build, tmp_path, TWO_RUNS) == benchmark.NOT_STARTED
    assert "C101_5-package-highspy-exact-artificial: cannot build" in capsys.readouterr().err
    assert runner_log(tmp_path) == []

    assert run(build, tmp_path, TWO_RUNS, "--no-check") == 0
    assert len(runner_log(tmp_path)) == 4


@pytest.mark.usefixtures("fake")
def test_the_checks_alone_write_nothing(
    build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    text = TWO_RUNS + 'layer = ["package", "cpp-bindings"]\nmaster = ["highspy", "highs"]\n'

    assert run(build, tmp_path, text, "--check", "options") == 0
    printed = capsys.readouterr().out
    assert printed.count("dropped:") == 2
    assert "layer package has no master highs" in printed
    assert "6 runs, 1 round, into" in printed
    assert "C101_5-cpp-bindings-highs-exact-artificial" in printed

    assert run(build, tmp_path, TWO_RUNS, "--check", "build") == 0
    assert "every run built" in capsys.readouterr().out
    assert not (tmp_path / "out").exists()


@pytest.mark.usefixtures("fake")
@pytest.mark.parametrize(
    ("text", "message"),
    [
        ('[defaults]\ninstance = "MISSING"\n', "MISSING"),
        ("[benchmark]\nprocesses = 0\n" + TWO_RUNS, "at least 1"),
        ('[defaults]\ninstance = "C101_5"\nmaster = "highs"\n', "has no master highs"),
    ],
)
def test_wrong_options_start_nothing(
    build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str], text: str, message: str
):
    assert run(build, tmp_path, text) == benchmark.NOT_STARTED
    assert message in capsys.readouterr().err
    assert not (tmp_path / "out").exists()


def test_the_example_file_expands_and_its_instances_read(
    build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    example = Path(benchmark.__file__).resolve().parent / "example.toml"
    output = tmp_path / "example"
    arguments = [str(example), "--check", "options", "--build", str(build), "--output", str(output)]

    assert benchmark.main(arguments) == 0
    printed = capsys.readouterr().out
    assert printed.count("dropped:") == 12
    assert "26 runs, 2 rounds, into" in printed
    assert "RC201_12-package-highspy-exact-single-customer-r<1..2>" in printed
    assert not output.exists()


def test_the_command_line_alone_needs_an_output():
    with pytest.raises(SystemExit):
        benchmark.parse_arguments(["--instance", "C101_5"])

    with pytest.raises(SystemExit):
        benchmark.parse_arguments(["file.toml", "--check", "build", "--no-check"])


# MARK: Outcomes and load


@pytest.mark.parametrize(
    ("exit_code", "result", "outcome"),
    [
        (0, {"status": "Optimal", "solution_check": "passed"}, "finished"),
        (None, {"status": "Optimal", "solution_check": None}, "finished"),
        (None, {"status": "timeout", "solution_check": None}, "timeout"),
        (1, {"status": "Optimal", "solution_check": "failed"}, "check failed"),
        (1, {"status": "Optimal", "solution_check": "passed"}, "crashed"),
        (1, None, "crashed"),
        (0, None, "crashed"),
        (-9, None, "crashed"),
        (2, None, "not started"),
    ],
)
def test_the_outcome_follows_the_exit_code_and_the_result(
    exit_code: int | None, result: dict[str, Any] | None, outcome: str
):
    assert benchmark.outcome_of(exit_code, result) == outcome


@pytest.mark.usefixtures("fake")
def test_the_end_sample_takes_the_processor_time_the_runner_reports(
    build: Path, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
):
    # More than the machine could give: the other processes then used nothing.
    plan(tmp_path, {FIRST: {"processor_seconds": 1e6}})

    # A machine whose processors are busy for 100 s between two samples: the other
    # processes always use some, whatever the real machine and its system's accounting.
    count = 0

    def sample() -> Sample:
        nonlocal count
        count += 1
        return Sample(time.monotonic(), 100.0 * count, 1000.0 * count, 0, None, None)

    monkeypatch.setattr(benchmark, "take_sample", sample)
    assert run(build, tmp_path, TWO_RUNS) == 0

    found = folders(tmp_path)
    assert metadata(found["C101_5-r1"])["load"]["other_processor_percent_mean"] == 0.0
    assert metadata(found["C101_25-r1"])["load"]["other_processor_percent_mean"] > 0.0


# MARK: Crashes, signals, the lock


@pytest.mark.usefixtures("fake")
def test_a_crash_after_its_result_is_kept_unless_retried(build: Path, tmp_path: Path):
    plan(tmp_path, {FIRST: {"crash_after_result": True}})

    assert run(build, tmp_path, TWO_RUNS) == benchmark.RUNS_FAILED
    crashed = folders(tmp_path)["C101_5-r1"]
    assert metadata(crashed)["outcome"] == "crashed"

    # Kept, and still counted as crashed: not a finished run.
    plan(tmp_path, {})
    clear_log(tmp_path)
    assert run(build, tmp_path, TWO_RUNS) == benchmark.RUNS_FAILED
    assert runner_log(tmp_path) == []
    assert events(tmp_path)[-1]["crashed"] == 1

    assert run(build, tmp_path, TWO_RUNS, "--retry-crashes") == 0
    assert runner_log(tmp_path) == ["start C101_5-r1", "end C101_5-r1"]
    assert metadata(crashed)["outcome"] == "finished"
    retried = events(tmp_path)[-2]
    assert (retried["retry_crashes"], retried["crashes_to_run_again"]) == (True, 1)


def fail_at_sample(monkeypatch: pytest.MonkeyPatch, number: int, failure: Any) -> None:
    """`failure` called at the `number`-th load sample: the first is a run's start, the
    next ones come every second."""
    samples = 0
    take_sample = benchmark.take_sample

    def failing() -> Sample:
        nonlocal samples
        samples += 1

        if samples == number:
            failure()

        return take_sample()

    monkeypatch.setattr(benchmark, "take_sample", failing)


@pytest.mark.usefixtures("fake")
def test_a_run_interrupted_after_its_result_is_finished(
    build: Path, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
):
    plan(tmp_path, {FIRST: {"sleep_after_result": 60.0}})

    def ctrl_c() -> None:
        raise KeyboardInterrupt

    fail_at_sample(monkeypatch, 3, ctrl_c)

    assert run(build, tmp_path, TWO_RUNS) == benchmark.INTERRUPTED
    done = folders(tmp_path)["C101_5-r1"]
    assert metadata(done)["outcome"] == "finished"
    assert processes_of(tmp_path) == []

    monkeypatch.undo()
    monkeypatch.setenv("FAKE_RUNNER_PLAN", str(tmp_path / "plan.json"))
    monkeypatch.setenv("FAKE_RUNNER_LINES", str(tmp_path / "runner_lines"))
    monkeypatch.setattr(benchmark, "runner_command", fake_runner_command)
    plan(tmp_path, {})
    clear_log(tmp_path)

    assert run(build, tmp_path, TWO_RUNS) == 0
    assert runner_log(tmp_path) == ["start C101_25-r1", "end C101_25-r1"]


@pytest.mark.usefixtures("fake")
def test_sigterm_ends_the_benchmark_as_ctrl_c_does(
    build: Path, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
):
    # A scheduler's end, `timeout`, `kill`: the runs must not go on as orphans.
    plan(tmp_path, {FIRST: {"sleep": 60.0}})
    fail_at_sample(monkeypatch, 3, lambda: signal.raise_signal(signal.SIGTERM))
    handler = signal.getsignal(signal.SIGTERM)

    assert run(build, tmp_path, TWO_RUNS) == benchmark.INTERRUPTED
    assert processes_of(tmp_path) == []
    assert "SIGTERM" in events(tmp_path)[-1]["reason"]
    assert signal.getsignal(signal.SIGTERM) == handler


@pytest.mark.usefixtures("fake")
def test_a_second_ctrl_c_does_not_stop_the_cleanup(
    build: Path, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
):
    plan(tmp_path, {FIRST: {"sleep": 60.0}})

    def ctrl_c() -> None:
        raise KeyboardInterrupt

    fail_at_sample(monkeypatch, 3, ctrl_c)
    kill_tree = benchmark.kill_tree

    def pressed_again(process: Any) -> None:
        signal.raise_signal(signal.SIGINT)
        kill_tree(process)

    monkeypatch.setattr(benchmark, "kill_tree", pressed_again)
    handler = signal.getsignal(signal.SIGINT)

    assert run(build, tmp_path, TWO_RUNS) == benchmark.INTERRUPTED
    assert processes_of(tmp_path) == []
    assert events(tmp_path)[-1]["event"] == "interrupted"
    assert signal.getsignal(signal.SIGINT) == handler


@pytest.mark.usefixtures("fake")
def test_a_runs_child_is_killed_at_the_timeout(build: Path, tmp_path: Path):
    plan(tmp_path, {FIRST: {"child_sleep": 60.0, "sleep": 60.0}})

    assert run(build, tmp_path, TWO_RUNS, "--timeout", "2") == benchmark.RUNS_FAILED
    assert processes_of(tmp_path) == []
    # Its output is in its log although it was killed: the runner writes it unbuffered.
    assert "working" in (folders(tmp_path)["C101_5-r1"] / run_folder.LOG_FILE).read_text()


@pytest.mark.usefixtures("fake")
def test_a_folder_in_use_is_refused_and_a_stale_lock_taken_over(
    build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    output = tmp_path / "out"
    output.mkdir()
    me = psutil.Process()
    lock = output / benchmark.LOCK_FILE
    lock.write_text(json.dumps({"pid": me.pid, "created": me.create_time()}))

    assert run(build, tmp_path, TWO_RUNS) == benchmark.NOT_STARTED
    assert f"in use by another benchmark (process {me.pid})" in capsys.readouterr().err

    # The process that held it is gone.
    lock.write_text(json.dumps({"pid": me.pid, "created": me.create_time() - 1000.0}))

    assert run(build, tmp_path, TWO_RUNS) == 0
    assert not lock.exists()


@pytest.mark.usefixtures("fake")
def test_a_stop_file_during_the_last_runs_finishes_the_benchmark(build: Path, tmp_path: Path):
    plan(tmp_path, {SECOND: {"stop": True}})

    assert run(build, tmp_path, TWO_RUNS) == 0
    assert [event["event"] for event in events(tmp_path)] == ["started", "finished"]
    assert not (tmp_path / "out" / benchmark.STOP_FILE).exists()


@pytest.mark.usefixtures("fake")
def test_a_stop_file_from_before_is_removed_at_the_start(
    build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    assert run(build, tmp_path, TWO_RUNS) == 0
    shutil.rmtree(folders(tmp_path)["C101_25-r1"])
    (tmp_path / "out" / benchmark.STOP_FILE).touch()

    assert run(build, tmp_path, TWO_RUNS) == 0
    assert "a STOP file from before was removed" in capsys.readouterr().out


@pytest.mark.usefixtures("fake")
def test_other_processes_with_runs_that_set_their_threads_resume(build: Path, tmp_path: Path):
    text = TWO_RUNS + '[defaults]\n"column_generation.threads" = 1\n'

    assert run(build, tmp_path, text) == 0
    shutil.rmtree(folders(tmp_path)["C101_25-r1"])

    # The runs are the same: only the benchmark's processes changed, not its record.
    assert run(build, tmp_path, text, "--processes", "2") == 0
    assert events(tmp_path)[-2]["changed"] == {"processes": [1, 2]}


@pytest.mark.usefixtures("fake")
def test_other_threads_are_other_runs_whatever_the_processes(
    build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    assert run(build, tmp_path, TWO_RUNS + '[defaults]\n"column_generation.threads" = 2\n') == 0

    text = TWO_RUNS + '[defaults]\n"column_generation.threads" = 4\n'

    assert run(build, tmp_path, text) == benchmark.NOT_STARTED
    printed = capsys.readouterr().err
    assert "another benchmark's runs" in printed and "--processes" not in printed


@pytest.mark.usefixtures("fake")
def test_allowing_a_mix_twice_records_the_other_record_once(build: Path, tmp_path: Path):
    assert run(build, tmp_path, TWO_RUNS, "--machine", "laptop") == 0

    for _ in range(2):
        shutil.rmtree(folders(tmp_path)["C101_25-r1"])
        assert run(build, tmp_path, TWO_RUNS, "--machine", "desktop", "--allow-mixed") == 0

    written = run_folder.read_json(tmp_path / "out" / benchmark.METADATA_FILE)
    assert [other["machine"]["label"] for other in written["other_records"]] == ["desktop"]


@pytest.mark.usefixtures("fake")
def test_an_unreadable_result_ends_the_benchmark_as_its_own_error(
    build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    assert run(build, tmp_path, TWO_RUNS) == 0
    (folders(tmp_path)["C101_25-r1"] / run_folder.RESULT_FILE).write_text("{ not json")

    assert run(build, tmp_path, TWO_RUNS) == benchmark.FAILED
    assert "interrupted" in capsys.readouterr().err


@pytest.mark.usefixtures("fake")
def test_a_failed_summary_only_warns(
    build: Path, tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
):
    def broken(output: Path) -> Any:
        raise ValueError("no figures")

    monkeypatch.setattr(benchmark, "write_summary", broken)

    assert run(build, tmp_path, TWO_RUNS) == 0
    assert "benchmark.py: no summary: no figures" in capsys.readouterr().err


@pytest.mark.usefixtures("fake")
def test_a_benchmark_from_the_command_line_alone(build: Path, tmp_path: Path):
    output = tmp_path / "out"
    arguments = ["--instance", "C101_5", "--build", str(build), "--output", str(output)]

    assert benchmark.main(arguments) == 0
    assert result_of(output / benchmark.RUNS_FOLDER / "C101_5-package-highspy-exact-artificial-r1")


@pytest.mark.usefixtures("fake")
def test_a_folder_without_its_metadata_resumes(build: Path, tmp_path: Path):
    assert run(build, tmp_path, TWO_RUNS) == 0
    (tmp_path / "out" / benchmark.METADATA_FILE).unlink()
    shutil.rmtree(folders(tmp_path)["C101_25-r1"])

    assert run(build, tmp_path, TWO_RUNS) == 0
    assert [event["event"] for event in events(tmp_path)] == ["resumed", "finished"]


@pytest.mark.usefixtures("fake")
def test_the_runs_processor_time_is_read_at_the_samples_only(
    build: Path, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
):
    # Listing a process tree costs the benchmark processor time, which the runs then lose.
    plan(tmp_path, {FIRST: {"sleep": 2.0}})
    reads = 0
    read = benchmark.tree_processor_seconds

    def counted(tree: Any) -> float | None:
        nonlocal reads
        reads += 1
        return read(tree)

    monkeypatch.setattr(benchmark, "tree_processor_seconds", counted)

    assert run(build, tmp_path, TWO_RUNS) == 0
    # About one a second and one at each end, not one every poll: 20 a second.
    assert reads < 12


def test_a_finished_process_has_no_tree_to_read():
    # On Windows, psutil cannot open a process that already ended.
    process = subprocess.Popen([sys.executable, "-c", "pass"])  # nosec B603
    process.wait()

    benchmark.tree_of(process)


def test_the_example_file_builds(build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]):
    if not has_example_and_gurobi():
        pytest.skip("the example file needs the column generation example and gurobipy")

    example = Path(benchmark.__file__).resolve().parent / "example.toml"
    output = tmp_path / "example"
    arguments = [str(example), "--check", "build", "--build", str(build), "--output", str(output)]

    assert benchmark.main([*arguments, "--processes", "4"]) == 0
    assert "every run built" in capsys.readouterr().out
    assert not output.exists()


def has_example_and_gurobi() -> bool:
    from test_benchmark_runners import has_example, has_gurobi

    return has_example() and has_gurobi()


# MARK: Real runners


SAME = ("status", "objective", "iterations", "columns", "artificial_columns", "integer_objective")


def same(result: dict[str, Any] | None) -> dict[str, Any]:
    assert result is not None
    return {name: result[name] for name in SAME}


def test_every_kind_of_input_runs_and_runs_again(
    build: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    """A benchmark of the real runners with a matrix, a ``[[runs]]`` entry and a
    ``from``, and a run of the C++ example's runner when the build has it; each of its
    runs then runs again from its ``run.toml`` and from its ``command.txt``.

    The summary and the comparison then read the folders the real runners wrote.
    """
    from test_benchmark_runners import RUNNERS, has_example

    recorded = tmp_path / "recorded.toml"
    recorded.write_text('instance = "C101_5"\nstart = "single-customer"\n')
    text = (
        '[defaults]\n"column_generation.max_iterations" = 50\n'
        '[matrix]\ninstance = ["C101_5"]\npricing = ["exact"]\n'
        '[[runs]]\ninstance = "C102_25"\n'
        '[[runs]]\nfrom = "recorded.toml"\n"start.type" = "single-customer"\n'
    )
    layers = {"package"}

    if has_example():
        text += '[[runs]]\ninstance = "C101_5"\nlayer = "cpp-bindings"\n'
        layers.add("cpp-bindings")

    assert run(build, tmp_path, text, "--column_generation.timeout_s", "60") == 0

    found = sorted((tmp_path / "out" / benchmark.RUNS_FOLDER).iterdir())
    assert len(found) == 2 + len(layers)

    for folder in found:
        table = tomllib.loads((folder / run_folder.RUN_FILE).read_text())
        assert table["column_generation"]["max_iterations"] == 50
        assert table["column_generation"]["timeout_s"] == 60.0
        layers.discard(table["layer"])
        result = same(run_folder.read_result(folder))

        again = tmp_path / "again" / folder.name
        arguments = ["--file", str(folder / run_folder.RUN_FILE), "--output", str(again)]
        assert RUNNERS[table["layer"]].main([*arguments, "--build", str(build)]) == 0
        assert same(run_folder.read_result(again)) == result

        command = (folder / run_folder.COMMAND_FILE).read_text(encoding="utf-8").strip()
        command += " --overwrite"
        recorded_command = command if sys.platform == "win32" else shlex.split(command)
        subprocess.run(recorded_command, check=True, capture_output=True)  # nosec B603
        assert same(run_folder.read_result(folder)) == result

    single = [
        folder for folder in found if folder.name.startswith("C101_5-package-highspy-exact-single")
    ]
    assert len(single) == 1
    # Every layer ran.
    assert layers == set()

    # The summary the benchmark wrote, from real records.
    written = (tmp_path / "out" / "summary.md").read_text()
    assert "- solvers: highspy" in written and "mixed" not in written
    assert "## Rounds not in the figures" not in written

    # A run of the benchmark against the same run made again by its runner alone.
    first = found[0]
    name = first.name.removesuffix("-r1")
    capsys.readouterr()
    arguments = [f"{tmp_path / 'out'}:{name}", str(tmp_path / "again" / first.name)]

    assert analysis.main(["compare", *arguments, "--floor", "1000"]) == 0
    printed = capsys.readouterr().out
    assert "- Options that differ: none" in printed
    assert "- Records that differ: none" in printed
    assert re.search(r"\| status +\| Optimal +\| Optimal +\| equal", printed)
    assert "Verdict: equivalent" in printed
