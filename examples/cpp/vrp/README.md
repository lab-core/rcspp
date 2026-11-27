# C++ VRP example

Column generation for the vehicle routing problem with time windows (VRPTW), in C++, with the C++ column generation
example ([`examples/column_generation/`](../../column_generation/README.md)). It reads an instance in Solomon's format,
solves the LP relaxation of the route-choice model, then picks an integer set of routes among those it generated. It is
the [Python VRP example](../../python/vrp/README.md) written in C++, file for file.

- **Master:** one row per customer, "visited exactly once"; one column per route, costing its distance.
- **Pricing:** a route is a path from the depot back to the depot in rcspp's `ResourceGraph`, within the vehicle's
  capacity and each customer's time window. Each iteration, the duals of the master become reduced costs on the arcs,
  and rcspp's exact search returns the routes with a negative reduced cost.
- **Integer plan:** the master with integer columns, over the routes generated. It is a heuristic: the best plan may
  need a route that column generation never produced. Branch-and-price would find it.

## Build

From the repository's root, with the column generation example and at least one of its master solvers:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DUSE_COLUMN_GENERATION_EXAMPLE=ON \
    -DUSE_COLUMN_GENERATION_EXAMPLE_HIGHS=ON
cmake --build build --config Release --target rcspp-vrp
```

| option | master |
|---|---|
| `-DUSE_COLUMN_GENERATION_EXAMPLE_HIGHS=ON` | `highs`: HiGHS, open source, found installed or else fetched (v1.15.1) |
| `-DUSE_COLUMN_GENERATION_EXAMPLE_GUROBI=ON` | `gurobi`: Gurobi's C++ API; needs Gurobi installed and a licence (free for academics) |

Both can be on. The program is `build/bin/rcspp-vrp` (`build/bin/Release/rcspp-vrp.exe` with Visual Studio). Without
a master solver, CMake skips the program and says so.

## Run

```bash
build/bin/rcspp-vrp instances/C101_25.txt --master highs
build/bin/rcspp-vrp C101_25 --master gurobi --no-integer
build/bin/rcspp-vrp R101_25 --master highs --relaxation ng --ng-size 8
build/bin/rcspp-vrp C101_5 --subproblem-only
```

An instance is a file, or the name of one in `instances/`.

| option | meaning |
|---|---|
| `--master highs` or `gurobi` | the master's LP solver; required, there is no default; only those built |
| `--max-solutions N` | the most routes a pricing call returns; all improving routes found by default |
| `--timeout S` | column generation's time limit, in seconds; none by default |
| `--integer`, `--no-integer` | solve the integer plan after the LP, or not; on by default |
| `--integer-timeout S` | the integer solve's time limit; 60 s by default |
| `--relaxation R` | the routes priced: `none` (a route may visit a customer twice), `ng` or `elementary`; `none` by default |
| `--ng-size N` | with `ng`: how many nearest customers each customer's neighbourhood holds, itself included; 8 by default |
| `--subproblem-only` | price once with zero duals and print the routes found: no master, no LP solver needed |
| `--help` | the options |

## Output

```text
C101_25: 25 customers, capacity 200, 27 nodes, 650 arcs
  iteration    1  LP      1132.1979  +  100 routes      0.01 s
  iteration    2  LP       459.8162  +   82 routes      0.01 s
  ...
  iteration   21  LP       191.8136  +    6 routes      0.04 s
  iteration   22  LP       191.8136  +    0 routes      0.04 s
Optimal: LP 191.8136, lower bound 191.8136
22 iterations, 620 routes generated, 0.04 s (master 0.02 s, pricing 0.02 s)
Integer solve: Optimal, 3 routes, distance 191.8136 (0.10 s)
     95.8847  [13, 17, 18, 19, 15, 16, 14, 12]
     59.4882  [5, 3, 7, 8, 10, 11, 9, 6, 4, 2, 1]
     36.4407  [20, 24, 25, 23, 22, 21]
```

- **The graph:** the depot is node 0 and again node 26, the sink, where a route ends.
- **One line per iteration:** the master's LP value, the routes pricing added and the time so far. The first LP is
  high: it starts from one route per customer.
- **`Optimal`:** the last pricing proved that no route has a negative reduced cost, so the LP value is the relaxation's
  optimum and also a lower bound on any plan's distance. The other endings (`TimeLimit`, `NotProven`...) are described
  in `examples/column_generation/cpp/rcspp/column_generation/column_generation.hpp`.
- **The integer plan:** each route's distance and its customers in the order visited. Here it reaches the LP's
  value, so it is optimal; on most instances it is above.

The number of iterations and of routes depends on the LP solver: HiGHS and Gurobi can give different duals at the
same LP optimum, and pricing then finds other routes (29 iterations and 903 routes with Gurobi here). The LP value is
the same. With the same LP solver, the Python example gives the same numbers.

With `--relaxation ng` or `elementary`, pricing searches routes that do not come back to a customer (`elementary`) or
not to one still in the route's memory (`ng`): the LP value can only rise, toward the best integer plan, and each
pricing call costs more. `elementary` is slow beyond small instances; `ng` with a small neighbourhood is the usual
middle ground.

## Files

| file | contents |
|---|---|
| `../solve_vrp.cpp` | the command line: reads the instance, runs column generation, prints |
| `instance.hpp`, `instance.cpp` | `read_solomon`: the instance as plain `Instance` and `Customer` structs |
| `graph.hpp`, `graph.cpp` | `pricing_graph`: the resources (distance, time, load, a route memory), the nodes and the arcs, each arc with its master row |
| `column_generation.hpp`, `column_generation.cpp` | the master over a Gurobi or HiGHS model, the starting routes, the master and its loop, the integer plan |

The tests are in `../tests/`: `tests-vrp`, and the command line through ctest (`ctest -R "Vrp|vrp\."`).

## Going further

- **A heuristic stage before the exact search.** Exact pricing is slow while the duals are poor. In
  `column_generation.cpp`, give the pricing strategy a
  `PricingStage::make<rcspp::strategy::ImprovingTabu>(params, false)` before the exact stage, with a finite
  `params.max_iterations`: the exact search then runs only when the heuristic finds nothing.
- **A fleet limit.** The instance's number of vehicles is not used. `problem.add_pricer(pricer,
  ConvexityRow{.max_paths = static_cast<double>(instance.vehicles)})` adds the row "at most that many routes".
- **The master's model.** `master_solver` builds a plain Gurobi or HiGHS model: add rows, variables or solver options
  there before the run.
