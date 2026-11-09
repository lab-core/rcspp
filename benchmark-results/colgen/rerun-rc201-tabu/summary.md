# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## RC201

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-tabu | 0 | Optimal | 1112.8414 | 71 | 21411 | 69.42* | 66.90* | 6.07 | 60.83 | 0.00 | 113 |
| layer-purepython-highspy-tabu | 0 | Optimal | 1112.8414 | 71 | 21411 | 67.27* | 66.11* | 6.80 | 59.27 | 0.04 | 118 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 1112.8414 | 72 | 20889 | 85.48* | 83.10* | 1.54 | 81.54 | 0.02 | 129 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 1112.8414 | 72 | 20889 | 66.49 | 65.29 | 1.51 | 63.73 | 0.04 | 127 |
| layer-cpp-highspy-tabu | 1 | Optimal | 1112.8414 | 71 | 21411 | 48.26 | 47.06 | 5.73 | 41.33 | 0.00 | 113 |
| layer-purepython-highspy-tabu | 1 | Optimal | 1112.8414 | 71 | 21411 | 49.46 | 48.34 | 5.94 | 42.36 | 0.04 | 114 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 1112.8414 | 72 | 20889 | 79.39* | 78.30* | 1.81 | 76.47 | 0.02 | 130 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 1112.8414 | 72 | 20889 | 69.25* | 68.03* | 1.54 | 66.44 | 0.05 | 127 |
| layer-cpp-highspy-tabu | 2 | Optimal | 1112.8414 | 71 | 21411 | 48.62 | 47.35 | 5.75 | 41.60 | 0.00 | 113 |
| layer-purepython-highspy-tabu | 2 | Optimal | 1112.8414 | 71 | 21411 | 48.40 | 47.14 | 5.97 | 41.13 | 0.04 | 114 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 1112.8414 | 72 | 20889 | 66.53 | 65.35 | 1.53 | 63.80 | 0.02 | 126 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 1112.8414 | 72 | 20889 | 66.69 | 65.40 | 1.52 | 63.83 | 0.05 | 127 |

## By configuration (mean wall s)

| config | RC201 |
|---|---|
| layer-purepython-highspy-tabu | 55.04 |
| layer-cpp-highspy-tabu | 55.43 |
| layer-purepython-gurobipy-tabu | 67.48 |
| layer-cpp-gurobipy-tabu | 77.13 |

## By configuration (mean solve s)

| config | RC201 |
|---|---|
| layer-purepython-highspy-tabu | 53.86 |
| layer-cpp-highspy-tabu | 53.77 |
| layer-purepython-gurobipy-tabu | 66.24 |
| layer-cpp-gurobipy-tabu | 75.58 |

## LP mismatches

none
