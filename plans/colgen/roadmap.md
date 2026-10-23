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
| T-1.1 | Interfaces (`D-21`–`D-24`, `D-30`) + `MasterProblem` with fixed rows (`D-34`) and `MasterInitialization` (`ArtificialColumns`, `D-45`) + `ColumnGeneration` + statistics tier 0–1; a fake `SolverBackend` for tests. Loop in C++ (`D-37`) | doing: PR 1 written and reviewed (local commits 1.1–1.9, 2026-10-23); the toy tests need a real LP (PR 2) | unit tests on the theory toy (LP 37.5, duals `(11.5, 12.5, 13.5)`, `docs/theory/master-and-pricing.md`) |
| T-1.2 | `HighsBackend` + `USE_HIGHS` (FetchContent pinned); the **backend self-test** (dual signs, ranges, add/remove columns, warm start) shared by every backend (`D-30`) | todo | same tests + self-test pass on HiGHS |
| T-1.3 | `GurobiBackend` + `USE_GUROBI` | todo | same tests + self-test pass locally |
| T-1.4 | `GraphPricer` with a fixed single stage; returns status + bound (`D-21`) | todo | toy + `C101_25` give the T-0.1 LP value |
| T-1.5 | Port the C++ VRP example and benchmark to `rcspp::column_generation` (VRP code reduced to instance → graph + rows) | todo | T-0.1 LP values to 1e-6 relative on every instance; time ≤ baseline + 5 % (Gurobi) |
| T-1.6 | Python layer (`D-36`): bindings + trampolines (`D-37`), **user-model masters** `GurobipyMaster` then `HighspyMaster` (`D-35`), port the Python example to a user-model master | todo | pytest on toy + `C101_25` pass |
| T-1.7 | User docs page (`docs/advanced/column-generation.md` rewrite) | todo | the Sphinx build passes |

Deferred (`D-52`): CI for backends (`USE_HIGHS` in `ci.yml`, Gurobi vs the gate: `Q-05`), the benchmark workflow
(`I-07`), a Gurobi runner (`Q-07`). Phase 1 code follows `D-52`'s rules so CI can be added later without changes.

## Phase 2: performance features

| ID | task | status | done when |
|---|---|---|---|
| T-2.1 | `PricingStrategy`: stages (`D-23`) + budget escalation, ported from #49 `pricing_budget.hpp`; multi-pricer options (`D-27`) | todo | no unproven stop counted as proof; ≥ #49's speed-ups on its instances |
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
