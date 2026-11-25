# Examples

Demonstration programs that use the `rcspp` library: a vehicle routing problem solved by column
generation, in C++ and in Python, and a column generation layer in C++. They are not part of the
installable packages.

## C++ (`examples/cpp/`)

[`solve_vrp.cpp`](cpp/solve_vrp.cpp) is the Python example below written in C++: it solves a VRPTW
instance by column generation with the C++ column generation example (`examples/column_generation/`),
over a HiGHS or Gurobi master, then picks an integer set of routes. Build it with the example and one
of its master solvers, then run (from the repository root):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DUSE_COLUMN_GENERATION_EXAMPLE=ON     -DUSE_COLUMN_GENERATION_EXAMPLE_HIGHS=ON
cmake --build build --config Release --target rcspp-vrp
build/bin/rcspp-vrp instances/C101_25.txt --master highs
```

The build options, the program's options (also ng-routes and elementary routes), the output and how
to extend the model: [`cpp/vrp/README.md`](cpp/vrp/README.md).

## Python (`examples/python/`)

[`solve_vrp.py`](python/solve_vrp.py) solves a VRPTW instance by column generation with
`rcspp.column_generation`, over a HiGHS or Gurobi master, then picks an integer set of routes.
Install rcspp with one of the two LP solvers (`pip install ".[highspy]"` or `".[gurobipy]"` from the
repository's root), then run:

```bash
# Column generation, then the integer plan
python examples/python/solve_vrp.py instances/C101_25.txt --master highspy

# One pricing call with zero duals: no LP solver needed
python examples/python/solve_vrp.py instances/C101_5.txt --subproblem-only
```

[`solve_vehicle_types.py`](python/solve_vehicle_types.py) solves it with a fleet of several vehicle types, each
with its capacity, fixed cost, cost per distance and limits: one pricer per type, and the plan says which vehicles
to use:

```bash
python examples/python/solve_vehicle_types.py C101_25 --master highspy --fleet truck-and-van
```

The options, the output and how to extend the model: [`python/vrp/README.md`](python/vrp/README.md).

## C++ column generation (`examples/column_generation/`)

A column generation layer in C++ over rcspp, with master solvers over Gurobi and HiGHS, Python
bindings and tests, built with `-DUSE_COLUMN_GENERATION_EXAMPLE=ON`: see its
[README](column_generation/README.md).
