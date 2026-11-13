# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 191.8136 | 34 | 783 | 2.37 | 0.73 | 0.61 | 0.04 | 0.04 | 75 |
| layer-cpp-highs-exact | 0 | Optimal | 191.8136 | 24 | 761 | 1.31 | 0.09 | 0.03 | 0.06 | 0.00 | 61 |
| layer-cpp-highs-heuristic | 0 | Optimal | 191.8136 | 5 | 140344 | 14.81* | 13.65* | 12.61 | 1.03 | 0.00 | 212 |
| layer-cpp-highspy-exact | 0 | Optimal | 191.8136 | 24 | 761 | 1.28 | 0.06 | 0.02 | 0.04 | 0.00 | 64 |
| layer-cpp-highspy-heuristic | 0 | Optimal | 191.8136 | 5 | 140344 | 15.56* | 14.17* | 13.20 | 0.97 | 0.00 | 216 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 191.8136 | 30 | 670 | 1.70 | 0.18 | 0.12 | 0.06 | 0.00 | 66 |
| layer-cpp-gurobipy-heuristic | 0 | Optimal | 191.8136 | 4 | 139663 | 5.70* | 4.25* | 0.34 | 3.83 | 0.07 | 281 |
| layer-python-highs-exact | 0 | Optimal | 191.8136 | 24 | 761 | 1.29 | 0.08 | 0.02 | 0.05 | 0.01 | 61 |
| layer-python-highs-heuristic | 0 | Optimal | 191.8136 | 5 | 140344 | 14.81* | 13.63* | 12.24 | 1.38 | 0.01 | 277 |
| layer-python-highspy-exact | 0 | Optimal | 191.8136 | 24 | 761 | 1.24* | 0.08* | 0.02 | 0.05 | 0.01 | 56 |
| layer-python-highspy-heuristic | 0 | Optimal | 191.8136 | 5 | 140344 | 14.68* | 13.40* | 12.06 | 1.34 | 0.01 | 280 |
| layer-python-gurobipy-exact | 0 | Optimal | 191.8136 | 30 | 670 | 1.31 | 0.21 | 0.12 | 0.07 | 0.01 | 63 |
| layer-python-gurobipy-heuristic | 0 | Optimal | 191.8136 | 4 | 139663 | 6.14* | 4.60* | 0.30 | 4.24 | 0.07 | 361 |
| layer-purepython-highspy-exact | 0 | Optimal | 191.8136 | 16 | 599 | 1.55 | 0.08 | 0.02 | 0.05 | 0.01 | 54 |
| layer-purepython-highspy-heuristic | 0 | Optimal | 191.8136 | 3 | 140329 | 16.89* | 15.62* | 13.53 | 2.08 | 0.02 | 292 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 191.8136 | 23 | 691 | 1.39 | 0.16 | 0.10 | 0.06 | 0.01 | 63 |
| layer-purepython-gurobipy-heuristic | 0 | Optimal | 191.8136 | 10 | 139714 | 5.80* | 4.29* | 0.56 | 3.68 | 0.06 | 382 |
| layer-cpp-highspy-tabu | 0 | Optimal | 191.8136 | 59 | 415 | 1.97* | 0.93* | 0.04 | 0.89 | 0.00 | 63 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 191.8136 | 39 | 389 | 2.13 | 1.04 | 0.19 | 0.85 | 0.01 | 62 |
| layer-purepython-highspy-tabu | 0 | Optimal | 191.8136 | 53 | 434 | 2.22* | 1.03* | 0.04 | 0.98 | 0.01 | 64 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 191.8136 | 68 | 449 | 2.56 | 1.44 | 0.29 | 1.14 | 0.01 | 64 |

## By configuration (mean wall s)

| config | C101_25 |
|---|---|
| example-mip | 2.37 |
| layer-cpp-highs-exact | 1.31 |
| layer-cpp-highs-heuristic | 14.81 |
| layer-cpp-highspy-exact | 1.28 |
| layer-cpp-highspy-heuristic | 15.56 |
| layer-cpp-gurobipy-exact | 1.70 |
| layer-cpp-gurobipy-heuristic | 5.70 |
| layer-python-highs-exact | 1.29 |
| layer-python-highs-heuristic | 14.81 |
| layer-python-highspy-exact | 1.24 |
| layer-python-highspy-heuristic | 14.68 |
| layer-python-gurobipy-exact | 1.31 |
| layer-python-gurobipy-heuristic | 6.14 |
| layer-purepython-highspy-exact | 1.55 |
| layer-purepython-highspy-heuristic | 16.89 |
| layer-purepython-highspy-tabu | 2.22 |
| layer-cpp-highspy-tabu | 1.97 |
| layer-purepython-gurobipy-exact | 1.39 |
| layer-purepython-gurobipy-heuristic | 5.80 |
| layer-purepython-gurobipy-tabu | 2.56 |
| layer-cpp-gurobipy-tabu | 2.13 |

## By configuration (mean solve s)

| config | C101_25 |
|---|---|
| example-mip | 0.73 |
| layer-cpp-highs-exact | 0.09 |
| layer-cpp-highs-heuristic | 13.65 |
| layer-cpp-highspy-exact | 0.06 |
| layer-cpp-highspy-heuristic | 14.17 |
| layer-cpp-gurobipy-exact | 0.18 |
| layer-cpp-gurobipy-heuristic | 4.25 |
| layer-python-highs-exact | 0.08 |
| layer-python-highs-heuristic | 13.63 |
| layer-python-highspy-exact | 0.08 |
| layer-python-highspy-heuristic | 13.40 |
| layer-python-gurobipy-exact | 0.21 |
| layer-python-gurobipy-heuristic | 4.60 |
| layer-purepython-highspy-exact | 0.08 |
| layer-purepython-highspy-heuristic | 15.62 |
| layer-purepython-highspy-tabu | 1.03 |
| layer-cpp-highspy-tabu | 0.93 |
| layer-purepython-gurobipy-exact | 0.16 |
| layer-purepython-gurobipy-heuristic | 4.29 |
| layer-purepython-gurobipy-tabu | 1.44 |
| layer-cpp-gurobipy-tabu | 1.04 |

## LP mismatches

none
