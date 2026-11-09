# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## R201

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-tabu | 0 | Optimal | 1084.9907 | 68 | 51973 | 1223.68 | 1222.39 | 22.14 | 1200.25 | 0.00 | 248 |
| layer-purepython-highspy-tabu | 0 | Optimal | 1084.9907 | 68 | 51973 | 1251.88 | 1250.54 | 23.18 | 1227.32 | 0.05 | 248 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 1084.9907 | 72 | 55403 | 830.50 | 829.12 | 4.13 | 824.96 | 0.03 | 222 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 1084.9907 | 72 | 55403 | 835.31 | 833.97 | 4.31 | 829.60 | 0.06 | 223 |

## RC201

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-tabu | 0 | Optimal | 1112.8414 | 80 | 22318 | 76.58 | 75.43 | 6.89 | 68.54 | 0.00 | 116 |
| layer-purepython-highspy-tabu | 0 | Optimal | 1112.8414 | 80 | 22318 | 77.94 | 76.64 | 7.06 | 69.54 | 0.04 | 118 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 1112.8414 | 75 | 24854 | 131.32 | 130.14 | 1.96 | 128.16 | 0.02 | 135 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 1112.8414 | 75 | 24854 | 132.70 | 131.37 | 1.94 | 129.38 | 0.05 | 132 |

## By configuration (mean wall s)

| config | R201 | RC201 |
|---|---|---|
| layer-purepython-highspy-tabu | 1251.88 | 77.94 |
| layer-cpp-highspy-tabu | 1223.68 | 76.58 |
| layer-purepython-gurobipy-tabu | 835.31 | 132.70 |
| layer-cpp-gurobipy-tabu | 830.50 | 131.32 |

## By configuration (mean solve s)

| config | R201 | RC201 |
|---|---|---|
| layer-purepython-highspy-tabu | 1250.54 | 76.64 |
| layer-cpp-highspy-tabu | 1222.39 | 75.43 |
| layer-purepython-gurobipy-tabu | 833.97 | 131.37 |
| layer-cpp-gurobipy-tabu | 829.12 | 130.14 |

## LP mismatches

none
