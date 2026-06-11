---
title: Beginner C++ API
---

# Beginner C++ API

These are the classes you will instantiate directly when using rcspp.
You never need to subclass them — just construct and call.

Include the umbrella header:

```cpp
#include "rcspp/rcspp.hpp"
using namespace rcspp;
```

---

## Graph

The main entry point. `ResourceGraph<R1, R2, …>` is a directed graph whose
labels carry a composition of resource types.

```{doxygenclass} rcspp::ResourceGraph
:members:
:undoc-members:
```

```{doxygenclass} rcspp::Graph
:members:
:undoc-members:
```

---

## Nodes and Arcs

```{doxygenclass} rcspp::Node
:members:
:undoc-members:
```

```{doxygenclass} rcspp::Arc
:members:
:undoc-members:
```

```{doxygenstruct} rcspp::Row
:members:
:undoc-members:
```

---

## Algorithm parameters and results

```{doxygenstruct} rcspp::AlgorithmParams
:members:
:undoc-members:
```

```{doxygenstruct} rcspp::SolveResult
:members:
:undoc-members:
```

```{doxygenstruct} rcspp::Solution
:members:
:undoc-members:
```

---

## Label containers

```{doxygenclass} rcspp::LabelList
:members:
:undoc-members:
```

```{doxygenclass} rcspp::LabelBuckets
:members:
:undoc-members:
```

---

## Built-in Extension Functions

```{doxygenclass} rcspp::AdditionExtensionFunction
:members:
:undoc-members:
```

```{doxygenclass} rcspp::SubtractExtensionFunction
:members:
:undoc-members:
```

```{doxygenclass} rcspp::TimeWindowExtensionFunction
:members:
:undoc-members:
```

```{doxygenclass} rcspp::UnionExtensionFunction
:members:
:undoc-members:
```

```{doxygenclass} rcspp::IntersectionExtensionFunction
:members:
:undoc-members:
```

```{doxygenclass} rcspp::NgPathExtensionFunction
:members:
:undoc-members:
```

---

## Built-in Feasibility Functions

```{doxygenclass} rcspp::TrivialFeasibilityFunction
:members:
:undoc-members:
```

```{doxygenclass} rcspp::MinMaxFeasibilityFunction
:members:
:undoc-members:
```

```{doxygenclass} rcspp::TimeWindowFeasibilityFunction
:members:
:undoc-members:
```

```{doxygenclass} rcspp::SizeFeasibilityFunction
:members:
:undoc-members:
```

```{doxygenclass} rcspp::IntersectionFeasibilityFunction
:members:
:undoc-members:
```

---

## Built-in Dominance Functions

```{doxygenclass} rcspp::TrivialDominanceFunction
:members:
:undoc-members:
```

```{doxygenclass} rcspp::ValueDominanceFunction
:members:
:undoc-members:
```

```{doxygenclass} rcspp::InclusionDominanceFunction
:members:
:undoc-members:
```

```{doxygenclass} rcspp::ContainDominanceFunction
:members:
:undoc-members:
```

---

## Built-in Cost Functions

```{doxygenclass} rcspp::TrivialCostFunction
:members:
:undoc-members:
```

```{doxygenclass} rcspp::ValueCostFunction
:members:
:undoc-members:
```

---

## Solution pool

```{doxygenclass} rcspp::SolutionPool
:members:
:undoc-members:
```
