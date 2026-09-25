# AGENT.md — RCSPP Library Guide for AI Agents

This file gives an AI agent the minimum context needed to understand, use, and
extend the `rcspp` library correctly and efficiently.

---

## What the library does

**RCSPP** solves the *Resource-Constrained Shortest Path Problem*: find the
minimum-cost path from a source node to a sink node in a directed graph, where
each arc consumes one or more resources and the path must stay within resource
limits.

It is most commonly used as the **pricing subproblem** in column-generation
algorithms for vehicle routing, crew scheduling, and similar combinatorial
optimisation problems.

---

## Repository layout

```text
cpp/rcspp/            ← C++ header-only library (the "engine")
python/
  bindings/           ← pybind11 C++ source (wraps the C++ engine)
  src/rcspp/          ← pure-Python package (the public Python API)
tests/
  cpp/                ← GoogleTest suite (test_*.hpp headers, split across
                        test_main.cpp / test_resources.cpp / test_algorithms.cpp)
  python/             ← pytest suite
examples/
  cpp/                ← VRP C++ benchmark
  python/             ← VRP Python example (solve_vrp.py)
extern/pybind11/      ← submodule
```

The C++ library is **header-only**.  The umbrella include is:

```cpp
#include "rcspp/rcspp.hpp"
```

All C++ symbols live in namespace `rcspp`.

---

## Core data model

### Graph

- `ResourceGraph<R₁, R₂, …>` — main entry point; templated by resource types
- `Graph<ResourceType>` — base directed graph (node/arc management, CSR)
- `Node` — has `id`, `source`, `sink`, `in_arcs`, `out_arcs`
- `Arc` — has `id`, `cost`, `origin`, `destination`, `extender`, `rows`
- `Row{int index, double coefficient}` — LP master constraint coefficient

### Resources

Each resource slot is defined by four functions.  For the five common shapes
(cost, window, budget, ng-path, elementary) use **`rcspp/resource/presets.hpp`** —
one call per resource, constructing a quadruple that cannot disagree — and drop to
the four-object form below for anything else.  From Python, `rcspp.presets` has the
first three; the two container shapes are C++-only:

| Function | Interface | Responsibility |
|---|---|---|
| `ExtensionFunction<R>` | `extend(current, arc_val, result*)` | How an arc modifies the resource |
| `FeasibilityFunction<R>` | `is_feasible(resource) → bool` | Whether the resource is within bounds |
| `DominanceFunction<R>` | `check_dominance(lhs, rhs) → bool` | Whether lhs makes rhs redundant |
| `CostFunction<R>` | `get_cost(resource) → double` | Resource's contribution to objective |

Built-in resource types: `RealResource` (double), `IntResource` (int),
`UIntResource`, `SetResource<T>`, `BitsetResource`.

### Labels

A `Label` represents a partial path.  It stores the accumulated resource state
and a pointer back to the parent label.  The solver keeps non-dominated labels
at each node.

---

## Python API — essential patterns

### Import

```python
from rcspp.graph    import ResourceGraph, AlgorithmParams, BucketAlgorithmParams
from rcspp.resource import (
    AdditionExtensionFunction, SubtractExtensionFunction,
    BudgetExtensionFunction,
    TimeWindowExtensionFunction, TimeWindowFeasibilityFunction,
    UnionExtensionFunction, IntersectionExtensionFunction,
    TrivialFeasibilityFunction, MinMaxFeasibilityFunction,
    SizeFeasibilityFunction,
    TrivialCostFunction, ValueCostFunction,
    ValueDominanceFunction,
    InclusionDominanceFunction, ContainDominanceFunction,
)
from rcspp import presets          # add_cost_resource, add_window_resource, add_budget_resource
from rcspp.pricing_pool import PricingPool
from rcspp._core.graph  import Solution, Column, Row, Algorithm, AlgorithmStatus
```

**Not exposed to Python.** `NgPathExtensionFunction`, `IntersectionFeasibilityFunction` and
`TrivialDominanceFunction` have no Python descriptors, so **an ng-path or elementary-path model
cannot be built from Python at all** — those need the C++ API.  `rcspp.presets` mirrors
`rcspp::presets` minus the two container presets, for the same reason.

### Build and solve

```python
rg = ResourceGraph()

# 1. Register resources (must match order of tuple elements in add_arc)
rg.add_real_resource(ext, feas, cost, dom)   # index 0
rg.add_int_resource(ext, feas, cost, dom)    # index 1

# 2. Add nodes
rg.add_node(0, source=True)
rg.add_node(1)
rg.add_node(2, sink=True)

# 3. Add arcs: (resource_tuple, origin, dest, cost, rows)
rg.add_arc((10.0, 3), 0, 1, cost=10.0)
rg.add_arc((5.0,  2), 1, 2, cost=5.0)

# 4. Solve
result = rg.solve()                          # returns SolveResult
result = rg.solve(algorithm="simple",        # or "greedy", "pushing", "astar",
                                             # "bidirectional", …
                  upper_bound=-1e-9,         # prune cost ≥ this
                  params=AlgorithmParams(),
                  preprocess=True,
                  cost_index=0)
```

### SolveResult

```python
result.solutions            # list[Solution], best-first
result.status               # AlgorithmStatus enum
result.status_string()      # "complete" | "timeout" | "max_solutions" | …
# How many labels the search extended is on the algorithm, not on SolveResult:
#   auto algo = graph.create_algorithm<Algo>(params);
#   graph.solve(algo.get());
#   algo->get_number_of_extended_labels();

sol = result.solutions[0]
sol.cost            # float
sol.path_node_ids   # list[int]
sol.path_arc_ids    # list[int]
sol.column          # Column(cost, rows=[Row(index, coefficient)])
```

### Column generation

```python
# Set rows on arcs at construction
rg.add_arc((dist,), i, j, cost=dist, rows=[(constraint_idx, 1.0)])

# Update reduced costs from LP duals
rg.update_reduced_costs(duals, cost_index=0)

# Find negative-RC columns only
result = rg.solve(upper_bound=-1e-9)
```

### Arc manipulation

```python
rg.remove_arcs([arc_id])           # temporarily remove
rg.restore_arcs([arc_id])          # undo removal
rg.remove_arcs_if(lambda arc: …)   # predicate-based remove, returns removed ids
rg.restore_arcs_if(lambda arc: …)
rg.clone()                          # deep copy, independent remove/restore state
```

### AlgorithmParams

```python
p = AlgorithmParams()
p.stop_after_X_solutions  = 10
p.max_iterations          = 1000
p.timeout_s               = 30.0
p.tabu_tenure             = 5
p.seed                    = 42
p.limit_to_available_ram  = True
p.memory_limit_fraction   = 0.8

# Bidirectional only
p.critical_resource_index = 1      # the clock's slot within the cost resource type
p.half_way_point          = 500.0  # H; the clock's range is taken as [0, 2H]
```

### `algorithm="bidirectional"`

Searches forward from the sources and backward from the sinks, stops each
direction at a half-way point `H` on one designated resource, and joins the halves
at the node where that resource crosses `H`. Three requirements:

- **The critical resource must be a clock**: monotone, bounded, and a *threshold*
  backwards (`TimeWindowExtensionFunction` or `BudgetExtensionFunction`, never
  `AdditionExtensionFunction` and never the cost). If it is not, the half-way bound
  switches itself off — the answer stays correct, the solve is just slower, and it
  logs a warning once per reason per process. The default `half_way_point = 0` is one
  of those reasons: set `H` to about half the clock's range.
- **Every resource must declare its backward semantics**, or the solve raises before
  the first label and names the offending component. A capacity may be written as
  `AdditionExtensionFunction` + `MinMaxFeasibilityFunction(0, cap)` or as
  `BudgetExtensionFunction(cap)` (signed numerical types only) + the same feasibility
  function; either way its loads must be non-negative.
- **A container that forbids anything must forbid each node at itself, and its memory
  must come from a node-identity mirror.** `IntersectionFeasibilityFunction` asks what
  the label has collected *so far*, which is a predicate on a prefix; only
  `forbidden[v] = {v}` carried by `NgPathExtensionFunction` has a backward reading.
  Anything else raises at setup and names the component — including the obvious
  elementary-path spelling, a visited set built with `UnionExtensionFunction`. Use
  `presets::add_ng_path_resource` or `presets::add_elementary_resource`. The other
  prefix questions are refused outright: `IntersectionFeasibilityFunction` with
  *required* values, `SizeFeasibilityFunction` with a non-zero minimum,
  `ReachableFeasibilityFunction`, and a `MinMaxFeasibilityFunction` floor (any non-zero
  minimum) under a threshold extension.

Unlike the clock, the last two are refusals rather than slowdowns: a model that cannot
express backward semantics cannot produce a correct answer at all. And a per-node cap goes on
both functions: a threshold extension clamps its backward label to its own bound at each node,
so build it and the feasibility function from one `NodeBounds` (`make_node_bounds`, or the
feasibility function's `bounds()`), as `presets::add_budget_resource` does. Setup refuses a
clamp that differs from the feasibility function's bound.

**A half-way point that adapts across a pricing loop.** `H` never moves *during* a solve,
but a `HalfWayController` can move it *between* solves, away from whichever direction
kept more labels. From Python, keep one across the loop — `rg.solve` builds a fresh
algorithm every call, so there is no `dynamic_half_way` parameter:

```python
from rcspp import HalfWayController

controller = HalfWayController(500.0)        # H0; also fixes the range [0, 2 * H0]
for _ in range(iterations):
    p.half_way_point = controller.h
    result = rg.solve(algorithm="bidirectional", params=p)
    controller.update(result)
```

From C++, set `params.dynamic_half_way = true` on an algorithm object you keep alive
(`create_algorithm` + `solve(algorithm.get(), ...)`); `half_way_controller()` reads,
freezes or resets it.

See `docs/advanced/algorithms.md` for the full description.

---

## C++ API — essential patterns

### Single resource

```cpp
using RG = ResourceGraph<RealResource>;
RG graph;
graph.add_resource<RealResource>(
    std::make_unique<AdditionExtensionFunction<RealResource>>(),
    std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 100.0),
    std::make_unique<ValueCostFunction<RealResource>>(),
    std::make_unique<ValueDominanceFunction<RealResource>>());

graph.add_node(0, /*source=*/true);
graph.add_node(1, false, /*sink=*/true);
graph.add_arc({10.0}, 0, 1, /*cost=*/10.0);

auto result = graph.solve();
```

### Multiple resources

```cpp
using RG = ResourceGraph<RealResource, IntResource>;
RG graph;
graph.add_resource<RealResource>(…);   // index 0
graph.add_resource<IntResource>(…);    // index 1

// arc: {real_val, int_val}
graph.add_arc(std::make_tuple(std::vector{10.0}, std::vector{3}),
              origin, dest, cost);
```

### Column generation

```cpp
// Arcs with LP rows
graph.add_arc(…, origin, dest, cost, {Row{constraint_idx, 1.0}});

// CG loop
graph.update_reduced_costs(duals, /*cost_index=*/0);
auto result = graph.solve(/*upper_bound=*/-1e-9);
```

---

## Resource function quick reference

### Extension

| Class | Resource | Effect |
|---|---|---|
| `AdditionExtensionFunction<T>` | Numerical | `+=` |
| `SubtractExtensionFunction<T>` | Numerical | `-=` |
| `TimeWindowExtensionFunction<T>(tw)` | Numerical | `max(cur + travel, ready[node])` |
| `BudgetExtensionFunction<T>(cap)` | Numerical | `+=`, as a *threshold* — the backward form of a capacity; per-node caps come as a `NodeBounds` shared with the paired feasibility function |
| `UnionExtensionFunction<T>` | Container | `∪=` the arc's value |
| `IntersectionExtensionFunction<T>` | Container | `∩=` the arc's value |
| `SubtractExtensionFunction<T>` | Container | `-=` the arc's value |
| `NgPathExtensionFunction<T>(ng)` | Set | `(memory ∪ {node left}) ∩ ng[node arrived]` — ignores the arc's value |

The last four are all containers, but they split into two **backward kinds**, and pairing the wrong
one with a forbidden-set feasibility is refused at setup:

- `NgPathExtensionFunction` reads node identities off the arc's *endpoints* and swaps them per
  direction (`BackwardKind::EndpointMirror`), so the memory at a node excludes that node going both
  ways.  Only this can carry the ng-route condition `forbidden[v] = {v}`.
- The other three accumulate the arc's *value*, which is the same object in both directions
  (`BackwardKind::ArcValue`).  Right for genuine per-arc set data; wrong for node identities.

### Feasibility

| Class | Condition |
|---|---|
| `TrivialFeasibilityFunction<T>` | always true |
| `MinMaxFeasibilityFunction<T>(min, max)` | `min ≤ val ≤ max` |
| `TimeWindowFeasibilityFunction<T>(tw)` | `val ≤ due[node]` |
| `SizeFeasibilityFunction<T>(min, max)` | `min ≤ size ≤ max` |
| `IntersectionFeasibilityFunction<T>(sets)` | forbidden (default): `∩ = ∅`; required: `∩ ≠ ∅` |

### Dominance

| Class | Condition |
|---|---|
| `TrivialDominanceFunction<T>` | never |
| `ValueDominanceFunction<T>` | `lhs ≤ rhs` |
| `InclusionDominanceFunction<T>` | `lhs ⊆ rhs` |
| `ContainDominanceFunction<T>` | `lhs ⊇ rhs` |

### Cost

| Class | Returns |
|---|---|
| `TrivialCostFunction<T>` | 0 |
| `ValueCostFunction<T>` | `double(value)` |

---

## Build

```bash
# C++ only
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# With Python bindings
cmake -B build -DCMAKE_BUILD_TYPE=Release -DUSE_PYTHON=ON
cmake --build build --parallel

# CMake options
# USE_PYTHON=ON   — build pybind11 extension
# USE_TESTS=ON    — build GoogleTest suite (default: ON)
# USE_VRP=ON      — build VRP benchmark (needs Gurobi)
# USE_COVERAGE=ON — instrument with gcov (gcc only)
```

### FetchContent

```cmake
FetchContent_Declare(rcspp
  GIT_REPOSITORY https://github.com/lab-core/rcspp.git
  GIT_TAG main
  GIT_SUBMODULES "extern/pybind11")
FetchContent_MakeAvailable(rcspp)
target_link_libraries(my_target PRIVATE rcspp)
```

---

## Tests

```bash
# C++ tests
ctest --test-dir build -C Release --output-on-failure

# Python tests
cd tests/python && pytest -v --tb=short
```

Test files:

- `tests/cpp/test_rcspp.hpp` — labelling, algorithms, graph ops
- `tests/cpp/test_solution_pool.hpp` — SolutionPool API
- `tests/python/test_rcspp.py` — Python solver tests
- `tests/python/test_bindings_coverage.py` — binding coverage

---

## Common pitfalls

1. **Resource order must match** — the tuple in `add_arc` must have one element
   per registered resource, in registration order.

2. **`update()` / flush** — Python `ResourceGraph` buffers nodes and arcs.
   They are flushed automatically before `solve()` and read operations, but not
   before `get_nodes_size()` (which counts the buffer).  Call `rg.update()`
   explicitly if you need the C++ graph up-to-date before a custom operation.

3. **Preprocessing** — `solve(preprocess=True)` (default) removes arcs that
   cannot appear in any optimal path.  Arcs are restored after `solve()`, so
   the graph is unchanged for subsequent calls.  Pass `preprocess=False` to
   skip this (e.g. if you call solve in a tight loop and the graph hasn't
   changed).

4. **`update_reduced_costs` does not rebuild the graph** — it only changes arc
   costs.  You do NOT need to rebuild nodes/arcs between CG iterations.
   `set_ng_neighborhoods` is the exception that does: arcs cache the ng neighbourhoods they
   read when they are built, so it re-creates every arc's ng component. Call it between solves.

5. **`DiversificationSearch` requires `max_iterations`** — without a finite
   limit the outer loop never terminates.

6. **Label containers** — `LabelList` (default) is O(N) per dominance check.
   Use `LabelBuckets` + `BucketAlgorithmParams` for problems with many labels
   per node (e.g. > 100 non-dominated labels).

7. **`clone()` is not free** — it deep-copies all nodes, arcs, and the
   resource factory.  Prefer `remove_arcs` / `restore_arcs` over clone-per-node
   in B&B.

8. **Thread safety** — `ResourceGraph` is NOT thread-safe.  `SolutionPool` /
   `PricingPool` ARE thread-safe for concurrent `add` + `price` calls.

9. **No source or sink inside a path** — every algorithm returns paths that start at
   a source and end at a sink with neither in between: a search never continues past
   a sink and never extends into a source. Model a depot a route passes through as
   separate source and sink nodes.

---

## Extending the library

### Custom resource type (C++)

```cpp
struct MyResource {
    double value = 0.0;
    bool leq(const MyResource& other) const { return value <= other.value; }
    bool geq(const MyResource& other) const { return value >= other.value; }
    void add(double v) { value += v; }
    void reset() { value = 0.0; }
    double get_value() const { return value; }
};

class MyExtensionFn : public ExtensionFunction<MyResource> {
    void extend(const MyResource& cur, const MyResource& arc,
                MyResource* res) override {
        res->value = cur.value + arc.value * 2.0;  // custom rule
    }
    std::unique_ptr<ExtensionFunction<MyResource>> clone() const override {
        return std::make_unique<MyExtensionFn>(*this);
    }
};
```

### Custom algorithm (C++)

Inherit from `Algorithm<ResourceType, LabelContainerType>` and override
`main_loop()`:

```cpp
template<typename R, typename LC = LabelList<R>>
class MyAlgorithm : public Algorithm<R, LC> {
  protected:
    void main_loop() override { /* custom label expansion */ }
};
```
