#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The options: blocks, run options and complete runs, benchmark files, the command line,
run names, and the errors raised before any run starts."""

from __future__ import annotations

import argparse
import math
import re
import tomllib
from collections.abc import Callable
from dataclasses import replace
from pathlib import Path

import pytest
from column_generation_benchmark import options
from column_generation_benchmark.options import Benchmark, OptionsError, Run, RunOptions
from column_generation_benchmark.options.blocks import (
    ColumnGeneration,
    Instance,
    Integer,
    Master,
    Pricing,
    Start,
    Statistics,
)
from column_generation_benchmark.options.command_line import toml_value

# Checking the stages and tolerances imports rcspp: every test needs the tree's build.
pytestmark = pytest.mark.usefixtures("build")

#: A fingerprint in a run's name.
FINGERPRINT = "[0-9a-f]{6}"


def parse(arguments: list[str]) -> argparse.Namespace:
    """The command line of a benchmark: its own options and the run options."""
    parser = argparse.ArgumentParser(prog="benchmark", exit_on_error=False)
    options.add_benchmark_options(parser)
    options.add_run_options(parser)
    return parser.parse_args(arguments)


def expand(text: str, arguments: list[str] | None = None) -> options.Expansion:
    """A benchmark file written as `text`, with a command line, expanded."""
    benchmark = Benchmark.from_table(tomllib.loads(text))
    command_line = options.run_from_arguments(parse(arguments or []), benchmark.names)
    return benchmark.expand(command_line)


def run_of(text: str) -> Run:
    """The one run of a benchmark file."""
    (run,) = expand(text).runs
    return run


# MARK: Presets


def test_the_presets_expand_to_one_run():
    expansion = expand('[defaults]\ninstance = "C101_25"\n')
    (run,) = expansion.runs

    assert (expansion.processes, expansion.repeats) == (1, 1)
    assert run.name == "C101_25-package-highspy-exact-artificial"
    assert run.to_table() == {
        "instance": {"type": "solomon", "name": "C101_25"},
        "layer": "package",
        "master": {"type": "highspy", "parameters": {}},
        "pricing": {"stages": [{"algorithm": "simple"}]},
        "start": {"type": "artificial", "cost_factor": 1e4},
        "graph": {},
        "column_generation": {},
        "integer": {"solve": True, "timeout_s": 60.0, "column_type": "binary"},
        "statistics": {"process": True, "solution": True},
    }


def test_the_presets_hold_only_names_and_defaults(tmp_path: Path):
    path = tmp_path / "presets.toml"
    path.write_text('[defaults]\nlayer = "package"\n[matrix]\nmaster = ["highspy"]\n')

    with pytest.raises(OptionsError, match="presets hold named blocks and"):
        options.read_presets(path)

    assert set(options.read_presets()) <= {"defaults", *options.BLOCKS}


def test_the_only_pricing_preset_is_exact():
    run = run_of('[defaults]\ninstance = "C101_25"\npricing = "exact"\n')

    assert run.pricing == Pricing([{"algorithm": "simple"}])

    with pytest.raises(OptionsError, match="unknown pricing 'tabu'"):
        expand('[defaults]\ninstance = "C101_25"\npricing = "tabu"\n')


def test_the_artificial_start_preset_gives_the_cost_factor():
    assert run_of('[defaults]\ninstance = "C101_25"\n').start == Start("artificial", 1e4)

    # A table merges into the presets' start: the cost factor comes from below.
    text = '[defaults]\ninstance = "C101_25"\nstart = { type = "artificial" }\n'

    assert run_of(text).start == Start("artificial", 1e4)

    with pytest.raises(OptionsError, match="start.cost_factor is missing"):
        Start.from_table({"type": "artificial"}, "start")


def test_a_block_alone_must_be_complete():
    with pytest.raises(OptionsError, match="pricing.stages is missing"):
        Pricing.from_table({}, "pricing")

    run = run_of('[defaults]\ninstance = "C101_25"\npricing = {}\n')

    assert run.pricing.stages == [{"algorithm": "simple"}]


# MARK: Matrix, runs, rounds

BENCHMARK = """
[benchmark]
processes = 2
repeats = 3

[pricing.heuristics-first]
stages = [
  { algorithm = "greedy", max_solutions = 100, timeout_s = 60 },
  { algorithm = "tabu", max_solutions = 20, max_iterations = 1000 },
  { algorithm = "simple" },
]

[master.one-thread]
type = "gurobipy"
parameters = { Presolve = 1 }

[defaults]
start = { type = "single-customer" }

[matrix]
instance = ["C101_25", "R201_25"]
layer = ["package", "cpp-bindings"]
master = ["highspy", "one-thread"]
pricing = ["exact", "heuristics-first"]

[[runs]]
instance = "R201_50"
master = "gurobipy"
pricing = { stages = [ { algorithm = "simple", max_solutions = 5 }, { algorithm = "simple" } ] }
"""


def test_the_matrix_multiplies_and_runs_follow():
    expansion = expand(BENCHMARK)
    names = [run.name for run in expansion.runs]

    # Two processes: every run has one thread, which its name shows.
    threads = f"-column_generation_{FINGERPRINT}"

    assert (expansion.processes, expansion.repeats) == (2, 3)
    assert len(names) == 2 * 2 * 2 * 2 + 1
    assert re.fullmatch("C101_25-package-highspy-exact-single-customer" + threads, names[0])
    assert re.fullmatch(
        "C101_25-package-highspy-heuristics-first-single-customer" + threads, names[1]
    )
    assert re.fullmatch("C101_25-package-one-thread-exact-single-customer" + threads, names[2])
    assert re.fullmatch(
        f"R201_50-package-gurobipy-pricing_{FINGERPRINT}-single-customer" + threads, names[-1]
    )
    assert {run.column_generation.threads for run in expansion.runs} == {1}
    assert len(set(names)) == len(names)
    assert expansion.runs[2].master == Master("gurobipy", {"Presolve": 1})
    assert [stage["algorithm"] for stage in expansion.runs[1].pricing.stages] == [
        "greedy",
        "tabu",
        "simple",
    ]


def test_rounds_run_the_whole_list_each_time():
    expansion = expand(BENCHMARK)
    rounds = expansion.rounds()

    assert len(rounds) == 3 * len(expansion.runs)
    assert [number for number, _ in rounds[:2]] == [1, 1]
    assert rounds[len(expansion.runs)] == (2, expansion.runs[0])
    assert options.folder_name(expansion.runs[0], 2) == expansion.runs[0].name + "-r2"


def test_a_dotted_name_sets_one_parameter():
    text = """
[defaults]
instance = "C101_25"
[matrix]
"start.cost_factor" = [1e3, 1e5]
"master.parameters.Method" = [0]
"""
    runs = expand(text).runs

    assert [run.start.cost_factor for run in runs] == [1e3, 1e5]
    assert runs[0].master.parameters == {"Method": 0}


def test_runs_that_expand_alike_are_kept_once():
    text = '[defaults]\ninstance = "C101_25"\n[matrix]\nmaster = ["highspy", "gurobipy"]\n'
    runs = expand(text, ["--master", "gurobipy"]).runs

    assert [run.name for run in runs] == ["C101_25-package-gurobipy-exact-artificial"]


def test_preprocessing_off_keeps_the_stages_and_names_the_pricing():
    on, off = expand(RUN + '[matrix]\n"pricing.preprocess" = [true, false]\n').runs

    assert (on.pricing.preprocess, off.pricing.preprocess) == (True, False)
    assert off.pricing.stages == [{"algorithm": "simple"}]
    assert re.fullmatch(f"C101_25-package-highspy-pricing_{FINGERPRINT}-artificial", off.name)
    assert run_of(RUN).pricing.to_table() == {"stages": [{"algorithm": "simple"}]}


def test_several_matrices_expand_one_after_the_other():
    text = """
[defaults]
instance = "C101_25"
[[matrix]]
layer = ["package"]
master = ["highspy", "gurobipy"]
[[matrix]]
layer = ["cpp-bindings", "cpp-native"]
master = ["highs", "highspy"]
[[matrix]]
master = ["highspy"]
[[runs]]
instance = "R201_25"
"""
    expansion = expand(text)

    # The last matrix's point is the first one's again, kept once.
    assert [(run.instance.name, run.layer, run.master.type) for run in expansion.runs] == [
        ("C101_25", "package", "highspy"),
        ("C101_25", "package", "gurobipy"),
        ("C101_25", "cpp-bindings", "highs"),
        ("C101_25", "cpp-bindings", "highspy"),
        ("C101_25", "cpp-native", "highs"),
        ("R201_25", "package", "highspy"),
    ]
    assert len(expansion.dropped) == 1
    assert "layer cpp-native has no master highspy" in expansion.dropped[0]


def test_without_a_matrix_or_runs_the_defaults_make_one_run():
    run = run_of('[defaults]\ninstance = "R201_25"\nlayer = "cpp-bindings"\n')

    assert (run.instance.name, run.layer) == ("R201_25", "cpp-bindings")


# MARK: Names


def test_a_run_is_named_after_its_options_however_they_were_written():
    text = """
[master.one-thread]
type = "gurobipy"
parameters = { Presolve = 1 }
[[runs]]
instance = "C101_25"
master = "one-thread"
[[runs]]
instance = "C101_25"
master = { type = "gurobipy", parameters = { Presolve = 1 } }
start = { type = "artificial", cost_factor = 1e4 }
"""
    (run,) = expand(text).runs

    assert run.name == "C101_25-package-one-thread-exact-artificial"


def test_a_block_without_a_name_gets_its_type_and_a_fingerprint():
    text = '[[runs]]\ninstance = "C101_25"\nmaster = { type = "highspy", parameters = { a = 1 } }\n'
    name = run_of(text).name

    assert re.fullmatch(f"C101_25-package-highspy_{FINGERPRINT}-exact-artificial", name)


def test_a_block_apart_from_the_defaults_is_named():
    text = '[[runs]]\ninstance = "C101_25"\ncolumn_generation = { max_iterations = 5 }\n'
    name = run_of(text).name
    expected = f"C101_25-package-highspy-exact-artificial-column_generation_{FINGERPRINT}"

    assert re.fullmatch(expected, name)


def test_same_names_get_a_fingerprint():
    text = """
[[runs]]
instance = "C101_25"
pricing = { stages = [ { algorithm = "simple", max_solutions = 5 } ] }
[[runs]]
instance = "C101_25"
pricing = { stages = [ { algorithm = "simple", max_solutions = 6 } ] }
[[runs]]
instance = "C101_25"
"pricing.stages" = [ { algorithm = "simple", max_solutions = 5 } ]
"""
    names = [run.name for run in expand(text).runs]

    assert len(names) == 2
    assert names[0] != names[1]

    # A definition named as another type, and that type alone: one name, then a fingerprint
    # each.
    text = """
[master.gurobipy]
type = "highspy"
parameters = { simplex_strategy = 1 }
[[runs]]
instance = "C101_25"
master = "gurobipy"
[[runs]]
instance = "C101_25"
master = { type = "gurobipy" }
"""
    names = [run.name for run in expand(text).runs]

    assert len(set(names)) == 2

    for name in names:
        assert re.fullmatch(f"C101_25-package-gurobipy-exact-artificial-{FINGERPRINT}", name)


def test_names_that_differ_only_in_case_get_a_fingerprint():
    # Windows and macOS file systems take Fast and fast for one folder.
    text = """
[master.Fast]
type = "highspy"
parameters = { simplex_strategy = 1 }
[master.fast]
type = "highspy"
parameters = { simplex_strategy = 4 }
[defaults]
instance = "C101_25"
[matrix]
master = ["Fast", "fast"]
"""
    names = [run.name for run in expand(text).runs]

    assert len({name.casefold() for name in names}) == 2
    assert all(
        re.fullmatch(f"C101_25-package-(Fast|fast)-exact-artificial-{FINGERPRINT}", name)
        for name in names
    )


def test_a_named_integer_block_names_the_run():
    text = '[integer.off]\nsolve = false\n[[runs]]\ninstance = "C101_25"\ninteger = "off"\n'

    assert run_of(text).name == "C101_25-package-highspy-exact-artificial-off"


def test_a_run_is_named_alike_by_a_benchmark_and_by_its_runner():
    # The benchmark file's [defaults] show in the name, as the runner shows the run file's.
    text = '[defaults]\ninstance = "C101_25"\nstatistics = { process = false }\n'
    by_benchmark = run_of(text)
    by_runner = options.expand_run({"instance": "C101_25", "statistics": {"process": False}})

    assert by_benchmark.name == by_runner.name
    assert re.fullmatch(
        f"C101_25-package-highspy-exact-artificial-statistics_{FINGERPRINT}", by_runner.name
    )


# MARK: Merging, names lookup


def test_a_block_merges_into_the_one_below_unless_it_names_another_type():
    text = """
[defaults]
instance = "C101_25"
column_generation = { max_iterations = 5, timeout_s = 60 }
master = { type = "gurobipy", parameters = { Presolve = 1 } }
[[runs]]
column_generation = { max_iterations = 10 }
master = { type = "gurobipy", parameters = { Method = 0 } }
[[runs]]
master = "highspy"
[[runs]]
master = { parameters = { Method = 1 } }
start.cost_factor = 1e3
"""
    merged, replaced, untyped = expand(text).runs

    assert merged.column_generation == ColumnGeneration(10, 60.0)
    assert merged.master.parameters == {"Presolve": 1, "Method": 0}
    assert replaced.master == Master("highspy", {})
    assert untyped.master.parameters == {"Presolve": 1, "Method": 1}
    assert untyped.start == Start("artificial", 1e3)


def test_a_definition_comes_before_a_type_and_an_instance_name():
    text = """
[master.highspy]
type = "highspy"
parameters = { simplex_strategy = 1 }
[instance.C101_25]
type = "solomon"
name = "R201_25"
[[runs]]
instance = "C101_25"
master = "highspy"
"""
    run = run_of(text)

    assert run.master.parameters == {"simplex_strategy": 1}
    assert run.instance == Instance("solomon", "R201_25")


def test_a_file_definition_replaces_a_preset():
    text = '[pricing.exact]\nstages = [{ algorithm = "astar" }]\n[defaults]\ninstance = "C101_25"\n'

    assert run_of(text).pricing.stages == [{"algorithm": "astar"}]


# MARK: Layers


def test_a_matrix_drops_a_master_its_layer_has_not():
    text = """
[defaults]
instance = "C101_25"
[matrix]
layer = ["package", "cpp-bindings"]
master = ["highspy", "highs"]
"""
    expansion = expand(text)

    assert [(run.layer, run.master.type) for run in expansion.runs] == [
        ("package", "highspy"),
        ("cpp-bindings", "highspy"),
        ("cpp-bindings", "highs"),
    ]
    assert len(expansion.dropped) == 1
    assert "layer package has no master highs" in expansion.dropped[0]


def test_a_matrix_that_drops_every_point_is_refused():
    text = '[defaults]\ninstance = "C101_25"\n[matrix]\nmaster = ["highs"]\n'

    with pytest.raises(OptionsError, match="no run left: 1 matrix point dropped"):
        expand(text)


def test_a_command_line_master_drops_the_matrix_points_whose_layer_has_not_it():
    text = '[defaults]\ninstance = "C101_25"\n[matrix]\nlayer = ["package", "cpp-bindings"]\n'
    expansion = expand(text, ["--master", "highs"])

    assert [(run.layer, run.master.type) for run in expansion.runs] == [("cpp-bindings", "highs")]
    assert len(expansion.dropped) == 1


def test_a_run_asking_for_a_master_its_layer_has_not_is_refused():
    with pytest.raises(OptionsError, match="layer package has no master highs"):
        expand('[[runs]]\ninstance = "C101_25"\nmaster = "highs"\n')

    command_line = RunOptions.from_table({"master": "highs"}, "command line")

    with pytest.raises(OptionsError, match="layer package has no master highs"):
        options.expand_run({"instance": "C101_25"}, command_line)


# MARK: Run files


def test_a_run_entry_can_start_from_a_run_file(tmp_path: Path):
    (tmp_path / "old").mkdir()
    (tmp_path / "old" / "run.toml").write_text(
        'instance = "R201_25"\nmaster = { type = "gurobipy", parameters = { Presolve = 1 } }\n'
    )
    text = '[[runs]]\nfrom = "old/run.toml"\nmaster = "highspy"\n'
    benchmark = Benchmark.from_table(tomllib.loads(text), folder=tmp_path)
    (run,) = benchmark.expand().runs

    assert run.instance.name == "R201_25"
    assert run.master == Master("highspy", {})

    absolute = f'[[runs]]\nfrom = "{(tmp_path / "old" / "run.toml").as_posix()}"\n'
    (again,) = Benchmark.from_table(tomllib.loads(absolute)).expand().runs

    assert again.master == Master("gurobipy", {"Presolve": 1})


def test_a_missing_run_file_is_refused(tmp_path: Path):
    with pytest.raises(OptionsError, match="missing.toml"):
        Benchmark.from_table({"runs": [{"from": "missing.toml"}]}, folder=tmp_path)


def test_a_recorded_run_file_reads_back_the_same():
    stages = [{"algorithm": "tabu", "max_iterations": 10}, {"algorithm": "simple"}]
    run = options.expand_run(
        {
            "instance": "R201_25",
            "layer": "cpp-bindings",
            "master": {"type": "highs", "parameters": {"simplex_strategy": 1}},
            "pricing": {"stages": stages},
            "start": "single-customer",
            "column_generation": {"max_iterations": 5, "timeout_s": 60, "tolerances": "relative"},
            "statistics": {"process": False},
        }
    )
    again = options.expand_run(run.to_table())

    assert again == run
    assert again.name == run.name


def test_a_hand_written_run_file_needs_only_what_differs():
    text = 'instance = "R201_25"\nmaster = "gurobipy"\n"column_generation.timeout_s" = 600\n'
    run = options.expand_run(tomllib.loads(text))
    expected = f"R201_25-package-gurobipy-exact-artificial-column_generation_{FINGERPRINT}"

    assert re.fullmatch(expected, run.name)
    assert run.column_generation.timeout_s == 600.0


def test_a_run_file_holds_run_options_only():
    with pytest.raises(OptionsError, match="unknown option 'defaults'"):
        options.expand_run({"instance": "C101_25", "defaults": {}})

    with pytest.raises(OptionsError, match="'one-thread' is not a parameter of master"):
        options.expand_run({"instance": "C101_25", "master.one-thread": {}})


# MARK: Command line


def test_the_command_line_overrides_the_file():
    text = '[defaults]\ninstance = "C101_25"\nmaster = "gurobipy"\n'
    arguments = parse(
        ["--master", "highspy", "--repeats", "2", "--column_generation.max_iterations", "7"]
    )
    benchmark = options.benchmark_from_arguments(
        Benchmark.from_table(tomllib.loads(text)), arguments
    )
    expansion = benchmark.expand(options.run_from_arguments(arguments, benchmark.names))
    (run,) = expansion.runs

    assert run.master.type == "highspy"
    assert run.column_generation.max_iterations == 7
    assert expansion.repeats == 2


def test_the_benchmark_options_come_from_the_file_then_the_command_line():
    text = '[benchmark]\ntimeout = 30\nmachine = "laptop"\nlogs = false\nrepeats = 2\n'
    text += '[defaults]\ninstance = "C101_25"\n'
    benchmark = Benchmark.from_table(tomllib.loads(text))

    assert (benchmark.timeout, benchmark.machine, benchmark.logs) == (30.0, "laptop", False)
    assert Benchmark().timeout is None and Benchmark().logs

    arguments = parse(["--timeout", "5", "--machine", "desktop", "--processes", "3"])
    expansion = options.benchmark_from_arguments(benchmark, arguments).expand()

    assert (expansion.processes, expansion.repeats, expansion.timeout) == (3, 2, 5.0)
    assert (expansion.machine, expansion.logs) == ("desktop", False)
    assert options.benchmark_from_arguments(Benchmark(), parse(["--no-logs"])).logs is False

    with pytest.raises(OptionsError, match="timeout must be a positive number"):
        options.benchmark_from_arguments(benchmark, parse(["--timeout", "-1"]))

    for arguments in (["--repeats", "0"], ["--processes", "0"]):
        with pytest.raises(OptionsError, match="at least 1"):
            options.benchmark_from_arguments(benchmark, parse(arguments))


def test_a_command_line_can_name_a_benchmark_file_definition():
    text = '[pricing.first]\nstages = [{ algorithm = "astar" }]\n[defaults]\ninstance = "C101_25"\n'
    benchmark = Benchmark.from_table(tomllib.loads(text))
    command_line = options.run_from_arguments(parse(["--pricing", "first"]), benchmark.names)
    (run,) = benchmark.expand(command_line).runs

    assert run.pricing.stages == [{"algorithm": "astar"}]
    assert run.name == "C101_25-package-highspy-first-artificial"


def test_a_name_and_a_dotted_name_in_one_place():
    text = """
[master.one-thread]
type = "gurobipy"
parameters = { Presolve = 1 }
[[runs]]
instance = "C101_25"
master = "one-thread"
"master.parameters.Method" = 0
"""

    assert run_of(text).master == Master("gurobipy", {"Presolve": 1, "Method": 0})


@pytest.mark.parametrize(
    ("text", "arguments", "message"),
    [
        # A runner: its run file alone, then with a command line.
        (None, [], "^run file: start single-customer has no cost_factor"),
        (None, ["--master", "gurobipy"], "^run file, with the command line: start single-customer"),
        # A benchmark: its [defaults], then with a command line.
        (
            '[defaults]\ninstance = "C101_25"\n'
            'pricing = { stages = [{ algorithm = ["simple"] }] }\n',
            [],
            r"^benchmark file: \[defaults\]: stages\[0\]",
        ),
        (
            '[defaults]\nmaster = "highspy"\n',
            ["--master", "highspy"],
            r"^benchmark file: \[defaults\], with the command line: instance is missing",
        ),
    ],
)
def test_an_error_names_the_place_it_comes_from(
    text: str | None, arguments: list[str], message: str
):
    with pytest.raises(OptionsError, match=message):
        if text is None:
            run_file = {
                "instance": "C101_25",
                "start": {"type": "single-customer", "cost_factor": 1},
            }
            options.expand_run(run_file, options.run_from_arguments(parse(arguments)))
        else:
            expand(text, arguments)


def test_a_runner_refuses_a_layer_that_is_not_a_name():
    with pytest.raises(OptionsError, match="layer {} is not one of"):
        options.expand_run(
            {"instance": "C101_25"}, options.run_from_arguments(parse(["--layer", "{}"]))
        )


@pytest.mark.parametrize("flag", ["--out", "--start.cost"])
def test_a_shortened_flag_is_refused(flag: str):
    from column_generation_benchmark import runner

    with pytest.raises(SystemExit):
        runner.parse_arguments(None, [flag, "3"])


def test_runs_side_by_side_get_one_thread_unless_they_set_theirs():
    text = (
        '[benchmark]\nprocesses = 2\n[defaults]\ninstance = "C101_25"\n'
        '[[runs]]\n[[runs]]\n"column_generation.threads" = 4\n'
    )
    one, own = expand(text).runs
    alone, _ = expand(text.replace("processes = 2", "processes = 1")).runs

    assert (one.column_generation.threads, own.column_generation.threads) == (1, 4)
    assert alone.column_generation.threads is None
    # Named after its threads, as any run that sets them.
    assert alone.name == "C101_25-package-highspy-exact-artificial"
    assert re.fullmatch(f"{alone.name}-column_generation_{FINGERPRINT}", one.name)


def test_a_runner_command_line_overrides_its_run_file_and_can_name_a_preset():
    arguments = parse(["--pricing", "exact", "--master", "gurobipy", "--start", "single-customer"])
    run_file = {"instance": "C101_25", "pricing": {"stages": [{"algorithm": "astar"}]}}
    run = options.expand_run(run_file, options.run_from_arguments(arguments))

    assert run.pricing.stages == [{"algorithm": "simple"}]
    assert run.master.type == "gurobipy"
    assert run.start == Start("single-customer")


PARAMETERS: list[tuple[str, str, Callable[[Run], bool]]] = [
    ("--instance.name", "R201_25", lambda run: run.instance.name == "R201_25"),
    (
        "--master.parameters",
        "{ Presolve = 1 }",
        lambda run: run.master.parameters == {"Presolve": 1},
    ),
    ("--start.cost_factor", "1e3", lambda run: run.start.cost_factor == 1e3),
    (
        "--column_generation.max_iterations",
        "5",
        lambda run: run.column_generation.max_iterations == 5,
    ),
    ("--column_generation.timeout_s", "60", lambda run: run.column_generation.timeout_s == 60.0),
    (
        "--column_generation.tolerances",
        "relative",
        lambda run: run.column_generation.tolerances == {"type": "relative"},
    ),
    ("--statistics.process", "false", lambda run: run.statistics == Statistics(False)),
    ("--layer", "cpp-bindings", lambda run: run.layer == "cpp-bindings"),
    ("--graph", "{}", lambda run: run.graph.to_table() == {}),
]


def test_the_fleet_and_the_stops_on_the_command_line():
    arguments = [
        *("--instance.vehicles", "25"),
        *("--column_generation.absolute_gap", "0.5"),
        *("--column_generation.relative_gap", "0.01"),
        *("--column_generation.cutoff", "190"),
        *("--column_generation.integer_objective", "true"),
    ]
    command_line = options.run_from_arguments(parse(arguments))
    run = options.expand_run({"instance": "C101_25"}, command_line)

    assert run.instance.vehicles == 25
    settings = run.column_generation
    assert (settings.absolute_gap, settings.relative_gap, settings.cutoff) == (0.5, 0.01, 190.0)
    assert settings.integer_objective is True


def test_stops_without_a_fleet_are_accepted_and_named():
    stops = "column_generation = { relative_gap = 0.01, integer_objective = false }\n"
    run = expand('[defaults]\ninstance = "C101_25"\n' + stops).runs[0]

    # Accepted: without the fleet row they cannot stop the run, which goes on as without them.
    assert options.stops_without_fleet(run) == [
        "column_generation.relative_gap",
        "column_generation.integer_objective",
    ]

    with_fleet = replace(run, instance=replace(run.instance, vehicles=25))
    assert options.stops_without_fleet(with_fleet) == []


def test_a_run_with_a_fleet_is_named_after_it():
    text = (
        '[[runs]]\ninstance = "C101_25"\n"instance.vehicles" = 25\n'
        '[[runs]]\ninstance = "C101_25"\n'
    )
    names = [run.name for run in expand(text).runs]

    assert names[0].startswith("C101_25-vehicles_25-package-")
    assert names[1].startswith("C101_25-package-")


@pytest.mark.parametrize(("argument", "value", "check"), PARAMETERS)
def test_every_parameter_has_its_command_line_option(
    argument: str, value: str, check: Callable[[Run], bool]
):
    command_line = options.run_from_arguments(parse([argument, value]))

    assert check(options.expand_run({"instance": "C101_25"}, command_line))


def test_stages_on_the_command_line():
    arguments = parse(["--stage", "tabu:max_solutions=20,max_iterations=1000", "--stage", "simple"])
    run = options.expand_run({"instance": "R201_25"}, options.run_from_arguments(arguments))

    assert run.pricing.stages == [
        {"algorithm": "tabu", "max_solutions": 20, "max_iterations": 1000},
        {"algorithm": "simple"},
    ]

    with pytest.raises(OptionsError, match="not both"):
        options.run_from_arguments(parse(["--stage", "simple", "--pricing", "exact"]))


def test_a_stage_on_the_command_line_takes_lists_and_spaces():
    arguments = parse(
        [
            "--stage",
            " tabu : max_iterations = 10, tabu_exempt_node_ids = [0, 26]",
            "--stage",
            "simple:",
        ]
    )
    run = options.expand_run({"instance": "R201_25"}, options.run_from_arguments(arguments))

    assert run.pricing.stages == [
        {"algorithm": "tabu", "max_iterations": 10, "tabu_exempt_node_ids": [0, 26]},
        {"algorithm": "simple"},
    ]


def test_command_line_values_are_read_as_toml():
    texts = ("8", "true", "C101_25", "{ type = 'gurobipy' }", "inf", "1\nx = 2", "a = b")

    assert [toml_value(text) for text in texts] == [
        8,
        True,
        "C101_25",
        {"type": "gurobipy"},
        math.inf,
        "1\nx = 2",
        "a = b",
    ]


@pytest.mark.parametrize(
    "arguments",
    [
        ["--instance"],
        ["--instance", "--master", "highspy"],
        ["--ngsize", "8"],
        ["--stage", "tabu:max_solutions"],
        ["--processes", "two"],
    ],
)
def test_argparse_refuses_what_it_does_not_know(arguments: list[str]):
    with pytest.raises((SystemExit, argparse.ArgumentError)):
        parse(arguments)


def test_the_help_lists_every_option_and_parameter():
    parser = argparse.ArgumentParser(prog="runner")
    options.add_run_options(parser)
    text = parser.format_help()

    for name in ("--instance", "--master", "--start.cost_factor", "--master.parameters", "--stage"):
        assert name in text


# MARK: Checks


def test_stages_take_any_algorithm_params_field():
    stage = {
        "algorithm": "tabu",
        "max_iterations": 1000,
        "direction": "forward",
        "tabu_exempt_node_ids": [0, 26],
        "timeout_s": 2,
    }
    algorithm, params = options.make_algorithm_params(stage)

    assert algorithm == "tabu"
    assert params.max_iterations == 1000
    assert params.tabu_exempt_node_ids == {0, 26}
    assert params.timeout_s == 2.0

    with pytest.raises(OptionsError, match="direction 'up' is not one of"):
        options.make_algorithm_params({"algorithm": "simple", "direction": "up"})


def test_tolerances_by_name_or_table():
    text = """
[[runs]]
instance = "C101_25"
column_generation = { tolerances = "relative" }
[[runs]]
instance = "C101_25"
column_generation = { tolerances = { type = "absolute", reduced_cost = 1e-6 } }
"""
    runs = expand(text).runs

    assert runs[0].column_generation.tolerances == {"type": "relative"}
    assert runs[1].column_generation.tolerances == {"type": "absolute", "reduced_cost": 1e-6}


def test_tolerances_by_name_merge_as_a_table():
    text = """
[defaults]
instance = "C101_25"
column_generation = { tolerances = "relative" }
[[runs]]
"column_generation.tolerances.reduced_cost" = 1e-7
[[runs]]
column_generation = { tolerances = "absolute" }
"column_generation.tolerances.feasibility" = 1e-8
"""
    below, same_place = expand(text).runs

    assert below.column_generation.tolerances == {"type": "relative", "reduced_cost": 1e-7}
    assert same_place.column_generation.tolerances == {"type": "absolute", "feasibility": 1e-8}


def test_the_integer_solve_takes_its_defaults_from_the_instance_type():
    text = """
[defaults]
instance = "C101_25"

[[runs]]

[[runs]]
"integer.solve" = false

[[runs]]
integer = { column_type = "integer", timeout_s = 5, relative_gap = 0.01, threads = 2 }
"""
    default, off, given = expand(text).runs

    # A Solomon instance: on, binary, as its type declares; a minute at most.
    assert default.integer == Integer(solve=True, timeout_s=60.0, column_type="binary")
    assert off.integer == Integer(solve=False, timeout_s=60.0, column_type="binary")
    assert given.integer == Integer(True, 5.0, 0.01, "integer", 2)


def test_an_integer_block_as_the_instance_type_declares_it_is_not_named():
    text = """
[defaults]
instance = "C101_25"

[[runs]]
"integer.solve" = true

[[runs]]
"integer.solve" = false
"""
    same, off = expand(text).runs

    assert same.name == "C101_25-package-highspy-exact-artificial"
    assert re.fullmatch(f"{same.name}-integer_{FINGERPRINT}", off.name)


def test_a_run_without_a_layer_is_refused():
    with pytest.raises(OptionsError, match="layer is missing"):
        RunOptions().complete()


RUN = '[defaults]\ninstance = "C101_25"\n'
VEHICLES = RUN + '"instance.vehicles" = 25\n'


@pytest.mark.parametrize(
    ("text", "message"),
    [
        # Options and names.
        ('[defaults]\nmaster = "highspy"\n', "instance is missing"),
        (RUN + 'layer = "cpp"\n', "layer 'cpp' is not one of"),
        (RUN + '"layer.x" = 1\n', "layer has no parameters"),
        (RUN + 'master = "cplex"\n', "unknown master 'cplex'"),
        (RUN + "master = 3\n", "master is a name or a table"),
        (RUN + "ngsize = 8\n", "unknown option 'ngsize'"),
        (RUN + 'master = "highspy"\n"master.type.x" = 1\n', "master.type is not a table"),
        # Blocks.
        ('[defaults]\ninstance = { name = "C101_25" }\n', "instance needs a type"),
        ('[defaults]\ninstance = { type = "solomon" }\n', "instance.name is missing"),
        ('[defaults]\ninstance = { type = "solomon", name = 3 }\n', "3 is not a string"),
        (RUN + "master = { Presolve = 1 }\n", "'Presolve' is not a parameter"),
        (
            RUN + 'master = { type = "highspy", simplex_strategy = 1 }\n',
            "'simplex_strategy' is not a parameter",
        ),
        (RUN + 'master = { type = "highspy", parameters = 1 }\n', "1 is not a table"),
        (RUN + 'start = { type = "single-customer", cost_factor = 1 }\n', "has no cost_factor"),
        (RUN + 'start = { type = "artificial", cost_factor = "big" }\n', "'big' is not a number"),
        (RUN + '"graph.ng" = 8\n', "'ng' is not a parameter of graph"),
        (RUN + '"graph.relaxation" = "full"\n', "relaxation 'full' is not one of none, ng"),
        (RUN + '"graph.relaxation" = "ng"\n', "relaxation ng needs graph.ng_size"),
        (RUN + '"graph.ng_size" = 8\n', "graph.ng_size is for relaxation ng only"),
        (
            RUN + 'graph = { relaxation = "elementary", ng_size = 8 }\n',
            "graph.ng_size is for relaxation ng only",
        ),
        (RUN + 'graph = { relaxation = "ng", ng_size = 1 }\n', "ng_size must be at least 2"),
        (RUN + 'graph = { relaxation = "ng", ng_size = 8.5 }\n', "8.5 is not a whole number"),
        (RUN + "statistics = { process = 3 }\n", "3 is not true or false"),
        (RUN + "pricing = { stages = [] }\n", "non-empty list"),
        (RUN + "pricing = { stages = [1] }\n", r"stages\[0\] is not a table"),
        (RUN + '"pricing.preprocess" = "no"\n', "'no' is not true or false"),
        (RUN + 'pricing = { stages = [{ algorithm = "dijkstra" }] }\n', "algorithm 'dijkstra'"),
        (
            RUN + 'pricing = { stages = [{ algorithm = "tabu" }] }\n',
            "a tabu stage needs a finite max_iterations",
        ),
        (
            RUN + 'pricing = { stages = [{ algorithm = "greedy", direction = "backward" }] }\n',
            "a greedy stage searches forward only",
        ),
        (
            RUN + 'pricing = { stages = [{ algorithm = "tabu", max_iterations = 10, '
            'direction = "bidirectional" }] }\n',
            "a tabu stage searches forward only",
        ),
        (
            RUN + 'pricing = { stages = [{ algorithm = "simple", max_solution = 5 }] }\n',
            "'max_solution' is not a field of AlgorithmParams",
        ),
        (
            RUN + 'pricing = { stages = [{ algorithm = "simple", max_solutions = "five" }] }\n',
            "max_solutions = 'five' is refused",
        ),
        (RUN + "column_generation = { max_iterations = 1.5 }\n", "1.5 is not a whole number"),
        (RUN + 'column_generation = { tolerances = "loose" }\n', "tolerances 'loose' is not one"),
        (RUN + 'column_generation = { tolerances = { type = "relative", gap = 1 } }\n', "gap"),
        (RUN + "column_generation = { threads = -1 }\n", "threads must be 0 or more"),
        (RUN + "column_generation = { threads = 1.5 }\n", "1.5 is not a whole number"),
        (RUN + "column_generation = { max_iterations = -3 }\n", "max_iterations must be 0 or more"),
        (RUN + "column_generation = { timeout_s = -5 }\n", "timeout_s must be 0 or more"),
        (RUN + "column_generation = { timeout_s = nan }\n", "timeout_s must be 0 or more"),
        (RUN + "integer = { relative_gap = nan }\n", "relative_gap must be 0 or more"),
        (
            VEHICLES + "column_generation = { absolute_gap = -1 }\n",
            "absolute_gap must be 0 or more",
        ),
        (VEHICLES + "column_generation = { relative_gap = nan }\n", "relative_gap must be 0 or"),
        (VEHICLES + "column_generation = { cutoff = nan }\n", "cutoff must be a number"),
        (VEHICLES + "column_generation = { integer_objective = 1 }\n", "1 is not true or false"),
        (RUN + '"instance.vehicles" = 0\n', "instance.vehicles must be 1 or more"),
        (RUN + '"instance.vehicles" = 2.5\n', "2.5 is not a whole number"),
        (RUN + 'layer = ["package"]\n', r"layer \['package'\] is not one of"),
        (RUN + "integer = { solve = 1 }\n", "1 is not true or false"),
        (RUN + "integer = { timeout_s = 0 }\n", "timeout_s must be a positive, finite number"),
        (RUN + "integer = { timeout_s = inf }\n", "timeout_s must be a positive, finite number"),
        (RUN + "integer = { relative_gap = -0.1 }\n", "relative_gap must be 0 or more"),
        (RUN + 'integer = { column_type = "real" }\n', "column_type 'real' is not one of"),
        (RUN + "integer = { threads = -1 }\n", "integer.threads must be 0 or more"),
        (RUN + "integer = { gap = 0.1 }\n", "'gap' is not a parameter of integer"),
        (
            RUN + 'master = { type = "highs", parameters = { threads = 1 } }\n',
            "master.parameters.threads is set by every solve.*use column_generation.threads",
        ),
        (
            RUN + 'master = { type = "highspy", parameters = { output_flag = true } }\n',
            "master.parameters.output_flag is set by every solve, from the layer's own parameters$",
        ),
        (
            RUN + 'master = { type = "gurobipy", parameters = { timeLIMIT = 5 } }\n',
            "master.parameters.timeLIMIT is set by every solve.*use column_generation.timeout_s",
        ),
        (
            RUN + 'master = { type = "highspy", parameters = { solve_relaxation = true } }\n',
            "master.parameters.solve_relaxation is set by every solve",
        ),
        (
            RUN + 'master = { type = "highs", parameters = { mip_rel_gap = 0.1 } }\n',
            "master.parameters.mip_rel_gap is set by every solve.*use integer.relative_gap",
        ),
        (
            RUN + 'master = { type = "gurobipy", parameters = { MIPGap = 0.1 } }\n',
            "master.parameters.MIPGap is set by every solve.*use integer.relative_gap",
        ),
        # Benchmark files.
        (RUN + '[matrix]\nmaster = "highspy"\n', "non-empty list"),
        (RUN + "[matrix]\ngraph.ng_size = [0, 8]\n", "quote a dotted"),
        ("matrix = 1\n", r"\[matrix\] is a table of lists, or \[\[matrix\]\] several"),
        ("matrix = []\n", r"\[\[matrix\]\] holds at least one matrix"),
        ("matrix = [1]\n", r"matrix\[0\] is a table of lists"),
        ('[[matrix]]\nmaster = ["highspy"]\n[[matrix]]\nmaster = "gurobipy"\n', "non-empty list"),
        ("[runs]\nmaster = 1\n", "runs is a list of tables"),
        ("runs = [1]\n", "a run is a table of options"),
        ("[master]\none-thread = 1\n", r"\[master\] holds named master tables"),
        ('master = "highspy"\n', r"\[master\] holds named master tables"),
        ("[benchmark]\nprocesses = 0\n", "at least 1"),
        ("[benchmark]\nrepeats = 0\n", "at least 1"),
        ("[benchmark]\nrepeats = 2.5\n", "at least 1"),
        ("benchmark = 1\n", r"\[benchmark\] is a table"),
        ('benchmark = "ab"\n', r"\[benchmark\] is a table"),
        ("[[runs]]\nfrom = 1\n", "from is the path of a run file"),
        ("[benchmark]\ntimeout = 0\n", "timeout must be a positive number"),
        ("[benchmark]\ntimeout = inf\n", "timeout must be a positive number"),
        ("[benchmark]\ntimeout = '1 h'\n", "is not a number"),
        ("[benchmark]\nmachine = 1\n", "is not a string"),
        ("[benchmark]\nlogs = 'no'\n", "is not true or false"),
        ("[benchmark]\nthreads = 2\n", "unknown benchmark option 'threads'"),
        ("[machine]\nlabel = 'laptop'\n", "unknown section 'machine'"),
    ],
)
def test_wrong_options_are_refused(text: str, message: str):
    with pytest.raises(OptionsError, match=message):
        expand(text)


def test_a_malformed_file_is_refused(tmp_path: Path):
    path = tmp_path / "benchmark.toml"
    path.write_text("[defaults\n")

    with pytest.raises(OptionsError, match="benchmark.toml"):
        options.read_toml(path)
