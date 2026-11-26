# `T-2.5`: per-iteration statistics

The design chat of 2026-11-26: everything `T-2.5` needs, in one place. Decisions `D-320`-`D-330`; findings, when
coding, `F-110`-`F-149`; issues `I-21`-`I-49` (`T-1.31`, in parallel, uses `D-350`+, `F-150`+, `I-50`+). The frame
is `phase-2-design.md` §1.

## 1. Scope

| item | decision | source |
|---|---|---|
| Places | the package first, in full; then the C++ example layer (loop, bindings, stubs, `cpp_column_generation`) and the benchmark (three runners, the `cpp-native` program) in a quick pass, the package's code reshaped | `D-320`, `D-308` |
| Hooks | `IterationInfo` grows; `on_iteration` stays the only hook, read-only (`D-31`); no trace stored in the package: a user collects through the hook, the benchmark writes a file | `D-323` |
| Debug checks (level 3, `D-42`) | out: their own task `T-2.7` (column consistency, dual signs, reduced costs recomputed over the master) | `D-329` |
| Earlier inputs | the candidates of `T-1.10` and `T-1.21` (roadmap row); `D-206` (the benchmark's per-iteration collector, never built), `D-239` (written as it happens), `D-281`, `D-286` | |

## 2. The groups (`D-321`)

Each group has its own switch. The base fields are always there (in `IterationInfo` today; in the benchmark's file
whenever it is written).

| group | fields | cost | package switch |
|---|---|---|---|
| (base) | iteration, LP value, lower bound, new and total columns, elapsed seconds | already computed | none |
| `times` | `relaxation_seconds`, `pricing_seconds` of the iteration; the layer's own time is not stored (elapsed minus the two) | two clock reads per phase | follows `timers` (on by default) |
| `pricers` | per pricer: `status`, `new_columns`, `min_reduced_cost`, `seconds`; and its `search_details` (§4) | small | `pricers` |
| `search` | in `search_details`: extended labels and `bidirectional` per stage | small; labels need the engine change (§5) | `search` |
| `duals` | one per row | one copy | `duals` |
| `master_values` | the LP solution's nonzero values, as (column id, value) | one `primal()` call | `master_values` |
| `columns` | the new columns: cost, rows, path | large (up to ~140,000 per call, `F-48`) | `columns` |

- `StatisticsParams` gains one boolean per group, full-word names as above; all off by default except `timers`.
- A group's fields are `None` (Python) or empty (C++) when its switch is off.
- Finer switches (one field each) are not built now: a group is split when someone needs it.

## 3. Duals and master values (`D-322`, `D-326`)

- Both are captured **in the loop, after the LP solve and before the new columns go in**: the hook runs after the
  columns are added, and gurobipy cannot be trusted to give the old solution once its model changed (`D-206`).
- The duals cost nothing more than a copy: the loop already reads them for pricing.
- `master_values`: only nonzero values (at most one per row in a basic solution), artificial columns included.
  A column id is its position in the master; with `columns` on, its path and cost are written the iteration it is
  added.
- Sparse data across the Python boundary (`D-16`): two numpy arrays (column ids, values), as `primal(ids)` already
  takes; no `scipy.sparse`.

## 4. A pricer's details (`D-323`, `D-324`)

| piece | what |
|---|---|
| `IterationInfo` new fields | `relaxation_seconds`, `pricing_seconds`, `pricers`, `duals`, `master_values` (+ the `columns` group's field, named at commit 2) |
| `PricerIterationInfo` | one per pricer, in the loop's order: `status`, `new_columns`, `min_reduced_cost`, `seconds`, `search_details`; the loop measures all but the last from outside, so any pricer gets them |
| `PricingResult.search_details` | optional, empty by default; a pricer's own account of its search |
| `GraphPricer` fills it | the stage reached; per stage passed through: status, columns, seconds; with `search`: extended labels and `bidirectional` |

## 5. The engine's label count (`D-325`)

- `SolveResult` gains `extended_labels` in `cpp/rcspp/` (the counter exists:
  `Algorithm::get_number_of_extended_labels`), bound to Python; in this branch, its own commit, its CHANGELOG line.
- Bidirectional: the two directions summed (to check at commit 1 how the counter behaves there).

## 6. The progress log (`D-327`)

| place | how |
|---|---|
| Package | `ColumnGenerationParams.log_frequency: int = 0`: 0 off; N, a line every N iterations and the last one; through `logging.getLogger("rcspp.column_generation")` at INFO (the user's handlers and levels apply) |
| C++ example layer | the same option, printed to the output |
| Benchmark | `column_generation.log_frequency`, into the run's `log.txt` |
| Columns | iteration, elapsed, LP time, pricing time, new and total columns, LP value, bound, gap; `D-31`'s incumbent left out until the loop has one (branch-and-price) |

## 7. The benchmark (`D-328`)

- `statistics.iterations`: a list of group names (§2), `[]` by default (no file). The `statistics` block already
  holds one switch per statistic (`process`, `solution`).
- `iterations.jsonl` in the run's folder: one line per iteration, flushed as it is written (`D-239`); the base
  fields always, then the groups listed.
- The summary flags runs that recorded (their times do not compare with runs that did not, `D-206`).
- `package` and `cpp-bindings` through `on_iteration`; `cpp-native` its own writer in C++.
- `OPTIONS.md`; tests. `T-1.31` edits the same block and `OPTIONS.md`: a conflict when the branches meet.

## 8. Branch, commits, done when (`D-330`)

Branch `feature/column-generation-statistics` from #124's head `26b67ed2` (it needs the bound per iteration), in
`../rcspp-dev`; PR base #124's branch (`feature/column-generation-lagrangian-bound`).

| # | commit | contents |
|---|---|---|
| 1 | `feat(core): count extended labels in a solve's result` | `SolveResult.extended_labels`, the binding, `.pyi`, tests, CHANGELOG |
| 2 | `feat(column-generation): per-iteration statistics and a progress log` | the package: `StatisticsParams`, `IterationInfo`, `PricerIterationInfo`, `PricingResult.search_details`, `GraphPricer`, the loop, `log_frequency`; `docs/advanced/column-generation.md`; CHANGELOG; tests |
| 3 | `feat(examples): per-iteration statistics in the C++ example layer` | the same in `column_generation.{hpp,cpp}`, its graph pricer, bindings, stubs, `params.py`; the example's README; C++ and Python tests |
| 4 | `feat(benchmark): per-iteration records and a progress log` | `statistics.iterations`, `iterations.jsonl` in the three runners and `cpp-native`, `column_generation.log_frequency`, the summary's flag, `OPTIONS.md`, tests |

| check | how |
|---|---|
| Records | the benchmark writes `iterations.jsonl` with every group on, in the three layers, on C101_25; the base fields and `times` equal in count, LP values and bounds equal across layers (`D-237`'s rule) |
| No overhead when off | the package, every group off, against #124's head on the same machine, C101_25 and R201_25, `D-139`'s rule |
| Cost when on | one run with every group on, recorded as a finding (`F-110`+) |
| Tests | each group off gives `None`; each on gives its fields; `master_values` and `duals` are the LP's before the columns go in (gurobipy too); `log_frequency` lines and the last one; a custom `Pricer` gets the loop's measures and no details |

## 9. Left to the coding

- The `columns` group's field name in `IterationInfo` (commit 2), for the user (`D-12`).
- The bidirectional label count (commit 1).
- Per run, the roadmap row's solver versions and machine are already recorded (the benchmark's `record.py`, groups
  `versions` and `machine`); the machine's load is not, and is out unless the user asks.

## 10. As built

Branch `feature/column-generation-statistics` in `../rcspp-dev`, from `26b67ed2`; rebased onto #125's head `566e4094`
2026-11-26 (`D-331`), no conflict; draft PR #127, base #125's branch (`feature/column-generation-benchmark-options`).

| # | state | commit |
|---|---|---|
| 1 | committed 2026-11-26 (the user read it first) | `455736e8` (was `1a2ae9d8` before `D-331`) `feat(algorithm): count extended labels in a solve's result` (the repo's scope for the engine, not `core`) |
| 2 | committed 2026-11-26 (the user read it first) | `66548527` `feat(python): per-iteration statistics and a progress log` (T-2.3's scope for the package, not `column-generation`) |
| 3 | committed 2026-11-26 (the user read it first) | `fd996d43` `feat(examples): per-iteration statistics in the C++ example layer` |
| 4 | committed 2026-11-26 (the user tested it first) | `8d410193` `feat(benchmark): per-iteration records and a progress log` |

| item | as designed | as built |
|---|---|---|
| `extended_labels` | in `SolveResult` | set in `Algorithm::annotate`; the labelling searches count (bidirectional: both searches, not the join); greedy, tabu, the dives and diversification do not count, 0 (`F-110`) |
| `search` | its own switch | needs `pricers`: `StatisticsParams(search=True)` alone is a `ValueError` |
| `PricingResult.search_details` | optional | `tuple[PricingStageInfo, ...]`, `()` by default; `PricingRequest.search_details` asks for it (off: the pricer reads no clock) |
| `PricingStageInfo` | (unnamed) | `stage`, `status` (the engine's `AlgorithmStatus`), `new_columns`, `seconds`, `extended_labels`, `bidirectional`; the last two `None` without `search` |
| `PricerIterationInfo.min_reduced_cost` | best reduced cost | of the columns returned, convexity dual included; `None` without a column |
| times | per iteration | `relaxation_seconds`, `pricing_seconds` (adding the columns to the master included, as the run's total) |
| `master_values` | (ids, values) | `tuple[IdArray, FloatArray]`, exact nonzeros of `primal()` over every column, read before pricing |
| `columns` group's field | named at commit 2 | `IterationInfo.added_columns: tuple[MasterColumn, ...]` (**for the user**, `D-12`) |
| the log | a line every N | plus the last iteration and an end line (status, iterations, times, columns, LP value, bound); `-` for what is unknown; `log_frequency` refused when not an `int` (`TypeError`) or negative |
| docs | "Options and hooks" | a new "What each iteration did" in `docs/advanced/column-generation.md`; a "How much a solve searched" in `docs/advanced/algorithms.md` (commit 1) |

Commit 3 as built: the same fields in C++ (`std::optional` where Python has `None`; `master_values` a pair of vectors,
`added_columns` copies), `PricingStageInfo` and `PricerIterationInfo` bound, numpy arrays and tuples across the
boundary; `search` without `pricers` a `std::invalid_argument` (`ValueError` in Python); the log through the engine's
`LOG_INFO`, the same lines as the package; a local `status_name` for the end line (the layer had none). Tests: C++ 879
(+9, `test_column_generation_statistics.hpp`), the example's Python 115 (+5, the log read with `capfd`), package 703,
benchmark 500 (`RCSPP_BENCHMARK_BUILD=build`: `../rcspp-dev` has two builds). Pyright's two `reportDeprecated` errors in
the example's `gurobipy_master_solver.py` were there before. Unexplained: right after the rebase's rebuild, one package
run had 2 failures, then 703 passed six times; runs now use `-rf` to name them if they come back.

Commit 4 as built: `statistics.iterations` (a list, kept in the groups' order, so the run name does not depend on how
it is written; `search` without `pricers` refused) and `column_generation.log_frequency` in the options;
`column_generation_benchmark/iterations.py` turns an `IterationInfo` into a line (the package's and the bindings' give
the same objects) and `IterationFile` holds the file, opened once the folder exists (the loop is built first, before
the folder); `iterations.jsonl` among the run files; `cpp-native` writes the same keys in C++ (`--iterations`,
`--iterations-file`, `--log-frequency`) and its runner prints the program's log lines into the run's output, without
the engine logger's color codes (`F-111`); the summary flags cells that record. Checked by hand on C101_25 with 25
vehicles and every group: the three layers' lines equal but their times; the test does the same. Tests: benchmark
520 (+20), package 703, ctest 879, the example's Python 115; pre-commit (cpplint wants clang-format run by hand on
`cpp_native_solver.cpp`: the hook passed with lines over 100) and Pyright clean. `F-112`: the intermittent package
failures named.

Done when, checked 2026-11-26 (`F-113`): the package off (the default) against #125's head `566e4094`'s package over
the same `_core`, and with every group on, highspy, one exact stage, big-M start; interleaved rounds in fresh processes
(a scratch script, not the runner: the runner's source-tree guard refuses another tree's build). Medians, seconds:

| instance | rounds | base | head, off | head, every group | off - base | `D-139` limit |
|---|---|---|---|---|---|---|
| C101_25 | 10 | 0.107 | 0.098 | 0.114 | -0.008 | 0.25 |
| R201_25 | 5 | 1.013 | 0.923 | 0.947 | -0.091 | 0.25 |

Iterations and LP values equal in every round. Indicative: the plan branch's rebase ran during part of it.

Tests (commit 2's tree, MSVC Release, `build/`): package 703 (+20: `test_column_generation_statistics.py`, one in
`test_column_generation_graph_pricer.py`), ctest 870 run of 878 (+2), the example's Python 110 (2 skipped); pre-commit
with `.venv/Scripts` and Cppcheck on `PATH`, Pyright clean. `F-110`: an exact search where no path beats the threshold
can count 0, the preprocessing having removed every arc.
