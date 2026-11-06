# Column generation measurements: handoff

State of the work on 2026-11-01, written so a new chat (possibly on another machine) can continue. The plan is
`plans/colgen/measurements.md`; this file is what happened and what is left. Branch `feature/column-generation`.

## Update 2026-11-02 (after the presentation)

What is next now lives in the plan (`plans/colgen/README.md` status, `T-1.13` to `T-1.16`); "Next" below is
superseded. Two findings below were corrected: the master solvers do differ in speed (`F-38`), and the big-M first
pricing is measured, not inferred (`F-39`).

- **Two machines.** `small/`, `large/`, `single-*` come from the desktop (`C:\Users\marbe\Desktop\...`); everything
  below from the laptop (`C:\Users\marbe\OneDrive\Desktop\...`). Timings do not compare across machines.
- **Laptop setup (done):** `psutil` in `.venv`; `build-python-highs` configured with `-DUSE_PYTHON=ON
  -DUSE_TESTS=OFF -DUSE_COLUMN_GENERATION_HIGHS=ON -DPython_EXECUTABLE=<repo>/.venv/Scripts/python.exe`, `_core`
  built. The `rcspp` installed in `.venv` has no HiGHS master solvers: keep `PYTHONPATH=build-python-highs/python`
  (the launcher sets it). No `../rcspp-main` worktree, so `example-cpp` does not run here.
- **New in the driver:** `COLGEN_HEURISTIC_CAP=<n>` sets greedy's `stop_after_X_solutions`.
- **New script:** `greedy_replay.py --instance R201_50 [--cap 100] [--timeout 60]` runs the Python example, records
  the duals of each iteration, and prices the first, middle and last ones with `simple`, capped `greedy` and
  capped `tabu` (`--tabu-cap 20 --tabu-iterations 1000` by default; `F-40`, `D-117`). Output:
  `heuristic-replay/R201_50.txt`.
- **New results:** `heuristic-cap100/` (C101_25, R101_25, all 12 layer configurations, cap 100);
  `heuristic-cap100-medium/` (R201_50 from single-customer columns: HiGHS exact, gurobipy exact, the Python example;
  the capped heuristic run was killed after 19 min and has no row); `r201-cost1e5/` (Gurobi with cost 1e5 killed
  after 110 min, no row). Findings: `F-39`, `F-40`.
- **Slides:** `presentation/slides.tex` (beamer; the colleague's style).
- **HiGHS failure on the big-M start:** `highs-dual-replay/` (scripts and `results.md`): where HiGHS stops, the
  threshold (7.7e5 × the cost scale), the options and fallback that fix it (`F-41`, `Q-43`).

## What exists

Quick scripts in `benchmark-results/colgen/` (not shipped, left out of the PR branches).

| file | role |
|---|---|
| `drivers.py` | one run in one process, last line `RESULT <json>`: `example` (Python VRP example), `layer` (new layer), `cpp` (C++ example) |
| `run_matrix.py` | the runner: one subprocess per run, resumable, stats, logs, `summary.md` |
| `python_layer_probes.py` | probes of the pure-Python layer (copies of the HiGHS solution, integer widths, `add_columns`, `ColumnBatch` copies) |
| `pricing_gap_probe.py` | the pure-Python layer's extra time against the C++-backed one, split by slice (`--pricing exact\|heuristic\|tabu`); results in `pricing-gap/` |
| `run_matrix.ps1` | launcher: builds what is missing, sets `PYTHONPATH`, starts the runner with the venv's Python |
| `after_large.sh`, `chain.sh` | the single-start runs queued behind the large run (`chain.sh` waits for a stop, then runs it) |
| `small/`, `large/`, `single-small/`, `single-large/` | results: `results.csv`, `summary.md`, `status.txt`, `machine.csv`, `logs/` |

Configurations (names in `run_matrix.py`): `example-mip` (Python example, python-mip picking Gurobi), `example-cpp` (C++
example `rcspp-ng-cg-comparison`, ng size 0 = cycles allowed), and `layer-{cpp,python}-{highs,highspy,gurobipy}-{exact,
heuristic}` (C++ `ColumnGeneration.run()` or the Python loop `examples/python/column_generation_loop.py`; exact =
`simple`, heuristic = `greedy` then `simple`), and `layer-purepython-{highspy,gurobipy}-{exact,heuristic}` (the
pure-Python layer `rcspp.python_column_generation`, driver option `--loop purepython`; run with
`PYTHONPATH=python/src`, the launcher's build folder has no such package; results of 2026-11-06 in
`pure-python-quick*/`, plan `python-layer-runs.md`). Instances are in `instances/`: small C101_25, R101_25, R201_25,
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
- Heuristic pricing (greedy, then simple) ran uncapped and exploded: about 140,000 columns per call on C101_25
  (5 to 16 s against 1 s for exact), and memory reached 98% on R201_25. Heuristic runs exist only for C101_25 and
  R101_25 (in `small/`); they are excluded from the large runs. **The cap exists, the driver did not set it**
  (`drivers.py`, `cg.PricingStage("greedy")` without params): `params = rcspp.graph.AlgorithmParams()`, then
  `params.stop_after_X_solutions = N` and `cg.PricingStage("greedy", params)` (the field is bound in
  `python/bindings/graph.cpp`). Set it before any heuristic rerun, or memory fills again.

## Caveats

- Timings are inflated by other processes (55 to 115% of a core during the large runs, `machine.csv`). For quotable
  numbers, rerun a few runs on an idle machine.
- Not run: RC201 with the default start, the examples on RC201, extra instances (C102, R102, RC102, C202, R202, RC202,
  R103 were planned), repeats (all `--repeats 1`), v3 (C++ driver for the new layer).
- Only the cycles-allowed relaxation: no ng-routes, no elementarity. 100 customers at most.
- "C++ example" is `main` with one local edit (`kRouteRelaxation` in `examples/cpp/vrp/vrp.hpp`, see above), run as
  `rcspp-ng-cg-comparison` with ng size 0: not the stock `rcspp-vrp-benchmark`.
- The C++ Gurobi master solver of the layer was not measured (v3 not done). For the C++ or Python Gurobi decision, the
  proxy is the C++ example (Gurobi's C++ API) against the layer with gurobipy, in a regime where pricing is over 95% of
  the time: it says the Python master costs nothing measurable there, not that it never does.

## Issues for the layer (not fixed: the layer code was not changed)

1. Automatic artificial-column cost too large on R2 instances: `ArtificialColumns` uses
   `automatic_cost_factor` (1e6, `master_initialization.hpp`) times the cost scale, which a `GraphPricer` reports as
   its largest arc cost (`graph_pricer.hpp`, `cost_scale`), so about 7e7. It is the layer's default start: a user
   without starting columns hits the HiGHS failure or the slowdown. To fix before shipping (lower factor, a bound,
   or a HiGHS fallback), then rerun R201_25, RC201_12 and R201 with the default start.
2. Heuristic stages have no cap unless the user sets one (see Findings): decide whether a heuristic stage gets a
   default cap.
3. The default start makes the Gurobi layer 4 times slower than the Gurobi example on R201 (inferred, see above);
   expected to go with 1.

## Commits

`bb40b192` (v1 scripts and small results), `91435338` (large and single-start results, runner changes), `7f307d4c`
(this file), and the plan commit after it: all pushed on 2026-11-01 with `feature/column-generation`. The PR branches
are pushed too; the packaging PR is #80 (draft).

## Starting on another machine

1. `git fetch`, check out `feature/column-generation`, `git lfs pull` (the instances are in LFS). Read
   `plans/colgen/README.md` (agent protocol), then this file and `plans/colgen/measurements.md`.
2. Python: a venv with `tests/python/requirements.txt`, then `pip install gurobipy mip psutil` (the requirements leave
   python-mip out on Windows; it works there). Gurobi needs the machine's license (`GUROBI_HOME`, a license file);
   without one, skip the gurobipy and Gurobi configurations (`--configs`).
3. Build with HiGHS for the layer configurations: `cmake -S . -B build-python-highs -DUSE_PYTHON=ON
   -DUSE_COLUMN_GENERATION_HIGHS=ON [-DUSE_COLUMN_GENERATION_GUROBI=ON]` (the options were renamed from `USE_HIGHS`
   and `USE_GUROBI`), then build `_core`. Run with `PYTHONPATH=build-python-highs/python`. Code that imports from
   `python/src` looks for the module in `build/`, `out/` or `cmake-build-*/`: on the first machine a junction
   `cmake-build-release/python/rcspp` pointed at `build-python-highs/python/rcspp`.
4. The C++ example needs the `../rcspp-main` worktree and its `build-v2` (the launcher makes both, with `USE_VRP` and
   Gurobi); on Linux, adapt the `.ps1` steps or build `rcspp-ng-cg-comparison` by hand.
5. Results of a new machine go in new output folders (machine names in the folder or in `--output`), not mixed with
   these: the timings are machine-specific.

## Next (in order)

1. Prepare the presentation (below); the numbers above are enough.
2. Optional, short: rerun the heuristic configurations on the small instances with a cap
   (`stop_after_X_solutions`, e.g. 50 to 200), so the heuristic row shows the stage working.
3. Optional: a few runs on an idle machine with `--repeats 3` for quotable timings.
4. Layer fixes (issues 1 and 2) are code on the PR branches: plan them with the user first (`plans/colgen/roadmap.md`
   `T-1.13`, `T-1.14`).

## For the presentation (discussed)

Colleague's goal (2026-10-30): ship column generation ready to use, really small (RCSPP is about the pricer),
highly customizable in C++ and Python, and not changing how RCSPP works today. Talk about the critical decisions (HiGHS
and Gurobi, C++ and Python) and how a user writes it. Format: a few minutes of talk with tables, maybe LaTeX later;
English. Points for the goal: column generation is its own library and option (the core builds and passes its 597
tests without it, `rcspp.hpp` no longer includes it); HiGHS in the wheel costs no import time, the wheel is about
3.4 MB (`plans/colgen/inventory.md` `F-35`).

Lead with correctness and parity (one LP table, the baseline match), then timing parity; show the artificial-start
finding as a lesson with its fix; state the noise caveat; do not claim speed wins (the claim is accuracy and time on a
par with the examples, plus a solver-agnostic, Python-accessible design); state the limits above; end with next steps
(layer fixes, an ng-route comparison with `rcspp-ng-cg-comparison`). Against state-of-the-art branch-price-and-cut
solvers (ng-routes, bidirectional bucket labeling, completion bounds, cuts) these timings are not competitive; that is
the pricing algorithm, not the layer. This comparison is from general knowledge, nothing was measured against them.
