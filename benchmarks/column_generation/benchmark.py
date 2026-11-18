#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Many runs, each by its layer's runner in its own process, into one benchmark folder.

    python benchmark.py my_benchmark.toml                  # into results/my_benchmark
    python benchmark.py my_benchmark.toml --check options  # list the runs, run nothing
    python benchmark.py --instance C101_25 --output results/one

The benchmark expands its file into runs and checks them all before the first starts: the
options, then each run built by its runner without solving (unless ``--no-check``). It then
starts ``processes`` runs at a time, round after round, each as
``runners/<layer>_runner.py --file run_files/<run>.toml --output runs/<run>-r<round>``.
Run again on the same folder, it resumes: a run folder with a ``result.json`` is finished
and skipped, any other is removed and its run started again. A ``STOP`` file in the folder
stops it once the runs in progress end.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess  # nosec B404: the runners, this folder's own scripts
import sys
import tempfile
import time
from collections import deque
from collections.abc import Mapping, Sequence
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from pathlib import Path
from typing import IO, TYPE_CHECKING, Any

# The code the runners and the benchmark share is in this folder.
sys.path.insert(0, str(Path(__file__).resolve().parent))

from column_generation_benchmark import run_folder, runner, source_tree  # noqa: E402
from column_generation_benchmark.instances import (  # noqa: E402
    INSTANCE_TYPES,
    make_instance,
)
from column_generation_benchmark.load import (  # noqa: E402
    INTERVAL_SECONDS,
    Load,
    take_sample,
    tree_processor_seconds,
)
from column_generation_benchmark.options import (  # noqa: E402
    Benchmark,
    Expansion,
    OptionsError,
    Run,
    add_benchmark_options,
    add_run_options,
    benchmark_from_arguments,
    folder_name,
    read_toml,
    run_from_arguments,
)
from column_generation_benchmark.record import make_record  # noqa: E402

if TYPE_CHECKING:
    import psutil

#: The runners, one per layer: ``<layer>_runner.py``.
RUNNERS = Path(__file__).resolve().parent / "runners"

BENCHMARK_FILE = "benchmark.toml"
METADATA_FILE = "metadata.json"
STATUS_FILE = "status.txt"
STOP_FILE = "STOP"
RUN_FILES_FOLDER = "run_files"
RUNS_FOLDER = "runs"

#: The starts every layer makes; the others come from the instance's type.
LAYER_STARTS = ("artificial",)

#: The seconds between two looks at the runs in progress.
POLL_SECONDS = 0.05

#: The benchmark's exit codes besides 0, when every run finished and passed its check.
RUNS_FAILED = 1  # a run timed out, crashed, did not start, or failed its check
NOT_STARTED = 2  # wrong options, a failed build check, a mixed benchmark refused
STOPPED = 3  # a STOP file stopped it before the end
INTERRUPTED = 130  # Ctrl-C

#: A run's outcome in its ``metadata.json``.
FINISHED = "finished"
CHECK_FAILED = "check failed"
CRASHED = "crashed"
RUN_NOT_STARTED = "not started"


class BenchmarkError(Exception):
    """A benchmark that cannot start: a failed build check, a folder of another benchmark."""


ERRORS = (*runner.ERRORS, BenchmarkError)


# MARK: Running


def main(arguments: Sequence[str] | None = None) -> int:
    """The benchmark a file and the command line give; returns the exit code."""
    parsed = parse_arguments(sys.argv[1:] if arguments is None else arguments)

    try:
        build = source_tree.use_tree(parsed.build)
        expansion = read_benchmark(parsed)
        output = output_folder(parsed)
        list_runs(expansion, output)
        check_options(expansion)

        if parsed.check == "options":
            return 0

        if parsed.check == "build":
            with tempfile.TemporaryDirectory() as folder:
                check_builds(expansion, write_run_files(Path(folder), expansion), build)

            print("every run built")
            return 0

        start_folder(output, expansion, build, parsed.allow_mixed)
        run_files = write_run_files(output / RUN_FILES_FOLDER, expansion)

        if not parsed.no_check:
            check_builds(expansion, run_files, build)
    except ERRORS as error:
        print(f"benchmark.py: {error}", file=sys.stderr)
        return NOT_STARTED

    return carry_out(expansion, output, run_files, build, parsed.retry_timeouts)


def runner_command(layer: str) -> list[str]:
    """The command that starts a layer's runner, before its flags."""
    return [sys.executable, str(RUNNERS / f"{layer.replace('-', '_')}_runner.py")]


# MARK: Before the runs


def parse_arguments(arguments: Sequence[str]) -> argparse.Namespace:
    """The benchmark's flags, its options, and the run options it puts over every
    run."""
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("file", nargs="?", help="the benchmark file")
    flags = parser.add_argument_group("flags")
    flags.add_argument(
        "--output",
        help="the benchmark folder (default: results/<file name> in the benchmark's folder)",
    )
    flags.add_argument("--build", help="the build folder of this tree to import rcspp from")
    flags.add_argument(
        "--check",
        choices=("options", "build"),
        help="stop after this check: options lists the runs, build builds each one too",
    )
    flags.add_argument(
        "--no-check", action="store_true", help="do not build each run before the first starts"
    )
    flags.add_argument(
        "--allow-mixed",
        action="store_true",
        help="resume although the source, versions or machine changed",
    )
    flags.add_argument(
        "--retry-timeouts",
        action="store_true",
        help="run again the runs killed at their timeout",
    )
    add_benchmark_options(parser)
    add_run_options(parser)
    parsed = parser.parse_args(arguments)

    if parsed.check and parsed.no_check:
        parser.error("--check and --no-check do not go together")

    if parsed.file is None and parsed.output is None:
        parser.error("--output is needed without a benchmark file")

    return parsed


def read_benchmark(arguments: argparse.Namespace) -> Expansion:
    """The runs of the benchmark file (none: one run of the defaults), with the command
    line's options over the file's.

    Raises:
        OptionsError: For wrong options.
    """
    if arguments.file is None:
        benchmark = Benchmark.from_table({}, "command line")
    else:
        path = Path(arguments.file)
        benchmark = Benchmark.from_table(read_toml(path), str(path), path.resolve().parent)

    benchmark = benchmark_from_arguments(benchmark, arguments)
    return benchmark.expand(run_from_arguments(arguments, benchmark.names))


def output_folder(arguments: argparse.Namespace) -> Path:
    if arguments.output is not None:
        return Path(arguments.output)

    return runner.RESULTS / Path(arguments.file).stem


def list_runs(expansion: Expansion, output: Path) -> None:
    """Prints the matrix points dropped, then each run and its folders."""
    for dropped in expansion.dropped:
        print(f"dropped: {dropped}")

    rounds = expansion.repeats
    print(f"{len(expansion.runs)} runs, {rounds} round{'s' * (rounds > 1)}, into {output}:")

    for run in expansion.runs:
        print(f"  {RUNS_FOLDER}/{run.name}-r<1..{rounds}>" if rounds > 1 else f"  {run.name}")


def check_options(expansion: Expansion) -> None:
    """What the options can say without a runner: each instance's file reads, and its
    type makes the run's start.

    Raises:
        InstanceError: When an instance file cannot be read.
        OptionsError: For a start the instance's type does not make.
    """
    read: set[str] = set()

    for run in expansion.runs:
        instance = run.instance.to_table()
        start = run.start.type

        if run.instance.name not in read:
            make_instance(instance)
            read.add(run.instance.name)

        if start not in LAYER_STARTS and start not in INSTANCE_TYPES[run.instance.type].STARTS:
            raise OptionsError(
                f"{run.name}: instance type {run.instance.type} has no {start} start"
            )


def benchmark_table(expansion: Expansion) -> dict[str, Any]:
    """``benchmark.toml``: the benchmark's options and every run, expanded and named."""
    options = {
        "processes": expansion.processes,
        "repeats": expansion.repeats,
        "timeout": expansion.timeout,
        "machine": expansion.machine,
        "logs": expansion.logs,
    }
    return {
        "benchmark": {name: value for name, value in options.items() if value is not None},
        "runs": [{"name": run.name, **run.to_table()} for run in expansion.runs],
    }


def benchmark_record(expansion: Expansion, build: Path) -> dict[str, Any]:
    """The record the benchmark's runs share: their master solvers' packages imported
    first, for their versions."""
    masters = {run.master.type for run in expansion.runs}

    for package in ("highspy", "gurobipy"):
        if package in masters:
            try:
                __import__(package)
            except ImportError:
                pass

    return make_record(build, expansion.machine, processes=expansion.processes)


def differences(old: Mapping[str, Any], new: Mapping[str, Any], prefix: str = "") -> list[str]:
    """The dotted names whose values differ between two records."""
    found: list[str] = []

    for name in sorted(set(old) | set(new)):
        before, after = old.get(name), new.get(name)

        if isinstance(before, Mapping) and isinstance(after, Mapping):
            found += differences(before, after, f"{prefix}{name}.")  # type: ignore[arg-type]
        elif before != after:
            found.append(f"{prefix}{name}: {before!r} -> {after!r}")

    return found


def start_folder(output: Path, expansion: Expansion, build: Path, allow_mixed: bool) -> None:
    """The benchmark folder, made or resumed: ``benchmark.toml`` and ``metadata.json``.

    Raises:
        BenchmarkError: When the folder holds another benchmark's runs, or was started on
            another source, versions or machine and `allow_mixed` is not set.
    """
    import tomli_w

    table = benchmark_table(expansion)
    record = benchmark_record(expansion, build)
    metadata_path = output / METADATA_FILE
    metadata: dict[str, Any] = {}

    if (output / BENCHMARK_FILE).is_file():
        if read_toml(output / BENCHMARK_FILE).get("runs") != table["runs"]:
            raise BenchmarkError(f"{output} holds another benchmark's runs: choose another folder")

        metadata = run_folder.read_json(metadata_path) if metadata_path.is_file() else {}
        changed = differences(metadata.get("record", {}), record)

        if changed and not allow_mixed:
            raise BenchmarkError(
                f"{output} was started with another source, versions or machine "
                f"({'; '.join(changed)}): resume with --allow-mixed to mix them"
            )

        if changed:
            metadata.setdefault("other_records", []).append(record)

        metadata.setdefault("resumed", []).append(runner.now())
    else:
        metadata = {"start": runner.now(), "record": record}

    metadata.update(
        processes=expansion.processes,
        repeats=expansion.repeats,
        runs=len(expansion.runs),
        rounds=expansion.repeats,
        dropped=expansion.dropped,
    )
    output.mkdir(parents=True, exist_ok=True)
    run_folder.write_atomically(output / BENCHMARK_FILE, tomli_w.dumps(table))
    run_folder.write_json(metadata_path, metadata)


def write_run_files(folder: Path, expansion: Expansion) -> dict[str, Path]:
    """One run file per run, which its runner reads with ``--file``; by run name."""
    import tomli_w

    folder.mkdir(parents=True, exist_ok=True)
    paths: dict[str, Path] = {}

    for run in expansion.runs:
        path = folder / f"{run.name}.toml"
        run_folder.write_atomically(path, tomli_w.dumps(run.to_table()))
        paths[run.name] = path

    return paths


def check_builds(expansion: Expansion, run_files: Mapping[str, Path], build: Path) -> None:
    """Each run built by its runner without solving, ``processes`` at a time.

    Raises:
        BenchmarkError: When a runner cannot build its run; with each one's message.
    """

    def check(run: Run) -> str | None:
        command = [
            *runner_command(run.layer),
            *("--check", "build", "--file", str(run_files[run.name]), "--build", str(build)),
        ]
        done = subprocess.run(  # nosec B603: this folder's runners
            command, capture_output=True, text=True, stdin=subprocess.DEVNULL
        )
        return None if done.returncode == 0 else f"{run.name}: {done.stderr.strip()}"

    with ThreadPoolExecutor(max_workers=expansion.processes) as pool:
        failed = [message for message in pool.map(check, expansion.runs) if message]

    if failed:
        raise BenchmarkError("the build check failed:\n  " + "\n  ".join(failed))


# MARK: The runs


@dataclass
class Running:
    """A run in progress: its process, and its load so far."""

    run: Run
    folder: Path
    process: subprocess.Popen[bytes]
    tree: psutil.Process
    log: IO[bytes] | None
    started: float
    started_at: str
    load: Load = field(default_factory=Load)
    processor_seconds: float = 0.0

    def read_processor_seconds(self) -> None:
        seconds = tree_processor_seconds(self.tree)

        if seconds is not None:
            self.processor_seconds = seconds


@dataclass
class Tally:
    """The runs' outcomes so far, finished ones from before a resume included."""

    total: int
    finished: int = 0
    timeouts: int = 0
    failed_checks: int = 0
    crashed: int = 0

    def add(self, outcome: str, result: Mapping[str, Any] | None) -> None:
        if result is not None and result.get("status") == run_folder.TIMEOUT:
            self.timeouts += 1
            self.finished += 1
        elif outcome in (FINISHED, CHECK_FAILED):
            self.finished += 1
            self.failed_checks += outcome == CHECK_FAILED
        else:
            self.crashed += 1


def carry_out(
    expansion: Expansion,
    output: Path,
    run_files: Mapping[str, Path],
    build: Path,
    retry_timeouts: bool,
) -> int:
    """Every run of every round not yet finished, ``processes`` at a time; returns the
    benchmark's exit code."""
    rounds = expansion.rounds()
    tally = Tally(total=len(rounds))
    waiting: deque[tuple[Run, Path]] = deque()

    for number, run in rounds:
        folder = output / RUNS_FOLDER / folder_name(run, number)
        result = run_folder.read_result(folder)

        if result is not None and retry_timeouts and result["status"] == run_folder.TIMEOUT:
            shutil.rmtree(folder)
            result = None

        if result is None:
            waiting.append((run, folder))
        else:
            tally.add(CHECK_FAILED if result["solution_check"] == "failed" else FINISHED, result)

    running: list[Running] = []
    stopped = False
    last_sample = time.monotonic()
    write_status(output, tally, running, len(waiting))

    try:
        while running or (waiting and not stopped):
            if not stopped and (output / STOP_FILE).exists():
                stopped = True
                print(f"{STOP_FILE}: the runs in progress end, no other starts")

            while not stopped and waiting and len(running) < expansion.processes:
                run, folder = waiting.popleft()
                command = [*runner_command(run.layer), "--file", str(run_files[run.name])]
                command += ["--output", str(folder), "--build", str(build)]
                command += ["--machine", expansion.machine] if expansion.machine else []
                running.append(start_run(run, folder, command, expansion.logs))
                write_status(output, tally, running, len(waiting))

            sample = None

            if time.monotonic() - last_sample >= INTERVAL_SECONDS:
                sample, last_sample = take_sample(), time.monotonic()

            for current in list(running):
                current.read_processor_seconds()
                exit_code = current.process.poll()
                timed_out = (
                    exit_code is None
                    and expansion.timeout is not None
                    and time.monotonic() - current.started > expansion.timeout
                )

                if exit_code is None and not timed_out:
                    if sample is not None:
                        current.load.add(sample, current.processor_seconds)

                    continue

                if timed_out:
                    kill(current)

                outcome, result = finish_run(current, exit_code, run_files, expansion.processes)
                tally.add(outcome, result)
                running.remove(current)
                print(f"{current.folder.name}: {outcome}")
                write_status(output, tally, running, len(waiting))

            time.sleep(POLL_SECONDS)
    except KeyboardInterrupt:
        for current in running:
            kill(current)
            run_folder.update_metadata(current.folder, {"outcome": "interrupted"})

        write_status(output, tally, running, len(waiting), "interrupted")
        print("interrupted: run the benchmark again to resume", file=sys.stderr)
        return INTERRUPTED

    run_folder.update_metadata(output, {"end": runner.now()})

    if stopped:
        (output / STOP_FILE).unlink(missing_ok=True)
        write_status(output, tally, running, len(waiting), "stopped")
        print(f"stopped, {len(waiting)} runs left: run the benchmark again to resume")
        return STOPPED

    write_status(output, tally, running, 0, "finished")
    problems = tally.timeouts + tally.failed_checks + tally.crashed
    return RUNS_FAILED if problems else 0


def start_run(run: Run, folder: Path, command: list[str], logs: bool) -> Running:
    """Starts a run's runner into `folder`, removing what an unfinished start left
    there."""
    import psutil

    if folder.exists():
        shutil.rmtree(folder)

    folder.mkdir(parents=True)
    log = open(folder / run_folder.LOG_FILE, "wb") if logs else None  # noqa: SIM115
    # Its own process group: Ctrl-C reaches the benchmark, which ends the runs itself.
    group = (
        {"creationflags": subprocess.CREATE_NEW_PROCESS_GROUP}
        if sys.platform == "win32"
        else {"start_new_session": True}
    )
    sample = take_sample()
    process = subprocess.Popen(  # nosec B603: this folder's runners
        command,
        stdin=subprocess.DEVNULL,
        stdout=log if log is not None else subprocess.DEVNULL,
        stderr=subprocess.STDOUT,
        **group,  # type: ignore[arg-type]
    )
    current = Running(
        run=run,
        folder=folder,
        process=process,
        tree=psutil.Process(process.pid),
        log=log,
        started=time.monotonic(),
        started_at=runner.now(),
    )
    current.load.add(sample, 0.0)
    return current


def kill(current: Running) -> None:
    """Kills a run's process and its children."""
    import psutil

    members = [current.tree]

    try:
        members += current.tree.children(recursive=True)
    except psutil.Error:
        pass

    for member in members:
        try:
            member.kill()
        except psutil.Error:
            pass

    current.process.wait()


def finish_run(
    current: Running, exit_code: int | None, run_files: Mapping[str, Path], processes: int
) -> tuple[str, dict[str, Any] | None]:
    """Records how a run ended in its ``metadata.json``; for a run killed at its timeout
    (`exit_code` None), writes its ``result.json`` too.

    Returns the outcome and the result.
    """
    seconds = time.monotonic() - current.started
    current.read_processor_seconds()
    current.load.add(take_sample(), current.processor_seconds)

    if current.log is not None:
        current.log.close()

    folder = current.folder
    result = run_folder.read_result(folder)

    if exit_code is None and result is None:
        # Killed at its timeout: its run file, so that it can run again from its folder.
        if not (folder / run_folder.RUN_FILE).exists():
            shutil.copyfile(run_files[current.run.name], folder / run_folder.RUN_FILE)

        result = run_folder.timeout_result(seconds)
        run_folder.write_result(folder, result)

    outcome = outcome_of(exit_code, result)
    metadata_path = folder / run_folder.METADATA_FILE
    metadata = run_folder.read_json(metadata_path) if metadata_path.is_file() else {}
    run_folder.update_metadata(
        folder,
        {
            "exit_code": exit_code,
            "outcome": outcome,
            "started": current.started_at,
            "wall_seconds": seconds,
            "benchmark": {**metadata.get("benchmark", {}), "processes": processes},
            "load": current.load.summary(),
        },
    )
    return outcome, result


def outcome_of(exit_code: int | None, result: Mapping[str, Any] | None) -> str:
    """How a run ended, from its runner's exit code (None: killed) and its result."""
    if result is not None and result["status"] == run_folder.TIMEOUT:
        return run_folder.TIMEOUT

    if result is not None and exit_code in (0, None):
        return FINISHED

    if result is not None and exit_code == 1 and result.get("solution_check") == "failed":
        return CHECK_FAILED

    return RUN_NOT_STARTED if exit_code == 2 else CRASHED


def write_status(
    output: Path, tally: Tally, running: Sequence[Running], waiting: int, end: str | None = None
) -> None:
    """``status.txt``: how many runs are done and how, then each run in progress."""
    lines = [
        f"{runner.now()}  {tally.finished} / {tally.total} runs finished ({tally.timeouts} "
        f"timeout, {tally.failed_checks} check failed), {tally.crashed} crashed or not started, "
        f"{len(running)} running, {waiting} waiting"
    ]
    now = time.monotonic()
    lines += [
        f"{current.folder.name}  started {current.started_at}, {now - current.started:.1f} s"
        for current in running
    ]

    if end is not None:
        lines.append(f"{end} at {runner.now()}")

    run_folder.write_atomically(output / STATUS_FILE, "\n".join(lines) + "\n")


if __name__ == "__main__":
    sys.exit(main())
