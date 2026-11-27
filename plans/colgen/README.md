# Column generation layer: plan

## Status (2026-11-27)

How we got here, dated: `history.md` (search it, do not read it whole); decisions: `decisions.md`; measurements and
how to run them: `benchmark-results/colgen/HANDOFF.md`.

- **What ships (`D-141`, `D-142`):** the pure-Python package `rcspp.column_generation`; the C++ layer and its bindings
  stay as an example in `examples/column_generation/` (`D-144`, `D-150`); the benchmark (`T-1.10`) runs three layers,
  `package`, `cpp-bindings`, `cpp-native`.
- **The code stack** (one PR per branch, each on the previous one, on `main` `f57a7658`, `D-374`): #61, #69, #72, #76,
  #77, #78, #80, #88 (the package), #97 (benchmark), #98, #99 (VRP examples), #114 (`T-1.9`), #124 (`T-2.3`), #125
  (`T-1.31`), #127 (`T-2.5`), #128 (`T-1.29`), #131 (`T-2.7`), #132 (`T-1.23`, the head `97ec7a7e`). Every check of
  #61-#128 green 2026-11-27; #131 and #132: CI pending. All drafts; review and merge order are the user's. `main` takes
  squash merges only: after each, the next PR is rebased with `--onto origin/main <parent's old head>`.
- **This branch:** the plan block on #132's head (`D-370`). #60 (theory docs) conflicts with `main`.
- **Worktrees:** `../rcspp-dev` on #131 (`feature/column-generation-debug-checks`; test build `build/`, Python build
  `build-python-highs`), `../rcspp-options` on #132 (`feature/column-generation-analysis`; its own `.venv` and build),
  `../rcspp-rebase` detached (for rebases). `git stash` is shared and holds an old unrelated entry: do not pop it.
- **Machines:** the home server is the timing reference (`D-267`, `server-setup.md`); the large all-features file runs
  there on `f0a81cc7` for `T-2.7` (result pending). The laptop's clones are in `C:\dev\` since 2026-11-24.
- **Next, main line (`D-307`):** phase 2: design chats for `T-2.1`, `T-2.2`, `T-2.4` (`phase-2-design.md` §3), then
  their code.
- **Next, parallel chats:** `T-1.26` and `T-1.37` first (the user, 2026-11-27), then `T-1.30`, `T-1.35`, `T-1.36`.
  Waiting: `T-1.8` (a merge), `T-1.22` (the colleagues).
- **Each chat adds its own short paragraph below** (task, branch, head, next) and moves the details to its task's file
  when the task is done.

### Chats in progress

None recorded yet.

## New chat

One task per chat. Paste this, then one sentence for the task (its roadmap row is the full brief):

```text
Column generation work in rcspp, branch feature/column-generation. Read plans/colgen/README.md first and
follow its agent protocol; open other plan files only when the task needs them. Before ending, update the
README status and the task's roadmap row.
Task:
```

## Agent protocol

- **Hard rule (`D-55`):** nothing that stays (code, comments, tests, commit messages, PR and issue texts, `docs/`)
  mentions `plans/colgen`, its files, its IDs or plan-only words ("phase 1", "tier 2"). Write the reason instead.
- Read this file first. Open the others only when the task needs them (see Index).
- Before a chat ends: record decisions (`D-`) and findings (`F-`), update the status above and the task's roadmap row,
  so the next chat starts from them alone.
- Refer to items by ID (`D-`, `Q-`, `P-`, `F-`, `T-`). Do not restate another file's content: link it.
- Code facts belong in `inventory.md`, with `path:line`. Code moves, so re-check a fact before relying on it.
- Q answered → add a `D-` in `decisions.md`, then mark the Q `→ D-xx`. Accepted D are never edited:
  supersede them with a new D.
- Style: tables and bullets, no intros, at most ~200 lines per file, 120 columns (markdownlint `MD013`).
- **Reminders to raise with the user** (don't decide alone):
  - any piece that moves sparse data (masters, pools, arc flows, Python boundary): numpy vs `scipy.sparse` vs
    C++ CSR (`D-16`);
  - any new name: full words (`D-12`).
- These files are not shipped docs. User-facing docs go in `docs/` once the code exists.
- **This branch's layout (`D-370`):** `feature/column-generation` = the code stack's head, then every plan commit on
  top (`plans/`, `benchmark-results/`, the markdownlint excludes in `.pre-commit-config.yaml`); never interleaved,
  never mixed in one commit. Code is written and committed on its PR branch (own worktree). Commit the plan docs as
  you go. Whenever the PR branch moves (new commit, fixup, rebase), move the plan block onto its head:
  `git rebase --autostash --onto <new code head> <old code head> feature/column-generation`, after checking that
  `git log --oneline <old code head>..feature/column-generation -- . ':!plans' ':!benchmark-results'
  ':!.pre-commit-config.yaml'` prints nothing. Sibling code branches (parallel chats on the same base): the one
  finishing second rebases onto the other, so the plan branch sits on one stack. The user force-pushes. Rebasing onto
  another chat's branch or head: propose it to the user first, with what it keeps and drops (`D-373`).
- **A new feature adds itself to the all-features test (`D-372`):** its runs in the benchmark's two all-features
  files and its check in their test; a package-only feature in the package's all-features test.

## Goal

A **generic** column generation (CG) layer (branch-and-price designed but not implemented yet, `D-20`), built on
the `rcspp` pricing engine. Usable from Python; the C++ layer is an example to copy (`D-142`). Built from
switchable components (LP solver, pricing strategy, stabilization…).
Fast by default. Diagnostics are cheap or can be switched off. It must support a benchmark project that compares
configurations.

## Scope

| in | out (for now) |
|---|---|
| Generic master: rows with `=`/`≥`/`≤`, columns = `rcspp::Column` | Anything VRP-specific in the library (VRP stays in examples and tests) |
| LP backends: HiGHS, Gurobi; SCIP later (`D-05`, `D-06`) | Non-robust cuts (they need new resources) |
| CG loop, pricing strategies, stabilization, column pool | Automatic decomposition (GCG-style) |
| B&P with arc branching (phase 3: design only, `D-20`; SCIP study `Q-22`) | |

## Index

Colleague-facing summaries (2026-10-21): French <https://claude.ai/artifact/CZfDgEKG37NtHWKCcpj2Ab>, English
<https://claude.ai/artifact/BZLej9R9NdVA4dcYt6joCf>. Keep both in sync with `decisions.md` (not yet updated from
`D-58` on). Colleague pages of 2026-10-26: SCIP pros and cons, SoPlex vs HiGHS (links in `prior-art.md`).

| file | contains | read when |
|---|---|---|
| `history.md` | the README's dated status, 2026-11-11 to 2026-11-27, word for word | tracing when or why something happened |
| `inventory.md` | existing pieces `P-`, facts and pitfalls `F-` | before designing or coding anything |
| `architecture.md` | components, interfaces, how they plug into the existing code, perf and stats design (early proposal: names and details older than the code) | design and implementation |
| `api-sketch.md` | what user code would look like (C++, Python, configs, B&P, custom backend; early proposal, older than the code) | discussing or reviewing the API |
| `phase-1-design.md` | detailed design of phase 1 in C++: signatures, layout, tests, PR breakdown and commits, proposals `DP-` | before implementing phase 1 |
| `phase-1-python.md` | the Python layer (PRs 5–6): bindings, graph pricers, master solvers over the user's model, packaging, tests, commits | before PR 5 |
| `roadmap.md` | phases, tasks `T-`, acceptance criteria | picking up work |
| `decisions.md` | decisions `D-` | before proposing something that may already be decided |
| `questions.md` | open questions `Q-` | before deciding; when blocked |
| `prior-art.md` | SCIP, GCG, Coluna, RouteOpt, HiGHS, python-mip…: comparison + proposals `PA-` | before designing a component |
| `issues.md` + `issues/` | GitHub issues to create (`I-`, one paste-ready file each) and to close | when a finding is outside the plan's code |
| `baseline.md` | measured benchmark results (informative and golden) | before and after perf-sensitive changes |
| `packaging.md` | study notes: Gurobi from Python (B1, B2, C API, adapters), Python packages, C++ packaging and flags | before `T-1.11`, `T-1.12`, or any packaging change |
| `python-layer.md` | the pure-Python layer (`T-1.15`): contents, fix A + B, pool option, commits, comparison with the C++ layer | before coding `T-1.15` |
| `python-layer-records.md` | the pure-Python layer's step records and review findings (B., D., F.) | before fixing or reviewing that layer |
| `python-layer-runs.md` | the pure-Python layer's runs against the C++-backed one (step G): LP values, times, probes of B.15, B.16, D.12, the `ColumnBatch` copy | before deciding those items, planning the full comparison |
| `python-layer-comparison.md` | step H: where the pure layer's extra time goes, the full comparison matrix, `Q-41`'s rule, D.12 after heuristic runs | before running the full comparison or deciding `Q-41` |
| `separation.md` | the colleagues' feedback of 2026-11-09, C++ layer as an example: what moves, open questions, PR shapes, VRP comparisons | before `T-1.17`–`T-1.19` |
| `fold-map.md` (+ `-commits`, `-targets`, `tools/fold_filter.py`) | the stack's rewrite into `examples/column_generation/`: method, filter, every commit, where each item goes, PR heads, final state | before rewriting the stack |
| `fold-record.md` | the rewrite as done: heads, checks, departures, the Linux run, step 6 | before the push or a stack review |
| `fold-review.md` | the read of the eight folded PRs: heads, findings `V.`, coverage, questions | before the fixups or the merge |
| `benchmark-runner.md` | `T-1.10`'s runner: shape, TOML input, output folders, records, statistics, growth per task, coding steps | before coding the runner or a follow-up's option |
| `benchmark-options.md` | `T-1.31`: what comes in, the worktree and build, the commit list, the acceptance runs and their record | working on `T-1.31` |
| `benchmark-review.md` | the read of PR #97: checks run, blockers, should-fix, test gaps, docs, proposal | before fixing #97 |
| `benchmark-solutions.md` | the runner's solutions: `--overwrite`, `solution.json`, the integer solve, the instance's resolved solution, per-iteration data | before step 5's fixup, the integer solve or per-iteration data |
| `measurements.md` | the measurement scripts for the 2026-10-30 presentation: matrix, runner requirements, facts, environment | before writing or running them |
| `server-setup.md` | the home server (`T-1.24`): the machine, choices, every setup step from scratch, updating, log | setting up or updating a benchmark machine |
| `overnight-runs.md` | the night of 2026-11-20/21: `T-1.19` on the laptop, `T-1.18` on the server, commands, next TOMLs | before starting or reading those runs |
| `meeting-2026-11-25.md` | the roadmap pass's output: the colleagues' questions, what exists, what Python lacks, the all-features test | before the meeting, `T-1.21` or `T-1.29` |
| `features-and-options.md` | `T-1.21`'s table: every feature and option of the engine, the bindings, the package, the C++ example layer, the benchmark and the colleague's PRs, with `path:line`, and the decision on each | before adding an option, binding a feature, or `T-1.29` |
| `phase-2-design.md` | phase 2's frame (three layers, `D-308`), `T-2.3`'s full design (bound, stops, outputs, commits, tests), the other tasks' open questions | before coding or designing a phase 2 task |
| `statistics-design.md` | `T-2.5`'s whole design: groups and switches, duals and master values, a pricer's details, the label count, the log, the benchmark's records, commits, checks, as built | working on `T-2.5` |
| `all-features.md` | `T-1.29`: the all-features test's files, runs, checks and commits | working on `T-1.29` or adding a feature |
| `analysis-tool.md` | `T-1.23`: the analysis tool's design (topics 1-10), the commands as designed, the drawing, as built | working on the analysis tool or adding a command |
| `debug-checks.md` (+ `-pr`) | `T-2.7`: setup, commits as built, the overhead, the server run's commands, what is left; the PR description draft | working on `T-2.7` or a check |
| `post-rebase-runs.md` | the server's check after the rebase onto the kernel stack: commands, files, the comparison | before starting or reading those runs |
| `home-server-runs.md` | the home server's benchmarks and their analysis (`layers-50`), findings for the benchmark | before reading or running a benchmark there |

## Project conventions that apply here

- C++23, core in `cpp/rcspp/` (mostly headers; non-template code in `.cpp`, `D-56`), namespace `rcspp`, snake_case, clang-format/clang-tidy/cpplint/cppcheck.
- Python: pybind11 bindings in `python/bindings/`, package in `python/src/rcspp/`, black/isort/flake8.
- CMake options are named `USE_*` (`USE_PYTHON`, `USE_VRP`, `USE_TESTS`, `USE_COVERAGE`).
- Coverage gates in CI: **100 % lines** for `cpp/rcspp/` and `python/bindings/` (`gcovr.cfg`,
  `scripts/check_cpp_coverage.py`), **90 %** for Python (`ci.yml`).
- Conventional commits (`feat(scope): …`, a pre-commit hook enforces them). Large features go in stacked PRs, each
  building and passing tests at its head.
- Tests: GoogleTest in `tests/cpp/`, pytest in `tests/python/`. Solomon VRPTW instances in `instances/` (git LFS).

## Glossary

| term | meaning |
|---|---|
| RMP | restricted master problem: the master LP over the columns generated so far |
| duals `π` | one per master row, from the LP solver. Pricing turns them into reduced costs |
| pricing | find columns with negative reduced cost (here: an RCSPP solve on a `ResourceGraph`) |
| exact proof | a pricing solve that finished (`AlgorithmStatus::COMPLETE`) and found nothing: the LP is optimal |
| stabilization | smooth the duals between iterations to avoid oscillation and tailing-off |
| Lagrangian bound | lower bound available before CG converges: `z_RMP + Σ_k U_k · min_rc_k` |
| B&P | branch-and-price: branch-and-bound where every node runs CG |
| backend, master solver | adapter to an external LP/MIP solver (HiGHS, Gurobi, SoPlex). The interface `SolverBackend` is renamed `MasterSolver` (`D-67`); older plan text keeps the old name |
