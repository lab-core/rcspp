---
title: Python API Reference
parent: Python Package
nav_order: 3
---

# Python API Reference

---

## `ResourceGraph`

```python
from rcspp.graph import ResourceGraph
rg = ResourceGraph(nx_graph=None)
```

### Resource registration

Must be called before adding nodes/arcs.  The order of calls defines the
resource index used in `add_arc` tuples and `cost_index`.

```python
# Numerical resources
rg.add_real_resource(ext, feas, cost, dom)   # RealResource (float)
rg.add_int_resource(ext, feas, cost, dom)    # IntResource (int)
rg.add_uint_resource(ext, feas, cost, dom)   # UIntResource (unsigned int)

# Container resources
rg.add_real_set_resource(ext, feas, cost, dom)  # set[float]
rg.add_int_set_resource(ext, feas, cost, dom)   # set[int]
rg.add_bitset_resource(ext, feas, cost, dom)    # bitset
```

Each argument is an instance of the corresponding function class from
`rcspp.resource` (see [Resource Functions](#resource-functions)).

### Building the graph

```python
rg.reserve(n_nodes, n_arcs)           # optional: pre-allocate capacity

rg.add_node(node_id, source=False, sink=False)

arc_id = rg.add_arc(
    resource_consumption,   # tuple of values, one per registered resource
    origin_id,
    destination_id,
    cost=0.0,
    rows=None,              # list of (constraint_index, coefficient) or Row objects
)

rg.update()   # flush buffers to C++ (called automatically before solve/get_*)
```

`resource_consumption` is a flat tuple when all resources are scalar:
`(real_val, int_val)`.  For container resources, each element is itself a
collection: `({1, 2, 3}, 5.0)`.

### Reading the graph

```python
node = rg.get_node(node_id)         # Node object or None
arc  = rg.get_arc(arc_id)           # Arc object or None
arcs = rg.get_arcs(origin, dest)    # list[Arc]

rg.get_nodes_size()  # int (includes buffered)
rg.get_arcs_size()   # int (includes buffered)
```

### Arc modification

```python
# Remove / restore individual arcs
removed  = rg.remove_arcs([arc_id, …])    # list[int] — actually removed
restored = rg.restore_arcs([arc_id, …])   # list[int] — actually restored

# In-place arc modification
rg.update_arc(arc, new_consumption)

# Append LP rows
rg.add_rows_to_arc(arc_id, [(constraint_idx, coeff), …])
rg.add_rows([
    (arc_id, constraint_idx, coeff),
    …
])

# arc_ids iterator
ids = rg.arc_ids()                 # list[int]

# Remove/restore by predicate (returns affected arc ids)
removed  = rg.remove_arcs_if(lambda arc: arc.id % 2 == 0)
restored = rg.restore_arcs_if(lambda arc: arc.id % 2 == 0)
```

### Solving

```python
result = rg.solve(
    algorithm="simple",    # "simple" | "pushing" | "pulling" | "greedy" | "astar"
                           # or Algorithm enum from rcspp._core.graph
    upper_bound=math.inf,
    params=None,           # AlgorithmParams or BucketAlgorithmParams
    preprocess=True,
    cost_index=0,          # which registered real/int resource is the objective
)
```

Returns a `SolveResult` (see below).

### Cloning

```python
clone = rg.clone(include_rows=True, clone_removed_arcs=False)
topo  = rg.clone_topology()   # same as clone(include_rows=False)
```

### Column generation

```python
rg.update_reduced_costs(duals, cost_index=0)
# Updates each arc's cost to: arc.cost - Σ(row.coeff * duals[row.index])
```

### NetworkX

```python
rg = ResourceGraph(nx_graph)     # construct from nx.DiGraph
                                  # nodes: attrs 'source', 'sink'
                                  # edges: attrs 'cost', 'consumption'
rg.from_networkx(nx_graph)       # rebuild from a new nx.DiGraph
```

---

## `AlgorithmParams`

```python
from rcspp.graph import AlgorithmParams

p = AlgorithmParams()
p.stop_after_X_solutions  = 1         # stop after N solutions
p.max_iterations          = 1_000_000
p.timeout_s               = 60.0
p.num_max_phases          = 1
p.seed                    = 0
p.tabu_tenure             = 5
p.tabu_random_noise       = True
p.forbidden_tabu          = {source_id, sink_id}
p.limit_to_available_ram  = False
p.memory_limit_fraction   = 0.9
p.release_after_solve     = True
```

## `BucketAlgorithmParams`

```python
from rcspp.graph import BucketAlgorithmParams

bp = BucketAlgorithmParams()
bp.range_buckets         = 100     # number of buckets
bp.bucket_resource_index = 0       # which resource partitions the buckets
bp.sort_resource_index   = 1       # sort key within each bucket
# All AlgorithmParams fields are also available
```

---

## `SolveResult`

```python
result = rg.solve()

result.solutions            # list[Solution], sorted best-first
result.status               # AlgorithmStatus enum
result.status_string()      # "complete" | "timeout" | "max_solutions" | …
result.num_extended_labels  # int

for sol in result:          # iterable
    print(sol.cost)

result[0]                   # index access
result[-1]                  # negative index
bool(result)                # True if any solutions
repr(result)                # "SolveResult(n=3, status=complete)"
```

### `Solution`

```python
sol.cost               # float
sol.path_node_ids      # list[int]
sol.path_arc_ids       # list[int]
sol.column             # Column object

# numpy helpers (requires numpy)
cost, nodes, row_idx, row_coeff = sol.to_arrays()
```

### `Column` and `Row`

```python
from rcspp._core.graph import Column, Row

col = Column()
col.cost = 10.0
col.rows = [Row(index=0, coefficient=1.0)]
```

---

## Resource Functions

All classes live in `rcspp.resource`.  The generic descriptors (e.g.
`AdditionExtensionFunction`) are automatically resolved to the correct typed
C++ class when `add_<type>_resource` is called.

### Extension functions

| Class | Applies to | Effect |
|---|---|---|
| `AdditionExtensionFunction()` | Numerical | `result = current + arc_value` |
| `SubtractExtensionFunction()` | Numerical | `result = current - arc_value` |
| `TimeWindowExtensionFunction(tw, default_max=None)` | Numerical | `result = max(current + travel, ready[node])` |
| `UnionExtensionFunction()` | Container | `result = current ∪ arc_value` |
| `IntersectionExtensionFunction()` | Container | `result = current ∩ arc_value` |
| `SubtractExtensionFunction()` | Container | `result = current − arc_value` |
| `NGPathExtensionFunction()` | Set | Adds destination node to visited set |

`tw` is a `dict` mapping `node_id → (ready_time, due_time)`.

### Feasibility functions

| Class | Condition |
|---|---|
| `TrivialFeasibilityFunction()` | Always feasible |
| `MinMaxFeasibilityFunction(min, max)` | `min ≤ value ≤ max` |
| `TimeWindowFeasibilityFunction(tw)` | `value ≤ due_time[node]` |
| `SizeFeasibilityFunction(min_size, max_size)` | `min ≤ len(container) ≤ max` |
| `IntersectionFeasibilityFunction()` | `current ∩ arc_value ≠ ∅` |
| `ReachableFeasibilityFunction()` | Can reach sink |

### Dominance functions

| Class | Condition |
|---|---|
| `TrivialDominanceFunction()` | Never dominates (keep all) |
| `ValueDominanceFunction()` | `lhs.value ≤ rhs.value` |
| `InclusionDominanceFunction()` | `lhs ⊆ rhs` |
| `ContainDominanceFunction()` | `lhs ⊇ rhs` |

### Cost functions

| Class | Returns |
|---|---|
| `TrivialCostFunction()` | `0.0` |
| `ValueCostFunction()` | `float(resource.value)` |

---

## `PricingPool` (column generation)

`PricingPool` bridges the C++ `SolutionPool` and a cross-process numpy shared
array, enabling lock-free pricing in worker processes.

```python
from rcspp.pricing_pool import PricingPool
import numpy as np

pool = PricingPool(n_constraints=200, max_cols=50_000)

# Add a solution (deduplicates by arc path)
col_id = pool.add(solution)

# Batch add
col_ids = pool.add_columns([sol1, sol2, sol3])

# Price: returns (ColumnIds, reduced_costs) sorted best-first
ids, rcs = pool.price(duals=np.zeros(200), threshold=-1e-9)

# Activity tracking (call after LP basis is updated)
pool.update_activity(basis_col_ids)     # forwarded to C++ pool

# Create a view (for B&B nodes)
sub = pool.new_filter(forbidden_arc_ids=[10, 11], max_age=100)
ids, rcs = sub.price(duals)

# Cleanup
pool.remove_stale(max_age=50, min_usage_rate=0.01)
pool.close()
```

### Cross-process usage (parallel pricing)

```python
# Master process
pool   = PricingPool(n_constraints=200, max_cols=50_000)
handle = pool.handle()              # serialisable dict

# Worker process
from rcspp.pricing_pool import PricingPool
shared = PricingPool.attach(handle) # SharedPricingPool (numpy only, no C++)
indices, rcs = shared.price(duals)  # lock-free; returns shared slot indices
```

---

## Logging

```python
from rcspp import LogLevel, set_log_level, get_log_level, init_logger

set_log_level(LogLevel.Debug)   # Debug | Info | Warning | Error | Off
init_logger(LogLevel.Info, to_console=True, file_path="rcspp.log")

current = get_log_level()
```

---

## Memory utilities

```python
from rcspp import process_memory_bytes, available_memory_bytes

rss       = process_memory_bytes()    # current RSS in bytes
available = available_memory_bytes()  # available system RAM in bytes
```
