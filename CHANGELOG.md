# Changelog

Notable changes to rcspp. There has been no release yet, so everything below is **Unreleased**.
Each entry says what an existing model or caller sees differently; new features are summarised,
with a pointer to where they are documented.

## Unreleased

### Behaviour changes

- **No path passes through a source or continues past a sink, in any algorithm.** A path starts
  at a source and ends at a sink, with neither strictly inside it. Before, every algorithm could
  extend a label into a source, and `pulling` and `greedy` (with diversification) could extend one
  out of a sink. On a graph with several sources, an arc into a source or an arc out of a sink,
  answers can change: in the example in the docs, -6 via `0 1 2` becomes -1 via `1 2`. See "What
  every algorithm returns" in `docs/advanced/algorithms.md`.

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

- **Bidirectional labelling**: `BidirectionalDominanceAlgorithm` in C++, `algorithm="bidirectional"`
  in Python. Give it a `critical_resource_index` and a `half_way_point`: with the default
  `half_way_point = 0` the half-way bound is off, and the solve does more work than a forward one
  and logs a warning saying so. See "`Bidirectional`" in `docs/advanced/algorithms.md`.
- **`CapacityExtensionFunction`** (C++ and Python, signed types only): additive forward and a
  threshold backward, which is what a bounded accumulation such as a capacity needs in a
  bidirectional solve.
- **Backward-semantics declarations** for custom functions, each with a default that keeps existing
  code compiling: `ExtensionFunction::backward_kind()` and `start_back()`; on `FeasibilityFunction`
  `join_rule()`, `ceiling_at()` and `requires_nondecreasing()`; and `CostFunction::cost_form()`. A
  composition `DominanceFunction` gains a `check_back_dominance()` whose default refuses, and a
  composition `CostFunction` an `is_additive()` whose default refuses: the join adds the two halves'
  costs, so a cost that reads a threshold's value is refused. An accumulation whose halves the join
  adds, through its cost or its join test, must be a sum, which setup checks by running the
  extension. A bidirectional solve refuses a model whose declarations are missing or incoherent
  before the first label, and names the component. It also refuses constraints a backward label
  cannot check exactly, such as per-node caps on `SizeFeasibilityFunction`; see the refusal table in
  `docs/advanced/algorithms.md`.
- **`SolveResult` diagnostics**: `bounded_by_half_way`, `number_of_joined_paths` and
  `memory_pressure_triggered`.

### Fixed

- Memory pressure no longer loosens a per-node label quota the caller set tighter than
  `memory_pressure_max_labels_per_node`, in any labelling algorithm. It used to replace the quota
  outright, so under pressure a quota of 5 became 200.
- `ResourceGraph::solve` restores the arcs its preprocessing removed even when the solve throws.
  Before, any exception out of a solve (a bidirectional refusal, a user function's error) left
  those arcs deleted from the caller's graph, so a fallback solve priced on a smaller graph.
- The Python bindings hold the GIL while they release the array `_add_rows_bulk` returns.
- The Windows wheel build no longer pins the "Visual Studio 17 2022" generator, which the current
  GitHub runner image does not have.
