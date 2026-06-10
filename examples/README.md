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

[`solve_vrp.py`](python/solve_vrp.py) solves a single VRPTW instance. Build the
Python extension first (`cmake -B build -DUSE_PYTHON=ON && cmake --build build`),
then run:

```bash
# RCSPP subproblem only — no Gurobi required
python examples/python/solve_vrp.py instances/C101_5.txt --subproblem-only

# Full column generation — requires Gurobi + mip
python examples/python/solve_vrp.py instances/C101_5.txt
```
