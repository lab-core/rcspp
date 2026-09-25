# Changelog

Notable changes to rcspp. There has been no release yet, so everything below is **Unreleased**.
Each entry says what an existing model or caller sees differently; new features are summarised,
with a pointer to where they are documented.

## Unreleased

### Behaviour changes

- **A bidirectional solve stopped by a timeout or the memory limit still runs its join.** Before,
  it skipped the join, so with the half-way bound in force it returned almost nothing, and a
  column-generation loop with a pricing budget stalled. The join now runs with the incumbent cutoff
  forced on and returns the improving paths the two halves already hold; the status still reports
  `TIMEOUT` or `MEMORY_LIMIT`. An interrupt (`should_stop`) still skips it. Set
  `join_after_early_stop = false` for the previous behaviour. See "Parameters that behave
  differently here" in `docs/advanced/algorithms.md`.

- **No path passes through a source or continues past a sink, in any algorithm.** A path starts
  at a source and ends at a sink, with neither strictly inside it. Before, every algorithm could
  extend a label into a source, and `pulling` and `greedy` (with diversification) could extend one
  out of a sink. On a graph with several sources, an arc into a source or an arc out of a sink,
  answers can change: in the example in the docs, -6 via `0 1 2` becomes -1 via `1 2`. See "What
  every algorithm returns" in `docs/advanced/algorithms.md`.
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
- **`MinMaxFeasibilityFunction`'s third constructor argument, `merge_by_increasing_value`, is
  ignored.** It only ever mattered to a bidirectional solve, and its backward seed and merge test
  now follow the extension it is paired with. The argument is kept so existing calls compile.

### Added

- **Growing ng neighbourhoods on a built graph** (C++). `set_ng_neighborhoods<R>(graph, component,
  table)` replaces an ng-path resource's neighbourhoods between solves and rebuilds every arc's ng
  component, since arcs cache what they read; `ng_neighborhoods<R>` reads them back.
  `find_cycles` and `is_ng_feasible` let a column-generation driver grow the table from the LP
  solution and remove the master columns it makes infeasible. See "Growing ng neighbourhoods
  between solves" in `docs/advanced/algorithms.md`.

- **Budgets on the bidirectional join** (C++ and Python). `join_column_budget` keeps only the
  join's cheapest K paths without stopping the search, so the optimum and a `complete` status are
  unchanged. `max_join_pairs` caps the join's work in merge-rule questions. `SolveResult` reports
  `join_pairs_tested` and `join_truncated`, and the half-way controller does not learn from a
  truncated join.

- **Bidirectional labelling**: `BidirectionalDominanceAlgorithm` in C++, `algorithm="bidirectional"`
  in Python. Give it a `critical_resource_index` and a `half_way_point`: with the default
  `half_way_point = 0` the half-way bound is off, and the solve does more work than a forward one
  and logs a warning saying so. See "`Bidirectional`" in `docs/advanced/algorithms.md`.
- **`BudgetExtensionFunction`** (C++ and Python, signed types only): additive forward and a
  threshold backward, which is what a bounded accumulation such as a capacity needs in a
  bidirectional solve. It takes its capacity, `BudgetExtensionFunction(capacity)`, or in C++ the
  per-node caps as a `NodeBounds` shared with its feasibility function.
- **`NodeBounds`** (C++, `rcspp/resource/functions/node_bounds.hpp`): per-node `[lower, upper]`
  bounds that a threshold extension and its feasibility function share, so the two cannot disagree.
  Build one with `make_node_bounds`. `MinMaxFeasibilityFunction`, `TimeWindowFeasibilityFunction`
  and `TimeWindowExtensionFunction` gain a constructor that takes one and a `bounds()` accessor;
  their other constructors are unchanged. A bidirectional solve refuses a threshold extension whose
  backward clamp at a node differs from the feasibility function's bound there.
- **Backward-semantics declarations** for custom functions, each with a default that keeps existing
  code compiling: `ExtensionFunction::backward_kind()`, `floor_at()` and `back_ceiling_at()`; on
  `FeasibilityFunction` `merge_rule()`, `back_seed_value()`, `back_floor_at()` and
  `requires_nondecreasing()`; and `CostFunction::cost_form()`. A composition `DominanceFunction`
  gains a `check_back_dominance()` whose default refuses, and a composition `CostFunction` an
  `adds_across_join()` whose default refuses: the join adds the two halves' costs, so a cost that
  reads a threshold's value is refused. A bidirectional solve refuses a model whose declarations are
  missing or incoherent before the first label, and names the component. It also refuses constraints
  a backward label cannot check exactly, such as per-node caps on `SizeFeasibilityFunction`; see the
  refusal table in `docs/advanced/algorithms.md`.
- **`SolveResult` diagnostics**: `bounded_by_half_way`, `number_of_joined_paths` and
  `memory_pressure_triggered`.
- **ng-path in bidirectional solves.** `NgPathExtensionFunction` joins exactly with an
  `IntersectionFeasibilityFunction` that forbids `{v}` at each node; a visited set built with
  `UnionExtensionFunction` is refused, naming the fix.
- **Presets**, one call per resource kind, each a coherent quadruple (`rcspp/resource/presets.hpp`):
  `add_cost_resource`, `add_window_resource`, `add_budget_resource`, `add_ng_path_resource` and
  `add_elementary_resource`. Python has the first three, in `rcspp.presets`. C++ callers can assert
  the same coherence on their own pairings with `rcspp::backward_coherent_v<Ext, Feas>`.

### Fixed

- **A solution budget of zero no longer reads an empty heap in the bidirectional join.** With
  `stop_after_X_solutions = 0` the join read the top of an empty heap (undefined behaviour; a
  checked-iterator build aborted). It now joins nothing.

- Memory pressure no longer loosens a per-node label quota the caller set tighter than
  `memory_pressure_max_labels_per_node`, in any labelling algorithm. It used to replace the quota
  outright, so under pressure a quota of 5 became 200.
- `ResourceGraph::solve` restores the arcs its preprocessing removed even when the solve throws.
  Before, any exception out of a solve (a bidirectional refusal, a user function's error) left
  those arcs deleted from the caller's graph, so a fallback solve priced on a smaller graph.
- The Python bindings hold the GIL while they release the array `_add_rows_bulk` returns.
- The Windows wheel build no longer pins the "Visual Studio 17 2022" generator, which the current
  GitHub runner image does not have.
