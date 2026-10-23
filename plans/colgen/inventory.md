# Inventory: what exists (verified 2026-10-20 on `main` @ `fdc41b26`)

Status legend: **LIB** in the library · **EX** example code only (VRP-specific) · **BR** on an unmerged branch or PR ·
**—** absent.

## Pieces (`P-`), mapped to the components in `architecture.md`

| ID | piece | status | where | gap |
|---|---|---|---|---|
| P-01 | Column type | LIB | `Row{index, long double coefficient}`, `Column{cost, rows}`: `cpp/rcspp/graph/row.hpp`; `Solution{cost, path_node_ids, path_arc_ids, column}`: `cpp/rcspp/algorithm/solution.hpp` | none: this is the master's input (`F-01`, `F-02`) |
| P-02 | Arc→row coefficients | LIB | `add_arc(…, rows)`, `add_rows_to_arc` (`graph/graph.hpp:307`), Python `add_rows` (numpy) | none |
| P-03 | Duals → reduced costs | LIB | `ResourceGraph::update_reduced_costs(duals, cost_index)`: `resource/resource_graph.hpp:439` | dense `vector<double>`; throws if a row index ≥ `duals.size()` |
| P-04 | Pricing solve | LIB | `ResourceGraph::solve(ub, params, preprocess, cost_index)` → `SolveResult{solutions, status}`: `algorithm/algorithm.hpp:35-75` | none for a single solve |
| P-05 | Exactness signal | LIB | `AlgorithmStatus`: only `COMPLETE` proves (`algorithm.hpp:35-48`) | BR #49: `proves_no_column()` also checks memory trims and truncated joins |
| P-06 | Pricing strategy (heuristic → exact) | EX, BR | the VRP benchmark chains tabu algorithms then exact (`examples/cpp/benchmark_main.cpp`, `VRP::solve`); BR #49 `examples/cpp/vrp/pricing_budget.hpp`: escalating timeout | no library interface |
| P-07 | Stop and timeout controls | LIB, BR | `AlgorithmParams::timeout_s`, `stop_after_X_solutions`, `should_stop` callback, memory limits; BR #49 `Algorithm::set_timeout()` | — |
| P-08 | Column pool | LIB | C++ `SolutionPool`/`FilteredSolutionPool` (`algorithm/solution_pool.hpp`): thread-safe `price(duals)`, `update_activity`, `remove_stale`, filters on compulsory/forbidden rows and arcs, `get_lp_data` (CSR export). Python `PricingPool` (`python/src/rcspp/pricing_pool.py`): numpy, shared memory | **examples don't use it** (`F-04`) |
| P-09 | Master problem | EX | C++ Gurobi `examples/cpp/vrp/cg/master_problem.cpp`; Python python-mip (Gurobi, else CBC) `examples/python/vrp/cg/master_problem.py` | VRP-only: `=1` per customer; coefficients recomputed from visited nodes instead of `Solution.column`; `std::map` everywhere |
| P-10 | LP backend abstraction | — | — | full gap. HiGHS is not used anywhere in the code (only in the theory docs, for the toy duals) |
| P-11 | CG loop | EX, BR | `examples/cpp/vrp/vrp.cpp:156-303`, `examples/python/vrp/vrp.py:178-224`; BR: `bidirectional_cg_main.cpp`, `ng_dynamic_cg_main.cpp` (~1 300 lines of driver code in the #41–#49 stack) | full gap in the library |
| P-12 | Dual stabilization | EX | `VRP::calculate_dual` (`vrp.cpp:550`): blends with precomputed optimal duals (`instances/duals/R101/`) | experiment only, needs known optimal duals; not Wentges |
| P-13 | Lagrangian bound, early stop | — | theory only (`docs/theory/master-and-pricing.md` §1.11) | full gap |
| P-14 | Statistics | LIB (partial), BR | `Timer` (`utils/timer.hpp`), `num_extended_labels`; BR #41: surviving labels and dominance checks on `SolveResult` | no CG-level stats |
| P-15 | Logging | LIB | `utils/logger.hpp`: runtime level, `LOG_*` macros, `LOG_*_ACTIVE()` guards, mutex per message | no compile-time strip (`F-07`) |
| P-16 | B&P primitives | LIB | `remove_arcs`/`restore_arcs`/`*_if` (`graph/graph.hpp:104-198`), `clone()` (`resource_graph.hpp:197`), pool filters by forbidden arcs | no tree, nodes, branching rules, arc flows, incumbent |
| P-17 | B&P design sketch | docs | `docs/theory/lp-to-integer.md` §5.4–5.6 (`process_node`) | spec only |
| P-18 | RMP MIP heuristic | EX | `MasterProblem::solve(integer=true)` flips variable types | no proof (theory §5.2) |
| P-19 | Robust cuts | LIB (primitive) | a new row index via `add_rows_to_arc` | no separation |
| P-20 | Benchmark harness | EX | `examples/cpp/benchmark_main.cpp` (C1/R1/RC1), `benchmark_large_main.cpp` (C2/R2/RC2), `benchmark_common.hpp` (BKS table), `.github/workflows/benchmark.yml` (manual, needs Gurobi on the runner) | only checks `LP ≤ BKS`; no LP reference values (`F-05`) |
| P-21 | SCIP | — | — | — |

## Facts and pitfalls (`F-`)

| ID | fact | impact |
|---|---|---|
| F-01 | `Solution.cost` = **reduced** cost (label cost); `Solution.column.cost` = **original** cost (sum of `arc->cost`) (`algorithm.hpp:494-520`). The docstring in `solution.hpp` says the opposite | fix the doc (`T-0.2`). The master must use `column.cost` |
| F-02 | `column.rows` is already aggregated per row index and sorted (`algorithm.hpp:494-514`) | the master can consume `Solution.column` as-is: no domain code needed |
| F-03 | Python `ResourceGraph` instantiates one C++ `ResourceGraph<R…>` lazily, chosen at runtime among pre-instantiated combinations (`python/bindings/graph_impl.hpp`, `graph_mix2/3.cpp`) | a bound CG driver must reach pricing through a **type-erased** interface (`architecture.md` `Pricer`) |
| F-04 | Neither VRP example uses `SolutionPool`/`PricingPool` | the pool's CG integration is untested in a real loop |
| F-05 | VRP pricing on `main` is **non-elementary**: the elementarity and ng resources are commented out (`vrp.cpp:418-433`). Distances are unrounded Euclidean (`vrp.cpp:521`). After #29 the relaxation is a compile-time choice, `kRouteRelaxation` (`vrp.hpp:34`), but it still defaults to `None` (`I-06`) | LP values are below the literature's (which use elementary or ng routes and 1-decimal distances). Baselines must be produced locally (`T-0.1`, `Q-09`) |
| F-06 | `ResourceGraph` is not thread-safe; `SolutionPool` is | parallel pricing needs one graph per thread (`clone()`) |
| F-07 | The logger checks the level at runtime and locks a mutex per emitted message | keep `LOG_*` out of hot loops, or guard with `LOG_*_ACTIVE()` |
| F-08 | **Update 2026-10-21:** the bidirectional stack (#28–#39, #46, #47, #29) and #48, #51 are merged (`main` @ `129b15e0`, +27.6k lines since `fdc41b26`). Still open: the RouteOpt stack #41–#45, #49 (drafts), #50 (draft), #22. **Update 2026-10-23:** `main` unchanged; new drafts #56 (public renames, on `main`), #57 and #58 (stacked on #56). F-01, F-05, F-11, F-12 re-checked on the new `main`: still true. Other `path:line` refs may have moved | `D-11`: wait for the RouteOpt stack (it holds the CG driver code and #49 budget); re-verify refs before coding |
| F-09 | The 100 % C++ coverage gate covers `cpp/rcspp/`; CI has no Gurobi license | a Gurobi backend cannot sit under `cpp/rcspp/` as is (`Q-05`) |
| F-10 | Python package dependencies: `networkx` only; `PricingPool` uses numpy | adding `highspy` etc. must stay optional (extras) |
| F-11 | The BKS table (`examples/cpp/benchmark_common.hpp`) holds literature values computed with distances **truncated to 1 decimal**; the code uses unrounded distances (`F-05`). C101's LP is integral, and its optimum with real distances is 828.94, so the benchmark prints `FAIL` against 827.3 although the result is correct | fix or relabel the table (`T-0.5`) |
| F-12 | `Row::coefficient` is `long double` (`graph/row.hpp:20`), as are the column building (`algorithm.hpp:496`) and pool pricing (`solution_pool.hpp:1019`); but duals, arc costs, LP solvers and the pool's own storage (`lp_.row_coefs`) are `double`. Since the first commit; no recorded reason. `long double` is 64-bit on MSVC and Apple ARM, 80-bit (16 bytes) on x86-64 GCC/Clang, 128-bit software-emulated on Linux aarch64 | platform-dependent results, a 2× bigger `Row`, x87 math that doesn't vectorize; no precision gain, because every boundary narrows to `double` (`T-0.4`) |
| F-13 | The benchmark already logs per CG iteration: `nb_iter`, `obj`, `min_reduced_cost`, `paths_generated`. The C101 log shows degenerate stalls (the same `obj` for 3–4 iterations, e.g. 863.749, 828.937) | evidence for stabilization (T-2.4); a seed for tier-2 records |
| F-14 | Existing CI (checked 2026-10-23): the `ci.yml` coverage gate runs on every PR base branch but skips draft PRs (`.github/workflows/ci.yml:8`, `:37`). The Python gate reads `tests/python/.coveragerc` (`omit` at `:10`, `fail_under = 90`) | new code under `cpp/rcspp/` and `python/bindings/` is measured once a PR leaves draft, whatever the plan says about CI (`D-52`); excluding a file = one `omit` line (`D-50`) |
