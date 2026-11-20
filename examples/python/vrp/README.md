# Python VRP example

Column generation for the vehicle routing problem with time windows (VRPTW), with rcspp's column generation package
`rcspp.column_generation`. It reads an instance in Solomon's format, solves the LP relaxation of the route-choice
model, then picks an integer set of routes among those it generated.

- **Master:** one row per customer, "visited exactly once"; one column per route, costing its distance.
- **Pricing:** a route is a path from the depot back to the depot in rcspp's `ResourceGraph`, within the vehicle's
  capacity and each customer's time window. Each iteration, the duals of the master become reduced costs on the arcs,
  and rcspp's exact search returns the routes with a negative reduced cost.
- **Integer plan:** the master with integer columns, over the routes generated. It is a heuristic: the best plan may
  need a route that column generation never produced. Branch-and-price would find it.

## Install

From the repository's root, with one of the two LP solvers for the master:

```bash
pip install ".[highspy]"     # HiGHS, open source
pip install ".[gurobipy]"    # Gurobi: needs a licence (free for academics)
```

Gurobi without a licence solves small models only: the larger instances generate too many routes.

To run on a CMake build instead of an installed package, build the Python module and put its folder first on
`PYTHONPATH`, with highspy or gurobipy installed:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DUSE_PYTHON=ON
cmake --build build --config Release
export PYTHONPATH=build/python     # PowerShell: $env:PYTHONPATH = "build/python"
```

## Run

```bash
python examples/python/solve_vrp.py instances/C101_25.txt --master highspy
python examples/python/solve_vrp.py C101_25 --master gurobipy --no-integer
python examples/python/solve_vrp.py C101_5 --subproblem-only
```

An instance is a file, or the name of one in `instances/`.

| option | meaning |
|---|---|
| `--master highspy` or `gurobipy` | the master's LP solver; required, there is no default |
| `--max-solutions N` | the most routes a pricing call returns; all improving routes found by default |
| `--timeout S` | column generation's time limit, in seconds; none by default |
| `--integer`, `--no-integer` | solve the integer plan after the LP, or not; on by default |
| `--integer-timeout S` | the integer solve's time limit; 60 s by default |
| `--subproblem-only` | price once with zero duals and print the routes found: no master, no LP solver needed |

## Output

```text
C101_25: 25 customers, capacity 200, 27 nodes, 650 arcs
  iteration    1  LP      1132.1979  +  100 routes      0.01 s
  iteration    2  LP       459.8162  +   82 routes      0.01 s
  ...
  iteration   21  LP       191.8136  +    6 routes      0.05 s
  iteration   22  LP       191.8136  +    0 routes      0.05 s
Optimal: LP 191.8136, lower bound 191.8136
22 iterations, 620 routes generated, 0.05 s (master 0.01 s, pricing 0.03 s)
Integer solve: Optimal, 3 routes, distance 191.8136 (0.12 s)
     95.8847  [13, 17, 18, 19, 15, 16, 14, 12]
     59.4882  [5, 3, 7, 8, 10, 11, 9, 6, 4, 2, 1]
     36.4407  [20, 24, 25, 23, 22, 21]
```

- **The graph:** the depot is node 0 and again node 26, the sink, where a route ends.
- **One line per iteration:** the master's LP value, the routes pricing added and the time so far. The first LP is
  high: it starts from one route per customer.
- **`Optimal`:** the last pricing proved that no route has a negative reduced cost, so the LP value is the relaxation's
  optimum and also a lower bound on any plan's distance. Other endings (`TimeLimit`, `NotProven`...) are described in
  `docs/advanced/column-generation.md`.
- **The integer plan:** each route's distance and its customers in the order visited. Here it reaches the LP's
  value, so it is optimal; on most instances it is above.

The number of iterations and of routes depends on the LP solver: HiGHS and Gurobi can give different duals at the
same LP optimum, and pricing then finds other routes. The LP value is the same.

## Files

| file | contents |
|---|---|
| `../solve_vrp.py` | the command line: reads the instance, runs column generation, prints |
| `instance.py` | `read_solomon`: the instance as `Instance` and `Customer` dataclasses |
| `graph.py` | `pricing_graph`: the resources (distance, time, load), the nodes and the arcs, each arc with its master row |
| `column_generation.py` | the master over highspy or gurobipy, the starting routes, the loop and the integer plan |

The tests are in `tests/python/test_vrp.py`.

## Going further

- **Elementary routes.** Nothing keeps a route from visiting a customer twice: the search is fast, the bound weaker.
  `rcspp.presets.add_elementary_resource` forbids it, and `add_ng_path_resource` forbids it only near each customer, a
  middle ground. Add one in `pricing_graph`, with its value on each arc.
- **A heuristic stage before the exact search.** Exact pricing is slow while the duals are poor. In
  `column_generation`, give the `GraphPricer` a `PricingStage("tabu", params)` before the `"simple"` one, with a
  finite `params.max_iterations`: the exact search then runs only when the heuristic finds nothing.
- **A fleet limit.** The instance's number of vehicles is not used. `problem.add_pricer(pricer,
  ConvexityRow(max_paths=instance.vehicles))` adds the row "at most that many routes".
- **The master's model.** `master_solver` builds a plain highspy or gurobipy model: add rows, variables or solver
  options there before the run.
