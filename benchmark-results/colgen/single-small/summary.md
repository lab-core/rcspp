# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highs-exact | 0 | Optimal | 191.8136 | 18 | 666 | 1.06 | 0.05 | 0.02 | 0.04 | 0.00 | 54 |
| layer-cpp-highspy-exact | 0 | Optimal | 191.8136 | 18 | 666 | 1.06 | 0.05 | 0.02 | 0.03 | 0.00 | 64 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 191.8136 | 29 | 723 | 1.50 | 0.18 | 0.12 | 0.06 | 0.00 | 63 |
| layer-python-highs-exact | 0 | Optimal | 191.8136 | 18 | 666 | 1.06 | 0.07 | 0.02 | 0.04 | 0.01 | 59 |
| layer-python-highspy-exact | 0 | Optimal | 191.8136 | 18 | 666 | 1.20 | 0.06 | 0.02 | 0.04 | 0.01 | 53 |
| layer-python-gurobipy-exact | 0 | Optimal | 191.8136 | 29 | 723 | 1.27 | 0.18 | 0.11 | 0.06 | 0.01 | 64 |

## R101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highs-exact | 0 | Optimal | 618.3299 | 7 | 99 | 1.06 | 0.01 | 0.00 | 0.01 | 0.00 | 53 |
| layer-cpp-highspy-exact | 0 | Optimal | 618.3299 | 7 | 99 | 1.06 | 0.01 | 0.00 | 0.01 | 0.00 | 63 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 618.3299 | 6 | 90 | 1.06 | 0.03 | 0.02 | 0.01 | 0.00 | 63 |
| layer-python-highs-exact | 0 | Optimal | 618.3299 | 7 | 99 | 0.92 | 0.02 | 0.00 | 0.01 | 0.01 | 49 |
| layer-python-highspy-exact | 0 | Optimal | 618.3299 | 7 | 99 | 1.00* | 0.02* | 0.00 | 0.01 | 0.01 | 52 |
| layer-python-gurobipy-exact | 0 | Optimal | 618.3299 | 6 | 90 | 1.06 | 0.04 | 0.02 | 0.01 | 0.01 | 63 |

## R201_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highs-exact | 0 | Optimal | 449.6598 | 12 | 3024 | 1.48 | 0.45 | 0.06 | 0.40 | 0.00 | 67 |
| layer-cpp-highspy-exact | 0 | Optimal | 449.6598 | 12 | 3024 | 1.48 | 0.50 | 0.07 | 0.44 | 0.00 | 71 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 449.6598 | 11 | 3247 | 1.60* | 0.61* | 0.06 | 0.55 | 0.00 | 70 |
| layer-python-highs-exact | 0 | Optimal | 449.6598 | 12 | 3024 | 1.48 | 0.47 | 0.06 | 0.41 | 0.01 | 67 |
| layer-python-highspy-exact | 0 | Optimal | 449.6598 | 12 | 3024 | 1.48 | 0.50 | 0.06 | 0.44 | 0.01 | 71 |
| layer-python-gurobipy-exact | 0 | Optimal | 449.6598 | 11 | 3247 | 1.48 | 0.55 | 0.06 | 0.48 | 0.01 | 70 |

## RC201_12

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highs-exact | 0 | Optimal | 427.2356 | 16 | 1050 | 1.21* | 0.11* | 0.02 | 0.09 | 0.00 | 61 |
| layer-cpp-highspy-exact | 0 | Optimal | 427.2356 | 16 | 1050 | 1.06 | 0.08 | 0.02 | 0.06 | 0.00 | 65 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 427.2356 | 16 | 1046 | 1.06 | 0.14 | 0.06 | 0.07 | 0.00 | 64 |
| layer-python-highs-exact | 0 | Optimal | 427.2356 | 16 | 1050 | 1.06 | 0.09 | 0.02 | 0.06 | 0.01 | 62 |
| layer-python-highspy-exact | 0 | Optimal | 427.2356 | 16 | 1050 | 1.06 | 0.09 | 0.02 | 0.06 | 0.01 | 65 |
| layer-python-gurobipy-exact | 0 | Optimal | 427.2356 | 16 | 1046 | 1.60 | 0.45 | 0.35 | 0.09 | 0.01 | 67 |

## C101_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highs-exact | 0 | Optimal | 363.2468 | 54 | 1749 | 1.27 | 0.35 | 0.11 | 0.24 | 0.00 | 66 |
| layer-cpp-highspy-exact | 0 | Optimal | 363.2468 | 54 | 1749 | 1.27 | 0.33 | 0.11 | 0.22 | 0.00 | 69 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 363.2468 | 43 | 1702 | 1.49 | 0.55 | 0.24 | 0.31 | 0.01 | 68 |
| layer-python-highs-exact | 0 | Optimal | 363.2468 | 54 | 1749 | 1.34 | 0.37 | 0.12 | 0.25 | 0.01 | 66 |
| layer-python-highspy-exact | 0 | Optimal | 363.2468 | 54 | 1749 | 1.35* | 0.34* | 0.11 | 0.23 | 0.01 | 69 |
| layer-python-gurobipy-exact | 0 | Optimal | 363.2468 | 43 | 1702 | 1.48 | 0.44 | 0.19 | 0.24 | 0.01 | 68 |

## R101_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highs-exact | 0 | Optimal | 1046.7011 | 16 | 410 | 1.06 | 0.07 | 0.01 | 0.06 | 0.00 | 57 |
| layer-cpp-highspy-exact | 0 | Optimal | 1046.7011 | 16 | 410 | 1.06 | 0.06 | 0.01 | 0.05 | 0.00 | 58 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 1046.7011 | 14 | 363 | 1.40* | 0.12* | 0.06 | 0.06 | 0.00 | 66 |
| layer-python-highs-exact | 0 | Optimal | 1046.7011 | 16 | 410 | 1.06 | 0.07 | 0.01 | 0.05 | 0.01 | 64 |
| layer-python-highspy-exact | 0 | Optimal | 1046.7011 | 16 | 410 | 1.27 | 0.07 | 0.01 | 0.05 | 0.01 | 55 |
| layer-python-gurobipy-exact | 0 | Optimal | 1046.7011 | 14 | 363 | 1.27 | 0.12 | 0.06 | 0.06 | 0.01 | 67 |

## RC101_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highs-exact | 0 | Optimal | 828.3619 | 22 | 678 | 1.06 | 0.12 | 0.02 | 0.10 | 0.00 | 64 |
| layer-cpp-highspy-exact | 0 | Optimal | 828.3619 | 22 | 678 | 1.25* | 0.11* | 0.02 | 0.09 | 0.00 | 68 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 828.3619 | 28 | 715 | 1.49 | 0.32 | 0.13 | 0.19 | 0.00 | 67 |
| layer-python-highs-exact | 0 | Optimal | 828.3619 | 22 | 678 | 1.06 | 0.11 | 0.02 | 0.08 | 0.01 | 58 |
| layer-python-highspy-exact | 0 | Optimal | 828.3619 | 22 | 678 | 1.06 | 0.12 | 0.02 | 0.09 | 0.01 | 68 |
| layer-python-gurobipy-exact | 0 | Optimal | 828.3619 | 28 | 715 | 1.37 | 0.25 | 0.11 | 0.13 | 0.01 | 67 |

## By configuration (mean wall s)

| config | C101_25 | R101_25 | R201_25 | RC201_12 | C101_50 | R101_50 | RC101_50 |
|---|---|---|---|---|---|---|---|
| layer-cpp-highs-exact | 1.06 | 1.06 | 1.48 | 1.21 | 1.27 | 1.06 | 1.06 |
| layer-cpp-highspy-exact | 1.06 | 1.06 | 1.48 | 1.06 | 1.27 | 1.06 | 1.25 |
| layer-cpp-gurobipy-exact | 1.50 | 1.06 | 1.60 | 1.06 | 1.49 | 1.40 | 1.49 |
| layer-python-highs-exact | 1.06 | 0.92 | 1.48 | 1.06 | 1.34 | 1.06 | 1.06 |
| layer-python-highspy-exact | 1.20 | 1.00 | 1.48 | 1.06 | 1.35 | 1.27 | 1.06 |
| layer-python-gurobipy-exact | 1.27 | 1.06 | 1.48 | 1.60 | 1.48 | 1.27 | 1.37 |

## By configuration (mean solve s)

| config | C101_25 | R101_25 | R201_25 | RC201_12 | C101_50 | R101_50 | RC101_50 |
|---|---|---|---|---|---|---|---|
| layer-cpp-highs-exact | 0.05 | 0.01 | 0.45 | 0.11 | 0.35 | 0.07 | 0.12 |
| layer-cpp-highspy-exact | 0.05 | 0.01 | 0.50 | 0.08 | 0.33 | 0.06 | 0.11 |
| layer-cpp-gurobipy-exact | 0.18 | 0.03 | 0.61 | 0.14 | 0.55 | 0.12 | 0.32 |
| layer-python-highs-exact | 0.07 | 0.02 | 0.47 | 0.09 | 0.37 | 0.07 | 0.11 |
| layer-python-highspy-exact | 0.06 | 0.02 | 0.50 | 0.09 | 0.34 | 0.07 | 0.12 |
| layer-python-gurobipy-exact | 0.18 | 0.04 | 0.55 | 0.45 | 0.44 | 0.12 | 0.25 |

## LP mismatches

none
