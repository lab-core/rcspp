#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The run folder: ``run.toml`` runs the same again, ``result.json`` has its fields, and the
record has every group."""

import dataclasses
import importlib
import math
import sys
import tomllib
from pathlib import Path
from typing import Any

import pytest
from column_generation_benchmark import collectors, options, record, run_folder
from column_generation_benchmark.options import OptionsError

pytestmark = pytest.mark.usefixtures("build")

BENCHMARK = """
[pricing.heuristics-first]
stages = [
  { algorithm = "tabu", max_solutions = 20, max_iterations = 1000, direction = "forward" },
  { algorithm = "simple" },
]

[[runs]]
instance = "R201_25"
layer = "cpp-bindings"
master = { type = "gurobipy", parameters = { Presolve = 1, Method = 0 } }
pricing = "heuristics-first"
start = { type = "artificial", cost_factor = 1e5 }
column_generation = { timeout_s = inf, tolerances = { type = "relative", reduced_cost = 1e-6 } }
statistics = { process = false }
"""


def result(**changes: Any) -> dict[str, Any]:
    """A finished run's result, with `changes`."""
    fields: dict[str, Any] = {
        "status": "Optimal",
        "objective": 191.8136,
        "iterations": 23,
        "columns": 661,
        "artificial_columns": 0,
        "master_seconds": 0.05,
        "pricing_seconds": 0.4,
        "other_seconds": 0.01,
        "solve_seconds": 0.46,
    }
    fields.update(changes)
    return fields


# MARK: run.toml


def test_the_run_file_runs_the_same_run_again(tmp_path: Path):
    (run,) = options.Benchmark.from_table(tomllib.loads(BENCHMARK)).expand().runs
    run_folder.write_run_file(tmp_path, run.to_table())
    again = options.expand_run(run_folder.read_run_file(tmp_path))

    assert again == run
    assert again.column_generation.timeout_s == math.inf
    assert "heuristics-first" not in (tmp_path / "run.toml").read_text()


def test_the_command_line_is_written_quoted(tmp_path: Path):
    arguments = [sys.executable, "package_runner.py", "--master", "{ type = 'gurobipy' }"]
    run_folder.write_command(tmp_path, arguments)
    text = (tmp_path / "command.txt").read_text()

    assert text.endswith("\n")
    assert "{ type = 'gurobipy' }" in text
    assert "package_runner.py" in text


def test_metadata_gathers_what_each_writer_adds(tmp_path: Path):
    run_folder.update_metadata(tmp_path, {"start": "2026-11-16T10:00:00"})
    metadata = run_folder.update_metadata(tmp_path, {"end": "2026-11-16T10:00:05"})

    assert metadata == {"start": "2026-11-16T10:00:00", "end": "2026-11-16T10:00:05"}
    assert run_folder.read_json(tmp_path / "metadata.json") == metadata


# MARK: result.json


def test_a_result_round_trips(tmp_path: Path):
    assert run_folder.read_result(tmp_path) is None

    written = result(objective=None, **collectors.measure_process())
    run_folder.write_result(tmp_path, written)

    assert run_folder.read_result(tmp_path) == written
    assert [path.name for path in tmp_path.iterdir()] == ["result.json"]


def test_an_infinite_value_reads_back(tmp_path: Path):
    run_folder.write_result(tmp_path, result(objective=-math.inf))

    assert run_folder.read_result(tmp_path) == result(objective=-math.inf)


@pytest.mark.parametrize(
    ("changes", "message"),
    [
        ({"iterations": 2.5}, "iterations = 2.5 is not integer"),
        ({"objective": "191.8"}, "objective = '191.8' is not number or null"),
        ({"columns": True}, "columns = True is not integer"),
        ({"lp": 191.8}, "lp is not a result field"),
    ],
)
def test_a_wrong_result_is_not_written(tmp_path: Path, changes: dict[str, Any], message: str):
    with pytest.raises(OptionsError, match=message):
        run_folder.write_result(tmp_path, result(**changes))

    assert run_folder.read_result(tmp_path) is None


def test_a_missing_field_is_refused():
    fields = result()
    del fields["solve_seconds"]

    assert run_folder.result_problems(fields) == ["solve_seconds is missing"]


# MARK: Collectors


def test_every_collector_has_its_statistics_option():
    keys = [item.name for item in dataclasses.fields(options.BLOCKS["statistics"])]

    assert set(collectors.COLLECTORS) == set(keys)


def test_collectors_measure_only_when_on():
    measured = collectors.collect({"process": True})

    assert set(measured) == set(collectors.COLLECTORS["process"].fields)
    assert measured["process_peak_memory_bytes"] > 0
    assert collectors.collect({"process": False}) == {}


# MARK: Record


def test_the_record_has_every_group(build: Path):
    importlib.import_module("highspy")  # imported, so its versions are recorded

    made = record.make_record(build, machine_label="laptop", processes=2, solver_threads=1)

    assert tuple(made) == record.RECORD_GROUPS
    assert set(made["source"]) == {"commit", "uncommitted_changes", "branch"}
    assert made["code"]["build"] == str(build)
    assert made["code"]["options"]["USE_PYTHON"] == "ON"
    assert made["code"]["compiler"]
    assert made["code"]["compiler_id"]
    assert made["versions"]["rcspp"]
    assert made["versions"]["highs"] == made["versions"]["highspy"]
    assert made["machine"]["label"] == "laptop"
    assert made["machine"]["logical_cores"] >= 1
    assert made["machine"]["memory_bytes"] > 0
    assert made["benchmark"] == {"processes": 2, "solver_threads": 1}


def test_the_source_is_this_tree():
    source = record.source_record()

    if source["commit"] is None:
        pytest.skip("not a git checkout")

    assert len(source["commit"]) == 40
    assert isinstance(source["uncommitted_changes"], bool)


def test_a_missing_build_cache_records_nothing(tmp_path: Path):
    assert record.read_cmake_cache(tmp_path) == {}
    assert record.compiler_version(tmp_path) == {}


def test_what_cannot_be_found_is_recorded_as_none(tmp_path: Path):
    assert record.git(["rev-parse", "HEAD"], tmp_path) is None
    assert record.package_version("not-a-package-of-this-environment") is None
    assert record.tree_version(tmp_path) is None


def test_the_gurobi_versions_are_recorded_when_imported():
    pytest.importorskip("gurobipy")

    versions = record.version_record()

    assert versions["gurobipy"] is not None
    assert versions["gurobi"].count(".") == 2


# MARK: Writing


def test_a_failed_write_leaves_no_file(tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
    def refuse(source: str, target: Path) -> None:
        raise OSError("disk full")

    monkeypatch.setattr(run_folder.os, "replace", refuse)

    with pytest.raises(OSError, match="disk full"):
        run_folder.write_atomically(tmp_path / "result.json", "{}")

    assert list(tmp_path.iterdir()) == []
