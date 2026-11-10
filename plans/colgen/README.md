# Column generation layer: plan

Status (2026-11-09): **Python layer only (`D-116`).** A pure-Python column generation layer is built, then compared
with the C++ layer (`D-111`, `T-1.15`); whether the C++ layer stays is open (`Q-41`). Until then the C++ layer is
frozen as it is: stacked drafts #61, #69, #72, #76, the Python PRs 5–6 and packaging #80. Big-M start (`T-1.13`)
done: fix A + B (`D-118`) reaches `Optimal` up to R201 and ships with `T-1.15`; the big-M start stays 3.9 times
slower with exact pricing on R201, tabu first cuts it 5.3 times (`F-41`).
Heuristic pricing is done (`T-1.14`, `F-40`; `D-117`: no default cap, tabu optional).
**Next:** `T-1.15`, the pure-Python layer (`python-layer.md`): on top of the stack, the C++ layer's features written the
Python way (`D-132`, superseding `D-121`); design `D-119`–`D-126`, pool postponed. Steps A–F done: commits 1–6
(`4881e169`, `0e620a91`, `ad46b14d`, `2ed8bbd5`, `4c5e1269`, `6c899842`), reviews B, D, F and the Python-way review
folded in (`D-127`–`D-136`); copied to `feature/column-generation-pure-python` (pushed, on #80's head `8bb6f36c`;
the same six patches as here, new hashes `6f9e8d6a`…`510e9b65`).
Step G done 2026-11-06 (`python-layer-runs.md`): LP values, iterations and columns equal on C101_25, R201_25 and
RC201_12 over highspy and gurobipy (and C101_50, C201 over highspy); times within 0–30 ms of the C++-backed layer
on the small instances (C201: +4.7 %, noisy machine); probes of B.15, B.16, D.12 and the `ColumnBatch` copy with
proposals. Decided (`D-137`, `D-135` confirmed): B.15, B.16, D.12 unchanged; `ColumnBatch` and `RowBatch` copy
their inputs (folded into commit 1; hashes after it). Records: `python-layer-records.md`,
`python-layer-runs.md`. Step H done 2026-11-06 (`python-layer-comparison.md`): the pure layer adds 1–2.4 ms per
CG iteration, spread over `add_columns`, the master solver, the graph pricer and the loop (`F-47`); LP values equal
with tabu too; capped greedy does not finish on R2 instances (`F-48`). **Proposed, for the user:** the matrix
(`D-138`: 4 configurations × exact/tabu × single/big-M, three tiers, about 1.5 h plus 6 h overnight on the idle
desktop), `Q-41`'s rule (`D-139`: within max(5 %, 0.25 s) per cell; the floor is the user's call), D.12 unchanged
(`D-140`). Full matrix run 2026-11-07/08 on the desktop (`D-138` accepted for running;
`python-layer-comparison.md` §6–7, `F-49`): 1,204 runs, LP, iterations and columns equal everywhere, 108 / 108 cells
equivalent by `D-139` (also with a 0.1 s floor). **Proposed:** `D-141`, Python only. **Next:** the user decides
`D-139`–`D-141` (`D-141` with the colleagues; presentation 2026-11-09, slides done 2026-11-09 in
`benchmark-results/colgen/presentation/python-layer/`: 9 slides + 3 backup, no plan IDs, the same-API slide, a
chart of the runs over 100 s against their run-to-run spread), then the last PR (`python-layer.md` §2). PR
description for the six commits drafted 2026-11-09 (base #80; the user opens the PR and reviews it there; the review
comments go to a later chat). PR #88 review done 2026-11-09 (R.1–R.22 in `python-layer-records.md`: two HiGHS
Ctrl-C / interrupt bugs, tests, the user's comments, a readability pass), approved and folded into commits 1–6 on
both branches (hashes in the record); worktree `../rcspp-dev` on `feature/column-generation-pure-python`. Both
branches force-pushed 2026-11-09.
Colleagues' meeting 2026-11-09: Python only ships, the C++ layer and its bindings stay as an isolated example
(`D-142`, amends `D-141`); the VRP examples ported to the layers after a speed check (`D-143`, supersedes `D-122`).
Feedback, analysis, inventory of what moves and open questions: `separation.md` (`Q-50`–`Q-55`, `T-1.17`–`T-1.19`).
Design chat 2026-11-10: `D-139`–`D-143` accepted; `D-144` (the example's bindings compiled into `_core` behind an
off-by-default option, sources under `examples/`, inert hooks in core), `D-145` (wheel without HiGHS), `D-146`
(example on in CI, out of the coverage gates), `D-147` (stack merged at once, PRs kept with paths rewritten, pure
layer its own PR), `D-148` (VRP rule), `D-149` (what the layers lack, e.g. pools, stays), `D-150` (one folder
`examples/column_generation/`, top-level package `cpp_column_generation`, C++ names kept); `D-151` (option names
`USE_COLUMN_GENERATION_EXAMPLE{,_GUROBI,_HIGHS}`). **Next:** `T-1.17`: the `D-144` prototype (`separation.md` §6);
`T-1.18` can run beside it; `T-1.19` after the layout; then the last PRs.
Then `T-1.16`.
History: `decisions.md`; measurements and how to run them: `benchmark-results/colgen/HANDOFF.md`.
Baseline: `main` @ `129b15e0`. CI is out of the plan for now (`D-52`).

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
| `measurements.md` | the measurement scripts for the 2026-10-30 presentation: matrix, runner requirements, facts, environment | before writing or running them |

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
