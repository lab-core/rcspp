# Pure-Python column generation layer (`T-1.15`): the full comparison

Step H of `python-layer.md` §8 (2026-11-06, Opus): the step G runs read (`python-layer-runs.md`), the gap probed,
the full matrix and `Q-41`'s rule planned. Proposed: `D-138` (matrix), `D-139` (rule), `D-140` (D.12). Findings:
`F-47`, `F-48`. Results (§6, 2026-11-08): all 108 cells equivalent, `F-49`, proposed `D-141`; analysis
for the user in §7. No code under `python/` or `tests/` changed; no layer commit touched; nothing pushed.

## 1. Where the gap comes from (`F-47`)

`benchmark-results/colgen/pricing_gap_probe.py --instance <name> --pricing exact|heuristic|tabu --cap <n>
--repeats 7`: C++-backed and pure runs alternate in one process (highspy master, single-customer start, the driver's
graph); timers split the pure pricing slice (wrappers, inner C++ calls, `_to_pricing_result`, `add_columns`,
`to_arrays`). Results: `benchmark-results/colgen/pricing-gap/` (laptop, VS Code and a browser open, not idle).

LP value, iterations and columns equal in every run (9 cells × 7 repeats × 2 layers). Solve ms, median [min–max]:

| instance | pricing | iterations | C++-backed | pure | difference | per iteration |
|---|---|---|---|---|---|---|
| C101_25 | exact | 18 | 46.9 [42.6–70.6] | 67.1 [60.3–78.1] | +20.2 (+43 %) | 1.12 |
| R201_25 | exact | 12 | 596.5 [486–617] | 599.4 [530–753] | +2.9 (+0.5 %) | (search noise) |
| RC201_12 | exact | 16 | 86.0 [84.6–95.7] | 106.7 [102.5–118.1] | +20.7 (+24 %) | 1.29 |
| C101_50 | exact | 54 | 389.3 [376–424] | 460.5 [430–488] | +71.2 (+18 %) | 1.32 |
| C101_25 | greedy 100 | 27 | 536.5 [504–682] | 601.3 [536–630] | +64.8 (+12 %) | 2.40 |
| C101_25 | tabu 20 | 66 | 1041.9 [1004–1124] | 1109.6 [1078–1157] | +67.7 (+6.5 %) | 1.03 |
| R201_25 | tabu 20 | 22 | 360.6 [355–409] | 393.4 [376–403] | +32.8 (+9.1 %) | 1.49 |
| RC201_12 | tabu 20 | 22 | 293.1 [281–302] | 323.3 [308–342] | +30.1 (+10 %) | 1.37 |
| C101_50 | tabu 20 | 67 | 1960.5 [1880–3155] | 2079.4 [1978–3596] | +118.9 (+6.1 %) | 1.77 |

The pure slices against the C++-backed run (medians, ms per run). The search itself (`solve_inner`) is the same C++
call in both layers: the C++-backed pricing slice is that search plus under 1 ms.

| ms per run | C101_25 exact | RC201_12 exact | C101_50 exact | C101_25 tabu | C101_50 tabu |
|---|---|---|---|---|---|
| difference, total | 20.2 | 20.7 | 71.2 | 67.7 | 118.9 |
| master slice difference | +3.3 | +2.8 | +11.1 | +7.9 | +11.6 |
| `add_columns` (the C++-backed one: 0.6–2 ms) | 9.5 | 11.2 | 28.7 | 25.9 | 32.0 |
| of which `Column.to_arrays` | 1.5 | 2.2 | 3.8 | 1.1 | 2.0 |
| GraphPricer's own Python (wrappers, stage loop, `_to_pricing_result`) | 3.1 | 3.8 | 10.3 | 10.2 | 12.9 |
| the loop's `_price` | 0.3 | 0.2 | 1.0 | 1.1 | 1.5 |
| the loop's "rest" (outside master and pricing) | 4.0 | 3.1 | 14.5 | 9.5 | 20.2 |

- **About 1–2 ms per CG iteration, plus 2–5 ms once**, in four places; the largest is `add_columns`.
- `add_columns` (27–54 %): **a fixed ~0.4 ms per call**, not per column (C101_25 tabu: 66 calls of ~6 columns,
  26 ms). cProfile of that run: the master solver's `add_columns` 37 % (its checks 13 %, HiGHS `addCols` 9 %),
  `ColumnBatch` 14 %, de-duplication 9 %, `to_arrays` 3 %, small numpy calls the rest.
- Master slice (10–20 %): `solve_relaxation` sets and restores 8 HiGHS options per solve through context managers.
- GraphPricer (15 %): `ResourceGraph` wrappers 0.6–1.1 ms, `_to_pricing_result` 2–7 ms (building
  `SolveResult.solutions` alone: 0.1–0.35 ms).
- Rest (15–20 %): `duals()` and `objective()` copies (B.15), records per iteration, and `cost_scale` once (one
  pybind `get_arc` per arc: 1.8 ms for 650 arcs).
- R201_25 exact: the search alone varies by ±10 % between runs, more than the whole gap.

## 2. Heuristic stages (`F-48`, `D-140`)

- Capped greedy (cap 100, then simple): C101_25 finished (above); R201_25 and RC201_12 did not finish 7 repeats in
  5 min and were stopped: once improving paths are rare, greedy enumerates (`F-40`). Not a stage for the matrix.
- Tabu (cap 20, 1000 iterations, then simple), the setup `D-117` suggests: cheap per call, few columns per call
  (6–43), more iterations. The gap per iteration stays 1.0–1.8 ms; in % it grows only because tabu makes the
  iterations shorter on these instances (+6–10 %).
- **D.12:** a C++ helper turning the `Solution` list into the batch (`D-112`) removes `to_arrays` and the
  concatenation: 1–4 ms per run here, 4–15 % of `add_columns` under tabu (35–64 % only on the exact runs' large batches,
  `python-layer-runs.md`). The cost with a heuristic stage is per call, in Python checks and small numpy
  arrays, which the helper does not touch.

## 3. The matrix (`D-138`)

| axis | values |
|---|---|
| layers × master solvers | `layer-{cpp,purepython}-{highspy,gurobipy}`: 4 (no `highs`: the pure layer has none, `D-124`) |
| pricing | `exact` (`simple`); `tabu` (tabu cap 20, `max_iterations` 1000, then `simple`). No greedy (`F-48`) |
| start | `single` (the example's single-customer columns); `bigm`: artificial columns, the **same explicit cost** in both layers, 1e4 × the pricer's cost scale (the C++ factor 1e6 is a constant and fix B is pure-only, `D-118`) |
| small tier, 5 repeats | C101_25, R101_25, R201_25, RC201_12, C101_50, R101_50, RC101_50, R201_50 |
| mid tier, 3 repeats | C101, R101, RC101, C201 (master-heavy: 190 iterations, 38,000 columns; HiGHS master 48–73 % of the time, `F-38`) |
| long tier, 1 repeat | R201, RC201, without `exact` + `bigm` (R201: 6836 s per configuration, pricing over 99 %, `F-41`; run last if time allows) |
| attribution | `pricing_gap_probe.py` (3 repeats) on C201 exact and tabu, and on every cell that fails `D-139` |

Driver changes first (benchmark scripts only, not the layer): `--pricing tabu` in `drivers.py` and its 4 configs in
`run_matrix.py`; `COLGEN_INIT=bigm` passing `ArtificialColumns(cost=1e4 * max(1, pricer.cost_scale()))` to both
layers; `pricing_gap_probe.py` also over gurobipy and with the big-M start; every run with `PYTHONPATH=python/src`.
The runner already runs a cell's configurations back to back in each repeat. One output folder per start
(`COLGEN_INIT` is read from the environment).

Run time (desktop figures, `HANDOFF.md`, `F-41`; RC201 with tabu and big-M is not measured, estimated from R201):

| tier | runs | estimate |
|---|---|---|
| small | 8 × 16 × 5 = 640 | 30–60 min (about 2 s each with start-up; R201_50 big-M exact 17–25 s) |
| mid | 4 × 16 × 3 = 192 | about 45 min (C201: 16 runs of 25–45 s per repeat) |
| attribution probes | C201 × 2 pricings × 3 × 2 layers | about 10 min |
| long | 2 × 12 = 24 | about 6 h (per configuration: R201 1850 + 206 + 1295 s, RC201 about 2200 s) |
| long, `exact` + `bigm` | 8 | about 12 h more |

- **Idle machine for everything timed**, and one machine (the desktop, where the long-run figures come from; laptop
  timings do not mix). Most sensitive: the small and mid tiers, whose differences (20–120 ms) are the size of the
  noise step G saw with other processes at 50–170 % of a core. Close VS Code, browsers and chat apps; re-run any
  run `summary.md` marks `*` (other processes above 50 % of a core). The long tier overnight.
- Gurobi license on that machine; no `rcspp-main` worktree needed (no `example-cpp`).

## 4. The rule for `Q-41` (`D-139`)

Per cell (instance × master solver × pricing × start), `C` and `P` = median solve seconds (master + pricing + rest,
no start-up) of the C++-backed and the pure layer.

1. **Gate:** both `Optimal`; LP values equal to 1e-6 relative. A difference → stop, a finding (as in step G).
   Equal LP but other iterations or columns → the cell is listed and left out of 2.
2. **Cell verdict:** equivalent when `P − C ≤ max(0.05 × C, 0.25 s)`; a faster pure layer is equivalent.
3. **Decision:**

| outcome | decision |
|---|---|
| every cell equivalent | Python only (`D-111`): the C++ layer and its bindings go; the pure package becomes `column_generation` (`python-layer.md` §2) |
| failing cells, and in each the attribution probe puts at least 2/3 of `P − C` in one component (`add_columns`, the master solver, the GraphPricer, the loop) | Python only, after that component is made faster (Python first, then C++ behind its interface, `D-112`) and the failing cells re-run equivalent |
| anything else | keep the C++ layer; `Q-41` goes back to the user and the colleagues with the table |

- **Expected outcome** from `F-47`: the gap is 1–2.4 ms per iteration, so every mid and long cell should be within
  1–2 % (C201: about 0.2 s per iteration over HiGHS). The small cells exceed 5 % by construction (+6 to +43 %);
  under 0.25 s absolute they pass (largest seen: +0.12 s, C101_50 tabu).
- **The floor is the user's choice.** 0.25 s is a quarter of Python's start-up, invisible to a user. With a floor of
  0.1 s, C101_50 under tabu fails (+0.12 s at 6.1 %) and, the gap being spread over four components (no 2/3), the
  third line would keep the C++ layer for 1–2 ms per iteration. A floor of
  `0.002 s × iterations` says the same as 0.25 s here and scales with the run.

## 5. Recommendation on D.12 (`D-140`, the user decides)

Unchanged by the heuristic runs: leave `add_columns` as it is, no C++ helper. The helper of `D-112` would save 1–4 ms
per run with tabu (`to_arrays` and the concatenation); the per-call cost is elsewhere. Should `D-139` single out
`add_columns`, the first step is fewer Python steps per call (checks, `ColumnBatch` of a few columns), measured with
`pricing_gap_probe.py`; a C++ helper only if that is not enough.

## 6. Results (2026-11-07/08, desktop; `F-49`, proposed `D-141`)

`D-138` run as planned (`full_comparison.sh`, 2026-11-07 02:00–07:57), except `exact` + `bigm` on R201 and RC201
(stopped by the user: pricing over 99 %, cannot change the verdict). Overnight re-runs (`overnight_reruns.sh`,
2026-11-08 01:35–05:09): RC201 tabu × 4 configurations × 3 repeats (`rerun-rc201-tabu/`), the small tier from the
single-customer start again on a quieter machine (`rerun-small-single/`, 5 repeats), a second repeat of the long tier
from the single-customer start (`full-long-single/`, now 2 repeats). 1,204 runs, **0 failed, no LP mismatch; LP
value, iterations and columns equal between the layers in every cell**. Verdict: `full_comparison_verdict.py`.
Machine not fully idle in the first night (Chrome, VS Code, Discord: other processes at 25–150 % of a core, peaks
3.5 cores at 01:35 on 2026-11-08); the overnight small tier at 17 % on average.

The tables use `rerun-small-single/` for the small tier from the single-customer start, the other `full-*/` folders
as they are: 108 cells, 872 runs (`presentation/python-layer/make_data.py`). Medians, pure − C++-backed:

| C++-backed run | cells | range (%) | median (%) | largest (s) | pure faster in |
|---|---|---|---|---|---|
| 0–1 s | 52 | +0.9 to +59.7 | +7.5 | +0.098 | 0 |
| 1–10 s | 30 | -0.9 to +7.5 | +2.5 | +0.199 | 4 |
| 10–100 s | 17 | -1.5 to +3.9 | +1.0 | +1.215 | 3 |
| over 100 s | 9 | -1.0 to +2.3 | +0.3 | +28.145 | 3 |

**`D-139`: 108 / 108 cells equivalent**, with the 0.25 s floor and with 0.1 s. Cells above 30 s:

| instance | master | pricing | start | C++-backed (s) | pure (s) | difference (s) | difference | repeats |
|---|---|---|---|---|---|---|---|---|
| C201 | gurobipy | exact | bigm | 32.4 | 32.4 | +0.02 | +0.1 % | 3 |
| C201 | highspy | exact | bigm | 32.7 | 33.3 | +0.63 | +1.9 % | 3 |
| C201 | highspy | exact | single | 36.8 | 37.8 | +0.95 | +2.6 % | 3 |
| RC201 | highspy | tabu | single | 48.0 | 49.1 | +1.03 | +2.1 % | 2 |
| C201 | highspy | tabu | single | 48.5 | 48.9 | +0.39 | +0.8 % | 3 |
| C201 | highspy | tabu | bigm | 49.7 | 50.7 | +1.02 | +2.0 % | 3 |
| C201 | gurobipy | tabu | single | 51.5 | 50.7 | -0.78 | -1.5 % | 3 |
| C201 | gurobipy | tabu | bigm | 61.1 | 61.0 | -0.11 | -0.2 % | 3 |
| RC201 | gurobipy | tabu | single | 67.0 | 67.9 | +0.84 | +1.3 % | 2 |
| RC201 | highspy | tabu | bigm | 75.4 | 76.6 | +1.21 | +1.6 % | 1 |
| RC201 | gurobipy | tabu | bigm | 130.1 | 131.4 | +1.23 | +0.9 % | 1 |
| R201 | gurobipy | tabu | single | 158.5 | 158.5 | +0.06 | +0.0 % | 2 |
| R201 | highspy | tabu | single | 188.3 | 190.7 | +2.40 | +1.3 % | 2 |
| R201 | gurobipy | tabu | bigm | 829.1 | 834.0 | +4.85 | +0.6 % | 1 |
| RC201 | gurobipy | exact | single | 944.3 | 941.5 | -2.81 | -0.3 % | 2 |
| RC201 | highspy | exact | single | 958.6 | 955.7 | -2.91 | -0.3 % | 2 |
| R201 | highspy | tabu | bigm | 1222.4 | 1250.5 | +28.14 | +2.3 % | 1 |
| R201 | gurobipy | exact | single | 1608.1 | 1612.8 | +4.67 | +0.3 % | 2 |
| R201 | highspy | exact | single | 1796.9 | 1778.1 | -18.86 | -1.0 % | 2 |

- RC201 tabu over highspy: +4.6 % with 1 repeat in the first night, +2.1 % over 2 repeats (`full-long-single/`) and
  over 3 (`rerun-rc201-tabu/`: 47.35 s against 48.34 s). Its gurobipy cell in the re-run: −16.5 %, noise (the first
  repeat ran with Chrome at a full core; spread 17.7 s).
- C201 (master-heavy, 190 iterations), probes (`full-probes/`, 3 repeats): exact 35.58 s against 36.52 s (+0.94 s:
  master +0.23, pricing slice +0.61 of which the pure `add_columns` 0.34 and the GraphPricer's Python 0.11, rest
  +0.06); tabu 46.04 s against 46.79 s (+0.75 s). The full matrix on C201: −1.5 % to +2.6 %.
- R201 exact: the two repeats of one layer differ by about 5 % (97 s): more than the gap between the layers.
- Draft slides for the user (not for presenting as they are): `benchmark-results/colgen/presentation/python-layer/`.

**Outcome by `D-139`'s table: Python only** (proposed `D-141`), once the user accepts `D-139`; it supersedes `D-02`,
`D-08`, `D-15`, `D-37` (C++ users lose the column generation layer) and decides the frozen stack's fate. The user
decides, with the colleagues.

## 7. For the user: the decisions, the analysis behind them, what is left (2026-11-08)

Written from the chat after the runs, so that another machine or session loses nothing.

**Decisions to take** (none accepted yet, except `D-138` for running):

| # | decision | proposal | why |
|---|---|---|---|
| 1 | `D-139`, the rule | accept, floor 0.25 s | fixed before reading the results was the intent; the outcome is the same with 0.1 s (largest small-run difference +0.098 s) |
| 2 | `Q-41` → `D-141` | Python only | speed is no longer an argument for the C++ layer: 108 / 108 cells pass, the long runs are within run-to-run noise |
| 3 | `D-140`, D.12 | no C++ helper for building the batch | it would save at most ~80 ms of 36 s on C201 (0.2 %), 1–4 ms per run with tabu |

**What Python only costs and gains** (decision 2, with the colleagues, who accepted `D-111`):

- Cost: C++ users lose the column generation layer (`D-02` "usable from C++ and Python" reversed; `D-08`, `D-15`,
  `D-37` "the loop written once, in C++" reversed). If C++ users of column generation matter, that is the reason to
  keep the C++ layer; speed is not.
- Gain: one implementation, in Python; no bindings layer for the loop, fewer builds and less code to maintain;
  users extend pricers and master solvers in Python.
- Work left: the last PR of `python-layer.md` §2 (drop the C++ layer and bindings, rename the pure package to
  `column_generation`); the frozen stack's drafts and PRs reworked or closed.

**Where the remaining gap is, and the cheap speed-ups** (optional, not needed by `D-139`): 1–5 ms per iteration,
in four places (`F-47`): `add_columns` (~0.4 ms per call in checks, `ColumnBatch`, de-duplication), the 8 HiGHS
options set and restored around every master solve, the GraphPricer's wrappers and `_to_pricing_result`, the loop's
copies of duals and records. The first two are the cheap ones, in Python, measured with `pricing_gap_probe.py`.

**Not run, and why it does not matter:** `exact` + `bigm` on R201 and RC201 (about 12 h; R201 6836 s per
configuration, pricing over 99 %, so the layers cannot differ by more than noise there).

**Caveats to state when presenting:** the long tier has 1–2 repeats (2 for the single-customer start); the desktop
was not fully idle in the first night (Chrome at a full core at times; runs over 50 % of a core are flagged `*` in
`summary.md`); all runs on one machine.

**Presentation (Monday 2026-11-09), suggested flow:** the message (same results, same speed within noise on
realistic runs); correctness (1,204 runs, LP, iterations and columns identical); the % against run length chart
with the pass line; where the milliseconds go (stacked bars, `F-47`); same API (the drivers switch layers by module
only, `drivers.layer_module`); the decision (`D-139`, `D-141`, what it costs); caveats. Draft charts and slides:
`benchmark-results/colgen/presentation/python-layer/` (`make_data.py` writes the data, then
`latexmk -pdf slides.tex`; quick and dirty, the stacked bar's legend overlaps).

**To resume on another machine:** results are all in `benchmark-results/colgen/full-*/`, `rerun-*/`,
`full-probes/`; `full_comparison_verdict.py [--floor s] [folders]` re-applies `D-139`; `make_data.py` rebuilds the
slide data. Timings from another machine do not mix with these (desktop) ones: re-run whole cells there if needed
(`full_comparison.sh` and `overnight_reruns.sh` are resumable; a `STOP` file in a folder stops after the current
run).
