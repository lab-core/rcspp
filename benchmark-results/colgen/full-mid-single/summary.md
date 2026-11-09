# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101

Baseline LP (current C++ example): 828.94

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 828.9369 | 72 | 3699 | 2.72 | 1.55 | 0.38 | 1.17 | 0.00 | 80 |
| layer-purepython-highspy-exact | 0 | Optimal | 828.9369 | 72 | 3699 | 2.80 | 1.65 | 0.39 | 1.22 | 0.04 | 80 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 828.9369 | 95 | 3915 | 3.14 | 2.11 | 0.55 | 1.55 | 0.01 | 79 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 828.9369 | 95 | 3915 | 3.43 | 2.24 | 0.56 | 1.63 | 0.04 | 79 |
| layer-cpp-highspy-tabu | 0 | Optimal | 828.9369 | 250 | 2119 | 13.23 | 12.27 | 0.45 | 11.82 | 0.00 | 78 |
| layer-purepython-highspy-tabu | 0 | Optimal | 828.9369 | 250 | 2119 | 13.77 | 12.70 | 0.48 | 12.17 | 0.05 | 79 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 828.9369 | 309 | 2494 | 18.94 | 17.72 | 1.67 | 16.01 | 0.03 | 82 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 828.9369 | 309 | 2494 | 18.88 | 17.65 | 1.42 | 16.16 | 0.06 | 79 |
| layer-cpp-highspy-exact | 1 | Optimal | 828.9369 | 72 | 3699 | 2.59 | 1.53 | 0.38 | 1.15 | 0.00 | 80 |
| layer-purepython-highspy-exact | 1 | Optimal | 828.9369 | 72 | 3699 | 2.71 | 1.59 | 0.38 | 1.17 | 0.03 | 80 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 828.9369 | 95 | 3915 | 3.20 | 2.17 | 0.55 | 1.60 | 0.01 | 79 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 828.9369 | 95 | 3915 | 3.35 | 2.22 | 0.55 | 1.63 | 0.04 | 79 |
| layer-cpp-highspy-tabu | 1 | Optimal | 828.9369 | 250 | 2119 | 13.69 | 12.57 | 0.46 | 12.12 | 0.00 | 79 |
| layer-purepython-highspy-tabu | 1 | Optimal | 828.9369 | 250 | 2119 | 13.85 | 12.70 | 0.49 | 12.16 | 0.05 | 78 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 828.9369 | 309 | 2494 | 18.28 | 17.24 | 1.40 | 15.81 | 0.03 | 78 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 828.9369 | 309 | 2494 | 19.45 | 18.22 | 1.47 | 16.68 | 0.07 | 79 |
| layer-cpp-highspy-exact | 2 | Optimal | 828.9369 | 72 | 3699 | 2.51 | 1.47 | 0.37 | 1.10 | 0.00 | 79 |
| layer-purepython-highspy-exact | 2 | Optimal | 828.9369 | 72 | 3699 | 2.78 | 1.62 | 0.38 | 1.20 | 0.04 | 79 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 828.9369 | 95 | 3915 | 3.38 | 2.17 | 0.56 | 1.60 | 0.01 | 79 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 828.9369 | 95 | 3915 | 3.47 | 2.20 | 0.55 | 1.61 | 0.04 | 79 |
| layer-cpp-highspy-tabu | 2 | Optimal | 828.9369 | 250 | 2119 | 14.28* | 13.29* | 0.48 | 12.81 | 0.00 | 78 |
| layer-purepython-highspy-tabu | 2 | Optimal | 828.9369 | 250 | 2119 | 14.03 | 12.85 | 0.48 | 12.32 | 0.05 | 79 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 828.9369 | 309 | 2494 | 18.65 | 17.51 | 1.67 | 15.80 | 0.03 | 82 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 828.9369 | 309 | 2494 | 18.35 | 17.23 | 1.42 | 15.75 | 0.07 | 79 |

## R101

Baseline LP (current C++ example): 1636.39

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 1636.3887 | 26 | 1521 | 1.48 | 0.42 | 0.10 | 0.32 | 0.00 | 78 |
| layer-purepython-highspy-exact | 0 | Optimal | 1636.3887 | 26 | 1521 | 1.47 | 0.48 | 0.10 | 0.35 | 0.03 | 78 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 1636.3887 | 30 | 1512 | 1.63 | 0.55 | 0.14 | 0.40 | 0.00 | 77 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 1636.3887 | 30 | 1512 | 1.68 | 0.60 | 0.15 | 0.42 | 0.03 | 77 |
| layer-cpp-highspy-tabu | 0 | Optimal | 1636.3887 | 153 | 1188 | 5.95 | 4.94 | 0.26 | 4.69 | 0.00 | 77 |
| layer-purepython-highspy-tabu | 0 | Optimal | 1636.3887 | 153 | 1188 | 6.13 | 5.00 | 0.27 | 4.69 | 0.04 | 77 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 1636.3887 | 168 | 1295 | 7.00 | 5.81 | 0.72 | 5.08 | 0.02 | 76 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 1636.3887 | 168 | 1295 | 7.28 | 6.09 | 0.74 | 5.30 | 0.05 | 76 |
| layer-cpp-highspy-exact | 1 | Optimal | 1636.3887 | 26 | 1521 | 1.69 | 0.49 | 0.10 | 0.39 | 0.00 | 78 |
| layer-purepython-highspy-exact | 1 | Optimal | 1636.3887 | 26 | 1521 | 1.67 | 0.47 | 0.10 | 0.35 | 0.03 | 78 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 1636.3887 | 30 | 1512 | 1.74 | 0.57 | 0.15 | 0.42 | 0.00 | 77 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 1636.3887 | 30 | 1512 | 1.67 | 0.59 | 0.15 | 0.41 | 0.03 | 77 |
| layer-cpp-highspy-tabu | 1 | Optimal | 1636.3887 | 153 | 1188 | 6.18 | 5.06 | 0.26 | 4.80 | 0.00 | 77 |
| layer-purepython-highspy-tabu | 1 | Optimal | 1636.3887 | 153 | 1188 | 6.37 | 5.20 | 0.27 | 4.88 | 0.04 | 77 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 1636.3887 | 168 | 1295 | 6.97 | 5.80 | 0.73 | 5.05 | 0.02 | 76 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 1636.3887 | 168 | 1295 | 7.26 | 6.00 | 0.72 | 5.23 | 0.05 | 77 |
| layer-cpp-highspy-exact | 2 | Optimal | 1636.3887 | 26 | 1521 | 1.50 | 0.42 | 0.10 | 0.32 | 0.00 | 78 |
| layer-purepython-highspy-exact | 2 | Optimal | 1636.3887 | 26 | 1521 | 1.67 | 0.47 | 0.10 | 0.34 | 0.03 | 78 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 1636.3887 | 30 | 1512 | 1.67 | 0.53 | 0.14 | 0.39 | 0.00 | 77 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 1636.3887 | 30 | 1512 | 1.77* | 0.57* | 0.14 | 0.39 | 0.03 | 77 |
| layer-cpp-highspy-tabu | 2 | Optimal | 1636.3887 | 153 | 1188 | 6.15 | 5.09 | 0.25 | 4.83 | 0.00 | 77 |
| layer-purepython-highspy-tabu | 2 | Optimal | 1636.3887 | 153 | 1188 | 6.36 | 5.25 | 0.28 | 4.93 | 0.04 | 77 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 1636.3887 | 168 | 1295 | 7.01 | 5.98 | 0.73 | 5.23 | 0.02 | 76 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 1636.3887 | 168 | 1295 | 7.21 | 6.01 | 0.72 | 5.24 | 0.05 | 77 |

## RC101

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 1572.7390 | 33 | 2513 | 1.88 | 0.91 | 0.18 | 0.73 | 0.00 | 79 |
| layer-purepython-highspy-exact | 0 | Optimal | 1572.7390 | 33 | 2513 | 2.10 | 0.96 | 0.19 | 0.74 | 0.03 | 79 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 1572.7390 | 36 | 2320 | 2.15 | 1.03 | 0.21 | 0.81 | 0.01 | 78 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 1572.7390 | 36 | 2320 | 2.09 | 1.07 | 0.21 | 0.83 | 0.03 | 78 |
| layer-cpp-highspy-tabu | 0 | Optimal | 1572.7390 | 56 | 1628 | 4.49 | 3.43 | 0.16 | 3.26 | 0.00 | 78 |
| layer-purepython-highspy-tabu | 0 | Optimal | 1572.7390 | 56 | 1628 | 4.88 | 3.72 | 0.17 | 3.52 | 0.03 | 78 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 1572.7390 | 57 | 1769 | 5.07 | 3.85 | 0.29 | 3.55 | 0.01 | 77 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 1572.7390 | 57 | 1769 | 4.81 | 3.58 | 0.28 | 3.27 | 0.03 | 77 |
| layer-cpp-highspy-exact | 1 | Optimal | 1572.7390 | 33 | 2513 | 1.88 | 0.87 | 0.18 | 0.69 | 0.00 | 79 |
| layer-purepython-highspy-exact | 1 | Optimal | 1572.7390 | 33 | 2513 | 2.09 | 0.97 | 0.19 | 0.75 | 0.03 | 79 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 1572.7390 | 36 | 2320 | 2.15* | 1.05* | 0.21 | 0.83 | 0.01 | 78 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 1572.7390 | 36 | 2320 | 2.09 | 1.03 | 0.20 | 0.79 | 0.03 | 78 |
| layer-cpp-highspy-tabu | 1 | Optimal | 1572.7390 | 56 | 1628 | 4.69 | 3.53 | 0.17 | 3.36 | 0.00 | 78 |
| layer-purepython-highspy-tabu | 1 | Optimal | 1572.7390 | 56 | 1628 | 4.88 | 3.68 | 0.17 | 3.48 | 0.03 | 77 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 1572.7390 | 57 | 1769 | 4.86 | 3.66 | 0.28 | 3.37 | 0.01 | 77 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 1572.7390 | 57 | 1769 | 4.81 | 3.65 | 0.28 | 3.34 | 0.03 | 77 |
| layer-cpp-highspy-exact | 2 | Optimal | 1572.7390 | 33 | 2513 | 1.94 | 0.91 | 0.18 | 0.72 | 0.00 | 79 |
| layer-purepython-highspy-exact | 2 | Optimal | 1572.7390 | 33 | 2513 | 2.10 | 0.96 | 0.19 | 0.74 | 0.03 | 79 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 1572.7390 | 36 | 2320 | 2.09 | 1.01 | 0.21 | 0.80 | 0.00 | 78 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 1572.7390 | 36 | 2320 | 2.37 | 1.10 | 0.21 | 0.85 | 0.04 | 78 |
| layer-cpp-highspy-tabu | 2 | Optimal | 1572.7390 | 56 | 1628 | 4.67 | 3.61 | 0.17 | 3.45 | 0.00 | 77 |
| layer-purepython-highspy-tabu | 2 | Optimal | 1572.7390 | 56 | 1628 | 4.64 | 3.52 | 0.17 | 3.31 | 0.03 | 77 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 1572.7390 | 57 | 1769 | 4.77 | 3.56 | 0.28 | 3.28 | 0.01 | 77 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 1572.7390 | 57 | 1769 | 4.91 | 3.67 | 0.28 | 3.35 | 0.03 | 77 |

## C201

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 591.5566 | 190 | 37974 | 37.82 | 36.62 | 26.82 | 9.80 | 0.00 | 146 |
| layer-purepython-highspy-exact | 0 | Optimal | 591.5566 | 190 | 37974 | 38.96 | 37.77 | 27.21 | 10.51 | 0.06 | 157 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 591.5566 | 235 | 45755 | 25.95 | 24.74 | 12.10 | 12.59 | 0.05 | 167 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 591.5566 | 235 | 45755 | 26.20 | 24.95 | 12.12 | 12.75 | 0.08 | 183 |
| layer-cpp-highspy-tabu | 0 | Optimal | 591.5566 | 227 | 27528 | 49.58 | 48.53 | 14.36 | 34.16 | 0.00 | 125 |
| layer-purepython-highspy-tabu | 0 | Optimal | 591.5566 | 227 | 27528 | 51.37 | 50.14 | 14.77 | 35.30 | 0.06 | 131 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 591.5566 | 303 | 33316 | 52.83 | 51.47 | 8.95 | 42.47 | 0.05 | 146 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 591.5566 | 303 | 33316 | 51.81 | 50.59 | 8.79 | 41.71 | 0.09 | 157 |
| layer-cpp-highspy-exact | 1 | Optimal | 591.5566 | 190 | 37974 | 38.24 | 37.08 | 27.13 | 9.95 | 0.00 | 147 |
| layer-purepython-highspy-exact | 1 | Optimal | 591.5566 | 190 | 37974 | 39.22 | 38.11 | 27.52 | 10.53 | 0.06 | 158 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 591.5566 | 235 | 45755 | 26.68 | 25.21 | 12.33 | 12.83 | 0.05 | 169 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 591.5566 | 235 | 45755 | 26.58 | 25.28 | 12.23 | 12.96 | 0.09 | 183 |
| layer-cpp-highspy-tabu | 1 | Optimal | 591.5566 | 227 | 27528 | 49.25 | 48.23 | 14.27 | 33.96 | 0.00 | 125 |
| layer-purepython-highspy-tabu | 1 | Optimal | 591.5566 | 227 | 27528 | 50.09 | 48.92 | 14.56 | 34.30 | 0.06 | 131 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 591.5566 | 303 | 33316 | 51.80 | 50.72 | 8.88 | 41.79 | 0.05 | 143 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 591.5566 | 303 | 33316 | 52.69 | 51.59 | 8.93 | 42.58 | 0.09 | 155 |
| layer-cpp-highspy-exact | 2 | Optimal | 591.5566 | 190 | 37974 | 37.98 | 36.82 | 26.99 | 9.83 | 0.00 | 146 |
| layer-purepython-highspy-exact | 2 | Optimal | 591.5566 | 190 | 37974 | 38.51 | 37.37 | 27.00 | 10.30 | 0.06 | 157 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 591.5566 | 235 | 45755 | 26.26 | 25.05 | 12.23 | 12.77 | 0.05 | 166 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 591.5566 | 235 | 45755 | 26.54 | 25.25 | 12.26 | 12.90 | 0.09 | 182 |
| layer-cpp-highspy-tabu | 2 | Optimal | 591.5566 | 227 | 27528 | 49.71 | 48.66 | 14.43 | 34.22 | 0.00 | 126 |
| layer-purepython-highspy-tabu | 2 | Optimal | 591.5566 | 227 | 27528 | 49.94 | 48.90 | 14.54 | 34.29 | 0.07 | 131 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 591.5566 | 303 | 33316 | 52.90 | 51.49 | 8.92 | 42.52 | 0.05 | 147 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 591.5566 | 303 | 33316 | 51.85 | 50.69 | 8.88 | 41.72 | 0.09 | 158 |

## By configuration (mean wall s)

| config | C101 | R101 | RC101 | C201 |
|---|---|---|---|---|
| layer-cpp-highspy-exact | 2.60 | 1.56 | 1.90 | 38.01 |
| layer-cpp-gurobipy-exact | 3.24 | 1.68 | 2.13 | 26.30 |
| layer-purepython-highspy-exact | 2.76 | 1.61 | 2.09 | 38.90 |
| layer-purepython-highspy-tabu | 13.89 | 6.29 | 4.80 | 50.47 |
| layer-cpp-highspy-tabu | 13.73 | 6.09 | 4.61 | 49.51 |
| layer-purepython-gurobipy-exact | 3.42 | 1.71 | 2.18 | 26.44 |
| layer-purepython-gurobipy-tabu | 18.89 | 7.25 | 4.84 | 52.11 |
| layer-cpp-gurobipy-tabu | 18.62 | 6.99 | 4.90 | 52.51 |

## By configuration (mean solve s)

| config | C101 | R101 | RC101 | C201 |
|---|---|---|---|---|
| layer-cpp-highspy-exact | 1.52 | 0.44 | 0.89 | 36.84 |
| layer-cpp-gurobipy-exact | 2.15 | 0.55 | 1.03 | 25.00 |
| layer-purepython-highspy-exact | 1.62 | 0.47 | 0.96 | 37.75 |
| layer-purepython-highspy-tabu | 12.75 | 5.15 | 3.64 | 49.32 |
| layer-cpp-highspy-tabu | 12.71 | 5.03 | 3.52 | 48.47 |
| layer-purepython-gurobipy-exact | 2.22 | 0.58 | 1.07 | 25.16 |
| layer-purepython-gurobipy-tabu | 17.70 | 6.03 | 3.63 | 50.95 |
| layer-cpp-gurobipy-tabu | 17.49 | 5.86 | 3.69 | 51.23 |

## LP mismatches

none
