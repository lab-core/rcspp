---
title: Column Generation
---

# Column Generation

RCSPP arises naturally as the pricing subproblem in column-generation (CG)
algorithms for vehicle routing and crew scheduling.  This page describes the
full integration pattern.

## The column generation package

The package `rcspp.column_generation` runs the whole loop in Python: you build the master LP in **highspy** or
**gurobipy**, describe the pricing problem as a `ResourceGraph`, and the loop solves the master, prices its duals
with rcspp's search and adds the columns it finds, until no improving column is left. The sections after this one
describe the concepts and the building blocks for writing a loop of your own.

### Installing a master solver

The package itself needs only rcspp and numpy. The master LP is solved by one of two master
solvers, each over a model of its own LP solver; install the one you use:

```bash
pip install rcspp[highspy]    # HighspyMasterSolver, over a highspy.Highs model
pip install rcspp[gurobipy]   # GurobipyMasterSolver, over a gurobipy.Model
```

| | `HighspyMasterSolver` | `GurobipyMasterSolver` |
|---|---|---|
| LP solver | [HiGHS](https://highs.dev), open source (MIT) | [Gurobi](https://www.gurobi.com), commercial |
| Licence | none | a Gurobi licence (free for academics); without one, small models only |
| Model you build | `highspy.Highs` | `gurobipy.Model` |

Both offer the same features and give the same results: choose the solver you already work
with.

### A first run

Three customers with demands 4, 5 and 6, vehicles of capacity 11, each customer visited exactly
once. The master has one row per customer; the pricing graph has the depot as its source and,
again, as its sink.

```python
import highspy

from rcspp import presets
from rcspp.column_generation import (
    ColumnGeneration,
    ConvexityRow,
    GraphPricer,
    HighspyMasterSolver,
    MasterProblem,
)
from rcspp.graph import ResourceGraph

capacity = 11.0
demands = [4.0, 5.0, 6.0]  # customers 1, 2 and 3
distances = [  # 0 is the depot
    [0.0, 10.0, 10.0, 10.0],
    [10.0, 0.0, 4.0, 5.0],
    [10.0, 4.0, 0.0, 6.0],
    [10.0, 5.0, 6.0, 0.0],
]
customers = len(demands)
sink = customers + 1  # the depot again, as the end of a route

# The pricing graph: cost, load, and each customer at most once per route.
graph = ResourceGraph()
presets.add_cost_resource(graph)
presets.add_capacity_resource(graph, "real", capacity)
presets.add_elementary_resource(graph, "bitset", range(sink + 1))

graph.add_node(0, source=True)
for customer in range(1, customers + 1):
    graph.add_node(customer)
graph.add_node(sink, sink=True)

for origin in range(customers + 1):
    for destination in range(1, sink + 1):
        place = 0 if destination == sink else destination
        if place == origin:
            continue
        distance = distances[origin][place]
        demand = 0.0 if destination == sink else demands[destination - 1]
        # An arc into customer j covers master row j - 1.
        rows = [] if destination == sink else [(destination - 1, 1.0)]
        graph.add_arc((distance, demand, set()), origin, destination, distance, rows)

# The master: one row per customer, "visited exactly once", empty for now.
model = highspy.Highs()
model.setOptionValue("output_flag", False)
for customer in range(customers):
    model.addRow(1.0, 1.0, 0, [], [])

master = MasterProblem(HighspyMasterSolver(model))
master.add_pricer(GraphPricer(graph), ConvexityRow(max_paths=2))  # at most two vehicles
loop = ColumnGeneration(master)
result = loop.run()

print(result.status, result.objective)  # ColumnGenerationStatus.Optimal 37.5
```

With Gurobi, only the master's model changes:

<!-- docs test: gurobipy master -->

```python
import gurobipy as gp

from rcspp.column_generation import GurobipyMasterSolver

model = gp.Model()
model.Params.OutputFlag = 0
for customer in range(customers):
    model.addLConstr(gp.LinExpr(), gp.GRB.EQUAL, 1.0)

master = MasterProblem(GurobipyMasterSolver(model))
```

#### Reading the solution

The master keeps every column with the path it came from. Column values are read by id:

```python
import numpy as np

from rcspp.column_generation import ColumnTag

ids = np.array([column.id for column in master.columns], dtype=np.uint64)
for column, value in zip(master.columns, master.solver.primal(ids)):
    if column.tag == ColumnTag.Real and value > 1e-6:
        path = list(column.solution.path_node_ids)
        print(f"{value:.2f} x {path}, cost {column.solution.column.cost}")
# 0.50 x [0, 1, 2, 4], cost 24.0
# 0.50 x [0, 1, 3, 4], cost 25.0
# 0.50 x [0, 2, 3, 4], cost 26.0
```

The LP optimum uses half of three routes. An integer plan over the columns generated so far
comes from the master's MIP. It is a heuristic: the best plan may need a column that column
generation never produced.

```python
from rcspp.column_generation import IntegerParams

loop.solve_restricted_master_integer(IntegerParams(timeout_s=10.0))
print(master.solver.objective())  # 44.0: routes {1, 2} and {3}
```

When the run ends, your model holds the generated columns, and you can keep working with it in
highspy or gurobipy.

### The pieces

| class | role |
|---|---|
| `MasterProblem` | the master's rows, its pricers and the columns generated so far |
| `HighspyMasterSolver`, `GurobipyMasterSolver` | solve the master LP over your model and read its duals |
| `GraphPricer` | prices a `ResourceGraph`: writes the reduced costs into its arcs, then searches |
| `PricingStage` | one search of a graph pricer: an algorithm and its parameters |
| `ColumnGeneration` | the loop: solve the master, price, add columns, until convergence or a limit |
| `ColumnGenerationParams` | the run's limits, tolerances, initialization and hooks |

`MasterSolver` and `Pricer` are abstract base classes: subclass them to use another LP solver or
another pricing method ([Your own components](#your-own-components)).

### The master is your model

You build the master's rows in your LP solver, with their names and anything else you need:
slack variables, starting columns, other constraints. The master solver adds the generated
columns to that model; variables it did not add are left alone.

- **Rows:** every row of the model is a master row by default, in the model's order; pass
  `rows=` to the master solver to use only some of them (row indices for highspy, constraints
  for gurobipy). `MasterProblem.add_rows` and `add_rows_between` append rows to the model.
- **Duals:** one per master row, with one convention for both solvers:
  `π_i = d(objective)/d(rhs_i)`, so a `>=` row has `π >= 0` and a `<=` row `π <= 0`. A
  maximizing model works too: the master solver negates its costs and duals, so that the master
  is always seen as a minimization.
- **During a run** the model belongs to the master solver: integer variables are relaxed for
  the LP solves, and the solver options a solve sets (time limit, threads, output) are put back
  after it. When the run ends, by an exception too, the model gets your settings back.
- **Ctrl-C** stops a run with `KeyboardInterrupt`, the model restored. HiGHS does not stop
  inside an LP solve: the interrupt arrives when that solve returns.

#### Convexity rows

A pricer's convexity row bounds how many of its paths the solution uses, e.g. the fleet size:

<!-- docs test: fragment -->

```python
master.add_pricer(pricer, ConvexityRow(max_paths=2))     # the master adds "sum of paths <= 2"
master.add_pricer(pricer, ConvexityRow(existing_row=3))  # row 3 of your model
master.add_pricer(pricer)                                # no convexity row
```

Each column of the pricer gets a 1 in its convexity row. Without one, any number of paths may be
used.

### Pricing

`GraphPricer(graph, stages)` prices a `ResourceGraph` whose arcs carry the master rows they cover
(the `rows` argument of `add_arc`, or `add_rows_to_arc`). The graph's first resource is the real
cost; the pricer writes the reduced costs into it at each call.

A pricer runs its stages in order, and stops at the first one that finds a column or proves that
none exists. Cheap heuristic stages come first, an exact one last:

```python
from rcspp.column_generation import PricingStage
from rcspp.graph import AlgorithmParams

tabu = AlgorithmParams()
tabu.max_solutions = 20     # stop after 20 columns
tabu.max_iterations = 1000  # tabu needs a finite number of iterations

pricer = GraphPricer(graph, [PricingStage("tabu", tabu), PricingStage("simple")])
```

| stage | finds | proves that no improving column exists |
|---|---|---|
| `"greedy"`, `"tabu"` | improving paths, fast | no |
| `"simple"` and the other exact algorithms | the best paths | yes, when the search finishes |

A run ends `Optimal` only when an exact stage finished and found nothing. A stage also takes the
other arguments of `ResourceGraph.solve`: its `direction` (`"forward"`, `"backward"`,
`"bidirectional"`) and the algorithm's parameters (see [Algorithms](algorithms.md)). For a
search of your own, pass `search=` and say whether it is `exact`.

Exact pricing gets slow when the duals are poor, at the start of a run in particular: a heuristic
stage before it often saves most of that time.

### Starting the master

The first master LP needs a feasible solution before any column exists. By default
(`ArtificialColumns`), the master gets one costly artificial column per row that needs one;
pricing then brings real columns that push them out. Their cost is automatic, 10⁴ times the
scale of the pricers' costs, or set it yourself:

```python
from rcspp.column_generation import ArtificialColumns, ColumnGenerationParams

params = ColumnGenerationParams(initialization=ArtificialColumns(cost=1e5))
```

A run that converges with an artificial column still in use ends `Infeasible`: no solution
exists **at that cost** (`result.statistics.artificial_cost`). A solution whose columns cost
more per row could still exist; raise the cost if that can happen.

If your model is feasible as it is (starting columns, slack variables), skip the initialization
with `initialization="none"`. Starting columns of a pricer go through the master, which gives
them their convexity entry:

<!-- docs test: fragment -->

```python
k = master.add_pricer(GraphPricer(graph))
master.add_columns(k, starting_routes)  # Solutions, each with its column
params = ColumnGenerationParams(initialization="none")
```

### Options and hooks

```python
from rcspp.column_generation import ColumnGenerationParams, RelaxationParams

params = ColumnGenerationParams(
    max_iterations=500,                           # LP solves
    timeout_s=60.0,                               # wall clock, LP and pricing
    relaxation=RelaxationParams(timeout_s=5.0),   # options of every LP solve
    tolerances="relative",                        # or "absolute" (the default), or an object
    on_iteration=lambda info: print(info.iteration, info.objective, info.new_columns),
)
result = ColumnGeneration(master, params).run()
```

`on_converged` is called after each proven convergence with the master, its duals and its column
values. It returns `ConvergenceAction.Continue` after changing something (adding rows, changing a
pricer's graph) to solve and price again, or `ConvergenceAction.Stop`. `run()` may also be called
again on the same loop after such changes.

#### How a run ends

| `result.status` | meaning |
|---|---|
| `Optimal` | no improving column exists, and no artificial column is used |
| `NotProven` | no new column, but a pricer did not prove it found the best ones |
| `Infeasible` | converged with an artificial column in use: no solution at the artificial cost |
| `RestrictedMasterInfeasible` | the master LP has no solution with its columns so far (no initialization) |
| `Unbounded` | the master LP is unbounded: look for a missing convexity row or bound |
| `TimeLimit`, `IterationLimit` | stopped by `timeout_s` or `max_iterations` |
| `Cutoff` | stopped by `cutoff`: nothing here costs less than the cutoff |
| `AbsoluteGapLimit`, `RelativeGapLimit` | stopped by `absolute_gap` or `relative_gap`: the bound is close enough |
| `IntegerObjectiveGap` | stopped by `integer_objective`: the bound rounded up reached the LP value rounded up |
| `SolverError` | an LP solve failed for another reason |

`result.objective` is the last master LP value, `result.lower_bound` a proven lower bound (below),
and `result.statistics` holds the number of columns generated and the time spent in LP solves and
in pricing.

#### The lower bound and early stops

Each master LP value is an upper bound on the LP optimum over every column: the master only has
some of them. Pricing gives a lower bound too, the Lagrangian bound. Any solution's cost is the LP
value plus each column's reduced cost times its value; a pricer's columns each cost at least its
best reduced cost, and its convexity row limits how many a solution uses. So

```text
lower bound = LP value
            + sum over the pricers of (convexity row's upper bound) * min(0, best reduced cost)
```

`IterationInfo.lower_bound` is this bound at each iteration, and `result.lower_bound` the best
one of the run (the LP value when `Optimal`). It is `-inf` when a pricer gives no bound (a
heuristic stage, a search stopped early: only a finished exact search knows its best reduced
cost) or has no convexity row with an upper bound. A pricer written in Python gives its bound in
`PricingResult.min_reduced_cost_bound`.

The bound lets a run stop before the last iterations, which often improve the LP value very
little:

<!-- docs test: fragment -->

```python
params = ColumnGenerationParams(
    relative_gap=0.001,       # stop when the LP value is within 0.1 % of the bound
    absolute_gap=0.5,         # or within 0.5
    integer_objective=True,   # costs are integers: the bound rounds up
    cutoff=1250.0,            # a known solution costs 1250: stop once nothing here is cheaper
)
```

All are off by default and are checked after an iteration that added columns and has a bound, in
this order: `cutoff` (status `Cutoff`), `absolute_gap` (`AbsoluteGapLimit`), `relative_gap`
(`RelativeGapLimit`), `integer_objective` (`IntegerObjectiveGap`); the first one reached gives
the status. Each leaves the last LP value in `result.objective`: the LP optimum lies between
`result.lower_bound` and it. With `integer_objective`, the run stops once the bound
rounded up reaches the LP value rounded up: more iterations could not change what the LP proves
about an integer solution. A `cutoff` is the cost of a solution already known, from a heuristic
or another search: once the bound reaches it, no solution here is cheaper, which proves the known
one optimal or, in a branch-and-price tree, prunes a node.

#### What each iteration did

`on_iteration` receives an `IterationInfo` after every iteration. The iteration number, the LP
value, the bound, the columns added and in the master, and the time since the start are always
there. `StatisticsParams` adds more, each group behind its own switch, and leaves its fields
`None` when off:

| switch | `IterationInfo` fields | cost per iteration |
|---|---|---|
| `timers` (on by default) | `relaxation_seconds`, `pricing_seconds` | two clock reads each |
| `pricers` | `pricers`: one `PricerIterationInfo` per pricer (status, columns, best reduced cost, time, and the stages a `GraphPricer` ran) | one clock read per pricer and stage |
| `search` (needs `pricers`) | in each stage: the labels the search extended, the bidirectional search's half-way point and join | none more |
| `duals` | `duals`, one per row | one copy |
| `master_values` | `master_values`: the ids and values of the columns the LP solution uses, read before the new columns go in | one read of every column's value |
| `columns` | `added_columns`: the iteration's new columns, each with its path | none more |

The time of the layer's own work is the elapsed time's increase minus the LP and pricing times.
A pricer written in Python fills `PricingResult.search_details` when the request asks for it
(`PricingRequest.search_details`), or leaves it empty.

`log_frequency` logs a line every that many iterations, and the last one, then how the run ended,
to the logger `rcspp.column_generation` at level INFO:

<!-- docs test: fragment -->

```python
import logging

from rcspp.column_generation import StatisticsParams

logging.basicConfig(level=logging.INFO)
params = ColumnGenerationParams(
    log_frequency=10,
    statistics=StatisticsParams(pricers=True, duals=True),
    on_iteration=lambda info: print(info.iteration, info.pricers[0].min_reduced_cost),
)
```

#### Checking a master solver or a pricer

A master solver or a pricer of your own can be wrong in ways that still give a run that ends: duals
of the wrong sign, a pricer that applies them to the wrong rows. `DebugChecks` checks each answer
against the others, after every LP solve or pricing call, and raises `DebugCheckError` (with the
check's name and the iteration) at the first that fails. Each check is off by default:

| switch | what must hold | cost per iteration |
|---|---|---|
| `pricer_reduced_costs` | each new column's reduced cost (`Solution.cost`) is its cost minus the duals of its rows (the convexity row's left out) | the new columns' entries |
| `dual_signs` | a `<=` row's dual is at most 0, a `>=` row's at least 0, a row with no bound's 0 | one pass over the rows |
| `master_reduced_costs` | no column of the master has a negative reduced cost, and those the LP solution uses have 0 | every entry of the master, one read of every column's value |
| `objective` | the LP value is the cost of its columns' values; checked only when your own variables cost nothing | one read of every column's value |

They all allow the run's reduced-cost tolerance, the larger of the tolerances' `reduced_cost` and
the master solver's `dual_tolerance()`, scaled by the size of the costs and duals involved. A row
with two different finite bounds is not checked by `dual_signs`, as its sign depends on the bound
the LP reaches. Variables you add to the master's model yourself are not seen by
`master_reduced_costs`, and a constant in your model's objective fails `objective`.

<!-- docs test: fragment -->

```python
from rcspp.column_generation import DebugChecks

params = ColumnGenerationParams(
    debug_checks=DebugChecks(pricer_reduced_costs=True, dual_signs=True, master_reduced_costs=True)
)
```

### Several pricers

Each pricer is one block of the master, e.g. one vehicle type: give each its own graph and its own
convexity row. Columns know their pricer (`MasterColumn.pricer`).

<!-- docs test: fragment -->

```python
large = master.add_pricer(GraphPricer(large_vehicle_graph), ConvexityRow(max_paths=1))
small = master.add_pricer(GraphPricer(small_vehicle_graph), ConvexityRow(max_paths=4))
```

### Your own components

- **A pricer:** subclass `Pricer` and implement `price(duals, request)`, returning a
  `PricingResult` with the new columns (`Solution`s with their `column`) and a `PricingStatus`.
  Only an exact call (`exact_stage=True`) that ends `Optimal` or `Infeasible` proves that no
  improving column exists. Its `min_reduced_cost_bound`, a lower bound on the reduced cost of any
  of its columns without the convexity dual, gives the lower bound; leave it `-inf` when unknown.
- **A master solver:** subclass `MasterSolver` for another LP solver. The loop trusts its
  answers, so check it with `master_solver_self_test(minimizing, maximizing)`: it solves small
  LPs with known optima, duals and integer solutions through the interface, and reports every
  check that failed.
- **Initializations and tolerances by name:** `register_initialization` and
  `register_tolerances` make your own classes usable as names in `ColumnGenerationParams`, e.g.
  in configuration files and benchmark grids.

Every class and option is described in its docstring, e.g.
`help(rcspp.column_generation.GraphPricer)`.

### A complete example: vehicle routing

`examples/python/` solves the vehicle routing problem with time windows with this package: it reads a
Solomon instance, builds the pricing graph (distance, time windows, capacity), runs column
generation over a highspy or gurobipy master, then picks an integer set of routes among those
generated. From the repository's root, with rcspp and one of the two LP solvers installed:

```bash
python examples/python/solve_vrp.py instances/C101_25.txt --master highspy
```

Its README, `examples/python/vrp/README.md`, describes the options, the output, and what to change
for elementary routes, a heuristic pricing stage or a fleet limit.

### The C++ example

The repository also has the same column generation written in C++ over the C++ library, with
Gurobi and HiGHS master solvers and Python bindings: `examples/column_generation/`. It is an
example to read or copy, not part of the library or of the Python package; its README tells how
to build it and run its tests.

The vehicle routing example above is also written in C++ on it, in `examples/cpp/`: its README,
`examples/cpp/vrp/README.md`, tells how to build and run it.

---

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
