# The column generation alone: the Python example against the pure layer

2026-11-21, laptop on power, not idle (a chat running): times indicative, the replay's ratios robust (both sides in
one process, at the same moments). Source `../rcspp-benchmark` (`a07d65a4`: `python/src`, `examples/python` and the
C++ core equal to #97's head `ee8b0272`), gurobipy and python-mip 2.0.0 over Gurobi 13.0.1, single-customer start.

- `loop-*.jsonl`: `drivers.py` with timers around `ResourceGraph.solve` (search), `update_reduced_costs`, and the LP
  solver's own adds (python-mip's `_add_column`, the master solvers' `add_columns`); small 5 rounds, mid 3.
- `replay-*.jsonl`: `replay.py`, the example's loop and at each iteration the layer's pricing step on the same duals
  (`GraphPricer.price`, `MasterProblem.add_columns` into a never-solved gurobipy master); 3 rounds.
- `search_probe.py`: the same graph and duals, `solve()` with each of the layer's arguments.
- Own = the column generation's own work: the pricing step minus the search, the reduced costs and the solver's adds.

Run: `python run.py loop|replay --instances ... --rounds N --output F`, `python run.py summarize F...`.

## Replay: the same duals, the same columns

Columns equal at every iteration (C101_50 and C201: 1-2 iterations with tied paths, same count and best cost).

| instance | it. | search old / layer | reduced costs | solver adds | own, ms/it. | own, µs/col. | step, layer − old, s/run | % of the example's LP |
|---|---|---|---|---|---|---|---|---|
| C101_25 | 24 | 0.56 / 0.92 | 0.085 / 0.013 | 0.17 / 0.27 | 0.63 / 0.25 | 20.9 / 8.3 | +0.001 | +0.5 |
| R101_25 | 7 | 0.41 / 0.65 | 0.233 / 0.012 | 0.08 / 0.14 | 0.25 / 0.17 | 18.2 / 12.2 | −0.000 | −0.1 |
| R201_25 | 14 | 17.3 / 17.6 | 0.147 / 0.023 | 1.17 / 1.61 | 7.15 / 1.16 | 34.6 / 5.6 | −0.076 | −16.4 |
| RC201_12 | 16 | 2.08 / 2.35 | 0.086 / 0.014 | 0.37 / 0.56 | 1.86 / 0.44 | 27.4 / 6.5 | −0.017 | −13.1 |
| C101_50 | 42 | 1.69 / 2.92 | 0.305 / 0.039 | 0.29 / 0.37 | 0.91 / 0.34 | 22.5 / 8.3 | +0.024 | +9.1 |
| R201_50 | 35 | 121.2 / 124.3 | 0.411 / 0.072 | 2.72 / 3.14 | 15.1 / 2.05 | 41.2 / 5.6 | −0.341 | −5.9 |
| RC101_50 | 28 | 1.58 / 2.43 | 0.409 / 0.024 | 0.16 / 0.22 | 0.51 / 0.23 | 22.1 / 9.7 | +0.006 | +4.6 |
| C201 | 162-196 | 26.8 / 33.9 | 0.572 / 0.230 | 2.35 / 3.87 | 9.15 / 1.35 | 46.8 / 6.8 | +0.067 | +0.2 |

ms per iteration unless said; C201's example takes another path each round (values per round, then the median).

- **The layer's own work is 3-7 times cheaper per column** (5.6-12 µs against 18-47 µs): the example builds `Path`
  objects, sums arc costs through `get_arc`, and builds python-mip columns from dictionaries; its dictionary duals make
  its reduced costs 4-20 times slower too.
- **Two parts are slower in the layer:** the search (below) and gurobipy's column adds (+20-80 % an iteration, the
  model's `update()` included). On C201 they cost +1.34 s and +0.28 s, the own work saves 1.55 s: the step nets +0.2 %.

## Why the layer's search is slower on the same duals

`search_probe.py`, 12 dual vectors of the example's loop, same improving paths in every variant:

| instance | `solve()` ms | bound −1e-8 | params | direction | the layer's call | the layer's call, preprocess off |
|---|---|---|---|---|---|---|
| C101_25 | 0.57 | ×1.80 | ×1.06 | ×1.05 | ×1.88 | ×0.92 |
| C101_50 | 1.85 | ×2.04 | ×1.00 | ×0.99 | ×2.00 | ×1.03 |
| RC101_50 | 1.29 | ×1.80 | ×1.04 | ×1.02 | ×1.81 | ×1.00 |
| R201_25 | 21.9 | ×1.11 | ×1.08 | ×1.01 | ×1.02 | ×1.00 |
| C201 | 181.8 | ×1.04 | ×1.00 | ×0.99 | ×1.04 | ×0.99 |
| R201_50 | 392.3 | ×1.01 | ×0.99 | ×1.00 | ×1.00 | ×1.00 |

The threshold given as `upper_bound` turns on the engine's `ShortestPathPreprocessor` (disabled for an infinite bound,
`cpp/rcspp/preprocessor/shortest_path_preprocessor.hpp:51`): a fixed 0.5-1.9 ms per search that prunes nothing useful
here. `GraphPricer(preprocess=False)` gives the example's speed. The C++ layer's `GraphPricer` has the same default.
