# docs(solution): Solution::cost and Column::cost descriptions contradict each other and the code

## Summary

The docstring of `Solution::cost` (`cpp/rcspp/algorithm/solution.hpp:103-108`) says it is the "total cost of the path (sum of arc costs)" and that `column.cost` is "the master-LP reduced cost used by the pricing step". The code does the opposite, and the docstring of `Column::cost` (`cpp/rcspp/graph/row.hpp`) already says so correctly. Anyone building a master problem from the `Solution` docs would put reduced costs in the master objective.

## What the code does

- **`Solution::cost`** is the label's cost, as computed by the graph's cost function (`cpp/rcspp/algorithm/algorithm.hpp`, ~712-750: `Solution(cost, ...)`). `update_reduced_costs` writes `arc.cost − Σ coefficient·dual` into the cost *resource* (`cpp/rcspp/resource/resource_graph.hpp`, ~519-527), not into `arc.cost`. So:
  - before any `update_reduced_costs`, `Solution::cost` is the plain path cost (the quickstarts' `25.0` is right);
  - after it, `Solution::cost` is the path's **reduced** cost;
  - with a custom cost function (non-linear cost, several cost components, `cost_index ≠ 0`), it can differ from `Σ arc.cost` even without duals.
- **`Column::cost`** is `Σ arc->cost` over the path (`algorithm.hpp`: `column.cost += arc->cost`): the `cost=` given to `add_arc`, never touched by duals.
- The code already relies on this:
  - `tests/cpp/test_solution_pool.hpp` asserts that `column.cost` keeps the original arc cost after pricing;
  - the VRP examples read `sol.cost` as the reduced cost (`examples/cpp/vrp/vrp.cpp:252`, `examples/python/vrp/vrp.py:210-212`).

## Proposed fix

1. Rewrite the `Solution::cost` docstring along these lines: "Cost of the label at the sink, as computed by the graph's cost function. In column generation, after `update_reduced_costs`, this is the path's reduced cost. The path's original cost (Σ `arc.cost`) is `column.cost`."
2. Add the same meaning to the Python stubs (`python/src/rcspp/_core/graph.pyi`: `Solution.cost`, `Column.cost`), to `AGENT.md` (SolveResult section) and to `docs/advanced/column-generation.md`.
3. Add one test, in C++ and in Python: a two-arc graph with rows, `update_reduced_costs(duals)`, then check `sol.cost == reduced cost` and `sol.column.cost == original cost`.

Optional follow-up, not required: the examples recompute the real path cost by summing arc costs (`VRP::calculate_solution_cost`, `examples/cpp/vrp/vrp.cpp:543`; `examples/python/vrp/vrp.py:172-174`); `sol.column.cost` already holds it.

## Done when

The two docstrings agree with each other and with the code, and the new test passes in C++ and Python.

Checked on `main` @ `129b15e0`.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
