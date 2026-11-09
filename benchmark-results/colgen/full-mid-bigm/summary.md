# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101

Baseline LP (current C++ example): 828.94

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 828.9369 | 55 | 2969 | 2.31 | 1.13 | 0.21 | 0.92 | 0.00 | 79 |
| layer-purepython-highspy-exact | 0 | Optimal | 828.9369 | 55 | 2969 | 2.31 | 1.19 | 0.21 | 0.94 | 0.03 | 79 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 828.9369 | 80 | 3263 | 2.82 | 1.74 | 0.44 | 1.29 | 0.01 | 78 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 828.9369 | 80 | 3263 | 2.93 | 1.80 | 0.44 | 1.32 | 0.04 | 79 |
| layer-cpp-highspy-tabu | 0 | Optimal | 828.9369 | 250 | 2176 | 14.24 | 13.05 | 0.50 | 12.55 | 0.00 | 78 |
| layer-purepython-highspy-tabu | 0 | Optimal | 828.9369 | 250 | 2176 | 14.62 | 13.42 | 0.53 | 12.84 | 0.05 | 79 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 828.9369 | 298 | 2302 | 17.95 | 16.59 | 1.35 | 15.21 | 0.04 | 78 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 828.9369 | 298 | 2302 | 18.48 | 17.32 | 1.36 | 15.89 | 0.07 | 79 |
| layer-cpp-highspy-exact | 1 | Optimal | 828.9369 | 55 | 2969 | 2.16 | 1.13 | 0.21 | 0.92 | 0.00 | 79 |
| layer-purepython-highspy-exact | 1 | Optimal | 828.9369 | 55 | 2969 | 2.30 | 1.21 | 0.21 | 0.96 | 0.04 | 79 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 828.9369 | 80 | 3263 | 2.97 | 1.79 | 0.44 | 1.34 | 0.01 | 78 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 828.9369 | 80 | 3263 | 2.92 | 1.77 | 0.44 | 1.29 | 0.04 | 79 |
| layer-cpp-highspy-tabu | 1 | Optimal | 828.9369 | 250 | 2176 | 14.09 | 13.07 | 0.50 | 12.57 | 0.00 | 78 |
| layer-purepython-highspy-tabu | 1 | Optimal | 828.9369 | 250 | 2176 | 14.66 | 13.58 | 0.53 | 13.00 | 0.06 | 79 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 828.9369 | 298 | 2302 | 17.80 | 16.72 | 1.33 | 15.35 | 0.03 | 79 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 828.9369 | 298 | 2302 | 18.02 | 16.96 | 1.33 | 15.57 | 0.07 | 78 |
| layer-cpp-highspy-exact | 2 | Optimal | 828.9369 | 55 | 2969 | 2.09 | 1.09 | 0.20 | 0.88 | 0.00 | 78 |
| layer-purepython-highspy-exact | 2 | Optimal | 828.9369 | 55 | 2969 | 2.38 | 1.23 | 0.21 | 0.99 | 0.03 | 79 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 828.9369 | 80 | 3263 | 2.71 | 1.71 | 0.44 | 1.27 | 0.01 | 78 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 828.9369 | 80 | 3263 | 3.00 | 1.81 | 0.44 | 1.33 | 0.04 | 79 |
| layer-cpp-highspy-tabu | 2 | Optimal | 828.9369 | 250 | 2176 | 14.24 | 13.13 | 0.50 | 12.63 | 0.00 | 78 |
| layer-purepython-highspy-tabu | 2 | Optimal | 828.9369 | 250 | 2176 | 14.89 | 13.77 | 0.54 | 13.17 | 0.06 | 79 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 828.9369 | 298 | 2302 | 18.61 | 17.53 | 1.60 | 15.90 | 0.03 | 82 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 828.9369 | 298 | 2302 | 17.88 | 16.72 | 1.34 | 15.32 | 0.07 | 78 |

## R101

Baseline LP (current C++ example): 1636.39

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 1636.3887 | 32 | 1838 | 1.68 | 0.52 | 0.13 | 0.39 | 0.00 | 78 |
| layer-purepython-highspy-exact | 0 | Optimal | 1636.3887 | 32 | 1838 | 1.68 | 0.59 | 0.13 | 0.42 | 0.03 | 78 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 1636.3887 | 36 | 1907 | 1.75 | 0.68 | 0.20 | 0.47 | 0.01 | 77 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 1636.3887 | 36 | 1907 | 1.88 | 0.75 | 0.20 | 0.52 | 0.03 | 77 |
| layer-cpp-highspy-tabu | 0 | Optimal | 1636.3887 | 143 | 1356 | 5.71 | 4.56 | 0.26 | 4.29 | 0.00 | 77 |
| layer-purepython-highspy-tabu | 0 | Optimal | 1636.3887 | 143 | 1356 | 5.73 | 4.64 | 0.28 | 4.31 | 0.04 | 77 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 1636.3887 | 174 | 1310 | 7.41 | 6.23 | 0.75 | 5.46 | 0.02 | 76 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 1636.3887 | 174 | 1310 | 7.72 | 6.47 | 0.76 | 5.66 | 0.05 | 76 |
| layer-cpp-highspy-exact | 1 | Optimal | 1636.3887 | 32 | 1838 | 1.71 | 0.53 | 0.13 | 0.40 | 0.00 | 78 |
| layer-purepython-highspy-exact | 1 | Optimal | 1636.3887 | 32 | 1838 | 1.67 | 0.59 | 0.13 | 0.43 | 0.03 | 78 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 1636.3887 | 36 | 1907 | 1.88 | 0.68 | 0.20 | 0.47 | 0.01 | 77 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 1636.3887 | 36 | 1907 | 1.95* | 0.73* | 0.20 | 0.50 | 0.04 | 77 |
| layer-cpp-highspy-tabu | 1 | Optimal | 1636.3887 | 143 | 1356 | 5.73 | 4.72 | 0.27 | 4.45 | 0.00 | 77 |
| layer-purepython-highspy-tabu | 1 | Optimal | 1636.3887 | 143 | 1356 | 5.92 | 4.87 | 0.29 | 4.53 | 0.04 | 77 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 1636.3887 | 174 | 1310 | 7.38 | 6.33 | 0.75 | 5.56 | 0.02 | 76 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 1636.3887 | 174 | 1310 | 7.58 | 6.33 | 0.74 | 5.54 | 0.05 | 77 |
| layer-cpp-highspy-exact | 2 | Optimal | 1636.3887 | 32 | 1838 | 1.68 | 0.53 | 0.13 | 0.40 | 0.00 | 78 |
| layer-purepython-highspy-exact | 2 | Optimal | 1636.3887 | 32 | 1838 | 1.75 | 0.62 | 0.13 | 0.46 | 0.03 | 78 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 1636.3887 | 36 | 1907 | 1.67 | 0.69 | 0.20 | 0.49 | 0.01 | 77 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 1636.3887 | 36 | 1907 | 1.88 | 0.71 | 0.20 | 0.48 | 0.03 | 77 |
| layer-cpp-highspy-tabu | 2 | Optimal | 1636.3887 | 143 | 1356 | 5.51 | 4.45 | 0.27 | 4.19 | 0.00 | 77 |
| layer-purepython-highspy-tabu | 2 | Optimal | 1636.3887 | 143 | 1356 | 5.73 | 4.72 | 0.28 | 4.39 | 0.04 | 77 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 1636.3887 | 174 | 1310 | 7.48 | 6.35 | 0.75 | 5.58 | 0.02 | 76 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 1636.3887 | 174 | 1310 | 7.52 | 6.28 | 0.74 | 5.49 | 0.05 | 77 |

## RC101

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 1572.7390 | 37 | 2441 | 2.09 | 0.97 | 0.22 | 0.76 | 0.00 | 79 |
| layer-purepython-highspy-exact | 0 | Optimal | 1572.7390 | 37 | 2441 | 2.09 | 1.06 | 0.23 | 0.80 | 0.03 | 79 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 1572.7390 | 38 | 2616 | 2.36 | 1.11 | 0.24 | 0.86 | 0.01 | 78 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 1572.7390 | 38 | 2616 | 2.30 | 1.15 | 0.24 | 0.88 | 0.03 | 78 |
| layer-cpp-highspy-tabu | 0 | Optimal | 1572.7390 | 67 | 1806 | 5.30 | 4.15 | 0.20 | 3.95 | 0.00 | 78 |
| layer-purepython-highspy-tabu | 0 | Optimal | 1572.7390 | 67 | 1806 | 5.30 | 4.21 | 0.21 | 3.96 | 0.03 | 77 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 1572.7390 | 73 | 1779 | 5.73 | 4.54 | 0.35 | 4.18 | 0.01 | 77 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 1572.7390 | 73 | 1779 | 5.70 | 4.61 | 0.35 | 4.22 | 0.04 | 77 |
| layer-cpp-highspy-exact | 1 | Optimal | 1572.7390 | 37 | 2441 | 2.16 | 0.99 | 0.22 | 0.77 | 0.00 | 79 |
| layer-purepython-highspy-exact | 1 | Optimal | 1572.7390 | 37 | 2441 | 2.30 | 1.07 | 0.22 | 0.82 | 0.03 | 79 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 1572.7390 | 38 | 2616 | 2.17 | 1.11 | 0.24 | 0.87 | 0.01 | 78 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 1572.7390 | 38 | 2616 | 2.29 | 1.13 | 0.24 | 0.85 | 0.03 | 78 |
| layer-cpp-highspy-tabu | 1 | Optimal | 1572.7390 | 67 | 1806 | 5.10 | 4.10 | 0.20 | 3.90 | 0.00 | 77 |
| layer-purepython-highspy-tabu | 1 | Optimal | 1572.7390 | 67 | 1806 | 5.52 | 4.44 | 0.22 | 4.19 | 0.03 | 77 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 1572.7390 | 73 | 1779 | 5.72 | 4.70 | 0.35 | 4.33 | 0.01 | 77 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 1572.7390 | 73 | 1779 | 5.93 | 4.84 | 0.35 | 4.45 | 0.04 | 77 |
| layer-cpp-highspy-exact | 2 | Optimal | 1572.7390 | 37 | 2441 | 2.09 | 1.03 | 0.22 | 0.82 | 0.00 | 79 |
| layer-purepython-highspy-exact | 2 | Optimal | 1572.7390 | 37 | 2441 | 2.43 | 1.21 | 0.23 | 0.94 | 0.03 | 79 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 1572.7390 | 38 | 2616 | 2.30 | 1.11 | 0.24 | 0.86 | 0.01 | 78 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 1572.7390 | 38 | 2616 | 2.33 | 1.16 | 0.24 | 0.89 | 0.03 | 78 |
| layer-cpp-highspy-tabu | 2 | Optimal | 1572.7390 | 67 | 1806 | 5.19* | 3.97* | 0.20 | 3.77 | 0.00 | 77 |
| layer-purepython-highspy-tabu | 2 | Optimal | 1572.7390 | 67 | 1806 | 5.28 | 4.07 | 0.21 | 3.83 | 0.03 | 77 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 1572.7390 | 73 | 1779 | 5.76 | 4.58 | 0.35 | 4.22 | 0.01 | 77 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 1572.7390 | 73 | 1779 | 5.98 | 4.70 | 0.35 | 4.31 | 0.04 | 77 |

## C201

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 591.5566 | 200 | 37649 | 33.85 | 32.65 | 22.66 | 10.00 | 0.00 | 141 |
| layer-purepython-highspy-exact | 0 | Optimal | 591.5566 | 200 | 37649 | 34.50 | 33.30 | 22.92 | 10.31 | 0.07 | 153 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 591.5566 | 261 | 54909 | 34.01 | 32.50 | 17.33 | 15.11 | 0.06 | 187 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 591.5566 | 261 | 54909 | 33.53 | 32.37 | 17.18 | 15.10 | 0.10 | 200 |
| layer-cpp-highspy-tabu | 0 | Optimal | 591.5566 | 223 | 28887 | 50.79 | 49.67 | 15.92 | 33.75 | 0.00 | 129 |
| layer-purepython-highspy-tabu | 0 | Optimal | 591.5566 | 223 | 28887 | 51.05 | 49.80 | 15.95 | 33.80 | 0.06 | 136 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 591.5566 | 356 | 39435 | 62.20 | 60.91 | 13.48 | 47.37 | 0.06 | 153 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 591.5566 | 356 | 39435 | 63.42 | 62.03 | 13.67 | 48.26 | 0.10 | 171 |
| layer-cpp-highspy-exact | 1 | Optimal | 591.5566 | 200 | 37649 | 33.83 | 32.67 | 22.59 | 10.08 | 0.00 | 142 |
| layer-purepython-highspy-exact | 1 | Optimal | 591.5566 | 200 | 37649 | 34.26 | 33.04 | 22.80 | 10.17 | 0.06 | 154 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 591.5566 | 261 | 54909 | 33.66 | 32.42 | 17.23 | 15.13 | 0.06 | 186 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 591.5566 | 261 | 54909 | 34.13 | 32.84 | 17.48 | 15.27 | 0.09 | 207 |
| layer-cpp-highspy-tabu | 1 | Optimal | 591.5566 | 223 | 28887 | 50.78 | 49.60 | 15.75 | 33.85 | 0.00 | 128 |
| layer-purepython-highspy-tabu | 1 | Optimal | 591.5566 | 223 | 28887 | 51.72 | 50.67 | 16.00 | 34.60 | 0.07 | 135 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 591.5566 | 356 | 39435 | 62.40 | 61.33 | 13.48 | 47.78 | 0.06 | 152 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 591.5566 | 356 | 39435 | 62.18 | 60.97 | 13.40 | 47.46 | 0.10 | 168 |
| layer-cpp-highspy-exact | 2 | Optimal | 591.5566 | 200 | 37649 | 33.96 | 32.91 | 22.92 | 9.98 | 0.00 | 141 |
| layer-purepython-highspy-exact | 2 | Optimal | 591.5566 | 200 | 37649 | 34.62 | 33.47 | 22.99 | 10.41 | 0.07 | 153 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 591.5566 | 261 | 54909 | 33.17 | 32.05 | 17.14 | 14.85 | 0.06 | 183 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 591.5566 | 261 | 54909 | 33.62 | 32.44 | 17.30 | 15.05 | 0.09 | 202 |
| layer-cpp-highspy-tabu | 2 | Optimal | 591.5566 | 223 | 28887 | 50.68 | 49.65 | 15.77 | 33.89 | 0.00 | 129 |
| layer-purepython-highspy-tabu | 2 | Optimal | 591.5566 | 223 | 28887 | 52.25 | 51.08 | 16.17 | 34.85 | 0.06 | 136 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 591.5566 | 356 | 39435 | 62.51 | 61.07 | 13.58 | 47.43 | 0.06 | 156 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 591.5566 | 356 | 39435 | 62.10 | 60.83 | 13.30 | 47.43 | 0.10 | 167 |

## By configuration (mean wall s)

| config | C101 | R101 | RC101 | C201 |
|---|---|---|---|---|
| layer-cpp-highspy-exact | 2.19 | 1.69 | 2.11 | 33.88 |
| layer-cpp-gurobipy-exact | 2.84 | 1.77 | 2.28 | 33.61 |
| layer-purepython-highspy-exact | 2.33 | 1.70 | 2.27 | 34.46 |
| layer-purepython-highspy-tabu | 14.72 | 5.79 | 5.37 | 51.67 |
| layer-cpp-highspy-tabu | 14.19 | 5.65 | 5.20 | 50.75 |
| layer-purepython-gurobipy-exact | 2.95 | 1.91 | 2.31 | 33.76 |
| layer-purepython-gurobipy-tabu | 18.13 | 7.61 | 5.87 | 62.57 |
| layer-cpp-gurobipy-tabu | 18.12 | 7.42 | 5.74 | 62.37 |

## By configuration (mean solve s)

| config | C101 | R101 | RC101 | C201 |
|---|---|---|---|---|
| layer-cpp-highspy-exact | 1.11 | 0.52 | 1.00 | 32.74 |
| layer-cpp-gurobipy-exact | 1.75 | 0.68 | 1.11 | 32.32 |
| layer-purepython-highspy-exact | 1.21 | 0.60 | 1.11 | 33.27 |
| layer-purepython-highspy-tabu | 13.59 | 4.74 | 4.24 | 50.52 |
| layer-cpp-highspy-tabu | 13.08 | 4.58 | 4.07 | 49.64 |
| layer-purepython-gurobipy-exact | 1.79 | 0.73 | 1.14 | 32.55 |
| layer-purepython-gurobipy-tabu | 17.00 | 6.36 | 4.72 | 61.28 |
| layer-cpp-gurobipy-tabu | 16.95 | 6.30 | 4.61 | 61.10 |

## LP mismatches

none
