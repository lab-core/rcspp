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
│   ├── algorithm.hpp           # Algorithm, the base class of every algorithm
│   ├── algorithm_params.hpp    # AlgorithmParams
│   ├── solve_result.hpp        # SolveResult, AlgorithmStatus
│   ├── limits.hpp              # kNoLimit
│   ├── strategies.hpp          # rcspp::strategy: Simple, AStar, Pushing, Pulling, Greedy,
│   │                           # Tabu, ImprovingTabu, Diversification
│   ├── make_algorithm.hpp      # builds the class a strategy needs in a direction
│   ├── labelling/              # the labelling kernel: the search, the take rule, the step,
│   │   │                       # the bidirectional search and its join (rcspp::detail)
│   │   └── parts/              # rcspp::parts: schedules, admission rules, retentions,
│   │                           # observers, interleaves
│   ├── backtracking_dive_algorithm.hpp
│   ├── tabu_search.hpp / improving_tabu_search.hpp
│   ├── tabu/                   # the tabu searches' tenures
│   ├── diversification_search.hpp
│   ├── path_evaluator.hpp      # PathEvaluator: replays a path, finds its first fault
│   ├── search_direction.hpp    # SearchDirection
│   ├── direction.hpp, container_direction.hpp  # the direction policies
│   ├── solution.hpp
│   └── solution_pool.hpp
├── label/
│   └── label.hpp, label_pool.hpp, label_factory.hpp, label_list.hpp, label_buckets.hpp
├── test_support/
│   └── optimum_problem.hpp     # checks a strategy of one's own against forward Simple
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
