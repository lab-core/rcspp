# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101

Baseline LP (current C++ example): 828.94

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 828.9369 | 80 | 3847 | 3.66 | 2.50 | 1.30 | 0.88 | 0.25 | 99 |
| example-cpp | 0 | Optimal | 828.9369 | 87 | - | 3.35 | 3.06 | - | - | - | 48 |
| layer-cpp-highs-exact | 0 | Optimal | 828.9369 | 74 | 3143 | 2.55 | 1.55 | 0.26 | 1.29 | 0.00 | 76 |
| layer-cpp-highspy-exact | 0 | Optimal | 828.9369 | 74 | 3143 | 3.14 | 1.81 | 0.28 | 1.53 | 0.00 | 80 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 828.9369 | 90 | 4263 | 3.83 | 2.70 | 0.89 | 1.79 | 0.02 | 82 |
| layer-python-highs-exact | 0 | Optimal | 828.9369 | 74 | 3143 | 2.74 | 1.59 | 0.26 | 1.31 | 0.01 | 77 |
| layer-python-highspy-exact | 0 | Optimal | 828.9369 | 74 | 3143 | 2.77 | 1.60 | 0.26 | 1.32 | 0.01 | 79 |
| layer-python-gurobipy-exact | 0 | Optimal | 828.9369 | 90 | 4263 | 3.77 | 2.51 | 0.62 | 1.86 | 0.02 | 79 |

## R101

Baseline LP (current C++ example): 1636.39

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 1636.3887 | 32 | 1612 | 1.92 | 0.95 | 0.30 | 0.28 | 0.08 | 95 |
| example-cpp | 0 | Optimal | 1636.3887 | 33 | - | 1.28 | 0.83 | - | - | - | 48 |
| layer-cpp-highs-exact | 0 | Optimal | 1636.3887 | 32 | 1828 | 1.71 | 0.62 | 0.14 | 0.48 | 0.00 | 75 |
| layer-cpp-highspy-exact | 0 | Optimal | 1636.3887 | 32 | 1828 | 1.73 | 0.57 | 0.14 | 0.43 | 0.00 | 78 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 1636.3887 | 36 | 2050 | 1.92 | 0.75 | 0.22 | 0.53 | 0.01 | 77 |
| layer-python-highs-exact | 0 | Optimal | 1636.3887 | 32 | 1828 | 1.70 | 0.56 | 0.13 | 0.42 | 0.01 | 76 |
| layer-python-highspy-exact | 0 | Optimal | 1636.3887 | 32 | 1828 | 1.89 | 0.66 | 0.14 | 0.51 | 0.01 | 78 |
| layer-python-gurobipy-exact | 0 | Optimal | 1636.3887 | 36 | 2050 | 1.92 | 0.75 | 0.21 | 0.53 | 0.01 | 78 |

## RC101

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 1572.7390 | 36 | 2403 | 2.55 | 1.51 | 0.35 | 0.59 | 0.13 | 103 |
| example-cpp | 0 | Optimal | 1572.7390 | 33 | - | 1.90 | 1.48 | - | - | - | 50 |
| layer-cpp-highs-exact | 0 | Optimal | 1572.7390 | 33 | 2294 | 2.13 | 0.98 | 0.20 | 0.78 | 0.00 | 77 |
| layer-cpp-highspy-exact | 0 | Optimal | 1572.7390 | 33 | 2294 | 2.12 | 1.02 | 0.21 | 0.81 | 0.00 | 79 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 1572.7390 | 37 | 2633 | 2.46 | 1.18 | 0.25 | 0.93 | 0.01 | 78 |
| layer-python-highs-exact | 0 | Optimal | 1572.7390 | 33 | 2294 | 2.13 | 0.97 | 0.20 | 0.76 | 0.01 | 77 |
| layer-python-highspy-exact | 0 | Optimal | 1572.7390 | 33 | 2294 | 2.16 | 0.95 | 0.20 | 0.74 | 0.01 | 79 |
| layer-python-gurobipy-exact | 0 | Optimal | 1572.7390 | 37 | 2633 | 2.51 | 1.18 | 0.25 | 0.92 | 0.01 | 78 |

## C201

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 591.5566 | 294 | 50525 | 52.57 | 51.47 | 34.68 | 10.19 | 4.93 | 393 |
| example-cpp | 0 | Optimal | 591.5566 | 245 | - | 52.93 | 52.57 | - | - | - | 100 |
| layer-cpp-highs-exact | 0 | Optimal | 591.5566 | 170 | 30896 | 28.59 | 27.20 | 18.98 | 8.22 | 0.00 | 131 |
| layer-cpp-highspy-exact | 0 | Optimal | 591.5566 | 170 | 30896 | 28.49 | 27.14 | 18.64 | 8.51 | 0.00 | 133 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 591.5566 | 328 | 68013 | 44.88 | 43.76 | 24.37 | 19.32 | 0.07 | 207 |
| layer-python-highs-exact | 0 | Optimal | 591.5566 | 170 | 30896 | 28.06 | 26.94 | 18.77 | 8.16 | 0.02 | 131 |
| layer-python-highspy-exact | 0 | Optimal | 591.5566 | 170 | 30896 | 27.92 | 26.80 | 18.74 | 8.04 | 0.02 | 133 |
| layer-python-gurobipy-exact | 0 | Optimal | 591.5566 | 328 | 68013 | 45.87 | 44.23 | 24.48 | 19.65 | 0.09 | 210 |

## R201

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 1084.9907 | 45 | 72539 | 1746.03 | 1744.89 | 11.41 | 1725.30 | 7.64 | 443 |
| example-cpp | 0 | Optimal | 1084.9907 | 91 | - | 2093.52 | 2092.98 | - | - | - | 166 |
| layer-cpp-highs-exact | 0 | SolverError | 9183136719.0084 | 1 | 29241 | 3515.02 | 3512.26 | 0.09 | 3512.17 | 0.00 | 324 |
| layer-cpp-highspy-exact | 0 | SolverError | 9183136719.0084 | 1 | 29241 | 3230.81 | 3229.41 | 0.09 | 3229.32 | 0.00 | 327 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 1084.9907 | 45 | 121759 | 7041.69 | 7039.60 | 8.75 | 7030.80 | 0.05 | 420 |
| layer-python-highs-exact | 0 | SolverError | 9183136719.0084 | 1 | 29241 | 3132.15 | 3130.83 | 0.08 | 3130.74 | 0.01 | 324 |
| layer-python-highspy-exact | 0 | SolverError | 9183136719.0084 | 1 | 29241 | 3282.43 | 3280.74 | 0.09 | 3280.64 | 0.01 | 327 |
| layer-python-gurobipy-exact | 0 | Optimal | 1084.9907 | 45 | 121759 | 7444.10* | 7442.07* | 8.82 | 7433.18 | 0.07 | 420 |

## By configuration (mean wall s)

| config | C101 | R101 | RC101 | C201 | R201 |
|---|---|---|---|---|---|
| example-mip | 3.66 | 1.92 | 2.55 | 52.57 | 1746.03 |
| example-cpp | 3.35 | 1.28 | 1.90 | 52.93 | 2093.52 |
| layer-cpp-highs-exact | 2.55 | 1.71 | 2.13 | 28.59 | 3515.02 |
| layer-cpp-highspy-exact | 3.14 | 1.73 | 2.12 | 28.49 | 3230.81 |
| layer-cpp-gurobipy-exact | 3.83 | 1.92 | 2.46 | 44.88 | 7041.69 |
| layer-python-highs-exact | 2.74 | 1.70 | 2.13 | 28.06 | 3132.15 |
| layer-python-highspy-exact | 2.77 | 1.89 | 2.16 | 27.92 | 3282.43 |
| layer-python-gurobipy-exact | 3.77 | 1.92 | 2.51 | 45.87 | 7444.10 |

## By configuration (mean solve s)

| config | C101 | R101 | RC101 | C201 | R201 |
|---|---|---|---|---|---|
| example-mip | 2.50 | 0.95 | 1.51 | 51.47 | 1744.89 |
| example-cpp | 3.06 | 0.83 | 1.48 | 52.57 | 2092.98 |
| layer-cpp-highs-exact | 1.55 | 0.62 | 0.98 | 27.20 | 3512.26 |
| layer-cpp-highspy-exact | 1.81 | 0.57 | 1.02 | 27.14 | 3229.41 |
| layer-cpp-gurobipy-exact | 2.70 | 0.75 | 1.18 | 43.76 | 7039.60 |
| layer-python-highs-exact | 1.59 | 0.56 | 0.97 | 26.94 | 3130.83 |
| layer-python-highspy-exact | 1.60 | 0.66 | 0.95 | 26.80 | 3280.74 |
| layer-python-gurobipy-exact | 2.51 | 0.75 | 1.18 | 44.23 | 7442.07 |

## LP mismatches

R201: LP from 1084.9907 to 9183136719.0084
