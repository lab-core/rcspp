# Roadmap

Status per task: `todo` · `doing` · `done` · `blocked(Q-xx)`. One PR per task unless noted. Order inside a phase =
suggested order.

## Phase 0: baseline and prerequisites (retired, `D-269`)

Done or superseded: `T-0.1` (baselines, now the benchmark's recorded runs), `T-0.1b` (capture script, now #97's
summary), `T-0.3` (`D-43`). Outside the plan, tracked on GitHub: #52 (`T-0.2`), #53 (`T-0.4`), #54 (`T-0.5`), #55
(route relaxation). LP value = regression target; #iterations and #columns depend on tie-breaking and the LP solver.

## Phase 1: CG core (LP only, single or multiple pricers; detailed design: `phase-1-design.md`)

| ID | task | status | done when |
|---|---|---|---|
| T-1.8 | **Theory and docs pages for the package (`D-273`, `D-64`):** the theory series' column generation and Python-layer pages updated for `rcspp.column_generation` (artificial start, convexity rows, stages, convergence), next to `docs/theory/` (#60 adds the column generation page); in `docs/advanced/column-generation.md`, one sentence: with only artificial columns, put a heuristic stage first (the first exact searches on big-M duals can be very slow, `F-41`, `F-103`) | waits for #60 or the column generation stack to merge (`D-276`); the one merged second adds the pages | the theory page checks and the Sphinx build pass |
| T-1.22 | **Benchmark presets with the colleagues (`D-192`):** which pricing setups (heuristic stages, caps, `timeout_s`), masters and starts the runner ships as presets, beside `exact`; agreed before any is added to `presets.toml` | waits for the colleagues (asked at the meeting of 2026-11-25; `T-1.21` done); `T-1.37`'s results feed it | the presets agreed and added, each with a comment saying when to use it |
| T-1.26 | **Pricing on one dual sequence (`Q-68`, `D-260`):** a replay runner over the per-iteration duals the benchmark records (`T-2.5`): one algorithm per run, time per solve, columns found; the check that exact algorithms agree per iteration; its comparison in `T-1.23`'s analysis tool | todo; **next** (the user, 2026-11-27); the per-iteration duals are recorded since `T-2.5` (its `duals` group, the benchmark's `iterations.jsonl`); the comparison as a command of `T-1.23`'s tool; with `T-1.35` if Boost is wanted as a cross-check | the engine benchmarks' comparison reproduced on C101_25 and R201_25 |
| T-1.30 | **A profiling pass on the package (`D-274`):** profile the pure layer (py-spy sampling and cProfile) over highspy and gurobipy, on cheap iterations (C1 and R1 at 50 customers) and heavy ones (R2), to find its weak points; the known suspects (`F-47`, `F-101`; issues `I-14`-`I-17`) confirmed or replaced; the C++ example as an optional second look | todo (`D-307`'s order: after `T-1.26`, `T-1.37`); `T-2.5`'s `times` group gives the split per iteration | a ranked list of weak points recorded, each turned into an issue or dropped |
| T-1.35 | **Boost's `r_c_shortest_paths` in the benchmark (`D-260`, `D-302`):** an algorithm name of the `cpp-native` runner only, refused by `package` and `cpp-bindings` and when the program is built without Boost; Boost optional in the build, the program's build record says whether it has it; `OPTIONS.md`, the tests | todo; `T-1.31` done; before or with `T-1.26`; adds itself to the all-features files (`D-372`) | on C101_25 and R201_25 the LP with Boost equal to the forward search's; refused where it does not apply |
| T-1.36 | **Regression results on GitHub Pages (`D-304`):** correctness in CI; results stored outside `main`; `pages.yml` renders them, and `T-1.23`'s stored analyses, with `T-1.23`'s tool; where timings are measured: `Q-66`, `Q-67` | todo (a parallel chat, `D-307`); `T-1.23` done; where timings are measured still open (`Q-66`, `Q-67`) | a page with each tracked instance's results over commits, and a CI check that fails on a changed LP |
| T-1.37 | **A tuner (`D-305`):** an existing configurator (irace, SMAC3 or Optuna) over the benchmark runner, per instance family, train and test instances; its results feed `T-1.22`'s presets and are stored as a `T-1.23` analysis | todo; **next** (the user, 2026-11-27); `T-1.31` and `T-2.5` done | a tuned configuration per family, faster than `exact` on the test instances |
| T-1.34 | **The table again after the colleague's merges (`D-278`, `D-290`; a reminder for the user, not a chat's task):** after the column generation stack's rebase onto a `main` with the kernel stack (#62-#75, merged 2026-11-23), then as #81-#87, #96 or the RouteOpt features land, their rows in `features-and-options.md` decided as `T-1.21`'s; known already: `ImprovingTabuSearch` and `DiversificationSearch` as Python, then benchmark, algorithm names (`D-280`); #74's removal of `algorithm="bidirectional"` followed by the package, the C++ example and the benchmark; #86's backward greedy and tabu: the benchmark's check then refuses only bidirectional (`F-150`) | reminder: the pass for #62-#75 done 2026-11-24 (`features-and-options.md` re-sourced on `b589e406` / `71c13451`, every path re-checked, rows A.2-A.11, B.1, B.5, B.8, C.3, I.1, L; `D-294`-`D-301` accepted); next for #81-#87, #96, #41-#50 | every merged PR's rows decided; the decided work added as tasks, each with LP values equal to the forward search's |

Done or closed (`D-270`; histories in `history.md`, `decisions.md` and the records):

- `T-1.1` interfaces, master, loop (C++ example, #61); `T-1.3` Gurobi master and self-test (#69); `T-1.4` graph pricer
  and stages (#72); `T-1.2` HiGHS master (#76; the cycling retry is tested in the package's highspy master)
- `T-1.6` bindings and user-model masters (#77, #78); `T-1.15` the pure-Python layer, the package (#88; `D-141`, `D-142`)
- `T-1.7` docs page (#88; open issues #94 API reference, #95 tabu default `max_iterations`, `F-60`)
- `T-1.13` big-M start, fix A + B (`D-118`; what is left: `T-1.28`); `T-1.14` heuristic stages, no default cap (`D-117`)
- `T-1.17` separation into `examples/` (fold, #61-#88; `D-144`-`D-166`); `T-1.24` the home server (`D-267`)
- `T-1.10` the benchmark (#97, `benchmark-runner.md`; `D-177`-`D-240`); `T-1.18` the Python VRP example on the package
  (#98; `F-90`); `T-1.19` the C++ VRP example on the C++ example layer and the `cpp-native` leg (#99; `F-91`, `F-92`)
- `T-1.9` several vehicle types (#114; `D-292`, `D-293`); `T-1.21` every feature and option (`features-and-options.md`,
  `D-278`-`D-290`; `T-1.34` its reminder)
- `T-1.31` benchmark options: ng and elementary routes, buckets, `preprocess`, several matrices (#125,
  `benchmark-options.md`, `D-350`-`D-362`); `T-2.5` per-iteration statistics (#127, `statistics-design.md`,
  `D-320`-`D-331`)
- `T-1.29` the all-features test (#128, `all-features.md`, `D-370`-`D-372`); `T-1.23` the analysis tool, `analysis.py`
  (#132, `analysis-tool.md`, `D-400`-`D-443`; its replay comparison waits for `T-1.26`)
- `T-1.20` many cheap iterations, three layers (`D-265`, `D-268`; `F-101`: `D-139` passes on all 38 cells; leftovers in
  `T-1.26` and `T-1.27`)
- closed: `T-1.32` and `T-1.33` (to issues #110 and #109, `D-290`), `T-1.16` and `T-1.25` (into `T-1.21`, `D-272`),
  `T-1.27` (to issue `I-17`, `D-273`), `T-1.28` (dropped, `D-273`), `T-1.5` (by `T-1.19`), `T-1.11` (moot, Python only),
  `T-1.12` (by the fold; #80 fixes #79)

Deferred (`D-52`): CI for backends (`USE_HIGHS` in `ci.yml`, Gurobi vs the gate: `Q-05`), the benchmark workflow
(`I-07`), a Gurobi runner (`Q-07`). Phase 1 code follows `D-52`'s rules so CI can be added later without changes.

## Phase 2: performance features (the package)

Re-scoped after the stack merges (`D-165`): written for the C++ layer; each task on the pure layer, or dropped. The
same for phases 3 and 4. The rule (`D-274`): what has several valid methods is a **component** passed in, as masters,
pricers and initializations are, with nothing provided by default when column generation works without it; a single
answer or a plain speed-up is built **as is**. Components: `T-2.1`'s budget policy and pricer schedule, `T-2.2`'s pool
and removal policy, `T-2.4`'s stabilization. As is: `T-2.3`'s bound and gap stop, `T-2.5`'s statistics (extension
through `on_iteration`); `T-2.6` folded into `T-2.1` (`D-306`). Each task's detailed shape: a design chat first
(`D-307`). Every task covers the package, the C++ example layer and the benchmark's options, complete on its own
(`D-308`); the frame and each task's design or open questions: `phase-2-design.md`.

| ID | task | status | done when |
|---|---|---|---|
| T-2.1 | `PricingStrategy`: stages (`D-23`, in PR 3) + budget escalation, ported from #49 `pricing_budget.hpp`; multi-pricer options (`D-27`); one generic, permissive schedule, in sequence or in parallel, with replaceable stop, merge and limit rules (`D-306`, folds `T-2.6` in) | todo; next on the main line (`D-307`), a design chat first: the budget and the options | no unproven stop counted as proof; ≥ #49's speed-ups on its instances |
| T-2.2 | `SolutionPool` in the loop (`F-04`): price stored columns first, `update_activity`; column management (`D-29`) | todo; next on the main line (`D-307`), a design chat first | measured effect recorded in the baseline file |
| T-2.4 | `Smoothing` stabilization: automatic α, mispricing schedule, bound from its formula (`D-25`) | todo; next on the main line (`D-307`), a design chat first | same LP value; fewer iterations on the R2 family |
| T-2.7 | Debug checks (`D-42`, `D-329`; moved out of `T-2.5`): column consistency (cost and rows from the path), dual signs against row senses, reduced costs recomputed over the master; a runtime flag, off by default; in the three places (`D-308`); the master-wide product raises `D-16` | designed 2026-11-27 (`debug-checks.md`, `D-380`-`D-391`): each new column's reported reduced cost recomputed (the rebuild from the path left for #81's `PathEvaluator`, `D-381`), dual signs from the row bounds (ranged rows skipped), every column's reduced cost and the used ones' near 0 (numpy, `D-385`), the LP value when the user's variables cost nothing; one switch each, `DebugCheckError`; the three layers; built on `feature/column-generation-debug-checks`, rebased onto #128 `9617552a`, then `e5e94a84`: `bf80f05f`, `5f82aba1`, `a99ad073`, `8cab04a1` (all reviewed); off: no measurable cost (C101_25 +0.0004 s, R201_25 +0.0039 s); on: +19 % to +34 %; every check in every run of both all-features files; pushed, draft PR #131 on #128 (`run-ci`); left: the large all-features file on the server (running), CI | each check fires on a broken master or pricer; off ⇒ no measurable overhead |

Done or closed: `T-2.3` Lagrangian bound and gap, cutoff stops (#124, `phase-2-design.md` §2.6, `D-309`-`D-317`);
`T-2.6` parallel pricers (folded into `T-2.1`, `D-306`).

## Directions, not planned: design constraints (`D-275`)

Branch-and-price, SoPlex and SCIP, and a separate benchmark project are possible directions, not plans. No task works
towards them; every design keeps them possible. Their old task IDs stay for reference (`D-20`, `D-32`, `prior-art.md`).

| direction | old tasks | what current designs keep possible |
|---|---|---|
| Branch-and-price, ours (`D-20`, `Q-20`) | `T-3.1` column bounds per node and the tree, `T-3.2` arc branching, `T-3.3` incumbent heuristic (the master as a MIP: the benchmark's integer solve does it at the root), `T-3.4` robust cuts | masters: column bounds and fixing, rows added during a run, warm restarts; pricers: arcs removed and restored between runs (`remove_arcs`/`restore_arcs`); the loop: re-run from existing columns; the pool (`T-2.2`): filters on branching decisions; `T-2.3`'s bound for pruning; branching rule, node selection and cut separators as components (`D-274`) |
| SCIP-hosted branch-and-price, PySCIPOpt now (`D-32`) | `T-4.2` pricer in a SCIP `ObjPricer`, `T-4.3` arc branching there, `T-4.4` PySCIPOpt pricer example, `T-4.5` ours against SCIP's | a pricer callable outside our loop: any dual vector in, columns and a bound out; a Farkas mode (pricing on an infeasible master's dual ray, zero costs) |
| SoPlex as a master | `T-4.1` | the `MasterSolver` contract and its self-test stay solver-neutral |
| A separate benchmark project (`Q-07` → `D-275`) | phase 5 | the benchmark lives in this repo (`T-1.10`); its records stay stable and documented; CI, regression runs and Compute Canada are the colleagues' questions `Q-65`-`Q-67` |
