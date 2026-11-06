# Pure-Python column generation layer (`T-1.15`): the full comparison

Step H of `python-layer.md` §8 (2026-11-06, Opus): the step G runs read (`python-layer-runs.md`), the gap probed,
the full matrix and `Q-41`'s rule planned. Proposed: `D-138` (matrix), `D-139` (rule), `D-140` (D.12). Findings:
`F-47`, `F-48`. No code under `python/` or `tests/` changed; no layer commit touched; nothing pushed.

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
