# Changelog

Notable changes to rcspp. There has been no release yet, so everything below is **Unreleased**.
Each entry says what an existing model or caller sees differently; new features are summarised,
with a pointer to where they are documented.

## Unreleased

### Behaviour changes

- **No path passes through a source or continues past a sink, in any algorithm.** A path starts
  at a source and ends at a sink, with neither strictly inside it. Before, every algorithm could
  extend a label into a source, and `pulling` and `greedy` (with the tabu and diversification
  searches) could extend one out of a sink. On a graph with several sources, answers can change.
  See "What every algorithm returns" in `docs/advanced/algorithms.md`.
- **`add_arc` refuses an arc into a source or out of a sink**, which no path could use:
  `std::invalid_argument` in C++ (`Graph::add_arc`, and so `ResourceGraph::add_arc`), `ValueError`
  in Python when the buffered arcs are sent to the graph. A model with such an arc used to build,
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

- **A search direction**: `AlgorithmBaseParams::direction` in C++ (`SearchDirection::Forward` by
  default, `Backward`, `Bidirectional`), and `solve(direction="forward" | "backward" |
  "bidirectional")` in Python. `simple` searches in all three; the other algorithms search forward
  only, and refuse another direction (`std::invalid_argument` in C++, `ValueError` in Python), as
  does a backward search with `LabelBuckets`. A backward search returns complete paths in forward
  order. See "Search directions" in `docs/advanced/algorithms.md`.
- **Bidirectional labelling**: `algorithm="bidirectional"` in Python, the same as `simple` with
  `direction="bidirectional"`; in C++, `SimpleDominanceAlgorithm` with
  `direction = SearchDirection::Bidirectional`, the clock's type being the last template parameter
  of `solve` and `create_algorithm`, after the cost's, and defaulting to it. Give it a
  `critical_resource_index` and a `half_way_point`: with the default `half_way_point = 0` the
  half-way bound is off, and the solve does more work than a forward one and logs a warning saying
  so. A bidirectional search runs one phase. See "`Bidirectional`" in
  `docs/advanced/algorithms.md`.
- **Model checks for a backward and a bidirectional search** (`cpp/rcspp/validation/`):
  `BackwardExtensionCheck` and `JoinCheck`. A backward or bidirectional solve runs the checks its
  direction needs on the whole model, removed arcs included, before any preprocessing and before
  the first label, and refuses a model that fails them with `ModelRefused` (a
  `std::runtime_error`; `RuntimeError` in Python) listing every problem.
  `ResourceGraph::check_model(direction)` in C++, and `check_model(direction="bidirectional")` in
  Python, run them without solving and return the problems. `Preprocessor` is now the Reduce kind
  of `PreSolveStage`; a check is the Check kind. See "Model checks" in
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
  declarations are missing or incoherent, naming the component; a bidirectional search is also
  refused constraints a backward label cannot check exactly, such as per-node caps on
  `SizeFeasibilityFunction`. See "Model checks" in `docs/advanced/algorithms.md`.
- **`SolveResult` diagnostics of a bidirectional solve** (C++ and Python): `bounded_by_half_way`,
  `half_way_off_reason` and `number_of_joined_paths`.
- **ng-path in bidirectional solves.** `NgPathExtensionFunction` joins exactly with an
  `IntersectionFeasibilityFunction` that forbids `{v}` at each node; a visited set built with
  `UnionExtensionFunction` is refused by the model checks, naming the fix.
- **Presets**, one call per resource kind, each a coherent quadruple (`rcspp/resource/presets.hpp`):
  `add_cost_resource`, `add_window_resource`, `add_capacity_resource`, `add_ng_path_resource` and
  `add_elementary_resource`. Python has the first three, in `rcspp.presets`. C++ callers can assert
  the same coherence on their own pairings with `rcspp::backward_coherent_v<Ext, Feas>`.
- **`SolveResult.memory_pressure_triggered`** (C++ and Python), and
  `Algorithm::memory_pressure_was_triggered()`: whether memory pressure trimmed the solve, which a
  `complete` status does not rule out.
- **`Algorithm::get_number_of_extended_labels()`**: the labels the last solve extended, a
  machine-independent measure of work.

### Fixed

- Memory pressure no longer loosens a per-node label quota the caller set tighter than
  `memory_pressure_max_labels_per_node`, in any labelling algorithm. It used to replace the quota
  outright, so under pressure a quota of 5 became 200.
- `ResourceGraph::solve` restores the arcs its preprocessing removed even when the solve throws.
  Before, any exception out of a solve (a user function's error, say) left those arcs deleted from
  the caller's graph, so a fallback solve priced on a smaller graph.
- The Python bindings hold the GIL while they release the array `_add_rows_bulk` returns.
- The Windows wheel build no longer pins the "Visual Studio 17 2022" generator, which the current
  GitHub runner image does not have.
