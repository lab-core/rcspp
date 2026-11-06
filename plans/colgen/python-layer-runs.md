# Pure-Python column generation layer (`T-1.15`): runs

Records of the runs of `python-layer.md` §6 (`D-123`). Steps A–F: `python-layer-records.md`.

Step G record (2026-11-06, Sonnet; the quick runs and the probes of four open items). No code under `python/` or
`tests/` changed; no layer commit touched; nothing pushed. Scripts: `benchmark-results/colgen/`.

## Runs

- `drivers.py layer --loop purepython --master highspy|gurobipy` runs `rcspp.python_column_generation` (its own loop,
  master problem, graph pricer and master solvers); `--loop cpp` is the C++-backed layer (`ColumnGeneration.run()`),
  the same instance, graph, start and pricing. `run_matrix.py` configurations `layer-purepython-{highspy,gurobipy}-
  {exact,heuristic}`. No `highs` master for the pure layer (`D-124`).
- Settings: exact pricing (one `simple` stage), `COLGEN_INIT=single` (the example's single-customer columns),
  `PYTHONPATH=python/src` (the launcher's `build-python-highs/python` has no pure package), the `.venv`'s Python.
- Command: `python benchmark-results/colgen/run_matrix.py --output <folder> --instances C101_25,R201_25,RC201_12
  --configs layer-cpp-highspy-exact,layer-purepython-highspy-exact,layer-cpp-gurobipy-exact,
  layer-purepython-gurobipy-exact --repeats 1`.
- Folders: `pure-python-quick/` (the asked runs, 1 repeat), `pure-python-quick-r5/` (the same, 5 repeats, added: the
  runs last 0.05–0.6 s, so one repeat cannot tell 20 ms from noise), `pure-python-quick-larger/` (C101_50 and
  C201, HiGHS only, 1 repeat), `pure-python-quick/probes/` (the probes below).

### LP values: equal

No mismatch in any of the 12 runs, nor in the 60 of the 5-repeat folder. The two layers give the **same LP value
to the last digit, the same iteration count and the same column count** with each master solver (the duals are the
same, so the pricing took the same paths):

| instance | master | LP value | iterations | columns |
|---|---|---|---|---|
| C101_25 | highspy | 191.8136197786562 | 18 | 666 |
| C101_25 | gurobipy | 191.8136197786562 | 29 | 723 |
| R201_25 | highspy | 449.65975310219824 | 12 | 3024 |
| R201_25 | gurobipy | 449.65975310219807 | 11 | 3247 |
| RC201_12 | highspy | 427.235576835984 | 16 | 1050 |
| RC201_12 | gurobipy | 427.23557683598403 | 16 | 1046 |
| C101_50 | highspy | 363.2468 (`363.246800411591`) | 54 | 1749 |
| C201 | highspy | 591.5565566715024 | 190 | 37974 |

They match the baseline values of `benchmark-results/colgen/HANDOFF.md` (C101_25 191.8136, R201_25 449.6598,
RC201_12 427.2356, C101_50 363.2468, C201 591.5566).

### Times (solve seconds = master + pricing + rest, without start-up)

1 repeat (`pure-python-quick/`), the asked table; `*` = other processes above 50 % of a core:

| instance | master | C++-backed | pure Python | difference |
|---|---|---|---|---|
| C101_25 | highspy | 0.06 | 0.07 | +0.01 |
| C101_25 | gurobipy | 0.20 | 0.21 | +0.01 |
| R201_25 | highspy | 0.52 | 0.56 | +0.04 |
| R201_25 | gurobipy | 0.61 | 0.62* | +0.01 |
| RC201_12 | highspy | 0.10 | 0.12 | +0.02 |
| RC201_12 | gurobipy | 0.17 | 0.17* | 0.00 |

5 repeats (`pure-python-quick-r5/`), median [min–max] solve seconds; "rest" is the loop's own Python time, the
median of the pure layer (the C++-backed one is 0.000–0.003):

| instance | master | C++-backed | pure Python | difference | pure "rest" |
|---|---|---|---|---|---|
| C101_25 | highspy | 0.045 [0.044–0.048] | 0.066 [0.060–0.093] | +0.021 | 0.0036 |
| C101_25 | gurobipy | 0.184 [0.177–0.185] | 0.199 [0.182–0.241] | +0.015 | 0.0055 |
| R201_25 | highspy | 0.503 [0.481–0.690] | 0.535 [0.496–0.585] | +0.032 | 0.0036 |
| R201_25 | gurobipy | 0.575 [0.542–0.611] | 0.555 [0.550–0.584] | −0.020 | 0.0047 |
| RC201_12 | highspy | 0.089 [0.084–0.097] | 0.122 [0.113–0.147] | +0.033 | 0.0035 |
| RC201_12 | gurobipy | 0.152 [0.147–0.185] | 0.160 [0.150–0.165] | +0.008 | 0.0035 |

Larger, 1 repeat, HiGHS (`pure-python-quick-larger/`): C101_50 0.45 against 0.44 s; C201 42.4 against 44.4 s
(+4.7 %; both runs noisy, other processes at 133 % and 170 % of a core, the master part alone 29.7 against 31.2 s).

Reading: the pure layer is up to about 30 ms slower per run on these small instances (up to 1.5 times on the
shortest, C101_25 and RC201_12 over HiGHS), about 3–7 % where a run lasts over 0.4 s, within the noise on C201. The
gap by slice (5-repeat medians): pricing +14, +27, +24 ms on the three HiGHS runs, master +3 to +4, the loop's own
"rest" +3.5. `add_columns` runs inside the pricing slice (D.12 below: +5.6 to +16.5 ms); the other 8–11 ms of it are
the pure `GraphPricer`'s own Python (duals, reduced costs, the `Solution` list), not probed. Times are not far off;
no LP difference, so nothing to stop on.

## Probes

`python_layer_probes.py --instance <name>`: the pure loop over highspy with timers around the calls (a `Highs`
subclass, patched methods; the layer untouched), then the solution lists the pricer returned are replayed through
the pure and the C++-backed `MasterProblem.add_columns` on fresh masters (best of 5 repeats per call). In-run
timers are single measurements (upper bounds); micro-benchmarks are the best of several. Sizes: R201_25 3049
columns, 12 iterations; C201 38074 columns, 190 iterations (the scale check).

### B.15: copies of the HiGHS solution

| | C101_25 | R201_25 | RC201_12 | C101_50 | C201 |
|---|---|---|---|---|---|
| iterations (= `duals()` calls) | 18 | 12 | 16 | 54 | 190 |
| `getSolution()` µs per call (final size) | 1.9 | 2.9 | 2.8 | 2.4 | 23 |
| `duals()` ms per run (in run, copies included) | 0.68 | 0.80 | 0.90 | 2.5 | 22 |
| the same, in % of the run's LP time | 3.4 | 1.3 | 3.1 | 2.0 | 0.08 |
| `getLp()` calls in a run (µs per call at final size) | 2 (4) | 2 (13) | 2 (7) | 2 (7) | 2 (1860) |
| `row_bounds()` calls in a run | 0 | 0 | 0 | 0 | 0 |

- `getInfo()` (for `objective()`) copies too: 17 µs per iteration. `solution.row_dual` read: 0.7 µs;
  `solution.col_value` (only `primal`, never called in these runs: no artificial column used, no hook): 51 µs at 3049
  columns.
- `getLp()` runs twice per run (the constructor and `largest_user_cost`); `row_bounds` is not called by the loop.
- **Proposal: none.** The copies cost at most 3.4 % of the LP time (0.08 % on C201) and
  does not bias the comparison; per-call figures are far below the LP solve of one iteration (1–155 ms). Revisit
  only for a master of millions of columns, or a loop that calls `row_bounds` or `primal` each iteration.

### B.16: the batches' integer widths

| per run, µs | C101_25 | R201_25 | RC201_12 | C101_50 | C201 |
|---|---|---|---|---|---|
| batches / columns per batch | 17 / 39 | 11 / 275 | 15 / 70 | 53 / 33 | 189 / 201 |
| casts as now (`uint32` rows, `starts[:-1]` to `int32`, `rows[uint32 index]`) | 154 | 191 | 157 | 526 | 4370 |
| the same with `int32` throughout | 151 | 187 | 191 | 447 | 4316 |
| `ColumnBatch` built from `uint32` arrays / from `int32` arrays | 380 / 425 | 274 / 329 | 353 / 397 | 1209 / 1355 | 5293 / 6414 |

- The widths cost 0.15–0.5 ms per run (4.4 ms on C201, 0.01 % of its run); `int32` throughout saves about nothing
  (the differences are noise). A `ColumnBatch` that kept `int32` would not be faster: its constructor stores `uint32`,
  so an `int32` input is converted anyway (last row).
- **Proposal: keep `uint32` / `uint64`.** No speed reason to change. Only a taste reason would remain (casts in
  `_send` and `HighspyMasterSolver.add_columns` disappear if the batch used `int32`, what highspy takes; `ids`
  would stay `uint64`); `ColumnBatch` would then differ from the C++ widths for nothing measurable.

### D.12: the cost of `MasterProblem.add_columns`

Replay of the lists the pricer returned (all non-empty calls of the run), best of 5, HiGHS master:

| | C101_25 | R201_25 | RC201_12 | C101_50 | C201 |
|---|---|---|---|---|---|
| calls / columns | 17 / 666 | 11 / 3024 | 15 / 1050 | 53 / 1749 | 189 / 37974 |
| pure Python, ms | 6.25 | 19.73 | 8.22 | 17.66 | 272.6 |
| C++-backed, ms | 0.61 | 3.24 | 0.98 | 1.99 | 61.0 |
| extra, ms (% of the run) | 5.6 (8.8) | 16.5 (3.0) | 7.2 (5.7) | 15.7 (3.6) | 212 (0.5) |
| `Column.to_arrays` calls, ms (µs per column) | 2.19 (3.3) | 11.27 (3.7) | 3.63 (3.5) | 6.34 (3.6) | 175 (4.6) |
| HiGHS `addCols` (the master solver's call, in the run), ms | 2.2 | 3.2 | 2.9 | 8.2 | 84 |
| `add_columns` in the run, ms (calls) | 8.2 (18) | 22.0 (12) | 13.1 (16) | 27.4 (54) | 391 (190) |

- `to_arrays` (one pybind call per column) is 35–64 % of the pure layer's time (the share grows with the number of
  columns per call); the HiGHS call is 14–30 % of `add_columns` in the run; the rest (concatenate, order check,
  `ColumnBatch`, `MasterColumn` records) is a few ms. The C++-backed one does all of it in
  C++ for 0.6–3.2 ms (C201: 61 ms), which includes its own HiGHS call.
- The extra cost is 3–9 % of a run on the 25–50 customer instances, 0.5 % on C201: about half of the gap between
  the layers' times above (the rest is the `GraphPricer`'s Python and the loop, not probed).
- **Proposal: leave it.** Exact pricing hides it. It would show with a cheap pricing stage that returns many columns
  (a capped greedy or tabu stage: `T-1.14`'s setup), which no run of this step covers. If it does, the item is the
  per-column `to_arrays` call: one C++ helper turning a list of `Solution` into the CSC arrays (`D-112`) would remove
  up to the 35–64 % (not measured: the helper does not exist). Decide at step H, from a run with a heuristic stage.

### `ColumnBatch`: copy its inputs?

Today (`types.py` `_array`): `np.asarray(values, dtype)` then a read-only **view**: a caller's array of the right
dtype is shared, so changing it afterwards changes the batch (measured: `True`); the batch's own arrays are not
writable. The layer's own path builds fresh arrays each call, so only a user building a `ColumnBatch` is concerned.

| per run, µs | C101_25 | R201_25 | RC201_12 | C101_50 | C201 |
|---|---|---|---|---|---|
| batches built, sharing (as now) | 345 | 236 | 312 | 1090 | 4718 |
| batches built, copying every array | 453 | 323 | 410 | 1495 | 7680 |
| the copies alone | 71 | 64 | 69 | 233 | 1855 |

- A copy adds 64–233 µs per run on the matrix, 1.9 ms on C201 (0.004 % of its run).
- **Proposal: copy** (`np.array(values, dtype=…)` in `_array`, then the read-only view; no `ascontiguousarray` call
  needed): the batch becomes a record that nobody can change from outside, for a cost nobody can measure. The layer's
  own batches pay it for nothing (the same 64–233 µs). The other choice is to document that the batch shares memory
  with its arguments.

## Left for the user and step H

Decisions asked: B.15 (proposal: none), B.16 (keep the widths), D.12 (leave it; look again with a heuristic stage),
and the copy in `ColumnBatch` (proposal: copy). Not run, outside this step: heuristic pricing, the big-M start,
the full matrix, repeats on an idle machine, the Gurobi probes (the copies of B.15 are HiGHS's).

## Fold of the `ColumnBatch` copy (2026-11-06, `D-137`)

`ColumnBatch` and `RowBatch` copy their input arrays (`_array` in `types.py`: `np.array`, then read-only; no view).
`test_the_arrays_of_a_batch_cannot_be_written` was extended first and fails without the change. Folded into commit 1
on both branches (`fixup!` + autosquash; the rest of the tree is unchanged); the pure-Python tests pass (262). The
hashes of commits 1-6 changed again: `feature/column-generation` `4881e169`, `0e620a91`, `ad46b14d`, `2ed8bbd5`,
`4c5e1269`, `6c899842`; `feature/column-generation-pure-python` `6f9e8d6a`, `3b723ec6`, `354af3c6`, `d31492c1`,
`fab75719`, `510e9b65` (the records of steps A-F keep the hashes of their time). Backups:
`backup/before-batch-copy`, `backup/pure-python-before-batch-copy`. Not pushed: both branches need a force push.
