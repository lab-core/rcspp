# `T-1.23`: the analysis tool

Designed with the user and built 2026-11-27 in a parallel chat (`D-307`), worktree `C:\dev\rcspp-options`; moved
here the same day. IDs reserved for this chat: `D-400`+, `F-180`+, `I-80`+. Decisions `D-400`-`D-443` in
`decisions.md`, findings `F-180`-`F-187` in `inventory.md`; the user-facing docs are the benchmark's `ANALYSIS.md`.

## 1. What it is

One tool over the benchmark's output and its instances. It never runs a solve. It reads run folders (`result.json`,
`solution.json`, `iterations.jsonl`, `runs.csv`, records) and instance files, and writes tables (CSV, markdown,
LaTeX), plots and drawings.

## 2. Decisions (2026-11-27, the user)

`D-400`-`D-407` in `decisions.md`.

## 2b. Inputs (topic 1, 2026-11-27)

Facts, on #128's head `5b03395c`: `F-180`-`F-185` in `inventory.md`.

`D-408`-`D-411` in `decisions.md`.

## 2c. The core (topic 2, 2026-11-27)

`D-412`-`D-418` in `decisions.md`.

## 2d. The analysis folder (topic 3, 2026-11-27)

`D-419`-`D-424` in `decisions.md`.

## 3. Layout and command line (topic 4, 2026-11-27)

```text
benchmarks/column_generation/
  analysis.py                     python analysis.py <command> ...
  requirements-analysis.txt       -r requirements.txt, pandas, matplotlib
  column_generation_benchmark/    unchanged, minus compare.py; folder.py stays
  column_generation_analysis/
    core/       tables.py, selection.py, analysis_folder.py
    commands/   compare.py, convergence.py, sweep.py, draw.py
    vrp/        drawing.py, overlays.py
  tests/        test_analysis_*.py beside test_benchmark_*.py
```

`D-425`-`D-432` in `decisions.md`.

## 4. The core's tables

| table | one row per | holds |
|---|---|---|
| runs | run × round | options flattened, records (machine, versions), result (status, LP, bound, iterations, columns, times) |
| iterations | run × round × iteration | objective, bound, time split, columns added, pricers' details; duals apart (wide) |
| solutions | run × round × route | the route's nodes, cost, value |

Schemas (column names, units, what a timeout or a crash gives) at the detailed design, from the files as T-2.5 writes
them.

## 4b. The commands (topic 5) and plot style (topic 6), 2026-11-27

Facts `F-186`, `F-187` in `inventory.md` (the best bound so far, the time axis).

The commands as designed (`D-433`, a starting point; as built: `D-441`-`D-443`, and `ANALYSIS.md`):

| command | behaviour |
|---|---|
| `compare` | unchanged; `--output-folder` writes an analysis folder |
| `convergence` | a figure per instance, the selected runs overlaid; objective solid, best bound dashed, same colour; `--gap` adds a log-scale gap panel; `--x time` (default) or `iteration`; `--y-range auto` (each curve's last 90 %), `full`, `LOW,HIGH`; each run's median round by `solve_seconds`, `--rounds all` thin lines; legend = the options that differ; unfinished runs drawn to their end with a mark and a warning; data `convergence-<instance>.csv` |
| `sweep` | `--option` dotted, repeatable (combined labels, `unset` when absent); `--fields` (default `objective,solve_seconds`; any result field or `seconds_per_iteration`); a figure per field, x the values, a line per instance (and per combination of other varying options); `--relative`; medians with min-max bars, other fields agreed or flagged; a table per field (instance × value); missing points marked and warned |
| `draw` | `--instance NAME` alone, or run selections (instance from `run.toml`, plus the solution); depot square, customers by demand, numbers up to about 30 customers (`--labels`); `--solution integer` (default when present, a colour per route) or `lp` (summed arc flows by value); a time-window chart per integer route (windows as bars, arrival and service marks, recomputed); overlays as plain functions (`draw_paths`, `draw_arc_values`, `draw_node_marks`); later `--added-columns ITERATION`, the negative cycle |

As built, after real runs: `D-441` (`convergence`), `D-442` (`sweep`), `D-443` (`draw`); plot style `D-434`.

## 5. The drawing

| piece | draws | data |
|---|---|---|
| base | depot, customers (position, demand), optionally time windows | the instance file |
| overlay: paths | node sequences, each styled (colour, width, label) | a solution's routes; later the columns added, a negative cycle |
| overlay: arc values | arcs with a value (width or colour) | later: the LP's flow per arc, branching |
| overlay: node marks | marked nodes | later: a cycle's nodes, uncovered customers |
| time-window chart | per path: each node's window as a bar, the arrival as a mark | the instance and the paths |

A drawing is the base plus a list of overlays. Paths, arcs and nodes are the generic shapes: a new use is new data for
an existing overlay, or one new overlay.

## 6. First PR, use cases

| use case | asked by | output |
|---|---|---|
| A slower than B? | regressions, ports | `compare`, as today |
| LP and bound against iteration or time, many runs | `T-2.3`, `T-2.4`, `T-2.1` | convergence curves |
| one option swept (ng size, buckets): LP and time | `T-1.31`'s runs | series plot and table |
| an instance, with or without a solution | debugging, slides | map with routes, time-window chart |

Later, by need: performance profiles and shifted geometric means (`T-1.22`, `T-1.37`), the time split per iteration
(`T-1.30`), box plots of rounds (noise), LaTeX tables for slides, replays' agreement (`T-1.26`), pages (`T-1.36`).

## 7. Code rules, tests, docs, branch (topics 7-10, 2026-11-27)

Facts: pyright strict covers `benchmarks/column_generation` already (`pyproject.toml:96`); the pre-commit hooks cover
`benchmarks/`, pyright is run by hand; ci-full installs `requirements.txt` and runs `benchmarks/column_generation/tests`
on Python 3.11-3.14 (`.github/workflows/ci-full.yml:127`); `tests/benchmark_folders.py` writes benchmark folders by
hand for `summary` and `compare`'s tests.

`D-435`-`D-440` in `decisions.md`.

## 8. As built

| item | value |
|---|---|
| branch | `feature/column-generation-analysis` in `C:\dev\rcspp-options`; first on #128 (`5b03395c`, `9617552a`), then on #131's head `8cab04a1` (the debug checks, finished first: `D-370`) |
| commits | `a70afbc3` the core; `926c717a` `compare` moved, `ANALYSIS.md`; `62bd4189` `convergence`; `a3c0ef71` `sweep`; `97ec7a7e` `draw` (each reviewed by the user) |
| docs | `ANALYSIS.md` beside the benchmark's README, which keeps a short section and the command table (the user, after commit 4: the README grew too fast) |
| checks | the benchmark suite 606 passed on #128's base, 613 on #131's; pyright strict and pre-commit clean; `D-440`'s done-when on real runs (folders in the worktree's ignored `results/`) |
| PR | description drafted (no plan IDs); not pushed |
