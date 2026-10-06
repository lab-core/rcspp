---
title: Algorithms
---

# Algorithms

## Choosing an algorithm

| Algorithm | Optimal | Speed | When to use |
|---|---|---|---|
| `Simple` | ✓ | medium | Default; correct answer needed |
| `Pushing` | ✓ | medium–fast | Dense graphs; pushing dominance forward |
| `Pulling` | ✓ | medium–fast | Bi-directional; long paths |
| `AStar` | ✓ | fast–slow | Good heuristic available |
| `Greedy` | ✗ | fast | Quick feasible solution; large graphs |
| `Tabu` | ✗ | medium | Metaheuristic diversity |
| `Diversification` | ✗ | configurable | Generate a diverse pool of solutions |

## What every algorithm returns

A path starts at a **source** and ends at a **sink**, with **neither a source nor a sink strictly
inside it**. The graph refuses an arc that no path could use: `add_arc` throws
`std::invalid_argument` for an arc into a source or out of a sink (in Python, a `ValueError` when the
buffered arcs are sent to the graph). The algorithms keep the rule as well: none continues past a
sink it reaches, and none extends a path into a source. A walk that returns to the depot and leaves
again is two routes; priced as one column, with the vehicle-count row counted once, a
set-partitioning master would buy it and be wrong.

:::{admonition} Changed
:class: note

The rule used to hold only in part. Every algorithm could pass through a *source*, and `pulling`
and `greedy` -- with the tabu and diversification searches built on the same dive -- could also
continue past a sink. With sources `0` and `1`, sink `2`, and arcs `0 → 1` (−5), `1 → 2` (−1),
`0 → 2` (0), the optimum was −6 through `0 1 2`. That graph is now refused when it is built, at
`0 → 1`; without that arc the optimum is −1 through `1 2`. A route that genuinely continues through
a depot is two routes: give the depot separate source and sink copies, which is what the VRP example
does.
:::

### One thing `status` cannot tell you

Every labelling algorithm prunes its label queues when RSS crosses
`memory_pressure_fraction ×` the effective limit, and tightens the per-node extension quota along
with it (never loosening one you set tighter).  Labels that were never extended are abandoned, so
the answer may no longer be optimal — but `status` still reads `complete`, because the label sets
really were exhausted *of what survived the trim*.  There is no status value for "exhausted but
lossy", and reusing `memory_limit` would conflate a hard stop with a soft trim, so it is reported as
a flag:

```python
result = rg.solve(algorithm="simple", params=p)
if result.memory_pressure_triggered:
    print("memory pressure trimmed this solve; the answer is not a proof")
```

Code that treats `complete` as proof of optimality — a column-generation loop deciding it has
converged, say — has to read this too.  `could_be_non_optimal()` will not tell you: it reads the
*parameters*, and memory pressure is a property of the run.

## `Simple` (default)

Classic forward label-setting.  Processes nodes in topological order and
propagates labels; dominated labels are discarded immediately.

```python
result = rg.solve()                        # algorithm="simple" by default
result = rg.solve(algorithm="simple")
```

## `Greedy`

Extends labels greedily (best-cost-first) with limited backtracking.
Much faster than the exact algorithms; no optimality guarantee.

```python
params = AlgorithmParams()
params.num_labels_to_extend_by_node = 5   # extend at most 5 labels per node

result = rg.solve(algorithm="greedy", params=params)
```

## `Tabu`

Tabu-arc metaheuristic.  At each iteration it removes the arcs used in the
current solution from the graph and re-solves, diversifying the search.
Arc removal is temporary (governed by `tabu_tenure`).

```python
params = AlgorithmParams()
params.tabu_tenure        = 5
params.tabu_random_noise  = True      # random ±1 tenure noise
params.max_iterations     = 100
params.forbidden_tabu     = {source_id, sink_id}  # never remove arcs from these

result = rg.solve(algorithm="tabu", params=params)
```

## `Diversification`

Wraps another algorithm (default: `Greedy`) and repeatedly solves the
subproblem after removing solution arcs, collecting a pool of diverse
solutions.

```python
params = AlgorithmParams()
params.stop_after_X_solutions = 20    # collect 20 distinct solutions
params.max_iterations         = 100
params.seed                   = 42

result = rg.solve(algorithm="greedy", params=params)
# In C++, use DiversificationSearch<Comp> explicitly to wrap any inner algorithm
```

## Model checks

A forward search reads nothing but the forward functions. A search that also extends labels
**backward**, from the sinks, reads each resource's backward semantics: its `backward_kind()`, its
backward extension, start and feasibility test. A **bidirectional** search also **joins** a forward
and a backward half into one path, so it reads each component's `join_rule()` as well. A model whose
backward semantics are missing or incoherent would let those searches return infeasible paths, or
none, while reporting `complete`. The model checks find that before any label exists:

| Check | Needed by | What it checks |
|---|---|---|
| `BackwardExtensionCheck` | backward and bidirectional searches | every component and the composition can extend, start, test and compare backward, and the backward values mean what the forward search enforces |
| `JoinCheck` | bidirectional searches | every component declares how two halves join, and the halves the join adds are sums |

`ResourceGraph::check_model(direction)` runs the checks a search in that direction needs, without
solving, and returns one line per problem (an empty `problems` list means the model is ready):

```cpp
const rcspp::ModelReport report = graph.check_model(rcspp::SearchDirection::Bidirectional);
for (const auto& problem : report.problems) {
    std::cerr << problem << '\n';
}
```

The checks read the whole model, arcs that preprocessing removed included, so a verdict never
depends on what one solve's bound happened to remove. They never change the graph. Two PRs later in
this stack, a backward or bidirectional solve runs them itself, before any preprocessing and whatever
`preprocess` says, and refuses a model that fails them with a `ModelRefused` (a
`std::runtime_error`, so Python sees a `RuntimeError`):

```text
a bidirectional search cannot run on this model:
  - component 1: arc 1 -> 2 consumes -3.000000, but its backward reading assumes no arc
    lowers the value: a backward label carries only a ceiling, so a path dipping below the
    floor would be accepted; loads must be non-negative
```

There are nineteen complaints, in two groups. The last column says which searches each one refuses:
a backward search is refused for thirteen of them, a bidirectional search for all nineteen.

**Something was not declared, or was declared for the wrong kind of resource:**

| Complaint | What to do | Refuses |
|---|---|---|
| `no node carries a resource` | The checks read the declarations off a node's resource; give the nodes their resources. | both |
| `its extension function declares no backward_kind()` | Derive the extension function from `BackwardForm` or a form built on it (`resource/functions/extension/backward_form.hpp` explains them), or declare `static constexpr BackwardKind kind`. | both |
| `its feasibility function declares no join_rule()` | Return a `JoinRule` from `join_rule()` — `AlwaysTrue` if the resource genuinely cannot block a join, and say why. | bidirectional |
| `its feasibility function declares JoinRule::ValueOrder … on a resource that has none` | `ValueOrder` is `forward <= backward` on two scalar values.  A container resource has no such order: declare `JoinRule::Custom` and write the test. | bidirectional |
| `the model's composition dominance function has no backward form` | A composition dominance function you passed to `ResourceGraph`'s constructor overrides `check_dominance` only, which is all a forward solve needs.  Override `check_back_dominance` too, or keep the default `CompositionDominanceFunction`. | both |
| `the model's composition extension function has no backward form`, `… has no backward start`, `… feasibility function has no backward form` | The same for a composition extension or feasibility function: one that overrides only `extend` or `is_feasible` compiles and solves forward, but has no `extend_back`, `start_back` or `is_back_feasible`.  Override those too, or keep the defaults `CompositionExtensionFunction` and `CompositionFeasibilityFunction`. | both |
| `the model's composition feasibility function has no join test` | The same for `can_be_joined`.  A `can_be_joined` of your own must be exact, like a component's: no check can tell one that refuses too much. | bidirectional |
| `the model's cost function is not additive` | A backward label that reaches a source is a complete path, and the join prices a path as the forward half's cost plus the backward half's.  That is exact only when every component the cost reads has a zero cost (`TrivialCostFunction`) or a value cost (`ValueCostFunction`) under an accumulating extension.  A threshold's backward value is a deadline or a remaining capacity, not a cost, so a `CompositionCostFunction` over a time window with a `ValueCostFunction` is refused.  Give that component a `TrivialCostFunction`, or keep the default cost function, which reads component 0 only. | both |

The `join_rule()` one has **two causes, and only one of them is yours.** A function you wrote that
never answered is the first. The second is a library function that answers `Unspecified` on
purpose, because the configuration you gave it has no backward reading: its test is a question
about what a label has collected *so far* — a predicate on a **prefix** — and a backward label
carries a **suffix**. Each of these would fail silently, returning an infeasible path or nothing at
all with a `complete` status, so they are refused instead:

| Component | When it refuses | What to do instead |
|---|---|---|
| `IntersectionFeasibilityFunction` | whenever any node's set is non-empty, forbidden or required | Solve that model with a forward algorithm.  No container extension at this point gives these sets a backward reading — even `forbidden(v) = {v}`, the elementary / ng-route condition, needs a memory that excludes the node it sits on in both directions, which the arc-value containers (`UnionExtensionFunction` and friends) cannot provide. |
| `SizeFeasibilityFunction` | a non-zero **minimum** size anywhere, or a **per-node cap** that differs from the default | Use one cap for every node, or solve forward.  A single cap is suffix-safe: a backward label that reaches a source holds the whole set, and every set along the path is a subset of it.  A floor or a per-node cap bounds what the path has collected *up to* a node, which a suffix cannot see. |
| `MinMaxFeasibilityFunction` | a non-zero **minimum** anywhere, default or per node, under a threshold extension (`CapacityExtensionFunction`, `TimeWindowExtensionFunction`) | The same fix, for the same reason: a backward label carries only a *ceiling*, so neither the join nor a backward label that reaches a source can tell whether the forward value got as high as the floor. |
| `MinMaxFeasibilityFunction` | under an accumulating extension (`AdditionExtensionFunction`), any window other than a single `[0, max]` | Use `CapacityExtensionFunction`.  A per-node window is the same prefix question as a per-node size cap, and a non-zero minimum rejects the empty suffix a backward label starts from; either way the sum of the two halves cannot be tested exactly. |
| `ReachableFeasibilityFunction` | always | Its `is_reachable` asks what the label has already collected, which a suffix cannot answer. |

In every one of these, forward-only solves are unaffected.

**Or two declarations were each legal on their own and incoherent together.** All nine of these
are a *pairing* fault: nothing is wrong with either half in isolation, which is why only a check
that sees both catches them.

The clamps a threshold extension applies are not declared: the checks observe them, by running the
extension on values beyond every bound (the infinities, or an integral type's extremes) through an
arc leaving and an arc entering each node, so a threshold of your own is checked however it is
written, and must accept any value of its type.  The same goes for where a backward label starts at
a sink: the checks call `start_back` through an arc entering the sink, as a backward search seeds
it.  Each clamp and start is then put to the node's forward test, which must admit it and reject a
value just above it: a backward label starts at, and is clamped to, the largest value the forward
test admits.  So that test must handle any value, and be exact at its ceiling.  A feasibility
function's floor is not declared either: the checks ask the node's backward test directly, at the
lowest value a forward label can hold there (the forward clamp, or 0), so that test must handle any
value too.  An arc that preprocessing removed still counts, here and in the negative-load scan
below.  A node no arc leaves is never the arrival of a backward step, so its backward clamp is not
checked, though a sink's start is; likewise the forward clamp at a node no arc enters.

| Complaint | What it means | Refuses |
|---|---|---|
| `its extension accumulates but its feasibility function declares JoinRule::ValueOrder` | `ValueOrder` compares the forward value against the backward one, which reads the backward value as a *bound*.  Under an accumulation it is not a bound, it is the suffix's own consumption, so the comparison tests nothing the model contains — and it fails by *accepting*.  Use a threshold extension, or declare `JoinRule::Custom` with a body that adds the two halves. | bidirectional |
| `its feasibility function rejects a backward value of … at node …, the lowest value a forward label can hold there` | The feasibility function's backward test has a floor, a node's opening time say, above that value: the extension never waits for it going forward, and the forward test does not check it either.  The backward search would then reject deadlines a forward path meets.  Build the extension and the feasibility function from the same windows (`TimeWindowExtensionFunction` with `TimeWindowFeasibilityFunction`), or pair `CapacityExtensionFunction` with `MinMaxFeasibilityFunction(0, capacity)`. | both |
| `its backward labels at node … are not clamped at all, but its feasibility function rejects values above some bound there` | The extension's backward step does not clamp, so a deadline can stay above the node's upper bound and the backward search admits deadlines the forward search rejects — under a time window's backward test, which reads only the opening time, the join or a backward label reaching a source would accept an infeasible path.  Clamp each backward label to its node's upper bound, as `ThresholdForm` does, built from the feasibility function's `NodeBounds`. | both |
| `its backward labels at sink … start at the type default, but its feasibility function rejects values above some bound there` | The extension's `start_back` sets nothing, so a backward label at the sink keeps the type default, 0, as its deadline, and the backward search rejects deadlines a forward path meets.  Start each backward label at the sink's upper bound, as `ThresholdForm::start_back` does. | both |
| `its backward labels at node … are clamped to …` (or `at sink … start at …`) `, which its feasibility function rejects there` | A threshold extension clamps each backward label to its own upper bound at the node.  If the node's `is_back_feasible` rejects that value, every backward label there is lost: the extension was built with other caps than the feasibility function, typically `CapacityExtensionFunction(capacity)` beside per-node caps on the feasibility function.  Build both from one `NodeBounds` (`make_node_bounds(0, capacity, per_node)`, or `feasibility->bounds()`). | both |
| `its backward labels at node … are clamped to …` (or `at sink … start at …`) `, above` (or `below`) `the largest value its feasibility function admits there` | The same mismatch where the node's backward test cannot see it.  The node's ceiling is the largest value its forward test admits.  A clamp or start *above* it lets the backward search admit deadlines the forward search rejects — a time window's backward test reads only the opening time, so the join would accept infeasible paths; one *below* it rejects deadlines a forward path meets.  The fix is the one above.  A forward test of your own that admits values a little above its ceiling, beyond the library's own `epsilon` tolerance, reads as a ceiling higher than the clamp and is refused: make it exact. | both |
| `arc … consumes …, but its backward reading assumes no arc lowers the value` | A backward label carries only a ceiling, so it cannot see a path's value dip below a floor inside the suffix: with a negative load, a `MinMaxFeasibilityFunction(0, capacity)` under `CapacityExtensionFunction` or `AdditionExtensionFunction` accepts a path whose load went below 0.  Loads must be non-negative.  A time window is exempt, since it waits at each node's opening time. | both |
| `its extension declares BackwardKind::Accumulate, but along … it is not a sum` | The join adds the component's two halves, through its cost (`ValueCostFunction`) or its feasibility function's join test (`MinMaxFeasibilityFunction` under an accumulation tests `forward + backward`), which is exact only if a path's value is the sum of one amount per arc.  `Accumulate` promises less: that the step is the same in both directions.  A bottleneck, the largest load seen, is such a step, but its halves 3 and 5 add up to 8 where the path's value is 5.  The check runs the extension: from the type default, two steps along the model's arcs must give the sum of the two single steps.  Write the step as an addition, or keep the component's halves apart: a `TrivialCostFunction`, and a feasibility function whose join test does not add. | bidirectional |
| `its extension declares BackwardKind::Accumulate, but starts its backward labels at sink …` | A forward label starts at the type default, so a backward label whose halves are added must start there too: any other start is counted on top of the path's own sum.  Leave `start_back` to its default. | bidirectional |

(The messages are generated by `BackwardExtensionCheck` and `JoinCheck`, in
`cpp/rcspp/validation/`; if you change a message there, change these tables too.)

A **capacity written as an addition**, `AdditionExtensionFunction` with
`MinMaxFeasibilityFunction(0, capacity)`, is not one of them: `MinMaxFeasibilityFunction` picks its
join test from the extension it is paired with.  Under an addition a backward label starts at the
empty suffix and counts its load *up* from zero, and the join tests `prefix + suffix <= capacity`.
That is exact while loads are non-negative, which the checks verify.  `CapacityExtensionFunction`
is the other way to write it: additive forward, a threshold backward.

One pairing needs no refusal: a time window's extension with a feasibility function whose floor is
lower, such as `MinMaxFeasibilityFunction(0, cap)`.  A threshold extension marks a backward deadline
below the node's opening time as unmeetable itself, whatever the feasibility function tests.

A threshold extension clamps its backward label to each node's upper bound **as its own bounds state
it**, so those must be the feasibility function's.  Give both functions one `NodeBounds`:

```cpp
auto caps = rcspp::make_node_bounds(0.0, capacity, per_node_caps);  // {node: {0, cap}}
graph.add_resource<RealResource>(
    std::make_unique<CapacityExtensionFunction<RealResource>>(caps),
    std::make_unique<MinMaxFeasibilityFunction<RealResource>>(caps),
    std::make_unique<TrivialCostFunction<RealResource>>(),
    std::make_unique<ValueDominanceFunction<RealResource>>());
```

`TimeWindowExtensionFunction` and `TimeWindowFeasibilityFunction` take a `NodeBounds` of windows the
same way, and every feasibility function that has one returns it from `bounds()`.  The checks refuse
a clamp that differs from the node's bound, in either direction.

## Bucket label containers

For graphs with many labels per node, `LabelBuckets` speeds up dominance
checking by partitioning labels on one resource dimension.

```python
from rcspp.graph import BucketAlgorithmParams

bp = BucketAlgorithmParams()
bp.range_buckets         = 200    # partition the time dimension into 200 buckets
bp.bucket_resource_index = 0      # resource 0 is time
bp.sort_resource_index   = 1      # sort within bucket by resource 1

result = rg.solve(algorithm="simple", params=bp)
```

In C++:

```cpp
BucketAlgorithmParams<LabelBuckets<Comp>> bp;
bp.range_buckets = 200;
bp.bucket_resource_index = 0;
bp.sort_resource_index = 1;
auto result = graph.solve<SimpleDominanceAlgorithm, RealResource, LabelBuckets>(ub, bp);
```
