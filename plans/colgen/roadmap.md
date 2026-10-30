# Roadmap

Status per task: `todo` · `doing` · `done` · `blocked(Q-xx)`. One PR per task unless noted. Order inside a phase =
suggested order.

## Phase 0: baseline and prerequisites

| ID | task | status | done when |
|---|---|---|---|
| T-0.1 | Record **golden baselines** at the commit where implementation starts (`D-11`, `Q-09`): per instance, LP value, #iterations, #columns, LP time, pricing time, total. Instances: `C101_25`, `R101_25`, `RC201_12`, `R101_50`, `C101` (100), `R101` (100); then a few large ones. Save to `plans/colgen/baseline.md` with commit, build type, machine, relaxation (`F-05`). Earlier runs on `main` are informative only | todo, needs `Q-09` | file exists; the LP value is reproducible to 1e-6 across two runs |
| T-0.1b | Capture script: the current benchmark prints a colored log; strip the ANSI codes, keep the full log, and extract the final table into a markdown summary (PowerShell, `baseline.md` "How to run"). Later superseded by T-2.5 records | done | one command produces `full.log` + `summary.md` |
| T-0.2 | #52: fix the swapped `Solution.cost` / `column.cost` docstring (`F-01`) | todo | merged |
| T-0.3 | Agree on how this work relates to the open stacks (`Q-02`) | done (`D-43`) | D recorded |
| T-0.4 | #53, independent fix: `Row::coefficient` → `double` (C++, bindings, tests); reject non-finite coefficients at `add_arc`/`add_rows_to_arc`, or in #46's model checks (`F-12`) | todo, colleagues to confirm | LP values unchanged to 1e-9 on Linux and Windows; `sizeof(Row) == 16` |
| T-0.5 | #54, benchmark: BKS table matching the distance convention, or labeled "BKS (1-decimal distances)" with a warning instead of `FAIL` (`F-11`) | todo | C101 no longer reports a false `FAIL` |

LP value = regression target (the LP optimum is unique). #iterations and #columns depend on tie-breaking and the
LP solver: compare them, but they are not golden.

## Phase 1: CG core (LP only, single or multiple pricers; detailed design: `phase-1-design.md`)

| ID | task | status | done when |
|---|---|---|---|
| T-1.1 | Interfaces (`D-21`–`D-24`, `D-30`) + `MasterProblem` with fixed rows (`D-34`) and `MasterInitialization` (`ArtificialColumns`, `D-45`) + `ColumnGeneration` + statistics tier 0–1; a fake `SolverBackend` for tests. Loop in C++ (`D-37`) | done in code: PR 1, draft #61; the toy tests run on Gurobi and HiGHS (PRs 2–4) | unit tests on the theory toy (LP 37.5, duals `(11.5, 12.5, 13.5)`, `docs/theory/master-and-pricing.md`) |
| T-1.3 | `GurobiBackend` + `USE_GUROBI`; the **backend self-test** (dual signs, ranges, add columns, warm start: `F-15`) shared by every backend (`D-30`). First backend (`D-58`), C++ API (`D-61`, `D-62`) | done in code: PR 2, draft #69 (rename `D-69`, self-test `D-70`, the toy) | self-test + toy pass locally; skipped cleanly without a license |
| T-1.4 | `GraphPricer` with a fixed single stage; returns status + bound (`D-21`) | done in code: PR 3, draft #72 (stages, not only one: `D-71`); `C101_25` waits for T-1.5 | toy + `C101_25` give the T-0.1 LP value |
| T-1.2 | `HighsMasterSolver` + `USE_HIGHS` (HiGHS installed, else fetched, `D-82`; details `D-77`–`D-81`); runs the self-test and toy tests written for Gurobi (`D-58`); retries after a false cycling stop (`F-21`). Before #61 leaves draft (`D-59`) | done in code: PR 4, draft #76; the cycling retry has no test (`F-21`) | same tests + self-test pass on HiGHS |
| T-1.10 | **Maintained benchmark scripts** (user, 2026-10-27: later): a master solver benchmark, started from the `F-24` program (random set-covering masters, rounds of added columns, time per round by method), run on every master solver built in; then column generation benchmarks with the VRP port (T-1.5) and the benchmark project (phase 5). Own build target behind `USE_HIGHS`/`USE_GUROBI`, a fixed output format | todo, after PR 4 | reproduces `F-19` and `F-24` |
| T-1.11 | **Gurobi runs from Python kept in C++ (`D-86`): test and compare.** B1: copy a gurobipy model into `GurobiMasterSolver` in a source build with `USE_GUROBI` (check that the extension and gurobipy load the same `gurobi130` library). B2: a Gurobi master solver over the C API, loading the library gurobipy bundles (works from the wheel). Both run the self-test and the end-to-end tests; then time A, B1 and B2 per iteration on the `F-17` masters and the toy, and decide which ships | todo, after PR 6 (A is the baseline); not to be dropped; B2's design ideas (C API, adapters by version range): `packaging.md` §3, `Q-40` | both pass the tests; a table of A / B1 / B2 per iteration recorded in `inventory.md`; a decision recorded |
| T-1.5 | Port the C++ VRP example and benchmark to `rcspp::column_generation` (VRP code reduced to instance → graph + rows) | todo | T-0.1 LP values to 1e-6 relative on every instance; time ≤ baseline + 5 % (Gurobi) |
| T-1.6 | Python layer (`D-36`): bindings + trampolines (`D-37`), **user-model masters** `GurobipyMaster` then `HighspyMaster` (`D-35`), port the Python example to a user-model master | in progress: design in `phase-1-python.md` (accepted, `D-86`–`D-92`); PR 5 done in code (5.1 to 5.6), branch `feature/column-generation-python` stacked on PR 4; PR 6 done in code (6.1 to 6.7, `D-101`, `D-104`, `D-105`), branch `feature/column-generation-python-masters`; the VRP example port and `C101_25` are in PR 8 (`phase-1-design.md` §6, with T-1.5) | pytest on toy + `C101_25` pass |
| T-1.7 | User docs page (`docs/advanced/column-generation.md` rewrite) | todo | the Sphinx build passes |
| T-1.9 | Several vehicle types: a test or example with one graph and one convexity row per type, types differing by capacity, then possibly cost per distance, a stop limit or a battery resource. Needs nothing new in the library (`D-13`); to design after PR 3. First case (user, 2026-10-27: here, not in PR 3): the toy's customers with a large vehicle (capacity 11, at most 1) and a small one (capacity 6, dearer per distance), small enough to enumerate every route | todo, after PR 3 | each type's routes respect its own resources; LP checked against all routes enumerated |
| T-1.8 | Theory pages for column generation (big-M and artificial columns, convexity rows, proof of convergence…) next to `docs/theory/` (`D-64`) | todo, after `docs/theory` merges and the branches are rebased | the theory page checks pass |
| T-1.12 | **C++ packaging (`D-102`):** `USE_COLUMN_GENERATION` and the column generation target, the master solver options renamed and requiring it, C++ components with install rules (`packaging.md` §5); with `I-11`'s fix if `main` has not fixed it | todo, after 6.4, before #61 leaves draft | `find_package(rcspp COMPONENTS ...)` from an installed prefix builds a column generation program; the core builds with the flag off |

Deferred (`D-52`): CI for backends (`USE_HIGHS` in `ci.yml`, Gurobi vs the gate: `Q-05`), the benchmark workflow
(`I-07`), a Gurobi runner (`Q-07`). Phase 1 code follows `D-52`'s rules so CI can be added later without changes.

## Phase 2: performance features

| ID | task | status | done when |
|---|---|---|---|
| T-2.1 | `PricingStrategy`: stages (`D-23`, in PR 3) + budget escalation, ported from #49 `pricing_budget.hpp`; multi-pricer options (`D-27`) | todo: the budget and the options | no unproven stop counted as proof; ≥ #49's speed-ups on its instances |
| T-2.2 | `SolutionPool` in the loop (`F-04`): price stored columns first, `update_activity`; column management (`D-29`) | todo | measured effect recorded in the baseline file |
| T-2.3 | Lagrangian bound from the pricers' bounds + early stop on gap or cutoff (`D-21`, `D-28`) | todo | bound ≤ LP at every iteration (tier 3 check) |
| T-2.4 | `Smoothing` stabilization: automatic α, mispricing schedule, bound from its formula (`D-25`) | todo | same LP value; fewer iterations on the R2 family |
| T-2.5 | Statistics tiers 2–3, standard log columns, read-only hooks, benchmark record format (`D-31`) | todo | the benchmark runner writes records; tier off ⇒ no measurable overhead |
| T-2.6 | Parallel pricers (one graph clone per thread, `F-06`) | todo, optional | speed-up on multi-pricer instances |

## Phase 3: branch-and-price (designed, not implemented until decided: `D-20`)

| ID | task | status | done when |
|---|---|---|---|
| T-3.1 | Column bounds per node; `BranchAndPrice` tree with best-bound node selection | todo | toy: 44 proven (theory §5.4) |
| T-3.2 | `ArcBranching` (arc flows from `path_arc_ids` × `x_p`) via `remove_arcs`/`restore_arcs` + pool filters | todo | counterexample §5.2 → 40 proven |
| T-3.3 | Incumbent heuristic: RMP MIP (`solve_integer`) at the root and periodically | todo | — |
| T-3.4 | Robust cuts hook (rows added during CG) | todo, optional | vehicle-count cut example of §5.5 |

## Phase 4: SoPlex and SCIP (study done: prior-art.md; `D-32`)

| ID | task | status | done when |
|---|---|---|---|
| T-4.1 | `SoplexBackend` (SoPlex C++ API directly, not SCIP), `USE_SOPLEX` | todo | phase 1 tests + backend self-test pass |
| T-4.2 | SCIP pricer adapter: rcspp `Pricer` in a SCIP `ObjPricer` (reduced-cost + Farkas pricing, `*lowerbound`); root-node CG example | todo | same LP value as our loop on `C101_25` |
| T-4.3 | Arc-branching rule + marker constraint handler driving `remove_arcs`/`restore_arcs` (Binpacking pattern) | todo, one of the two B&P routes (`D-20`) | toy: 44 proven; counterexample §5.2: 40 proven |
| T-4.4 | PySCIPOpt `Pricer` example on the Python bindings | todo | same results as T-4.2 from Python |
| T-4.5 | Benchmark: our B&P (phase 3) vs SCIP-hosted B&P | todo, if both exist | comparison recorded |

## Phase 5: benchmark project

Inputs needed: `Q-07`. Config grid (backend × strategy × stabilizer × algorithm) × instance set → records (T-2.5).
