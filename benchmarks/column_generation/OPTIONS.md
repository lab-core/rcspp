# Options

How to say what a run does and which runs a benchmark makes: in [TOML](https://toml.io) files, on the command line,
or both. The first sections explain how options are given; [the reference](#reference) lists every one.

## Words

| word | what | example |
|---|---|---|
| option | a top-level name of a run | `instance`, `layer`, `master` |
| block | the table an option holds; every option but `layer` | `{ type = "gurobipy", parameters = { Presolve = 1 } }` |
| type | the key of a block that picks its object, for the blocks that have several | `type = "gurobipy"` |
| parameter | any other key of a block | `cost_factor` in `start` |
| name | a word standing for a whole block: a preset, a definition of your own, a type, an instance's name | `master = "one-thread"` |

Three kinds of settings, never mixed:

| kind | what they decide | in a run's `run.toml` |
|---|---|---|
| run options | what is solved and how: change one and the result may change | yes, expanded |
| benchmark options | how a list of runs is carried out: `processes`, `repeats` | no |
| flags | where things are and what to do, such as the output folder | no; `command.txt` has them |

A run option `column_generation.timeout_s` stops the solve from inside; a benchmark's timeout kills a run's process.

## What each place can hold

| place | named blocks | `[defaults]` | run options | `[matrix]`, `[[runs]]` | benchmark options |
|---|---|---|---|---|---|
| [`presets.toml`](column_generation_benchmark/presets.toml) | yes | yes | no | no | no |
| a benchmark file | yes | yes | in `[defaults]`, `[matrix]`, `[[runs]]` | yes | `[benchmark]` |
| a run file | no | no | yes, at the top level | no | no |
| a runner's command line | no | no | yes, over the run file | no | no |
| `benchmark.py`'s command line | no | no | yes, over every run | no | `--processes`, `--repeats` |

Anything else in a place stops everything with an error, before the first run.

## A run file

A run file is one run: its options at the top level. Write only what differs from the presets' defaults
(`package`, `highspy`, `exact`, `artificial`):

```toml
instance = "R201_25"
master = { type = "gurobipy", parameters = { Presolve = 1 } }
start = "single-customer"
"column_generation.timeout_s" = 600     # quoted: one parameter of the block
```

The `run.toml` a run records holds every option, expanded, so the run reads the same after a preset changes:

```toml
instance = { type = "solomon", name = "R201_25" }
layer = "package"
master = { type = "gurobipy", parameters = { Presolve = 1 } }
pricing = { stages = [{ algorithm = "simple" }] }
start = { type = "single-customer" }
graph = {}
column_generation = { timeout_s = 600.0 }
integer = { solve = true, timeout_s = 60.0, column_type = "binary" }
statistics = { process = true, solution = true }
```

## Where a run's options come from

From the lowest to the highest, each over the ones before:

1. the presets' `[defaults]`;
2. the benchmark file's `[defaults]`;
3. the run file a `[[runs]]` entry names with `from`;
4. the run's own entry: one point of `[matrix]`, or one `[[runs]]` entry (for a runner: its run file);
5. the command line.

Each place's names are first replaced by their tables. Then a block **merges** into the block below it: the
parameters it gives replace those below, the others stay. A block that names another `type` **replaces** the one
below, keeping nothing of it.

```toml
[defaults]
master = { type = "gurobipy", parameters = { Presolve = 1 } }

[[runs]]                                  # gurobipy, Presolve = 1 and Method = 0: merged
master = { parameters = { Method = 0 } }

[[runs]]                                  # highspy, no parameters: another type replaces
master = "highspy"
```

## Names: presets and your own

A block can be given by a name, looked up in this order:

1. a named block of the place's file, or of the presets: `[master.one-thread]` defines `master = "one-thread"`;
2. a type: `master = "gurobipy"` is `{ type = "gurobipy" }`;
3. for `instance` only, an instance's name: `instance = "C101_25"` is `{ type = "solomon", name = "C101_25" }`.

The presets ([`presets.toml`](column_generation_benchmark/presets.toml)):

| name | what |
|---|---|
| `pricing = "exact"` | the exact search alone: `{ stages = [{ algorithm = "simple" }] }` |
| `start = "artificial"` | big-M with the package's cost factor: `{ type = "artificial", cost_factor = 1e4 }` |

Define your own in a benchmark file, under the option's name, and use it anywhere in that file, in the run files its
`[[runs]]` name with `from`, and on `benchmark.py`'s command line:

```toml
[master.one-thread]
type = "gurobipy"
parameters = { Presolve = 1 }

[pricing.heuristic-first]
stages = [{ algorithm = "tabu", max_solutions = 20, max_iterations = 1000 }, { algorithm = "simple" }]

[matrix]
master = ["highspy", "one-thread"]
pricing = ["exact", "heuristic-first"]
```

A definition with a preset's name replaces the preset in that file (`[pricing.exact]`). A run file and a runner's
command line can use the presets' names, not define any.

## Dotted names

`<option>.<parameter> = value` sets one parameter and leaves the block's others as they are. Deeper tables work too:
`master.parameters.Presolve`.

| where | how |
|---|---|
| a run file, `[defaults]`, a `[[runs]]` entry | `"start.cost_factor" = 1e3`, quoted |
| `[matrix]` | `"start.cost_factor" = [1e3, 1e5]`, quoted: unquoted, TOML reads a table and the matrix refuses it |
| the command line | `--start.cost_factor 1e3`, `--master.parameters "{ Presolve = 1 }"` |

Unquoted in a run file, `start.cost_factor = 1e3` is TOML for `start = { cost_factor = 1e3 }`: a block without a
type, which merges the same way. In one place, dotted names apply after the whole blocks: `master = "one-thread"`
then `"master.parameters.Method" = 0` gives both parameters.

## Reference

### `instance`

| type | parameters | name alone |
|---|---|---|
| `solomon` | `name`: a file `instances/<name>.txt` of the tree, in Solomon's VRPTW format | `instance = "C101_25"` |

### `layer`

`package` (the shipped pure-Python package) or `cpp-bindings` (the C++ example through its Python bindings). Not a
block: a name only. A runner runs its own layer: when neither its run file nor its command line names one, the run's
layer is the runner's; one that names another layer is refused. `--layer` on the command line goes over the run
file's, so a run of one layer runs again on another.

### `master`

| type | what | layers |
|---|---|---|
| `highspy` | HiGHS through its Python package; on `cpp-bindings` the model is copied to C++ and solved there | `package`, `cpp-bindings` |
| `gurobipy` | Gurobi through its Python package; needs a licence | `package`, `cpp-bindings` |
| `highs` | HiGHS from C++, the example's own master; needs the example built with HiGHS | `cpp-bindings` |

`parameters` (a table, empty by default) goes to the solver as it is: `{ Presolve = 1, Method = 0 }` for Gurobi. In
a `[matrix]`, a layer and a master that do not go together are dropped and listed before the first run; anywhere
else they are an error.

Every master solve sets some of the solver's parameters from the layer's own, and puts the given values back after
it: they would never reach a solve, so they are refused here. Threads, the time limit and the MIP gap have run
options instead.

| solver | refused | instead |
|---|---|---|
| HiGHS (`highspy`, `highs`) | `threads`, `time_limit`, `output_flag`, `solve_relaxation`, `mip_rel_gap` | `column_generation.threads`, `column_generation.timeout_s`, `integer.relative_gap` |
| Gurobi (`gurobipy`; names in any case) | `Threads`, `TimeLimit`, `OutputFlag`, `MIPGap` | `column_generation.threads`, `column_generation.timeout_s`, `integer.relative_gap` |

### `pricing`

No type. `stages`, a non-empty list, tried in order: a stage that finds no column hands over to the next. Each stage
is `algorithm` and any field of `rcspp.graph.AlgorithmParams`:

| key | what | default |
|---|---|---|
| `algorithm` | `simple`, `pushing`, `pulling`, `astar`, `bidirectional` (exact); `greedy`, `tabu` (heuristic) | required |
| `max_solutions` | stop after this many columns | no limit |
| `max_iterations` | a heuristic's iterations; `tabu` needs it | no limit |
| `timeout_s` | the stage's time limit, in seconds | none |
| `direction` | `forward`, `backward`, `bidirectional` (by name) | `forward` |
| any other field | as `AlgorithmParams` documents it; a set is written as a list (`tabu_exempt_node_ids = [0]`) | the engine's |

A heuristic stage should be followed by an exact one: without it, no iteration ends with a proof. Without a cap,
`greedy` can enumerate every path; give it `max_solutions` and `timeout_s`. On the command line, `--stage` writes a
stage as `algorithm:key=value,key=value` (see [the command line](#the-command-line)).

### `start`

| type | what | parameters |
|---|---|---|
| `artificial` | big-M: one costly artificial column per row, from the layer | `cost_factor`, required |
| `single-customer` | one route per customer, from the instance | none |

Each artificial column costs `cost_factor` times the largest arc cost (at least 1). The preset `artificial` gives
1e4, the package's own factor: a larger cost gives large duals, on which HiGHS's warm-started dual simplex can fail;
a smaller one misses the solutions whose columns cost more than it per row they cover. `{ type = "artificial" }`
alone merges into the default start and keeps its factor.

### `graph`

No type, no parameters yet: `{}`.

### `column_generation`

No type; the column generation's own parameters. Each is optional: absent, the layer's default holds.

| parameter | what | absent |
|---|---|---|
| `max_iterations` | the most LP solves | no limit |
| `timeout_s` | the run's wall-clock limit, in seconds | no limit |
| `tolerances` | `absolute` or `relative`, by name or as a table with `reduced_cost`, `feasibility`, `optimality_gap` | the layer's (the package's: `absolute`) |
| `threads` | the master solver's threads in every solve, a whole number; 0 lets the solver choose | 0 |

`tolerances = "relative"` is `{ type = "relative" }`; `{ type = "absolute", reduced_cost = 1e-6 }` sets one value.

### `integer`

No type. Once column generation is over, the **integer master over the generated columns**: the same master with
its generated columns made integer, solved by the master's solver. It gives a feasible solution and an upper bound,
not the integer optimum: there is no branching, and the best solution may need columns column generation never
produced.

| parameter | what | absent |
|---|---|---|
| `solve` | solve it or not | the instance type's: on for `solomon` |
| `timeout_s` | its time limit, in seconds: positive and finite, as it can take far longer than the LP | 60 |
| `relative_gap` | the solver's MIP gap, at which it stops | the layer's (the package's: 1e-4) |
| `column_type` | `integer`, or `binary` (integer, at most 1) | the instance type's: `binary` for `solomon` |
| `threads` | the solver's threads in the integer solve; 0 lets the solver choose | `column_generation.threads` |

With rows covered exactly once, as Solomon's, `integer` and `binary` give the same solutions; `binary` gives the
solver the bounds. Its time is `integer_seconds`, outside `solve_seconds`, so column generation times compare
whether it is on or off. Different masters build different columns, so their integer results can differ even where
their LP values are equal. A run that sets these as its instance type does is named as one that leaves them out.

### `statistics`

No type. One parameter per statistic, on or off:

| parameter | what it adds to `result.json` | costs the run | default |
|---|---|---|---|
| `process` | the process's peak memory and processor time | two system calls once the run is over | on |
| `solution` | `solution_columns`; writes `solution.json`, the last LP solution and the integer one (see the README) | the LP's values and duals, read once the run is over; one more LP solve when a limit stopped the run after pricing | on |

## A benchmark file

```toml
[benchmark]                     # benchmark options
processes = 1                   # runs at a time
repeats = 3                     # rounds: every run once, then again

[pricing.heuristic-first]       # a definition
stages = [
  { algorithm = "greedy", max_solutions = 100, timeout_s = 60 },   # capped in solutions and in time
  { algorithm = "simple" },                                        # the exact search for the proof
]

[defaults]                      # a run under every other one
start = "single-customer"

[matrix]                        # a run whose options are lists: 2 × 3 × 2 = 12 runs, minus 2 dropped
instance = ["C101_25", "R201_25"]
master = ["highspy", "gurobipy", "highs"]
layer = ["package", "cpp-bindings"]

[[runs]]                        # one more run, outside the product
instance = "R201_50"
pricing = "heuristic-first"
"column_generation.timeout_s" = 600

[[runs]]                        # a recorded run again, one option changed
from = "old/runs/R201_50-package-gurobipy-exact-artificial-r1/run.toml"
master = "highspy"
```

- The runs are the matrix's combinations, then the `[[runs]]`; without either, one run of the defaults.
- `from` is a path, relative to the benchmark file's folder or absolute; the entry's options go over the file's.
- Runs that come out the same are kept once.
- Every run is expanded and checked before the first one starts.

## The command line

Every run option is `--<option> <value>` and every parameter `--<option>.<parameter> <value>`; both go over the files.
A script's `--help` lists them all, with its own flags.

```bash
--master gurobipy                                   # a name
--master "{ type = 'gurobipy', parameters = { Presolve = 1 } }"   # a table
--start.cost_factor 1e3                             # one parameter
--master.parameters "{ Presolve = 1, Method = 0 }"   # a solver's parameters: one table
--stage tabu:max_solutions=20,max_iterations=1000 --stage simple   # the pricing's stages, in order
--processes 2 --repeats 3                           # benchmark.py only
```

- A value is read as a TOML value (`8`, `true`, `inf`, `{ ... }`), else as text: `--instance C101_25`.
  `--name=value` works too.
- `--stage` and `--pricing` do not go together.
- A runner's flags (`--file`, `--output`, `--overwrite`, `--build`, `--machine`, `--check build`) are in the
  [README](README.md#run-once).
- An unknown option, or one without its value, stops the command.
- Quote a table as one argument, with single quotes inside for its strings: `"{ type = 'gurobipy' }"` reads the same
  in bash and in PowerShell. Double quotes inside single quotes (`'{ type = "gurobipy" }'`) work in bash and in
  PowerShell 7.3 or later, not in Windows PowerShell 5.1, which drops them.

## A run's name

A run's folder is named after its instance, layer, master, pricing and start, then any other block that differs from
the defaults, then its round: `C101_25-package-highspy-exact-artificial-r1`. A block shows as:

1. the name of the preset or definition it equals: `one-thread`, `exact`;
2. else its type, when it is that type with its defaults: `highspy`, `single-customer`;
3. else its type (or its option) and a fingerprint of its table: `artificial_3fa2c1`, `column_generation_9b01e4`.

The name depends only on a run's options, not on how they were written. Two different runs that would get the same
name each get a fingerprint of their options added.
