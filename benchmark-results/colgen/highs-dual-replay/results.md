# HiGHS "excessive dual values" on the big-M start

Laptop, 2026-11-02, highspy 1.15.1 (the vendored HiGHS is 1.15.1 too). Python loop, `HighsMasterSolver`, exact
pricing, default artificial start.

## Scripts

Run from any folder, with `PYTHONPATH=build-python-highs/python` and the `.venv` Python.

| script | does |
|---|---|
| `dump_lp.py INSTANCE [cost]` | runs the layer one iteration at a time and writes each master LP as `INSTANCE_<cost>_it<k>.mps` in the current folder, until a solve fails |
| `warm.py LP.mps [cost=x] [cold=1] [verbose=1] [option=value…]` | the layer's two solves in plain highspy: the artificial columns alone, then the priced columns added and solved again from that basis. `cost` replaces the artificial cost, `cold=1` retries a failed solve after `clearSolver()` |
| `full.py INSTANCE [option=value…]` | a whole run with HiGHS options set on the master solver |

The MPS writer drops free rows (row 0, the convexity row); the failure reproduces without them.

## Where it fails

- Message: `HEkkDual.cpp:727` / `:987`, on `kRebuildReasonChooseColumnFail`, raised when the bound-flipping ratio
  test (`HEkkDualRow::chooseFinal`) returns an error.
- Here: `chooseFinalWorkGroupQuad` (`HEkkDualRow.cpp:349`), "No change in loop 2" (shown with `log_dev_level=3`):
  the step `selectTheta` stays equal to `remainTheta` (5.56e8 on R201_25, 5.48e8 on RC201_12), with a dual norm of
  5.7e10 and 4.3e10.
- Cause, as the code reads: a candidate's breakpoint is `(dual + Td) / alpha`, with `Td` the
  `dual_feasibility_tolerance` (1e-7). Once `|dual|` is near 2^30 ≈ 1.07e9, `dual + 1e-7 == dual` in double precision,
  `(dual / alpha) * alpha` can fall below `dual`, and the loop stops moving. So the threshold is not a constant: it
  is `|dual| · 2^-53 ≈ Td`, on HiGHS's internal (scaled) reduced costs.
- `kMaxSelectTheta = 1e18` (`HEkkDualRow.h:22`) is the other hard limit of that loop: a constant, not an option, and
  not the one hit here.
- It fails on the second solve (2032 columns on R201_25), warm-started with the dual simplex at iteration 0 of dual
  phase 1. The same LP solved cold (presolve on) is `Optimal` in 50 iterations.

## Threshold (artificial cost, warm replay, same columns)

| instance | cost scale | automatic cost | last passing cost | first failing cost | ratio to the cost scale |
|---|---|---|---|---|---|
| R201_25 | 69.46 | 6.95e7 | 5.344e7 | 5.344e7 | 7.7e5 |
| RC201_12 | 91.26 | 9.13e7 | 7.009e7 | 7.010e7 | 7.7e5 |

Every cost from 1e3 to 4e7 passes on R201_25; 1e8 and 1e9 fail. The automatic factor 1e6 is just above the line.

## Options (warm replay, R201_25)

| option | 6.9e7 | 1e8 | 1e9 | 1e10 |
|---|---|---|---|---|
| default | error | error | error | |
| `dual_feasibility_tolerance=1e-6` | Optimal | Optimal | error | error |
| `user_objective_scale=-7` (HiGHS's own advice in its warning) | Optimal | Optimal | Optimal | error |
| `simplex_strategy=4` (primal simplex) | Optimal | Optimal | Optimal | Optimal |
| `simplex_scale_strategy=0` | error | | | |
| `solver=ipm`, `solver=pdlp` | Optimal | | | |
| fallback: `clearSolver()` then solve again | Optimal | | Optimal | Optimal (and 1e12) |

`user_objective_scale` returns unscaled values and duals. Duals differ between the passing runs (degenerate LP).

## Whole runs (`full.py`, default automatic cost)

| instance | default | `dual_feasibility_tolerance=1e-6` | `user_objective_scale=-7` | `simplex_strategy=4` |
|---|---|---|---|---|
| R201_25 | SolverError, iteration 1 | Optimal 449.6597531, 18 it. | Optimal 449.6597531, 15 it. | Optimal 449.6597531, 17 it. |
| RC201_12 | SolverError, iteration 1 | Optimal 427.2355768, 15 it. | Optimal 427.2355768, 14 it. | Optimal 427.2355768, 19 it. |

The LP values equal the runs from single-customer columns (`small/`, `single-small/`). R201 (100 customers) was not
run: its big-M first pricing takes about an hour.

## Fixes A and B (`ab_run.py`, `ab_runs.sh`, `ab-runs.txt`)

A: `fallback_master.py`, a pure-Python master solver over highspy that solves a failed LP again after
`clearSolver()`. B: artificial cost 1e4 times the cost scale instead of 1e6. Laptop, one run at a time; `progress/`
has one line per LP solve of the R201 runs. Tabu: cap 20, `max_iterations` 1000, then `simple`.

| instance | master | start | pricing | status | LP | iterations | columns | solve s | master s | pricing s | retried solves |
|---|---|---|---|---|---|---|---|---|---|---|---|
| R201_25 | C++ HiGHS | 1e6 | exact | SolverError | | 1 | 2007 | 0.3 | | | |
| R201_25 | A | 1e6 | exact | Optimal | 449.6597531 | 16 | 4460 | 0.6 | 0.2 | 0.4 | 2 |
| R201_25 | C++ HiGHS | 1e4 | exact | Optimal | 449.6597531 | 16 | 4185 | 0.4 | 0.1 | 0.3 | |
| R201_25 | A | 1e4 | exact | Optimal | 449.6597531 | 16 | 4185 | 0.7 | 0.1 | 0.5 | none |
| RC201_12 | A | 1e6 | exact | Optimal | 427.2355768 | 18 | 2466 | 0.2 | 0.1 | 0.1 | 2 |
| RC201_12 | A | 1e4 | exact | Optimal | 427.2355768 | 14 | 2553 | 0.1 | 0.0 | 0.1 | none |
| R201_50 | A | single customers | exact | Optimal | 756.3386122 | 28 | 12671 | 8.6 | 1.5 | 7.1 | none |
| R201_50 | A | 1e6 | exact | Optimal | 756.3386122 | 26 | 19503 | 27.7 | 2.5 | 25.1 | 2, 3 |
| R201_50 | C++ HiGHS | 1e4 | exact | Optimal | 756.3386122 | 33 | 17929 | 20.3 | 1.7 | 18.5 | |
| R201_50 | A | 1e4 | exact | Optimal | 756.3386122 | 33 | 17929 | 18.9 | 1.7 | 17.1 | none |
| R201_50 | A | 1e4 | tabu | Optimal | 756.3386122 | 44 | 5072 | 2.9 | 0.3 | 2.6 | none |

- A alone fixes the failure: one or two retried solves, right after the first pricings.
- B alone fixes it too: no solve fails at 1e4, so A never fires with B. A stays as the net for costs users set.
- The big-M start still costs pricing time on R201_50 (17–25 s against 7 s from single-customer columns, `F-39`);
  tabu first removes it: 2.9 s, faster than the single-customer start.

R201 (stopped 2026-11-02 by hand, to resume): A+B exact was still in its first pricing after 25 min (one LP solved,
100 artificial columns at 9.2e5); the three other R201 runs did not start. Resume with `ab_runs.sh` (the R201_50
runs are commented out).
