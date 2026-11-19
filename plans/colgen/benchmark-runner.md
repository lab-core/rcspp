# Benchmark runner (`T-1.10`): design

Design chat 2026-11-16, decided by the user (`D-177`–`D-187`, `D-189`–`D-194`). Starting point, kept as records:
`benchmark-results/colgen/drivers.py`, `run_matrix.py`, `full_comparison_verdict.py`. Coding steps: §9.

## 1. Shape (`D-177`, `D-178`, `D-189`)

| piece | what |
|---|---|
| PR | a follow-up based on #88's branch, merged after the stack; results stay on the plan branch |
| folder | `benchmarks/column_generation/`, not in the wheel; it measures the tree it lives in (`python/src` and the tree's `build*/python` `_core`), the source guard kept (`F-59` (1)) |
| runners | one per layer, each a script that does one run and can be called alone; its body is the column generation calls, so it is also the example a new user reads |
| benchmark | `benchmark.py`: takes one or more runs, starts each runner as its own process, at most `processes` at a time; one run = a simple runner call |
| contract | a runner takes options and/or a run file and an output folder, does one run, writes its files there (§4); the benchmark knows only this, never imports a runner |
| docs | the folder's `README.md`: install the dependencies, build `_core` with the example on, build the C++ runner when it exists, run one runner, run a benchmark, read the output (`D-187`) |

| layer | runner | what runs | masters (`D-190`) |
|---|---|---|---|
| `package` | `runners/package_runner.py` | `rcspp.column_generation`, the shipped pure-Python package | `highspy`, `gurobipy` |
| `cpp-bindings` | `runners/cpp_bindings_runner.py` | the C++ example layer through its bindings: C++ pricer and loop | `highs`; `highspy`, `gurobipy` through the Python master adapter |
| `cpp-native` | the C++ program of `T-1.19` | pure C++, no Python (`D-164`); added by `T-1.19` | `highs`; `gurobi` when built with it |

Dropped: the example's Python loop over the bindings (measured only loop against loop) and the old Python VRP
example (`T-1.18` compares it once, then ports it onto the package).

`processes` defaults to 1. A benchmark with more records it and its summary flags the timings; Gurobi gets
`Threads = 1` then unless set (`D-178`).

## 2. Input: TOML and building blocks (`D-184`)

- One name per option, usable as `--name value` or as a key in a file; the command line overrides the file. TOML:
  read by `tomllib` (Python ≥ 3.11), comments allowed.
- Every building block is a table: `type` picks the object from a fixed list, the other keys are its parameters.
  Unknown keys are an error before any run starts. No code in a file.
- Any block can be named and reused (`[masters.gurobi-one-thread]`, then `master = "gurobi-one-thread"`). The
  built-in presets live in a default TOML file shipped with the runners, in the same format as the user's.
- Where the package already makes objects from a name and parameters (`registry`: tolerances, initializations), the
  runner uses it.

| block | example | notes |
|---|---|---|
| `instance` | `{ type = "solomon", name = "C101_25" }`, shorthand `"C101_25"` | §5 |
| `graph` | `{ ng_size = 8 }` | `T-1.16` adds `ng_size` |
| `master` | `{ type = "gurobipy", parameters = { Method = 0 } }` | per layer (§1); `parameters` passed to the solver as they are |
| `pricing` | `{ stages = [ { algorithm = "tabu", max_solutions = 20, max_iterations = 1000 }, { algorithm = "simple" } ] }` | stage keys are the engine's `AlgorithmParams` fields, `timeout_s` caps a stage; preset `exact` only (`D-192`) |
| `start` | `{ type = "single-customer" }`, `{ type = "artificial", cost_factor = 1e4 }` | `artificial` from the layer; `single-customer` (one route per customer) from the instance (`D-191`) |
| `column_generation` | tolerances, limits | the package's own parameters |
| `statistics` | `{ per_iteration = true }` | collectors on or off (§6) |

Command-line shorthand for quick runs: repeatable `--stage tabu:max_solutions=20,max_iterations=1000`.

A benchmark file:

```toml
[benchmark]
processes = 1
repeats = 3                     # rounds: every run once, then again (§3)

[pricing.heuristics-first]
stages = [
  { algorithm = "greedy", max_solutions = 100, timeout_s = 60 },
  { algorithm = "tabu", max_solutions = 20, max_iterations = 1000 },
  { algorithm = "simple" },
]

[defaults]
start = { type = "single-customer" }

[matrix]                        # the product of these lists; any key, dotted for a block's key
instance = ["C101_25", "R201_25"]
layer = ["package", "cpp-bindings"]
master = ["highspy", "gurobipy", "highs"]   # package × highs dropped and listed (D-190)
pricing = ["exact", "heuristics-first"]
"graph.ng_size" = [0, 8]

[[runs]]                        # runs outside the product
instance = "R201_50"
layer = "package"
master = "gurobipy"
pricing = { stages = [ { algorithm = "simple", max_solutions = 5 }, { algorithm = "simple" } ] }

[[runs]]                        # a recorded run again, one option changed (D-194)
from = "old/runs/R201_50-package-gurobipy-exact-artificial-r1/run.toml"
master = "highspy"
```

The benchmark expands everything before the first run; a run's `run.toml` holds the expanded values, not the names,
so it runs the same after a preset changes. Then it checks (`D-191`): `options` always (layer × master, instance
files, starts; lists the runs and their folders), and `build` unless `--no-check`: each runner with `--check build`
builds instance, graph, master, pricer and starting columns without solving, once per run.

## 3. Repeats (`D-184`)

`repeats = N` runs the whole list N times as rounds, so slow changes in the machine (heat, a background job) spread
over every cell instead of one cell's repeats. Folders `runs/<name>-r<k>/`. The summary gives per cell the median,
min, max and spread over the rounds.

## 4. Output (`D-185`)

```text
<output>/
  benchmark.toml      the input, expanded: the list of runs
  metadata.json       start, end, processes, runs, rounds; machine, source, versions (§7)
  summary.md, summary.csv
  status.txt          the runs in progress
  runs/<instance>-<layer>-<master>-<pricing>-...-r<k>/
    run.toml          this run's expanded options: `<runner> --file run.toml` runs it again
    command.txt       the exact command line
    metadata.json     start, end, exit code, the load during the run (§7)
    result.json       status, objective, iterations, columns, artificial columns, master, pricing, other and solve
                      seconds; the statistics collectors' fields
    log.txt           unless --no-logs
```

- A run folder with a `result.json` is skipped: a benchmark resumes. A `STOP` file stops it after the runs in
  progress; `--timeout` kills a run's process tree and records status `timeout`.
- `result.json` replaces the `RESULT <json>` line, also for the C++ program (amends `D-164`'s output detail). Its
  fields are defined once, in the shared module; a test checks every runner's file against them.
- Writing `run.toml` needs a TOML writer (`tomllib` only reads): `tomli-w`, in the folder's requirements.

## 5. Instances (`D-186`)

An interface in the shared module: read the instance, build the graph and the master's rows, give starting columns
(`single-customer` belongs here: it is a VRP idea; each type declares its starts, `D-191`). One implementation,
Solomon VRPTW, picked by `type` from a dict.
No discovery. Later, if users need their own: `type = "module:Class"`, an import path, without changing the format.

## 6. Statistics (`D-181`)

One collector per statistic: a name, its cost, an option turning it on; it adds fields to `result.json` or writes a
per-iteration file. Now: the totals, the process measures and the summary's per-cell figures, and
`summary --compare A B` (`D-139`'s verdict, extra seconds per iteration). Later, each with the task that needs it:
per-iteration values through the package's `on_iteration` (`T-1.20`), a per-iteration time split (if `T-1.20`
fails), per-stage and engine counts and the Lagrangian bound (`T-2.5`). Solutions, the integer solve at the end and
per-iteration data: `benchmark-solutions.md` (`D-203`–`D-206`).

## 7. What each run records (`D-180`)

| group | fields |
|---|---|
| source | commit, uncommitted changes or not, branch |
| code | `rcspp` and `_core` paths; the build's configuration and compiler, from its `CMakeCache.txt` |
| versions | Python, `rcspp`, numpy; highspy and HiGHS, gurobipy and Gurobi when imported; the C++ program's solvers |
| machine | a label the user gives (no host name), OS, CPU, cores, memory |
| benchmark | processes, solver threads |
| load, sampled during the run | other processes' CPU mean and max, memory in use, CPU frequency, on battery or not |

A benchmark refuses to resume with a different commit, versions or machine unless `--allow-mixed`; the summary
lists them and checks `D-164`'s same solver versions across the layers per cell.

## 8. Shared code and layers (`D-186`), growth, out of scope

- Shared, never duplicated: options and TOML, instances and graphs, record, collectors, the run folder, the
  `result.json` fields. A runner holds only the column generation calls (about 100 lines). `package` and
  `cpp-bindings` look alike on purpose: merged, they would hide the code a reader looks for.
- `cpp-native` shares no Python (`D-193`): it reads an expanded `run.toml`, writes `result.json`, supports
  `--check build`; it ports the instance (tested arc for arc against Python's); `benchmark.py` writes its record.
- A test keeps the layers from drifting: every runner built gives the same objective, iterations and columns on
  C101_25.

| task | adds (`D-182`) |
|---|---|
| `T-1.18` | nothing: the port runs on the package; its comparison uses the old drivers once |
| `T-1.19` | the `cpp-native` runner and its build in the README |
| `T-1.16` | `graph.ng_size` |
| `T-1.21` | the engine's features as stage keys, or one key each in `graph` or `pricing` |
| `T-1.20` | presets of truncated stages; `--compare` over three layers; per-iteration data (`D-206`) |

Out of scope (`D-183`): `T-1.9`; phase 5's grid and `T-2.5`'s records. The master solver benchmark is set aside
(`D-179`): the runner serves for now. The runner moves no new sparse data (`D-16` not triggered).

## 9. Coding steps (later commits of the PR)

Steps 1–4 done 2026-11-16 (`F-62`); what they change for steps 5–9: `F-62`, the README status.

`D-190`–`D-196` folded into steps 1–4 2026-11-16 (rebuilt from `0597e768`, the rename folded too; force-pushed,
old head local `backup/benchmark-steps-1-4-8e944acd`): 1 `58a9071b` the README without options; 2 `6c3b19cc` the
`options/` package (`D-195`; a table merges unless it names another `type`), `presets.toml` with `exact` only,
`OPTIONS.md`; 3 `67822d53` `STARTS` per instance type (`D-191`); 4 `f1aff5e7`. Tests and hooks pass at each; 75 tests.

Then, on top (2026-11-17, pushed): `76c4f444` pyright strict; `589966ad` the options as dataclasses (`D-197`–`D-200`:
`RunOptions`, `Run`, `Benchmark`, argparse; places; `cost_factor` in the presets; names from the complete run),
`OPTIONS.md` as the full reference (`D-201`), tests at 100 % of `options/` (`D-202`).

Step 5 done 2026-11-17 (`F-63`, `61cf282c`): the flags and the folder writing are in the shared `runner.py`, for
step 6 too. Then (`benchmark-solutions.md`): step 5's fixup (`--overwrite`, `solution.json`, `D-203`), done and
folded 2026-11-17 (`F-64`; step 5 is now `374d2ea6`);
the integer solve and the instance's resolved solution after step 6 (`D-204`, `D-205`); per-iteration data with
`T-1.20` (`D-206`).

Step 6 done 2026-11-17 (`F-65`, `f97e6f73` on `374d2ea6`; 170 tests): `cpp_bindings_runner.py`; the
example's package via `source_tree.use_example()`; a run's layer defaults to the runner's; both runners build before
touching the folder (`F-63` (1) corrected); the runner tests parametrized over both layers, with the drift test.
Then (2026-11-17, `F-66`): `36c9490b` `column_generation.threads` (`D-209`, `Q-62`), `4ddc513e` the runners in
`runners/` (`D-210`, `Q-63`); 177 tests; pushed 2026-11-17, head `4ddc513e`. Step 7 sets
`column_generation.threads = 1` when `processes` > 1 and starts the runners as `runners/<layer>_runner.py`.
Then the integer solve (2026-11-17, `F-67`, `D-211`–`D-213`): `094beacd`, and `7190eb83` the runners' shared steps in
`runner.main` (`F-67` (11)); pushed, head `7190eb83`; 206 tests. Step 7: a runner's exit code 1 is a finished run
whose integer solution failed the instance's check; reuse `runner.main`'s steps, do not copy them.

Step 7 done 2026-11-17 (`F-68`, `D-214`–`D-218`, `9efea8da` on `7190eb83`, not pushed; 240 tests): `benchmark.py`,
`load.py`, a fake runner. Step 8 reads `runs/*/result.json` (a `timeout` one has null fields) and each run's
`metadata.json` `outcome` and `load`. Then (2026-11-18, `F-69`, `D-219`): `51b8223e`, `76bac866`; the benchmark's
`metadata.json` has `events` instead of `start` / `end`.

Step 8 done 2026-11-18 (`F-70`, `D-220`, `e72c99cd` on `b1d434aa`, pushed 2026-11-18; 261 tests). Step 8b (`D-221`)
reuses its reader (`read_folder`, `Cell`, `Round`) for A and B, and `agreed` / `measure` for the fields.

Step 8b done 2026-11-18 (`F-71`, `D-225`, `19160d73` on `e72c99cd`, pushed 2026-11-18; 285 tests): `compare.py`;
hand-written folders for tests in `tests/benchmark_folders.py`. Step 9 adds `example.toml` and the
CHANGELOG entry only.

Step 9 done 2026-11-18 (`F-72`, `D-226`, `ac832b35` on `19160d73`, pushed 2026-11-18; 286 tests): `example.toml`
with `RC201_12` (no `RC201_25` in `instances/`), the CHANGELOG entry. Every coding step done. PR #97 opened by the
user 2026-11-18 (draft, base `feature/column-generation-pure-python`, head `ac832b35`).

Added to steps 5–9: runners declare their flags with argparse, add the run options with `add_run_options`, read
them with `run_from_arguments`, support `--check build` (5, 6); `highs` on `cpp-bindings` (6); `benchmark.py` adds
`add_benchmark_options`, checks `options` then `build` unless `--no-check`, prints the dropped matrix points, passes
the file's folder for `from` (7); the integration tests of `D-202` (5–7); `OPTIONS.md` and the README grow with each
step.

| # | commit | contents | tests |
|---|---|---|---|
| 1 | the folder and its docs | `README.md` (install, build `_core` with the example, run), `requirements.txt` (psutil, tomli-w), the source guard | the guard |
| 2 | options and TOML | blocks, named definitions, defaults file with the presets, `[defaults]`, `[matrix]` with dotted keys, `[[runs]]`, rounds; expansion to a list of runs; errors on unknown keys | expansion, overrides, errors |
| 3 | instances | the interface, Solomon VRPTW, graph and rows, `single-customer` start | toy instance: graph, rows, start columns |
| 4 | the run folder and record | `run.toml`, `command.txt`, `metadata.json`, `result.json` fields, record (§7) | round trip of `run.toml`; record fields present |
| 5 | the `package` runner | options to objects, run, result | smoke test on a tiny instance over highspy |
| 6 | the `cpp-bindings` runner | the same over the example | the drift test; skipped when the example is not built |
| 7 | the benchmark | processes, resume, `STOP`, `status.txt`, timeout, load sampler, refuse mixed | a fake runner: order, resume, timeout |
| 8 | the summary (`D-220`) | the reader, `summary.py`, `summary.md`, `summary.csv`, `runs.csv`; mixed lists, flags; written at every end of `benchmark.py` | on fixed results |
| 8b | the comparison (`D-221`, `D-223`) | `compare.py A B`: options and records that differ, fields side by side, the time rule as an option; output as §12 | on fixed results |
| 9 | the example benchmark file and CHANGELOG (`D-224`) | `example.toml` as §12, generic (not `D-138`'s matrix); the PR's `CHANGELOG.md` entry (`D-167` (1)) | the file expands |

To check when coding: what the sdist takes from `benchmarks/`; where the tests live so CI collects them; that a
runner's own imports stay inside the folder and the tree.

## 10. For the implementer

Push and PR only when asked. Facts to port, from `benchmark-results/colgen/drivers.py`:

- `master_model`: the depot is node 0, so master row 0 is free and rows 1..n are `= 1`; arcs out of customer `i`
  cover row `i`. highspy: `addRow(lower, upper, 0, [], [])`; gurobipy: `= 1` rows, the free row as `≥ -1e30`.
  `cpp-bindings` also has the C++ `HighsMasterSolver(row_lower=..., row_upper=...)`; the package has none.
- `FiniteVRP`: the example's graph has an infinite-cost depot-to-depot arc (the empty route); the layers refuse a
  column built from it, so the graph skips it.
- `add_single_customer_routes`: depot, customer, depot, cost twice the distance, as `rcspp._core.graph` `Column`,
  `Row`, `Solution`; then `master.add_columns(pricer, routes)` and `master.mark_initialized()`.
- Stages: `PricingStage(name, params)` with `rcspp.graph.AlgorithmParams` (`max_solutions`, `max_iterations`); tabu
  needs a finite `max_iterations` or the package refuses it (`D-174`). Artificial start:
  `ArtificialColumns(cost=factor × max(1, graph_pricer.cost_scale()))`.
- API differences: the package's `ColumnGenerationParams` is frozen (pass `initialization=` to the constructor), the
  example's is set by attribute. Both results give `status`, `objective`, `iterations` and `statistics`
  (`generated_columns`, `relaxation_seconds`, `pricing_seconds`).
- Environment: `.venv` has an old `rcspp` in site-packages (the source guard's reason). Until the README exists, the
  build line and the `PYTHONPATH` order are in `benchmark-results/colgen/HANDOFF.md` (update of 2026-11-13); built on
  the laptop in `../rcspp-dev/build-python-highs`.

## 11. Comparisons and plots for the next tasks (2026-11-18, `D-221`, `D-222`)

| task | question | comparison / plot | needs |
|---|---|---|---|
| `T-1.18` | the port as fast as the old example? | `compare.py` with the time rule (`D-148`) | 8b |
| `T-1.16` | how much tighter the LP, at what cost? | per instance LP (or gap) and pricing time against `graph.ng_size`; bound against time | `runs.csv`; a series over one option |
| `T-1.21` | same LP, faster pricing? | LP gate against the forward search; pricing-time ratio per instance; half-way sweep; performance profile or shifted geometric mean | pairing with a reference; aggregation over instances; later label counts |
| `T-1.20` | extra time per iteration constant? bindings or Python? | time per iteration per layer; (A − B) / iterations against iterations; master / pricing / other bars; the time rule per cell; LP against time | 8b twice against `cpp-native`; per-iteration data (`D-206`) |
| `T-1.22` | which stages converge fastest? | LP against time; iterations, columns; performance profile | per-iteration data |
| any | noise small enough? | box plot of each cell's rounds; other-CPU load against time; spread against median | `runs.csv` with `load` |
| regression (`Q-66`) | a change slower? | ratio new / old per run, the rounds' spread as a band | 8b across two folders |

## 12. Step 8b's output and step 9's file (2026-11-18, `D-223`, `D-224`)

`A` and `B`: a round's folder, a run folder, or `<benchmark folder>:<run name>` (the run over its rounds). Numbers
from `results/manual-checks/five` in `../rcspp-dev` (ignored by git):

```text
$ python compare.py results/manual-checks/five:R201_50-package-highspy-exact-artificial-column_generation_c9520b \
    results/manual-checks/five:R201_50-cpp-bindings-highspy-exact-artificial-column_generation_c9520b \
    --time-within 5% --floor 0.25
A: five / R201_50-package-highspy-...       2 / 2 rounds finished
B: five / R201_50-cpp-bindings-highspy-...  2 / 2 rounds finished
Options that differ: layer: package -> cpp-bindings
Records that differ: none
Warning: 5 runs at a time, the timings shared the machine

| field             | A                       | B                       | A - B                          |
|-------------------|-------------------------|-------------------------|--------------------------------|
| status            | Optimal                 | Optimal                 | equal                          |
| objective         | 756.339                 | 756.339                 | equal                          |
| iterations        | 33                      | 33                      | equal                          |
| columns           | 17929                   | 17929                   | equal                          |
| integer_objective | 1219.73                 | 1219.73                 | equal                          |
| solve_seconds     | 89.561 [87.187..91.935] | 89.047 [87.623..90.471] | +0.514 (+0.6 %), within spread |
| master_seconds    | 3.158                   | 3.036                   | +0.122                         |
| pricing_seconds   | 86.373                  | 86.010                  | +0.363                         |
| other_seconds     | 0.030                   | 0.000                   | +0.030                         |
| per iteration     | 2.714                   | 2.698                   | +15.6 ms                       |

Verdict: equivalent (both Optimal, same LP; +0.514 s <= max(5 % of 89.047, 0.25) = 4.452 s)
```

Step 9's `example.toml` (44 rounds: 20 matrix runs, package x highs dropped, plus 2 standalone, 2 rounds):

```toml
[benchmark]
repeats = 2

[pricing.heuristics-first]
stages = [
  { algorithm = "tabu", max_solutions = 20, max_iterations = 1000 },
  { algorithm = "simple" },
]

[defaults]
start = { type = "artificial" }

[matrix]
instance = ["C101_25", "R201_25"]
layer = ["package", "cpp-bindings"]
master = ["highspy", "gurobipy", "highs"]
pricing = ["exact", "heuristics-first"]

[[runs]]
instance = "RC201_25"
layer = "package"
master = "highspy"
start = { type = "single-customer" }

[[runs]]
instance = "C101_25"
layer = "cpp-bindings"
master = { type = "gurobipy", parameters = { Method = 0 } }
column_generation = { timeout_s = 60 }
statistics = { solution = false }
```
