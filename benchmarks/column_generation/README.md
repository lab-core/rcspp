# Column generation benchmark

Runs column generation on rcspp's layers and records how they do. It measures the source tree it lives in: rcspp's
Python sources in `python/src` and the compiled `_core` of one of this tree's builds. Nothing here is installed; the
wheel does not have it.

| layer          | what runs                                                                                      |
| -------------- | ---------------------------------------------------------------------------------------------- |
| `package`      | `rcspp.column_generation`, the column generation package rcspp ships                           |
| `cpp-bindings` | the C++ column generation example of `examples/column_generation`, through its Python bindings |

| path                           | contents                                                           |
| ------------------------------ | ------------------------------------------------------------------ |
| `benchmark.py`                 | many runs, each by its runner in its own process                   |
| `runners/`                     | the runners: one run of a layer each                               |
| `column_generation_benchmark/` | the code the runners and the benchmark share                       |
| `tests/`                       | the tests                                                          |
| `results/`                     | where runs go unless told otherwise; ignored by git (`.gitignore`) |
| `OPTIONS.md`                   | the options of a run and of a benchmark                            |

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

## Run once

Each layer has a runner in `runners/`, a script that does one run and writes its folder. `package_runner.py` runs the
`package` layer, `cpp_bindings_runner.py` the `cpp-bindings` layer; a runner's body is the column generation calls,
the place to read how its layer is used. Both take the same flags and options; the run's layer is the runner's.

```bash
cd benchmarks/column_generation
python runners/package_runner.py --instance C101_25 # into results/C101_25-package-highspy-exact-artificial
python runners/package_runner.py --instance R201_25 --master gurobipy --start single-customer
python runners/package_runner.py --file results/C101_25-package-highspy-exact-artificial/run.toml --output results/again
python runners/package_runner.py --instance C101_25 --check build # build, do not solve
python runners/cpp_bindings_runner.py --instance C101_25 --master highs
python runners/cpp_bindings_runner.py --file results/C101_25-package-highspy-exact-artificial/run.toml --layer cpp-bindings
```

`cpp_bindings_runner.py` needs the build to have the column generation example, and its HiGHS part for the masters
`highs` and `highspy`; it stops with a message otherwise.

| flag            | what                                                                                          |
| --------------- | --------------------------------------------------------------------------------------------- |
| `--file`        | a run file; the command line's run options go over it                                         |
| `--output`      | the run folder, made when missing; by default `results/<run name>` here                       |
| `--overwrite`   | remove the run folder first when it already holds a run                                       |
| `--build`       | the build folder to import rcspp from, when the tree has several                              |
| `--machine`     | the machine's label, for `metadata.json`                                                      |
| `--check build` | build the instance, graph, master, pricer and starting columns without solving; write nothing |

`--help` lists them with every run option. Wrong options stop the runner with a message and exit code 2, before
anything is written. So does an output folder that already holds a run (any of the files below), unless
`--overwrite` is given: the folder is then removed and written again. A folder without them, or with other files
only, is used as it is.

## A run's folder

| file | contents |
| --- | --- |
| `run.toml` | the run's options, expanded: the runner's `--file run.toml` runs it again |
| `command.txt` | the command line that started the run |
| `metadata.json` | start and end; the process's processor time (`processor_seconds`); the commit and whether it had uncommitted changes, the build and its compiler, the versions of Python, rcspp, numpy and the solvers, the machine's label, processor, cores and memory |
| `solution.json` | with the statistic `solution` (on by default): `lp`, `integer`, `resolved` below |
| `result.json` | `status`, `objective`, `iterations`, `columns`, `artificial_columns`, `master_seconds`, `pricing_seconds`, `other_seconds`, `solve_seconds`, `lp_solved_again_seconds`, `integer_status`, `integer_objective`, `integer_seconds`, `gap`, `solution_check` (below), then the fields of the statistics turned on (`process`: `process_peak_memory_bytes`, `process_processor_seconds`; `solution`: `solution_columns`, the number of columns with a value in `lp`) |

`result.json` is written last: a folder that has one holds a finished run.

`solution.json` holds three parts:

| part | contents |
| --- | --- |
| `lp` | the last master LP solution: its `objective`, the `columns` with a value (`id`, `value`, `cost`, `path` as node ids, `rows` as `[index, coefficient]`), the `artificial_columns` still in use (`id`, `value`), the `duals`, one per row, and `solved_again`. When a limit stopped the run after pricing added columns, the master solvers have dropped the last LP's values: the runner solves the LP once more, over every column, and sets `solved_again` (its time is `lp_solved_again_seconds`; `objective` in `result.json` stays the loop's). Null when no LP was solved, or the LP solved again did not end optimal; `lp_missing` says why |
| `integer` | the integer master over the generated columns (the run option `integer`): `status`, `objective`, `columns` and `artificial_columns` as in `lp`; `objective` null and no columns when it found no solution. Null when it is off |
| `resolved` | the instance's own solution, from the integer one: for `solomon`, `routes` (customer sequences, without the depot), `vehicles` and `distance` (the columns' costs), and the `errors` its check found: each customer served once, every route within the time windows (back at the depot by its due time) and the capacity, the distance recomputed from the file equal to `distance` and to the integer objective. Null without an integer solution, or when it uses artificial columns; `resolved_missing` says why |

In `result.json`, the integer fields are null when the integer solve is off. `integer_seconds` is outside
`solve_seconds`. `gap` is (integer objective - LP objective) / |integer objective|, the solvers' MIP gap, and null
unless column generation ended `Optimal`: only then is the LP value a lower bound. `solution_check` is `passed`,
`failed`, or null without a resolved solution; the check runs with the statistic `solution` off too. A failed check
writes the whole folder and exits with code 1. The solvers give integer values up to a few ulps (`1.0000000000000002`):
a column is used as many times as its rounded value.

## Run a benchmark

`benchmark.py` makes many runs from a benchmark file ([`OPTIONS.md`](OPTIONS.md#a-benchmark-file)): each by its
layer's runner, in its own process, into one benchmark folder.

```bash
cd benchmarks/column_generation
python benchmark.py my_benchmark.toml                    # into results/my_benchmark
python benchmark.py my_benchmark.toml --check options    # list the runs, run nothing
python benchmark.py my_benchmark.toml --processes 2 --timeout 600 --machine laptop
python benchmark.py --instance C101_25 --master highs --layer cpp-bindings --output results/one
```

Before the first run, the benchmark expands the file, prints the matrix points it drops (a layer without the master)
and every run, checks each instance file and start, then has each runner build its run without solving
(`--check build`, once per run, not per round). Anything wrong stops it there, and leaves the benchmark folder as it
was. The runs then go round after round, in the file's order, `processes` at a time. It prints what it has to do
(`starting <folder>: 8 runs to run`, or on a resume `resuming <folder>: 5 / 8 finished (1 timeout, 0 check failed),
3 to run (2 cut off, removed first)`), a line per run that ends (`[6/8] <run folder>: finished, 33.2 s`), and at the
end the counts of `status.txt`.

| flag | what |
| --- | --- |
| `--output` | the benchmark folder; by default `results/<file name>` here; needed without a file |
| `--build` | the build folder to import rcspp from, when the tree has several |
| `--check options` | expand, list and check the runs, then stop; write nothing; on an existing folder, say how many runs it already finished |
| `--check build` | the same, and check that the folder can be resumed, then build each run, then stop; write nothing |
| `--no-check` | do not build each run before the first starts |
| `--allow-mixed` | resume although the source, versions or machine changed |
| `--retry-timeouts` | run again the runs killed at their timeout |

It also takes the benchmark options (`--processes`, `--repeats`, `--timeout`, `--machine`, `--no-logs`) and every run
option, which go over every run of the file.

### Resume, stop

Run the same command again on the same folder to resume it. A run folder with a `result.json` is finished and kept;
any other, left by a run cut off, is removed and its run starts again from the beginning: a run does not continue
from its last iteration. The benchmark refuses a folder holding another benchmark's runs, and one started on another
commit, with other uncommitted changes, versions or machine, unless `--allow-mixed` (both records are then kept).
More rounds (`--repeats`) can be added on resume. Another number of processes cannot change one process into
several or back: with more than one, every run that leaves `column_generation.threads` unset gets 1, which makes
other runs; the benchmark says so and names the folder's `--processes`.

- To stop: create a file `STOP` in the benchmark folder. The runs in progress end, no other starts, the benchmark
  deletes `STOP` and exits; run it again to go on.
- Ctrl-C, or an error of the benchmark itself, kills the runs in progress (and the runners still building during
  the build check), records them as `interrupted`, and exits; they start again on resume.
- `timeout` kills a run's process and its children. Its `result.json` gets status `timeout`, `solve_seconds` the time
  it ran, and null fields; a resume keeps it unless `--retry-timeouts`. `column_generation.timeout_s` is another
  thing: the run stops itself and writes its result.

### The benchmark folder

| file | contents |
| --- | --- |
| `benchmark.toml` | the benchmark options and every run, expanded and named |
| `metadata.json` | processes, rounds, runs, the matrix points dropped; the record its runs share (source, build, versions of the masters' packages, machine), and the other records when mixed; `events` (below) |
| `status.txt` | how many runs are finished and how, then each run in progress and since when; rewritten every second and at every change |
| `run_files/<run>.toml` | each run's options, given to its runner as `--file` |
| `runs/<run>-r<round>/` | each run's folder, as above, with `log.txt` (the runner's output) unless `--no-logs` |

`events` lists each start and end of the benchmark, in order, each with its `time` and `event`:

| event | other fields |
| --- | --- |
| `started`, `resumed` | `total`, `finished` and `to_run` (runs), `cut_off` (unfinished folders removed), `timeouts_to_run_again`; `retry_timeouts` and `allow_mixed` when given; on a resume, `changed`: the benchmark options changed since the folder's last start, as `[before, after]` |
| `stopped` | by `STOP`: `runs_left`, and the counts |
| `interrupted` | by Ctrl-C or an error: `reason`, `killed` (the run folders it killed), and the counts |
| `finished` | the counts: `total` (runs of every round, as this start saw them), `finished`, `timeouts`, `failed_checks`, `crashed` |

A start refused (another benchmark's runs, mixed, a failed check) writes nothing.

The benchmark adds to each run's `metadata.json`:

| field | contents |
| --- | --- |
| `exit_code` | the runner's; null when killed at its timeout |
| `outcome` | `finished`, `check failed` (exit code 1, the integer solution fails the instance's check), `timeout`, `crashed` (any other ending without `result.json`, a Python error included), `not started` (exit code 2: wrong options, a folder holding a run), `interrupted` (Ctrl-C, or an error of the benchmark) |
| `started`, `wall_seconds` | when the process started, and how long it ran |
| `benchmark.processes` | runs at a time |
| `load` | the machine during the run, sampled every second from outside it, and at its start and end: `other_processor_percent_mean` and `_max` (what the other processes used of every processor), `memory_used_bytes_mean` and `_max`, `frequency_mhz_mean` and `_min` (Windows often gives the nominal frequency), `on_battery` (null without a battery), `samples`, `interval_seconds`. The run's own processor time at the end is the one its runner wrote; a run killed has none, and its last moments (at most 50 ms) count as the other processes' |

`benchmark.py` exits with 0 when every run finished and passed its check, 1 when a run timed out, crashed, did not
start or failed its check, 2 when the benchmark could not start, 3 when `STOP` stopped it, 4 on an error of the
benchmark itself, 130 on Ctrl-C.

## Tests

```bash
pytest benchmarks/column_generation/tests
```

The tests find the build as the benchmark does; `RCSPP_BENCHMARK_BUILD=<folder>` chooses one among several. Without a
build, the tests that need rcspp skip; without the column generation example in it, those of `cpp-bindings` skip. One
test checks that every layer the build has gives the same objective, iterations and columns on C101_25. The tests
use HiGHS only: run a Gurobi master by hand.
