# C++ column generation example

A column generation layer written in C++ over rcspp's pricing engine, with master solvers over Gurobi and HiGHS,
Python bindings and tests. It is an example to read and copy, not part of the library: nothing here is installed, and
the wheel does not have it. In Python, the package's `rcspp.column_generation` is the same layer written in Python.

| folder | contents |
|---|---|
| `cpp/rcspp/column_generation/` | the layer: master problem, loop, pricers, tolerances, initializations; `cpp/rcspp/column_generation.hpp` includes all of it |
| `cpp/rcspp/column_generation/gurobi/`, `highs/` | the master solvers, a library each |
| `bindings/` | the Python bindings, compiled into rcspp's module as `rcspp._core.cpp_column_generation` |
| `python/cpp_column_generation/` | the Python package over those bindings |
| `python/column_generation_loop.py` | the loop written again in Python from the bound pieces |
| `tests/cpp/`, `tests/python/` | the tests |

## Options

| option | builds |
|---|---|
| `USE_COLUMN_GENERATION_EXAMPLE` | the layer, its tests and, with `USE_PYTHON`, its bindings |
| `USE_COLUMN_GENERATION_EXAMPLE_GUROBI` | the Gurobi master solver (needs Gurobi and its C++ library) |
| `USE_COLUMN_GENERATION_EXAMPLE_HIGHS` | the HiGHS master solver, over an installed HiGHS (`-Dhighs_DIR=<dir>`), else HiGHS v1.15.1 fetched |

All are off by default; the master solvers need the first.

## Build with rcspp

From rcspp's root:

```bash
cmake -B build -DUSE_PYTHON=ON -DUSE_COLUMN_GENERATION_EXAMPLE=ON -DUSE_COLUMN_GENERATION_EXAMPLE_HIGHS=ON
cmake --build build --config Release
ctest --test-dir build -C Release
```

ctest runs the example's C++ tests (`tests-column-generation`, and `tests-column-generation-gurobi` or
`tests-column-generation-highs` with their master solvers) with rcspp's.

The bindings are in that build's Python module; the package `cpp_column_generation` is read from this folder. The
Python tests and the loop script put it on the path themselves:

```bash
pip install pytest -r tests/python/requirements.txt
PYTHONPATH=build/python pytest examples/column_generation/tests/python
PYTHONPATH=build/python python examples/column_generation/python/column_generation_loop.py
```

## Build on its own

Over an installed rcspp, as a project that copies the example would:

```bash
cmake --install build --prefix /path/to/prefix
cmake -S examples/column_generation -B build-example -DCMAKE_PREFIX_PATH=/path/to/prefix \
    -DUSE_COLUMN_GENERATION_EXAMPLE_HIGHS=ON
cmake --build build-example --config Release
ctest --test-dir build-example -C Release
```

This builds the C++ layer, its master solvers and their tests; the Python bindings need rcspp's own build (above).
With the example on, rcspp's install check (the `install_and_find_package` test) builds it this way.
