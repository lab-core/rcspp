# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 191.8136 | 18 | 666 | 1.06 | 0.04 | 0.02 | 0.03 | 0.00 | 63 |
| layer-purepython-highspy-exact | 0 | Optimal | 191.8136 | 18 | 666 | 1.28 | 0.08 | 0.02 | 0.05 | 0.01 | 65 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 191.8136 | 29 | 723 | 1.29 | 0.18 | 0.12 | 0.06 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 191.8136 | 29 | 723 | 1.30 | 0.24 | 0.14 | 0.09 | 0.01 | 65 |
| layer-cpp-highspy-exact | 1 | Optimal | 191.8136 | 18 | 666 | 1.00 | 0.05 | 0.02 | 0.03 | 0.00 | 52 |
| layer-purepython-highspy-exact | 1 | Optimal | 191.8136 | 18 | 666 | 1.29 | 0.09 | 0.03 | 0.06 | 0.00 | 65 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 191.8136 | 29 | 723 | 1.29 | 0.18 | 0.12 | 0.06 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 191.8136 | 29 | 723 | 1.51 | 0.20 | 0.12 | 0.07 | 0.01 | 65 |
| layer-cpp-highspy-exact | 2 | Optimal | 191.8136 | 18 | 666 | 1.23* | 0.05* | 0.02 | 0.03 | 0.00 | 51 |
| layer-purepython-highspy-exact | 2 | Optimal | 191.8136 | 18 | 666 | 1.29 | 0.06 | 0.02 | 0.04 | 0.00 | 54 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 191.8136 | 29 | 723 | 1.28 | 0.18 | 0.12 | 0.06 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 191.8136 | 29 | 723 | 1.29 | 0.19 | 0.11 | 0.07 | 0.00 | 64 |
| layer-cpp-highspy-exact | 3 | Optimal | 191.8136 | 18 | 666 | 1.07 | 0.05 | 0.02 | 0.03 | 0.00 | 64 |
| layer-purepython-highspy-exact | 3 | Optimal | 191.8136 | 18 | 666 | 1.11* | 0.07* | 0.02 | 0.04 | 0.00 | 54 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 191.8136 | 29 | 723 | 1.28 | 0.18 | 0.11 | 0.06 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 191.8136 | 29 | 723 | 1.28 | 0.18 | 0.11 | 0.06 | 0.01 | 65 |
| layer-cpp-highspy-exact | 4 | Optimal | 191.8136 | 18 | 666 | 1.06 | 0.04 | 0.02 | 0.03 | 0.00 | 64 |
| layer-purepython-highspy-exact | 4 | Optimal | 191.8136 | 18 | 666 | 1.07 | 0.06 | 0.02 | 0.04 | 0.00 | 64 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 191.8136 | 29 | 723 | 1.14 | 0.18 | 0.11 | 0.06 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 191.8136 | 29 | 723 | 1.40* | 0.20* | 0.12 | 0.07 | 0.01 | 64 |

## R201_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 449.6598 | 12 | 3024 | 1.62 | 0.49 | 0.06 | 0.43 | 0.00 | 70 |
| layer-purepython-highspy-exact | 0 | Optimal | 449.6598 | 12 | 3024 | 1.73 | 0.53 | 0.07 | 0.46 | 0.00 | 71 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 449.6598 | 11 | 3247 | 1.73 | 0.60 | 0.07 | 0.53 | 0.00 | 70 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 449.6598 | 11 | 3247 | 1.85 | 0.55 | 0.06 | 0.49 | 0.00 | 71 |
| layer-cpp-highspy-exact | 1 | Optimal | 449.6598 | 12 | 3024 | 1.62 | 0.54 | 0.06 | 0.48 | 0.00 | 70 |
| layer-purepython-highspy-exact | 1 | Optimal | 449.6598 | 12 | 3024 | 1.72* | 0.59* | 0.06 | 0.52 | 0.00 | 71 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 449.6598 | 11 | 3247 | 1.95 | 0.55 | 0.06 | 0.49 | 0.00 | 70 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 449.6598 | 11 | 3247 | 1.76 | 0.55 | 0.06 | 0.48 | 0.00 | 71 |
| layer-cpp-highspy-exact | 2 | Optimal | 449.6598 | 12 | 3024 | 1.84 | 0.69 | 0.07 | 0.62 | 0.00 | 70 |
| layer-purepython-highspy-exact | 2 | Optimal | 449.6598 | 12 | 3024 | 1.58* | 0.50* | 0.06 | 0.43 | 0.00 | 71 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 449.6598 | 11 | 3247 | 1.50 | 0.54 | 0.06 | 0.48 | 0.00 | 70 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 449.6598 | 11 | 3247 | 1.71 | 0.56 | 0.06 | 0.49 | 0.00 | 71 |
| layer-cpp-highspy-exact | 3 | Optimal | 449.6598 | 12 | 3024 | 1.50 | 0.48 | 0.06 | 0.42 | 0.00 | 70 |
| layer-purepython-highspy-exact | 3 | Optimal | 449.6598 | 12 | 3024 | 1.61 | 0.54 | 0.06 | 0.47 | 0.00 | 71 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 449.6598 | 11 | 3247 | 1.50 | 0.58 | 0.07 | 0.51 | 0.00 | 70 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 449.6598 | 11 | 3247 | 1.71 | 0.55 | 0.06 | 0.48 | 0.00 | 71 |
| layer-cpp-highspy-exact | 4 | Optimal | 449.6598 | 12 | 3024 | 1.75 | 0.50 | 0.06 | 0.44 | 0.00 | 70 |
| layer-purepython-highspy-exact | 4 | Optimal | 449.6598 | 12 | 3024 | 1.71 | 0.55 | 0.06 | 0.48 | 0.00 | 72 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 449.6598 | 11 | 3247 | 1.87* | 0.61* | 0.07 | 0.54 | 0.00 | 70 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 449.6598 | 11 | 3247 | 1.71 | 0.58 | 0.06 | 0.51 | 0.01 | 71 |

## RC201_12

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 427.2356 | 16 | 1050 | 1.08 | 0.09 | 0.02 | 0.07 | 0.00 | 65 |
| layer-purepython-highspy-exact | 0 | Optimal | 427.2356 | 16 | 1050 | 1.29 | 0.11 | 0.03 | 0.08 | 0.00 | 57 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 427.2356 | 16 | 1046 | 1.29 | 0.15 | 0.07 | 0.08 | 0.00 | 64 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 427.2356 | 16 | 1046 | 1.29 | 0.16 | 0.07 | 0.09 | 0.00 | 65 |
| layer-cpp-highspy-exact | 1 | Optimal | 427.2356 | 16 | 1050 | 1.21* | 0.08* | 0.02 | 0.06 | 0.00 | 65 |
| layer-purepython-highspy-exact | 1 | Optimal | 427.2356 | 16 | 1050 | 1.29 | 0.15 | 0.03 | 0.11 | 0.00 | 66 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 427.2356 | 16 | 1046 | 1.29 | 0.15 | 0.07 | 0.09 | 0.00 | 64 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 427.2356 | 16 | 1046 | 1.29 | 0.16 | 0.07 | 0.09 | 0.00 | 65 |
| layer-cpp-highspy-exact | 2 | Optimal | 427.2356 | 16 | 1050 | 1.23 | 0.10 | 0.03 | 0.07 | 0.00 | 64 |
| layer-purepython-highspy-exact | 2 | Optimal | 427.2356 | 16 | 1050 | 1.30 | 0.12 | 0.03 | 0.09 | 0.00 | 66 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 427.2356 | 16 | 1046 | 1.29 | 0.15 | 0.07 | 0.08 | 0.00 | 64 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 427.2356 | 16 | 1046 | 1.29 | 0.15 | 0.07 | 0.08 | 0.00 | 65 |
| layer-cpp-highspy-exact | 3 | Optimal | 427.2356 | 16 | 1050 | 1.07 | 0.09 | 0.02 | 0.06 | 0.00 | 64 |
| layer-purepython-highspy-exact | 3 | Optimal | 427.2356 | 16 | 1050 | 1.14 | 0.12 | 0.03 | 0.09 | 0.00 | 66 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 427.2356 | 16 | 1046 | 1.17* | 0.15* | 0.07 | 0.08 | 0.00 | 64 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 427.2356 | 16 | 1046 | 1.28 | 0.16 | 0.07 | 0.09 | 0.00 | 65 |
| layer-cpp-highspy-exact | 4 | Optimal | 427.2356 | 16 | 1050 | 1.08 | 0.09 | 0.03 | 0.07 | 0.00 | 57 |
| layer-purepython-highspy-exact | 4 | Optimal | 427.2356 | 16 | 1050 | 1.29 | 0.12 | 0.03 | 0.09 | 0.00 | 66 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 427.2356 | 16 | 1046 | 1.51 | 0.19 | 0.08 | 0.10 | 0.00 | 64 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 427.2356 | 16 | 1046 | 1.75 | 0.17 | 0.07 | 0.09 | 0.00 | 65 |

## By configuration (mean wall s)

| config | C101_25 | R201_25 | RC201_12 |
|---|---|---|---|
| layer-cpp-highspy-exact | 1.09 | 1.67 | 1.13 |
| layer-cpp-gurobipy-exact | 1.26 | 1.71 | 1.31 |
| layer-purepython-highspy-exact | 1.21 | 1.67 | 1.26 |
| layer-purepython-gurobipy-exact | 1.35 | 1.75 | 1.38 |

## By configuration (mean solve s)

| config | C101_25 | R201_25 | RC201_12 |
|---|---|---|---|
| layer-cpp-highspy-exact | 0.05 | 0.54 | 0.09 |
| layer-cpp-gurobipy-exact | 0.18 | 0.58 | 0.16 |
| layer-purepython-highspy-exact | 0.07 | 0.54 | 0.12 |
| layer-purepython-gurobipy-exact | 0.20 | 0.56 | 0.16 |

## LP mismatches

none
