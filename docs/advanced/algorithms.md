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
