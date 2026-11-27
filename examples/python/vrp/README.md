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

| option                           | meaning                                                                               |
| -------------------------------- | ------------------------------------------------------------------------------------- |
| `--master highspy` or `gurobipy` | the master's LP solver; required, there is no default                                 |
| `--max-solutions N`              | the most routes a pricing call returns; all improving routes found by default         |
| `--timeout S`                    | column generation's time limit, in seconds; none by default                           |
| `--integer`, `--no-integer`      | solve the integer plan after the LP, or not; on by default                            |
| `--integer-timeout S`            | the integer solve's time limit; 60 s by default                                       |
| `--subproblem-only`              | price once with zero duals and print the routes found: no master, no LP solver needed |

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

## Several vehicle types

`solve_vehicle_types.py` solves the same problem with a fleet of several vehicle types: one pricing graph and one
pricer per type, all over the same customer rows. The master chooses the routes, and so the fleet: how many vehicles
of each type are worth using.

```bash
python examples/python/solve_vehicle_types.py C101_25 --master highspy
python examples/python/solve_vehicle_types.py C101_25 --master highspy --fleet three-sizes
python examples/python/solve_vehicle_types.py R101_25 --master gurobipy --vehicle-type "truck capacity=100% fixed=100 per-distance=1.2" --vehicle-type "van capacity=30% fixed=20 max=4"
```

A route costs its vehicle's fixed cost (paid once per route: what using one more vehicle costs, whatever it drives)
plus its cost per distance times the distance. Each `--vehicle-type` is a name and its settings:

| setting        | meaning                                                                                | default        |
| -------------- | -------------------------------------------------------------------------------------- | -------------- |
| `capacity`     | the most load a route carries: a number, or a share of the instance's capacity (`30%`) | the instance's |
| `fixed`        | the cost of each route                                                                 | 0              |
| `per-distance` | the cost of each unit of distance                                                      | 1              |
| `max-distance` | the longest route                                                                      | none           |
| `max`          | the most vehicles of the type in the plan, a convexity row                             | none           |

Without `--vehicle-type`, a ready-made fleet, `--fleet`:

| fleet                     | types                                                                              |
| ------------------------- | ---------------------------------------------------------------------------------- |
| `single`                  | one vehicle, the instance's capacity: the plain VRP, the same LP as `solve_vrp.py` |
| `truck-and-van` (default) | a truck (full capacity, fixed 100, 1.2 per distance) and a van (30 %, fixed 20)    |
| `three-sizes`             | the same truck and van, and a bike (10 %, fixed 10, routes up to 60 long)          |

The other options are `solve_vrp.py`'s. The output lists the types, the iterations, then the plan's fleet and each
route with its type:

```text
C101_25: 25 customers
  truck: capacity 200, fixed cost 100, 1.2 per distance
  van: capacity 60, fixed cost 20, 1 per distance
  iteration    1  LP      1632.1979  +  182 routes      0.01 s
  ...
Optimal: LP 475.0388, lower bound 475.0388
14 iterations, 711 routes generated, 0.06 s (master 0.02 s, pricing 0.04 s)
Integer solve: Optimal, cost 485.7166 (0.11 s)
Fleet: 2 truck, 2 van
       truck  cost   215.0617  distance   95.8847  load    190  [13, 17, 18, 19, 15, 16, 14, 12]
       truck  cost   171.3859  distance   59.4882  load    160  [5, 3, 7, 8, 10, 11, 9, 6, 4, 2, 1]
         van  cost    54.2426  distance   34.2426  load     60  [24, 25, 23]
         van  cost    45.0265  distance   25.0265  load     50  [20, 22, 21]
```

The larger vehicle pays off only where its load saves routes: on C101_25 the plan mixes trucks and vans; on R101_25,
whose time windows keep the routes short and light, it takes vans only.

The starting routes serve one customer each, on the cheapest type without a vehicle limit that can drive it. When a
customer has none (every type that can serve it is limited), the master starts from artificial columns instead.

## Files

| file                        | contents                                                                                                                                                                             |
| --------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `../solve_vrp.py`           | the command line: reads the instance, runs column generation, prints                                                                                                                 |
| `../solve_vehicle_types.py` | the command line with several vehicle types                                                                                                                                          |
| `instance.py`               | `read_solomon`: the instance as `Instance` and `Customer` dataclasses                                                                                                                |
| `graph.py`                  | `pricing_graph`: the resources (cost, time, load, and the distance when routes have a maximum), the nodes and the arcs, each arc with its master row; a vehicle's capacity and costs |
| `column_generation.py`      | the master over highspy or gurobipy, the starting routes, the loop and the integer plan                                                                                              |
| `vehicle_types.py`          | `VehicleType`, reading one from text, the fleets, the master with one pricer per type and the plan by type                                                                           |

The tests are in `tests/python/test_vrp.py` and `tests/python/test_vrp_vehicle_types.py`.

## Going further

- **Elementary routes.** Nothing keeps a route from visiting a customer twice: the search is fast, the bound weaker.
  `rcspp.presets.add_elementary_resource` forbids it, and `add_ng_path_resource` forbids it only near each customer, a
  middle ground. Add one in `pricing_graph`, with its value on each arc.
- **A heuristic stage before the exact search.** Exact pricing is slow while the duals are poor. In
  `column_generation`, give the `GraphPricer` a `PricingStage("tabu", params)` before the `"simple"` one, with a
  finite `params.max_iterations`: the exact search then runs only when the heuristic finds nothing.
- **A fleet limit.** The instance's number of vehicles is not used. `problem.add_pricer(pricer,
ConvexityRow(max_paths=instance.vehicles))` adds the row "at most that many routes"; `solve_vehicle_types.py`
  does so for each type with a `max`.
- **The master's model.** `master_solver` builds a plain highspy or gurobipy model: add rows, variables or solver
  options there before the run.
