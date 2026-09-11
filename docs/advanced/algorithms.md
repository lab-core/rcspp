---
title: Algorithms
---

# Algorithms

## Choosing an algorithm

| Algorithm | Optimal | Speed | When to use |
|---|---|---|---|
| `Simple` | ✓ | medium | Default; correct answer needed |
| `Pushing` | ✓ | medium–fast | Dense graphs; pushing dominance forward |
| `Pulling` | ✓ | medium–fast | Dense graphs; pulling dominance from in-arcs |
| `Bidirectional` | ✓ | fast–slow | Long routes with a tight, monotone clock |
| `AStar` | ✓ | fast–slow | Good heuristic available |
| `Greedy` | ✗ | fast | Quick feasible solution; large graphs |
| `Tabu` | ✗ | medium | Metaheuristic diversity |
| `Diversification` | ✗ | configurable | Generate a diverse pool of solutions |

## `Simple` (default)

Classic forward label-setting.  Processes nodes in topological order and
propagates labels; dominated labels are discarded immediately.

```python
result = rg.solve()                        # algorithm="simple" by default
result = rg.solve(algorithm="simple")
```

## `Pulling`

Forward label-setting like `Simple`, but each node *gathers* labels through its
in-arcs instead of each label being *pushed* through out-arcs.  The labels still
travel source → sink; this is not a bidirectional search.

## `Bidirectional`

Searches forward from the sources and backward from the sinks, stopping each
direction half-way along one designated resource, and joins the surviving halves
across the arc where that resource crosses the middle.  Because dominance is
checked within each half rather than over whole paths, the number of labels grows
with half the path length instead of all of it.

```python
from rcspp.graph import AlgorithmParams

p = AlgorithmParams()
p.critical_resource_index = 1     # slot 1 is the clock (time), not the cost
p.half_way_point          = 500.0 # H; the resource range is taken as [0, 2H]

result = rg.solve(algorithm="bidirectional", params=p)
```

### The critical resource

One resource acts as the **clock** that says where "half-way" is.  It must be:

- **monotone** — never decreasing along an arc, so its value crosses `H` exactly
  once along any path;
- **bounded**, with a known finite range;
- **a threshold backwards** — its backward form must carry a *ceiling* on the
  forward scale (the latest arrival still admissible here), not the consumption
  from here to the sink.  `TimeWindowExtensionFunction` and
  `BudgetExtensionFunction` are thresholds; `AdditionExtensionFunction` is not.

Cost is never a valid clock: reduced costs go negative during column generation,
which breaks monotonicity.

If the chosen resource fails any of these, **the half-way bound is switched off**
and a `LOG_DEBUG` line says why.  The solve stays correct — both directions simply
run to completion and every pair is considered — it is only slower.  A *wrong*
bound would be far worse: it can discard a valid path in both directions and then
report `complete`.

`critical_resource_index` is an index within the **cost resource type's** slot, so
the clock must share that type (a real-valued time alongside a real-valued cost is
the usual case).  A model whose clock is an `int` resource while its cost is
`real` needs the C++ API.

### What the model must declare

Every resource in the model must declare how it behaves backwards.  A solve whose
model does not raises **before the first label is created**, naming each offending
component:

```text
BidirectionalDominanceAlgorithm cannot run on this model:
  - component 1: its extension accumulates but its feasibility function supplies a
    backward seed; use a Threshold extension (e.g. BudgetExtensionFunction) instead
```

The usual cause is a **capacity written as an addition**: `AdditionExtensionFunction`
with `MinMaxFeasibilityFunction(0, capacity)` counts *up* from zero while the
backward seed says "start at the bound and count *down*".  That is fine
forward-only and incoherent backwards, so it is refused rather than solved wrongly.
Use `BudgetExtensionFunction`, which subtracts:

```python
from rcspp.resource import BudgetExtensionFunction, MinMaxFeasibilityFunction

rg.add_real_resource(
    BudgetExtensionFunction(),                 # additive forward, threshold backward
    MinMaxFeasibilityFunction(0.0, capacity),
    TrivialCostFunction(),
    ValueDominanceFunction(),
)
```

`BudgetExtensionFunction` is available for the signed numerical types (`"real"`
and `"int"`) only — extending backwards subtracts, and on an unsigned type that
wraps to a huge positive value that reads as a very loose bound.

### When it pays, and when it does not

What decides this is **how many labels dominance has to sift at each node**, not how
long the horizon is and not how tight the windows are.  Dominance compares a new
label against every label held at its node, so halving the set at each node makes
each comparison cheaper as well as making fewer of them — which is why the
wall-clock gain, where there is one, is larger than the reduction in label
extensions.

Measured across all six Solomon families (see `analysis/bidirectional-results.md`
for the full tables).  On the largest instances the difference stops being a
speed-up and becomes a question of whether an answer arrives at all: on R202_50
and on the full C201, the forward search exhausts its time budget while
bidirectional finishes with the proven optimum.

| Situation | Effect |
|---|---|
| Small label sets | 0.8–1.1× — break-even, sometimes a small loss |
| Moderate label pressure | 1.4–2.9× |
| Heavy label pressure | 4–21×, widening as instances grow |
| Forward search cannot finish at all | Bidirectional finishes, with the optimum |

So it is **not a default**.  It does not pay when:

- **Few labels survive per node**, whatever the horizon.  A clustered instance with
  tight windows can have a horizon five times longer than another and still hold
  smaller label sets — and there the second set of containers, plus a join pass that
  walks every arc, costs more than the halving saves.
- **Short routes**, which is the same thing seen from the other side: nothing to halve.
- **No usable clock**, in which case the bound turns itself off and the search becomes
  a forward search plus a backward search plus a join — strictly more work than
  `Simple`.  `bounded_by_half_way()` reports this; it is worth checking rather than
  assuming.

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
