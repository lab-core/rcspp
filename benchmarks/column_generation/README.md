# Column generation benchmark

Runs column generation on rcspp's layers and records how they do. It measures the source tree it lives in: rcspp's
Python sources in `python/src` and the compiled `_core` of one of this tree's builds. Nothing here is installed; the
wheel does not have it.

| layer | what runs |
|---|---|
| `package` | `rcspp.column_generation`, the column generation package rcspp ships |
| `cpp-bindings` | the C++ column generation example of `examples/column_generation`, through its Python bindings |

| path | contents |
|---|---|
| `column_generation_benchmark/` | the code the runners and the benchmark share |
| `tests/` | the tests |
| `OPTIONS.md` | the options of a run and of a benchmark |

## Install

Python 3.11 or later, in a virtual environment:

```bash
pip install -r benchmarks/column_generation/requirements.txt
```

`gurobipy` and a Gurobi licence are needed only for the Gurobi master solvers.

## Build rcspp

The benchmark imports rcspp from a build folder of this tree, `build*/` at its root. Build the Python module there,
with the column generation example for the `cpp-bindings` layer (`examples/column_generation/README.md` lists the
options):

```bash
cmake -B build-python -DCMAKE_BUILD_TYPE=Release -DUSE_PYTHON=ON -DUSE_TESTS=OFF \
    -DUSE_COLUMN_GENERATION_EXAMPLE=ON -DUSE_COLUMN_GENERATION_EXAMPLE_HIGHS=ON
cmake --build build-python --config Release
```

Add `-DUSE_COLUMN_GENERATION_EXAMPLE_GUROBI=ON` when Gurobi is installed, and `-DPython_EXECUTABLE=<python>` when
CMake would otherwise find another Python than the environment's.

The build copies `python/src/rcspp` next to `_core`; build again after changing the Python sources.

## Which rcspp is measured

Before importing rcspp, the benchmark puts the build's `python` folder first on the path, then checks that rcspp and
its `_core` come from that build, and that the build's copy of the Python sources equals `python/src`. It stops
otherwise: a virtual environment may have another rcspp installed, which a run would measure without failing.

## Options

A run is a layer and its blocks (`instance`, `master`, `pricing`, `start`, ...), given in a TOML file, on the command
line, or both; a benchmark file makes many runs. [`OPTIONS.md`](OPTIONS.md) has the words, the TOML they need, where a
run's options come from, examples, and every option's parameters.

## A run's folder

| file | contents |
|---|---|
| `run.toml` | the run's options, expanded: the runner's `--file run.toml` runs it again |
| `command.txt` | the command line that started the run |
| `metadata.json` | start and end; the commit and whether it had uncommitted changes, the build and its compiler, the versions of Python, rcspp, numpy and the solvers, the machine's label, processor, cores and memory |
| `result.json` | `status`, `objective`, `iterations`, `columns`, `artificial_columns`, `master_seconds`, `pricing_seconds`, `other_seconds`, `solve_seconds`, then the fields of the statistics turned on (`process`: `process_peak_memory_bytes`, `process_processor_seconds`) |

`result.json` is written last: a folder that has one holds a finished run.

## Tests

```bash
pytest benchmarks/column_generation/tests
```

The tests find the build as the benchmark does; `RCSPP_BENCHMARK_BUILD=<folder>` chooses one among several. Without a
build, the tests that need rcspp skip.
