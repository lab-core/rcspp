# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 191.8136 | 18 | 666 | 1.29 | 0.06 | 0.02 | 0.04 | 0.00 | 64 |
| layer-purepython-highspy-exact | 0 | Optimal | 191.8136 | 18 | 666 | 1.28 | 0.07 | 0.02 | 0.04 | 0.00 | 64 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 191.8136 | 29 | 723 | 1.49 | 0.20 | 0.13 | 0.07 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 191.8136 | 29 | 723 | 1.63 | 0.21 | 0.13 | 0.08 | 0.01 | 64 |

## R201_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 449.6598 | 12 | 3024 | 1.71 | 0.52 | 0.06 | 0.46 | 0.00 | 70 |
| layer-purepython-highspy-exact | 0 | Optimal | 449.6598 | 12 | 3024 | 1.71 | 0.56 | 0.06 | 0.49 | 0.00 | 71 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 449.6598 | 11 | 3247 | 1.96 | 0.61 | 0.07 | 0.53 | 0.00 | 70 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 449.6598 | 11 | 3247 | 1.87* | 0.62* | 0.07 | 0.55 | 0.00 | 71 |

## RC201_12

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 427.2356 | 16 | 1050 | 1.30 | 0.10 | 0.03 | 0.07 | 0.00 | 64 |
| layer-purepython-highspy-exact | 0 | Optimal | 427.2356 | 16 | 1050 | 1.28 | 0.12 | 0.03 | 0.08 | 0.00 | 66 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 427.2356 | 16 | 1046 | 1.41 | 0.17 | 0.07 | 0.09 | 0.00 | 64 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 427.2356 | 16 | 1046 | 1.38* | 0.17* | 0.07 | 0.09 | 0.00 | 65 |

## By configuration (mean wall s)

| config | C101_25 | R201_25 | RC201_12 |
|---|---|---|---|
| layer-cpp-highspy-exact | 1.29 | 1.71 | 1.30 |
| layer-cpp-gurobipy-exact | 1.49 | 1.96 | 1.41 |
| layer-purepython-highspy-exact | 1.28 | 1.71 | 1.28 |
| layer-purepython-gurobipy-exact | 1.63 | 1.87 | 1.38 |

## By configuration (mean solve s)

| config | C101_25 | R201_25 | RC201_12 |
|---|---|---|---|
| layer-cpp-highspy-exact | 0.06 | 0.52 | 0.10 |
| layer-cpp-gurobipy-exact | 0.20 | 0.61 | 0.17 |
| layer-purepython-highspy-exact | 0.07 | 0.56 | 0.12 |
| layer-purepython-gurobipy-exact | 0.21 | 0.62 | 0.17 |

## LP mismatches

none
