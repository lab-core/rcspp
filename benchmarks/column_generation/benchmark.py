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
stops it once the runs in progress end. Ctrl-C, or an error of the benchmark itself, kills
the runs in progress.
"""

from __future__ import annotations

import argparse
import contextlib
import shutil
import subprocess  # nosec B404: the runners, this folder's own scripts
import sys
import tempfile
import threading
import time
import traceback
from collections import deque
from collections.abc import Mapping, Sequence
from concurrent.futures import ThreadPoolExecutor, wait
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
NOT_STARTED = 2  # wrong options, a failed build check, a folder refused
STOPPED = 3  # a STOP file stopped it before the end
FAILED = 4  # an error of the benchmark itself
INTERRUPTED = 130  # Ctrl-C

#: A run's outcome in its ``metadata.json``.
FINISHED = "finished"
CHECK_FAILED = "check failed"
CRASHED = "crashed"
RUN_NOT_STARTED = "not started"
RUN_INTERRUPTED = "interrupted"


class BenchmarkError(Exception):
    """A benchmark that cannot start: a failed build check, a folder refused."""


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

        if parsed.check is not None:
            print_finished(expansion, output)

        if parsed.check == "options":
            return 0

        # Refused folders and failed checks leave the folder as it was.
        metadata, opening = open_folder(output, expansion, build, parsed)

        if parsed.check == "build" or not parsed.no_check:
            with tempfile.TemporaryDirectory() as folder:
                check_builds(expansion, write_run_files(Path(folder), expansion), build)

        if parsed.check == "build":
            print("every run built")
            return 0

        write_folder(output, expansion, metadata)
        run_files = write_run_files(output / RUN_FILES_FOLDER, expansion)
    except ERRORS as error:
        print(f"benchmark.py: {error}", file=sys.stderr)
        return NOT_STARTED
    except KeyboardInterrupt:
        print("interrupted before the first run", file=sys.stderr)
        return INTERRUPTED
    except Exception:
        traceback.print_exc()
        return FAILED

    return carry_out(expansion, output, run_files, build, opening)


def runner_command(layer: str) -> list[str]:
    """The command that starts a layer's runner, before its flags."""
    return [sys.executable, str(RUNNERS / f"{layer.replace('-', '_')}_runner.py")]


def own_group() -> dict[str, Any]:
    """Popen's arguments that start a process in its own group: Ctrl-C then reaches the
    benchmark alone, which ends its runners itself."""
    if sys.platform == "win32":
        return {"creationflags": subprocess.CREATE_NEW_PROCESS_GROUP}

    return {"start_new_session": True}


def kill_tree(process: psutil.Process) -> None:
    """Kills a process and its children; a runner started from a virtual environment's
    Python is two processes, the launcher and the interpreter."""
    import psutil

    members = [process]

    try:
        members += process.children(recursive=True)
    except psutil.Error:
        pass

    for member in members:
        try:
            member.kill()
        except psutil.Error:
            pass


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


def run_folders(expansion: Expansion, output: Path) -> list[tuple[Run, Path]]:
    """Every run of every round, with its folder."""
    return [
        (run, output / RUNS_FOLDER / folder_name(run, number)) for number, run in expansion.rounds()
    ]


def print_finished(expansion: Expansion, output: Path) -> None:
    """Prints how many of the runs the folder already holds finished."""
    if (output / BENCHMARK_FILE).is_file():
        folders = run_folders(expansion, output)
        finished = sum(run_folder.read_result(folder) is not None for _, folder in folders)
        print(f"{output} already holds {finished} / {len(folders)} finished runs")


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


@dataclass
class Opening:
    """The event that opens a start: ``started`` for a new folder, ``resumed`` for one
    that holds these runs; with the flags given and, on a resume, the benchmark options
    changed since the folder's last start, as ``[before, after]``."""

    event: str
    retry_timeouts: bool
    allow_mixed: bool
    changed: dict[str, list[Any]] = field(default_factory=dict[str, list[Any]])

    def details(self) -> dict[str, Any]:
        """The fields the event writes: the flags only when given, the changes only when
        some."""
        details: dict[str, Any] = {}

        if self.retry_timeouts:
            details["retry_timeouts"] = True

        if self.allow_mixed:
            details["allow_mixed"] = True

        if self.changed:
            details["changed"] = self.changed

        return details


def open_folder(
    output: Path, expansion: Expansion, build: Path, arguments: argparse.Namespace
) -> tuple[dict[str, Any], Opening]:
    """The benchmark folder's ``metadata.json`` to write, and the event that opens this
    start. Writes nothing.

    Raises:
        BenchmarkError: When the folder holds other runs, or was started on another
            source, versions or machine and ``--allow-mixed`` is not given.
    """
    table = benchmark_table(expansion)
    record = benchmark_record(expansion, build)
    opening = Opening("started", arguments.retry_timeouts, arguments.allow_mixed)

    if not (output / BENCHMARK_FILE).is_file():
        return {"record": record, "events": []}, opening

    old = read_toml(output / BENCHMARK_FILE)

    if old.get("runs") != table["runs"]:
        raise BenchmarkError(other_runs(output, old, table))

    path = output / METADATA_FILE
    metadata: dict[str, Any] = run_folder.read_json(path) if path.is_file() else {"record": record}
    changed_record = differences(metadata.get("record", {}), record)

    if changed_record and not arguments.allow_mixed:
        raise BenchmarkError(
            f"{output} was started with another source, versions or machine "
            f"({'; '.join(changed_record)}): resume with --allow-mixed to mix them"
        )

    if changed_record:
        metadata.setdefault("other_records", []).append(record)

    opening.event = "resumed"
    before, after = old.get("benchmark", {}), table["benchmark"]
    opening.changed = {
        name: [before.get(name), after.get(name)]
        for name in sorted(set(before) | set(after))
        if before.get(name) != after.get(name)
    }
    return metadata, opening


def other_runs(output: Path, old: Mapping[str, Any], table: Mapping[str, Any]) -> str:
    """Why a folder's runs are not the benchmark's: their threads only, as the number of
    processes changed, or the runs added and removed."""

    def without_threads(runs: list[dict[str, Any]]) -> list[dict[str, Any]]:
        return [
            {
                **{name: value for name, value in run.items() if name != "name"},
                "column_generation": {
                    name: value
                    for name, value in run.get("column_generation", {}).items()
                    if name != "threads"
                },
            }
            for run in runs
        ]

    old_runs: list[dict[str, Any]] = old.get("runs", [])
    new_runs: list[dict[str, Any]] = table["runs"]

    if without_threads(old_runs) == without_threads(new_runs):
        before = old.get("benchmark", {}).get("processes", 1)
        after = table["benchmark"]["processes"]
        return (
            f"{output} holds these runs as run with {before} processes: with more than one, "
            "every run that leaves column_generation.threads unset gets 1, so with "
            f"{after} they are other runs; resume with --processes {before}, or choose "
            "another folder"
        )

    def some(names: list[str]) -> str:
        return ", ".join(names[:3]) + (", ..." if len(names) > 3 else "")

    old_names = [run["name"] for run in old_runs]
    new_names = [run["name"] for run in new_runs]
    added = [name for name in new_names if name not in old_names]
    removed = [name for name in old_names if name not in new_names]
    parts = [f"{len(added)} added: {some(added)}"] if added else []
    parts += [f"{len(removed)} removed: {some(removed)}"] if removed else []
    found = "; ".join(parts) or "the same names with other options"
    return f"{output} holds another benchmark's runs ({found}): choose another folder"


def write_folder(output: Path, expansion: Expansion, metadata: dict[str, Any]) -> None:
    """The benchmark folder's ``benchmark.toml`` and ``metadata.json``."""
    import tomli_w

    metadata.update(
        processes=expansion.processes,
        runs=len(expansion.runs),
        rounds=expansion.repeats,
        dropped=expansion.dropped,
    )
    output.mkdir(parents=True, exist_ok=True)
    table = benchmark_table(expansion)
    run_folder.write_atomically(output / BENCHMARK_FILE, tomli_w.dumps(table))
    run_folder.write_json(output / METADATA_FILE, metadata)


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
    """Each run built by its runner without solving, ``processes`` at a time. On Ctrl-C
    or an error, the runners still building are killed.

    Raises:
        BenchmarkError: When a runner cannot build its run; with each one's message.
    """
    import psutil

    started: list[subprocess.Popen[str]] = []
    lock = threading.Lock()
    stopping = threading.Event()

    def check(run: Run) -> str | None:
        command = [
            *runner_command(run.layer),
            *("--check", "build", "--file", str(run_files[run.name]), "--build", str(build)),
        ]

        with lock:
            if stopping.is_set():
                return None

            process = subprocess.Popen(  # nosec B603: this folder's runners
                command,
                stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                **own_group(),
            )
            started.append(process)

        _, errors = process.communicate()
        return None if process.returncode == 0 else f"{run.name}: {errors.strip()}"

    pool = ThreadPoolExecutor(max_workers=expansion.processes)
    futures = [pool.submit(check, run) for run in expansion.runs]

    try:
        # A little at a time, so that Ctrl-C reaches this thread.
        while wait(futures, timeout=POLL_SECONDS).not_done:
            pass

        failed = [message for future in futures if (message := future.result())]
    except BaseException:
        with lock:
            stopping.set()

        for process in started:
            with contextlib.suppress(psutil.Error):
                kill_tree(psutil.Process(process.pid))

        raise
    finally:
        pool.shutdown(wait=True, cancel_futures=True)

    if failed:
        raise BenchmarkError("the build check failed:\n  " + "\n  ".join(failed))


# MARK: The runs


@dataclass
class Running:
    """A run in progress: its process, and its load so far."""

    run: Run
    folder: Path
    process: subprocess.Popen[Any]
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

    def counts(self) -> dict[str, int]:
        return {
            "total": self.total,
            "finished": self.finished,
            "timeouts": self.timeouts,
            "failed_checks": self.failed_checks,
            "crashed": self.crashed,
        }

    def line(self, running: int, waiting: int) -> str:
        return (
            f"{self.finished} / {self.total} runs finished ({self.timeouts} timeout, "
            f"{self.failed_checks} check failed), {self.crashed} crashed or not started, "
            f"{running} running, {waiting} waiting"
        )


def add_event(output: Path, event: str, **details: Any) -> None:
    """Appends an event to the benchmark's ``metadata.json``: started, resumed, stopped,
    interrupted or finished."""
    path = output / METADATA_FILE
    metadata = run_folder.read_json(path)
    metadata.setdefault("events", []).append({"time": runner.now(), "event": event, **details})
    run_folder.write_json(path, metadata)


def carry_out(
    expansion: Expansion,
    output: Path,
    run_files: Mapping[str, Path],
    build: Path,
    opening: Opening,
) -> int:
    """Every run of every round not yet finished, ``processes`` at a time; returns the
    benchmark's exit code."""
    folders = run_folders(expansion, output)
    tally = Tally(total=len(folders))
    waiting: deque[tuple[Run, Path]] = deque()
    cut_off = retried = 0

    for run, folder in folders:
        result = run_folder.read_result(folder)

        if result is not None and opening.retry_timeouts and result["status"] == run_folder.TIMEOUT:
            shutil.rmtree(folder)
            result = None
            retried += 1

        if result is None:
            waiting.append((run, folder))
            cut_off += folder.exists()
        else:
            tally.add(CHECK_FAILED if result["solution_check"] == "failed" else FINISHED, result)

    print_opening(output, opening.event, tally, len(waiting), cut_off, retried)
    add_event(
        output,
        opening.event,
        total=tally.total,
        finished=tally.finished,
        to_run=len(waiting),
        cut_off=cut_off,
        timeouts_to_run_again=retried,
        **opening.details(),
    )
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
                write_status(output, tally, running, len(waiting))

            for current in list(running):
                current.read_processor_seconds()
                exit_code = current.process.poll()
                seconds = time.monotonic() - current.started
                timed_out = (
                    exit_code is None
                    and expansion.timeout is not None
                    and seconds > expansion.timeout
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
                ended = tally.finished + tally.crashed
                print(f"[{ended}/{tally.total}] {current.folder.name}: {outcome}, {seconds:.1f} s")
                write_status(output, tally, running, len(waiting))

            time.sleep(POLL_SECONDS)
    except BaseException as error:
        return interrupt(output, tally, running, len(waiting), error)

    if stopped:
        (output / STOP_FILE).unlink(missing_ok=True)
        add_event(output, "stopped", runs_left=len(waiting), **tally.counts())
        write_status(output, tally, running, len(waiting), "stopped")
        print(f"stopped, {len(waiting)} runs left: run the benchmark again to resume")
        return STOPPED

    add_event(output, "finished", **tally.counts())
    write_status(output, tally, running, 0, "finished")
    print(tally.line(0, 0))
    problems = tally.timeouts + tally.failed_checks + tally.crashed
    return RUNS_FAILED if problems else 0


def print_opening(
    output: Path, event: str, tally: Tally, to_run: int, cut_off: int, retried: int
) -> None:
    """Prints what this start of the benchmark has to do."""
    if event == "started":
        print(f"starting {output}: {to_run} runs to run")
        return

    done = f"{tally.finished} / {tally.total} finished"
    done += f" ({tally.timeouts} timeout, {tally.failed_checks} check failed)"
    notes = [f"{cut_off} cut off, removed first"] if cut_off else []
    notes += [f"{retried} timeouts to run again"] if retried else []
    left = f"{to_run} to run" + (f" ({'; '.join(notes)})" if notes else "")
    print(f"resuming {output}: {done}, {left}")


def interrupt(
    output: Path, tally: Tally, running: list[Running], waiting: int, error: BaseException
) -> int:
    """Ends a benchmark that Ctrl-C or an error stopped: kills the runs in progress
    first, then records them as interrupted, as far as it can."""
    for current in running:
        kill(current)

    killed = [current.folder.name for current in running]
    reason = (
        "Ctrl-C" if isinstance(error, KeyboardInterrupt) else f"{type(error).__name__}: {error}"
    )

    with contextlib.suppress(OSError):
        for current in running:
            run_folder.update_metadata(current.folder, {"outcome": RUN_INTERRUPTED})

        add_event(output, "interrupted", reason=reason, killed=killed, **tally.counts())
        write_status(output, tally, [], waiting + len(running), "interrupted")

    if not isinstance(error, KeyboardInterrupt):
        traceback.print_exception(error)

    print(f"interrupted ({reason}): run the benchmark again to resume", file=sys.stderr)
    return INTERRUPTED if isinstance(error, KeyboardInterrupt) else FAILED


def start_run(run: Run, folder: Path, command: list[str], logs: bool) -> Running:
    """Starts a run's runner into `folder`, removing what an unfinished start left
    there."""
    import psutil

    if folder.exists():
        shutil.rmtree(folder)

    folder.mkdir(parents=True)
    log = open(folder / run_folder.LOG_FILE, "wb") if logs else None  # noqa: SIM115
    sample = take_sample()
    process = subprocess.Popen(  # nosec B603: this folder's runners
        command,
        stdin=subprocess.DEVNULL,
        stdout=log if log is not None else subprocess.DEVNULL,
        stderr=subprocess.STDOUT,
        **own_group(),
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
    """Kills a run's process tree, then waits for it and closes its log."""
    kill_tree(current.tree)
    current.process.wait()

    if current.log is not None:
        current.log.close()


def finish_run(
    current: Running, exit_code: int | None, run_files: Mapping[str, Path], processes: int
) -> tuple[str, dict[str, Any] | None]:
    """Records how a run ended in its ``metadata.json``; for a run killed at its timeout
    (`exit_code` None), writes its ``result.json`` too.

    Returns the outcome and the result.
    """
    seconds = time.monotonic() - current.started
    folder = current.folder
    metadata_path = folder / run_folder.METADATA_FILE
    metadata = run_folder.read_json(metadata_path) if metadata_path.is_file() else {}
    # The runner's own count: its interpreter has ended, and with it what psutil can read.
    current.read_processor_seconds()
    reported = metadata.get("processor_seconds") or 0.0
    current.load.add(take_sample(), max(current.processor_seconds, reported))

    if current.log is not None:
        current.log.close()

    result = run_folder.read_result(folder)

    if exit_code is None and result is None:
        # Killed at its timeout: its run file, so that it can run again from its folder.
        if not (folder / run_folder.RUN_FILE).exists():
            shutil.copyfile(run_files[current.run.name], folder / run_folder.RUN_FILE)

        result = run_folder.timeout_result(seconds)
        run_folder.write_result(folder, result)

    outcome = outcome_of(exit_code, result)
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
    """``status.txt``: how many runs are done and how, then each run in progress.

    Only
    for reading: when the file cannot be written, the benchmark goes on without it.
    """
    now = time.monotonic()
    lines = [f"{runner.now()}  {tally.line(len(running), waiting)}"]
    lines += [
        f"{current.folder.name}  started {current.started_at}, {now - current.started:.1f} s"
        for current in running
    ]

    if end is not None:
        lines.append(f"{end} at {runner.now()}")

    with contextlib.suppress(OSError):
        run_folder.write_atomically(output / STATUS_FILE, "\n".join(lines) + "\n")


if __name__ == "__main__":
    sys.exit(main())
