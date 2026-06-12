---
title: Advanced C++ API
---

# Advanced C++ API

These are the base classes you subclass to implement custom resource behaviour
or custom algorithms.  You do **not** need these for typical use — start with
the [Beginner API](beginner_api) and the built-in implementations.

---

## Custom Extension Functions

Subclass `ExtensionFunction<ResourceType>` to define how traversing an arc
changes a resource value.

```{doxygenclass} rcspp::ExtensionFunction
:members:
:undoc-members:
```

```{doxygenclass} rcspp::TrivialExtensionFunction
:members:
:undoc-members:
```

---

## Custom Feasibility Functions

Subclass `FeasibilityFunction<ResourceType>` to define whether a label's
resource is still within bounds at a given node.

```{doxygenclass} rcspp::FeasibilityFunction
:members:
:undoc-members:
```

---

## Custom Dominance Functions

Subclass `DominanceFunction<ResourceType>` to decide when one label makes
another redundant.

```{doxygenclass} rcspp::DominanceFunction
:members:
:undoc-members:
```

---

## Custom Cost Functions

Subclass `CostFunction<ResourceType>` to map a resource value to a scalar cost
contribution.

```{doxygenclass} rcspp::CostFunction
:members:
:undoc-members:
```

---

## Algorithm base class

Subclass `Algorithm` to implement a custom label-management strategy.

```{doxygenclass} rcspp::Algorithm
:members:
:undoc-members:
```

---

## Clonable CRTP helper

Provides `clone()` and `create()` via CRTP so resource functions can be
deep-copied when the graph is duplicated across threads.

```{doxygenclass} rcspp::Clonable
:members:
:undoc-members:
```

---

## Low-level internals

```{doxygenclass} rcspp::Extender
:members:
:undoc-members:
```

```{doxygenclass} rcspp::Label
:members:
:undoc-members:
```

```{doxygenconcept} rcspp::ResourceTypeConcept
```
