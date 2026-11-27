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
Run again on the same folder, it resumes: a round with a ``result.json`` is kept, any other
is removed and started again. A timeout and a crash after its result are kept too, unless
``--retry-timeouts`` or ``--retry-crashes`` runs them again. One benchmark at a time works on
a folder (its ``.lock``). A ``STOP`` file in the folder stops the starts once the runs in
progress end. Ctrl-C, SIGTERM, SIGHUP, or an error of the benchmark itself, kills the runs in
progress.

Exit codes: 0 every run finished and passed its check; 1 a run timed out, crashed, did not
start or failed its check; 2 the benchmark could not start; 3 a STOP file stopped it; 4 an
error of the benchmark itself; 130 Ctrl-C, SIGTERM or SIGHUP.
"""

from __future__ import annotations

import argparse
import contextlib
import json
import os
import shutil
import signal
import subprocess  # nosec B404: the runners, this folder's own scripts
import sys
import tempfile
import threading
import time
import traceback
from collections import deque
from collections.abc import Generator, Mapping, Sequence
from concurrent.futures import ThreadPoolExecutor, wait
from dataclasses import dataclass, field
from pathlib import Path
from typing import IO, Any

import psutil
import tomli_w

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
    STOPS_WITHOUT_FLEET,
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
    stops_without_fleet,
)
from column_generation_benchmark.record import make_record  # noqa: E402
from column_generation_benchmark.run_folder import BENCHMARK_FILE, RUNS_FOLDER  # noqa: E402
from column_generation_benchmark.summary import write_summary  # noqa: E402

#: The runners, one per layer: ``<layer>_runner.py``.
RUNNERS = Path(__file__).resolve().parent / "runners"

METADATA_FILE = "metadata.json"
STATUS_FILE = "status.txt"
STOP_FILE = "STOP"
LOCK_FILE = ".lock"
RUN_FILES_FOLDER = "run_files"

#: The starts every layer makes; the others come from the instance's type.
LAYER_STARTS = ("artificial",)

#: The seconds between two looks at the runs in progress.
POLL_SECONDS = 0.05

#: The most seconds to wait for a killed run's process.
KILL_WAIT_SECONDS = 5.0

#: The signals that end the benchmark as Ctrl-C does, where the platform has them.
ENDING_SIGNALS = ("SIGTERM", "SIGHUP")

#: The benchmark's exit codes besides 0, when every run finished and passed its check.
RUNS_FAILED = 1  # a run timed out, crashed, did not start, or failed its check
NOT_STARTED = 2  # wrong options, a failed build check, a folder refused
STOPPED = 3  # a STOP file stopped it before the end
FAILED = 4  # an error of the benchmark itself
INTERRUPTED = 130  # Ctrl-C, SIGTERM or SIGHUP


class BenchmarkError(Exception):
    """A benchmark that cannot start: a failed build check, a folder refused or in use."""


class Terminated(BaseException):
    """SIGTERM or SIGHUP, named by the exception's text: the benchmark ends as on
    Ctrl-C."""


ERRORS = (*runner.ERRORS, BenchmarkError)


# MARK: Running


def main(arguments: Sequence[str] | None = None) -> int:
    """Runs the benchmark its file and the command line give; returns the exit code."""
    parsed = parse_arguments(sys.argv[1:] if arguments is None else arguments)

    with ending_signals():
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

            lock = take_lock(output)
        except ERRORS as error:
            print(f"benchmark.py: {error}", file=sys.stderr)
            return NOT_STARTED
        except (KeyboardInterrupt, Terminated):
            print("interrupted before the first run", file=sys.stderr)
            return INTERRUPTED
        except Exception:
            traceback.print_exc()
            return FAILED

        try:
            exit_code = carry_out(expansion, output, build, opening, metadata)
            summarize(output)
        finally:
            lock.unlink(missing_ok=True)

    return exit_code


def summarize(output: Path) -> None:
    """Writes the folder's summary; a failure only warns: the runs are recorded."""
    try:
        written = write_summary(output)
    except Exception as error:  # the summary's own failure must not hide the runs'
        print(f"benchmark.py: no summary: {error}", file=sys.stderr)
        return

    print(f"summary: {written[0]}")


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


@contextlib.contextmanager
def ending_signals() -> Generator[None]:
    """SIGTERM and SIGHUP (a scheduler's end, ``timeout``, ``kill``, a closed terminal) end
    the benchmark as Ctrl-C does: its runners are in their own sessions, so they would
    otherwise go on without it. The handlers before are put back after."""

    def end(number: int, frame: Any) -> None:
        raise Terminated(signal.Signals(number).name)

    handlers: dict[int, Any] = {}

    for number in ending_signal_numbers():
        # Only the main thread can set a handler.
        with contextlib.suppress(ValueError):
            handlers[number] = signal.signal(number, end)

    try:
        yield
    finally:
        restore_signals(handlers)


def ending_signal_numbers() -> list[int]:
    return [getattr(signal, name) for name in ENDING_SIGNALS if hasattr(signal, name)]


def ignore_signals() -> dict[int, Any]:
    """Ignores Ctrl-C and the ending signals; returns the handlers to put back."""
    handlers: dict[int, Any] = {}

    for number in (signal.SIGINT, *ending_signal_numbers()):
        with contextlib.suppress(ValueError):
            handlers[number] = signal.signal(number, signal.SIG_IGN)

    return handlers


def restore_signals(handlers: Mapping[int, Any]) -> None:
    for number, handler in handlers.items():
        with contextlib.suppress(ValueError):
            signal.signal(number, handler)


def take_lock(output: Path) -> Path:
    """Takes the folder's lock, ``.lock``: this process's id and start time, so that no
    other benchmark works on the folder at once. A lock whose process has ended is taken
    over; the caller removes the lock at its end.

    Raises:
        BenchmarkError: When a running benchmark holds the lock.
    """
    output.mkdir(parents=True, exist_ok=True)
    path = output / LOCK_FILE
    me = psutil.Process()
    text = json.dumps({"pid": me.pid, "created": me.create_time()})

    # Twice at most: once more after removing a lock whose process has ended.
    for _ in range(2):
        try:
            with open(path, "x", encoding="utf-8") as file:
                file.write(text)

            return path
        except FileExistsError:
            holder = read_lock(path)

            if holder is not None and holds(holder):
                raise BenchmarkError(
                    f"{output} is in use by another benchmark (process {holder['pid']}): wait "
                    f"for it to end, or stop it; remove {LOCK_FILE} only if no benchmark runs"
                ) from None

            path.unlink(missing_ok=True)

    raise BenchmarkError(f"{output}: its {LOCK_FILE} could not be taken")


def read_lock(path: Path) -> dict[str, Any] | None:
    try:
        holder: dict[str, Any] = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None

    return holder


def holds(holder: Mapping[str, Any]) -> bool:
    """Whether the process a lock names still runs: the same id and the same start
    time, as an id can be given again to a later process."""
    try:
        return bool(psutil.Process(holder["pid"]).create_time() == holder["created"])
    except (psutil.Error, KeyError, TypeError, ValueError):
        return False


# MARK: Before the runs


def parse_arguments(arguments: Sequence[str]) -> argparse.Namespace:
    """The benchmark's flags, its options, and the run options it puts over every
    run."""
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        allow_abbrev=False,
    )
    parser.add_argument("file", nargs="?", help="the benchmark file")
    flags = parser.add_argument_group("flags")
    flags.add_argument(
        "--output",
        help="the benchmark folder (default: results/<file name> in the benchmark's folder)",
    )
    flags.add_argument(
        "--build",
        help="a build of this tree to import rcspp from (relative to the tree's root)",
    )
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
        help="resume although the record changed: the source (commit, changes), the build, "
        "the versions or the machine",
    )
    flags.add_argument(
        "--retry-timeouts",
        action="store_true",
        help="run again the runs killed at their timeout",
    )
    flags.add_argument(
        "--retry-crashes",
        action="store_true",
        help="run again the runs that crashed or did not start after writing a result",
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

    # Once for the benchmark: each run's own runner warns in its log too.
    without_fleet = [run.name for run in expansion.runs if stops_without_fleet(run)]

    if without_fleet:
        count = len(without_fleet)
        print(
            f"warning: {count} run{'s' * (count > 1)} set a column generation stop "
            f"({', '.join(without_fleet)}); {STOPS_WITHOUT_FLEET}",
            file=sys.stderr,
        )

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
    """What differs between two records, as ``name: before -> after``, the name
    dotted."""
    found: list[str] = []

    for name in sorted(set(old) | set(new)):
        before, after = old.get(name), new.get(name)

        if isinstance(before, Mapping) and isinstance(after, Mapping):
            found += differences(before, after, f"{prefix}{name}.")  # type: ignore[arg-type]
        elif before != after:
            found.append(f"{prefix}{name}: {before!r} -> {after!r}")

    return found


@dataclass(frozen=True)
class Opening:
    """The event that opens a start: ``started`` for a new folder, ``resumed`` for one
    that holds these runs; with the flags given and, on a resume, the benchmark options
    changed since the folder's last start, as ``[before, after]``."""

    event: str
    retry_timeouts: bool
    retry_crashes: bool
    allow_mixed: bool
    changed: dict[str, list[Any]] = field(default_factory=dict[str, list[Any]])

    def details(self) -> dict[str, Any]:
        """The fields the event writes: the flags only when given, the changes only when
        some."""
        details: dict[str, Any] = {}

        if self.retry_timeouts:
            details["retry_timeouts"] = True

        if self.retry_crashes:
            details["retry_crashes"] = True

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
    flags = (arguments.retry_timeouts, arguments.retry_crashes, arguments.allow_mixed)

    if not (output / BENCHMARK_FILE).is_file():
        return {"record": record, "events": []}, Opening("started", *flags)

    old = read_toml(output / BENCHMARK_FILE)

    if old.get("runs") != table["runs"]:
        raise BenchmarkError(other_runs(output, old, table))

    path = output / METADATA_FILE
    metadata: dict[str, Any] = run_folder.read_json(path) if path.is_file() else {"record": record}
    # The processes are the benchmark's own option, compared below with the others.
    before_record = {
        name: value for name, value in metadata.get("record", {}).items() if name != "benchmark"
    }
    after_record = {name: value for name, value in record.items() if name != "benchmark"}
    changed_record = differences(before_record, after_record)

    if changed_record and not arguments.allow_mixed:
        raise BenchmarkError(
            f"{output} was started with another source, build, versions or machine "
            f"({'; '.join(changed_record)}): resume with --allow-mixed to mix them"
        )

    if changed_record and record not in metadata.get("other_records", []):
        metadata.setdefault("other_records", []).append(record)

    before, after = old.get("benchmark", {}), table["benchmark"]
    changed = {
        name: [before.get(name), after.get(name)]
        for name in sorted(set(before) | set(after))
        if before.get(name) != after.get(name)
    }
    return metadata, Opening("resumed", *flags, changed)


def other_runs(output: Path, old: Mapping[str, Any], table: Mapping[str, Any]) -> str:
    """Why a folder's runs are not the benchmark's: their threads only, as the number of
    processes changed, or the runs added and removed."""
    old_runs: list[dict[str, Any]] = old.get("runs", [])
    new_runs: list[dict[str, Any]] = table["runs"]
    before = old.get("benchmark", {}).get("processes", 1)
    after = table["benchmark"]["processes"]

    if before != after and without_threads(old_runs) == without_threads(new_runs):
        return (
            f"{output} holds these runs as run with {before} processes: with more than one, "
            "every run that leaves column_generation.threads unset gets 1, so with "
            f"{after} they are other runs; resume with --processes {before}, or choose "
            "another folder"
        )

    old_names = [run["name"] for run in old_runs]
    new_names = [run["name"] for run in new_runs]
    added = [name for name in new_names if name not in old_names]
    removed = [name for name in old_names if name not in new_names]
    parts = [f"{len(added)} added: {first_names(added)}"] if added else []
    parts += [f"{len(removed)} removed: {first_names(removed)}"] if removed else []
    found = "; ".join(parts) or "the same names with other options"
    return f"{output} holds another benchmark's runs ({found}): choose another folder"


def without_threads(runs: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """The runs without their names and their ``column_generation.threads``."""
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


def first_names(names: list[str]) -> str:
    return ", ".join(names[:3]) + (", ..." if len(names) > 3 else "")


def write_folder(output: Path, expansion: Expansion, metadata: dict[str, Any]) -> None:
    """The benchmark folder's ``benchmark.toml`` and ``metadata.json``."""
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
            # One already waited for is gone, and its id may be another process's now.
            if process.poll() is None:
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
    """A run in progress: its process, and its load so far. `tree` is None when the
    process ended before psutil could open it."""

    run: Run
    folder: Path
    process: subprocess.Popen[Any]
    log: IO[bytes] | None
    started: float
    started_at: str
    tree: psutil.Process | None = None
    load: Load = field(default_factory=Load)
    processor_seconds: float = 0.0

    def read_processor_seconds(self) -> None:
        seconds = None if self.tree is None else tree_processor_seconds(self.tree)

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
        elif outcome in (run_folder.FINISHED, run_folder.CHECK_FAILED):
            self.finished += 1
            self.failed_checks += outcome == run_folder.CHECK_FAILED
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
    build: Path,
    opening: Opening,
    metadata: dict[str, Any],
) -> int:
    """Writes the benchmark folder, then runs every round not yet finished,
    ``processes`` at a time; returns the benchmark's exit code.

    Ctrl-C, SIGTERM, SIGHUP or an error of
    the benchmark end it through :func:`interrupt`.
    """
    folders = run_folders(expansion, output)
    tally = Tally(total=len(folders))
    running: list[Running] = []
    waiting: deque[tuple[Run, Path]] = deque()
    run_files: dict[str, Path] = {}
    started = next((event["time"] for event in metadata.get("events", [])), "earlier")

    try:
        write_folder(output, expansion, metadata)
        run_files = write_run_files(output / RUN_FILES_FOLDER, expansion)
        again = scan(folders, opening, tally, waiting)
        print_opening(output, opening, started, tally, len(waiting), again)
        details = {"finished": tally.finished, "to_run": len(waiting), **again}
        add_event(output, opening.event, total=tally.total, **details, **opening.details())

        if (output / STOP_FILE).exists():
            (output / STOP_FILE).unlink()
            print(f"a {STOP_FILE} file from before was removed")

        stopped = run_rounds(expansion, output, run_files, build, tally, running, waiting)
        return end(output, tally, len(waiting), stopped)
    except BaseException as error:
        return interrupt(output, tally, running, len(waiting), error, run_files, expansion)


def scan(
    folders: list[tuple[Run, Path]],
    opening: Opening,
    tally: Tally,
    waiting: deque[tuple[Run, Path]],
) -> dict[str, int]:
    """Sorts the rounds an earlier start left: each with a result counts in `tally` and is
    kept, unless it timed out or crashed after its result and the flags say to run those
    again; every other goes to `waiting`, its folder removed when it starts again.

    Returns:
        How many went to `waiting` because they were unfinished (their folder exists, without
        a result, or interrupted), timeouts or crashes to run again.
    """
    again = {"unfinished": 0, "timeouts_to_run_again": 0, "crashes_to_run_again": 0}

    for run, folder in folders:
        result = run_folder.read_result(folder)
        outcome = recorded_outcome(folder)

        if result is not None and outcome != run_folder.INTERRUPTED:
            timeout = result.get("status") == run_folder.TIMEOUT
            crashed = outcome in (run_folder.CRASHED, run_folder.NOT_STARTED)

            if timeout and opening.retry_timeouts:
                again["timeouts_to_run_again"] += 1
            elif crashed and opening.retry_crashes:
                again["crashes_to_run_again"] += 1
            else:
                # No outcome: the benchmark ended before it could record the run's.
                if outcome is None:
                    failed = result.get("solution_check") == "failed"
                    outcome = run_folder.CHECK_FAILED if failed else run_folder.FINISHED

                tally.add(outcome, result)
                continue
        elif folder.exists():
            again["unfinished"] += 1

        waiting.append((run, folder))

    return again


def recorded_outcome(folder: Path) -> str | None:
    """The outcome the benchmark recorded in a run's ``metadata.json``, if any."""
    path = folder / run_folder.METADATA_FILE
    return run_folder.read_json(path).get("outcome") if path.is_file() else None


def run_rounds(
    expansion: Expansion,
    output: Path,
    run_files: Mapping[str, Path],
    build: Path,
    tally: Tally,
    running: list[Running],
    waiting: deque[tuple[Run, Path]],
) -> bool:
    """Starts the waiting rounds, ``processes`` at a time, until every one has ended or
    a ``STOP`` file stops the starts; returns whether one did."""
    stopped = False
    last_sample = time.monotonic()
    write_status(output, tally, running, len(waiting))

    while running or (waiting and not stopped):
        # Only while runs are waiting: once all have started, it would stop nothing.
        if not stopped and waiting and (output / STOP_FILE).exists():
            stopped = True
            print(f"{STOP_FILE}: the runs in progress end, no other starts")

        while not stopped and waiting and len(running) < expansion.processes:
            run, folder = waiting.popleft()
            command = [*runner_command(run.layer), "--file", str(run_files[run.name])]
            command += ["--output", str(folder), "--build", str(build)]
            command += ["--machine", expansion.machine] if expansion.machine else []
            start_run(run, folder, command, expansion.logs, running)
            write_status(output, tally, running, len(waiting))

        sample = None

        if time.monotonic() - last_sample >= INTERVAL_SECONDS:
            sample, last_sample = take_sample(), time.monotonic()
            write_status(output, tally, running, len(waiting))

        for current in list(running):
            exit_code = current.process.poll()
            seconds = time.monotonic() - current.started
            timed_out = (
                exit_code is None and expansion.timeout is not None and seconds > expansion.timeout
            )

            if exit_code is None and not timed_out:
                # At a sample only: listing a process tree costs the processor time the runs
                # are measured on.
                if sample is not None:
                    current.read_processor_seconds()
                    current.load.add(sample, current.processor_seconds)

                continue

            if timed_out:
                kill(current)
                wait_for(current)

            outcome, result = finish_run(current, exit_code, run_files, expansion.processes)
            tally.add(outcome, result)
            running.remove(current)
            ended = tally.finished + tally.crashed
            print(f"[{ended}/{tally.total}] {current.folder.name}: {outcome}, {seconds:.1f} s")
            write_status(output, tally, running, len(waiting))

        time.sleep(POLL_SECONDS)

    return stopped


def end(output: Path, tally: Tally, waiting: int, stopped: bool) -> int:
    """Records how the benchmark ended, removing a ``STOP`` file; returns its exit
    code."""
    (output / STOP_FILE).unlink(missing_ok=True)

    if stopped:
        add_event(output, "stopped", runs_left=waiting, **tally.counts())
        write_status(output, tally, [], waiting, "stopped")
        print(f"stopped, {waiting} runs left: run the benchmark again to resume")
        return STOPPED

    add_event(output, "finished", **tally.counts())
    write_status(output, tally, [], 0, "finished")
    print(tally.line(0, 0))
    problems = tally.timeouts + tally.failed_checks + tally.crashed
    return RUNS_FAILED if problems else 0


def print_opening(
    output: Path,
    opening: Opening,
    started: str,
    tally: Tally,
    to_run: int,
    again: Mapping[str, int],
) -> None:
    """Prints what this start of the benchmark has to do; on a resume, says so loudly
    first: the folder's finished rounds are kept."""
    if opening.event == "started":
        print(f"starting {output}: {to_run} runs to run")
        return

    rule = "=" * 72
    print(
        f"{rule}\nRESUMING: {output} already holds this benchmark, started {started}.\n"
        "Its finished rounds are kept and only the others run; give another --output to "
        f"start anew.\n{rule}",
        file=sys.stderr,
    )
    done = f"{tally.finished} / {tally.total} finished"
    done += f" ({tally.timeouts} timeout, {tally.failed_checks} check failed)"
    notes = [f"{again['unfinished']} unfinished, removed first"] if again["unfinished"] else []
    notes += [f"{again['timeouts_to_run_again']} timeouts to run again"] * bool(
        again["timeouts_to_run_again"]
    )
    notes += [f"{again['crashes_to_run_again']} crashes to run again"] * bool(
        again["crashes_to_run_again"]
    )
    left = f"{to_run} to run" + (f" ({'; '.join(notes)})" if notes else "")
    print(f"resuming {output}: {done}, {left}")


def interrupt(
    output: Path,
    tally: Tally,
    running: list[Running],
    waiting: int,
    error: BaseException,
    run_files: Mapping[str, Path],
    expansion: Expansion,
) -> int:
    """Ends a benchmark that Ctrl-C, a signal or an error stopped: kills every run in
    progress first, then waits for each, then records them, as far as it can.

    A run that wrote its result before is finished; the others are interrupted. Another
    Ctrl-C or signal meanwhile is ignored, so that no runner is left running.
    """
    handlers = ignore_signals()

    try:
        for current in running:
            kill(current)

        for current in running:
            wait_for(current)

        killed: list[str] = []

        with contextlib.suppress(OSError, ValueError):
            for current in running:
                if run_folder.read_result(current.folder) is not None:
                    outcome, result = finish_run(current, None, run_files, expansion.processes)
                    tally.add(outcome, result)
                else:
                    killed.append(current.folder.name)
                    run_folder.update_metadata(current.folder, {"outcome": run_folder.INTERRUPTED})

            add_event(
                output, "interrupted", reason=reason_of(error), killed=killed, **tally.counts()
            )
            write_status(output, tally, [], waiting + len(killed), "interrupted")
    finally:
        restore_signals(handlers)

    ended = isinstance(error, KeyboardInterrupt | Terminated)

    if not ended:
        traceback.print_exception(error)

    print(f"interrupted ({reason_of(error)}): run the benchmark again to resume", file=sys.stderr)
    return INTERRUPTED if ended else FAILED


def reason_of(error: BaseException) -> str:
    if isinstance(error, KeyboardInterrupt):
        return "Ctrl-C"

    if isinstance(error, Terminated):
        return f"signal {error}"

    return f"{type(error).__name__}: {error}"


def start_run(
    run: Run, folder: Path, command: list[str], logs: bool, running: list[Running]
) -> None:
    """Starts a run's runner into `folder`, removing what an earlier start left there,
    and adds it to `running` at once: a Ctrl-C a moment later still finds it to kill."""
    if folder.exists():
        shutil.rmtree(folder)

    folder.mkdir(parents=True)
    log = open(folder / run_folder.LOG_FILE, "wb") if logs else None  # noqa: SIM115
    sample = take_sample()
    # Unbuffered: a run killed at its timeout keeps in its log what it printed.
    environment = {**os.environ, "PYTHONUNBUFFERED": "1"}
    process = subprocess.Popen(  # nosec B603: this folder's runners
        command,
        stdin=subprocess.DEVNULL,
        stdout=log if log is not None else subprocess.DEVNULL,
        stderr=subprocess.STDOUT,
        env=environment,
        **own_group(),
    )
    current = Running(run, folder, process, log, time.monotonic(), runner.now())
    running.append(current)
    current.tree = tree_of(process)
    current.load.add(sample, 0.0)


def tree_of(process: subprocess.Popen[Any]) -> psutil.Process | None:
    """The psutil process of a runner just started; None when it has already ended,
    which psutil cannot open on Windows."""
    try:
        return psutil.Process(process.pid)
    except psutil.NoSuchProcess:
        return None


def kill(current: Running) -> None:
    """Kills a run's process tree, without waiting for it."""
    if current.tree is not None:
        kill_tree(current.tree)
        return

    with contextlib.suppress(OSError):
        current.process.kill()


def wait_for(current: Running) -> None:
    """Waits a few seconds at most for a killed run's process, then closes its log."""
    with contextlib.suppress(subprocess.TimeoutExpired):
        current.process.wait(timeout=KILL_WAIT_SECONDS)

    if current.log is not None:
        current.log.close()


def finish_run(
    current: Running, exit_code: int | None, run_files: Mapping[str, Path], processes: int
) -> tuple[str, dict[str, Any] | None]:
    """Records how a run ended in its ``metadata.json``; for a run killed at its timeout
    (`exit_code` None, no result), writes its ``result.json`` too.

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
        return run_folder.FINISHED

    if result is not None and exit_code == 1 and result.get("solution_check") == "failed":
        return run_folder.CHECK_FAILED

    return run_folder.NOT_STARTED if exit_code == 2 else run_folder.CRASHED


def write_status(
    output: Path, tally: Tally, running: Sequence[Running], waiting: int, end: str | None = None
) -> None:
    """``status.txt``: how many runs are done and how, then each run in progress.

    Only for
    reading: when the file cannot be written, the benchmark goes on without it.
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
