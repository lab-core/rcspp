---
title: Python Package
---

# Python Package

The Python package `rcspp` wraps the C++ core via pybind11 and adds a
Pythonic layer for ease of use.

## Package layout

```text
rcspp/
├── __init__.py         # top-level imports: LogLevel, set_log_level, …
├── graph.py            # ResourceGraph, AlgorithmParams, BucketAlgorithmParams
├── resource.py         # Extension / feasibility / cost / dominance functions
├── pricing_pool.py     # PricingPool, FilteredPricingPool, SharedPricingPool
├── logger.py           # Logging helpers
└── _core/              # Compiled C++ extension (auto-discovered at import)
    ├── graph           # SolveResult, Solution, Column, Row, AlgorithmParams, …
    ├── resource        # Typed C++ function classes
    └── solution_pool   # SolutionPool, FilteredSolutionPool
```

## Import style

```python
# High-level Python wrappers (recommended)
from rcspp.graph    import ResourceGraph, AlgorithmParams, BucketAlgorithmParams
from rcspp.resource import (
    AdditionExtensionFunction, MinMaxFeasibilityFunction,
    ValueCostFunction, ValueDominanceFunction, TrivialFeasibilityFunction,
    TrivialCostFunction, TimeWindowExtensionFunction,
    TimeWindowFeasibilityFunction, UnionExtensionFunction,
    IntersectionExtensionFunction, InclusionDominanceFunction,
    ContainDominanceFunction, SizeFeasibilityFunction,
)
from rcspp.pricing_pool import PricingPool

# Low-level C++ objects (advanced use)
from rcspp._core.graph        import Solution, Column, Row, Algorithm
from rcspp._core.solution_pool import SolutionPool
```

:::{toctree}
:maxdepth: 1
:hidden:

quickstart
api
:::
