#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Every feature of the column generation through the benchmark, end to end: a benchmark
file runs once, then each test checks one rule over its runs.

By default the file is ``all_features_quick.toml`` (seconds, HiGHS only). The large file runs by
hand, with Gurobi when it has a licence:

    pytest benchmarks/column_generation/tests/test_benchmark_all_features.py \\
        --all-features-file benchmarks/column_generation/all_features.toml

The rules come from each run's options, not from its name: a run with a loop limit ends with
that limit's status; a run with a stop on the lower bound ends with that stop's status or
proves its LP; any other run proves its LP, which then equals, per instance and route
relaxation, the package's forward simple search's and the value recorded below. A new feature
adds its runs to both files and, when its runs need a rule of their own, the rule here.
"""

import math
import tomllib
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import benchmark
import pytest
from column_generation_benchmark import run_folder, source_tree
from column_generation_benchmark.instances import SolomonInstance
from column_generation_benchmark.options.blocks import ColumnGeneration
from test_benchmark_runners import has_example, has_gurobi

QUICK_FILE = Path(__file__).resolve().parent / "all_features_quick.toml"

#: How far two LP values of one problem may be apart, relative to their size.
TOLERANCE = 1e-6

#: The LP of each instance and route relaxation, by the forward simple search of the package
#: over highspy (the run every other run of the problem is compared with).
KNOWN_LP = {
    ("C101_10", "none"): 58.32595327336471,
    ("C101_10", "ng 4"): 58.32595327336471,
    ("C101_10", "ng 8"): 58.32595327336471,
    ("C101_10", "ng 10"): 58.32595327336471,
    ("C101_10", "elementary"): 58.32595327336465,
    ("C102_10", "none"): 56.116033814287235,
    ("C102_10", "ng 4"): 57.24996285655929,
    ("C102_10", "ng 8"): 57.24996285655929,
    ("C102_10", "ng 10"): 57.24996285655929,
    ("C102_10", "elementary"): 57.24996285655929,
    ("C101_25", "none"): 191.8136197786562,
    ("C101_25", "ng 8"): 191.8136197786562,
    ("R201_25", "none"): 449.65975310219824,
    ("R201_25", "ng 8"): 461.3023272941951,
    ("RC201_12", "none"): 427.235576835984,
    ("RC201_12", "ng 8"): 446.01348049272934,
}

#: The loop's limits and the status each gives.
LIMITS = {"max_iterations": "IterationLimit", "timeout_s": "TimeLimit"}

#: The stops on the lower bound and the status each gives.
STOPS = {
    "cutoff": "Cutoff",
    "absolute_gap": "AbsoluteGapLimit",
    "relative_gap": "RelativeGapLimit",
    "integer_objective": "IntegerObjectiveGap",
}

#: What the engine logs when a bidirectional search cannot use its half-way point.
HALF_WAY_POINT_OFF = "the half-way point is off"


@dataclass
class Run:
    """A run's resolved options, its result and its log."""

    name: str
    options: dict[str, Any]
    result: dict[str, Any]
    log: str

    @property
    def instance(self) -> str:
        return self.options["instance"]["name"]

    @property
    def relaxation(self) -> str:
        graph = self.options.get("graph", {})
        relaxation = graph.get("relaxation", "none")
        return f"ng {graph['ng_size']}" if relaxation == "ng" else relaxation

    @property
    def problem(self) -> tuple[str, str]:
        """The LP this run solves: its instance and route relaxation. A fleet row as large as
        the customers, the files' only one, leaves the LP as it is."""
        return self.instance, self.relaxation

    @property
    def settings(self) -> dict[str, Any]:
        return self.options.get("column_generation", {})

    @property
    def stages(self) -> list[dict[str, Any]]:
        return self.options["pricing"]["stages"]

    @property
    def limit(self) -> str | None:
        """The status of the loop limit the run sets, if any."""
        found = [status for name, status in LIMITS.items() if is_set(self.settings.get(name))]
        assert len(found) <= 1, f"{self.name}: one loop limit per run, for a known status"
        return found[0] if found else None

    @property
    def stops(self) -> set[str]:
        """The statuses of the stops on the lower bound the run sets."""
        return {
            status for name, status in STOPS.items() if is_set_stop(name, self.settings.get(name))
        }

    @property
    def is_reference(self) -> bool:
        """Whether the run is its problem's reference: the package's forward simple
        search, nothing else set."""
        return (
            self.options["layer"] == "package"
            and self.stages
            in ([{"algorithm": "simple"}], [{"algorithm": "simple", "direction": "forward"}])
            and self.options["pricing"].get("preprocess", True)
            and self.options.get("instance", {}).get("vehicles") is None
            and self.limit is None
            and not self.stops
        )


def is_set(value: Any) -> bool:
    """Whether a loop parameter is set: absent, false and inf are off."""
    return (
        value is not None
        and value is not False
        and not (isinstance(value, float) and math.isinf(value))
    )


def is_set_stop(name: str, value: Any) -> bool:
    """Whether a stop on the lower bound is set: a gap of 0 is off too."""
    return is_set(value) and not (name in ("absolute_gap", "relative_gap") and value == 0)


def same_lp(first: float, second: float) -> bool:
    return math.isclose(first, second, rel_tol=TOLERANCE, abs_tol=TOLERANCE)


# MARK: The benchmark, run once


@pytest.fixture(scope="module")
def runs(
    request: pytest.FixtureRequest, build: Path, tmp_path_factory: pytest.TempPathFactory
) -> list[Run]:
    """Every run of the benchmark file, after one benchmark of it."""
    chosen = request.config.getoption("--all-features-file")
    file = Path(chosen).resolve() if chosen else QUICK_FILE
    text = file.read_text(encoding="utf-8")

    if ("cpp-bindings" in text or "cpp-native" in text) and not has_example():
        pytest.skip("the build has no column generation example")

    if "gurobi" in text and not has_gurobi():
        pytest.skip("the file has Gurobi runs, and gurobipy is not installed or has no licence")

    output = tmp_path_factory.mktemp("all_features")
    assert benchmark.main([str(file), "--build", str(build), "--output", str(output)]) == 0
    found: list[Run] = []

    for folder in sorted((output / run_folder.RUNS_FOLDER).iterdir()):
        result = run_folder.read_result(folder)
        assert result is not None, f"{folder.name} has no result"
        options = tomllib.loads((folder / run_folder.RUN_FILE).read_text(encoding="utf-8"))
        log_path = folder / run_folder.LOG_FILE
        log = log_path.read_text(encoding="utf-8", errors="replace") if log_path.exists() else ""
        found.append(Run(folder.name, options, result, log))

    return found


@pytest.fixture(scope="module")
def references(runs: list[Run]) -> dict[tuple[str, str], float]:
    """Each problem's LP, from its reference run."""
    found: dict[tuple[str, str], float] = {}

    for run in runs:
        if run.is_reference:
            assert run.result["status"] == "Optimal", run.name
            found[run.problem] = run.result["objective"]

    missing = {run.problem for run in runs} - set(found)
    message = "problems without a reference run (the package's forward simple search)"
    assert not missing, f"{message}: {missing}"
    return found


# MARK: The rules


def test_every_run_ends_with_the_status_its_options_give(runs: list[Run]):
    wrong: list[str] = []

    for run in runs:
        status = run.result["status"]

        if run.limit is not None:
            expected = {run.limit}
        else:
            expected = run.stops | {"Optimal"}

        if status not in expected:
            wrong.append(f"{run.name}: {status}, expected {sorted(expected)}")

    assert not wrong, "\n".join(wrong)


def test_every_problem_has_its_known_lp(references: dict[tuple[str, str], float]):
    unknown = set(references) - set(KNOWN_LP)
    assert not unknown, f"add the LP of {unknown} to KNOWN_LP, from its reference run"

    for problem, objective in references.items():
        assert same_lp(
            objective, KNOWN_LP[problem]
        ), f"{problem}: {objective} for {KNOWN_LP[problem]}"


def test_every_proven_lp_is_its_problems(runs: list[Run], references: dict[tuple[str, str], float]):
    wrong = [
        f"{run.name}: {run.result['objective']} for {references[run.problem]}"
        for run in runs
        if run.result["status"] == "Optimal"
        and not same_lp(run.result["objective"], references[run.problem])
    ]

    assert not wrong, "\n".join(wrong)


def test_a_stop_ends_within_its_gap_and_below_the_lp(
    runs: list[Run], references: dict[tuple[str, str], float]
):
    for run in runs:
        status = run.result["status"]

        if status not in STOPS.values():
            continue

        objective, bound = run.result["objective"], run.result["lower_bound"]
        lp = references[run.problem]
        assert bound <= lp + TOLERANCE * max(1.0, abs(lp)), run.name
        assert objective >= lp - TOLERANCE * max(1.0, abs(lp)), run.name

        if status == "AbsoluteGapLimit":
            assert objective - bound <= run.settings["absolute_gap"] + TOLERANCE, run.name
        elif status == "RelativeGapLimit":
            assert (
                objective - bound <= run.settings["relative_gap"] * abs(objective) + TOLERANCE
            ), run.name
        elif status == "Cutoff":
            rounded = (
                math.ceil(bound - TOLERANCE) if run.settings.get("integer_objective") else bound
            )
            assert rounded >= run.settings["cutoff"] - TOLERANCE, run.name
        else:
            assert math.ceil(bound - TOLERANCE) >= math.ceil(objective - TOLERANCE), run.name


def test_every_stop_and_limit_fires_somewhere(runs: list[Run]):
    # The integer objective stop needs the bound past the LP's integer part before the last
    # iteration, which small instances rarely give: it may end every run proven.
    wanted: set[str] = set()

    for run in runs:
        wanted |= run.stops | ({run.limit} if run.limit else set())

    wanted.discard("IntegerObjectiveGap")
    seen = {run.result["status"] for run in runs}

    assert wanted <= seen, f"never fired: {sorted(wanted - seen)}"


def test_the_route_relaxations_order_their_lps(references: dict[tuple[str, str], float]):
    # No relaxation <= ng-routes, larger neighbourhoods higher, <= elementary routes; ng-routes
    # as large as the customers are elementary routes.
    by_instance: dict[str, dict[str, float]] = defaultdict(dict)

    for (instance, relaxation), objective in references.items():
        by_instance[instance][relaxation] = objective

    for instance, lps in by_instance.items():
        customers = (
            len(SolomonInstance(source_tree.TREE / "instances" / f"{instance}.txt").customers) - 1
        )
        order = sorted(lps, key=lambda name: relaxation_rank(name, customers))
        values = [lps[name] for name in order]

        for lower, higher in zip(values, values[1:]):
            assert lower <= higher + TOLERANCE * max(
                1.0, abs(higher)
            ), f"{instance}: {dict(zip(order, values))}"

        if "elementary" in lps:
            for name, objective in lps.items():
                if name.startswith("ng ") and int(name.split()[1]) >= customers:
                    assert same_lp(objective, lps["elementary"]), f"{instance}: {name}"


def relaxation_rank(name: str, customers: int) -> int:
    if name == "none":
        return 0

    if name == "elementary":
        return customers + 1

    return min(int(name.split()[1]), customers)


def test_the_integer_solution_passes_its_check_and_costs_at_least_the_lp(
    runs: list[Run], references: dict[tuple[str, str], float]
):
    solved = [run for run in runs if run.options.get("integer", {}).get("solve")]
    assert solved, "no run solves the integer master"

    for run in solved:
        lp = references[run.problem]
        assert run.result["integer_status"] == "Optimal", run.name
        assert run.result["solution_check"] == "passed", run.name
        assert run.result["integer_objective"] >= lp - TOLERANCE * max(1.0, abs(lp)), run.name


def test_every_run_made_every_debug_check(runs: list[Run]):
    # A failed check crashes its run, which fails the benchmark above; here, that no matrix's
    # own column_generation table dropped the checks of the file's defaults.
    every_check = list(ColumnGeneration.DEBUG_CHECKS)
    wrong = [run.name for run in runs if run.settings.get("debug_checks") != every_check]

    assert not wrong, f"runs without every debug check: {wrong}"


def test_a_half_way_point_is_used(runs: list[Run]):
    # A bidirectional search falls back silently (it logs, the LP is the same) when its
    # critical resource cannot hold a half-way point.
    wrong = [
        run.name
        for run in runs
        if any(stage.get("half_way_point") for stage in run.stages)
        and HALF_WAY_POINT_OFF in run.log
    ]

    assert not wrong, f"half-way point off: {wrong}"
