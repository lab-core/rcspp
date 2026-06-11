---
title: C++ Library
---

# C++ Library

The C++ core is a **header-only** template library under `cpp/rcspp/`.  Include
the umbrella header and you get everything:

```cpp
#include "rcspp/rcspp.hpp"
```

All classes live in the `rcspp` namespace.

## Header organisation

```text
cpp/rcspp/
├── rcspp.hpp                   # umbrella header
├── resource/
│   ├── resource_graph.hpp      # ResourceGraph<...> — main entry point
│   ├── base/                   # ExtensionFunction, FeasibilityFunction, …
│   └── concrete/               # NumericalResource, ContainerResource, built-ins
├── graph/
│   ├── graph.hpp               # Graph<ResourceType> base
│   ├── node.hpp, arc.hpp
│   └── row.hpp
├── algorithm/
│   ├── algorithm.hpp           # Algorithm base, AlgorithmParams, SolveResult
│   ├── simple_dominance_algorithm.hpp
│   ├── pushing_dominance_algorithm.hpp
│   ├── pulling_dominance_algorithm.hpp
│   ├── greedy.hpp
│   ├── tabu_search.hpp / improving_tabu_search.hpp
│   ├── astar_dominance_algorithm.hpp
│   ├── diversification_search.hpp
│   ├── label_buckets.hpp       # LabelList, LabelBuckets
│   ├── solution.hpp
│   └── solution_pool.hpp
├── label/
│   ├── label.hpp, label_pool.hpp, label_factory.hpp
└── preprocessor/
    ├── shortest_path_preprocessor.hpp
    ├── feasibility_preprocessor.hpp
    └── connectivity_matrix.hpp
```

:::{toctree}
:maxdepth: 1
:hidden:

concepts
quickstart
beginner_api
advanced_api
api/library_root
:::
