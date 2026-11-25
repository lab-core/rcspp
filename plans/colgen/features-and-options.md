# Every feature and option (`T-1.21`, again for `T-1.34`)

Sources since 2026-11-24 (`T-1.34`): `main` @ `b589e406` (CHANGELOG, `docs/`; the kernel stack #62-#75 and #108
merged); the stack's head `feature/column-generation-vehicle-types` @ `71c13451` (code; its engine equals `main`'s,
its bindings and `graph.py` add the stack's Ctrl-C and helpers); the colleague's PRs from their descriptions. First
read 2026-11-23 on `main` @ `6367ee56` and `e91db296` (`D-278`-`D-289`); every path re-checked 2026-11-24. The kernel
stack's rows: `D-294`-`D-301`, accepted 2026-11-24. The VRP examples are out of the decisions (`D-284`). `T-1.29`
reads its list from the rows whose decision says "test". Follow-ups: `T-1.31`, `T-1.34` (a reminder),
issues #109 and #110 (`D-290`). Corrections to `meeting-2026-11-25.md` §2: bucket parameters are refused by the
benchmark, not unchecked (D.2); every mix of the Python types with `real` first works (E.1); Python gets the whole
model report, not a typed refusal (H.6); `add_coherent_resource` is internal (F).

Paths at `71c13451` unless a PR. Short names:

| short | path | short | path |
|---|---|---|---|
| `alg` | `cpp/rcspp/algorithm/algorithm.hpp` | `PKG/` | `python/src/rcspp/column_generation/` |
| `par` | `cpp/rcspp/algorithm/algorithm_params.hpp` | `CEX/` | `examples/column_generation/cpp/rcspp/column_generation/` |
| `res` | `cpp/rcspp/algorithm/solve_result.hpp` | `EPY/` | `examples/column_generation/python/cpp_column_generation/` |
| `str` | `cpp/rcspp/algorithm/strategies.hpp` | `BND` | `examples/column_generation/bindings/column_generation.cpp` |
| `lab/` | `cpp/rcspp/algorithm/labelling/` | `OPT` | `benchmarks/column_generation/OPTIONS.md` |
| `RG` | `cpp/rcspp/resource/resource_graph.hpp` | `blocks` | `benchmarks/column_generation/column_generation_benchmark/options/blocks.py` |
| `PRE` | `cpp/rcspp/resource/presets.hpp` | `prog` | `benchmarks/column_generation/cpp_native/cpp_native_solver.cpp` |
| `gi` | `python/bindings/graph_impl.hpp` | `pkg`, `bnd`, `nat` | `benchmarks/column_generation/runners/{package,cpp_bindings,cpp_native}_runner.py` |
| `gc` | `python/bindings/graph.cpp` | `AD` | `docs/advanced/algorithms.md` |
| `rc` | `python/bindings/resource.cpp` | `VPY/`, `VCPP/` | `examples/python/`, `examples/cpp/` (the VRP examples) |
| `gp`, `pp` | `python/src/rcspp/{graph,presets}.py` | `ref/` | `tests/cpp/reference/` (the old classes, frozen for tests) |

Columns: engine C++ | Python (`python/bindings/` and `python/src/rcspp/`) | the package | the C++ example layer |
the benchmark (options and runners). "—" = absent. "by name" = the benchmark's stage takes it as an `AlgorithmParams`
field (`blocks:410-442`, `prog:514-552`, all three runners).

## A. Algorithms

Since #74 a strategy is a descriptor of `str` (`Simple`, `AStar`, `Pushing`, `Pulling`, `Greedy`, `Tabu`,
`ImprovingTabu`, `Diversification<Inner>`), built from the kernel's parts (`lab/parts/`); the old classes are only
`ref/`. Python's 6 names `gp:29-34`, dispatched `gi:77-82`; the direction applies to any of them (`gp:41`, `gp:72`).

| id | feature | engine C++ | Python | package | C++ example | benchmark | note | decision |
|---|---|---|---|---|---|---|---|---|
| A.1 | simple labelling (forward) | `str:48`, `lab/labelling_algorithm.hpp:47` | `gc:85`, `gi:77`, `gp:29` | stage `algorithm` `PKG/graph_pricer.py:69` | `CEX/pricing_strategy.hpp:69-90`, `EPY/graph_pricer.py:39` | `blocks:405` (6 names), `prog:563-592` | the default | `D-279`: as is; test |
| A.2 | backward search | every labelling strategy (#74); `direction` `par:119` | `direction=` `gp:41,573` | stage `direction` `PKG/graph_pricer.py:71` | `EPY/graph_pricer.py:41`; any strategy through `make` | by name (`direction`) | greedy and tabu refuse it (`ValueError` at the solve); #86 backward dives | `D-279`; `D-294`: every exact algorithm in each direction in `T-1.29` |
| A.3 | bidirectional | `lab/bidirectional_labelling_algorithm.hpp:83`, every labelling strategy (#75) | `direction="bidirectional"`; the algorithm name is gone (#74) | stage | stage | `direction` by name | the stack followed the removal in its rebase | `D-279`; `D-294` |
| A.4 | pushing, pulling | `str:67,74` | `gc:86-87`, `gp:30-31` | stage | stage | names | backward and bidirectional since #74, #75 | `D-279`; `D-294` |
| A.5 | A\* | `str:59` | `gp:34` | stage | stage | name | bounds the objective by a Bellman–Ford over each arc's share (#68); `heuristic_cost_index` gone; backward and bidirectional since #74, #75 | `D-279`; `D-294` |
| A.6 | greedy | `algorithm/greedy.hpp:37`, `str:80` | `gc:88`, `gp:32` | stage, a heuristic by default | stage | name | forward only; #82 greedy as a depth-first search; #86 `bound_guided_greedy` | `D-279`; `D-295`: the benchmark refuses another direction |
| A.7 | tabu | `algorithm/tabu_search.hpp:38`, `str:86` | `gc:89`, `gp:33` | stage; refuses infinite `max_iterations` `PKG/graph_pricer.py:318-325` | stage; no such check | name; `OPT:187` says required, only `pkg` refuses | forward only; a dive cut short no longer reads as a proof (#62); #83 tabu on the kernel; #95 | `D-279`; `D-295` |
| A.8 | `ImprovingTabuSearch` | `algorithm/improving_tabu_search.hpp:44`, `str:92` | — | — | any strategy through `make` | — | `Truncated` when it spends `max_iterations` (#62); `diversification_tenure` is gone | `D-280`; `D-296`: Python after #83 still |
| A.9 | `DiversificationSearch` (greedy inside by default) | `algorithm/diversification_search.hpp:45`, `str:106` | — | — | any strategy through `make` | — | `AD:566-580` and `docs/python/quickstart.md:125-138` show a Python example that runs greedy (#109) | `D-280`; `D-296` |
| A.10 | an algorithm built and passed in (`create_algorithm`, `solve(Algorithm*)`) | `RG:301-305`, `RG:395-398`, `alg:99` | — (a new algorithm per solve, `gi:424-455`) | own search `PKG/graph_pricer.py:73-89` (any Python callable) | `CEX/pricing_strategy.hpp:51` (C++ search); `EPY` has none | — | the package's own search covers the use from Python | `D-280`: not in Python |
| A.11 | a strategy of one's own: a descriptor from `rcspp::parts` or own parts, checked by concepts; `test_support::optimum_problem` | `str:17-43`, `lab/parts/`, `algorithm/make_algorithm.hpp:51,174`, `cpp/rcspp/test_support/optimum_problem.hpp:109`; `AD:817,877` | — | own search (A.10) | `PricingStage::make<Strategy>` `CEX/pricing_strategy.hpp:69-81` | — | new with #74 | `D-297`: C++ only |

## B. Algorithm parameters (`par:34-263`, bound in `gc:206-285`, re-exported `gp:877`)

| id | feature | engine C++ | Python | package | C++ example | benchmark | note | decision |
|---|---|---|---|---|---|---|---|---|
| B.1 | limits: `max_solutions`, `max_extended_labels_per_node`, `max_phases`, `max_iterations`, `timeout_s`, `tolerance`, `return_dominated_solutions`, `prune_by_incumbent` | `par:62-112` | `gc:210-221` | stage params, copied `PKG/graph_pricer.py:97-116` | `BND` factory `to_params` (`graph_pricer_factory.hpp:71`) | by name | `prune_by_incumbent` (off by default `par:78`) now adds a completion bound, one Bellman–Ford more a solve; the package passes its threshold as the upper bound (`PKG/graph_pricer.py:294`) | `D-281`; `D-298`: the prune in `T-1.29` |
| B.2 | tabu settings: `tabu_tenure`, `tabu_exempt_node_ids`, `tabu_random_noise`, `seed` | `par:129-140` | `gc:230-233` | stage params | stage params | by name | | `D-281`: as is; test (not memory) |
| B.3 | memory: `max_memory_gb`, `limit_to_{available,total}_ram`, `memory_limit_fraction`, `memory_check_interval`, `memory_pressure_*` | `par:182-239`, `utils/memory.hpp:229` | `gc:235-256`; `MemoryInfo` `rcspp.cpp:87-94` (no `total_system_bytes`) | stage params | stage params | by name | | `D-281`: as is; test (not memory) |
| B.4 | `release_after_solve` | `par:126` | `gc:224` | stage params | stage params | by name | no use where each solve builds a new algorithm | `D-281`: as is; docs: no effect from Python |
| B.5 | `should_stop` | `par:109` | set by the bindings (`gi:108`) | — | — | — | internal (Ctrl-C); `heuristic_cost_index` removed (#68) | `D-281`: internal, not an item |
| B.6 | #84: `admission_rule` (exact, relaxed, k-best), `dominance_ignored_components`, `max_admitted_labels_per_node` | #84 | bindings in #84 | — | — | — | heuristic admissions lose the proof (`ProofLoss::RelaxedAdmission`) | `D-278`: after its merge |
| B.7 | #85: `prune_by_threshold` | #85 | bindings in #85 | — | — | — | refuses a non-additive cost | `D-278`: after its merge |
| B.8 | the kernel's caps (#62, #73): the per-node quota strict in Simple and A\* (fewer labels, fewer solutions); a bidirectional `max_iterations` caps the whole search, one phase, counting labels taken | `lab/parts/schedule/per_node_quota.hpp`, `lab/bidirectional_labelling_algorithm.hpp` | as C++ | none set by default (`PKG/`, `CEX/`, `presets.toml`, `example.toml` checked) | none set | by name | moves only capped heuristic stages; the recorded runs are checked by `post-rebase-runs.md` S11 | `D-300`: no action |

## C. Bidirectional search

| id | feature | engine C++ | Python | package | C++ example | benchmark | note | decision |
|---|---|---|---|---|---|---|---|---|
| C.1 | `half_way_point`, `critical_resource_index`, `join_after_early_stop` | `par:148-165`, `lab/half_way.hpp:34`, `lab/join.hpp:31` | `gc:265-282` | stage params | stage params | by name | | `D-281`: as is; test |
| C.2 | a clock of another type than the cost | template parameter `CriticalResourceType`, `RG:301-305,335-338` | — (`gi:60-62`: the clock is the cost's type) | — | — | — | `AD:131-147` says C++ only | `D-281`: not in Python unless the colleagues ask |
| C.3 | `SolveResult.bidirectional` (`half_way_used`, `half_way_off_reason`, joins) | `res:53-70` | `gc:116-150` | — | — | — (not recorded) | `bounded_by_half_way` renamed `half_way_used`; #41 label counts, #43 join statistics | `D-281`: into `T-2.5`; `D-301`: the rename only |
| C.4 | RouteOpt: moving half-way point (#42), derived half-way point (#49), join budgets (#43) | #41-#43, #49 | partly in the PRs | — | — | — | the stack edits classes now in `ref/` (`F-87`): to be redone on the kernel | `D-278`: after its merge |

## D. Label containers

| id | feature | engine C++ | Python | package | C++ example | benchmark | note | decision |
|---|---|---|---|---|---|---|---|---|
| D.1 | label list (default) | `cpp/rcspp/label/label_list.hpp` | `gi:442` | default | default | default | moved from `algorithm/` | `D-282`: as is |
| D.2 | label buckets (width, bucket and sort resources) | `cpp/rcspp/label/label_buckets.hpp:65` | `gi:177-220`, `gp:889-1009` `BucketAlgorithmParams` | stage params `PKG/graph_pricer.py:70,311` | `EPY/graph_pricer.py:80` | refused: `blocks:421-422`, `prog:548-549` | | `D-282`: a benchmark stage option (`T-1.31`); test |
| D.3 | a custom container; buckets on set or bitset resources | user type (`AD:632`) | — | — | — | — | | `D-282`: not in Python |

## E. Resources and graph types

| id | feature | engine C++ | Python | package | C++ example | benchmark | note | decision |
|---|---|---|---|---|---|---|---|---|
| E.1 | `real`, `int`, `real_set`, `int_set`, `bitset` (uint) and their mixes | `resource/resource_traits.hpp:23-43` | `python/bindings/resource_types.hpp:15-29`; 9 graph classes (`gc:410`, `graph_mix2.cpp`, `graph_mix3.cpp`), else the smallest bound superset (`gp:194-211`) | user's graph | user's graph | 3 real resources (`instances.py:185-197`, `prog:408-425`) | | `D-282`: as is; test (ng `bitset` through G.1) |
| E.2 | `uint`, `uint_set`, `size_t_set`, `size_t` bitset | `resource_traits.hpp:25-44` | — | — | — | — | `docs/index.md:103` lists `uint` without "C++ only" | `D-282`: not in Python; docs say C++ only |
| E.3 | a cost or first resource that is not real; a custom resource type | `RG:34` | — (`gp:185-192`) | — | — | — | | `D-282`: not in Python |

## F. Resource functions

| id | feature | engine C++ | Python | package | C++ example | benchmark | note | decision |
|---|---|---|---|---|---|---|---|---|
| F.1 | addition, capacity, time window, difference, union, intersection, ng-path, min-max, size, intersection feasibility, value cost and dominance, subset, superset, trivial feasibility and cost | `resource/concrete/functions/...` | `rc:39-145`, `python/src/rcspp/resource.py` | user's graph | user's graph | fixed model | | `D-283`: as is |
| F.2 | per-node bounds: `NodeBounds`, `add_capacity_resource(per_node)` | `resource/functions/node_bounds.hpp:23`, `PRE:170` | — (per-node windows yes) | — | — | — | | `D-283`, `D-290`: issue #110 |
| F.3 | `AdditionExtensionFunction(min_value)` | `addition_extension_function.hpp:46` | — (`rc:39` no argument) | — | — | — | a floor now turns the completion bound off (#68) | `D-283`, `D-290`: issue #110 |
| F.4 | `ReachableFeasibilityFunction`, its composition form | `reachable_feasibility_function.hpp:29`, `reachable_composition_feasibility_function.hpp:25` | — | — | — | — | refused for backward search | `D-283`, `D-290`: issue #110 |
| F.5 | trivial extension and dominance | `trivial_extension_function.hpp:22`, `trivial_dominance_function.hpp:19` | — | — | — | — | | `D-283`, `D-290`: issue #110 |
| F.6 | composition functions, a custom composition | `resource/composition/functions/*`, `RG:40-55` | — | — | — | — | | `D-283`: not in Python |
| F.7 | functions written in Python | abstract bases | — (`rc:25-31`) | — | — | — | would call Python per extension | `D-283`: not in Python |
| F.8 | `add_cost_resource` of any type | `PRE:81` | real only (`pp:47`) | — | — | — | follows E.3 | `D-283`: not in Python |
| — | `add_coherent_resource` | `PRE:43-70`, in `presets::detail` | — | — | — | — | internal | not an item |

## G. Route relaxations (one row each, `D-277`)

| id | feature | engine C++ | Python | package | C++ example | benchmark | note | decision |
|---|---|---|---|---|---|---|---|---|
| G.1 | ng-routes (ng size) | `PRE:202`, `ng-path_extension_function.hpp:59` | `pp:138`, `rc:136-139` | user's graph | VRP C++ `--relaxation ng --ng-size` `VCPP/solve_vrp.cpp:146-149`, `VCPP/vrp/graph.cpp:27-91`; VRP Python — (`VPY/vrp/graph.py:60-62`) | — : `graph` block empty (`OPT:210`, `blocks:249`) | `AD:414-415,457-460` still say "not available" (#109); #45, #50 need bindings | `D-284`: a benchmark `graph` option (`T-1.31`); test 0 and 8 |
| G.2 | elementary routes | `PRE:243` | `pp:175` | user's graph | VRP C++ `--relaxation elementary` | — | | `D-284`: same task; test on the smallest |

## H. Graph operations

| id | feature | engine C++ | Python | package | C++ example | benchmark | note | decision |
|---|---|---|---|---|---|---|---|---|
| H.1 | arcs: add, update, remove, restore, force, `*_if`, clone, reduced costs, rows | `graph/graph.hpp:80-364`, `RG:231,494` | `gi:282-582`, `gp:335-658`; the rest through `gp:745` | pricer updates reduced costs | same | — | | `D-285`: as is |
| H.2 | #96: `add_arcs` from NumPy | — | bindings in #96 | — | — | — | | `D-278`: after its merge |
| H.3 | per-solve arc masks (#83) | #83 | — | — | — | — | | `D-278`: after its merge |
| H.4 | `sort_nodes_by_connectivity` | `RG:276-283` | — (`sort_nodes` only, `gi:282-290`) | — | — | — | a solve calls it (`RG:599`) | `D-285`: not in Python |
| H.5 | preprocessing during a solve (on/off) | `RG:416-442,563-613` | `preprocess` `gi:453`, `gp:571` | `GraphPricer(preprocess=)` `PKG/graph_pricer.py:186` | `CEX/graph_pricer.hpp:56` | — | backward and bidirectional run their model checks whatever it says | `D-285`: a benchmark option (`T-1.31`); test on and off |
| H.6 | model checks; a refused model | `RG:463`, `validation/model_report.hpp:29` | `gi:456-462`; `ModelRefused` arrives as `RuntimeError` | — | — | — | | `D-285`, `D-290`: issue #110 |
| H.7 | node initializers | `RG:96-153` | — | — | — | — | | `D-285`: not in Python |
| H.8 | #81: `PathEvaluator` | #81 | — | — | — | — | could check columns | `D-278`: after its merge |
| H.9 | `from_networkx` | — | `gp:764` | — | — | — | Python only | `D-285`: as is |

## I. Solve results and control

| id | feature | engine C++ | Python | package | C++ example | benchmark | note | decision |
|---|---|---|---|---|---|---|---|---|
| I.1 | status (`Complete`, `Timeout`, `MaxSolutions`, `Truncated`, `Interrupted`, `MemoryLimit`), solutions, `is_optimal`, label counts | `res:18-25,73-90`, `alg:126,259` | `gc:106-112,142-192`; no `is_optimal`, no extended-label count | proves on `Complete` untrimmed only (`PKG/graph_pricer.py:332-353`); its test lacks `Truncated`, `MaxSolutions`, `MemoryLimit` | same (`CEX/graph_pricer.hpp:131`) | — | `Truncated` merged (#62): a capped or abandoning search no longer reads as complete; #83 `proven`, `proof_lost_by`; #50 `unreachable_folds` | `D-286`; `D-299`: the missing statuses tested in `T-1.31` |
| I.2 | Ctrl-C | `par:109` | `gc:9-61`, `python/bindings/active_call.hpp` | `PKG/master_solvers/*` | `BND:668,984,991`; `EPY`'s gurobipy master has none | runs killed, exit 130 | | `D-286`: as is; test |
| I.3 | solution pool | `algorithm/solution_pool.hpp:34-306` | `python/bindings/solution_pool.cpp`, `python/src/rcspp/pricing_pool.py:117-1012` | — (not in the loop) | — | — | `T-2.2` | `D-286`: as is (`T-2.2`) |
| I.4 | #49: `Algorithm::set_timeout` | #49 | — | — | — | — | each Python solve takes `timeout_s` | `D-278`: after its merge |

## J. The column generation layer

Unchanged by the kernel stack but for the paths' lines; checked 2026-11-24.

| id | feature | package | C++ example | benchmark | decision |
|---|---|---|---|---|---|
| J.1 | masters, `set_option`, self-test | `PKG/master_solvers/*`, `PKG/master_solver.py:116`, `PKG/master_solver_self_test.py:244` | `CEX/highs/highs_master_solver.hpp:51`, `CEX/gurobi/gurobi_master_solver.hpp:46`, `BND:1011` | `OPT:159-176`; `options/run.py:35-38` | `D-287`: as is; test every master |
| J.2 | stages, `exact`, `cost_index`, `last_stage_index` | `PKG/graph_pricer.py:72,185,230,235-353` | `CEX/graph_pricer.hpp:55,84-139` | `OPT:180-194` | `D-287`: as is |
| J.3 | several pricers, convexity rows | `PKG/master_problem.py:71-86,199` | `CEX/master_problem.hpp:53-57,104` | one pricer | `D-287`: `T-1.9` (done, #114) |
| J.4 | starts: artificial (1e4 against 1e6 in C++), none, columns, registry | `PKG/master_initialization.py:33,105`, `PKG/registry.py:24-27` | `CEX/master_initialization.hpp:42,66`, `EPY/registry.py:28-31` | `OPT:200-206` | `D-287`: as is; test both starts |
| J.5 | loop limits, tolerances, threads, verbose | `PKG/params.py:36-37`, `PKG/tolerances.py:94,110`, `PKG/types.py:71-72` | `CEX/column_generation.hpp:46-48`, `CEX/tolerances.hpp:53,78`, `CEX/types.hpp:94-96` | `OPT:218-225`; verbose — | `D-287`: as is; test |
| J.6 | hooks, timers | `PKG/params.py:42-43`, `PKG/column_generation_loop.py:269-304`, `PKG/statistics.py:16` | `CEX/column_generation.hpp:59-61` | — | `D-288`: test outside the benchmark |
| J.7 | results (objective `None` against `inf`) | `PKG/column_generation_loop.py:25-64` | `CEX/column_generation.hpp:65-96` | `result.json`, `solution.json` | `D-288`: as is |
| J.8 | integer master | `PKG/column_generation_loop.py:181`, `PKG/types.py:76-83` | `CEX/column_generation.hpp:121`, `CEX/types.hpp:103-114` | `OPT:227-245` | `D-288`: as is; test |
| J.9 | per-iteration data | `PKG/statistics.py:40-52` | `CEX/statistics.hpp:35-46` | — | `D-288`: `T-2.5` |
| J.10 | reserved: `min_reduced_cost_bound`, `optimality_gap`, `farkas()`, `Pricer.update` | `PKG/pricer.py:55,99`, `PKG/tolerances.py:49`, `PKG/master_solver.py:110` | `CEX/pricer.hpp:40,79`, `CEX/tolerances.hpp:47` | — | `D-288`: bound and gap `T-2.3` |
| J.11 | phase 2 (bound, gap, pool, stabilization, budget, schedule, parallel) | — | — | — | `D-288`: `T-2.1`-`T-2.6` |

## K. Docs findings (issue #109, `D-290`; still true on `b589e406`)

`AD:414-415,457-460` (ng-path and elementary "not available" in Python); `AD:566-580`,
`docs/python/quickstart.md:125-138` (a Diversification example that runs greedy); `docs/index.md:103-104` (`uint`,
Diversification without "C++ only"); `docs/cpp/concepts.md:199-202` (`LabelBuckets<R>`); `docs/python/api.rst` has
no `rcspp.presets`; undocumented: A.8, F.3, H.4, `UIntSetResource`, `SizeTSetResource`, `SizeTBitsetResource`.

## L. The colleague's PRs

| PR | state (2026-11-24) | adds (rows) | bindings once merged |
|---|---|---|---|
| #22 | open, stale `src/` layout | G.1 | superseded by #50 |
| #41-#43, #45, #49, #50 | drafts on `main` before the kernel | C.3, C.4, G.1, I.1, I.4, J.11 | to redo on the kernel and the layer (`F-87`) |
| #62-#75, #108 | **merged** 2026-11-23 | A.2-A.8, A.11, B.1, B.5, B.8, C.3, I.1 | bound; rows decided `D-294`-`D-301` |
| #81-#87 | drafts, #81 on `main` | A.2, A.6, A.7, B.6, B.7, H.3, H.8, I.1 | #81 `PathEvaluator`, #83 arc masks need bindings |
| #96 | draft | H.2 | it is the binding |
