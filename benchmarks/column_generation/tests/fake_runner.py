#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""A runner that solves nothing, for the benchmark's tests: it takes a runner's flags
and fills a run folder as a runner does, doing what the test asks for its run.

The test writes a JSON file named by ``FAKE_RUNNER_PLAN``: for a run whose folder name holds
a key, what to do (``sleep`` seconds, ``exit`` 2 before writing anything, ``crash`` after
starting the folder, ``crash_after_result``, ``sleep_after_result`` seconds,
``child_sleep`` seconds in a child process of its own, ``check_failed``, ``stop`` the
benchmark, ``processor_seconds`` to report, ``build_fails``, ``build_sleep`` seconds). Each
start and end goes as a file into the folder named by ``FAKE_RUNNER_LINES``; each build makes
a file ``building-<run>`` next to it. A runner prints ``working`` to its log as it starts.
"""

import argparse
import json
import os
import shutil
import subprocess  # nosec B404: a sleeping child, for the tests
import sys
import time
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from column_generation_benchmark import run_folder  # noqa: E402


def plan_for(output: str) -> dict[str, Any]:
    with open(os.environ["FAKE_RUNNER_PLAN"], encoding="utf-8") as file:
        plan: dict[str, dict[str, Any]] = json.load(file)

    name = Path(output).name
    nothing: dict[str, Any] = {}
    return next((what for key, what in plan.items() if key in name), nothing)


def log(line: str) -> None:
    """One file per line, named by the time: two runners appending to one file at once
    can lose a line."""
    folder = Path(os.environ["FAKE_RUNNER_LINES"])
    folder.mkdir(exist_ok=True)
    (folder / f"{time.time_ns():020d}-{os.getpid()}.txt").write_text(line, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--file", required=True)
    parser.add_argument("--output")
    parser.add_argument("--build")
    parser.add_argument("--machine")
    parser.add_argument("--check")
    arguments = parser.parse_args()

    if arguments.check == "build":
        building = plan_for(Path(arguments.file).stem)
        Path(os.environ["FAKE_RUNNER_LINES"]).with_name(
            f"building-{Path(arguments.file).stem}"
        ).touch()
        time.sleep(building.get("build_sleep", 0.0))

        if building.get("build_fails"):
            print("cannot build", file=sys.stderr)
            return 2

        return 0

    folder = Path(arguments.output)
    what = plan_for(arguments.output)
    log(f"start {folder.name}")
    print("working")

    if what.get("exit") == 2:
        return 2

    shutil.copyfile(arguments.file, folder / run_folder.RUN_FILE)
    run_folder.update_metadata(folder, {"start": "now", "benchmark": {"solver_threads": None}})

    if what.get("stop"):
        (folder.parents[1] / "STOP").touch()

    if "child_sleep" in what:
        # Its folder on the command line, so that the test finds it among the processes.
        sleeping = f"import time; time.sleep({what['child_sleep']})"
        subprocess.Popen([sys.executable, "-c", sleeping, str(folder)])  # nosec B603

    time.sleep(what.get("sleep", 0.0))

    if what.get("crash"):
        raise RuntimeError("the fake runner crashed")

    result: dict[str, Any] = dict.fromkeys(run_folder.RESULT_FIELDS)
    result.update(
        status="Optimal",
        objective=1.0,
        iterations=1,
        columns=1,
        artificial_columns=0,
        master_seconds=0.0,
        pricing_seconds=0.0,
        other_seconds=0.0,
        solve_seconds=0.0,
        solution_check="failed" if what.get("check_failed") else "passed",
    )

    if "processor_seconds" in what:
        run_folder.update_metadata(folder, {"processor_seconds": what["processor_seconds"]})

    run_folder.write_result(folder, result)
    time.sleep(what.get("sleep_after_result", 0.0))

    if what.get("crash_after_result"):
        raise RuntimeError("the fake runner crashed after its result")

    log(f"end {folder.name}")
    return 1 if what.get("check_failed") else 0


if __name__ == "__main__":
    sys.exit(main())
