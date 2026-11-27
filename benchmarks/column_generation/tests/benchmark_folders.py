#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Benchmark folders written by hand, for the tests of the summary and of the
comparison:

a run's options, a result, a round's ``metadata.json``, and the folders that hold them.
"""

import csv
from pathlib import Path
from typing import Any

import tomli_w
from column_generation_benchmark import run_folder

COMMIT = "a" * 40
RUN = "C101_25-package-highspy-exact-artificial"


def options(
    layer: str = "package", master: str = "highspy", instance: str = "C101_25"
) -> dict[str, Any]:
    return {
        "layer": layer,
        "instance": {"type": "solomon", "name": instance},
        "master": {"type": master, "parameters": {}},
        "pricing": {"stages": [{"algorithm": "simple"}]},
        "start": {"type": "artificial", "cost_factor": 10000.0},
        "graph": {},
    }


def result(**changes: Any) -> dict[str, Any]:
    """A finished run's result, with `changes`."""
    found: dict[str, Any] = {
        "status": "Optimal",
        "objective": 191.8136,
        "iterations": 16,
        "columns": 599,
        "artificial_columns": 25,
        "master_seconds": 0.04,
        "pricing_seconds": 0.09,
        "other_seconds": 0.01,
        "solve_seconds": 1.0,
        "lp_solved_again_seconds": None,
        "integer_status": "Optimal",
        "integer_objective": 191.8136,
        "integer_seconds": 0.1,
        "gap": 0.0,
        "solution_check": "passed",
        "process_peak_memory_bytes": 64 * 1024 * 1024,
    }
    found.update(changes)
    return found


def metadata(
    outcome: str | None = "finished", master: str = "highspy", **changes: Any
) -> dict[str, Any]:
    """A round's ``metadata.json``, with `changes` over its groups."""
    versions: dict[str, Any] = {"python": "3.14.0", "rcspp": "0.0.2", "numpy": "2.5.3"}

    if master == "highspy":
        versions.update(highspy="1.15.1", highs="1.15.1")

    if master == "gurobipy":
        versions.update(gurobipy="13.0.1", gurobi="13.0.1")

    found: dict[str, Any] = {
        "processor_seconds": 1.5,
        "source": {"commit": COMMIT, "uncommitted_changes": False, "branch": "main"},
        "code": {"build": "build-python"},
        "versions": versions,
        "machine": {"label": "desk", "processor": "cpu", "logical_cores": 8},
        "benchmark": {"processes": 1, "solver_threads": None},
        "exit_code": 0,
        "wall_seconds": 2.0,
        "load": {"other_processor_percent_mean": 5.0, "on_battery": False},
    }

    if outcome is not None:
        found["outcome"] = outcome

    found.update(changes)
    return found


def write_round(
    folder: Path,
    result_of_round: dict[str, Any] | None,
    metadata_of_round: dict[str, Any] | None,
    run_options: dict[str, Any] | None = None,
) -> None:
    folder.mkdir(parents=True)

    if run_options is not None:
        (folder / run_folder.RUN_FILE).write_text(tomli_w.dumps(run_options))

    if metadata_of_round is not None:
        run_folder.write_json(folder / run_folder.METADATA_FILE, metadata_of_round)

    if result_of_round is not None:
        run_folder.write_json(folder / run_folder.RESULT_FILE, result_of_round)


def write_benchmark(
    path: Path,
    runs: dict[str, dict[str, Any]],
    rounds: int,
    processes: int = 1,
    events: list[dict[str, Any]] | None = None,
) -> None:
    """A benchmark folder's own files: `runs` by name, with their options."""
    path.mkdir(parents=True, exist_ok=True)
    table = {
        "benchmark": {"processes": processes, "repeats": rounds},
        "runs": [{"name": name, **value} for name, value in runs.items()],
    }
    (path / run_folder.BENCHMARK_FILE).write_text(tomli_w.dumps(table))
    folder_metadata = {
        "processes": processes,
        "runs": len(runs),
        "rounds": rounds,
        "events": events or [],
    }
    run_folder.write_json(path / run_folder.METADATA_FILE, folder_metadata)


def round_folder(path: Path, name: str, number: int) -> Path:
    return path / run_folder.RUNS_FOLDER / f"{name}-r{number}"


def write_rounds(
    path: Path,
    name: str,
    results: list[dict[str, Any]],
    run_options: dict[str, Any] | None = None,
    master: str = "highspy",
) -> None:
    """A benchmark of one run, a finished round per result, each with its
    ``run.toml``."""
    run_options = run_options or options()
    write_benchmark(path, {name: run_options}, rounds=len(results))

    for number, found in enumerate(results, start=1):
        folder = round_folder(path, name, number)
        write_round(folder, found, metadata(master=master), run_options)


def read_csv(path: Path) -> list[dict[str, str]]:
    with open(path, encoding="utf-8", newline="") as file:
        return list(csv.DictReader(file))
