# `T-2.7`: debug checks

Written untracked in `C:\dev\rcspp-dev\plans\colgen\` while another chat rebased the stack, moved here
2026-11-27. IDs reserved for this chat: `D-380`+, `F-170`+, `I-70`+.

## Setup

| item | value |
|---|---|
| branch | `feature/column-generation-debug-checks` from #128's head `0baf6bcd` (`feature/column-generation-all-features`) |
| worktree | `C:\dev\rcspp-dev`; the other worktrees are read-only for this chat |
| rebase | done 2026-11-27 onto the rebased stack's #128 head `9617552a`, then `e5e94a84` (asked first, `D-373`; no conflict, nothing dropped; the second changed `test_benchmark_script.py` only, 49 passed; the server's all-features run on the first head `f0a81cc7` stays valid): `bf80f05f`, `5f82aba1`, `a99ad073`, `8cab04a1` (the hashes below are before it); the plan block moved onto `8cab04a1` |

## Already decided

`D-42` (runtime flag, off by default), `D-329` (three checks, own task), `D-308` (three layers), `D-372` (all-features
test), `D-16` (sparse format: the user's).

## Decisions

`D-380`-`D-391` in `decisions.md` (this task's, one per design item).

## Findings

`F-170` (`Solution.cost`'s comment), `F-171` (`F-112`'s cause) in `inventory.md`.

## Commits

| # | state | contents |
|---|---|---|
| 1 | committed `62d31697` (hooks passed) | `docs(core)`: `Solution.cost`'s comment (`F-170`) |
| 2 | committed `854da68f` (reviewed) | `python/src/rcspp/column_generation/debug_checks.py` (`DebugChecks`, `DebugCheckError`, four check functions, `KeptColumns` + `keep_new_columns`); the loop (`_check_solution` after the duals; the column check in `_price` before `add_columns`; bounds and user cost read once per run and after `on_converged` continues); `params.debug_checks`; artificial columns keep their entry, the tolerance scale reads real columns only; exports; docs section "Checking a master solver or a pricer"; CHANGELOG; tests: `test_column_generation_debug_checks.py` (20), the master test of artificial columns, every check on in the package's all-features test (both masters; the checks ran 9 LP solves, 18 pricer calls). Package 725 passed (+20); pre-commit and pyright clean |
| 3 | committed `0d980184` (reviewed) | the C++ example layer: `debug_checks.hpp/.cpp` (plain functions, `KeptColumns`), the loop (`check_solution`, the column check in `price`), artificial columns keep their entry, the tolerance scale reads real columns only; bindings (`DebugChecks`, a translator raising `cpp_column_generation.DebugCheckError(RuntimeError)` with `check`, `iteration`, `message`, through `gil_safe_call_once_and_store`), stubs, the example's `params.py` and exports; CHANGELOG (the README lists no features: nothing there). `D-390`'s open point: the C++ `GraphPricer`'s `Solution.cost` means the package's (`pricer.hpp:28`). C++ messages print doubles shortest (`-89`, Python `-89.0`). Tests: `test_column_generation_debug_checks.hpp` (17), `test_cpp_column_generation_debug_checks.py` (6), every check on real masters in the end-to-end test (3: HiGHS from arrays, highspy, gurobipy). ctest 904 (one `F-165` flake, passes alone), the example's Python 124 (+9), package 725 |
| 4 | committed `007a3ed5` (reviewed) | the benchmark: `column_generation.debug_checks` in `blocks.py` (a list, kept in `DEBUG_CHECKS`' order, None or `[]` leave the run's table and name; `debug_check_switches`), the package and bindings runners (`DebugChecks(**switches)`), the native runner and `cpp_native_solver.cpp` (`--debug-checks`, names refused in `check_options`, exit 1 on a failed check); `OPTIONS.md`; both all-features files run every check in every run (`[defaults]`, merged into each matrix's table) and their test checks that each run made them; tests: options (refused values, order, no name change), native refusal, a run per layer and start. Benchmark 536 passed (+15; the quick all-features file included); pre-commit and pyright clean; the large all-features file not run |

**Overhead (`D-391`, 2026-11-27, laptop):** the package at commit 2 against `0baf6bcd`'s package over the same `_core`,
highspy, `exact`, artificial start; interleaved rounds in fresh processes (scratch script over `package_runner.build`).
Medians, solve seconds:

| instance | rounds | base | off | every check on | off - base | `D-139` limit |
|---|---|---|---|---|---|---|
| C101_25 | 10 | 0.0361 | 0.0365 | 0.0485 (+34 %) | +0.0004 | 0.25 |
| R201_25 | 5 | 0.3998 | 0.4037 | 0.4769 (+19 %) | +0.0039 | 0.25 |

Status, LP value and iterations equal in every round (Optimal; 191.8136, 16; 449.6598, 16); every check passed.

## The large all-features file on the server

On the home server (`server-setup.md`; Gurobi WLS: no Gurobi on another machine meanwhile), once the branch is
pushed. In `tmux`: about an hour.

```bash
tmux new -s features                  # Ctrl-b d to leave; tmux attach -t features to come back
cd ~/rcspp && source .venv/bin/activate
git status --short                    # nothing tracked changed
git fetch origin feature/column-generation-debug-checks
git checkout -B feature/column-generation-debug-checks origin/feature/column-generation-debug-checks
git lfs pull && git submodule update --init --recursive
git log --oneline -1                  # the pushed head
cmake -B build-python                 # the stack added files: configure again
cmake --build build-python --parallel 2 2>&1 | tail -3
RCSPP_BENCHMARK_BUILD=build-python pytest -rs benchmarks/column_generation/tests/test_benchmark_all_features.py \
    --all-features-file benchmarks/column_generation/all_features.toml 2>&1 | tee all-features-debug-checks.log
```

Expected: every test passes, no skip (a skip names a missing Gurobi or example part). The log stays untracked in the
clone's root. A failure: the test's message, then the failing run's `log.txt` under pytest's temporary folder (the path
is in the message).

## Left

- The large all-features file on the server (above), after the push.
- The draft PR on #128's branch: description in `debug-checks-pr.md` (no plan IDs), read with the user.
- Follow-ups: `PathEvaluator` as a check of `GraphPricer`'s paths once #81 is merged (`D-381`); `F-171`'s fix.
