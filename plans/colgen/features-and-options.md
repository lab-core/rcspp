# Every feature and option (`T-1.21`)

Sources: `main` @ `6367ee56` (CHANGELOG, `docs/`); the stack's head `feature/column-generation-cpp-vrp-example` @
`e91db296` (code; its engine and bindings equal `main`'s but for build files and the example's inert hooks); the
colleague's PRs from their descriptions (2026-11-23). Read and decided 2026-11-23 with the user (`D-278`-`D-289`):
the open PRs' rows wait for `T-1.34` (`D-278`); the VRP examples are out of the decisions (`D-284`). `T-1.29` reads
its list from the rows whose decision says "test". Follow-ups: `T-1.31`, `T-1.34` (a reminder), issues #109, #110
(`D-290`). The kernel stack (#62-#75) merged on 2026-11-23 after this snapshot: its rows wait for `T-1.34`. Corrections
to `meeting-2026-11-25.md` §2: bucket parameters are refused by the benchmark, not unchecked (D.2); every mix of the
Python types with `real` first works (E.1); Python gets the whole model report, not a typed refusal (H.6);
`add_coherent_resource` is internal (F).

Paths at `e91db296` unless a PR. Short names:

| short | path | short | path |
|---|---|---|---|
| `alg` | `cpp/rcspp/algorithm/algorithm.hpp` | `PKG/` | `python/src/rcspp/column_generation/` |
| `RG` | `cpp/rcspp/resource/resource_graph.hpp` | `CEX/` | `examples/column_generation/cpp/rcspp/column_generation/` |
| `PRE` | `cpp/rcspp/resource/presets.hpp` | `EPY/` | `examples/column_generation/python/cpp_column_generation/` |
| `gi` | `python/bindings/graph_impl.hpp` | `BND` | `examples/column_generation/bindings/column_generation.cpp` |
| `gc` | `python/bindings/graph.cpp` | `OPT` | `benchmarks/column_generation/OPTIONS.md` |
| `rc` | `python/bindings/resource.cpp` | `blocks` | `benchmarks/column_generation/column_generation_benchmark/options/blocks.py` |
| `gp` | `python/src/rcspp/graph.py` | `prog` | `benchmarks/column_generation/cpp_native/cpp_native_solver.cpp` |
| `pp` | `python/src/rcspp/presets.py` | `pkg`, `bnd`, `nat` | `benchmarks/column_generation/runners/{package,cpp_bindings,cpp_native}_runner.py` |
| `AD` | `docs/advanced/algorithms.md` (main) | `VPY/`, `VCPP/` | `examples/python/`, `examples/cpp/` (the VRP examples) |

Columns: engine C++ | Python (`python/bindings/` and `python/src/rcspp/`) | the package | the C++ example layer |
the benchmark (options and runners). "—" = absent. "by name" = the benchmark's stage takes it as an `AlgorithmParams`
field (`blocks:410-442`, `prog:515-553`, all three runners).

## A. Algorithms

| id | feature | engine C++ | Python | package | C++ example | benchmark | note | decision |
|---|---|---|---|---|---|---|---|---|
| A.1 | simple labelling (forward) | `algorithm/simple_dominance_algorithm.hpp:152` | `gc:85`, `gi:110`, `gp:29` | stage `algorithm` `PKG/graph_pricer.py:69` | `CEX/pricing_strategy.hpp:69-90`, `EPY/graph_pricer.py:39` | `blocks:405` (7 names), `prog:564-593` | the default | `D-279`: as is; test |
| A.2 | backward search | `simple_dominance_algorithm.hpp:139`; simple only | `direction=` `gp:42,587` | stage `direction` `PKG/graph_pricer.py:71` | `EPY/graph_pricer.py:41`, factory `:53` | by name (`direction`) | other algorithms refuse it; #74 adds A\*, pushing, pulling, #86 greedy, tabu (bindings in the PRs) | `D-279`: as is; test |
| A.3 | bidirectional | `algorithm/bidirectional_dominance_algorithm.hpp:64` | `gi:95-107`, `gp:35` (alias of simple + `direction`) | stage | stage | name `bidirectional` | #74 removes the name `bidirectional` (breaking: the package, the example, the benchmark's 7 names); #75 every strategy bidirectional | `D-279`: as is; test |
| A.4 | pushing, pulling | `pushing_dominance_algorithm.hpp:16`, `pulling_dominance_algorithm.hpp:18` | `gc:86-87`, `gp:30-31` | stage | stage | names | forward only until #74 | `D-279`: as is; test |
| A.5 | A\* | `astar_dominance_algorithm.hpp:51` | `gi:78-90`, `gp:34` | stage | stage | name; `prog:574` sets `heuristic_cost_index` | #63: ordered by cost + completion bound, `AStarAlgoEntry` gone | `D-279`: as is; test |
| A.6 | greedy | `algorithm/greedy.hpp:35` | `gc:88`, `gp:32` | stage, a heuristic by default | stage | name | #82 greedy as a depth-first search (`max_iterations` counts labels); #86 `bound_guided_greedy` (bindings in PR) | `D-279`: as is; test |
| A.7 | tabu | `algorithm/tabu_search.hpp:38` | `gc:89`, `gp:33` | stage; refuses infinite `max_iterations` `PKG/graph_pricer.py:318-325` | stage; no such check | name; `OPT:187` says required, only `pkg` refuses | #83 tabu on the kernel; #95 (finite default or refused stage) | `D-279`: as is; the options check refuses an infinite `max_iterations`; test |
| A.8 | `ImprovingTabuSearch`, `diversification_tenure` | `algorithm/improving_tabu_search.hpp:44`, `alg:251` | — | — | — | — | undocumented; #74 a `strategy::ImprovingTabu` descriptor, still C++ only after #83 | `D-280`: bound after #74 and #83 (`D-278`); docs say C++ only now |
| A.9 | `DiversificationSearch` (wraps greedy by default) | `algorithm/diversification_search.hpp:39` | — | — | — | — | `D-260` wanted it as a stage option; `AD:551-565` and `docs/python/quickstart.md:125-138` show a Python example that runs greedy | `D-280`: bound after #74 and #83 (`D-278`); docs say C++ only now |
| A.10 | an algorithm built and passed in (`create_algorithm`, `solve(Algorithm*)`, a custom `Algorithm`) | `RG:303-327`, `RG:408-457`, `alg:390` | — (a new algorithm per solve, `gi:457-488`) | own search `PKG/graph_pricer.py:73-89` (any Python callable) | `CEX/pricing_strategy.hpp:51` (C++ search); `EPY` has none | — | the package's own search covers the use from Python | `D-280`: not in Python; again if `T-1.30` finds the construction costly or a merged feature needs a kept search |

## B. Algorithm parameters (`alg:167-356`, all bound in `gc:206-285`, re-exported `gp:896`)

| id | feature | engine C++ | Python | package | C++ example | benchmark | note | decision |
|---|---|---|---|---|---|---|---|---|
| B.1 | limits: `max_solutions`, `max_extended_labels_per_node`, `max_phases`, `max_iterations`, `timeout_s`, `tolerance`, `return_dominated_solutions`, `prune_by_upper_bound` | `alg:200-225` | `gc:210-221` | stage params, copied `PKG/graph_pricer.py:97-116` | `BND` factory `to_params` (`graph_pricer_factory.hpp:71`) | by name | `max_solutions` also `--max-solutions` in both VRP examples | `D-281`: as is; test (not memory) |
| B.2 | tabu settings: `tabu_tenure`, `tabu_exempt_node_ids`, `tabu_random_noise`, `seed` | `alg:242-253` | `gc:230-233` | stage params | stage params | by name | | `D-281`: as is; test (not memory) |
| B.3 | memory: `max_memory_gb`, `limit_to_{available,total}_ram`, `memory_limit_fraction`, `memory_check_interval`, `memory_pressure_*` | `alg:304-355`, `utils/memory.hpp:223` | `gc:235-256`; `MemoryInfo` `rcspp.cpp:87-94` (no `total_system_bytes`) | stage params | stage params | by name | | `D-281`: as is; test (not memory) |
| B.4 | `release_after_solve` | `alg:239` | `gc:224` | stage params | stage params | by name | no use where each solve builds a new algorithm | `D-281`: as is; docs: no effect from Python |
| B.5 | `heuristic_cost_index`, `should_stop` | `alg:262`, `alg:222` | set by the bindings (`gi:85`, `gi:142`) | — | — | — | internal (A\*, Ctrl-C) | `D-281`: internal, not an item |
| B.6 | #84: `admission_rule` (exact, relaxed, k-best), `dominance_ignored_components`, `max_admitted_labels_per_node` | #84 | bindings in #84 | — | — | — | heuristic admissions lose the proof (`ProofLoss::RelaxedAdmission`); replaces closed #44 | `D-278`: after its merge |
| B.7 | #85: `prune_by_threshold` | #85 | bindings in #85 | — | — | — | refuses a non-additive cost | `D-278`: after its merge |

## C. Bidirectional search

| id | feature | engine C++ | Python | package | C++ example | benchmark | note | decision |
|---|---|---|---|---|---|---|---|---|
| C.1 | `half_way_point`, `critical_resource_index`, `join_after_early_stop` | `alg:270-287`, `algorithm/half_way_policy.hpp:50`, `label_join.hpp:28` | `gc:265-282` | stage params | stage params | by name | | `D-281`: as is; test |
| C.2 | a clock of another type than the cost | template parameter, `RG:303-306,348-351` | — (`gi:92-106` passes the cost's type twice) | — | — | — | `AD:126-142` says C++ only | `D-281`: not in Python unless the colleagues ask |
| C.3 | `SolveResult.bidirectional` (half-way point used, joins) | `alg:76-107` | `gc:116-150` | — | — | — (not recorded) | #41 adds label counts, #43 join statistics (read-only bindings in the PRs) | `D-281`: into `T-2.5` (passed on by the layer, switchable) |
| C.4 | RouteOpt: moving half-way point (#42 `dynamic_half_way`, `HalfWayController`), derived half-way point (#49), join budgets (#43 `max_join_pairs`, `join_column_budget`) | #41-#43, #49 | partly in the PRs (`HalfWayController`, the budgets, `derive_half_way_point`); `dynamic_half_way` unbound (needs a persistent algorithm) | — | — | — | the stack edits the classes #73 deletes and builds on removed VRP files (`F-87`): to be redone on the kernel | `D-278`: after its merge |

## D. Label containers

| id | feature | engine C++ | Python | package | C++ example | benchmark | note | decision |
|---|---|---|---|---|---|---|---|---|
| D.1 | label list (default) | `algorithm/label_buckets.hpp:27` | `gi:476` | default | default | default | | `D-282`: as is |
| D.2 | label buckets (width, bucket and sort resources) | `label_buckets.hpp:163` | `gi:211-254`, `gp:908-1028` `BucketAlgorithmParams`; numerical bucket resource, sort resource the cost's type | stage params `PKG/graph_pricer.py:70,311` | `EPY/graph_pricer.py:80`, factory `:51-69` | refused: `blocks:421-422`, `prog:549-550` | §2's "not checked" is wrong: refused, so not benchmarkable (`D-260` wanted it) | `D-282`: a benchmark stage option (follow-up task); test |
| D.3 | a custom container; buckets on set or bitset resources | user type (`AD:606`) | — | — | — | — | | `D-282`: not in Python |

## E. Resources and graph types

| id | feature | engine C++ | Python | package | C++ example | benchmark | note | decision |
|---|---|---|---|---|---|---|---|---|
| E.1 | `real`, `int`, `real_set`, `int_set`, `bitset` (uint) and their mixes | `resource/resource_traits.hpp:23-43` | `python/bindings/resource_types.hpp:15-29`; 9 graph classes (`gc:410`, `graph_mix2.cpp:14-17`, `graph_mix3.cpp:14-19`), else the smallest bound superset (`gp:208-225`) | user's graph | user's graph | 3 real resources (`instances.py:185-197`, `prog:409-426`) | §2's "combinations beyond those bound" is wrong: every mix with `real` first works | `D-282`: as is; test (ng `bitset` through G.1) |
| E.2 | `uint`, `uint_set`, `size_t_set`, `size_t` bitset | `resource_traits.hpp:25-44` | — | — | — | — | `docs/index.md:103` lists `uint` without "C++ only" | `D-282`: not in Python; docs say C++ only |
| E.3 | a cost or first resource that is not real; a custom resource type | `RG:34` | — (`gp:199-206`) | — | — | — | | `D-282`: not in Python |

## F. Resource functions

| id | feature | engine C++ | Python | package | C++ example | benchmark | note | decision |
|---|---|---|---|---|---|---|---|---|
| F.1 | addition, capacity (scalar), time window (dict), difference, union, intersection, ng-path, min-max (min, max), size (min, max), intersection feasibility, value cost and dominance, subset, superset, trivial feasibility and cost | `resource/concrete/functions/...` | `rc:39-145`, `python/src/rcspp/resource.py` | user's graph | user's graph | fixed model | | `D-283`: as is |
| F.2 | per-node bounds: `NodeBounds`, capacity, min-max and size per node, `add_capacity_resource(per_node)` | `resource/functions/node_bounds.hpp:23`, `PRE:170` | — (per-node windows yes, through the dict) | — | — | — | | `D-283`, `D-290`: issue #110 (low priority, a question); not a benchmark option |
| F.3 | `AdditionExtensionFunction(min_value)` | `addition_extension_function.hpp:40` | — (`rc:39` no argument) | — | — | — | undocumented | `D-283`, `D-290`: issue #110 (low priority, a question); not a benchmark option |
| F.4 | `ReachableFeasibilityFunction`, its composition form | `reachable_feasibility_function.hpp:29`, `reachable_composition_feasibility_function.hpp:25` | — | — | — | — | refused for backward search | `D-283`, `D-290`: issue #110 (low priority, a question); not a benchmark option |
| F.5 | trivial extension and dominance | `trivial_extension_function.hpp:22`, `trivial_dominance_function.hpp:19` | — | — | — | — | | `D-283`, `D-290`: issue #110 (low priority, a question); not a benchmark option |
| F.6 | composition functions, a custom composition | `resource/composition/functions/*`, `RG:40-55` | — (always `ComponentCostFunction<0>`) | — | — | — | | `D-283`: not in Python |
| F.7 | functions written in Python | abstract bases | — (`rc:25-31`: no constructor, no trampoline) | — | — | — | would call Python per extension | `D-283`: not in Python |
| F.8 | `add_cost_resource` of any type | `PRE:81` | real only (`pp:47`) | — | — | — | follows E.3 | `D-283`: not in Python |
| — | `add_coherent_resource` | `PRE:43-70`, in `presets::detail` | — | — | — | — | internal in C++ too: §2's claim dropped | not an item |

## G. Route relaxations (one row each, `D-277`)

| id | feature | engine C++ | Python | package | C++ example | benchmark | note | decision |
|---|---|---|---|---|---|---|---|---|
| G.1 | ng-routes (ng size) | `PRE:202`, `ng-path_extension_function.hpp:59` | `pp:138`, `rc:136-139` | user's graph | VRP C++: `--relaxation ng --ng-size` (8) `VCPP/solve_vrp.cpp:146-149`, `VCPP/vrp/graph.cpp:27-91`; VRP Python: — (`VPY/vrp/graph.py:42-44`, README `:95-97`) | — : `graph` block empty (`OPT:210`, `blocks:248-255` refuses `ng_size`) | `F-98`'s ng runs by hand; `AD:399-400,442-445` say "not available" in Python (stale); #45 ng swapped on a built graph, #50 unreachable neighbours folded (both need bindings), #22 superseded by #50 | `D-284`: a benchmark `graph` option (follow-up task); docs fixed; test 0 and 8 |
| G.2 | elementary routes | `PRE:243` | `pp:175` | user's graph | VRP C++ `--relaxation elementary`; VRP Python — | — | | `D-284`: a benchmark `graph` option (same task); test on the smallest |

## H. Graph operations

| id | feature | engine C++ | Python | package | C++ example | benchmark | note | decision |
|---|---|---|---|---|---|---|---|---|
| H.1 | arcs: add, update, remove, restore, force, `*_if`, clone, reduced costs, rows | `graph/graph.hpp:80-364`, `RG:231,507` | `gi:280-616`, `gp:349-672`; the rest through `gp:764` (`__getattr__`) | pricer updates reduced costs | same | — | | `D-285`: as is |
| H.2 | #96: `add_arcs` from NumPy | — | bindings in #96 | — | — | — | numeric resources only | `D-278`: after its merge |
| H.3 | per-solve arc masks (#83 `ArcMask`, `set_arc_mask`) | #83 | — (needs bindings) | — | — | — | Python has remove and restore | `D-278`: after its merge |
| H.4 | `sort_nodes_by_connectivity` | `RG:276-283` | — (`sort_nodes` only, `gi:316-324`) | — | — | — | a solve calls it on unsorted nodes (`RG:612`) | `D-285`: not in Python |
| H.5 | preprocessing during a solve (on/off) | `RG:429-455,576-626` | `preprocess` `gi:487`, `gp:585` | `GraphPricer(preprocess=)` `PKG/graph_pricer.py:186` | `CEX/graph_pricer.hpp:56` | — | `D-266`: on by default; never paid off up to 50 customers | `D-285`: a benchmark option (follow-up task); test on and off |
| H.6 | model checks; a refused model | `RG:476`, `validation/model_report.hpp:29` | `gi:489-496` (the whole report: `problems`); `ModelRefused` arrives as `RuntimeError` | — | — | — | §2's "full model report" is wrong; what lacks is a typed exception | `D-285`, `D-290`: issue #110 |
| H.7 | node initializers (`add_node(init)`, a default initializer) | `RG:96-153`, `RG:104-131` | — | — | — | — | | `D-285`: not in Python |
| H.8 | #81: `PathEvaluator` (replays a path, says why it fails) | #81 | — (needs bindings) | — | — | — | could check columns | `D-278`: after its merge |
| H.9 | `from_networkx` | — | `gp:783` | — | — | — | Python only | `D-285`: as is |

## I. Solve results and control

| id | feature | engine C++ | Python | package | C++ example | benchmark | note | decision |
|---|---|---|---|---|---|---|---|---|
| I.1 | status, solutions, `is_optimal`, label counts | `alg:41,94-110,413,564` | `gc:106,142-192`; no `is_optimal`, no extended-label count | stage proof from the status | same | — | #62 renames `MaxPhases` → `Truncated` (bindings in PR); #83 `proven`, `proof_lost_by` (bindings in PR); #50 `unreachable_folds` | `D-286`: status covers `is_optimal`; the label count into `T-2.5` |
| I.2 | Ctrl-C | `alg:222` | `gc:9-61`, `python/bindings/active_call.hpp` | `PKG/master_solvers/*` (highspy holds SIGINT, gurobipy raises) | `BND:668,984,991`; `EPY`'s gurobipy master has none | runs killed, exit 130 | | `D-286`: as is; the example's gurobipy gap recorded only; test |
| I.3 | solution pool | `algorithm/solution_pool.hpp:34-306` | `python/bindings/solution_pool.cpp`, `python/src/rcspp/pricing_pool.py:117-1012` | — (not in the loop) | — | — | `T-2.2` | `D-286`: as is (`T-2.2`) |
| I.4 | #49: `Algorithm::set_timeout` | #49 | — | — | — | — | not needed: each Python solve takes `timeout_s` | `D-278`: after its merge |

## J. The column generation layer

| id | feature | package | C++ example | benchmark | note | decision |
|---|---|---|---|---|---|---|
| J.1 | masters: highspy, gurobipy (user's model), `highs`, `gurobi` (C++); `set_option`; self-test | `PKG/master_solvers/*:149,62`, `PKG/master_solver.py:116`, `PKG/master_solver_self_test.py:244` | `CEX/highs/highs_master_solver.hpp:51`, `CEX/gurobi/gurobi_master_solver.hpp:46` (no binding), `BND:1011` | `OPT:159-176`; masters per layer `options/run.py:35-38` | | `D-287`: as is; test every master |
| J.2 | stages: order, `exact` override, only an exact stage proves; `cost_index`; `last_stage_index` | `PKG/graph_pricer.py:72,185,230,268-353` | `CEX/graph_pricer.hpp:55,84-139`; no `last_stage_index` in `EPY` | `OPT:180-194`; `exact` not an option (by algorithm) | | `D-287`: as is |
| J.3 | several pricers, convexity rows | `PKG/master_problem.py:71-86,199` | `CEX/master_problem.hpp:53-57,104` | one pricer | `T-1.9` | `D-287`: `T-1.9` (with an example and tests) |
| J.4 | starts: artificial (big-M), none, starting columns, registry | `PKG/master_initialization.py:33,105`, `PKG/registry.py:24-27` | `CEX/master_initialization.hpp:42,66`, `EPY/registry.py:28-31` | `OPT:200-206` (`artificial` with `cost_factor`, `single-customer`) | automatic factor 1e4 (package, `:64`) against 1e6 (C++, `:45`); the benchmark always passes the cost | `D-287`: as is; 1e4 against 1e6 recorded only; test both starts |
| J.5 | loop: `max_iterations`, `timeout_s`, absolute and relative tolerances, threads, verbose | `PKG/params.py:36-37`, `PKG/tolerances.py:94,110`, `PKG/types.py:71-72` | `CEX/column_generation.hpp:46-48`, `CEX/tolerances.hpp:53,78`, `CEX/types.hpp:94-96` | `OPT:218-225`; verbose — | | `D-287`: as is; test limits and relative tolerances |
| J.6 | hooks `on_iteration`, `on_converged`; timers | `PKG/params.py:42-43`, `PKG/column_generation_loop.py:269-304`, `PKG/statistics.py:16` | `CEX/column_generation.hpp:59-61`; `examples/column_generation/python/column_generation_loop.py` calls neither | — (timers always on) | | `D-288`: no benchmark option; test outside the benchmark |
| J.7 | results: 8 statuses, objective, `lower_bound`, statistics, columns | `PKG/column_generation_loop.py:25-64` | `CEX/column_generation.hpp:65-96` | `result.json`, `solution.json` | objective `None` (package) against `inf` (C++); `artificial_cost` `None` against 0.0; `lower_bound` = LP at `Optimal` only, not recorded | `D-288`: as is |
| J.8 | integer master over the columns (timeout, gap, column type, threads) | `PKG/column_generation_loop.py:181`, `PKG/types.py:76-83` | `CEX/column_generation.hpp:121`, `CEX/types.hpp:103-114` | `OPT:227-245` | | `D-288`: as is; test |
| J.9 | per-iteration data: iteration, objective, new and total columns, elapsed | `PKG/statistics.py:40-52` | `CEX/statistics.hpp:35-46` | — | `T-2.5`; duals only at convergence | `D-288`: `T-2.5` |
| J.10 | reserved, never read: `min_reduced_cost_bound`, `optimality_gap`, `farkas()`, `Pricer.update` | `PKG/pricer.py:55,99`, `PKG/tolerances.py:49`, `PKG/master_solver.py:110` | `CEX/pricer.hpp:40,79`, `CEX/tolerances.hpp:47` | — | `T-2.3` (bound, gap); `farkas` for a phase-one start | `D-288`: recorded; bound and gap `T-2.3`, `farkas` no task |
| J.11 | phase 2: Lagrangian bound, gap stop, pool, column management, stabilization, pricing budget (#49's `PricingBudget`, example only), pricer schedule, parallel pricers | — | — | — | `T-2.1`-`T-2.6` (`D-274`) | `D-288`: `T-2.1`-`T-2.6` |

## K. Docs findings (main; issue #109, `D-290`)

`AD:399-400,442-445` (ng-path and elementary "not available" in Python: stale); `AD:551-565`,
`docs/python/quickstart.md:125-138` (a Diversification example that runs greedy); `docs/index.md:103-104` (`uint`,
Diversification without "C++ only"); `docs/cpp/concepts.md:199-202` (`LabelBuckets<R>`, real signature has four
parameters); `docs/python/api.rst` has no `rcspp.presets`; undocumented: A.8, F.3, H.4, `UIntSetResource`,
`SizeTSetResource`, `SizeTBitsetResource`.

## L. The colleague's PRs

| PR | state | adds (rows) | bindings once merged |
|---|---|---|---|
| #22 | open, stale `src/` layout | ng unreachable fold (G.1) | superseded by #50 |
| #41-#43, #45, #49, #50 | drafts, on `main`; #44 closed | C.3, C.4, G.1, I.1, I.4, J.11 | #45, #50 need bindings; the rest in the PRs or none; all to redo on the kernel and the layer (`F-87`) |
| #46-#48 | merged | `check_model`, `direction` (A.2, H.6) | already bound |
| #62-#75 | open (no #67, #70) | A.2-A.5, I.1; the kernel itself internal | in the PRs; #74 breaks `algorithm="bidirectional"` |
| #81-#87 | drafts | A.2, A.6, A.7, B.6, B.7, H.3, H.8, I.1 | #81 `PathEvaluator`, #83 arc masks need bindings; the rest in the PRs |
| #96 | draft | H.2 | it is the binding |
