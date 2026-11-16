#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The options: blocks and names, the levels that override each other, the matrix and the
runs, and the errors raised before any run starts."""

import math
import tomllib
from pathlib import Path
from typing import Any

import pytest
from column_generation_benchmark import options
from column_generation_benchmark.options import OptionsError

# Checking the stages and tolerances imports rcspp: every test needs the tree's build.
pytestmark = pytest.mark.usefixtures("build")


def expand(text: str, command_line: dict[str, Any] | None = None) -> options.Expansion:
    """A benchmark file written as `text`, expanded."""
    return options.expand_benchmark(tomllib.loads(text), command_line or {})


# MARK: Presets


def test_the_presets_expand_to_one_run():
    expansion = expand('[defaults]\ninstance = "C101_25"\n')
    (run,) = expansion.runs

    assert expansion.benchmark == {"processes": 1, "repeats": 1}
    assert run.name == "C101_25-package-highspy-exact-artificial"
    assert run.options == {
        "instance": {"type": "solomon", "name": "C101_25"},
        "layer": "package",
        "master": {"type": "highspy", "parameters": {}},
        "pricing": {"stages": [{"algorithm": "simple"}]},
        "start": {"type": "artificial", "cost_factor": 1e4},
        "graph": {},
        "column_generation": {},
        "statistics": {"process": True},
    }


def test_the_only_pricing_preset_is_exact():
    (run,) = expand('[defaults]\ninstance = "C101_25"\npricing = "exact"\n').runs

    assert run.options["pricing"] == {"stages": [{"algorithm": "simple"}]}

    with pytest.raises(OptionsError, match="unknown pricing 'tabu'"):
        expand('[defaults]\ninstance = "C101_25"\npricing = "tabu"\n')


# MARK: Matrix, runs, rounds


BENCHMARK = """
[benchmark]
processes = 2
repeats = 3

[pricing.heuristics-first]
stages = [
  { algorithm = "greedy", max_solutions = 100 },
  { algorithm = "tabu", max_solutions = 20, max_iterations = 1000 },
  { algorithm = "simple" },
]

[master.one-thread]
type = "gurobipy"
parameters = { Threads = 1 }

[defaults]
start = { type = "single-customer" }

[matrix]
instance = ["C101_25", "R201_25"]
layer = ["package", "cpp-bindings"]
master = ["highspy", "one-thread"]
pricing = ["exact", "heuristics-first"]
"start.cost_factor" = [1e4]

[[runs]]
instance = "R201_50"
master = "gurobipy"
pricing = { stages = [ { algorithm = "simple", max_solutions = 5 }, { algorithm = "simple" } ] }
"""


def test_the_matrix_multiplies_and_runs_follow():
    text = BENCHMARK.replace('"start.cost_factor" = [1e4]\n', "")
    expansion = expand(text)
    names = [run.name for run in expansion.runs]

    assert expansion.benchmark == {"processes": 2, "repeats": 3}
    assert len(names) == 2 * 2 * 2 * 2 + 1
    assert names[0] == "C101_25-package-highspy-exact-single-customer"
    assert names[1] == "C101_25-package-highspy-heuristics-first-single-customer"
    assert names[-1].startswith("R201_50-package-gurobipy-pricing_")
    assert len(set(names)) == len(names)

    one_thread = expansion.runs[2].options["master"]
    assert one_thread == {"type": "gurobipy", "parameters": {"Threads": 1}}

    heuristics = expansion.runs[1].options["pricing"]["stages"]
    assert [stage["algorithm"] for stage in heuristics] == ["greedy", "tabu", "simple"]


def test_rounds_run_the_whole_list_each_time():
    expansion = expand(BENCHMARK.replace('"start.cost_factor" = [1e4]\n', ""))
    rounds = expansion.rounds()

    assert len(rounds) == 3 * len(expansion.runs)
    assert [number for number, _ in rounds[:2]] == [1, 1]
    assert rounds[len(expansion.runs)] == (2, expansion.runs[0])
    assert options.folder_name(expansion.runs[0], 2) == expansion.runs[0].name + "-r2"


def test_a_dotted_key_sets_one_key_of_a_block():
    text = """
[defaults]
instance = "C101_25"
[matrix]
"start.cost_factor" = [1e3, 1e5]
"master.parameters.Method" = [0]
"""
    runs = expand(text).runs

    assert [run.options["start"]["cost_factor"] for run in runs] == [1e3, 1e5]
    assert runs[0].options["master"]["parameters"] == {"Method": 0}
    assert runs[0].name == "C101_25-package-highspy-Method-0-exact-artificial-cost_factor-1000.0"


def test_runs_that_expand_alike_are_kept_once():
    text = '[defaults]\ninstance = "C101_25"\n[matrix]\nmaster = ["highspy", "gurobipy"]\n'
    runs = expand(text, {"master": "gurobipy"}).runs

    assert [run.name for run in runs] == ["C101_25-package-gurobipy-exact-artificial"]


def test_a_block_apart_from_the_defaults_is_named():
    text = '[[runs]]\ninstance = "C101_25"\ncolumn_generation = { max_iterations = 5 }\n'
    (run,) = expand(text).runs

    assert run.name.startswith("C101_25-package-highspy-exact-artificial-column_generation_")


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


# MARK: Overrides


def test_the_command_line_overrides_the_file():
    text = '[defaults]\ninstance = "C101_25"\nmaster = "gurobipy"\n'
    command_line = options.parse_assignments(
        ["--master", "highspy", "--repeats=2", "--column_generation.max_iterations", "7"]
    )
    expansion = expand(text, command_line)
    (run,) = expansion.runs

    assert run.options["master"]["type"] == "highspy"
    assert run.options["column_generation"] == {"max_iterations": 7}
    assert expansion.benchmark["repeats"] == 2


def test_a_file_definition_replaces_a_preset():
    text = '[pricing.exact]\nstages = [{ algorithm = "astar" }]\n[defaults]\ninstance = "C101_25"\n'
    (run,) = expand(text).runs

    assert run.options["pricing"] == {"stages": [{"algorithm": "astar"}]}


def test_a_run_file_and_the_command_line_make_one_run():
    heuristic = [{"algorithm": "tabu", "max_solutions": 20, "max_iterations": 1000}]
    stages = [*heuristic, {"algorithm": "simple"}]
    run_file = {"instance": "R201_25", "pricing": {"stages": stages}, "start": "single-customer"}
    command_line = options.parse_assignments(
        ["--stage", "tabu:max_solutions=20,max_iterations=1000", "--stage", "simple"]
    )

    with pytest.raises(OptionsError, match="not both"):
        options.parse_assignments(["--stage", "simple", "--pricing", "exact"])

    run = options.expand_run({"instance": "R201_25"}, command_line)
    tabu = options.expand_run(run_file, {})

    assert run.options["pricing"] == tabu.options["pricing"]
    assert tabu.name.startswith("R201_25-package-highspy-pricing_")
    assert tabu.name.endswith("-single-customer")


def test_command_line_values_are_read_as_toml():
    assert options.parse_assignments(
        ["--a", "8", "--b", "true", "--c", "C101_25", "--d", "{ type = 'gurobipy' }", "--e", "inf"]
    ) == {"a": 8, "b": True, "c": "C101_25", "d": {"type": "gurobipy"}, "e": math.inf}


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

    assert runs[0].options["column_generation"]["tolerances"] == {"type": "relative"}
    assert runs[1].options["column_generation"]["tolerances"]["reduced_cost"] == 1e-6


# MARK: Errors


@pytest.mark.parametrize(
    ("text", "message"),
    [
        ('[defaults]\nmaster = "highspy"\n', "instance is missing"),
        ('[defaults]\ninstance = "C101_25"\nlayer = "cpp"\n', "layer 'cpp' is not one of"),
        ('[defaults]\ninstance = "C101_25"\nmaster = "cplex"\n', "unknown master 'cplex'"),
        ('[defaults]\ninstance = { name = "C101_25" }\n', "instance needs a type"),
        (
            '[defaults]\ninstance = "C101_25"\nmaster = { Threads = 1 }\n',
            "'Threads' is not a parameter",
        ),
        (
            '[defaults]\ninstance = "C101_25"\nmaster = { type = "highspy", threads = 1 }\n',
            "'threads' is not a parameter of master",
        ),
        (
            '[defaults]\ninstance = "C101_25"\n"graph.ng_size" = 8\n',
            "'ng_size' is not a parameter of graph",
        ),
        ('[defaults]\ninstance = "C101_25"\nngsize = 8\n', "unknown option 'ngsize'"),
        ('[defaults]\ninstance = "C101_25"\npricing = { stages = [] }\n', "non-empty list"),
        (
            '[defaults]\ninstance = "C101_25"\n'
            'pricing = { stages = [{ algorithm = "dijkstra" }] }\n',
            "algorithm 'dijkstra'",
        ),
        (
            '[defaults]\ninstance = "C101_25"\n'
            'pricing = { stages = [{ algorithm = "simple", max_solution = 5 }] }\n',
            "'max_solution' is not a field of AlgorithmParams",
        ),
        (
            '[defaults]\ninstance = "C101_25"\n'
            'pricing = { stages = [{ algorithm = "simple", max_solutions = "five" }] }\n',
            "max_solutions = 'five' is refused",
        ),
        (
            '[defaults]\ninstance = "C101_25"\ncolumn_generation = { max_iterations = 1.5 }\n',
            "1.5 is not a whole number",
        ),
        (
            '[defaults]\ninstance = "C101_25"\ncolumn_generation = { tolerances = "loose" }\n',
            "tolerances 'loose' is not one of",
        ),
        (
            '[defaults]\ninstance = "C101_25"\n'
            'column_generation = { tolerances = { type = "relative", gap = 1 } }\n',
            "gap",
        ),
        ('[defaults]\ninstance = "C101_25"\n[matrix]\nmaster = "highspy"\n', "non-empty list"),
        ('[defaults]\ninstance = "C101_25"\n[matrix]\ngraph.ng_size = [0, 8]\n', "quote a dotted"),
        ("[benchmark]\nprocesses = 0\n", "at least 1"),
        ("[benchmark]\nthreads = 2\n", "unknown benchmark option 'threads'"),
        ("[machine]\nlabel = 'laptop'\n", "unknown section 'machine'"),
    ],
)
def test_wrong_options_are_refused(text: str, message: str):
    with pytest.raises(OptionsError, match=message):
        expand(text)


def test_command_line_errors():
    with pytest.raises(OptionsError, match="needs a value"):
        options.parse_assignments(["--instance"])

    with pytest.raises(OptionsError, match="--instance needs a value"):
        options.parse_assignments(["--instance", "--master", "highspy"])

    with pytest.raises(OptionsError, match="is not an option"):
        options.parse_assignments(["C101_25"])

    with pytest.raises(OptionsError, match="not name=value"):
        options.parse_assignments(["--stage", "tabu:max_solutions"])

    with pytest.raises(OptionsError, match="option of a benchmark"):
        options.expand_run({"instance": "C101_25"}, {"processes": 2})


def test_a_malformed_file_is_refused(tmp_path: Path):
    path = tmp_path / "benchmark.toml"
    path.write_text("[defaults\n")

    with pytest.raises(OptionsError, match="benchmark.toml"):
        options.read_toml(path)


# MARK: Merging, layers, run files, help


def test_a_table_merges_into_a_block_of_the_same_type():
    text = """
[defaults]
instance = "C101_25"
column_generation = { max_iterations = 5, timeout_s = 60 }
master = { type = "gurobipy", parameters = { Threads = 1 } }
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

    assert merged.options["column_generation"] == {"max_iterations": 10, "timeout_s": 60.0}
    assert merged.options["master"]["parameters"] == {"Threads": 1, "Method": 0}
    assert replaced.options["master"] == {"type": "highspy", "parameters": {}}
    assert untyped.options["master"] == {
        "type": "gurobipy",
        "parameters": {"Threads": 1, "Method": 1},
    }
    assert untyped.options["start"] == {"type": "artificial", "cost_factor": 1e3}


def test_a_matrix_drops_a_master_its_layer_has_not():
    text = """
[defaults]
instance = "C101_25"
[matrix]
layer = ["package", "cpp-bindings"]
master = ["highspy", "highs"]
"""
    expansion = expand(text)

    assert [run.name.split("-exact")[0] for run in expansion.runs] == [
        "C101_25-package-highspy",
        "C101_25-cpp-bindings-highspy",
        "C101_25-cpp-bindings-highs",
    ]
    assert len(expansion.dropped) == 1
    assert "layer package has no master highs" in expansion.dropped[0]


def test_a_run_asking_for_a_master_its_layer_has_not_is_refused():
    with pytest.raises(OptionsError, match="layer package has no master highs"):
        expand('[[runs]]\ninstance = "C101_25"\nmaster = "highs"\n')

    with pytest.raises(OptionsError, match="layer package has no master highs"):
        options.expand_run({"instance": "C101_25"}, {"master": "highs"})


def test_a_run_entry_can_start_from_a_run_file(tmp_path: Path):
    (tmp_path / "old").mkdir()
    (tmp_path / "old" / "run.toml").write_text(
        'instance = { type = "solomon", name = "R201_25" }\nmaster = { type = "gurobipy" }\n'
    )
    text = '[[runs]]\nfrom = "old/run.toml"\nmaster = "highspy"\n'
    (run,) = options.expand_benchmark(tomllib.loads(text), {}, folder=tmp_path).runs

    assert run.options["instance"]["name"] == "R201_25"
    assert run.options["master"]["type"] == "highspy"


def test_the_help_lists_every_option():
    import argparse

    parser = argparse.ArgumentParser(prog="runner")
    parser.add_argument("--output")
    flags, given = options.split_command_line(
        parser, ["--output", "out", "--master", "gurobipy", "--start.cost_factor", "1e3"]
    )

    assert flags.output == "out"
    assert given == {"master": "gurobipy", "start.cost_factor": 1e3}

    for option in options.RUN_OPTIONS:
        assert option in options.describe()
