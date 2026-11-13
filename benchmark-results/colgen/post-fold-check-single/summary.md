# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 191.8136 | 18 | 666 | 1.31 | 0.06 | 0.02 | 0.04 | 0.00 | 63 |
| layer-python-highspy-exact | 0 | Optimal | 191.8136 | 18 | 666 | 1.08 | 0.05 | 0.02 | 0.03 | 0.01 | 63 |
| layer-purepython-highspy-exact | 0 | Optimal | 191.8136 | 18 | 666 | 1.07 | 0.06 | 0.02 | 0.04 | 0.00 | 64 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 191.8136 | 29 | 723 | 1.29 | 0.19 | 0.12 | 0.07 | 0.00 | 63 |
| layer-python-gurobipy-exact | 0 | Optimal | 191.8136 | 29 | 723 | 1.25 | 0.20 | 0.12 | 0.07 | 0.01 | 63 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 191.8136 | 29 | 723 | 1.29 | 0.21 | 0.13 | 0.08 | 0.01 | 64 |
| layer-cpp-highspy-tabu | 0 | Optimal | 191.8136 | 66 | 412 | 2.14 | 1.01 | 0.04 | 0.98 | 0.00 | 63 |
| layer-purepython-highspy-tabu | 0 | Optimal | 191.8136 | 66 | 412 | 2.25 | 1.12 | 0.05 | 1.06 | 0.01 | 64 |

## By configuration (mean wall s)

| config | C101_25 |
|---|---|
| layer-cpp-highspy-exact | 1.31 |
| layer-cpp-gurobipy-exact | 1.29 |
| layer-python-highspy-exact | 1.08 |
| layer-python-gurobipy-exact | 1.25 |
| layer-purepython-highspy-exact | 1.07 |
| layer-purepython-highspy-tabu | 2.25 |
| layer-cpp-highspy-tabu | 2.14 |
| layer-purepython-gurobipy-exact | 1.29 |

## By configuration (mean solve s)

| config | C101_25 |
|---|---|
| layer-cpp-highspy-exact | 0.06 |
| layer-cpp-gurobipy-exact | 0.19 |
| layer-python-highspy-exact | 0.05 |
| layer-python-gurobipy-exact | 0.20 |
| layer-purepython-highspy-exact | 0.06 |
| layer-purepython-highspy-tabu | 1.12 |
| layer-cpp-highspy-tabu | 1.01 |
| layer-purepython-gurobipy-exact | 0.21 |

## LP mismatches

none
