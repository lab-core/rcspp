# Changelog

Notable changes to rcspp. There has been no release yet, so everything below is **Unreleased**.
Each entry says what an existing model or caller sees differently.
New features are summarised, with a pointer to where they are documented.

## Unreleased

### Behaviour changes

- **No path passes through a source or continues past a sink, in any algorithm.** A path starts
  at a source and ends at a sink, with neither strictly inside it. Before, every algorithm could
  extend a label into a source, and `pulling` and `greedy` (with the tabu and diversification
  searches) could extend one out of a sink. On a graph with several sources, answers can change.
  See "What every algorithm returns" in `docs/advanced/algorithms.md`.
- **`add_arc` refuses an arc into a source or out of a sink**, which no path could use:
  `std::invalid_argument` in C++ (`Graph::add_arc`, and so `ResourceGraph::add_arc`), `ValueError`
  in Python when the buffered arcs are sent to the graph, which then adds none of the arcs
  buffered with it and keeps later arc ids matching. A model with such an arc used to build,
  and now fails when it is built. Model a depot that a route passes through as separate source and
  sink nodes, as the VRP example does.
- **Breaking: `ResourceGraph::create_algorithm<Strategy>(params)` returns
  `std::unique_ptr<Algorithm<ResourceType, LabelContainerType>>`**, not
  `std::unique_ptr<Strategy<ResourceType, LabelContainerType>>`: the class that searches in
  `params.direction` is not always `Strategy` itself. Code that stored the result in a
  `std::unique_ptr<Strategy<…>>`, or called a member only `Strategy` has, no longer compiles; use
  `auto`, or build the algorithm directly. With constructor arguments beyond the params,
  `create_algorithm` still returns the type it names.
- **`preprocess` only says whether a solve may reduce the graph.** A backward or bidirectional
  search runs the model checks its direction needs before any preprocessing, whatever `preprocess`
  says, and also when `Algorithm::solve` is called directly; a model that fails them is refused
  with `ModelRefused`. Forward solves are unchanged: they need no check.
- **`NgPathExtensionFunction` reads the node it adds from the arc's endpoints, and ignores the
  arc's value. This is a breaking change outside the ng-route case.** Before, it added the arc's
  value, normally `{origin}`, to the memory. It also stores the memory already narrowed by the node
  arrived at, which makes dominance stronger: on R101 with ng(8) the forward search extends 34 475
  labels instead of 190 339.
  - For the ng-route condition (`forbidden(v) = {v}` on `IntersectionFeasibilityFunction`) with
    arcs carrying `{origin}`, the optimum is unchanged. The set of columns a forward ng pricing
    solve returns can differ.
  - Otherwise results can change. An arc value other than `{origin}` is now ignored, including an
    empty one, which used to leave the memory empty and ng inert. A forbidden set other than `{v}`
    now tests a memory already narrowed by the node arrived at, so a node forgotten on arrival no
    longer counts: in the example in `tests/cpp/test_ng_forward.hpp`, -1 becomes -10.
- **`MinMaxFeasibilityFunction(min, max, merge_by_increasing_value)` is removed.** The flag only
  chose the direction of `can_be_merged`, which no algorithm called before this release; the join
  test now follows the extension the function is paired with. Drop the third argument:
  `MinMaxFeasibilityFunction(min, max)`.
- **`can_be_merged` is now `can_be_joined`**, on feasibility functions, resources and
  compositions, to match the join it serves. An override of your own must be renamed; it is only
  called when the function's `join_rule()` is `JoinRule::Custom`. `TrivialFeasibilityFunction`
  and `TimeWindowFeasibilityFunction` no longer override it: their rules, `AlwaysTrue` and
  `ValueOrder`, decide without it.
- **`TimeWindowExtensionFunction::extend_back` returns a deadline.** It computes
  `min(latest[origin], deadline - arc_time)`, the latest time a label can leave the arc's origin
  and still meet the deadline it carries. Before, it added the travel time. No algorithm called
  it before this release.
- **Renamed, with no alias.** Code that names the old spelling, or overrides it, no longer
  compiles; Python code that passes an old keyword or reads an old attribute raises.
  - `Preprocessor::remove_arc` is `should_remove_arc`: it only answers, where
    `Graph::remove_arc` removes.
  - The values of `AlgorithmStatus` are in CapWords, as in Python: `Complete`, `Timeout`,
    `MaxSolutions`, `MaxPhases`, `Interrupted` and `MemoryLimit`.
  - `Algorithm::prepareNextPhase` is `prepare_next_phase`. Rename your overrides.
  - `Algorithm::get_labels_at_sinks` is `get_labels_at_terminals`: going backward, it returns
    the labels at the sources.
  - `Graph::track_modifications` is `mark_unmodified`: it clears the modified flag.
  - `ResourceGraph::process_feasibility` is `remove_infeasible_arcs`, in C++ and in Python, where
    it was `preprocess_feasibility`.
  - Fields of `AlgorithmBaseParams`, in C++ and in Python: `stop_after_X_solutions` is
    `max_solutions` and `num_max_phases` is `max_phases`, after the statuses they cause;
    `num_labels_to_extend_by_node` is `max_extended_labels_per_node`, a cap that applies in each
    phase; `forbidden_tabu` is `tabu_exempt_node_ids`: it lists the nodes whose arcs are never made
    tabu. In C++ only, `prune_based_on_upper_bound_` is `prune_by_upper_bound`.
  - `range_buckets` is `bucket_width`, on `BucketAlgorithmParams` in Python and on the
    `LabelBuckets` constructor, and `LabelBuckets::suggest_range` is `suggest_bucket_width`: it is
    the width of a bucket along its resource, not a number of buckets.
  - `ContainDominanceFunction` is `SupersetDominanceFunction`, `InclusionDominanceFunction` is
    `SubsetDominanceFunction`, and `SubtractExtensionFunction` is `DifferenceExtensionFunction`,
    in C++ and in Python, with their headers. The first two names were synonyms for opposite
    rules; the new ones say which set dominates.
  - Python keywords: `id` is `node_id`, `arc_id` or `column_id` (`add_node`, `get_node`,
    `get_arc`, and the pool's `get`, `get_activity` and `get_entry`); `origin_id` and
    `destination_id` are `origin_node_id` and `destination_node_id` (`add_arc`, `get_arcs`), as
    on `ResourceGraph`; the pool's `col_ids` is `column_ids`, and `update_activity(basis_ids)` is
    `update_activity(basis_column_ids)`.
  - The LP arrays have one vocabulary: `column_costs`, `column_starts`, `row_indices` and
    `row_coefficients`. That changes the keywords of `SharedPricingPool.add_from_lp_arrays`,
    which were `col_costs`, `row_starts`, `col_indices` and `col_values`, and its views
    (`column_costs_view`, `column_starts_view`, `row_indices_view`, `row_coefficients_view`).
- **Removed: API that did nothing, could not be used, or duplicated another name.**
  - `AlgorithmBaseParams::use_pool`, in C++ and Python: nothing read it, so setting it to `false`
    changed nothing.
  - `ReachableCompositionExtensionFunction`, whose constructor always threw.
  - `Graph::get_nodes_size()` and `get_arcs_size()`: use `get_number_of_nodes()` and
    `get_number_of_arcs()`, which return the same values.
  - Python `ResourceGraph.get_nodes_size()` and `get_arcs_size()`: use `number_of_nodes()` and
    `number_of_arcs()`, which flush the buffers first and so count every node and arc.
  - `FilteredSolutionPool.size()` in Python: use `len()`.
  - `Algorithm::all_labels_processed()`, `is_resource_base_composition_v` and
    `ResourceCompositionTypeConcept`, which nothing used.
- **`FilteredSolutionPool::price` keeps only reduced costs below `-1e-9` by default**, in C++ and
  in the binding, where it used to be `0.0`. The Python pools already used `-1e-9`. The value is
  now one constant, `kDefaultPriceThreshold`, bound to Python as
  `rcspp._core.solution_pool.DEFAULT_PRICE_THRESHOLD`. A column whose reduced cost is negative
  only by rounding is no longer returned as improving.
- **Python `ResourceGraph.to_string()` leaves the arcs out by default**, as in C++. `repr()` of a
  `ResourceGraph` is now a one-line summary that does not flush the buffers, so it no longer
  raises when a buffered arc would be refused. `str()` is still `to_string()`.
- **Python `ResourceGraph()` takes no arguments.** `ResourceGraph(nx_graph)` imported the graph
  before any resource could be registered, so it always raised; the unused `**kwargs` go too.
  Register the resources, then call `from_networkx(G)`.
- **Removed from Python: `ResourceGraph.get_resource_factory()`, `Node.resource` and
  `Arc.extender`.** Their types are not bound, so they could only raise `TypeError`.
- **Python names each resource type once.** The bitset type is `"bitset"` everywhere, as
  `add_bitset_resource` already said: the function classes are `…_bitset`, and
  `create("bitset")` works. It used to be `"uint_bitset"` in some places, which no longer
  works. The types no bound graph uses (`uint`, `uint_set`, `size_t_set`, `size_t_bitset`) are
  no longer exported, nor are their function classes, which could not be used. Neither are the
  three graph classes whose first type is `int`, which a `ResourceGraph` could never pick: its
  first resource is real.

### Added

- **A search direction**: `AlgorithmBaseParams::direction` (`SearchDirection::Forward` by
  default, `Backward`, `Bidirectional`), read through `Algorithm::direction()`.
  `SimpleDominanceAlgorithm` searches backward, from the sinks; `create_algorithm` and `solve`
  build the class that does it.
  In Python, `solve(direction="forward" | "backward" | "bidirectional")`; an unsupported
  direction raises `ValueError`.
  An algorithm asked for a direction it does not search (`Algorithm::supported_directions()`)
  throws `std::invalid_argument` when it is created or solved, as does a backward search with a
  label container that declares no `with_direction<TargetDirection>()` (`LabelList` and
  `LabelBuckets` both do).
  A backward search returns complete paths in forward order. See "Search directions" in
  `docs/advanced/algorithms.md`.
- **Bidirectional labelling**: `SimpleDominanceAlgorithm` with
  `direction = SearchDirection::Bidirectional`, the clock's type being the last template parameter
  of `solve` and `create_algorithm`, after the cost's, and defaulting to it.
  In Python, `algorithm="bidirectional"`, the same as `simple` with `direction="bidirectional"`.
  Give it a `critical_resource_index` and a `half_way_point`: with the default `half_way_point = 0`
  the half-way bound is off, and the solve does more work than a forward one and logs a warning
  saying so. A bidirectional search runs one phase, and its two searches interleave: the next label
  comes from whichever has extended fewer arcs. After a timeout or the memory limit it still runs
  its join, with the incumbent cutoff on, so it returns the improving paths the two halves hold;
  `join_after_early_stop = false` skips it, and so does an interrupt. With
  `prune_by_upper_bound`, it prunes on lower bounds of the cost still to come, computed from
  the cost function itself, so they hold whichever components the objective reads;
  `heuristic_cost_index` is not read. What it reports about its
  bound and its join is in `SolveResult::bidirectional`, a `BidirectionalStats`, empty from every
  other algorithm. See "`Bidirectional`" in `docs/advanced/algorithms.md`.

- **Model checks for a backward and a bidirectional search** (`cpp/rcspp/validation/`):
  `BackwardExtensionCheck` and `JoinCheck`, which find a model whose backward semantics are
  missing or incoherent before any label exists. They read the whole model, removed arcs included.
  `ResourceGraph::check_model(direction)` runs the checks a search in that direction needs without
  solving and returns every problem. A `PreSolvePlan` built from a direction holds the checks that
  direction needs; its `enforce()` refuses a failing model with `ModelRefused` (a
  `std::runtime_error`) listing them.
  In Python, `check_model(direction="bidirectional")` returns the problems, and a refused solve
  raises `RuntimeError`.
  A check and a `Preprocessor` are both a `PreSolveStage`: a check only inspects the graph, a
  preprocessor runs on it and is undone after the search. See "Model checks" in
  `docs/advanced/algorithms.md`.
- **`CapacityExtensionFunction`** (C++ and Python, signed types only): additive forward and a
  threshold backward, which is what a bounded accumulation such as a capacity needs in a backward
  or bidirectional solve. It takes its capacity, `CapacityExtensionFunction(capacity)`, or in C++
  the per-node caps as a `NodeBounds` shared with its feasibility function.
- **`NodeBounds`** (C++, `rcspp/resource/functions/node_bounds.hpp`): per-node `[lower, upper]`
  bounds that a threshold extension and its feasibility function share, so the two cannot disagree.
  Build one with `make_node_bounds`. `MinMaxFeasibilityFunction`, `TimeWindowFeasibilityFunction`
  and `TimeWindowExtensionFunction` gain a constructor that takes one and a `bounds()` accessor;
  their other constructors are unchanged. The model checks refuse a threshold extension whose
  backward clamp at a node, or whose backward start at a sink, is not the largest value the
  feasibility function admits there, or that does not clamp or start at all. The clamps are
  observed, not declared: the checks run the extension on values beyond every bound, so a
  threshold extension of your own is checked however it is written, and must accept any value of
  its type. A feasibility function's floor is checked the same way, by asking its backward test.
- **Backward-semantics declarations** for custom functions, each with a default that keeps existing
  code compiling: `ExtensionFunction::directional_resource_kind()` and `start_back()`; on
  `FeasibilityFunction` `join_rule()` and `requires_nondecreasing()`; and
  `CostFunction::cost_kind()`. A composition `DominanceFunction` gains a `check_back_dominance()`
  whose default refuses, and a composition `CostFunction` an `is_additive()` whose default refuses:
  a backward label's cost is its suffix's, and the join adds the two halves' costs, so a cost that
  reads a threshold's value is refused. An accumulation whose halves the join adds, through its cost
  or its join test, must be a sum, which the checks verify by running the extension. The model
  checks refuse a model whose declarations are missing or incoherent, naming the component. A
  backward or bidirectional search is also refused a constraint a backward label cannot check
  exactly, a test of what the path has collected up to a node such as per-node caps on
  `SizeFeasibilityFunction` or per-node windows on an accumulating `MinMaxFeasibilityFunction`; and
  a backward search a source whose window opens after the value a forward label starts with. See
  "Model checks" in `docs/advanced/algorithms.md`.
- **`SolveResult.bidirectional`** (C++ and Python): a bidirectional solve's diagnostics,
  `bounded_by_half_way`, `half_way_off_reason` and `number_of_joined_paths`, as a
  `BidirectionalStats`; empty (`None` in Python) from every other algorithm.
- **ng-path in bidirectional solves.** `NgPathExtensionFunction` joins exactly with an
  `IntersectionFeasibilityFunction` that forbids `{v}` at each node; a visited set built with
  `UnionExtensionFunction` is refused by the model checks, naming the fix.
- **Presets**, one call per resource kind, each building its four function objects so that they
  agree (`rcspp/resource/presets.hpp`): `add_cost_resource`, `add_window_resource`,
  `add_capacity_resource`, `add_ng_path_resource` and `add_elementary_resource`, also in Python, in
  `rcspp.presets`. Only the cost preset prices its resource; a window or a capacity costs nothing.
  Each asserts at compile time that its extension declares a directional resource kind; C++ callers
  can assert the same of their own extensions with
  `rcspp::declares_directional_resource_kind_v<ExtensionFunctionType>`. The rest of a pairing is
  checked by the model checks.
- **`SolveResult.memory_pressure_triggered`** (C++ and Python), and
  `Algorithm::memory_pressure_was_triggered()`: whether memory pressure trimmed the solve, which a
  `complete` status does not rule out.
- **`Algorithm::get_number_of_extended_labels()`**: the labels the last solve extended, a
  machine-independent measure of work.
- **The C++ pool classes have a public Python home**: `SolutionPool`, `FilteredSolutionPool`,
  `ColumnActivity` and `PricedColumn` are importable from `rcspp.pricing_pool`, with
  `DEFAULT_PRICE_THRESHOLD`. The docs no longer import from the private `rcspp._core`.
- **`AlgorithmParams.prune_by_upper_bound` in Python.** The C++ field was not bound, so Python
  users could not turn pruning by the upper bound on.

### Fixed

- **A truncated run that uses up its phases reports `MaxPhases`, not `Complete`**, in C++ and in
  Python. With `release_after_solve`, the default, the labels a phase held back (truncated
  labelling, memory pressure) were not counted when the status was read, so such a run reported
  `Complete` although labels were never extended; with `release_after_solve = false` it already
  reported `MaxPhases`. The status no longer depends on that setting.
- **`Algorithm::is_optimal()` answers for the last solve**: whether it ended `Complete`. It used to
  read the label queues, which `release_after_solve` empties when the solve ends, so it answered
  `true` after a timeout or any other early stop. It answers `false` before the first solve.
- `ResourceCompositionFactory::clone()` copies the per-type sub-factories too. The inherited
  `ResourceFactory::clone()` it used to fall back on copied only the composed prototype.
- **A node or arc added to a cloned `ResourceGraph` is priced as in the original**, in C++ and in
  Python. The clone's factory was rebuilt with the default composition functions, whose cost sums
  every resource's, while a default graph's cost is its first resource's alone; a graph built
  with custom composition functions lost them too. Nodes and arcs present when the graph was
  cloned were not affected, so a clone that is only solved, or whose arcs are only removed and
  restored (as `DiversificationSearch` does), gave the same results.
- `ResourceGraph::solve` restores the arcs its preprocessing removed even when the solve throws.
  Before, any exception out of a solve (a user function's error, say) left those arcs deleted from
  the caller's graph, so a fallback solve priced on a smaller graph.
- Memory pressure no longer loosens a per-node label quota the caller set tighter than
  `memory_pressure_max_labels_per_node`, in any labelling algorithm. It used to replace the quota
  outright, so under pressure a quota of 5 became 200.
- The Python bindings hold the GIL while they release the array `_add_rows_bulk` returns.
- The Windows wheel build no longer pins the "Visual Studio 17 2022" generator, which the current
  GitHub runner image does not have.
