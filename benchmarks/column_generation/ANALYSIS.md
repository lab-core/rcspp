# Analysis

`analysis.py` reads run folders and writes what a question needs, a command per question; it
never runs a solve. It needs more than the runners: `pip install -r requirements-analysis.txt`
(the benchmark's requirements, pandas and matplotlib). `python analysis.py --help` lists the
commands, `python analysis.py <command> --help` a command's options.

| command       | what                                                                  |
| ------------- | --------------------------------------------------------------------- |
| `compare`     | the comparison of two runs, with an optional time rule               |
| `convergence` | the objective and the best bound of runs, iteration after iteration |
| `sweep`       | one option swept: result fields against its values, a line per instance |

The commands but `compare` take the same runs and output options:

| option | what |
| --- | --- |
| `RUNS` | one or more: a benchmark folder, a folder of run folders, a run folder, or `<folder>:<run name>`, the name in full or its start (every run that starts so) |
| `--where OPTION=VALUE` | keep the runs whose dotted option has that value (`graph.ng_size=8`, `master.type=highspy`); the value is read as TOML, else as text; may be given more than once. An option no run has is an error |
| `--output-folder FOLDER` | the analysis folder; by default `results/analysis/<command>-<date>-<time>` |
| `--overwrite` | replace the analysis the folder already holds |
| `--format FORMAT` | a figure's format, `png`, `pdf` or `svg`; may be given more than once (default `png` and `pdf`) |

A command that writes files writes them into an analysis folder, with two files that say how
they were made:

| file | contents |
| --- | --- |
| `analysis.json` | the command, its arguments as typed, the options it resolved, when, the source tree (commit, uncommitted changes and their digest, branch), the versions of Python, numpy, pandas and matplotlib, the inputs (each folder as given and resolved, each round read with a digest of each of its files, which tells when a run folder changed since), the files written and the warnings |
| `command.txt` | the command line that makes it again, the interpreter first |

A folder that holds an analysis is refused, unless `--overwrite`: that analysis's files (those
its `analysis.json` lists) are then removed first, other files kept. A folder that holds other
files and no analysis is refused, `--overwrite` or not.

## Compare

`python analysis.py compare A B` compares two runs: A, the run checked, against B, the reference. Each is a round's folder
(`results/my_benchmark/runs/<run>-r1`), a run folder a runner wrote, or `<benchmark folder>:<run name>`, the run over
its rounds; the name may be cut short when no other run of the folder starts the same way. A and B may come from two
benchmark folders, from other layers or from runners that write other fields. On the example's results:

```bash
python analysis.py compare results/example:C101_25-package-highspy-exact results/example:C101_25-cpp-bindings-highspy-exact
```

On a larger benchmark, with the time rule:

```text
$ python analysis.py compare results/five:R201_50-package-highspy results/five:R201_50-cpp-bindings-highspy --time-within 5%
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

`--output-folder FOLDER` writes both into an analysis folder, as `comparison.md` and `comparison.csv`, with its
`analysis.json` and `command.txt` (`--overwrite` to replace an analysis there). The `2` of the exit codes also
covers a folder that cannot take it.

## Convergence

`python analysis.py convergence RUNS` draws how runs converge, a figure per instance with the runs of that instance:
each run's objective as a solid line, its best bound so far as a dashed one of the same colour (an iteration records
its own bound, from the pricers; the best is the highest so far), against the loop's time or the iterations. It needs
the runs' iterations (`statistics.iterations`, any groups) and, for a bound, `instance.vehicles`.

```bash
python analysis.py convergence results/my_benchmark --gap
python analysis.py convergence results/my_benchmark:R201_25 --where master.type=highspy --x iteration
```

| option | what |
| --- | --- |
| `--x` | `time` (default), the loop's time in seconds, or `iteration` |
| `--gap` | a panel below with the relative gap, (objective - best bound) / \|objective\|, on a log scale |
| `--y-range` | `auto` (default): from half the lowest last objective to twice the highest, the end of the convergence (an artificial start's first objectives are orders of magnitude above it); `full`; or `LOW,HIGH` |
| `--rounds` | `median` (default): each run's round whose loop took the median time (the lower one for an even count); `all`: every round, thin lines |

The legend names a run by the parts of its name that the other runs of its figure do not all have (`exact-artificial`,
`heuristics-first-single-customer`), with its benchmark folder when two folders hold it. A round that did not finish
(killed at its timeout, crashed) is drawn to its last iteration, ended with a cross. Each figure,
`convergence-<instance>.<format>`, comes with its data, `convergence-<instance>.csv`: per iteration of each curve, its
benchmark folder, run and round, `iteration`, `elapsed_seconds`, `objective`, `lower_bound`, `best_bound` and `gap`.

The runs without iterations are left out with a warning, and none at all is an error (exit code 2, as a wrong
argument or a folder that cannot take the analysis). A run not finished and a gap panel with no bound warn too.

## Sweep

`python analysis.py sweep RUNS --option OPTION` draws result fields against the values of an option, a line per
instance: a figure and a table per field.

```bash
python analysis.py sweep results/ng --option graph.relaxation --option graph.ng_size --log
python analysis.py sweep results/ng --option graph.ng_size --where graph.relaxation=ng --field iterations --relative
```

| option | what |
| --- | --- |
| `--option` | the dotted option swept; several make one label per combination (`ng 8`), `unset` for a run that has none of them. Numbers make a number axis, any other value an axis in the runs' order |
| `--field` | a field of `result.json`, or `seconds_per_iteration`; may be given more than once (default `objective` and `solve_seconds`) |
| `--relative` | divide each line by its value at its first point |
| `--log` | a log scale for the fields' axis |

A point is a run over its finished rounds, as the summary has it: a measured field's median, drawn with the rounds'
min and max, or the value the rounds agree on. The runs whose other options are the same make a line, named by the
parts of its runs' names that the other lines' do not all have (`C101_25`); lines left with one name, or none, get
the other options that differ between them (`graph.relaxation=ng`): sweeping `graph.ng_size` alone over runs with
and without ng gives the runs without ng a line of their own, as their `graph.relaxation` differs.

Each field gives `sweep-<field>.<format>`, its data `sweep-<field>.csv` (a point per row: `line`, `value`, its
benchmark folder, `run`, `rounds`, `median`, `min`, `max`) and a table `sweep-<field>.md` (a line per row, a value
per column, the medians). A swept option or a field no run has is an error (exit code 2). A run with no finished
round, rounds that disagree on a field that should match, and an option with one value only warn.
