# Options

How to say what a run does and which runs a benchmark makes: in TOML files, on the command line, or both.

## Words

| word | what | example |
|---|---|---|
| option | a top-level name of a run | `instance`, `layer`, `master` |
| block | the table an option holds; every option but `layer` | `{ type = "gurobipy", parameters = { Threads = 1 } }` |
| type | the key of a block that picks its object, for the blocks that have several | `type = "gurobipy"` |
| parameter | any other key of a block | `cost_factor` in `start` |
| name | a word standing for a whole block: a definition, a preset, a type, or an instance's name | `master = "one-thread"` |

Three kinds of settings, never mixed:

| kind | what they decide | where they go | in a run's `run.toml` |
|---|---|---|---|
| run options | what is solved and how: change one and the result may change | a run file; `[defaults]`, `[matrix]`, `[[runs]]` of a benchmark file; `--name value` | yes, expanded |
| benchmark options | how a list of runs is carried out: `processes`, `repeats` | `[benchmark]` of a benchmark file; the benchmark's command line | no; a runner refuses them |
| flags | where things are and what to do: `--file`, `--output`, `--build`, `--machine`, `--check` | the command line | no; `command.txt` has them |

A run option `column_generation.timeout_s` stops the solve from inside; a benchmark's timeout kills a run's process.

## Enough TOML

```toml
layer = "package"                                # name = value: text, number, true/false, [lists]
start = { type = "artificial", cost_factor = 1e4 }   # { ... }: a table on one line

[master.one-thread]                              # [header]: the lines below go into this table
type = "gurobipy"                                # (dots in a header nest: master → one-thread)
parameters = { Threads = 1 }

[[runs]]                                         # [[header]]: one more table in the list `runs`
instance = "C101_25"
[[runs]]                                         # a second one
instance = "R201_25"
```

A header holds every line up to the next header, so a file's top-level names come before its first header.

A quoted name with dots, `"start.cost_factor" = 1e3`, is one name: it sets one parameter of a block. Unquoted,
`start.cost_factor = 1e3` is TOML for `start = { cost_factor = 1e3 }`: a table, which merges into `start` as below.
In `[matrix]`, quote it.

## The options of a run

| option | example | types and parameters |
|---|---|---|
| `instance` | `"C101_25"`, or `{ type = "solomon", name = "C101_25" }` | `solomon`: `name`, a file of the tree's `instances/` |
| `layer` | `"package"` | `package`, `cpp-bindings` |
| `master` | `{ type = "gurobipy", parameters = { Method = 0 } }` | `highspy`, `gurobipy`, `highs`; `parameters` go to the solver as they are |
| `pricing` | `{ stages = [{ algorithm = "simple" }] }` | no type; `stages`: each stage is `algorithm` and any field of `rcspp.graph.AlgorithmParams` |
| `start` | `"artificial"`, `{ type = "artificial", cost_factor = 1e3 }` | `artificial` (`cost_factor`); `single-customer`, one route per customer |
| `graph` | `{}` | no parameters yet |
| `column_generation` | `{ max_iterations = 100, timeout_s = 60, tolerances = "relative" }` | no type; tolerances by name or `{ type = "absolute", reduced_cost = 1e-6 }` |
| `statistics` | `{ process = false }` | no type; one parameter per statistic, on or off |

The masters each layer has:

| layer | masters |
|---|---|
| `package` | `highspy`, `gurobipy` |
| `cpp-bindings` | `highs`, and `highspy`, `gurobipy` through the bindings |

`options.describe()` lists every option with its types and parameters; a runner's `--help` prints it.

## Where a run's options come from

From the lowest to the highest, each overriding the ones before:

1. the presets' `[defaults]` ([`presets.toml`](column_generation_benchmark/presets.toml)): `package`, `highspy`,
   `exact`, `artificial`;
2. the benchmark file's `[defaults]`;
3. the run file a `[[runs]]` entry names with `from`;
4. the run's own entry: one point of `[matrix]`, or one `[[runs]]` entry;
5. the command line.

A name is replaced by its table first. Then a table **merges** into the block below it: the parameters it gives
replace those, the others stay (`master = { parameters = { Method = 0 } }` keeps a `Threads` set below). A table
that names another `type` **replaces** the block: `master = "highspy"` over a gurobipy block keeps none of its
parameters. A quoted dotted name sets one parameter, after the source's whole blocks.

Presets: `pricing = "exact"`, the exact search alone. A file defines its own names under an option's name
(`[pricing.heuristic-first]`), and may replace a preset by defining its name.

## A benchmark file

```toml
[benchmark]                     # benchmark options
processes = 1
repeats = 3                     # rounds: every run once, then again

[pricing.heuristic-first]       # a definition: a name for a block
stages = [
  { algorithm = "greedy", max_solutions = 100, timeout_s = 60 },   # capped in solutions and in time
  { algorithm = "simple" },                                        # the exact search for the proof
]

[defaults]                      # every run of this file starts from these
instance = "C101_25"
start = "single-customer"

[matrix]                        # one run per combination: here 2 × 3 × 2 = 12, minus 2 below
instance = ["C101_25", "R201_25"]
master = ["highspy", "gurobipy", "highs"]
layer = ["package", "cpp-bindings"]

[[runs]]                        # runs outside the product
instance = "R201_50"
pricing = "heuristic-first"
column_generation = { timeout_s = 600 }

[[runs]]                        # a recorded run again, one option changed
from = "old/runs/R201_50-package-gurobipy-exact-artificial-r1/run.toml"
master = "highspy"
```

- A matrix point whose layer has not its master (`package` with `highs`) is dropped and listed before the first run;
  the same pair in a `[[runs]]` entry or on the command line is an error.
- `from` is read from the benchmark file's folder.
- Runs that come out the same are kept once.
- Everything is checked before the first run: an unknown name or parameter, or a wrong value, stops the benchmark.

## The command line

```bash
--master gurobipy                         # an option: a name
--master "{ type = 'gurobipy', parameters = { Threads = 1 } }"   # or a table
--start.cost_factor 1e3                   # one parameter
--stage tabu:max_solutions=20,max_iterations=1000 --stage simple  # the pricing's stages, in order
```

A value is read as TOML (`8`, `true`, `inf`, `{ ... }`), else as text; `--name=value` works too. A value cannot
start with `--`. The command line overrides the files.

## A run's name

A run's folder is named after its instance, layer, master, pricing and start, then any other block that differs from
the defaults: `C101_25-package-highspy-exact-artificial-r1`. A block is named by the name it was given, or its type,
or a short fingerprint of its table (`pricing_e1c445`), then the parameters set one by one
(`artificial-cost_factor-1000.0`).

A run's `run.toml` holds its options expanded, names replaced by their tables, so it runs the same after a preset
changes.
