# Measurements for the 2026-10-30 presentation

**Status (2026-11-01):** v1 and v2 done (the runner, the layer from Python, both current examples), small and
large results committed; v3 not done. What happened, results, caveats, how to restart on another machine and
what is next: `benchmark-results/colgen/HANDOFF.md`. Findings for the layer: `F-36` → `T-1.13`, `F-37` →
`T-1.14`.

Quick and dirty scripts, not shipped: they live in `benchmark-results/colgen/` on `feature/column-generation` (that
folder is left out of the PR branches). Goal: tables of the current VRP examples and of the new layer, small instances
first, large ones left running overnight. English. Not code for review: no lint hooks run there.

## Matrix (18 configurations per instance)

| group | loop | masters | pricing | stage |
|---|---|---|---|---|
| current Python example (`examples/python/vrp`) | its own | python-mip (picks Gurobi here) | as is | v1 |
| new layer from Python | C++ `ColumnGeneration.run()` and the Python loop (`examples/python/column_generation_loop.py`, `run(master)`) | `HighsMasterSolver` (arrays, C++ HiGHS), `HighspyMasterSolver`, `GurobipyMasterSolver` | exact `simple`; `greedy` then `simple` (`PricingStage`) | v1 |
| current C++ example (`rcspp-ng-cg-comparison`, takes an instance by name) | its own | Gurobi C++ | as is | v2: built from a `main` worktree with `USE_VRP` and Gurobi |
| new layer, C++ driver | C++ | `HighsMasterSolver`, `GurobiMasterSolver` | exact; heuristic then exact | v3, optional |

Instances (`instances/`, git LFS): small C101_25, R101_25, R201_25, RC201_12, C101_50, R101_50, RC101_50; large
(end of day) C101, R101, RC101, then C201, R201, RC201. Reference LP values (current C++ example, cycles allowed):
`baseline.md` (C101 828.94, R101 1636.39).

## Runner requirements (user)

- **No timeout.** One subprocess per run (isolation, process stats).
- **Stop and resume:** one row appended to `results.csv` per finished run; Ctrl-C loses only the run in progress.
  `--output <folder>`: an existing folder resumes (runs only what is missing), a new one starts from scratch.
  `--repeats N` (repeat index in the key), filters `--instances`, `--configs`.
- **Progress:** a line per run (n/total, instance, config, elapsed, ETA from finished runs) and `status.txt`.
- **Stats per run:** wall, CPU user/system, peak memory (psutil polling the child; `peak_wset` on Windows); from the
  run: status, LP value, iterations, columns, master (relaxation) time, pricing time, rest (= Python overhead).
  Each driver prints a last line `RESULT <json>` the runner parses.
- **Machine stats:** a sampler thread writes total CPU %, memory and the busiest other processes every few seconds
  to `machine.csv` (the user works on the machine meanwhile).
- **Logs:** each run's stdout and stderr in `logs/<instance>-<config>-<repeat>.log`.
- **Summary:** `summary.md`, tables per instance and per configuration, LP mismatches flagged, the baseline compared.
- **Launcher** `run_matrix.ps1`: builds what is missing (v1: `_core` in `build-python-highs`; v2: the `main`
  worktree), then starts the runner with the venv's Python.

## Facts for the drivers

- Graph: `VRP(instance).construct_resource_graph()` (`examples/python/vrp/vrp.py`): real resources cost, time
  windows, capacity (no elementarity: cycles allowed, as the baseline). An arc out of customer `i` covers row `i`
  (`Row(orig_id, 1.0)`); the depot is node 0, the sink `n + 1`, so master rows are indexed by customer id: give row 0
  free bounds or shift the ids. Check `calculate_distance` against the baseline's convention before comparing LPs.
- Instance reader: `examples/python/vrp/instance_reader.py`. Paths: add `examples/python` and `python/src` to
  `sys.path`, as `examples/python/solve_vrp.py` and `tests/python/test_vrp.py` do.
- Layer API: `tests/python/test_column_generation_end_to_end.py` (fixture `UserModel`: arrays, highspy, gurobipy) is
  the closest model. Result fields: `result.objective`, `.iterations`, `.statistics.relaxation_seconds`,
  `.pricing_seconds`, `.generated_columns`; the Python loop returns a `LoopResult` with the same fields, flat.
- Current Python example: `VRP.solve()`; it prints its own log, so time it from outside and read its LP value.

## Environment (this machine)

- `.venv` has highspy, gurobipy (licensed), python-mip 2.0.0 (CBC and Gurobi work), psutil, numpy.
- Build with HiGHS and Gurobi: `build-python-highs` (Release, Visual Studio 18 2026); `cmake --build
  build-python-highs --config Release --target _core`. Run with `PYTHONPATH=build-python-highs/python`; the junction
  `cmake-build-release/python/rcspp` points at it too, for code that imports from `python/src`.
- Builds take minutes: run them in the background. The full Python test suite is slow; run only what is touched.
