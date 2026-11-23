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
| T-1.9 | Several vehicle types: a test or example with one graph and one convexity row per type, types differing by capacity, then possibly cost per distance, a stop limit or a battery resource. Needs nothing new in the library (`D-13`); to design after PR 3. First case (user, 2026-10-27: here, not in PR 3): the toy's customers with a large vehicle (capacity 11, at most 1) and a small one (capacity 6, dearer per distance), small enough to enumerate every route | **next** (`D-276`); todo, ready now (`D-273`): the package's `MasterProblem` takes several pricers, nothing new needed | each type's routes respect its own resources; LP checked against all routes enumerated |
| T-1.8 | **Theory and docs pages for the package (`D-273`, `D-64`):** the theory series' column generation and Python-layer pages updated for `rcspp.column_generation` (artificial start, convexity rows, stages, convergence), next to `docs/theory/` (#60 adds the column generation page); in `docs/advanced/column-generation.md`, one sentence: with only artificial columns, put a heuristic stage first (the first exact searches on big-M duals can be very slow, `F-41`, `F-103`) | waits (`D-276`): for #60 or the column generation stack to merge; the one merged second adds the pages | the theory page checks and the Sphinx build pass |
| T-1.21 | **Every feature and option, where it exists, and what to do with each (`D-272`; replaces `T-1.16` and `T-1.25`):** list the engine's features and options (`main`'s CHANGELOG and docs, the colleague's open stacks #62-#75 and #81-#87, the engine benchmarks' features `D-260`: bucket containers, `DiversificationSearch`, Boost's `r_c_shortest_paths`), the C++ example layer's and the package's; for each, where it exists (core C++, core Python bindings, package, C++ example, benchmark runners) and what is missing from Python or exists only as an example. Then, item by item with the user (and the colleagues where needed): make it available in Python or not, a benchmark option or not, kept as an example only. ng-routes included (an ng size in the benchmark's empty `graph` block and the Python VRP example; `F-98`'s ng runs were by hand); ng-routes are one of its features, with no separate comparison against the removed `rcspp-ng-cg-comparison` (`D-277`) | done 2026-11-23: `features-and-options.md` (every row decided, `D-278`-`D-289`; open PRs wait for `T-1.34`); follow-ups `T-1.31`, `T-1.34` (a reminder) and issues #109, #110 (`D-290`), `T-1.29`'s list, `T-2.5`'s candidates | the table of every item with its places and the decision; the decided options added as follow-up tasks, each with LP values equal to the forward search's |
| T-1.22 | **Benchmark presets with the colleagues (`D-192`):** which pricing setups (heuristic stages, caps, `timeout_s`), masters and starts the runner ships as presets, beside `exact`; agreed before any is added to `presets.toml` | waits (`D-276`); todo, after the meeting of 2026-11-25 and `T-1.21` (a preset uses only options that exist; `D-273`) | the presets agreed and added, each with a comment saying when to use it |
| T-1.23 | **The analysis tool (`D-273`; replaces the plots task, closes `Q-70`):** one tool that reads run folders and never runs anything: `compare.py` moved into it, then the per-iteration comparison across runs (duals, objective, columns, reduced costs) and the replays' agreement once `T-1.26` records them, then plots (series over one option, box plots of rounds, performance profiles, convergence curves); its dependencies (matplotlib) outside the runners' requirements; `summary.md` stays in the benchmark. Grown by need, each piece when a measurement asks for it | waits (`D-276`); todo | `compare.py`'s tests pass from the tool; the comparisons and plots `T-1.21`'s and `T-1.26`'s measurements use |
| T-1.26 | **Pricing on one dual sequence (`Q-68`, `D-260`):** a replay runner over the duals `T-1.20`'s per-iteration data records: one algorithm per run, time per solve, columns found; the check that exact algorithms agree per iteration; with `Q-70`'s comparison module | waits (`D-276`); todo; first step: the per-iteration data (`D-239`: duals, objective, columns per iteration, written as they happen), which no runner records yet | the engine benchmarks' comparison reproduced on C101_25 and R201_25 |
| T-1.29 | **The all-features test (`D-273`):** one test, or a small benchmark file, running every available feature together through the whole loop and the benchmark (ng-routes included: so far checked only on the search itself, `F-98`); its list: the rows of `features-and-options.md` whose decision says "test" (`D-279`-`D-288`; hooks outside the benchmark), with `T-1.31`'s options; its first draft `meeting-2026-11-25.md` §3; CI or by hand decided at its design | waits (`D-276`); todo, after `T-1.31` (`T-1.21` done) | every feature of the list run end to end, LP values checked against a reference |
| T-1.30 | **A profiling pass on the package (`D-274`):** profile the pure layer (py-spy sampling and cProfile) over highspy and gurobipy, on cheap iterations (C1 and R1 at 50 customers) and heavy ones (R2), to find its weak points; the known suspects (`F-47`, `F-101`; issues `I-14`-`I-17`) confirmed or replaced; the C++ example as an optional second look | waits (`D-276`); todo | a ranked list of weak points recorded, each turned into an issue or dropped |
| T-1.31 | **Benchmark options from `T-1.21` (`D-279`, `D-282`, `D-284`, `D-285`):** the `graph` block takes an ng size and elementary routes; a stage takes label buckets (`bucket_width`, the bucket and sort resources by name: cost, time, load); the pricer's `preprocess`; the options check refuses a tabu stage without a finite `max_iterations`; the three runners, `OPTIONS.md`, the tests | **next** (`D-291`); todo; on #99's head (the benchmark's stack); before `T-1.29` | each option accepted by the three runners, refused where it does not apply; on C101_25, R201_25, RC201_25 the LP of every exact algorithm, with buckets and with preprocessing off, equal to the forward search's on the same graph, for ng 0, 8 and elementary; LP without ng ≤ ng 8 ≤ elementary |
| T-1.34 | **The table again after the colleague's merges (`D-278`, `D-290`; a reminder for the user, not a chat's task):** after the column generation stack's rebase onto a `main` with the kernel stack (#62-#75, merged 2026-11-23), then as #81-#87, #96 or the RouteOpt features land, their rows in `features-and-options.md` decided as `T-1.21`'s; known already: `ImprovingTabuSearch` and `DiversificationSearch` as Python, then benchmark, algorithm names (`D-280`); #74's removal of `algorithm="bidirectional"` followed by the package, the C++ example and the benchmark | reminder (the user); due after the stack's rebase onto the new `main` | every merged PR's rows decided; the decided work added as tasks, each with LP values equal to the forward search's |
| T-2.5 | **Per-iteration statistics (moved from phase 2, `D-275`; as is, `D-274`):** statistics tiers 2–3, standard log columns, read-only hooks, benchmark record format (`D-31`). Candidates (from `T-1.10`, `D-271`; each with its cost, switchable): time per iteration split into master, pricing and the layer's own work (`F-47`), per pricing stage the columns, the most negative reduced cost and the hand-overs, the engine's label and join counts, the LP value and Lagrangian bound per iteration, per run the solver versions, the machine and its load; from `T-1.21`: `SolveResult.bidirectional` (`D-281`) and the engine's extended-label count, bound when needed (`D-286`) | **next** (`D-276`); todo; before `T-1.26` (its per-iteration duals), `T-1.23`'s convergence curves and `T-1.30`'s time split | the benchmark runner writes records; tier off ⇒ no measurable overhead |

Done or closed (`D-270`; histories in the README, `decisions.md` and the records):

- `T-1.1` interfaces, master, loop (C++ example, #61); `T-1.3` Gurobi master and self-test (#69); `T-1.4` graph pricer
  and stages (#72); `T-1.2` HiGHS master (#76; the cycling retry is tested in the package's highspy master)
- `T-1.6` bindings and user-model masters (#77, #78); `T-1.15` the pure-Python layer, the package (#88; `D-141`, `D-142`)
- `T-1.7` docs page (#88; open issues #94 API reference, #95 tabu default `max_iterations`, `F-60`)
- `T-1.13` big-M start, fix A + B (`D-118`; what is left: `T-1.28`); `T-1.14` heuristic stages, no default cap (`D-117`)
- `T-1.17` separation into `examples/` (fold, #61-#88; `D-144`-`D-166`); `T-1.24` the home server (`D-267`)
- `T-1.10` the benchmark (#97, `benchmark-runner.md`; `D-177`-`D-240`); `T-1.18` the Python VRP example on the package
  (#98; `F-90`); `T-1.19` the C++ VRP example on the C++ example layer and the `cpp-native` leg (#99; `F-91`, `F-92`)
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
through `on_iteration`), `T-2.6` (an executor option). Each task's detailed shape: the user's later pass.

| ID | task | status | done when |
|---|---|---|---|
| T-2.1 | `PricingStrategy`: stages (`D-23`, in PR 3) + budget escalation, ported from #49 `pricing_budget.hpp`; multi-pricer options (`D-27`) | waits (`D-276`); todo: the budget and the options | no unproven stop counted as proof; ≥ #49's speed-ups on its instances |
| T-2.2 | `SolutionPool` in the loop (`F-04`): price stored columns first, `update_activity`; column management (`D-29`) | waits (`D-276`); todo | measured effect recorded in the baseline file |
| T-2.3 | Lagrangian bound from the pricers' bounds + early stop on gap or cutoff (`D-21`, `D-28`) | waits (`D-276`); todo | bound ≤ LP at every iteration (tier 3 check) |
| T-2.4 | `Smoothing` stabilization: automatic α, mispricing schedule, bound from its formula (`D-25`) | waits (`D-276`); todo | same LP value; fewer iterations on the R2 family |
| T-2.6 | Parallel pricers (one graph clone per thread, `F-06`) | waits (`D-276`); todo, optional | speed-up on multi-pricer instances |

## Directions, not planned: design constraints (`D-275`)

Branch-and-price, SoPlex and SCIP, and a separate benchmark project are possible directions, not plans. No task works
towards them; every design keeps them possible. Their old task IDs stay for reference (`D-20`, `D-32`, `prior-art.md`).

| direction | old tasks | what current designs keep possible |
|---|---|---|
| Branch-and-price, ours (`D-20`, `Q-20`) | `T-3.1` column bounds per node and the tree, `T-3.2` arc branching, `T-3.3` incumbent heuristic (the master as a MIP: the benchmark's integer solve does it at the root), `T-3.4` robust cuts | masters: column bounds and fixing, rows added during a run, warm restarts; pricers: arcs removed and restored between runs (`remove_arcs`/`restore_arcs`); the loop: re-run from existing columns; the pool (`T-2.2`): filters on branching decisions; `T-2.3`'s bound for pruning; branching rule, node selection and cut separators as components (`D-274`) |
| SCIP-hosted branch-and-price, PySCIPOpt now (`D-32`) | `T-4.2` pricer in a SCIP `ObjPricer`, `T-4.3` arc branching there, `T-4.4` PySCIPOpt pricer example, `T-4.5` ours against SCIP's | a pricer callable outside our loop: any dual vector in, columns and a bound out; a Farkas mode (pricing on an infeasible master's dual ray, zero costs) |
| SoPlex as a master | `T-4.1` | the `MasterSolver` contract and its self-test stay solver-neutral |
| A separate benchmark project (`Q-07` → `D-275`) | phase 5 | the benchmark lives in this repo (`T-1.10`); its records stay stable and documented; CI, regression runs and Compute Canada are the colleagues' questions `Q-65`-`Q-67` |
