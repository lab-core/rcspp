# The C++ VRP example against the C++ example layer, small tier

Speed check for the C++ example's port (2026-11-20, laptop `leo-laptop` on power, **not idle**: VS Code and Chrome
kept 38–42 % of the processors busy). Source checkout `../rcspp-dev` at `b28a8e5d` (the Python port's head, C++ as
at #97's head), Release, MSVC, Gurobi 13.0.1 (`C:\gurobi1301`), threads left to Gurobi.

The old `drivers.py cpp` driver cannot run: it calls `rcspp-ng-cg-comparison` in `../rcspp-main/build-v2`, which no
longer exists; that program also needs `kRouteRelaxation = NgPath` edited in, and prints only a total time. So this
folder holds its own programs, built over the checkout without changing it (`CMakeLists.txt`, `add_subdirectory`):

| program | what |
|---|---|
| `old-vrp` | `examples/cpp/vrp` copied into `old/vrp`, unchanged but for `VRP::solve_timed`: its template `solve<SimpleDominanceAlgorithm>(params)` (what the benchmark mains run), one algorithm, a timer around each step |
| `new-vrp` | the same VRP on the layer (`layer_vrp.hpp`): the same graph (resources, nodes, arcs, arc order) but rows `customer − 1` and none on depot arcs; `GurobiMasterSolver` over one `= 1` row per customer; `GraphPricer` with its default exact stage; single-customer columns, `NoInitialization`; `ColumnGeneration` defaults |
| `replay-vrp` | the old loop, and at each of its iterations, outside its timers, the layer's `GraphPricer` on the same duals (threshold −1e-8 as the old one) and a shadow layer master (never solved) receiving its columns |

Both log at `Warn` (the old benchmarks log one line per iteration at `Info`). Run: `python run.py --instances
C101_25,R101_25,R201_25,RC201_12 --rounds 5 --output small.jsonl` (sides alternated per round), `--replay`,
`--summarize`, `--summarize-replay`.

## Loop against loop

Solve = the loop from the first master solve to the end (graph and master construction left out); median of 5.

| instance | LP (both) | it. old / new | col. old / new | solve s old | solve s new [min–max] | new − old | rule |
|---|---|---|---|---|---|---|---|
| C101_25 | 191.813620 | 22 / 29 | 645 / 903 | 0.0566 | 0.0819 [0.0485–0.1015] | +0.0252 | pass |
| R101_25 | 618.329916 | 11 / 6 | 100 / 92 | 0.0166 | 0.0101 [0.0058–0.0137] | −0.0065 | pass |
| R201_25 | 449.659753 | 22 / 12 | 6,373 / 3,007 | 0.7769 | 0.4508 [0.4295–0.9129] | −0.3261 | pass |
| RC201_12 | 427.235577 | 22 / 17 | 2,077 / 1,061 | 0.1147 | 0.0761 [0.0670–0.0977] | −0.0386 | pass |

- LP equal to 1e-12 relative; iterations and columns the same in every round on each side.
- The new side's 29 / 903 on C101_25 are the Python port's (`F-81` (2), rows without the free row 0).
- Different iterations: other duals (threshold −1e-8 against max(1e-9, Gurobi's 1e-6); the layer sets primal simplex,
  the old master leaves Gurobi's method; a degenerate LP).

Per iteration, ms (median of each part / iterations). Old: master = `optimize` and reading the solution maps; search =
`update_reduced_costs` + `solve`; add = `add_paths` (cost summed over arcs, `GRBColumn`, `addVar`); rest = the dual
map's copy and the loop. New: master = `solve_relaxation`; search = `GraphPricer::price`; add = the rest of the
pricing step (`MasterProblem::add_columns`, with Gurobi's `update()`); rest = the loop.

| instance | side | it. | master | search | add | duals + rest | outside the master |
|---|---|---|---|---|---|---|---|
| C101_25 | old | 22 | 1.015 | 1.461 | 0.079 | 0.023 | 1.564 |
| C101_25 | new | 29 | 0.649 | 1.955 | 0.213 | 0.006 | 2.174 |
| R101_25 | old | 11 | 0.369 | 1.087 | 0.038 | 0.015 | 1.139 |
| R101_25 | new | 6 | 0.274 | 1.148 | 0.070 | 0.009 | 1.227 |
| R201_25 | old | 22 | 2.057 | 32.605 | 0.233 | 0.259 | 33.097 |
| R201_25 | new | 12 | 1.822 | 35.029 | 0.825 | 0.013 | 35.867 |
| RC201_12 | old | 22 | 1.442 | 3.753 | 0.145 | 0.070 | 3.968 |
| RC201_12 | new | 17 | 0.694 | 3.472 | 0.262 | 0.006 | 3.740 |

The search per iteration is not comparable here: each side searches its own duals.

## Pricing on one dual sequence (`replay-vrp`)

The old loop's duals, priced by both; 5 runs. Both find the same columns at every iteration (count and best reduced
cost; total columns equal).

| instance | it. | columns | pricing ms/it old | layer [min–max] | layer / old | add µs/column old | layer |
|---|---|---|---|---|---|---|---|
| C101_25 | 22 | 645 | 1.748 | 1.728 [1.436–3.461] | 0.99 | 2.66 | 4.74 |
| R101_25 | 11 | 100 | 1.093 | 1.297 [0.885–3.787] | 1.19 | 2.23 | 5.74 |
| R201_25 | 22 | 6,373 | 41.175 | 38.399 [23.593–56.873] | 0.93 | 0.95 | 3.37 |
| RC201_12 | 22 | 2,077 | 3.880 | 3.733 [3.561–6.387] | 0.96 | 1.47 | 3.34 |

The old master leaves Gurobi's `update()` to `optimize()` (counted in its master); the layer's master solver calls it
in `add_columns`. With the old example's copy calling `update()` at the end of its `add_columns` too
(`OLD_UPDATE_IN_ADD=1`, `replay-small-update.jsonl`):

| instance | pricing ms/it old | layer [min–max] | layer / old | add µs/column old | layer |
|---|---|---|---|---|---|
| C101_25 | 1.352 | 1.418 [1.254–1.910] | 1.05 | 6.06 | 4.83 |
| R101_25 | 0.993 | 0.866 [0.709–0.948] | 0.87 | 5.37 | 6.61 |
| R201_25 | 33.153 | 33.502 [31.531–35.761] | 1.01 | 1.08 | 3.37 |
| RC201_12 | 3.880 | 3.413 [3.288–4.075] | 0.88 | 2.94 | 3.33 |

- Pricing: equal within the noise (0.87–1.19, the faster side changing between runs).
- Adding: equal once `update()` is counted on both sides, except R201_25 (long routes, 290 columns per iteration):
  the layer 2.3 µs more per column, 0.7 ms per iteration against 33 ms of pricing (2 %). Not traced.
- The loop's own work: the layer's 0.006–0.013 ms per iteration against 0.015–0.26 ms (the old dual map's copy).
- The replay prices the layer second on each iteration (warm caches); not alternated.
