# Changelog

Notable changes to rcspp. There has been no release yet, so everything below is **Unreleased**.
Each entry says what an existing model or caller sees differently.

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

### Added

- **A search direction**: `AlgorithmBaseParams::direction` (`SearchDirection::Forward` by
  default, `Backward`, `Bidirectional`), read through `Algorithm::direction()`.
  `SimpleDominanceAlgorithm` searches backward, from the sinks; `create_algorithm` and `solve`
  build the class that does it.
  An algorithm asked for a direction it does not search (`Algorithm::supported_directions()`)
  throws `std::invalid_argument` when it is created or solved, as does a backward search with
  `LabelBuckets`, which has no backward form. A backward search returns complete paths in
  forward order. See "Search directions" in `docs/advanced/algorithms.md`.
- **Bidirectional labelling**: `SimpleDominanceAlgorithm` with
  `direction = SearchDirection::Bidirectional`, the clock's type being the last template parameter
  of `solve` and `create_algorithm`, after the cost's, and defaulting to it.
  Give it a `critical_resource_index` and a `half_way_point`: with the default `half_way_point = 0`
  the half-way bound is off, and the solve does more work than a forward one and logs a warning
  saying so. A bidirectional search runs one phase, and its two searches interleave: the next label
  comes from whichever has extended fewer arcs. After a timeout or the memory limit it still runs
  its join, with the incumbent cutoff on, so it returns the improving paths the two halves hold;
  `join_after_early_stop = false` skips it, and so does an interrupt. See "`Bidirectional`" in
  `docs/advanced/algorithms.md`.

- **Model checks for a backward and a bidirectional search** (`cpp/rcspp/validation/`):
  `BackwardExtensionCheck` and `JoinCheck`, which find a model whose backward semantics are
  missing or incoherent before any label exists. They read the whole model, removed arcs included.
  `ResourceGraph::check_model(direction)` runs the checks a search in that direction needs without
  solving and returns every problem. A `PreSolvePlan` built from a direction holds the checks that
  direction needs; its `enforce()` refuses a failing model with `ModelRefused` (a
  `std::runtime_error`) listing them.
  `Preprocessor` is now the Reduce kind of `PreSolveStage`; a check is the Check kind. See "Model
  checks" in `docs/advanced/algorithms.md`.
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
