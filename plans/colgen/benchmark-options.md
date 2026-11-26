# `T-1.31`: benchmark options

The benchmark options `T-1.21` decided (`features-and-options.md` A.6, A.7, D.2, G.1, G.2, H.5, I.1), designed
2026-11-25 (`D-350`-`D-359`). Roadmap row: `roadmap.md` `T-1.31`. This chat's IDs: `D-350`+, `F-150`+, `I-50`+.

## What comes in

| item | run option | decisions | layers |
|---|---|---|---|
| route relaxation | `graph = { relaxation = "ng", ng_size = 8 }`; `none` (default), `ng`, `elementary` | `D-284`, `D-350`, `D-351` | all three |
| label buckets | stage keys `bucket_width`, `bucket_resource`, `sort_resource` (`cost`, `time`, `load`) | `D-282`, `D-352` | all three |
| preprocessing | `pricing = { stages = [...], preprocess = false }` | `D-285`, `D-353` | all three |
| tabu's iterations | a tabu stage without a finite `max_iterations` refused by the options check | `D-279` | all three (one check) |
| greedy, tabu forward only | a greedy or tabu stage with another `direction` refused by the options check | `D-295` | all three (one check) |
| proof test | the package's test of searches that prove nothing gains `Truncated`, `MaxSolutions`, `MemoryLimit` | `D-299` | the package |
| several matrices | `[[matrix]]`: an array of matrices, each expanded on its own | `D-357` | the benchmark file |

Not in: presets (`D-358`, `T-1.22`), main's docs on ng and elementary (`D-289`, its own docs PR), the VRP examples
(`D-284`), Boost (`T-1.35`).

## Setup

| what | where | state |
|---|---|---|
| worktree | `C:\dev\rcspp-options`, branch `feature/column-generation-benchmark-options` from #114 `71c13451` | created 2026-11-25 |
| pybind11 | `extern/pybind11` at `d6ae0e16`, objects referenced from `../rcspp-dev`'s | done |
| `.venv` | its own, Python 3.14.0, the packages of `C:\dev\rcspp\.venv` (a freeze); rcspp not installed | done |
| test build | `build-tests`: Visual Studio 18, Release, tests on, Python, the example with HiGHS and Gurobi; HiGHS's sources from `../rcspp-dev/build-python-highs/_deps/highs-src`; logs `build-tests/configure.log`, `build.log` | built 2026-11-25 (warnings only); tests use `RCSPP_BENCHMARK_BUILD=build-tests`, the script `--build build-tests` (relative to the worktree's root) |
| editor | in `C:\dev\rcspp.code-workspace` as "rcspp (options)"; `.clangd` and `.vscode/settings.json` copied from `rcspp` (excluded by `.git/info/exclude`), the folder's interpreter its `.venv` | done 2026-11-26 |
| clangd build | `build/clangd`: Ninja, clang++, Debug, configured inside `vcvars64` (as `F-23`), Python, tests, the example with HiGHS and Gurobi; log `build/clangd/configure.log` | done 2026-11-26 |

Plans stay in `C:\dev\rcspp` (`D-359`): the phase 2 chat edits them too; stage explicit paths only.

Working notes:

- The package's tests import `python/src`: copy `build-tests/python/rcspp/_core*.pyd` into `python/src/rcspp/` after
  each rebuild (ignored by git).
- A C++ commit's pre-commit needs `.venv/Scripts` (cpplint) and `C:\Program Files\Cppcheck` on `PATH`.
- clang-tidy on Windows (`clang-tidy -p build/clangd` inside `vcvars64`) reads templates as MSVC does: a missing
  `template` keyword on a dependent call passes it and the MSVC build, and fails only CI's Linux clang-tidy. Check
  a new template's dependent calls by eye; the local run still catches the warnings (complexity, magic numbers).

## Commits

As built, on #124's `26b67ed2` (PR #125). Each passes the benchmark's tests on its own; `OPTIONS.md` changes with
the option.

| # | commit | what |
|---|---|---|
| 1 | `a45db9c3` `feat(benchmark): several matrices in a benchmark file` | `[[matrix]]`, each expanded on its own; an empty array refused |
| 2 | `a333db43` `feat(benchmark): refuse the stages a search cannot run` | tabu without a finite `max_iterations`; greedy and tabu other than forward |
| 3 | `590f669f` `test(column-generation): searches stopped by a cap prove nothing` | `Truncated`, `MaxSolutions`, `MemoryLimit` beside `Timeout`, `Interrupted` |
| 4 | `824bbdf5` `feat(benchmark): pricing without preprocessing` | `pricing.preprocess`; native `--no-preprocess`; kept #124's convexity row (the rebase's one conflict) |
| 5 | `7de2a455` `refactor(benchmark): the native program over its graph's resources` | `Model`, `make_stage`, `set_param`, `run_on<Resources...>` |
| 6 | `08dde07f` `feat(benchmark): ng-routes and elementary routes` | `graph.relaxation`, `ng_size`; `ng_neighborhoods`; native `--relaxation`, `--ng-size`, `add_resources` |
| 7 | `c21bf71f` `feat(benchmark): label buckets in a pricing stage` | bucket keys to `BucketAlgorithmParams`; native `LabelBuckets`; heuristics refuse buckets |
| 8 | `bb0c02e0` `docs(benchmark): the changelog` | the benchmark's `CHANGELOG.md` entry |
| 9 | `6a8557be` `fix(benchmark): a customer's own ng slot, and the native stage checks` | Copilot's two comments |
| 10 | `566e4094` `fix(benchmark): the native program refuses what the options check refuses` | `F-156` |

Backups: `backup/benchmark-options-before-rebase` (`0b37aeeb`, on #114), `backup/benchmark-options-before-fold`
(`b4762f20`, the fix commit before it was folded into 6 and 7).

## Settled while coding

- Buckets on greedy and tabu: refused. They keep their own path (`greedy.hpp`) and never read the label container.
- `bucket_width`: a whole number of at least 1; a real width refused, not rounded. Default 100, resources `cost`.
- The native program: `--no-preprocess`, as its other switches. It picks its graph type in `run()` and calls
  `run_on<RealResource>` or `run_on<RealResource, SizeTBitsetResource>`.
- `ng_size` is a neighbourhood's size with the customer itself: itself first, then the `ng_size` - 1 nearest others
  (`6a8557be`: with customers sharing a spot it could lose its own place, and the engine adds it anyway).
- `graph` merges key by key like every block: each matrix point and `[[run]]` gives its own, so no special merge rule
  (the user, 2026-11-26).
- The native program refuses everything the options check refuses (`D-362`): it is run by hand to reproduce runs.

## Acceptance runs (`D-356`, as run: `D-360`)

`benchmark-results/colgen/t-1.31-acceptance/` (not committed): `acceptance.toml` (seven `[[matrix]]`, 276 runs),
`check.py` (the rules below, then the median solve times), `results/`, `benchmark.log`, `retry.log`. LP only, the
single-customer start, 4 processes, a 300 s limit (900 s for the three reruns).

| matrix | layers | searches | relaxations | instances |
|---|---|---|---|---|
| full | `package` | simple, pushing, pulling, astar; forward, backward, bidirectional; plain and buckets (width 20 on time); plain without preprocessing | none, ng 8 | C101_25, R201_25, RC201_12 |
| subset | `cpp-bindings`, `cpp-native` | simple forward: plain, buckets, preprocessing off | none, ng 8 | the same |
| small | the three | simple forward | none, ng 8, ng 10, elementary | C101_10 |
| small, every search | `package` | the 12 algorithm and direction pairs | elementary | C101_10 |

Passed 2026-11-26: every run `Optimal`; every LP equal (1e-6) to the package's forward simple search per instance and
relaxation; none ≤ ng 8; ng 10 = elementary on C101_10. To run it again: `python benchmark.py <file> --build
build-tests --output <folder>` from the worktree's `benchmarks/column_generation`, then `python check.py`.

## Record

- 2026-11-25: designed (`D-350`-`D-359`); worktree, `.venv` and test build set up.
- 2026-11-25: commit 1 coded, uncommitted, for the user's review: 425 tests pass, pre-commit clean, the `OPTIONS.md`
  example lists its 18 runs (`--check options`). A matrix whose points are all dropped is an error only when no run is
  left, as before. `instances/` has no `RC201_25` (the recorded runs used `RC201_12`): the acceptance's third
  instance is open → `RC201_12` (the user). Commit 1 `ec7dbbda`.
- 2026-11-25: commit 2 coded (428 tests pass). `F-150`: #86 (the heuristics stack, 6 of 7, on #81-#85) makes greedy,
  tabu and improving tabu search backward (`direction="backward"` from Python too); bidirectional stays refused.
  `D-295`'s backward refusal holds on the stack's engine (`main` without #81-#87); for `T-1.34` once #86 merges.
  Kept as is (the user). Commit 2 `ad097d71`.
- 2026-11-25: commit 3 `ed04d295` (the pricer's tests, 37 pass). The package's tests import `python/src`: the
  built `_core` `.pyd` is copied there by hand (`build-tests/python/rcspp/`, ignored by git), again after a rebuild.
- 2026-11-25: commit 4 `5a2fea81` (`pricing.preprocess`; native `--no-preprocess`; 434 tests). A C++ commit needs
  `.venv/Scripts` (cpplint) and `C:\Program Files\Cppcheck` on PATH for pre-commit. `F-151`: one full run failed
  `test_the_integer_solve_and_the_solution_can_be_left_out` (the native program exited 0 with no output, as the
  flaky test on #99); passed alone and in 9 runs of the native tests. `F-152`: greedy and tabu refuse
  bidirectional in the engine (`strategies.hpp`, `bidirectional = false`), and #86 keeps it so: "dive and join"
  was left out by agreement (it needs a retention keeping labels at the terminals and the boundary, a backward-first
  interleave, and a gate on a column generation; dives in front of an exact stage found a column on 4-15 % of
  iterations).
- 2026-11-25: commits 3 and 4 described to the user after the fact (commit 4 went in without a stop); reviewed, ok.
  Commit 5 `984a9204`: `Model`, `make_stage`, `set_param` templated on the resources, `run` picks the graph and calls
  `run_on<...>`; the LP reading and the integer solve take the master's parts. Native and runner tests: 103 pass.
- 2026-11-26: commit 6 coded, **uncommitted for the user's review** (the user asked): `graph.relaxation`, `ng_size`;
  `SolomonInstance.ng_neighborhoods`; the native `--relaxation`, `--ng-size` on `ResourceGraph<RealResource,
  SizeTBitsetResource>`; 451 tests pass, pre-commit clean. The three layers give equal LP and iterations: RC201_12
  none 427.2356 (16), ng 8 446.0135 (15); C101_10 ng 10 and elementary 58.3260. `F-153`: full memories are very slow
  in the engine: with the single-customer start, elementary on RC201_12 (20 customers, not 12), C101_25, R201_25
  does not finish in 60-120 s (the C++ VRP example's `--relaxation elementary` on C101_25 neither); on RC201_12 ng 16
  takes 6 s, ng 18 over 60 s; ng 8 under 0.5 s everywhere. With the artificial start even ng 25 on C101_25 ran over
  10 min. On C101_25 ng 8 gives the LP of none (191.8136). `D-356`'s elementary runs on the 25-customer instances
  cannot run: open, for the user.
- 2026-11-26: the worktree's editor setup: added to `rcspp.code-workspace`, `.clangd` and `.vscode/settings.json`
  copied (ignored by git; the folder's interpreter its `.venv`), `build/clangd` configured in `vcvars64` (Ninja,
  clang++, Debug, the example with HiGHS and Gurobi). Merging `graph` needs no change: each matrix point and
  `[[run]]` gives its own (the user). Reviewed; commit 6 `1003db47`. **Next:** commit 7.
- 2026-11-26: commit 7 coded, **uncommitted for the user's review**: any of `bucket_width` (a whole number, at least
  1, default 100), `bucket_resource`, `sort_resource` (`cost`, `time`, `load`, default `cost`) makes the stage a
  `BucketAlgorithmParams` one; native `LabelBuckets<RealResource, RealResource, ...>` over `AlgorithmBaseParams`,
  its own refusals. Settled: buckets on `greedy` and `tabu` refused (they keep their own path, never read the
  container; `greedy.hpp`), a real width refused. On C101_25 the three layers' LP equal the list's (plain and ng 8;
  buckets on cost, time, load). The native tests' `program_arguments` calls narrowed to `SolomonInstance` (pyright,
  from commits 4 and 6). 464 tests pass, pre-commit and pyright clean. **Next:** commit 8 (the changelog).
- 2026-11-26: commit 7 reviewed, `965ba718`. Commit 8 coded, **uncommitted for the user's review**: the
  benchmark's `CHANGELOG.md` entry (unreleased, so extended, not a new one) names the routes, buckets,
  `pricing.preprocess`, the stage refusals and several matrices. **Next:** the acceptance runs (`D-356`, elementary
  open per `F-153`), then the PR on #114.
- 2026-11-26: commit 8 `0b37aeeb`: the eight commits are in. Elementary in the acceptance only on a small instance
  (C101_10), ng 8 on the three planned ones (the user). The acceptance's plan proposed to the user.
- 2026-11-26: **acceptance passed** (`benchmark-results/colgen/t-1.31-acceptance/`: `acceptance.toml`, seven
  `[[matrix]]`, 276 runs; `check.py`). LP only, single-customer start, 4 processes. Every run `Optimal`, every LP
  equal (1e-6) to the package's forward simple search per instance and relaxation, on every layer, algorithm,
  direction, with buckets (width 20 on time) and without preprocessing. LP: C101_25 none = ng 8 191.8136; R201_25
  none 449.6598, ng 8 461.3023; RC201_12 none 427.2356, ng 8 446.0135; C101_10 58.3260 for none, ng 8, ng 10 and
  elementary (the 12 algorithm-direction pairs too). `F-154`: bidirectional with the default `half_way_point` (0)
  is slow where windows are wide: on R201_25 130-480 s against 0.5 s forward (the other instances fast); three
  bidirectional bucket runs passed 300 s and were rerun with 900 s (410-480 s), LP right. `F-155`: on C101_10
  every relaxation gives the same LP, so its ng 10 = elementary check is weak (the routes differ, not the LP);
  buckets on time halve the forward search on R201_25 (0.24 s against 0.60 s). **Next:** the PR on #114.
- 2026-11-26: #124 (`T-2.3`, the Lagrangian bound) is open on #114 too (#114 still at `71c13451`). A trial merge
  with it: 4 small conflicts, both sides adding to the same lines (preprocess vs the fleet's convexity row) in
  `package_runner.py`, `cpp_bindings_runner.py`, `cpp_native_solver.cpp` (its `RealResource` against the template)
  and `test_benchmark_runners.py`. Proposed: no rebase now, a sibling PR on #114; whichever lands second rebases.
- 2026-11-26: rebased onto #124 instead (the user: everything in one place). Backup branch
  `backup/benchmark-options-before-rebase` (`0b37aeeb`). Commit 4 kept both sides (preprocess and the fleet's
  convexity row, the two options in the reach test); commits 5-8 applied as they were, #124's convexity row inside
  `run_on<Resources...>`. Now `a45db9c3`..`695d5cb4` on `26b67ed2`. 479 benchmark tests pass, pre-commit and
  pyright clean. The PR's base: `feature/column-generation-lagrangian-bound`. **Next:** the PR, once the user
  approves its description.
- 2026-11-26: draft PR #125 on #124, assigned to the user. **Next:** its review and CI.
- 2026-11-26: CI's clang-tidy (Linux) refused `graph.add_resource<...>` in the templated `build_graph` (needs
  `template`; MSVC and Windows clang accept it): folded into commits 6 and 7 with `add_resources` and
  `default_bucket_width`, force-pushed (`bb0c02e0`; backup `backup/benchmark-options-before-fold`). CI does not
  start on #125 since it was added to a GitHub stack with #124 (mergeability `UNKNOWN`): the user's to sort out.
  Copilot's two comments fixed in `6a8557be` (a customer's own ng slot, no LP change: no instance has two
  customers at one spot; the native stage checks), replied on the threads, description updated. `F-156`: the
  native program called directly still accepts what the options check refuses (the runner checks in Python
  first): master options a run option owns (`threads`, `time_limit`: overridden silently), `--vehicles 0`
  (`RestrictedMasterInfeasible`), negative or NaN `--timeout`, negative `--threads` and `--integer-threads`,
  NaN `--cutoff`, `--integer-timeout 0`, negative `--integer-gap`, an unknown `--integer-column-type` (taken as
  binary), `--artificial-cost-factor` with the single-customer start and `--tolerance` without `--tolerances`
  (ignored); a negative gap passes `--check` and fails at the run (exit 1). Fixed on #125 (the user):
  `566e4094`, `check_options` and `check_master_options` at the end of `parse_options`, one test case each
  (500 tests pass); description updated.
