# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highs-heuristic | 0 | Optimal | 191.8136 | 41 | 2793 | 1.70 | 0.75 | 0.12 | 0.63 | 0.00 | 63 |
| layer-cpp-highspy-heuristic | 0 | Optimal | 191.8136 | 41 | 2793 | 1.70 | 0.69 | 0.11 | 0.58 | 0.00 | 66 |
| layer-cpp-gurobipy-heuristic | 0 | Optimal | 191.8136 | 38 | 2548 | 2.23 | 0.97 | 0.20 | 0.77 | 0.01 | 65 |
| layer-python-highs-heuristic | 0 | Optimal | 191.8136 | 41 | 2793 | 1.85 | 0.79 | 0.12 | 0.66 | 0.01 | 64 |
| layer-python-highspy-heuristic | 0 | Optimal | 191.8136 | 41 | 2793 | 1.71 | 0.78 | 0.12 | 0.66 | 0.01 | 66 |
| layer-python-gurobipy-heuristic | 0 | Optimal | 191.8136 | 38 | 2548 | 1.96 | 0.85 | 0.17 | 0.67 | 0.01 | 65 |
| layer-purepython-highspy-heuristic | 0 | Optimal | 191.8136 | 30 | 2658 | 1.78* | 0.64* | 0.07 | 0.56 | 0.01 | 67 |
| layer-purepython-gurobipy-heuristic | 0 | Optimal | 191.8136 | 32 | 2737 | 1.92 | 0.87 | 0.15 | 0.72 | 0.01 | 67 |

## By configuration (mean wall s)

| config | C101_25 |
|---|---|
| layer-cpp-highs-heuristic | 1.70 |
| layer-cpp-highspy-heuristic | 1.70 |
| layer-cpp-gurobipy-heuristic | 2.23 |
| layer-python-highs-heuristic | 1.85 |
| layer-python-highspy-heuristic | 1.71 |
| layer-python-gurobipy-heuristic | 1.96 |
| layer-purepython-highspy-heuristic | 1.78 |
| layer-purepython-gurobipy-heuristic | 1.92 |

## By configuration (mean solve s)

| config | C101_25 |
|---|---|
| layer-cpp-highs-heuristic | 0.75 |
| layer-cpp-highspy-heuristic | 0.69 |
| layer-cpp-gurobipy-heuristic | 0.97 |
| layer-python-highs-heuristic | 0.79 |
| layer-python-highspy-heuristic | 0.78 |
| layer-python-gurobipy-heuristic | 0.85 |
| layer-purepython-highspy-heuristic | 0.64 |
| layer-purepython-gurobipy-heuristic | 0.87 |

## LP mismatches

none
