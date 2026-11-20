# Examples

Demonstration programs that use the `rcspp` library and the VRP column-generation
solver. They are not part of the installable packages.

## C++ (`examples/cpp/`)

Benchmark drivers for the VRP solver over the Solomon and Gehring & Homberger
instances under [`../instances/`](../instances). They link the reusable
`rcspp-vrp-lib` target and require Gurobi (and optionally Boost).

Build and run (from the repository root):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DUSE_VRP=ON
cmake --build build --target rcspp-vrp-benchmark
./build/bin/rcspp-vrp-benchmark 2          # 2 instances per C1/R1/RC1 family
```

The large benchmark target is `rcspp-vrp-benchmark-large`.

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

The options, the output and how to extend the model: [`python/vrp/README.md`](python/vrp/README.md).

## C++ column generation (`examples/column_generation/`)

A column generation layer in C++ over rcspp, with master solvers over Gurobi and HiGHS, Python
bindings and tests, built with `-DUSE_COLUMN_GENERATION_EXAMPLE=ON`: see its
[README](column_generation/README.md). A column generation loop written in Python over its bindings is
in [`column_generation/python/column_generation_loop.py`](column_generation/python/column_generation_loop.py);
it needs the example's HiGHS master solver (`-DUSE_COLUMN_GENERATION_EXAMPLE_HIGHS=ON`) and
highspy.
