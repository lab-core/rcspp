# API sketch: what a user writes (proposal, nothing implemented)

Purpose: test the architecture (`architecture.md`) against concrete user code. Graph calls are today's real API;
everything under `rcspp.column_generation` / `rcspp::column_generation` is proposed. Names follow `D-12`.
Running example: VRPTW with `n` customers, master row `i` = "customer `i` covered once".

## 1. Python, minimal

```python
from rcspp.graph import ResourceGraph
from rcspp.column_generation import (ColumnGeneration, ColumnGenerationParams, GraphPricer, MasterProblem,
                                     PricingStage, PricingStrategy, RowSense)

master = MasterProblem()
cover = master.add_rows(count=n, sense=RowSense.EQUAL, rhs=1.0)    # -> row indices [0, n)

graph = ResourceGraph()                                            # today's API, unchanged
graph.add_real_resource(...)                                       # cost, time, load: as in the VRP example
for i, j, dist, time, load in arcs:
    rows = [(cover[j], 1.0)] if is_customer(j) else []             # D-01: the user maps arcs to master rows
    graph.add_arc((dist, time, load), i, j, cost=dist, rows=rows)

pricer = GraphPricer(graph, strategy=PricingStrategy([
    PricingStage("tabu", params=tabu_params),                      # heuristic: may stop the round early
    PricingStage("pulling", exact=True),                           # only an exact stage can prove (P-05)
]))
master.add_pricer(pricer, max_paths=n_vehicles)                    # D-13: convexity row managed by the master

cg = ColumnGeneration(master, backend="highs", params=ColumnGenerationParams(time_limit_s=600))
result = cg.run()

result.status            # OPTIMAL | TIME_LIMIT | NOT_PROVEN (a pricing round couldn't prove) | INFEASIBLE
result.objective         # LP value of the last restricted master
result.lower_bound       # best Lagrangian bound (= objective when proven)
result.statistics        # tier 0 + 1 (architecture.md), numpy-friendly
for column, value in result.primal_columns(): ...                 # Solution objects with x_p > 0
integer = cg.solve_restricted_master_integer(time_limit_s=60)     # heuristic plan (theory §5.2), needs MIP support
```

## 1b. Python, with the user's own model (`D-35`, the main Python path)

```python
model = gp.Model()
cover = [model.addConstr(gp.LinExpr() == 1, name=f"cover[{i}]") for i in customers]
fleet = model.addConstr(gp.LinExpr() <= n_vehicles)
master = GurobipyMaster(model, rows=cover)                 # Row{i} on the arcs <-> cover[i]
master.add_pricer(pricer, convexity_row=fleet)             # instead of max_paths=...
result = ColumnGeneration(master, params=ColumnGenerationParams(time_limit_s=600)).run()
model.optimize()                                           # the user's model now holds the generated columns
```

## 2. C++, same model

```cpp
#include "rcspp/rcspp.hpp"
#include "rcspp/column_generation/column_generation.hpp"
#include "rcspp_backends/highs/highs_backend.hpp"
namespace column_generation = rcspp::column_generation;

column_generation::MasterProblem master;
auto cover = master.add_rows(n, column_generation::RowSense::Equal, 1.0);

ResourceGraph<RealResource, RealResource, IntResource> graph;      // today's API
// ... add_resource / add_node / add_arc(..., {Row{cover[j], 1.0}})

column_generation::GraphPricer pricer(graph, column_generation::PricingStrategy{}
    .add_stage<ImprovingTabuSearch>(tabu_params)
    .add_exact_stage<PullingDominanceAlgorithm>(exact_params));
master.add_pricer(pricer, /*max_paths=*/n_vehicles);

column_generation::HighsBackend backend;
column_generation::ColumnGeneration solver(master, backend, {.time_limit_s = 600});
auto result = solver.run();
```

The templates (`GraphPricer<R...>`, algorithm types) stay inside the pricer. The loop only sees the type-erased
`Pricer` (`F-03`), which is what makes the Python binding possible.

## 3. Configuration by name (benchmark grids, `D-03`)

```python
params = ColumnGenerationParams.from_dict({
    "backend": "highs",                                    # or "gurobi", or an object
    "stabilization": {"smoothing": {"alpha": "auto"}},     # or "none" (default), D-25
    "pool": {"reprice_first": True, "max_age": 50},
    "statistics": {"timers": True, "trace": False},        # tiers (architecture.md)
})
```

Each component kind has a **registry** of named implementations with a default. The same names exist in C++.
A benchmark run = one dict + one instance, so a run's config can be stored next to its results.

## 4. Branch-and-price (phase 3, designed only; or SCIP-hosted, `D-32`)

```python
bp = BranchAndPrice(cg, branching="arc", node_selection="best_bound",
                    primal_heuristic={"restricted_master_integer": {"every_nodes": 50}})
result = bp.run()        # result.incumbent, result.lower_bound, result.gap, result.nodes
```

Underneath, arc branching uses today's `graph.remove_arcs` / `restore_arcs` and pool filters (`P-16`).

## 5. A custom backend in Python (`D-14`)

```python
class GurobipyBackend(SolverBackend):           # pybind11 trampoline: C++ loop calls these methods
    def add_rows(self, lower, upper): ...                                      # ranges (D-30)
    def add_columns(self, costs, lower, upper, starts, indices, values): ...   # CSC as numpy (D-16, D-30)
    def solve_linear_relaxation(self, params): ...                          # returns objective, primal, duals
    def capabilities(self): return Capabilities(integer=True, warm_start=True)

run_backend_self_test(GurobipyBackend())       # dual signs, ranges, add/remove columns (D-30)
cg = ColumnGeneration(master, backend=GurobipyBackend(), params=params)
```

## 6. What happens underneath (worth knowing)

- **Convexity rows aren't on the arcs.** The master adds coefficient 1 in row `k` to every column from pricer `k`.
  Its dual `μ_k` isn't subtracted on any arc, so a path is improving iff `graph reduced cost − μ_k < 0`. The
  pricer therefore calls `solve(upper_bound = μ_k − ε)` instead of `−ε`. One subtle point the sketch exposed.
- **Columns** are `Solution.column` as-is (`F-01`, `F-02`), plus the convexity coefficient. No domain code.
- **Each pricer returns a status and a bound** on its best reduced cost (`D-21`), so `result.lower_bound` exists
  even when a heuristic or a time limit cut pricing short.
- **No starting columns needed:** the default `MasterInitialization` adds big-M artificial columns (`D-45`).
- **Hooks**: `params.on_iteration = callback` (Python or C++), called once per iteration (`architecture.md`).

## 7. Open points raised by this sketch → `questions.md`

| point | Q |
|---|---|
| Who owns `graph`, `backend`, `master`? References (C++ above) vs shared ownership; Python lifetimes | Q-15 |
| No initial columns: artificial columns (big-M cost) vs Farkas pricing | Q-16 → D-26 |
| Components by name (registry, strings) and by object: both? | Q-17 |
| Tolerances: absolute `−1e-9` vs relative to cost size; one place to set them | Q-18 |
| `add_rows(count=n)` returns a range; named rows (`"cover[7]"`) for debugging, at what cost? | Q-19 |
