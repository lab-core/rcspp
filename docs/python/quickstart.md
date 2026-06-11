---
title: Python Quick Start
---

# Python Quick Start

## Installation

```bash
pip install rcspp
```

## Minimal example

```python
from rcspp.graph import ResourceGraph
from rcspp.resource import (
    AdditionExtensionFunction,
    MinMaxFeasibilityFunction,
    ValueCostFunction,
    ValueDominanceFunction,
)

rg = ResourceGraph()

# One real resource: distance, must stay ≤ 50
rg.add_real_resource(
    AdditionExtensionFunction(),
    MinMaxFeasibilityFunction(0.0, 50.0),
    ValueCostFunction(),
    ValueDominanceFunction(),
)

rg.add_node(0, source=True)
rg.add_node(1)
rg.add_node(2)
rg.add_node(3, sink=True)

# add_arc(resource_consumption_tuple, origin, dest, cost)
rg.add_arc((10.0,), 0, 1, cost=10.0)
rg.add_arc((15.0,), 1, 3, cost=15.0)
rg.add_arc((20.0,), 0, 2, cost=20.0)
rg.add_arc((10.0,), 2, 3, cost=10.0)

result = rg.solve()
print(result.solutions[0].cost)           # 25.0
print(result.solutions[0].path_node_ids)  # [0, 1, 3]
print(result.status_string())             # "complete"
```

---

## Two resources — time windows + capacity

```python
from rcspp.graph import ResourceGraph
from rcspp.resource import (
    TimeWindowExtensionFunction, TimeWindowFeasibilityFunction, ValueDominanceFunction,
    AdditionExtensionFunction, MinMaxFeasibilityFunction,
    ValueCostFunction, TrivialCostFunction,
)

# time windows: {node_id: (ready_time, due_time)}
tw = {0: (0.0, 0.0), 1: (5.0, 20.0), 2: (0.0, 100.0)}

rg = ResourceGraph()

# Resource 0: time (objective = total time)
rg.add_real_resource(
    TimeWindowExtensionFunction(tw),
    TimeWindowFeasibilityFunction(tw),
    ValueCostFunction(),
    ValueDominanceFunction(),
)

# Resource 1: demand / capacity ≤ 10
rg.add_int_resource(
    AdditionExtensionFunction(),
    MinMaxFeasibilityFunction(0, 10),
    TrivialCostFunction(),
    ValueDominanceFunction(),
)

rg.add_node(0, source=True)
rg.add_node(1)
rg.add_node(2, sink=True)

# arc: (time, demand)
rg.add_arc((5.0, 3), 0, 1, cost=5.0)
rg.add_arc((8.0, 4), 1, 2, cost=8.0)

result = rg.solve()
```

---

## From a NetworkX graph

```python
import networkx as nx
from rcspp.graph import ResourceGraph
from rcspp.resource import AdditionExtensionFunction, TrivialFeasibilityFunction, \
    ValueCostFunction, ValueDominanceFunction

G = nx.DiGraph()
G.add_node(0, source=True)
G.add_node(1)
G.add_node(2, sink=True)
G.add_edge(0, 1, cost=3.0, consumption=(3.0,))
G.add_edge(1, 2, cost=5.0, consumption=(5.0,))

rg = ResourceGraph(G)
rg.add_real_resource(
    AdditionExtensionFunction(),
    TrivialFeasibilityFunction(),
    ValueCostFunction(),
    ValueDominanceFunction(),
)
result = rg.solve()
```

---

## Multiple solutions (diversification)

```python
from rcspp._core.graph import AlgorithmParams, Algorithm

params = AlgorithmParams()
params.stop_after_X_solutions = 10
params.max_iterations = 100
params.seed = 42

result = rg.solve(algorithm="greedy", params=params)
for sol in result.solutions:
    print(sol.cost, sol.path_node_ids)
```

---

## Column generation loop

```python
import math

rg = ResourceGraph()
# … build graph, add rows to arcs …
# rg.add_arc((10.0,), 0, 1, cost=10.0, rows=[(constraint_idx, coefficient)])

duals = [0.0] * n_constraints

while True:
    rg.update_reduced_costs(duals, cost_index=0)
    result = rg.solve(upper_bound=-1e-9)

    if not result.solutions:
        break  # no negative reduced-cost column

    for sol in result.solutions:
        master.add_column(sol.column)

    duals = master.get_duals()
```

---

## Set-based resource (NG-paths)

```python
from rcspp.resource import (
    NGPathExtensionFunction, SizeFeasibilityFunction,
    TrivialCostFunction, InclusionDominanceFunction,
)

rg = ResourceGraph()

# Track visited nodes (set); no two paths that visit the same node dominate
rg.add_int_set_resource(
    NGPathExtensionFunction(),
    SizeFeasibilityFunction(0, 10),     # path length ≤ 10 nodes
    TrivialCostFunction(),
    InclusionDominanceFunction(),        # L1 dominates L2 if visited(L1) ⊆ visited(L2)
)
```
