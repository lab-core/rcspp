# `T-1.29`: the all-features test

Decisions: `D-371` (the form), `D-372` (the details). This chat's IDs: `D-370`+, `F-160`+, `I-60`+.

## Where

- Code: worktree `C:\dev\rcspp-options`, branch `feature/column-generation-all-features` from #125's head
  `566e4094` (the statistics branch is a sibling on #125: whichever finishes second rebases onto the other, `D-370`).
- Plans: here, on `feature/column-generation`, the plan block rebased onto the code head (`D-370`).

## Files

| file | what |
|---|---|
| `benchmarks/column_generation/tests/all_features_quick.toml` | CI's file: one point per feature, HiGHS only, C101_10 and a few C101_25; about 40 runs, about 20 s |
| `benchmarks/column_generation/all_features.toml` | by hand: `T-1.31`'s acceptance matrices, every master, three instances; tens of minutes |
| `benchmarks/column_generation/tests/test_benchmark_all_features.py` | runs `benchmark.py` on the quick file (or `--all-features-file <file>`), then checks the results |
| `tests/python/test_column_generation_all_features.py` | the package alone: its own two-vehicle-type model (no example code), several features at once, both hooks |

Each TOML starts with a comment: a new feature adds its runs here and its rule to the test (the rule `D-372`, also
in the README's agent protocol). Without plan IDs (`D-55`).

## Runs (the large file; the quick file a subset on C101_10)

| area | runs |
|---|---|
| exact pricing | `T-1.31`'s matrices: simple, pushing, pulling, astar × forward, backward, bidirectional; with buckets; preprocessing off; none and ng 8 on C101_25, R201_25, RC201_12; C101_10 with ng 8, ng 10, elementary |
| masters | every master per layer (`package`: highspy, gurobipy; `cpp-bindings`: highspy, gurobipy, highs; `cpp-native`: highs, gurobi), forward simple |
| starts | `artificial` and `single-customer` per layer |
| heuristic stages | greedy (capped) then simple; tabu (`max_iterations`) then simple; greedy, tabu, simple |
| stage fields | on an exact stage: `max_solutions`, `timeout_s`, `max_phases`, `prune_by_incumbent`; bidirectional with `half_way_point`, `join_after_early_stop` |
| loop | `max_iterations` (`IterationLimit`), `timeout_s` (`TimeLimit`), relative tolerances, `threads = 2` |
| stops (#124) | with `instance.vehicles`: `absolute_gap`, `relative_gap`, `cutoff`, `integer_objective` |
| after the loop | the integer solve on; `solution.json` checked against the instance |
| together | one or two runs with every compatible option on (heuristic stages, bidirectional buckets on ng 8, prune, preprocessing off, a gap stop, the integer solve) |

## Checks (in the test, read from each run's `run.toml` and `result.json`)

- Every run ends; the expected status follows from its options: a loop limit gives its status, a stop gives its
  status or `Optimal`, anything else `Optimal`.
- Per instance and relaxation, every `Optimal` LP equals the package's forward simple search's (1e-6), and a fixed
  LP per instance and relaxation in the test (from `T-1.31`'s acceptance results).
- none ≤ ng 8 ≤ elementary; ng at the customer count = elementary.
- A stopped run: LP − lower bound within its gap; the lower bound ≤ the reference LP.
- The integer objective ≥ the LP; the solution check passed.
- Gurobi masters skipped with a reason without a licence.

## Commits (planned)

1. `test(benchmark): every feature through the benchmark`: both TOMLs, the test, the conftest option, a README line
   (`e65934b5`, `aeea750f` on `T-2.5`).
2. `test(column-generation): every feature of the package together` (`285e5d07`, `0baf6bcd` on `T-2.5`).

## As built (commit 1, uncommitted, 2026-11-26)

Departures from the plan above, and what the runs showed:

- Quick file: 103 runs (HiGHS on the three layers; `cpp-bindings` with both its masters), about 2 min on the laptop
  (30 s of the benchmark's build checks, 98 s of runs, 4 processes). Relaxations on C102_10 (none, ng 4, ng 10,
  elementary; `F-163`), the rest on C101_10, one point on C101_25. Stops with a fleet of 10: `absolute_gap = 10`,
  `relative_gap = 0.3`, `cutoff = 1`, `integer_objective`, `cutoff = 58` with `integer_objective`.
- Large file: 664 runs (`T-1.31`'s matrices, C++ layers with bidirectional buckets, C101_10 and C102_10 with ng 4,
  8, 10 and elementary, elementary through every algorithm, every master with both starts, heuristics, stage fields,
  half-way points per instance, loop limits over every master, stops on four instances, the integer solve over every
  master, everything at once over every master).
- The test (`test_benchmark_all_features.py`): one benchmark per module, then one test per rule: the status from the
  options; known LPs (`KNOWN_LP`, from `T-1.31`'s results and these runs); every proven LP equal to its problem's
  reference; a stop within its gap, the bound ≤ the LP; every limit and stop fires somewhere (but
  `IntegerObjectiveGap`, `F-163`); the relaxations' order and ng at the customer count = elementary; the integer
  solution `Optimal`, checked, ≥ the LP; a half-way point not logged off (`F-161`). A file with C++ layers skips
  without the example; a file with Gurobi skips without a licence. Option `--all-features-file` in the tests'
  `conftest.py` (`ci-full` runs that folder).
- Checked by mutation (a scratch script over a kept results folder): each of 11 doctored results fails its rule.
- Also in commit 1: the runner fix and its test (`F-160`), `OPTIONS.md`'s half-way paragraph (`F-161`), the README's
  file list and Tests section.
- No changelog entry yet (tests only, and the runner fix leaves with the rebase onto `T-2.5`, `F-160`).

## Record

- 2026-11-26: designed with the user (`D-370`-`D-372`); plan branch rebased onto #125 (`566e4094`).
- 2026-11-26: branch `feature/column-generation-all-features` from `566e4094` in `C:\dev\rcspp-options`; commit 1
  written, not committed (as built above); quick test 8 passed; the other benchmark tests 501 passed (500 + the warning
  test); pre-commit and pyright clean; the large run started.
- 2026-11-26: the large run (laptop, with Gurobi): 664 runs in 44 min after the build checks, none timed out or
  crashed (584 `Optimal`, 28 `Cutoff`, 16 `RelativeGapLimit`, 8 `AbsoluteGapLimit`, 14 `TimeLimit`, 14
  `IterationLimit`); 7 of 8 rules passed, the known-LP rule asked for two new problems (C101_10 ng 4, C102_10 ng 8),
  added to `KNOWN_LP` from their reference runs and checked over the kept results. Slowest run 682 s (R201_25,
  bidirectional `pulling` with buckets, no half-way point, `F-154`): the file's timeout raised from 900 to 1800 s.
- 2026-11-26: commit 1 reviewed by the user and committed, `e65934b5`. Commit 2, `285e5d07`
  (`tests/python/test_column_generation_all_features.py`): four customers, a large type (capacity 10, at most one) and
  a small dearer one (capacity 6, cost 1.5 times the distance); greedy, tabu, bidirectional with a half-way point on
  the load, pulling over buckets with the prune, ng-routes (full neighbourhoods), backward A* without preprocessing;
  artificial start, relative tolerances, both hooks (one restart), the integer solve; LP 44.25 and the best plan 48.5
  by enumeration (13 and 7 routes). No threads: HiGHS fixes them per process and warns. The half-way check fails when
  the clock is the cost (checked), but the engine logs its fallback once per process. Package tests 685 passed;
  pre-commit and pyright clean. Plan branch rebased onto `285e5d07`.
- 2026-11-26: `T-2.5` landed first (PR open, head `8d410193`; the plan branch already on it): our two commits rebased
  onto it (`D-370`), the runner conflict resolved with `T-2.5`'s version (`F-160`), commit 1 reworded: `aeea750f`
  (benchmark), `0baf6bcd` (package); backup tag `backup/all-features-before-statistics-rebase` (`285e5d07`). Plan
  branch moved from `8d410193` onto `0baf6bcd` (backup `backup/colgen-plan-before-all-features-rebase`; plan files
  identical, 136 plan commits). `build-tests` rebuilt; package 705, benchmark 521, quick all-features 8 passed
  (after `F-164`).
- 2026-11-26: pushed; draft PR #128 (base `feature/column-generation-statistics`, #127), assigned to the user.
- 2026-11-26: `D-373` (ask before rebasing onto another chat's work); `I-60` drafted (a comment on #105, the logger's
  reset after the line break). Both commits reviewed by the user: `T-1.29` done (`D-271`); left: CI on #128. Scratch
  scripts of this chat (mutations, large-run references, the output loops) were throwaway, in the session's scratchpad.
- 2026-11-27: CI on #128 (the first full run above #114): macOS missed the `absolute_gap = 10` stop (`F-167`). The stack
  rebased onto `main` with fixes (`D-374`); #128 is now `723c34c2`, `4ac48ca9` and `5b03395c` (the quick file's gap
  stops on C101_25 with a fleet of 25: `absolute_gap = 60`, `relative_gap = 0.3`; the quick test 8 passed locally).
- 2026-11-27: a second round of CI fixes (#97's end-sample test, #124's gap check); #128 is now `9617552a`.
