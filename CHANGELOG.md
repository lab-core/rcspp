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

- **`MinMaxFeasibilityFunction`'s third constructor argument, `merge_by_increasing_value`, is
  ignored.** It only ever mattered to a bidirectional solve, and its backward seed and merge test
  now follow the extension it is paired with. The argument is kept so existing calls compile.

### Added

- **Bidirectional labelling**: `BidirectionalDominanceAlgorithm` in C++, `algorithm="bidirectional"`
  in Python. Give it a `critical_resource_index` and a `half_way_point`: with the default
  `half_way_point = 0` the half-way bound is off, and the solve does more work than a forward one
  and logs a warning saying so. See "`Bidirectional`" in `docs/advanced/algorithms.md`.
- **`BudgetExtensionFunction`** (C++ and Python, signed types only): additive forward and a
  threshold backward, which is what a bounded accumulation such as a capacity needs in a
  bidirectional solve.
- **Backward-semantics declarations** for custom functions, each with a default that keeps existing
  code compiling: `ExtensionFunction::backward_kind()`, `floor_at()` and `back_ceiling_at()`; on
  `FeasibilityFunction` `merge_rule()`, `back_seed_value()`, `ceiling_at()`, `back_floor_at()` and
  `requires_nondecreasing()`; and `CostFunction::cost_form()`. A composition `DominanceFunction`
  gains a `check_back_dominance()` whose default refuses, and a composition `CostFunction` an
  `adds_across_join()` whose default refuses: the join adds the two halves' costs, so a cost that
  reads a threshold's value is refused. A bidirectional solve refuses a model whose declarations are
  missing or incoherent before the first label, and names the component. It also refuses constraints
  a backward label cannot check exactly, such as per-node caps on `SizeFeasibilityFunction`; see the
  refusal table in `docs/advanced/algorithms.md`.
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
