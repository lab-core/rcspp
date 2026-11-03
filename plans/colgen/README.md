# Column generation layer: plan

Status (2026-11-03): **Python layer only (`D-116`).** A pure-Python column generation layer is built, then compared
with the C++ layer (`D-111`, `T-1.15`); whether the C++ layer stays is open (`Q-41`). Until then the C++ layer is
frozen as it is: stacked drafts #61, #69, #72, #76, the Python PRs 5–6 and packaging #80. Big-M start (`T-1.13`)
done: fix A + B (`D-118`) reaches `Optimal` up to R201 and ships with `T-1.15`; the big-M start stays 3.9 times
slower with exact pricing on R201, tabu first cuts it 5.3 times (`F-41`).
Heuristic pricing is done (`T-1.14`, `F-40`; `D-117`: no default cap, tabu optional).
**Next:** `T-1.15`, design in discussion (`python-layer.md`): on top of the stack, mirroring the C++ layer
(`D-119`–`D-123`); design accepted (`D-119`–`D-126`, pool postponed). Step A done (commits 1–2 `b97e1c18`,
`5631af0b`; record in its §5); step B (review) done, its fixes folded into commits 1–2 (§5, `D-127`–`D-129`); step C
done (commits 3–4 `7e236413`, `06413bea`: gurobipy master solver, master problem, tolerances, initializations with
fix B, self-test; record in §5); next is D (Opus review of C).
Then `T-1.16`. History: `decisions.md`; measurements and how to run them: `benchmark-results/colgen/HANDOFF.md`.
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
