# Column generation measurements: handoff

State of the work on 2026-11-01, written so a new chat (possibly on another machine) can continue. The plan is
`plans/colgen/measurements.md`; this file is what happened and what is left. Branch `feature/column-generation`.

## What exists

Quick scripts in `benchmark-results/colgen/` (not shipped, left out of the PR branches).

| file | role |
|---|---|
| `drivers.py` | one run in one process, last line `RESULT <json>`: `example` (Python VRP example), `layer` (new layer), `cpp` (C++ example) |
| `run_matrix.py` | the runner: one subprocess per run, resumable, stats, logs, `summary.md` |
| `run_matrix.ps1` | launcher: builds what is missing, sets `PYTHONPATH`, starts the runner with the venv's Python |
| `after_large.sh`, `chain.sh` | the single-start runs queued behind the large run (`chain.sh` waits for a stop, then runs it) |
| `small/`, `large/`, `single-small/`, `single-large/` | results: `results.csv`, `summary.md`, `status.txt`, `machine.csv`, `logs/` |

Configurations (names in `run_matrix.py`): `example-mip` (Python example, python-mip picking Gurobi), `example-cpp` (C++
example `rcspp-ng-cg-comparison`, ng size 0 = cycles allowed), and `layer-{cpp,python}-{highs,highspy,gurobipy}-{exact,
heuristic}` (C++ `ColumnGeneration.run()` or the Python loop `examples/python/column_generation_loop.py`; exact =
`simple`, heuristic = `greedy` then `simple`). Instances are in `instances/`: small C101_25, R101_25, R201_25,
RC201_12, C101_50, R101_50, RC101_50; large C101, R101, RC101, C201, R201, RC201.

## How to run (Windows, this machine's setup)

- From the repo root: `powershell -ExecutionPolicy Bypass -File benchmark-results\colgen\run_matrix.ps1 --output
  benchmark-results/colgen/<folder> --instances A,B --configs a,b [--repeats N]`. The bypass is needed because
  `.ps1` scripts are blocked on that machine; it is per process.
- An existing output folder resumes (runs only what is missing). A new one starts from scratch.
  `--summary-only` rewrites `summary.md` from `results.csv`.
- Stop cleanly: create a file `STOP` in the output folder; the runner stops after the run in progress.
  `status.txt` shows the run in progress, refreshed every 5 s.
- Environment variables read by the layer driver: `COLGEN_INIT=single` (start from the example's single-customer
  columns instead of artificial columns), `COLGEN_ARTIFICIAL_COST=<x>` (explicit artificial cost; only tried by hand).
- Needs: `.venv` with highspy, gurobipy (licensed), python-mip, psutil, numpy; `build-python-highs` built (`_core`).
  The C++ example needs a `main` worktree at `../rcspp-main` with `USE_VRP` and Gurobi (`GUROBI_HOME`), built in
  `build-v2`; the launcher does that. It edits `examples/cpp/vrp/vrp.hpp` there (`kRouteRelaxation` from `None` to
  `NgPath`, a `static_assert` of the example); nothing in this branch is touched. The worktree is not tracked.
- Commits here run the repo's `pre-commit` (markdownlint, line length 120): put `.venv/Scripts` on `PATH` or the
  hook is not found. Do not skip it.

## Driver facts

- Graph: `VRP(instance).construct_resource_graph()` from `examples/python/vrp/vrp.py`. The example's graph has an
  infinite-cost depot to depot arc (the empty route); the layer rejects a column built from it ("cost or coefficient
  not finite"), so the layer driver subclasses `VRP` and skips that arc. Arcs out of customer `i` cover master row
  `i`; the depot is node 0, so master row 0 is free (bounds minus infinity to plus infinity) and rows 1 to n are `= 1`.
- Example driver: `VRP.solve()` is timed from outside; `MasterProblem.solve` is patched in the driver to get the LP
  value (the example returns the integer solve's value), the master time and the iteration count. `rest_s` excludes
  its final integer solve, reported as `integer_s`.
- The C++ example prints only LP, iterations and seconds (so no columns or master/pricing split). It caps each pricing
  solve at 120 s; "NOT proven optimal" is reported as status `NotProven` (never seen so far).
- Time columns: `wall_s` includes about 1 s of Python start-up; `solve_s` = master + pricing + rest. Memory and CPU
  sum the process tree (the venv `python.exe` is a launcher). `other_cpu_mean` is the CPU of other processes during the
  run (in % of one core); `summary.md` marks runs above 50% with `*`.

## Results (cycles allowed, solve seconds, noisy machine)

LP values agree in every configuration that finished: C101 828.9369 (matches the baseline 828.94), R101 1636.3887,
RC101 1572.7390, C201 591.5566, R201 1084.9907, RC201 1112.8414 (layer only: no example run on RC201), and all small
instances (C101_25 191.8136, R201_25 449.6598, RC201_12 427.2356, C101_50 363.2468, R101_50 1046.7011, RC101_50 828.3619).

| | C101/R101/RC101 | C201 | R201 | RC201 |
|---|---|---|---|---|
| Python example | 1 to 3 s | 51 s | 1745 s | not run |
| C++ example | 1 to 3 s | 53 s | 2093 s | not run |
| layer, single-customer start (6 configs) | 1 to 3 s | 25 to 37 s | 1730 to 1980 s | 990 to 1190 s |
| layer, default artificial start | 1 to 3 s | 27 to 44 s | 3100 to 7440 s, or HiGHS `SolverError` | not run |

Findings:

- The Python and C++ loops show no measurable difference: pricing is over 95% of the time on the hard instances.
- HiGHS, highspy and Gurobi differ only through different dual solutions (so different iteration counts), not solver
  speed.
- Default start: the automatic artificial cost is `automatic_cost_factor * context.cost_scale`
  (`cpp/rcspp/column_generation/master_initialization.cpp`), 7e7 on R2 instances. HiGHS fails with "excessive dual
  values" (at once on R201_25 and RC201_12; after 52 to 59 min of the first pricing on R201); with Gurobi, R201 takes
  7000 s against 1745 s for the example. Starting from the example's columns (`COLGEN_INIT=single`) fixes both.
  Explicit cost 1e5 also fixed R201_25 by hand. The slowness is an inference (first pricing with huge duals), not
  measured per iteration.
- Heuristic pricing (greedy, then simple) without a column limit explodes: about 140,000 columns per call on C101_25
  (5 to 16 s against 1 s for exact), and memory reached 98% on R201_25. Heuristic runs exist only for C101_25 and
  R101_25 (in `small/`); they are excluded from the large runs.

## Caveats

- Timings are inflated by other processes (55 to 115% of a core during the large runs, `machine.csv`). For quotable
  numbers, rerun a few runs on an idle machine.
- Not run: RC201 with the default start, the examples on RC201, extra instances (C102, R102, RC102, C202, R202, RC202,
  R103 were planned), repeats (all `--repeats 1`), v3 (C++ driver for the new layer).
- Only the cycles-allowed relaxation: no ng-routes, no elementarity. 100 customers at most.

## Issues for the layer (not fixed: the layer code was not changed)

1. Automatic artificial-column cost too large on R2 instances (fix: lower the scale, or make the HiGHS master solver
   robust to large costs).
2. `greedy` stage has no column limit per call.
3. The default start makes the Gurobi layer 4 times slower than the Gurobi example on R201 (inferred, see above).

## Commits (nothing pushed)

`bb40b192` (v1 scripts and small results), `91435338` (large and single-start results, runner changes) and the one
that adds this file. The user asked not to push; push before continuing on another machine, or copy the folder.

## For the presentation (discussed)

Lead with correctness and parity (one LP table, the baseline match), then timing parity; show the artificial-start
finding as a lesson with its fix; state the noise caveat; do not claim speed wins (the claim is accuracy and time on a
par with the examples, plus a solver-agnostic, Python-accessible design); state the limits above; end with next steps
(layer fixes, an ng-route comparison with `rcspp-ng-cg-comparison`). Against state-of-the-art branch-price-and-cut
solvers (ng-routes, bidirectional bucket labeling, completion bounds, cuts) these timings are not competitive; that is
the pricing algorithm, not the layer. This comparison is from general knowledge, nothing was measured against them.
