# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 191.8136 | 35 | 777 | 1.51 | 0.32 | 0.22 | 0.03 | 0.04 | 72 |
| layer-cpp-highs-exact | 0 | Optimal | 191.8136 | 24 | 761 | 1.09 | 0.06 | 0.02 | 0.04 | 0.00 | 54 |
| layer-cpp-highs-heuristic | 0 | Optimal | 191.8136 | 5 | 140344 | 14.75 | 13.66 | 12.85 | 0.81 | 0.00 | 214 |
| layer-cpp-highspy-exact | 0 | Optimal | 191.8136 | 24 | 761 | 1.83 | 0.10 | 0.03 | 0.07 | 0.00 | 64 |
| layer-cpp-highspy-heuristic | 0 | Optimal | 191.8136 | 5 | 140344 | 20.70 | 18.90 | 17.53 | 1.37 | 0.00 | 217 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 191.8136 | 30 | 670 | 1.30 | 0.17 | 0.11 | 0.06 | 0.00 | 63 |
| layer-cpp-gurobipy-heuristic | 0 | Optimal | 191.8136 | 4 | 139663 | 4.96 | 3.82 | 0.31 | 3.45 | 0.06 | 279 |
| layer-python-highs-exact | 0 | Optimal | 191.8136 | 24 | 761 | 1.08 | 0.07 | 0.02 | 0.04 | 0.01 | 62 |
| layer-python-highs-heuristic | 0 | Optimal | 191.8136 | 5 | 140344 | 13.89 | 12.81 | 11.59 | 1.21 | 0.01 | 278 |
| layer-python-highspy-exact | 0 | Optimal | 191.8136 | 24 | 761 | 1.08 | 0.07 | 0.02 | 0.04 | 0.01 | 65 |
| layer-python-highspy-heuristic | 0 | Optimal | 191.8136 | 5 | 140344 | 14.48 | 13.43 | 12.12 | 1.30 | 0.01 | 281 |
| layer-python-gurobipy-exact | 0 | Optimal | 191.8136 | 30 | 670 | 1.38 | 0.18 | 0.11 | 0.06 | 0.01 | 64 |
| layer-python-gurobipy-heuristic | 0 | Optimal | 191.8136 | 4 | 139663 | 5.98 | 4.92 | 0.52 | 4.34 | 0.06 | 361 |
| example-cpp | 0 | Optimal | 191.8136 | 22 | - | 0.43 | 0.13 | - | - | - | 33 |

## R101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 618.3299 | 7 | 96 | 1.32 | 0.12 | 0.09 | 0.01 | 0.01 | 67 |
| layer-cpp-highs-exact | 0 | Optimal | 618.3299 | 7 | 137 | 1.08 | 0.01 | 0.00 | 0.01 | 0.00 | 60 |
| layer-cpp-highs-heuristic | 0 | Optimal | 618.3299 | 6 | 663 | 1.10 | 0.02 | 0.01 | 0.01 | 0.00 | 61 |
| layer-cpp-highspy-exact | 0 | Optimal | 618.3299 | 7 | 137 | 1.08 | 0.01 | 0.00 | 0.01 | 0.00 | 64 |
| layer-cpp-highspy-heuristic | 0 | Optimal | 618.3299 | 6 | 663 | 1.28 | 0.02 | 0.01 | 0.01 | 0.00 | 49 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 618.3299 | 8 | 127 | 1.08 | 0.04 | 0.03 | 0.01 | 0.00 | 63 |
| layer-cpp-gurobipy-heuristic | 0 | Optimal | 618.3299 | 6 | 697 | 1.08 | 0.05 | 0.02 | 0.02 | 0.00 | 63 |
| layer-python-highs-exact | 0 | Optimal | 618.3299 | 7 | 137 | 1.08 | 0.02 | 0.00 | 0.01 | 0.01 | 54 |
| layer-python-highs-heuristic | 0 | Optimal | 618.3299 | 6 | 663 | 1.08 | 0.02 | 0.01 | 0.01 | 0.01 | 61 |
| layer-python-highspy-exact | 0 | Optimal | 618.3299 | 7 | 137 | 0.98 | 0.02 | 0.00 | 0.01 | 0.01 | 53 |
| layer-python-highspy-heuristic | 0 | Optimal | 618.3299 | 6 | 663 | 0.99 | 0.02 | 0.01 | 0.01 | 0.01 | 51 |
| layer-python-gurobipy-exact | 0 | Optimal | 618.3299 | 8 | 127 | 1.08 | 0.05 | 0.03 | 0.01 | 0.01 | 63 |
| layer-python-gurobipy-heuristic | 0 | Optimal | 618.3299 | 6 | 697 | 1.09 | 0.06 | 0.03 | 0.02 | 0.01 | 59 |
| example-cpp | 0 | Optimal | 618.3299 | 11 | - | 0.43 | 0.05 | - | - | - | 32 |

## R201_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 449.6598 | 14 | 2985 | 1.95 | 0.81 | 0.18 | 0.40 | 0.20 | 78 |
| layer-cpp-highs-exact | 0 | SolverError | 1736555498.6812 | 1 | 2007 | 1.48 | 0.40 | 0.01 | 0.39 | 0.00 | 64 |
| example-cpp | 0 | Optimal | 449.6598 | 22 | - | 1.29 | 0.94 | - | - | - | 43 |
| layer-cpp-highspy-exact | 0 | SolverError | 1736555498.6812 | 1 | 2007 | 1.93 | 0.41 | 0.01 | 0.40 | 0.00 | 70 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 449.6598 | 18 | 4577 | 2.29 | 0.71 | 0.12 | 0.58 | 0.01 | 74 |
| layer-python-highs-exact | 0 | SolverError | 1736555498.6812 | 1 | 2007 | 1.50 | 0.51 | 0.01 | 0.49 | 0.01 | 67 |
| layer-python-highspy-exact | 0 | SolverError | 1736555498.6812 | 1 | 2007 | 1.50 | 0.46 | 0.01 | 0.45 | 0.01 | 68 |
| layer-python-gurobipy-exact | 0 | Optimal | 449.6598 | 18 | 4577 | 1.95 | 0.79 | 0.11 | 0.66 | 0.01 | 71 |

## RC201_12

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 427.2356 | 16 | 1058 | 1.47 | 0.32 | 0.15 | 0.06 | 0.07 | 67 |
| example-cpp | 0 | Optimal | 427.2356 | 22 | - | 0.43 | 0.20 | - | - | - | 34 |
| layer-cpp-highs-exact | 0 | SolverError | 1825267103.7413 | 1 | 872 | 1.07 | 0.06 | 0.00 | 0.06 | 0.00 | 63 |
| layer-cpp-highspy-exact | 0 | SolverError | 1825267103.7413 | 1 | 872 | 1.07 | 0.06 | 0.00 | 0.06 | 0.00 | 64 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 427.2356 | 14 | 2437 | 1.28 | 0.25 | 0.07 | 0.18 | 0.00 | 67 |
| layer-python-highs-exact | 0 | SolverError | 1825267103.7413 | 1 | 872 | 1.23 | 0.10 | 0.00 | 0.09 | 0.01 | 63 |
| layer-python-highspy-exact | 0 | SolverError | 1825267103.7413 | 1 | 872 | 1.08 | 0.07 | 0.00 | 0.07 | 0.00 | 64 |
| layer-python-gurobipy-exact | 0 | Optimal | 427.2356 | 14 | 2437 | 1.28 | 0.28 | 0.07 | 0.21 | 0.01 | 67 |

## C101_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 363.2468 | 43 | 1607 | 1.49 | 0.56 | 0.32 | 0.12 | 0.09 | 72 |
| example-cpp | 0 | Optimal | 363.2468 | 44 | - | 1.07 | 0.68 | - | - | - | 37 |
| layer-cpp-highs-exact | 0 | Optimal | 363.2468 | 45 | 1456 | 1.24 | 0.27 | 0.07 | 0.20 | 0.00 | 66 |
| layer-cpp-highspy-exact | 0 | Optimal | 363.2468 | 45 | 1456 | 1.28 | 0.25 | 0.07 | 0.18 | 0.00 | 69 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 363.2468 | 32 | 829 | 1.28 | 0.28 | 0.12 | 0.15 | 0.00 | 67 |
| layer-python-highs-exact | 0 | Optimal | 363.2468 | 45 | 1456 | 1.29 | 0.35 | 0.08 | 0.27 | 0.01 | 65 |
| layer-python-highspy-exact | 0 | Optimal | 363.2468 | 45 | 1456 | 1.31 | 0.27 | 0.07 | 0.19 | 0.01 | 69 |
| layer-python-gurobipy-exact | 0 | Optimal | 363.2468 | 32 | 829 | 1.70 | 0.29 | 0.13 | 0.16 | 0.01 | 67 |

## R101_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 1046.7011 | 15 | 406 | 1.74 | 0.24 | 0.13 | 0.06 | 0.03 | 72 |
| example-cpp | 0 | Optimal | 1046.7011 | 15 | - | 0.44 | 0.14 | - | - | - | 32 |
| layer-cpp-highs-exact | 0 | Optimal | 1046.7011 | 18 | 461 | 1.07 | 0.07 | 0.02 | 0.05 | 0.00 | 54 |
| layer-cpp-highspy-exact | 0 | Optimal | 1046.7011 | 18 | 461 | 1.10 | 0.08 | 0.02 | 0.06 | 0.00 | 68 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 1046.7011 | 18 | 449 | 1.21 | 0.13 | 0.07 | 0.06 | 0.00 | 67 |
| layer-python-highs-exact | 0 | Optimal | 1046.7011 | 18 | 461 | 1.07 | 0.09 | 0.02 | 0.06 | 0.01 | 64 |
| layer-python-highspy-exact | 0 | Optimal | 1046.7011 | 18 | 461 | 1.07 | 0.08 | 0.02 | 0.06 | 0.01 | 66 |
| layer-python-gurobipy-exact | 0 | Optimal | 1046.7011 | 18 | 449 | 1.07 | 0.14 | 0.07 | 0.06 | 0.01 | 67 |

## RC101_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 828.3619 | 24 | 670 | 1.84 | 0.35 | 0.18 | 0.09 | 0.04 | 68 |
| example-cpp | 0 | Optimal | 828.3619 | 21 | - | 0.73 | 0.25 | - | - | - | 36 |
| layer-cpp-highs-exact | 0 | Optimal | 828.3619 | 25 | 657 | 1.30 | 0.13 | 0.02 | 0.10 | 0.00 | 65 |
| layer-cpp-highspy-exact | 0 | Optimal | 828.3619 | 25 | 657 | 1.07 | 0.13 | 0.02 | 0.11 | 0.00 | 68 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 828.3619 | 26 | 739 | 1.75 | 0.23 | 0.11 | 0.12 | 0.00 | 67 |
| layer-python-highs-exact | 0 | Optimal | 828.3619 | 25 | 657 | 1.27 | 0.17 | 0.03 | 0.14 | 0.01 | 65 |
| layer-python-highspy-exact | 0 | Optimal | 828.3619 | 25 | 657 | 1.32 | 0.12 | 0.02 | 0.10 | 0.01 | 68 |
| layer-python-gurobipy-exact | 0 | Optimal | 828.3619 | 26 | 739 | 1.28 | 0.29 | 0.12 | 0.16 | 0.01 | 67 |

## By configuration (mean wall s)

| config | C101_25 | R101_25 | R201_25 | RC201_12 | C101_50 | R101_50 | RC101_50 |
|---|---|---|---|---|---|---|---|
| example-mip | 1.51 | 1.32 | 1.95 | 1.47 | 1.49 | 1.74 | 1.84 |
| example-cpp | 0.43 | 0.43 | 1.29 | 0.43 | 1.07 | 0.44 | 0.73 |
| layer-cpp-highs-exact | 1.09 | 1.08 | 1.48 | 1.07 | 1.24 | 1.07 | 1.30 |
| layer-cpp-highs-heuristic | 14.75 | 1.10 | - | - | - | - | - |
| layer-cpp-highspy-exact | 1.83 | 1.08 | 1.93 | 1.07 | 1.28 | 1.10 | 1.07 |
| layer-cpp-highspy-heuristic | 20.70 | 1.28 | - | - | - | - | - |
| layer-cpp-gurobipy-exact | 1.30 | 1.08 | 2.29 | 1.28 | 1.28 | 1.21 | 1.75 |
| layer-cpp-gurobipy-heuristic | 4.96 | 1.08 | - | - | - | - | - |
| layer-python-highs-exact | 1.08 | 1.08 | 1.50 | 1.23 | 1.29 | 1.07 | 1.27 |
| layer-python-highs-heuristic | 13.89 | 1.08 | - | - | - | - | - |
| layer-python-highspy-exact | 1.08 | 0.98 | 1.50 | 1.08 | 1.31 | 1.07 | 1.32 |
| layer-python-highspy-heuristic | 14.48 | 0.99 | - | - | - | - | - |
| layer-python-gurobipy-exact | 1.38 | 1.08 | 1.95 | 1.28 | 1.70 | 1.07 | 1.28 |
| layer-python-gurobipy-heuristic | 5.98 | 1.09 | - | - | - | - | - |

## By configuration (mean solve s)

| config | C101_25 | R101_25 | R201_25 | RC201_12 | C101_50 | R101_50 | RC101_50 |
|---|---|---|---|---|---|---|---|
| example-mip | 0.32 | 0.12 | 0.81 | 0.32 | 0.56 | 0.24 | 0.35 |
| example-cpp | 0.13 | 0.05 | 0.94 | 0.20 | 0.68 | 0.14 | 0.25 |
| layer-cpp-highs-exact | 0.06 | 0.01 | 0.40 | 0.06 | 0.27 | 0.07 | 0.13 |
| layer-cpp-highs-heuristic | 13.66 | 0.02 | - | - | - | - | - |
| layer-cpp-highspy-exact | 0.10 | 0.01 | 0.41 | 0.06 | 0.25 | 0.08 | 0.13 |
| layer-cpp-highspy-heuristic | 18.90 | 0.02 | - | - | - | - | - |
| layer-cpp-gurobipy-exact | 0.17 | 0.04 | 0.71 | 0.25 | 0.28 | 0.13 | 0.23 |
| layer-cpp-gurobipy-heuristic | 3.82 | 0.05 | - | - | - | - | - |
| layer-python-highs-exact | 0.07 | 0.02 | 0.51 | 0.10 | 0.35 | 0.09 | 0.17 |
| layer-python-highs-heuristic | 12.81 | 0.02 | - | - | - | - | - |
| layer-python-highspy-exact | 0.07 | 0.02 | 0.46 | 0.07 | 0.27 | 0.08 | 0.12 |
| layer-python-highspy-heuristic | 13.43 | 0.02 | - | - | - | - | - |
| layer-python-gurobipy-exact | 0.18 | 0.05 | 0.79 | 0.28 | 0.29 | 0.14 | 0.29 |
| layer-python-gurobipy-heuristic | 4.92 | 0.06 | - | - | - | - | - |

## LP mismatches

R201_25: LP from 449.6598 to 1736555498.6812
RC201_12: LP from 427.2356 to 1825267103.7413
