# Column generation layer: plan

Status: **phase 1 design accepted (`D-43`–`D-53`), no code yet**. Updated: 2026-10-23. Baseline: `main` @
`129b15e0`. CI is out of the plan for now (`D-52`).

## Agent protocol

- **Hard rule (`D-55`):** nothing that stays (code, comments, tests, commit messages, PR and issue texts, `docs/`)
  mentions `plans/colgen`, its files, its IDs or plan-only words ("phase 1", "tier 2"). Write the reason instead.
- Read this file first. Open the others only when the task needs them (see Index).
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
the `rcspp` pricing engine. Usable from C++ and Python. Built from switchable components (LP solver, pricing strategy, stabilization…).
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
<https://claude.ai/artifact/BZLej9R9NdVA4dcYt6joCf>. Keep both in sync with `decisions.md`.

| file | contains | read when |
|---|---|---|
| `inventory.md` | existing pieces `P-`, facts and pitfalls `F-` | before designing or coding anything |
| `architecture.md` | components, interfaces, how they plug into the existing code, perf and stats design | design and implementation |
| `api-sketch.md` | what user code would look like (C++, Python, configs, B&P, custom backend) | discussing or reviewing the API |
| `phase-1-design.md` | detailed design of phase 1: signatures, Python user-model contract, layout, tests, PR breakdown, proposals `DP-` | before implementing phase 1 |
| `roadmap.md` | phases, tasks `T-`, acceptance criteria | picking up work |
| `decisions.md` | decisions `D-` | before proposing something that may already be decided |
| `questions.md` | open questions `Q-` | before deciding; when blocked |
| `prior-art.md` | SCIP, GCG, Coluna, RouteOpt, HiGHS, python-mip…: comparison + proposals `PA-` | before designing a component |
| `issues.md` + `issues/` | GitHub issues to create (`I-`, one paste-ready file each) and to close | when a finding is outside the plan's code |
| `baseline.md` | measured benchmark results (informative and golden) | before and after perf-sensitive changes |

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
| backend | adapter to an external LP/MIP solver (HiGHS, Gurobi, SCIP) |
