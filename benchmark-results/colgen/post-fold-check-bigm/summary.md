# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 191.8136 | 16 | 599 | 1.07 | 0.04 | 0.01 | 0.02 | 0.00 | 63 |
| layer-python-highspy-exact | 0 | Optimal | 191.8136 | 16 | 599 | 1.07 | 0.05 | 0.01 | 0.03 | 0.01 | 64 |
| layer-purepython-highspy-exact | 0 | Optimal | 191.8136 | 16 | 599 | 1.07 | 0.06 | 0.02 | 0.04 | 0.00 | 55 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 191.8136 | 23 | 691 | 1.07 | 0.14 | 0.09 | 0.05 | 0.00 | 62 |
| layer-python-gurobipy-exact | 0 | Optimal | 191.8136 | 23 | 691 | 1.37 | 0.16 | 0.09 | 0.05 | 0.01 | 62 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 191.8136 | 23 | 691 | 1.28 | 0.16 | 0.10 | 0.06 | 0.01 | 64 |
| layer-cpp-highspy-tabu | 0 | Optimal | 191.8136 | 53 | 434 | 2.14 | 0.92 | 0.03 | 0.89 | 0.00 | 63 |
| layer-purepython-highspy-tabu | 0 | Optimal | 191.8136 | 53 | 434 | 1.98 | 0.99 | 0.04 | 0.94 | 0.01 | 64 |

## By configuration (mean wall s)

| config | C101_25 |
|---|---|
| layer-cpp-highspy-exact | 1.07 |
| layer-cpp-gurobipy-exact | 1.07 |
| layer-python-highspy-exact | 1.07 |
| layer-python-gurobipy-exact | 1.37 |
| layer-purepython-highspy-exact | 1.07 |
| layer-purepython-highspy-tabu | 1.98 |
| layer-cpp-highspy-tabu | 2.14 |
| layer-purepython-gurobipy-exact | 1.28 |

## By configuration (mean solve s)

| config | C101_25 |
|---|---|
| layer-cpp-highspy-exact | 0.04 |
| layer-cpp-gurobipy-exact | 0.14 |
| layer-python-highspy-exact | 0.05 |
| layer-python-gurobipy-exact | 0.16 |
| layer-purepython-highspy-exact | 0.06 |
| layer-purepython-highspy-tabu | 0.99 |
| layer-cpp-highspy-tabu | 0.92 |
| layer-purepython-gurobipy-exact | 0.16 |

## LP mismatches

none
