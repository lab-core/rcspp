---
title: Concepts
---

# Core Concepts

## The Label-Setting Algorithm

RCSPP is solved by a *label-setting* algorithm.  A **label** represents a
partial path from a source node to some intermediate node, together with the
accumulated resource state along that path.

At each node the algorithm maintains a set of non-dominated labels.  A label
`L₁` *dominates* `L₂` when `L₁` can reach the sink at least as cheaply as
`L₂` while consuming no more of any resource — so `L₂` can be discarded.

### Label lifecycle

```text
source node
    │
    ▼  extend label along arc (extension function)
intermediate node
    │
    ▼  feasibility check (feasibility function)
    │  dominance check against existing labels (dominance function)
    │
    ├─ dominated? discard
    └─ non-dominated? keep, add to node's label set
         │
         ▼  sink node
         extract path, compute cost (cost function)
```

---

## Resource composition

`ResourceGraph<R₁, R₂, …>` is parametrised by one or more resource types.
Internally the variadic types are wrapped in a
`ResourceTypeComposition<R₁, R₂, …>` whose extension, feasibility, dominance,
and cost functions are the **composition** of the individual functions.

Two labels are compared component-wise: `L₁ ≤ L₂` iff every resource of `L₁`
dominates the corresponding resource of `L₂`.

---

## Built-in resource types

| C++ type | Python name | Underlying value | Typical use |
|---|---|---|---|
| `RealResource` | `"real"` | `double` | Distance, time, cost |
| `IntResource` | `"int"` | `int` | Demand, hop count |
| `UIntResource` | — (C++ only) | `unsigned int` | Unsigned quantities |
| `SetResource<int>` | `"int_set"` | `std::set<int>` | Visited customers |
| `SetResource<double>` | `"real_set"` | `std::set<double>` | |
| `UIntBitsetResource` (`BitsetResource<unsigned>`) | `"bitset"` | `std::vector<uint64_t>`, one bit per element | Compact visited-node tracking |

---

## Resource functions

Each resource slot has four functions that you supply at construction time.

The interfaces below are simplified: each class also has a pure virtual `clone()`, which
`Clonable<Derived, Base>` supplies when you derive through it, and the backward and
bidirectional searches read further members, which have defaults. `AGENT.md` ("Custom
resource type") shows a complete, compiled example.

### Extension function

Defines how an arc modifies the resource as a label is extended along it.

<!-- docs-check: skip -->

```cpp
template<typename ResourceType>
class ExtensionFunction {
    virtual void extend(
        const ResourceType& current,        // label's resource at origin
        const ResourceType& arc_value,      // arc's resource consumption
        ResourceType* result) = 0;          // written in-place
};
```

Built-in implementations:

| Class | Effect |
|---|---|
| `AdditionExtensionFunction<T>` | `result = current + arc_value` |
| `TimeWindowExtensionFunction<T>` | `result = max(current + travel, ready_time)` |
| `CapacityExtensionFunction<T>` | `result = current + arc_value`, as a *threshold* (a capacity read backward) |
| `UnionExtensionFunction<T>` | `result = current ∪ arc_value` |
| `IntersectionExtensionFunction<T>` | `result = current ∩ arc_value` |
| `DifferenceExtensionFunction<T>` | `result = current ∖ arc_value` (containers only) |
| `NgPathExtensionFunction<T>` | `result = (current ∪ {node left}) ∩ ng[node arrived]` — derived from the arc's endpoints, so `arc_value` is ignored |

### Feasibility function

Returns `true` when a label's resource value is still within bounds.

<!-- docs-check: skip -->

```cpp
template<typename ResourceType>
class FeasibilityFunction {
    virtual bool is_feasible(const ResourceType& resource) = 0;
};
```

Built-in implementations:

| Class | Condition |
|---|---|
| `TrivialFeasibilityFunction<T>` | Always `true` |
| `MinMaxFeasibilityFunction<T>(min, max)` | `min ≤ value ≤ max` |
| `TimeWindowFeasibilityFunction<T>(tw)` | `value ≤ due_time[node]` |
| `SizeFeasibilityFunction<T>(min, max)` | `min ≤ container.size() ≤ max` |
| `IntersectionFeasibilityFunction<T>` | `current ∩ arc_value ≠ ∅` |
| `ReachableFeasibilityFunction<T>` | A node in its `checked_nodes` is entered only if the label's set already holds it |

### Dominance function

Returns `true` when `lhs` dominates `rhs` (i.e. `rhs` can be pruned).

<!-- docs-check: skip -->

```cpp
template<typename ResourceType>
class DominanceFunction {
    virtual bool check_dominance(
        const ResourceType& lhs, const ResourceType& rhs) = 0;
    virtual bool fast_check_dominance(      // the same test, within a tolerance;
        const ResourceType& lhs,            // LabelBuckets uses it to find a
        const ResourceType& rhs, double delta) = 0;   // label's bucket
};
```

Built-in implementations:

| Class | Condition |
|---|---|
| `TrivialDominanceFunction<T>` | Never dominates (keep all labels) |
| `ValueDominanceFunction<T>` | `lhs.value ≤ rhs.value` |
| `SubsetDominanceFunction<T>` | `lhs ⊆ rhs` |
| `SupersetDominanceFunction<T>` | `lhs ⊇ rhs` |

### Cost function

Returns the contribution of this resource slot to the label's total cost.

<!-- docs-check: skip -->

```cpp
template<typename ResourceType>
class CostFunction {
    virtual double get_cost(const ResourceType& resource) const = 0;
};
```

Built-in implementations:

| Class | Returns |
|---|---|
| `TrivialCostFunction<T>` | `0.0` |
| `ValueCostFunction<T>` | `static_cast<double>(resource.value)` |

---

## Arc consumption

Each arc stores a `unique_ptr<Extender<ResourceType>>`.  The extender bundles
the arc's resource consumption value together with the pre-bound extension
function, so extending a label along an arc is a single virtual call.

Arc resource consumption is specified at construction time:

<!-- docs-check: given size_t origin = 0; size_t dest = 1; double arc_cost = 1.0; -->

```cpp
// Single resource
graph.add_arc<RealResource>({10.0}, origin, dest, arc_cost);

// Two resources
graph.add_arc<RealResource, IntResource>({10.0, 3}, origin, dest, arc_cost);
```

---

## Label containers

The algorithm stores non-dominated labels at each node in a *label container*.
Two built-in containers are provided:

| Type | Complexity | Best for |
|---|---|---|
| `LabelList<R>` (default) | O(N) dominance check | Few labels per node |
| `LabelBuckets<R>` | O(log B) dominance check | Many labels; partitioned by one resource |

`LabelBuckets` requires a `BucketAlgorithmParams` specifying the bucket
resource index and sort resource index.

---

## Preprocessing

Before the main label loop, the solver (when `preprocess=true`) runs:

1. **Feasibility preprocessor** — removes arcs whose resource consumption
   makes them immediately infeasible.
2. **Shortest-path preprocessor** — uses a Bellman-Ford variant to remove
   arcs that cannot appear in any optimal path.
3. **Connectivity check** — ensures source and sink are connected; marks
   reachable subsets.

Removed arcs are restored automatically after `solve()` returns so the graph
can be reused in a column-generation loop.
