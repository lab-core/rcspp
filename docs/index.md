---
title: Home
---

# RCSPP

**RCSPP** is a modern C++23 library for solving
[Resource-Constrained Shortest Path Problems (RCSPP)](https://en.wikipedia.org/wiki/Constrained_shortest_path_first)
with optional Python bindings via [pybind11](https://github.com/pybind/pybind11).

---

## What is the RCSPP?

Given a directed graph where each arc consumes resources (time, capacity, cost …), find the
minimum-cost path from source to sink that stays within all resource limits.

The library models each resource independently through four pluggable functions:

| Function | Role |
|---|---|
| **Extension** | How traversing an arc changes the resource value |
| **Feasibility** | Whether the current resource value is still within bounds |
| **Dominance** | Whether one partial path makes another redundant |
| **Cost** | The contribution of this resource to the objective |

The solver maintains a set of *labels* (partial paths with accumulated resource state) and
prunes dominated ones, yielding an exact (or heuristic) set of optimal solutions.

---

## Quick Example

### Python

```python
from rcspp.graph import ResourceGraph
from rcspp.resource import (
    AdditionExtensionFunction,
    MinMaxFeasibilityFunction,
    TrivialCostFunction,
    ValueCostFunction,
    ValueDominanceFunction,
)

rg = ResourceGraph()

# Resource 0: cumulative distance — drives the objective (ValueCostFunction)
rg.add_real_resource(
    AdditionExtensionFunction(),
    MinMaxFeasibilityFunction(0.0, 50.0),   # max distance = 50
    ValueCostFunction(),
    ValueDominanceFunction(),
)

# Graph: 0 → 1 → 3  (cost 25, distance 25)
#        0 → 2 → 3  (cost 25, distance 30)
rg.add_node(0, source=True)
rg.add_node(1)
rg.add_node(2)
rg.add_node(3, sink=True)
rg.add_arc((10.0,), 0, 1, cost=10.0)
rg.add_arc((15.0,), 1, 3, cost=15.0)
rg.add_arc((20.0,), 0, 2, cost=20.0)
rg.add_arc((10.0,), 2, 3, cost=10.0)   # cheaper but farther

result = rg.solve()
print(result.solutions[0].cost)          # 25.0
print(result.solutions[0].path_node_ids) # [0, 1, 3]
```

### C++

```cpp
#include "rcspp/rcspp.hpp"

using namespace rcspp;
using RG = ResourceGraph<RealResource>;

RG graph;
graph.add_resource<RealResource>(
    std::make_unique<AdditionExtensionFunction<RealResource>>(),
    std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 50.0),
    std::make_unique<ValueCostFunction<RealResource>>(),
    std::make_unique<ValueDominanceFunction<RealResource>>());

graph.add_node(0, /*source=*/true);
graph.add_node(1);
graph.add_node(2);
graph.add_node(3, false, /*sink=*/true);
graph.add_arc({10.0}, 0, 1, 10.0);
graph.add_arc({15.0}, 1, 3, 15.0);
graph.add_arc({20.0}, 0, 2, 20.0);
graph.add_arc({10.0}, 2, 3, 10.0);

auto result = graph.solve();
std::cout << result.solutions[0].cost << "\n"; // 25.0
```

---

## Key Features

- **Multiple resource types** — `real`, `int`, `uint`, `real_set`, `int_set`, `bitset` and more
- **Multiple algorithms** — Simple Dominance, Pushing/Pulling Dominance, Greedy, Tabu Search, A★, Diversification
- **Column generation support** — `PricingPool` with deduplication, activity tracking, and cross-process shared memory
- **Preprocessing** — automatic arc removal via Bellman-Ford + connectivity analysis
- **Memory management** — configurable RSS limits, label pool recycling
- **NetworkX interop** — construct graphs directly from `nx.DiGraph`
- **Header-only C++ core** — drop into any CMake project via `FetchContent`

---

## Repository Layout

```text
rcspp/
├── cpp/rcspp/            # C++ headers (header-only library)
├── python/
│   ├── bindings/         # pybind11 C++ bindings
│   └── src/rcspp/        # Pure-Python package
├── examples/             # C++ benchmark and Python VRP examples
├── tests/
│   ├── cpp/              # GoogleTest suite
│   └── python/           # pytest suite
└── extern/pybind11/      # submodule
```

:::{toctree}
:maxdepth: 2
:hidden:
:caption: Contents

installation
cpp/index
python/index
advanced/index
:::
