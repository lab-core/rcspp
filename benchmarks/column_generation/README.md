# Column generation benchmark

Runs column generation on rcspp's layers and records how they do. It measures the source tree it lives in: rcspp's
Python sources in `python/src` and the compiled `_core` of a build of this tree. Nothing here is installed; the
wheel does not have it.

| layer          | what runs                                                                                      |
| -------------- | ---------------------------------------------------------------------------------------------- |
| `package`      | `rcspp.column_generation`, the column generation package rcspp ships                           |
| `cpp-bindings` | the C++ column generation example of `examples/column_generation`, through its Python bindings |
| `cpp-native`   | the same C++ example in a C++ program, `cpp_native/`, without Python                           |

| path                           | contents                                                           |
| ------------------------------ | ------------------------------------------------------------------ |
| `benchmark.py`                 | many runs, each by its runner in its own process                   |
| `summary.py`                   | the summary of a benchmark folder                                  |
| `compare.py`                   | the comparison of two runs                                         |
| `example.toml`                 | an example benchmark file                                          |
| `runners/`                     | the runners: one run of a layer each                               |
| `cpp_native/`                  | the `cpp-native` layer's C++ program, built with the example       |
| `column_generation_benchmark/` | the code the runners and the benchmark share                       |
| `tests/`                       | the tests                                                          |
| `results/`                     | where runs go unless told otherwise; ignored by git (`.gitignore`) |
| `OPTIONS.md`                   | the options of a run and of a benchmark                            |

## Install

Python 3.11 or later, in a virtual environment:

```bash
pip install -r benchmarks/column_generation/requirements.txt
```

`gurobipy` and a Gurobi licence are needed only for the Gurobi master solvers. `highspy` is pinned to the HiGHS
version the column generation example builds, so that every layer solves with the same HiGHS.

## Build rcspp

The benchmark imports rcspp from a build of this tree: a `build*/` folder at its root, found alone, or one given with
`--build` (relative to the tree's root, or absolute anywhere). Build the Python module there, with the column
generation example for the `cpp-bindings` and `cpp-native` layers (`examples/column_generation/README.md` lists the
options):

```bash
cmake -B build-python -DCMAKE_BUILD_TYPE=Release -DUSE_PYTHON=ON -DUSE_TESTS=OFF \
    -DUSE_COLUMN_GENERATION_EXAMPLE=ON -DUSE_COLUMN_GENERATION_EXAMPLE_HIGHS=ON "-DCMAKE_CXX_CLANG_TIDY="
cmake --build build-python --config Release
```

`-DCMAKE_CXX_CLANG_TIDY=` turns off clang-tidy, which the project's build otherwise runs on every file. Add
`-DPython_EXECUTABLE=<python>` when CMake would otherwise find another Python than the environment's. The Gurobi
masters `gurobipy` need no build option: `package` and `cpp-bindings` reach Gurobi through gurobipy. The `cpp-native`
layer's `gurobi` master is Gurobi's C++ API: add `-DUSE_COLUMN_GENERATION_EXAMPLE_GUROBI=ON` (Gurobi installed, with
its C++ library); give gurobipy the same Gurobi version, so that the layers compare on one.

The example on, the build also makes the `cpp-native` layer's program, `cpp-native-solver`, in `bin/` (in
`bin/<configuration>/` with Visual Studio, the configuration `_core` was built in).

The build copies `python/src/rcspp` next to `_core`; build again after changing the Python sources, and after
changing the C++ ones: nothing checks that `_core` was built from the current C++.

## Which rcspp is measured

Before importing rcspp, the benchmark checks that the build was configured from this tree (its `CMakeCache.txt`),
puts the build's `python` folder first on the path, then checks that rcspp and its `_core` come from that build, and
that the build's copy of the Python sources equals `python/src`, with no module the sources no longer have. It stops
otherwise: a virtual environment may have another rcspp installed, which a run would measure without failing, and a
build configured from another checkout holds that checkout's C++.

## Options

A run is a layer and its blocks (`instance`, `master`, `pricing`, `start`, ...), given in a TOML file, on the command
line, or both; a benchmark file makes many runs. [`OPTIONS.md`](OPTIONS.md) has the words, the TOML they need, where a
run's options come from, examples, and every option's parameters.

## Run once

Each layer has a runner in `runners/`, a script that does one run and writes its folder. `package_runner.py` runs the
`package` layer, `cpp_bindings_runner.py` the `cpp-bindings` layer; a runner's body is the column generation calls,
the place to read how its layer is used. `cpp_native_runner.py` runs the `cpp-native` layer: it expands the run into
the flags of the program `cpp-native-solver` (`cpp_native/cpp_native_solver.cpp`, whose body is that layer's
calls), starts it and reads its result. All take the same flags and options; the run's layer is the runner's.

```bash
cd benchmarks/column_generation
python runners/package_runner.py --instance C101_25 # into results/C101_25-package-highspy-exact-artificial
python runners/package_runner.py --instance R201_25 --master gurobipy --start single-customer
python runners/package_runner.py --file results/C101_25-package-highspy-exact-artificial/run.toml --output results/again
python runners/package_runner.py --instance C101_25 --check build # build, do not solve
python runners/cpp_bindings_runner.py --instance C101_25 --master highs
python runners/cpp_bindings_runner.py --file results/C101_25-package-highspy-exact-artificial/run.toml --layer cpp-bindings
python runners/cpp_native_runner.py --instance C101_25 --master highs
```

`cpp_bindings_runner.py` needs the build to have the column generation example, and its HiGHS part for the masters
`highs` and `highspy`; it stops with a message otherwise. `cpp_native_runner.py` needs the build's `cpp-native-solver`,
and names its master: the default master, `highspy`, is not one of its layer's. It reads Solomon instances only. Its
run records the program's solvers' versions, its configuration and compiler (`code.program`), and the program's own
peak memory and processor time.

| flag            | what                                                                                          |
| --------------- | --------------------------------------------------------------------------------------------- |
| `--file`        | a run file; the command line's run options go over it                                         |
| `--output`      | the run folder, made when missing; by default `results/<run name>` here                       |
| `--overwrite`   | remove the run's files from the folder first when it already holds a run                      |
| `--build`       | the build to import rcspp from, when the tree has several: relative to the tree's root        |
| `--machine`     | the machine's label, for `metadata.json`                                                      |
| `--check build` | build the instance, graph, master, pricer and starting columns without solving; write nothing |

`--help` lists them with every run option. Wrong options stop the runner with a message and exit code 2, before
anything is written. So does an output folder that already holds a run (any of the files below), unless
`--overwrite` is given: the run's files and `log.txt` are then removed, other files kept, and the run written again.
A benchmark's folder (with `benchmark.toml` or `runs/`) is refused, `--overwrite` or not. A folder without run
files is used as it is. A run stopped after its folder was started, by an error (exit code 1) or Ctrl-C, leaves the
start in it: run it again with `--overwrite`.

The two runners are twins: each `main` reads the run, builds the instance, master, pricer and start, runs the loop,
reads the LP solution, solves the integer master and writes the folder, in that order, with its own layer's calls;
`column_generation_benchmark/runner.py` holds the steps around them.

## A run's folder

| file | contents |
| --- | --- |
| `run.toml` | the run's options, expanded: the runner's `--file run.toml` runs it again |
| `command.txt` | the command line that started the run |
| `metadata.json` | start and end; the process's processor time (`processor_seconds`); the commit and whether it had uncommitted changes, the build, its type and its compiler, the versions of Python, rcspp, numpy and the solvers (`highs`: the HiGHS that solves; `highspy_highs`: highspy's own), the machine's label, processor, cores and memory; the processes sharing the machine and the solver's threads in the LP solves and in the integer solve (`benchmark`) |
| `solution.json` | with the statistic `solution` (on by default): `lp`, `integer`, `resolved` below |
| `iterations.jsonl` | with the statistic `iterations` (off by default): one line per iteration, written as the run goes, so that a run killed half-way keeps them; its fields in `OPTIONS.md` |
| `result.json` | `status`, `objective`, `lower_bound` (null when unknown), `iterations`, `columns`, `artificial_columns`, `master_seconds`, `pricing_seconds`, `other_seconds`, `solve_seconds`, `lp_solved_again_seconds`, `integer_status`, `integer_objective`, `integer_seconds`, `gap`, `solution_check` (below), then the fields of the statistics turned on (`process`: `process_peak_memory_bytes`, `process_processor_seconds`; `solution`: `solution_columns`, the number of columns with a value in `lp`) |

`result.json` is written last: a folder that has one holds a finished run.

`solution.json` holds three parts:

| part | contents |
| --- | --- |
| `lp` | the last master LP solution: its `objective`, the `columns` with a value (`id`, `value`, `cost`, `path` as node ids, `rows` as `[index, coefficient]`), the `artificial_columns` still in use (`id`, `value`), the `duals`, one per row, and `solved_again`. It is read as the master solver holds it only after `Optimal`, `NotProven` or `Infeasible`: that LP finished and nothing came after. After a limit or a failed solve, pricing may have added columns, which drop the LP's values, or the limit stopped an LP solve: the runner solves the LP once more, over every column, with the run's own time limit at most, and sets `solved_again` (its time is `lp_solved_again_seconds`; `objective` in `result.json` stays the loop's). Null when no LP was solved, or the LP solved again did not end optimal; `lp_missing` says why |
| `integer` | the integer master over the generated columns (the run option `integer`): `status`, `objective`, `columns` and `artificial_columns` as in `lp`; `objective` null and no columns when it found no solution. Null when it is off |
| `resolved` | the instance's own solution, from the integer one: for `solomon`, `routes` (customer sequences, without the depot), `vehicles` and `distance` (the columns' costs), and the `errors` its check found: each customer served once, every route within the time windows (back at the depot by its due time) and the capacity, the distance recomputed from the file equal to `distance` and to the integer objective. Null without an integer solution, or when it uses artificial columns; `resolved_missing` says why |

In `result.json`, the integer fields are null when the integer solve is off. `integer_seconds` is outside
`solve_seconds`. `gap` is (integer objective - LP objective) / |integer objective|, the solvers' MIP gap, and null
unless column generation ended `Optimal`: only then is the LP value a lower bound. `solution_check` is `passed`,
`failed`, or null without a resolved solution; the check runs with the statistic `solution` off too. A failed check
writes the whole folder and exits with code 1. The solvers give integer values up to a few ulps (`1.0000000000000002`):
a column is used as many times as its rounded value, and a value within 1e-9 of 0 is no use.

## Run a benchmark

`benchmark.py` makes many runs from a benchmark file ([`OPTIONS.md`](OPTIONS.md#a-benchmark-file)): each by its
layer's runner, in its own process, into one benchmark folder. [`example.toml`](example.toml) is one to start from,
each kind of entry commented: every layer and master on C101_25 and R201_25 with two pricing setups, and two runs of
their own (22 runs, 2 rounds; about a minute and a half on a laptop). Without Gurobi, remove `gurobipy` from it;
without the column generation example built with its HiGHS part (`USE_COLUMN_GENERATION_EXAMPLE=ON`,
`USE_COLUMN_GENERATION_EXAMPLE_HIGHS=ON`), remove `cpp-bindings` and `highs`.

```bash
cd benchmarks/column_generation
python benchmark.py example.toml                         # into results/example
python benchmark.py my_benchmark.toml                    # into results/my_benchmark
python benchmark.py my_benchmark.toml --check options    # list the runs, run nothing
python benchmark.py my_benchmark.toml --processes 2 --timeout 600 --machine laptop
python benchmark.py --instance C101_25 --master highs --layer cpp-bindings --output results/one
```

Before the first run, the benchmark expands the file, prints the matrix points it drops (a layer without the master)
and every run, checks each instance file and start, then has each runner build its run without solving
(`--check build`, once per run, not per round). Anything wrong stops it there, and leaves the benchmark folder as it
was. The runs then go round after round, in the file's order, `processes` at a time. It prints what it has to do
(`starting <folder>: 8 runs to run`, or on a resume, after a framed `RESUMING` notice on stderr, `resuming <folder>:
5 / 8 finished (1 timeout, 0 check failed), 3 to run (2 unfinished, removed first)`), a line per run that ends
(`[6/8] <run folder>: finished, 33.2 s`), and at the end the counts of `status.txt`, and the summary's path
([Summary](#summary)).

| flag | what |
| --- | --- |
| `--output` | the benchmark folder; by default `results/<file name>` here; needed without a file |
| `--build` | the build to import rcspp from, when the tree has several: relative to the tree's root, or absolute |
| `--check options` | expand, list and check the runs, then stop; write nothing; on an existing folder, say how many runs it already finished |
| `--check build` | the same, and check that the folder can be resumed, then build each run, then stop; write nothing |
| `--no-check` | do not build each run before the first starts |
| `--allow-mixed` | resume although the record changed: the source (commit, uncommitted changes), the build, the versions or the machine |
| `--retry-timeouts` | run again the runs killed at their timeout |
| `--retry-crashes` | run again the runs that crashed or did not start after writing a result |

It also takes the benchmark options (`--processes`, `--repeats`, `--timeout`, `--machine`, `--no-logs`) and every run
option, which go over every run of the file.

### Resume, stop

Run the same command again on the same folder to resume it; the benchmark says so loudly. A round with a
`result.json` is kept: finished, timed out (unless `--retry-timeouts`), or crashed after writing it (unless
`--retry-crashes`; it still counts as crashed). Any other, left unfinished or interrupted, is removed and starts again
from the beginning: a run does not continue from its last iteration. The benchmark refuses a folder holding another
benchmark's runs, and one started on another commit, with other uncommitted changes (a digest of `git diff HEAD`;
untracked files are not in it), another build of `_core`, versions or machine, unless `--allow-mixed` (each other
record is then kept once). Only one benchmark works on a folder at a time: it holds `.lock` (its process id and start)
while it runs, and another is refused; a lock left by a benchmark that ended without removing it is taken over.
More rounds (`--repeats`) can be added on resume. Another number of processes cannot change one process into
several or back: with more than one, every run that leaves `column_generation.threads` unset gets 1, which makes
other runs; the benchmark says so and names the folder's `--processes`.

- To stop: create a file `STOP` in the benchmark folder. The runs in progress end, no other starts, the benchmark
  deletes `STOP` and exits; run it again to go on. A `STOP` created once every run has started stops nothing: the
  benchmark finishes and deletes it. One left from before is deleted at the next start.
- Ctrl-C, SIGTERM, SIGHUP (a scheduler's end, `timeout`, `kill`, a closed terminal), or an error of the benchmark
  itself, kills the runs in progress (and the runners still building during the build check), waits a few seconds
  for each, records them as `interrupted`, and exits; they start again on resume. A run that had written its result
  is recorded `finished`. Another Ctrl-C meanwhile is ignored. Only `kill -9` (or the Task Manager) leaves the runs
  going.
- `timeout` kills a run's process and its children. It counts the run's wall time from its process's start, Python's
  and rcspp's imports included. The run's `result.json` gets status `timeout`, `solve_seconds` the time it ran, and
  null fields; a resume keeps it unless `--retry-timeouts`. The runners write their output unbuffered, so `log.txt`
  keeps what a run killed printed. `column_generation.timeout_s` is another thing: the run stops itself and writes
  its result.

### The benchmark folder

| file | contents |
| --- | --- |
| `benchmark.toml` | the benchmark options and every run, expanded and named |
| `metadata.json` | processes, rounds, runs, the matrix points dropped; the record its runs share (source, build, versions of the masters' packages, machine), and the other records when mixed; `events` (below) |
| `status.txt` | how many runs are finished and how, then each run in progress and since when; rewritten every second and at every change |
| `summary.md`, `summary.csv`, `runs.csv` | the summary ([below](#summary)) |
| `run_files/<run>.toml` | each run's options, given to its runner as `--file` |
| `runs/<run>-r<round>/` | each run's folder, as above, with `log.txt` (the runner's output) unless `--no-logs` |

`events` lists each start and end of the benchmark, in order, each with its `time` and `event`:

| event | other fields |
| --- | --- |
| `started`, `resumed` | `total`, `finished` and `to_run` (runs), `unfinished` (folders without a result, or interrupted, removed first), `timeouts_to_run_again`, `crashes_to_run_again`; `retry_timeouts`, `retry_crashes` and `allow_mixed` when given; on a resume, `changed`: the benchmark options changed since the folder's last start, as `[before, after]` |
| `stopped` | by `STOP`: `runs_left`, and the counts |
| `interrupted` | by Ctrl-C, a signal or an error: `reason`, `killed` (the run folders it killed before their result), and the counts |
| `finished` | the counts: `total` (runs of every round, as this start saw them), `finished`, `timeouts`, `failed_checks`, `crashed` |

A start refused (another benchmark's runs, mixed, a failed check, a folder in use) writes nothing.

The benchmark adds to each run's `metadata.json`:

| field | contents |
| --- | --- |
| `exit_code` | the runner's; null when killed at its timeout |
| `outcome` | `finished`, `check failed` (exit code 1, the integer solution fails the instance's check), `timeout`, `crashed` (any other ending: no `result.json`, a Python error, an error after the result was written), `not started` (exit code 2: wrong options, a folder holding a run), `interrupted` (Ctrl-C, a signal, or an error of the benchmark, before the run's result) |
| `started`, `wall_seconds` | when the process started, and how long it ran |
| `benchmark.processes` | runs at a time |
| `load` | the machine during the run, sampled every second from outside it, and at its start and end: `other_processor_percent_mean` and `_max` (what the other processes used of every processor), `memory_used_bytes_mean` and `_max`, `frequency_mhz_mean` and `_min` (Windows often gives the nominal frequency), `on_battery` (null without a battery), `samples`, `interval_seconds`. The run's own processor time at the end is the one its runner wrote; a run killed has none, and its last moments (at most a second, the sampling interval) count as the other processes' |

`benchmark.py` exits with 0 when every run finished and passed its check, 1 when a run timed out, crashed, did not
start or failed its check, 2 when the benchmark could not start, 3 when `STOP` stopped it, 4 on an error of the
benchmark itself, 130 on Ctrl-C, SIGTERM or SIGHUP; `--help` lists them too.

## Summary

`benchmark.py` writes a summary into the benchmark folder each time it ends, finished, stopped or interrupted;
`summary.py` writes it again from the folder's files alone. A folder of run folders the runners wrote by themselves
works too, as does a single run folder: there is then no `benchmark.toml`, and the runs are the folder names without
`-r<round>`.

```bash
cd benchmarks/column_generation
python summary.py results/my_benchmark
```

| file | contents |
| --- | --- |
| `summary.md` | the runs, rounds and processes; the rounds finished, timed out and not finished; the last event; the flags (below); the record the rounds share, and any other with the rounds that have it; the solvers' versions over the rounds (from the benchmark's own record for a solver no round recorded); a results table and a times table, one line per run; the rounds not in the figures (timeouts included), with their outcome and exit code |
| `summary.csv` | one row per run: `run`, `layer`, `instance`, `master`, how its rounds ended (`rounds`, `finished`, `failed_checks`, `timeouts`, `not_finished`), the figures of each result field (a measured field as `<field>_median`, `_mean`, `_min`, `_max`, `_spread_percent`; any other under its own name), then `seconds_per_iteration`, `wall_seconds_median`, `processor_seconds_median`, `other_processor_percent` (the median over the rounds of each round's `load.other_processor_percent_mean`), `on_battery`, `flags` |
| `runs.csv` | one row per round: `run`, `round`, `folder`, `outcome`, the run's options as dotted columns (`master.type`, `graph.ng_size`; lists as JSON), every field of `result.json`, then from `metadata.json` `exit_code`, `started`, `wall_seconds`, `processor_seconds` and the `benchmark.`, `load.`, `source.` and `versions.` fields; for plots and other tools |

The columns keep these groups whatever run comes first: a field only a later run has goes with the result fields. A
result field named as one of these columns (a runner's own `outcome`, say) is written `result.<field>`.

The figures are over the finished rounds, those whose check failed included. A field whose name ends in `_seconds` or
`_bytes` is measured: its median, mean, min, max and spread, (max - min) / median in percent (`summary.csv` has all
five, `summary.md` the median, and the min, max and spread of `solve_seconds`). The median is the main figure: a
round slowed by the machine moves the mean more than the median. `seconds_per_iteration` is the median over the
rounds of each round's `solve_seconds` / `iterations`, an average over the run's iterations, early and late alike.

Any other field has one value when every finished round agrees, numbers within 1e-6 relative; otherwise it is empty
in `summary.csv`, `summary.md` shows `min..max` or each value with its count, and the run is flagged. Timeouts,
crashed, not started, interrupted, cut off (`unfinished`), missing and `unreadable` rounds (a file of the round's
that cannot be read) are counted, not in the figures. A runner's own fields get the same treatment.

The flags:

- more than one process: each run shared the machine with others, and ran its master solver on one thread unless
  told otherwise;
- rounds on battery (said once, for the folder);
- mixed: rounds with another source, build, versions or machine (resumed with `--allow-mixed`); a round killed
  before its runner wrote its record (at its timeout, or still running) is in no record;
- a solver with several versions in the folder: the layers compare only on the same solver (`highs` is the HiGHS that
  solves: the column generation example's on `cpp-bindings` and `cpp-native`, highspy's on `package`); a round whose
  master's solver
  version was not recorded;
- per run: timeouts, failed checks, rounds not finished, rounds that differ, and, in a folder of run folders, rounds
  of one name with other options (the first round's are shown).

## Compare

`compare.py A B` compares two runs: A, the run checked, against B, the reference. Each is a round's folder
(`results/my_benchmark/runs/<run>-r1`), a run folder a runner wrote, or `<benchmark folder>:<run name>`, the run over
its rounds; the name may be cut short when no other run of the folder starts the same way. A and B may come from two
benchmark folders, from other layers or from runners that write other fields. On the example's results:

```bash
python compare.py results/example:C101_25-package-highspy-exact results/example:C101_25-cpp-bindings-highspy-exact
```

On a larger benchmark, with the time rule:

```text
$ python compare.py results/five:R201_50-package-highspy results/five:R201_50-cpp-bindings-highspy --time-within 5%
- A: five / R201_50-package-highspy-exact-artificial-column_generation_c9520b       2 / 2 rounds finished
- B: five / R201_50-cpp-bindings-highspy-exact-artificial-column_generation_c9520b  2 / 2 rounds finished
- Options that differ: layer: package -> cpp-bindings
- Records that differ: none
- Warning: 5 runs at a time: each run shared the machine with others, and the runs that left column_generation.threads unset ran with 1 thread

| field                     | A                       | B                       | A - B                          |
|---------------------------|-------------------------|-------------------------|--------------------------------|
| status                    | Optimal                 | Optimal                 | equal                          |
| objective                 | 756.339                 | 756.339                 | equal                          |
| iterations                | 33                      | 33                      | equal                          |
| master_seconds            | 3.158                   | 3.036                   | +0.121 (+4.0 %)                |
| pricing_seconds           | 86.373                  | 86.010                  | +0.363 (+0.4 %)                |
| solve_seconds             | 89.561 [87.187..91.935] | 89.047 [87.623..90.471] | +0.514 (+0.6 %), within spread |
| process_peak_memory_bytes | 116.7 MiB               | 108.8 MiB               | +7.9 MiB (+7.3 %)              |
| per iteration             | 2713.98 ms              | 2698.39 ms              | +15.6 ms (+0.6 %)              |

Verdict: equivalent (both Optimal, same objective; +0.514 s <= max(5 % of 89.047, 0.25) = 4.452 s)
```

(Rows cut short here: every field of `result.json` has one.) The header gives each side's rounds and how those not
finished ended, the options that differ (dotted names, `-` for an option a side does not have), and the records
that differ: source, build, versions and machine, the solvers' versions when both sides recorded them. The warnings
are the summary's flags of each side, said once when both share them, and a side with several records.

The figures are over each side's finished rounds, as in the summary. A field whose name ends in `_seconds` or
`_bytes` is measured: each side's median, A - B and its percent of B; `solve_seconds` also shows each side's min and
max, and whether A - B is within the rounds' spread (the two ranges overlap), beyond it, or not known (a side with one
round). A difference under a millisecond is in milliseconds. `per iteration` is each round's `solve_seconds` /
`iterations`, their median. Any other field should match: `equal` (numbers within 1e-6 relative), `differs` with
A - B, or the side whose rounds disagree (`both sides' rounds differ` when both do). A field only one side has is
`only in A` or `only in B`.

The time rule judges A when `--time-within PERCENT` (default 5 %) or `--floor SECONDS` (default 0.25) is given:
PERCENT is in percent, `5` or `5%`, not `0.05`; both are finite, 0 or more. Every round of both sides finished (no
timeout, crash or missing round: the finished rounds alone would make a side look faster), both sides `Optimal` with
the same objective within 1e-6 relative; then A's median solve time may exceed B's by at most
max(PERCENT of B, SECONDS). A faster is always equivalent. The floor keeps
short runs, where a fixed cost weighs a lot, from failing on a few milliseconds. When the sides' iterations differ,
the verdict stands but its reason says so (`the iterations differ, 25 and 16: the times are of other work`): the
same objective was reached by another path, as two LP solvers can do on a degenerate LP with duals a rounding error
apart, and `per iteration` is then the fairer figure.

| exit code | when |
| --- | --- |
| 0 | the comparison printed: no time rule, or A equivalent |
| 1 | A slower than the time rule allows |
| 2 | an argument names no single run, or a wrong option |
| 3 | the time rule cannot judge: a side not `Optimal`, objectives that differ, a round not finished, no finished round |

`--output FILE` also writes the comparison, as many times as given: `.md` the text above, `.csv` one row per field
(`field`, `kind`, `a`, `b`, `a_min`, `a_max`, `b_min`, `b_max`, `difference`, `percent`, `judgement`), in the units
of `result.json`. `kind` is `measured` or `should match`. For a measured field, `a` and `b` are the medians and
`judgement` says whether the difference is `within spread`, `beyond spread`, or `1 round, no spread`, for every
measured field (the text shows it for `solve_seconds` only). For a field that should match, `a` or `b` is text when
that side's rounds disagree: `600..601`, or `Optimal x2, TimeLimit x1`. The `seconds_per_iteration` row has no
min, max or judgement.

## Tests

```bash
pytest benchmarks/column_generation/tests
```

The tests find the build as the benchmark does; `RCSPP_BENCHMARK_BUILD=<folder>` chooses one among several. Without a
build, the tests that need rcspp skip; without the column generation example in it, those of `cpp-bindings` and
`cpp-native` skip. One
test checks that every layer the build has gives the same objective on C101_25, and the same iterations and columns
where the LP solver is the same HiGHS build: the example's two masters. The package's highspy wheel and the example's
HiGHS can return duals a rounding error apart, and on a degenerate LP that is enough to change the path (the same
objective in other iterations). The Gurobi tests skip without gurobipy or its licence.
