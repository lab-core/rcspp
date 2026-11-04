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
│   ├── presets.hpp             # one call per resource kind
│   ├── base/                   # Resource, Extender, ResourceFactory
│   ├── functions/              # ExtensionFunction, FeasibilityFunction, DominanceFunction,
│   │                           # CostFunction, the directional resource kinds and forms
│   ├── concrete/               # NumericalResource, ContainerResource, built-in functions
│   └── composition/            # resources made of several components
├── graph/
│   ├── graph.hpp               # Graph<ResourceType> base
│   ├── node.hpp, arc.hpp
│   └── row.hpp
├── algorithm/
│   ├── algorithm.hpp           # Algorithm base, AlgorithmParams, SolveResult
│   ├── simple_dominance_algorithm.hpp
│   ├── pushing_dominance_algorithm.hpp
│   ├── pulling_dominance_algorithm.hpp
│   ├── astar_dominance_algorithm.hpp
│   ├── arc_mask.hpp            # ArcMask: arcs a solve leaves out (hard) or avoids (soft)
│   ├── proof_record.hpp        # ProofLoss: what a finished solve can still have lost
│   ├── diversification_search.hpp
│   ├── label_buckets.hpp       # LabelList, LabelBuckets
│   ├── path_evaluator.hpp      # PathEvaluator: replays a path, finds its first fault
│   ├── search_direction.hpp    # SearchDirection
│   ├── solution.hpp
│   └── solution_pool.hpp
├── label/
│   └── label.hpp, label_pool.hpp, label_factory.hpp
├── preprocessor/
│   ├── shortest_path_preprocessor.hpp
│   ├── feasibility_preprocessor.hpp
│   └── connectivity_matrix.hpp
├── validation/                 # model checks run before backward and bidirectional searches
├── utils/                      # logger, memory, timer
└── general/                    # Clonable
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
