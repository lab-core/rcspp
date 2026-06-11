---
title: Column Generation
---

# Column Generation

RCSPP arises naturally as the pricing subproblem in column-generation (CG)
algorithms for vehicle routing and crew scheduling.  This page describes the
full integration pattern.

## Concept

In a CG loop the LP master provides dual values `π` (one per constraint).  The
pricing subproblem finds a path (column) with *negative reduced cost*:

```text
reduced_cost = arc_cost_sum - Σ π[i] * coefficient[i]
```

If no such column exists, the LP is optimal.

Each arc carries a list of `Row` objects `(index, coefficient)` describing its
contribution to each LP constraint.  The solver recomputes arc costs from duals
via `update_reduced_costs(duals)`, then calls `solve(upper_bound=-1e-9)` to
find only negative-RC columns.

---

## Minimal Python CG loop

```python
import math
from rcspp.graph import ResourceGraph
from rcspp.resource import (
    AdditionExtensionFunction, MinMaxFeasibilityFunction,
    ValueCostFunction, ValueDominanceFunction,
)

# Build graph
rg = ResourceGraph()
rg.add_real_resource(
    AdditionExtensionFunction(),
    MinMaxFeasibilityFunction(0.0, math.inf),
    ValueCostFunction(),
    ValueDominanceFunction(),
)

n_customers = 5
for i in range(n_customers + 2):
    rg.add_node(i, source=(i == 0), sink=(i == n_customers + 1))

for i in range(n_customers + 1):
    for j in range(1, n_customers + 2):
        if i != j:
            dist = distance_matrix[i][j]
            # Row: this arc "visits" customer j → constraint index j-1
            rg.add_arc(
                (dist,), i, j, cost=dist,
                rows=[(j - 1, 1.0)] if 1 <= j <= n_customers else [],
            )

# CG loop
duals = [0.0] * n_customers

while True:
    rg.update_reduced_costs(duals, cost_index=0)
    result = rg.solve(upper_bound=-1e-9)

    if not result.solutions:
        break   # LP optimal

    for sol in result.solutions:
        master.add_column(sol.column)

    duals = master.solve_and_get_duals()
```

---

## `PricingPool` — column pool with activity tracking

For large CG applications, reusing previously found columns across iterations
is critical.  `PricingPool` manages the column pool and exposes fast
numpy-based pricing.

```python
from rcspp.pricing_pool import PricingPool
import numpy as np

pool = PricingPool(n_constraints=n_customers, max_cols=100_000)

# CG loop
duals = np.zeros(n_customers)

while True:
    rg.update_reduced_costs(duals.tolist())
    result = rg.solve(upper_bound=-1e-9)

    if result.solutions:
        new_ids = pool.add_columns(result.solutions)
        for sol in result.solutions:
            master.add_column(sol.column)

    # Price ALL stored columns (fast numpy path)
    ids, rcs = pool.price(duals, threshold=-1e-9)

    if len(ids) == 0 and not result.solutions:
        break   # no new and no stored negative-RC column

    # Mark which columns are in the LP basis (for activity tracking)
    pool.update_activity(master.get_basis_column_ids())
    duals = np.array(master.solve_and_get_duals())
```

### B&B node filtering

At each branch-and-bound node, certain arcs are forbidden.  Create a
`FilteredPricingPool` view instead of rebuilding the pool:

```python
node_pool = pool.new_filter(forbidden_arc_ids=[forbidden_arc])
ids, rcs = node_pool.price(duals)
```

### Cleanup

```python
# Remove columns that have not been used recently
pool.remove_stale(max_age=50, min_usage_rate=0.01)

# Or custom predicate
pool.global_remove_if(lambda col_id, sol, act: act.age > 100)
```

---

## Cross-process parallel pricing

For parallel B&B or multi-commodity CG, share the column pool across
processes using shared memory:

```python
# Master process
pool   = PricingPool(n_constraints=200, max_cols=50_000)
handle = pool.handle()    # serialisable dict — pass to workers

# Worker process (no pybind11 C++ needed — numpy only)
from rcspp.pricing_pool import PricingPool
shared = PricingPool.attach(handle)      # SharedPricingPool
indices, rcs = shared.price(np.array(duals))
```

The shared pool uses a memory-mapped numpy array; `price()` is lock-free and
safe to call from multiple processes simultaneously.

---

## Arc rows: multiple commodity constraints

```python
# Arc visits customer 3 (constraint 3) AND uses one vehicle (constraint 0)
rg.add_arc(
    (dist,), i, j, cost=dist,
    rows=[(0, 1.0), (3, 1.0)],
)
```

Rows can also be added after construction:

```python
rg.add_rows_to_arc(arc_id, [(constraint_idx, coeff)])

# Bulk add (numpy-friendly)
rg.add_rows(np.array([
    [arc_id_0, constraint_0, coeff_0],
    [arc_id_1, constraint_1, coeff_1],
    # ...
]))
```
