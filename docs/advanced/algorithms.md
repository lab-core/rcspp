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
inside it**. No algorithm continues past a sink it reaches, and no algorithm extends a path into a
source. A walk that returns to the depot and leaves again is two routes; priced as one column, with
the vehicle-count row counted once, a set-partitioning master would buy it and be wrong.

:::{admonition} Changed
:class: note

The rule used to hold only in part. Every algorithm could pass through a *source*, and `pulling`
and `greedy` -- with the tabu and diversification searches built on the same dive -- could also
continue past a sink. On a graph with several sources, arcs into a source, or arcs out of a sink,
they can now return a different answer. With sources `0` and `1`, sink `2`, and arcs `0 → 1` (−5),
`1 → 2` (−1), `0 → 2` (0), the old optimum was −6 through `0 1 2`; it is now −1 through `1 2`. A
route that genuinely continues through a depot is two routes: give the depot separate source and
sink copies, which is what the VRP example does.
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
