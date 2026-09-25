# Changelog

Notable changes to rcspp. There has been no release yet, so everything below is **Unreleased**.
Each entry says what an existing model or caller sees differently.
New features are summarised, with a pointer to where they are documented.

## Unreleased

### Behaviour changes

- **The VRP example no longer takes a `complete` pricing solve as proof when its join was truncated
  or its labels were trimmed.** `VRP::solve` read only the status, so a solve whose join stopped
  at `max_join_pairs`, or that memory pressure trimmed, ended column generation "proven" and could
  call the convergence hook on an LP that was not the optimum. It now applies
  `proves_no_column(result)`: `complete`, no memory trim, no truncated join. The warning for an
  unproven stop now says the LP value is an upper bound on the optimum, not "a valid bound".
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
  `std::unique_ptr<Algorithm<R, LC>>`**, not `std::unique_ptr<Strategy<R, LC>>`: the class that
  searches in `params.direction` is not always `Strategy` itself. Code that stored the result in a
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

### Added

- **A search direction**: `AlgorithmBaseParams::direction` (`SearchDirection::Forward` by
  default, `Backward`, `Bidirectional`), read through `Algorithm::direction()`.
  `SimpleDominanceAlgorithm` searches backward, from the sinks; `create_algorithm` and `solve`
  build the class that does it.
  In Python, `solve(direction="forward" | "backward" | "bidirectional")`; an unsupported
  direction raises `ValueError`.
  An algorithm asked for a direction it does not search (`Algorithm::supported_directions()`)
  throws `std::invalid_argument` when it is created or solved, as does a backward search with a
  label container that declares no `with_direction<D>()` (`LabelList` and `LabelBuckets` both do).
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
  `prune_based_on_upper_bound_`, it prunes on lower bounds of the cost still to come, computed from
  the cost function itself, so they hold whichever components the objective reads;
  `heuristic_cost_index` is not read. What it reports about its
  bound and its join is in `SolveResult::bidirectional`, a `BidirectionalStats`, empty from every
  other algorithm. See "`Bidirectional`" in `docs/advanced/algorithms.md`.
- **A pricing budget that escalates** (VRP example, C++). `VRP::set_pricing_budget(PricingBudget)`
  times every pricing solve; one that times out with no improving column is re-run on the same
  duals with a larger budget, so column generation keeps the speed of a short timeout and still
  ends proven. `CGSolveResult` reports `escalations`, `truncated_solves` and
  `final_budget_seconds`. `rcspp-bidirectional-cg` and `rcspp-ng-dynamic-cg` price on a 0.05 s
  budget by default (`--pricing-budget=S`, `--budget-reset`; `--pricing-timeout` is its cap, and
  `--pricing-budget=inf` turns it off). See "A pricing budget that still ends proven" in
  `docs/advanced/algorithms.md`.
- **`Algorithm::set_timeout`** (C++): overrides `timeout_s` on an existing algorithm object between
  solves, so a persistent pricer, and the half-way controller on it, can be re-run with another
  budget. `timeout()` reports the one in force.
- **`derive_half_way_point`** (C++ and Python, default off): reads the clock's range from the
  sinks' ceilings; with `half_way_point = 0` the solve uses `H = R/2`, and a dynamic `H` is kept
  inside that range. `HalfWayController::over_range(h, range)` starts a controller with a range of
  its own. A bidirectional solve on a graph with no arcs turns the bound off as
  `HalfWayOff::NoArcs`, at debug level only.
- **`VRP(Instance, ng_table)`** (VRP example): builds the model from a given ng table, such as one
  dynamic growth learned. The ng driver gains `--dynamic-h`, `--join-budget` and
  `--max-join-pairs` for its bidirectional pricer; its `--reduce` round is a new baseline, its
  time brakes compare pricing time per solve rather than per round, and its comparison ignores
  unproven runs.

- **Growing ng neighbourhoods on a built graph** (C++). `set_ng_neighborhoods<R>(graph, component,
  table)` replaces an ng-path resource's neighbourhoods between solves and rebuilds every arc's ng
  component, since arcs cache what they read; `ng_neighborhoods<R>` reads them back.
  `find_cycles` and `is_ng_feasible` let a column-generation driver grow the table from the LP
  solution and remove the master columns it makes infeasible. See "Growing ng neighbourhoods
  between solves" in `docs/advanced/algorithms.md`.

- **Relaxing dominance for one solve** (C++). `dominance_ignored_components` names components that
  dominance leaves out, for that solve only, in every labeling algorithm and in both directions of
  a bidirectional one. Before, relaxing a resource's dominance meant a second graph built with a
  `TrivialDominanceFunction`; the parameter gives the same search on one graph. It is a heuristic,
  reported by `could_be_non_optimal()`. Naming the bidirectional clock turns the half-way bound
  off. See "Relaxing dominance for one solve" in `docs/advanced/algorithms.md`.

- **Budgets on the bidirectional join** (C++ and Python). `join_column_budget` keeps only the
  join's cheapest K paths without stopping the search, so the optimum and a `complete` status are
  unchanged. `max_join_pairs` caps the join's work in join-rule questions.
  `SolveResult::bidirectional` reports `join_pairs_tested` and `join_truncated`, and the half-way
  controller does not learn from a truncated join.

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
  code compiling: `ExtensionFunction::backward_kind()` and `start_back()`; on `FeasibilityFunction`
  `join_rule()` and `requires_nondecreasing()`; and `CostFunction::cost_form()`. A composition
  `DominanceFunction` gains a `check_back_dominance()` whose default refuses, and a composition
  `CostFunction` an `is_additive()` whose default refuses: a backward label's cost is its
  suffix's, and the join adds the two halves' costs, so a cost that reads a threshold's value is
  refused. An accumulation whose halves the join adds, through its cost or its join test, must be
  a sum, which the checks verify by running the extension. The model checks refuse a model whose
  declarations are missing or incoherent, naming the component. A backward or bidirectional
  search is also refused a constraint a backward label cannot check exactly, a test of what the
  path has collected up to a node such as per-node caps on `SizeFeasibilityFunction` or per-node
  windows on an accumulating `MinMaxFeasibilityFunction`; and a backward search a source whose
  window opens after the value a forward label starts with. See "Model checks" in
  `docs/advanced/algorithms.md`.
- **`SolveResult.bidirectional`** (C++ and Python): a bidirectional solve's diagnostics,
  `bounded_by_half_way`, `half_way_off_reason` and `number_of_joined_paths`, as a
  `BidirectionalStats`; empty (`None` in Python) from every other algorithm.
- **ng-path in bidirectional solves.** `NgPathExtensionFunction` joins exactly with an
  `IntersectionFeasibilityFunction` that forbids `{v}` at each node; a visited set built with
  `UnionExtensionFunction` is refused by the model checks, naming the fix.
- **Presets**, one call per resource kind, each building its four function objects so that they
  agree (`rcspp/resource/presets.hpp`): `add_cost_resource`, `add_window_resource`,
  `add_capacity_resource`, `add_ng_path_resource` and `add_elementary_resource`, also in Python,
  in `rcspp.presets`. Only the cost preset prices its resource; a window or a
  capacity costs nothing. Each asserts at compile time that its extension declares a
  backward kind; C++ callers can assert the same of their own extensions with
  `rcspp::declares_backward_kind_v<Ext>`. The rest of a pairing is checked by the model checks.
- **`SolveResult.memory_pressure_triggered`** (C++ and Python), and
  `Algorithm::memory_pressure_was_triggered()`: whether memory pressure trimmed the solve, which a
  `complete` status does not rule out.
- **`Algorithm::get_number_of_extended_labels()`**: the labels the last solve extended, a
  machine-independent measure of work.

### Fixed

- `ResourceGraph::solve` restores the arcs its preprocessing removed even when the solve throws.
  Before, any exception out of a solve (a user function's error, say) left those arcs deleted from
  the caller's graph, so a fallback solve priced on a smaller graph.
- Memory pressure no longer loosens a per-node label quota the caller set tighter than
  `memory_pressure_max_labels_per_node`, in any labelling algorithm. It used to replace the quota
  outright, so under pressure a quota of 5 became 200.
- The Python bindings hold the GIL while they release the array `_add_rows_bulk` returns.
- The Windows wheel build no longer pins the "Visual Studio 17 2022" generator, which the current
  GitHub runner image does not have.
