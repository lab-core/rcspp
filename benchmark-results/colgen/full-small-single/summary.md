# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 191.8136 | 18 | 666 | 1.28 | 0.06 | 0.02 | 0.04 | 0.00 | 55 |
| layer-purepython-highspy-exact | 0 | Optimal | 191.8136 | 18 | 666 | 1.72 | 0.08 | 0.02 | 0.05 | 0.00 | 65 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 191.8136 | 29 | 723 | 1.27 | 0.18 | 0.12 | 0.06 | 0.00 | 64 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 191.8136 | 29 | 723 | 1.36 | 0.19 | 0.12 | 0.07 | 0.01 | 64 |
| layer-cpp-highspy-tabu | 0 | Optimal | 191.8136 | 66 | 412 | 1.89 | 0.94 | 0.04 | 0.90 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 0 | Optimal | 191.8136 | 66 | 412 | 2.11 | 1.03 | 0.04 | 0.98 | 0.01 | 65 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 191.8136 | 64 | 527 | 2.69* | 1.36* | 0.26 | 1.09 | 0.01 | 63 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 191.8136 | 64 | 527 | 2.53 | 1.26 | 0.25 | 1.00 | 0.01 | 64 |
| layer-cpp-highspy-exact | 1 | Optimal | 191.8136 | 18 | 666 | 1.06 | 0.05 | 0.02 | 0.03 | 0.00 | 64 |
| layer-purepython-highspy-exact | 1 | Optimal | 191.8136 | 18 | 666 | 1.06 | 0.06 | 0.02 | 0.04 | 0.00 | 57 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 191.8136 | 29 | 723 | 1.42* | 0.19* | 0.12 | 0.07 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 191.8136 | 29 | 723 | 1.27 | 0.18 | 0.11 | 0.06 | 0.00 | 65 |
| layer-cpp-highspy-tabu | 1 | Optimal | 191.8136 | 66 | 412 | 1.90 | 0.96 | 0.04 | 0.92 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 1 | Optimal | 191.8136 | 66 | 412 | 2.22 | 1.01 | 0.04 | 0.96 | 0.01 | 65 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 191.8136 | 64 | 527 | 2.33 | 1.23 | 0.25 | 0.98 | 0.01 | 63 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 191.8136 | 64 | 527 | 2.33 | 1.26 | 0.25 | 1.00 | 0.01 | 64 |
| layer-cpp-highspy-exact | 2 | Optimal | 191.8136 | 18 | 666 | 1.06 | 0.05 | 0.02 | 0.03 | 0.00 | 64 |
| layer-purepython-highspy-exact | 2 | Optimal | 191.8136 | 18 | 666 | 1.06 | 0.06 | 0.02 | 0.04 | 0.00 | 54 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 191.8136 | 29 | 723 | 1.27 | 0.19 | 0.12 | 0.06 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 191.8136 | 29 | 723 | 1.34 | 0.19 | 0.12 | 0.07 | 0.01 | 65 |
| layer-cpp-highspy-tabu | 2 | Optimal | 191.8136 | 66 | 412 | 2.01* | 0.97* | 0.04 | 0.94 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 2 | Optimal | 191.8136 | 66 | 412 | 2.11 | 1.00 | 0.04 | 0.95 | 0.01 | 65 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 191.8136 | 64 | 527 | 2.43* | 1.28* | 0.25 | 1.02 | 0.01 | 63 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 191.8136 | 64 | 527 | 2.33 | 1.30 | 0.26 | 1.04 | 0.01 | 64 |
| layer-cpp-highspy-exact | 3 | Optimal | 191.8136 | 18 | 666 | 1.06 | 0.05 | 0.02 | 0.03 | 0.00 | 54 |
| layer-purepython-highspy-exact | 3 | Optimal | 191.8136 | 18 | 666 | 1.27 | 0.06 | 0.02 | 0.04 | 0.00 | 65 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 191.8136 | 29 | 723 | 1.27 | 0.18 | 0.12 | 0.06 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 191.8136 | 29 | 723 | 1.33 | 0.21 | 0.12 | 0.08 | 0.01 | 64 |
| layer-cpp-highspy-tabu | 3 | Optimal | 191.8136 | 66 | 412 | 2.16* | 0.98* | 0.04 | 0.94 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 3 | Optimal | 191.8136 | 66 | 412 | 2.12 | 1.06 | 0.05 | 1.01 | 0.01 | 65 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 191.8136 | 64 | 527 | 2.40* | 1.26* | 0.25 | 1.00 | 0.01 | 63 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 191.8136 | 64 | 527 | 2.54 | 1.29 | 0.25 | 1.02 | 0.01 | 64 |
| layer-cpp-highspy-exact | 4 | Optimal | 191.8136 | 18 | 666 | 1.06 | 0.05 | 0.02 | 0.03 | 0.00 | 63 |
| layer-purepython-highspy-exact | 4 | Optimal | 191.8136 | 18 | 666 | 1.27 | 0.06 | 0.02 | 0.04 | 0.00 | 65 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 191.8136 | 29 | 723 | 1.27 | 0.18 | 0.12 | 0.06 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 191.8136 | 29 | 723 | 1.27 | 0.19 | 0.11 | 0.07 | 0.00 | 64 |
| layer-cpp-highspy-tabu | 4 | Optimal | 191.8136 | 66 | 412 | 2.01* | 0.98* | 0.04 | 0.94 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 4 | Optimal | 191.8136 | 66 | 412 | 2.13 | 1.08 | 0.05 | 1.03 | 0.01 | 65 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 191.8136 | 64 | 527 | 2.44 | 1.37 | 0.26 | 1.11 | 0.01 | 63 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 191.8136 | 64 | 527 | 2.54 | 1.29 | 0.25 | 1.03 | 0.01 | 64 |

## R101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 618.3299 | 7 | 99 | 1.28 | 0.02 | 0.01 | 0.01 | 0.00 | 56 |
| layer-purepython-highspy-exact | 0 | Optimal | 618.3299 | 7 | 99 | 1.16 | 0.02 | 0.00 | 0.01 | 0.00 | 58 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 618.3299 | 6 | 90 | 1.06 | 0.03 | 0.02 | 0.01 | 0.00 | 54 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 618.3299 | 6 | 90 | 1.28 | 0.03 | 0.02 | 0.01 | 0.00 | 63 |
| layer-cpp-highspy-tabu | 0 | Optimal | 618.3299 | 7 | 96 | 1.06 | 0.03 | 0.00 | 0.03 | 0.00 | 63 |
| layer-purepython-highspy-tabu | 0 | Optimal | 618.3299 | 7 | 96 | 1.06 | 0.04 | 0.00 | 0.03 | 0.00 | 64 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 618.3299 | 9 | 95 | 1.47 | 0.11 | 0.04 | 0.07 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 618.3299 | 9 | 95 | 1.30* | 0.10* | 0.04 | 0.06 | 0.00 | 64 |
| layer-cpp-highspy-exact | 1 | Optimal | 618.3299 | 7 | 99 | 1.18 | 0.01 | 0.00 | 0.01 | 0.00 | 63 |
| layer-purepython-highspy-exact | 1 | Optimal | 618.3299 | 7 | 99 | 1.06 | 0.02 | 0.01 | 0.01 | 0.00 | 55 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 618.3299 | 6 | 90 | 1.06 | 0.03 | 0.02 | 0.01 | 0.00 | 62 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 618.3299 | 6 | 90 | 1.06 | 0.04 | 0.02 | 0.01 | 0.00 | 54 |
| layer-cpp-highspy-tabu | 1 | Optimal | 618.3299 | 7 | 96 | 1.06 | 0.03 | 0.00 | 0.03 | 0.00 | 63 |
| layer-purepython-highspy-tabu | 1 | Optimal | 618.3299 | 7 | 96 | 1.13 | 0.04 | 0.00 | 0.03 | 0.00 | 55 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 618.3299 | 9 | 95 | 1.19* | 0.09* | 0.03 | 0.06 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 618.3299 | 9 | 95 | 1.51 | 0.09 | 0.03 | 0.06 | 0.00 | 57 |
| layer-cpp-highspy-exact | 2 | Optimal | 618.3299 | 7 | 99 | 1.06 | 0.01 | 0.00 | 0.01 | 0.00 | 63 |
| layer-purepython-highspy-exact | 2 | Optimal | 618.3299 | 7 | 99 | 1.09 | 0.02 | 0.00 | 0.01 | 0.00 | 53 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 618.3299 | 6 | 90 | 1.24* | 0.03* | 0.02 | 0.01 | 0.00 | 56 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 618.3299 | 6 | 90 | 1.06 | 0.03 | 0.02 | 0.01 | 0.00 | 54 |
| layer-cpp-highspy-tabu | 2 | Optimal | 618.3299 | 7 | 96 | 1.06 | 0.03 | 0.00 | 0.03 | 0.00 | 62 |
| layer-purepython-highspy-tabu | 2 | Optimal | 618.3299 | 7 | 96 | 1.06 | 0.04 | 0.00 | 0.03 | 0.00 | 54 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 618.3299 | 9 | 95 | 1.11 | 0.09 | 0.03 | 0.06 | 0.00 | 57 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 618.3299 | 9 | 95 | 1.20* | 0.11* | 0.04 | 0.07 | 0.00 | 63 |
| layer-cpp-highspy-exact | 3 | Optimal | 618.3299 | 7 | 99 | 1.06 | 0.01 | 0.00 | 0.01 | 0.00 | 58 |
| layer-purepython-highspy-exact | 3 | Optimal | 618.3299 | 7 | 99 | 1.17 | 0.02 | 0.00 | 0.01 | 0.00 | 59 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 618.3299 | 6 | 90 | 1.06 | 0.03 | 0.02 | 0.01 | 0.00 | 54 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 618.3299 | 6 | 90 | 1.28 | 0.04 | 0.02 | 0.01 | 0.00 | 54 |
| layer-cpp-highspy-tabu | 3 | Optimal | 618.3299 | 7 | 96 | 1.06 | 0.03 | 0.00 | 0.03 | 0.00 | 54 |
| layer-purepython-highspy-tabu | 3 | Optimal | 618.3299 | 7 | 96 | 1.06 | 0.04 | 0.01 | 0.03 | 0.00 | 54 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 618.3299 | 9 | 95 | 1.18 | 0.09 | 0.03 | 0.05 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 618.3299 | 9 | 95 | 1.27 | 0.09 | 0.03 | 0.05 | 0.00 | 64 |
| layer-cpp-highspy-exact | 4 | Optimal | 618.3299 | 7 | 99 | 1.07 | 0.01 | 0.00 | 0.01 | 0.00 | 53 |
| layer-purepython-highspy-exact | 4 | Optimal | 618.3299 | 7 | 99 | 1.06 | 0.02 | 0.00 | 0.01 | 0.00 | 54 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 618.3299 | 6 | 90 | 1.21 | 0.04 | 0.02 | 0.01 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 618.3299 | 6 | 90 | 1.27 | 0.03 | 0.02 | 0.01 | 0.00 | 64 |
| layer-cpp-highspy-tabu | 4 | Optimal | 618.3299 | 7 | 96 | 1.08 | 0.04 | 0.00 | 0.03 | 0.00 | 53 |
| layer-purepython-highspy-tabu | 4 | Optimal | 618.3299 | 7 | 96 | 1.27 | 0.04 | 0.00 | 0.03 | 0.00 | 64 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 618.3299 | 9 | 95 | 1.27 | 0.09 | 0.03 | 0.05 | 0.00 | 56 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 618.3299 | 9 | 95 | 1.39* | 0.10* | 0.03 | 0.06 | 0.00 | 64 |

## R201_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 449.6598 | 12 | 3024 | 3.56 | 0.91 | 0.07 | 0.84 | 0.00 | 71 |
| layer-purepython-highspy-exact | 0 | Optimal | 449.6598 | 12 | 3024 | 1.84* | 0.55* | 0.06 | 0.48 | 0.00 | 72 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 449.6598 | 11 | 3247 | 1.72 | 0.55 | 0.07 | 0.49 | 0.00 | 70 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 449.6598 | 11 | 3247 | 1.49 | 0.56 | 0.06 | 0.50 | 0.00 | 71 |
| layer-cpp-highspy-tabu | 0 | Optimal | 449.6598 | 22 | 947 | 1.33 | 0.33 | 0.02 | 0.31 | 0.00 | 65 |
| layer-purepython-highspy-tabu | 0 | Optimal | 449.6598 | 22 | 947 | 1.41* | 0.35* | 0.02 | 0.32 | 0.00 | 66 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 449.6598 | 22 | 831 | 1.49 | 0.42 | 0.09 | 0.33 | 0.00 | 64 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 449.6598 | 22 | 831 | 1.48 | 0.40 | 0.08 | 0.31 | 0.00 | 65 |
| layer-cpp-highspy-exact | 1 | Optimal | 449.6598 | 12 | 3024 | 1.49 | 0.49 | 0.06 | 0.43 | 0.00 | 71 |
| layer-purepython-highspy-exact | 1 | Optimal | 449.6598 | 12 | 3024 | 1.82 | 0.59 | 0.07 | 0.52 | 0.00 | 71 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 449.6598 | 11 | 3247 | 1.70 | 0.57 | 0.06 | 0.51 | 0.00 | 70 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 449.6598 | 11 | 3247 | 1.70 | 0.56 | 0.06 | 0.50 | 0.00 | 71 |
| layer-cpp-highspy-tabu | 1 | Optimal | 449.6598 | 22 | 947 | 1.49 | 0.35 | 0.02 | 0.33 | 0.00 | 65 |
| layer-purepython-highspy-tabu | 1 | Optimal | 449.6598 | 22 | 947 | 1.64* | 0.42* | 0.03 | 0.38 | 0.00 | 66 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 449.6598 | 22 | 831 | 1.48 | 0.42 | 0.09 | 0.33 | 0.00 | 64 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 449.6598 | 22 | 831 | 1.48 | 0.42 | 0.09 | 0.33 | 0.00 | 65 |
| layer-cpp-highspy-exact | 2 | Optimal | 449.6598 | 12 | 3024 | 1.48 | 0.48 | 0.06 | 0.42 | 0.00 | 70 |
| layer-purepython-highspy-exact | 2 | Optimal | 449.6598 | 12 | 3024 | 1.69 | 0.51 | 0.06 | 0.45 | 0.00 | 71 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 449.6598 | 11 | 3247 | 1.59 | 0.59 | 0.07 | 0.52 | 0.00 | 70 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 449.6598 | 11 | 3247 | 1.69 | 0.56 | 0.06 | 0.49 | 0.00 | 71 |
| layer-cpp-highspy-tabu | 2 | Optimal | 449.6598 | 22 | 947 | 1.27 | 0.35 | 0.02 | 0.33 | 0.00 | 65 |
| layer-purepython-highspy-tabu | 2 | Optimal | 449.6598 | 22 | 947 | 1.48 | 0.37 | 0.03 | 0.34 | 0.00 | 66 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 449.6598 | 22 | 831 | 1.58* | 0.46* | 0.09 | 0.36 | 0.00 | 64 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 449.6598 | 22 | 831 | 1.48 | 0.43 | 0.09 | 0.33 | 0.00 | 65 |
| layer-cpp-highspy-exact | 3 | Optimal | 449.6598 | 12 | 3024 | 1.71 | 0.50 | 0.06 | 0.44 | 0.00 | 71 |
| layer-purepython-highspy-exact | 3 | Optimal | 449.6598 | 12 | 3024 | 1.69 | 0.50 | 0.06 | 0.44 | 0.00 | 71 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 449.6598 | 11 | 3247 | 1.76* | 0.58* | 0.07 | 0.51 | 0.00 | 70 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 449.6598 | 11 | 3247 | 1.69 | 0.55 | 0.06 | 0.49 | 0.00 | 71 |
| layer-cpp-highspy-tabu | 3 | Optimal | 449.6598 | 22 | 947 | 1.27 | 0.34 | 0.02 | 0.32 | 0.00 | 65 |
| layer-purepython-highspy-tabu | 3 | Optimal | 449.6598 | 22 | 947 | 1.52 | 0.36 | 0.03 | 0.33 | 0.00 | 66 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 449.6598 | 22 | 831 | 1.61* | 0.42* | 0.09 | 0.33 | 0.00 | 64 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 449.6598 | 22 | 831 | 1.48 | 0.42 | 0.09 | 0.33 | 0.00 | 65 |
| layer-cpp-highspy-exact | 4 | Optimal | 449.6598 | 12 | 3024 | 1.48 | 0.49 | 0.06 | 0.43 | 0.00 | 70 |
| layer-purepython-highspy-exact | 4 | Optimal | 449.6598 | 12 | 3024 | 1.69 | 0.52 | 0.06 | 0.45 | 0.00 | 71 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 449.6598 | 11 | 3247 | 1.80 | 0.59 | 0.07 | 0.52 | 0.00 | 70 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 449.6598 | 11 | 3247 | 1.70 | 0.57 | 0.06 | 0.50 | 0.01 | 72 |
| layer-cpp-highspy-tabu | 4 | Optimal | 449.6598 | 22 | 947 | 1.48 | 0.35 | 0.02 | 0.32 | 0.00 | 65 |
| layer-purepython-highspy-tabu | 4 | Optimal | 449.6598 | 22 | 947 | 1.48 | 0.37 | 0.03 | 0.34 | 0.00 | 66 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 449.6598 | 22 | 831 | 1.58* | 0.44* | 0.09 | 0.35 | 0.00 | 64 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 449.6598 | 22 | 831 | 1.48 | 0.43 | 0.09 | 0.33 | 0.00 | 65 |

## RC201_12

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 427.2356 | 16 | 1050 | 1.09 | 0.08 | 0.02 | 0.06 | 0.00 | 65 |
| layer-purepython-highspy-exact | 0 | Optimal | 427.2356 | 16 | 1050 | 1.19* | 0.10* | 0.02 | 0.07 | 0.00 | 66 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 427.2356 | 16 | 1046 | 1.06 | 0.14 | 0.06 | 0.08 | 0.00 | 64 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 427.2356 | 16 | 1046 | 1.27 | 0.15 | 0.06 | 0.08 | 0.00 | 65 |
| layer-cpp-highspy-tabu | 0 | Optimal | 427.2356 | 22 | 483 | 1.28 | 0.32 | 0.02 | 0.30 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 0 | Optimal | 427.2356 | 22 | 483 | 1.63* | 0.36* | 0.02 | 0.33 | 0.00 | 65 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 427.2356 | 22 | 392 | 1.50 | 0.34 | 0.08 | 0.26 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 427.2356 | 22 | 392 | 1.71 | 0.41 | 0.09 | 0.31 | 0.01 | 64 |
| layer-cpp-highspy-exact | 1 | Optimal | 427.2356 | 16 | 1050 | 1.07 | 0.09 | 0.02 | 0.07 | 0.00 | 63 |
| layer-purepython-highspy-exact | 1 | Optimal | 427.2356 | 16 | 1050 | 1.36* | 0.11* | 0.03 | 0.08 | 0.00 | 66 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 427.2356 | 16 | 1046 | 1.28 | 0.15 | 0.07 | 0.08 | 0.00 | 64 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 427.2356 | 16 | 1046 | 1.27 | 0.15 | 0.06 | 0.08 | 0.00 | 65 |
| layer-cpp-highspy-tabu | 1 | Optimal | 427.2356 | 22 | 483 | 1.28 | 0.29 | 0.02 | 0.27 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 1 | Optimal | 427.2356 | 22 | 483 | 1.33 | 0.32 | 0.02 | 0.29 | 0.00 | 65 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 427.2356 | 22 | 392 | 1.39* | 0.35* | 0.08 | 0.27 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 427.2356 | 22 | 392 | 1.48 | 0.37 | 0.08 | 0.29 | 0.00 | 64 |
| layer-cpp-highspy-exact | 2 | Optimal | 427.2356 | 16 | 1050 | 1.06 | 0.10 | 0.02 | 0.07 | 0.00 | 64 |
| layer-purepython-highspy-exact | 2 | Optimal | 427.2356 | 16 | 1050 | 1.06 | 0.11 | 0.03 | 0.08 | 0.00 | 59 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 427.2356 | 16 | 1046 | 1.33 | 0.15 | 0.07 | 0.08 | 0.00 | 64 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 427.2356 | 16 | 1046 | 1.40* | 0.16* | 0.07 | 0.08 | 0.00 | 65 |
| layer-cpp-highspy-tabu | 2 | Optimal | 427.2356 | 22 | 483 | 1.49 | 0.28 | 0.02 | 0.26 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 2 | Optimal | 427.2356 | 22 | 483 | 1.27 | 0.30 | 0.02 | 0.28 | 0.00 | 65 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 427.2356 | 22 | 392 | 1.54 | 0.37 | 0.08 | 0.29 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 427.2356 | 22 | 392 | 1.48 | 0.36 | 0.09 | 0.27 | 0.00 | 64 |
| layer-cpp-highspy-exact | 3 | Optimal | 427.2356 | 16 | 1050 | 1.06 | 0.08 | 0.02 | 0.06 | 0.00 | 63 |
| layer-purepython-highspy-exact | 3 | Optimal | 427.2356 | 16 | 1050 | 1.06 | 0.10 | 0.03 | 0.07 | 0.00 | 57 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 427.2356 | 16 | 1046 | 1.22* | 0.15* | 0.07 | 0.08 | 0.00 | 64 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 427.2356 | 16 | 1046 | 1.27 | 0.15 | 0.07 | 0.08 | 0.00 | 65 |
| layer-cpp-highspy-tabu | 3 | Optimal | 427.2356 | 22 | 483 | 1.27 | 0.28 | 0.02 | 0.27 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 3 | Optimal | 427.2356 | 22 | 483 | 1.49 | 0.42 | 0.02 | 0.39 | 0.01 | 65 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 427.2356 | 22 | 392 | 1.43 | 0.38 | 0.09 | 0.29 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 427.2356 | 22 | 392 | 1.48 | 0.36 | 0.08 | 0.27 | 0.00 | 64 |
| layer-cpp-highspy-exact | 4 | Optimal | 427.2356 | 16 | 1050 | 1.28 | 0.09 | 0.02 | 0.07 | 0.00 | 53 |
| layer-purepython-highspy-exact | 4 | Optimal | 427.2356 | 16 | 1050 | 1.27 | 0.11 | 0.03 | 0.08 | 0.00 | 66 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 427.2356 | 16 | 1046 | 1.40 | 0.17 | 0.07 | 0.09 | 0.00 | 64 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 427.2356 | 16 | 1046 | 1.27 | 0.16 | 0.07 | 0.09 | 0.00 | 65 |
| layer-cpp-highspy-tabu | 4 | Optimal | 427.2356 | 22 | 483 | 1.49 | 0.38 | 0.02 | 0.36 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 4 | Optimal | 427.2356 | 22 | 483 | 1.48 | 0.32 | 0.02 | 0.30 | 0.00 | 65 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 427.2356 | 22 | 392 | 1.35 | 0.36 | 0.08 | 0.28 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 427.2356 | 22 | 392 | 1.48 | 0.36 | 0.08 | 0.27 | 0.00 | 64 |

## C101_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 363.2468 | 54 | 1749 | 1.75 | 0.43 | 0.12 | 0.30 | 0.00 | 69 |
| layer-purepython-highspy-exact | 0 | Optimal | 363.2468 | 54 | 1749 | 1.63* | 0.45* | 0.13 | 0.31 | 0.01 | 70 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 363.2468 | 43 | 1702 | 1.49 | 0.44 | 0.19 | 0.24 | 0.00 | 68 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 363.2468 | 43 | 1702 | 1.93 | 0.59 | 0.24 | 0.34 | 0.02 | 69 |
| layer-cpp-highspy-tabu | 0 | Optimal | 363.2468 | 67 | 675 | 3.38* | 1.88* | 0.06 | 1.81 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 0 | Optimal | 363.2468 | 67 | 675 | 2.76 | 1.69 | 0.07 | 1.60 | 0.02 | 69 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 363.2468 | 111 | 815 | 4.56* | 3.45* | 0.46 | 2.98 | 0.01 | 67 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 363.2468 | 111 | 815 | 4.58* | 3.46* | 0.45 | 2.99 | 0.02 | 68 |
| layer-cpp-highspy-exact | 1 | Optimal | 363.2468 | 54 | 1749 | 1.49 | 0.35 | 0.11 | 0.24 | 0.00 | 69 |
| layer-purepython-highspy-exact | 1 | Optimal | 363.2468 | 54 | 1749 | 1.62* | 0.45* | 0.13 | 0.31 | 0.01 | 70 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 363.2468 | 43 | 1702 | 1.49 | 0.46 | 0.20 | 0.26 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 363.2468 | 43 | 1702 | 1.70 | 0.48 | 0.20 | 0.27 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 1 | Optimal | 363.2468 | 67 | 675 | 2.61* | 1.59* | 0.06 | 1.53 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 1 | Optimal | 363.2468 | 67 | 675 | 2.75 | 1.71 | 0.07 | 1.63 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 363.2468 | 111 | 815 | 4.52* | 3.40* | 0.45 | 2.94 | 0.01 | 67 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 363.2468 | 111 | 815 | 5.01* | 3.80* | 0.47 | 3.30 | 0.02 | 68 |
| layer-cpp-highspy-exact | 2 | Optimal | 363.2468 | 54 | 1749 | 1.48 | 0.36 | 0.11 | 0.24 | 0.00 | 69 |
| layer-purepython-highspy-exact | 2 | Optimal | 363.2468 | 54 | 1749 | 1.48 | 0.41 | 0.12 | 0.28 | 0.01 | 70 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 363.2468 | 43 | 1702 | 1.58* | 0.46* | 0.20 | 0.26 | 0.00 | 68 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 363.2468 | 43 | 1702 | 1.69 | 0.48 | 0.20 | 0.26 | 0.01 | 69 |
| layer-cpp-highspy-tabu | 2 | Optimal | 363.2468 | 67 | 675 | 2.74 | 1.61 | 0.06 | 1.55 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 2 | Optimal | 363.2468 | 67 | 675 | 2.84* | 1.68* | 0.06 | 1.61 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 363.2468 | 111 | 815 | 4.74* | 3.59* | 0.47 | 3.11 | 0.01 | 67 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 363.2468 | 111 | 815 | 4.43 | 3.40 | 0.45 | 2.93 | 0.02 | 68 |
| layer-cpp-highspy-exact | 3 | Optimal | 363.2468 | 54 | 1749 | 1.48 | 0.37 | 0.12 | 0.25 | 0.00 | 69 |
| layer-purepython-highspy-exact | 3 | Optimal | 363.2468 | 54 | 1749 | 1.70 | 0.57 | 0.15 | 0.40 | 0.01 | 70 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 363.2468 | 43 | 1702 | 1.57* | 0.44* | 0.19 | 0.24 | 0.00 | 68 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 363.2468 | 43 | 1702 | 1.70 | 0.46 | 0.20 | 0.25 | 0.01 | 69 |
| layer-cpp-highspy-tabu | 3 | Optimal | 363.2468 | 67 | 675 | 2.54 | 1.56 | 0.06 | 1.51 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 3 | Optimal | 363.2468 | 67 | 675 | 2.80* | 1.65* | 0.07 | 1.57 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 363.2468 | 111 | 815 | 4.57* | 3.43* | 0.46 | 2.96 | 0.01 | 67 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 363.2468 | 111 | 815 | 4.67 | 3.36 | 0.45 | 2.89 | 0.02 | 68 |
| layer-cpp-highspy-exact | 4 | Optimal | 363.2468 | 54 | 1749 | 1.48 | 0.34 | 0.11 | 0.23 | 0.00 | 69 |
| layer-purepython-highspy-exact | 4 | Optimal | 363.2468 | 54 | 1749 | 1.48 | 0.40 | 0.12 | 0.27 | 0.01 | 70 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 363.2468 | 43 | 1702 | 1.57* | 0.47* | 0.20 | 0.26 | 0.01 | 68 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 363.2468 | 43 | 1702 | 1.48 | 0.46 | 0.20 | 0.25 | 0.01 | 69 |
| layer-cpp-highspy-tabu | 4 | Optimal | 363.2468 | 67 | 675 | 2.53 | 1.55 | 0.06 | 1.50 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 4 | Optimal | 363.2468 | 67 | 675 | 2.83* | 1.65* | 0.06 | 1.58 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 363.2468 | 111 | 815 | 4.96* | 3.89* | 0.78 | 3.10 | 0.01 | 70 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 363.2468 | 111 | 815 | 4.44 | 3.35 | 0.45 | 2.88 | 0.02 | 68 |

## R101_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 1046.7011 | 16 | 410 | 1.07 | 0.06 | 0.01 | 0.05 | 0.00 | 56 |
| layer-purepython-highspy-exact | 0 | Optimal | 1046.7011 | 16 | 410 | 1.07 | 0.08 | 0.01 | 0.06 | 0.01 | 57 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 1046.7011 | 14 | 363 | 1.10 | 0.11 | 0.05 | 0.05 | 0.00 | 62 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 1046.7011 | 14 | 363 | 1.46* | 0.12* | 0.05 | 0.06 | 0.01 | 67 |
| layer-cpp-highspy-tabu | 0 | Optimal | 1046.7011 | 24 | 352 | 1.27 | 0.24 | 0.02 | 0.22 | 0.00 | 67 |
| layer-purepython-highspy-tabu | 0 | Optimal | 1046.7011 | 24 | 352 | 1.28 | 0.26 | 0.02 | 0.23 | 0.01 | 68 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 1046.7011 | 32 | 335 | 1.52 | 0.48 | 0.12 | 0.36 | 0.00 | 66 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 1046.7011 | 32 | 335 | 1.67* | 0.49* | 0.12 | 0.36 | 0.01 | 67 |
| layer-cpp-highspy-exact | 1 | Optimal | 1046.7011 | 16 | 410 | 1.06 | 0.06 | 0.01 | 0.05 | 0.00 | 67 |
| layer-purepython-highspy-exact | 1 | Optimal | 1046.7011 | 16 | 410 | 1.07 | 0.08 | 0.01 | 0.06 | 0.01 | 57 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 1046.7011 | 14 | 363 | 1.27 | 0.11 | 0.06 | 0.05 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 1046.7011 | 14 | 363 | 1.32 | 0.13 | 0.05 | 0.06 | 0.01 | 67 |
| layer-cpp-highspy-tabu | 1 | Optimal | 1046.7011 | 24 | 352 | 1.28* | 0.24* | 0.02 | 0.23 | 0.00 | 67 |
| layer-purepython-highspy-tabu | 1 | Optimal | 1046.7011 | 24 | 352 | 1.27 | 0.27 | 0.02 | 0.24 | 0.01 | 68 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 1046.7011 | 32 | 335 | 1.48 | 0.48 | 0.12 | 0.35 | 0.00 | 66 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 1046.7011 | 32 | 335 | 1.77* | 0.53* | 0.13 | 0.39 | 0.01 | 67 |
| layer-cpp-highspy-exact | 2 | Optimal | 1046.7011 | 16 | 410 | 1.30* | 0.06* | 0.01 | 0.05 | 0.00 | 67 |
| layer-purepython-highspy-exact | 2 | Optimal | 1046.7011 | 16 | 410 | 1.27 | 0.08 | 0.01 | 0.06 | 0.01 | 68 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 1046.7011 | 14 | 363 | 1.27 | 0.11 | 0.05 | 0.05 | 0.00 | 66 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 1046.7011 | 14 | 363 | 1.27 | 0.12 | 0.05 | 0.06 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 2 | Optimal | 1046.7011 | 24 | 352 | 1.39 | 0.28 | 0.02 | 0.26 | 0.00 | 67 |
| layer-purepython-highspy-tabu | 2 | Optimal | 1046.7011 | 24 | 352 | 1.27 | 0.27 | 0.02 | 0.24 | 0.01 | 68 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 1046.7011 | 32 | 335 | 1.48 | 0.47 | 0.12 | 0.35 | 0.00 | 66 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 1046.7011 | 32 | 335 | 1.91 | 0.65 | 0.15 | 0.49 | 0.01 | 67 |
| layer-cpp-highspy-exact | 3 | Optimal | 1046.7011 | 16 | 410 | 1.23* | 0.07* | 0.01 | 0.05 | 0.00 | 68 |
| layer-purepython-highspy-exact | 3 | Optimal | 1046.7011 | 16 | 410 | 1.27 | 0.08 | 0.02 | 0.06 | 0.01 | 68 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 1046.7011 | 14 | 363 | 1.06 | 0.11 | 0.05 | 0.05 | 0.00 | 62 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 1046.7011 | 14 | 363 | 1.27 | 0.13 | 0.06 | 0.06 | 0.01 | 67 |
| layer-cpp-highspy-tabu | 3 | Optimal | 1046.7011 | 24 | 352 | 1.35* | 0.25* | 0.02 | 0.24 | 0.00 | 67 |
| layer-purepython-highspy-tabu | 3 | Optimal | 1046.7011 | 24 | 352 | 1.48 | 0.27 | 0.02 | 0.24 | 0.01 | 68 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 1046.7011 | 32 | 335 | 1.48 | 0.48 | 0.12 | 0.35 | 0.00 | 66 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 1046.7011 | 32 | 335 | 1.69 | 0.50 | 0.12 | 0.37 | 0.01 | 67 |
| layer-cpp-highspy-exact | 4 | Optimal | 1046.7011 | 16 | 410 | 1.17* | 0.07* | 0.01 | 0.05 | 0.00 | 67 |
| layer-purepython-highspy-exact | 4 | Optimal | 1046.7011 | 16 | 410 | 1.27 | 0.08 | 0.02 | 0.06 | 0.01 | 55 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 1046.7011 | 14 | 363 | 1.27 | 0.11 | 0.05 | 0.05 | 0.00 | 66 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 1046.7011 | 14 | 363 | 1.28 | 0.12 | 0.05 | 0.06 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 4 | Optimal | 1046.7011 | 24 | 352 | 1.33 | 0.25 | 0.02 | 0.24 | 0.00 | 67 |
| layer-purepython-highspy-tabu | 4 | Optimal | 1046.7011 | 24 | 352 | 1.30* | 0.26* | 0.02 | 0.23 | 0.01 | 68 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 1046.7011 | 32 | 335 | 1.69 | 0.51 | 0.12 | 0.38 | 0.00 | 66 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 1046.7011 | 32 | 335 | 1.48 | 0.48 | 0.12 | 0.35 | 0.01 | 67 |

## RC101_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 828.3619 | 22 | 678 | 1.28 | 0.11 | 0.02 | 0.09 | 0.00 | 58 |
| layer-purepython-highspy-exact | 0 | Optimal | 828.3619 | 22 | 678 | 1.28 | 0.14 | 0.02 | 0.11 | 0.01 | 69 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 828.3619 | 28 | 715 | 1.30 | 0.25 | 0.11 | 0.13 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 828.3619 | 28 | 715 | 1.48* | 0.26* | 0.11 | 0.14 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 0 | Optimal | 828.3619 | 38 | 612 | 2.12 | 1.18 | 0.03 | 1.15 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 0 | Optimal | 828.3619 | 38 | 612 | 2.44 | 1.22 | 0.03 | 1.18 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 828.3619 | 34 | 585 | 2.33 | 1.14 | 0.13 | 1.01 | 0.00 | 67 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 828.3619 | 34 | 585 | 2.32 | 1.18 | 0.13 | 1.03 | 0.01 | 68 |
| layer-cpp-highspy-exact | 1 | Optimal | 828.3619 | 22 | 678 | 1.06 | 0.11 | 0.02 | 0.09 | 0.00 | 68 |
| layer-purepython-highspy-exact | 1 | Optimal | 828.3619 | 22 | 678 | 1.27 | 0.14 | 0.02 | 0.10 | 0.01 | 69 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 828.3619 | 28 | 715 | 1.27 | 0.25 | 0.11 | 0.13 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 828.3619 | 28 | 715 | 1.52 | 0.26 | 0.11 | 0.14 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 1 | Optimal | 828.3619 | 38 | 612 | 2.27* | 1.14* | 0.03 | 1.11 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 1 | Optimal | 828.3619 | 38 | 612 | 2.55 | 1.25 | 0.03 | 1.20 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 828.3619 | 34 | 585 | 2.43* | 1.17* | 0.14 | 1.03 | 0.00 | 67 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 828.3619 | 34 | 585 | 2.32 | 1.20 | 0.13 | 1.06 | 0.01 | 68 |
| layer-cpp-highspy-exact | 2 | Optimal | 828.3619 | 22 | 678 | 1.39* | 0.11* | 0.02 | 0.09 | 0.00 | 68 |
| layer-purepython-highspy-exact | 2 | Optimal | 828.3619 | 22 | 678 | 1.28 | 0.13 | 0.02 | 0.10 | 0.01 | 69 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 828.3619 | 28 | 715 | 1.27 | 0.26 | 0.11 | 0.14 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 828.3619 | 28 | 715 | 1.48 | 0.26 | 0.11 | 0.14 | 0.01 | 67 |
| layer-cpp-highspy-tabu | 2 | Optimal | 828.3619 | 38 | 612 | 2.41* | 1.20* | 0.03 | 1.17 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 2 | Optimal | 828.3619 | 38 | 612 | 2.33 | 1.20 | 0.03 | 1.16 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 828.3619 | 34 | 585 | 2.18 | 1.15 | 0.13 | 1.01 | 0.00 | 66 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 828.3619 | 34 | 585 | 2.68* | 1.55* | 0.50 | 1.05 | 0.01 | 71 |
| layer-cpp-highspy-exact | 3 | Optimal | 828.3619 | 22 | 678 | 1.35* | 0.12* | 0.02 | 0.09 | 0.00 | 58 |
| layer-purepython-highspy-exact | 3 | Optimal | 828.3619 | 22 | 678 | 1.28 | 0.14 | 0.02 | 0.10 | 0.01 | 69 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 828.3619 | 28 | 715 | 1.27 | 0.24 | 0.11 | 0.13 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 828.3619 | 28 | 715 | 1.27 | 0.25 | 0.11 | 0.13 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 3 | Optimal | 828.3619 | 38 | 612 | 2.21* | 1.18* | 0.03 | 1.16 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 3 | Optimal | 828.3619 | 38 | 612 | 2.33 | 1.18 | 0.03 | 1.14 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 828.3619 | 34 | 585 | 2.36 | 1.19 | 0.13 | 1.06 | 0.00 | 67 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 828.3619 | 34 | 585 | 2.70* | 1.27* | 0.14 | 1.12 | 0.01 | 68 |
| layer-cpp-highspy-exact | 4 | Optimal | 828.3619 | 22 | 678 | 1.17 | 0.11 | 0.02 | 0.09 | 0.00 | 68 |
| layer-purepython-highspy-exact | 4 | Optimal | 828.3619 | 22 | 678 | 1.27 | 0.13 | 0.02 | 0.10 | 0.01 | 69 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 828.3619 | 28 | 715 | 1.28 | 0.26 | 0.11 | 0.15 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 828.3619 | 28 | 715 | 1.50 | 0.26 | 0.11 | 0.14 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 4 | Optimal | 828.3619 | 38 | 612 | 2.22* | 1.25* | 0.03 | 1.22 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 4 | Optimal | 828.3619 | 38 | 612 | 2.33 | 1.19 | 0.03 | 1.14 | 0.01 | 68 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 828.3619 | 34 | 585 | 2.31 | 1.19 | 0.13 | 1.06 | 0.00 | 67 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 828.3619 | 34 | 585 | 2.47* | 1.19* | 0.13 | 1.04 | 0.01 | 68 |

## R201_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 756.3386 | 28 | 12671 | 10.60* | 9.43* | 1.06 | 8.37 | 0.00 | 89 |
| layer-purepython-highspy-exact | 0 | Optimal | 756.3386 | 28 | 12671 | 10.72* | 9.45* | 1.06 | 8.37 | 0.01 | 91 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 756.3386 | 31 | 11918 | 9.69* | 8.44* | 0.73 | 7.70 | 0.01 | 88 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 756.3386 | 31 | 11918 | 9.95* | 8.62* | 0.43 | 8.18 | 0.02 | 91 |
| layer-cpp-highspy-tabu | 0 | Optimal | 756.3386 | 47 | 4097 | 4.16* | 3.04* | 0.30 | 2.73 | 0.00 | 74 |
| layer-purepython-highspy-tabu | 0 | Optimal | 756.3386 | 47 | 4097 | 4.31* | 3.27* | 0.34 | 2.92 | 0.01 | 75 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 756.3386 | 41 | 4723 | 4.03 | 2.92 | 0.27 | 2.64 | 0.01 | 73 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 756.3386 | 41 | 4723 | 4.12* | 2.91* | 0.26 | 2.63 | 0.01 | 76 |
| layer-cpp-highspy-exact | 1 | Optimal | 756.3386 | 28 | 12671 | 9.96* | 8.94* | 1.01 | 7.93 | 0.00 | 89 |
| layer-purepython-highspy-exact | 1 | Optimal | 756.3386 | 28 | 12671 | 10.71* | 9.51* | 1.14 | 8.36 | 0.01 | 91 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 756.3386 | 31 | 11918 | 9.11* | 8.10* | 0.43 | 7.66 | 0.01 | 88 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 756.3386 | 31 | 11918 | 9.43* | 8.27* | 0.45 | 7.81 | 0.02 | 91 |
| layer-cpp-highspy-tabu | 1 | Optimal | 756.3386 | 47 | 4097 | 4.07 | 2.90 | 0.31 | 2.59 | 0.00 | 74 |
| layer-purepython-highspy-tabu | 1 | Optimal | 756.3386 | 47 | 4097 | 4.14* | 2.92* | 0.32 | 2.59 | 0.01 | 75 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 756.3386 | 41 | 4723 | 4.12* | 2.92* | 0.27 | 2.65 | 0.01 | 74 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 756.3386 | 41 | 4723 | 4.12* | 3.03* | 0.29 | 2.73 | 0.01 | 75 |
| layer-cpp-highspy-exact | 2 | Optimal | 756.3386 | 28 | 12671 | 10.39* | 9.42* | 1.05 | 8.36 | 0.00 | 89 |
| layer-purepython-highspy-exact | 2 | Optimal | 756.3386 | 28 | 12671 | 10.36* | 9.25* | 1.08 | 8.16 | 0.01 | 91 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 756.3386 | 31 | 11918 | 9.32* | 8.12* | 0.45 | 7.66 | 0.01 | 88 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 756.3386 | 31 | 11918 | 9.51* | 8.15* | 0.45 | 7.68 | 0.02 | 91 |
| layer-cpp-highspy-tabu | 2 | Optimal | 756.3386 | 47 | 4097 | 4.06 | 2.89 | 0.31 | 2.58 | 0.00 | 74 |
| layer-purepython-highspy-tabu | 2 | Optimal | 756.3386 | 47 | 4097 | 4.21* | 3.02* | 0.32 | 2.68 | 0.01 | 75 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 756.3386 | 41 | 4723 | 4.10* | 2.97* | 0.27 | 2.69 | 0.01 | 73 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 756.3386 | 41 | 4723 | 4.09* | 3.02* | 0.27 | 2.73 | 0.01 | 76 |
| layer-cpp-highspy-exact | 3 | Optimal | 756.3386 | 28 | 12671 | 11.38* | 10.27* | 1.06 | 9.21 | 0.00 | 89 |
| layer-purepython-highspy-exact | 3 | Optimal | 756.3386 | 28 | 12671 | 10.04* | 8.88* | 1.08 | 7.79 | 0.01 | 91 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 756.3386 | 31 | 11918 | 9.75* | 8.58* | 0.42 | 8.15 | 0.01 | 89 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 756.3386 | 31 | 11918 | 9.10* | 7.91* | 0.45 | 7.44 | 0.02 | 91 |
| layer-cpp-highspy-tabu | 3 | Optimal | 756.3386 | 47 | 4097 | 3.80 | 2.78 | 0.30 | 2.48 | 0.00 | 74 |
| layer-purepython-highspy-tabu | 3 | Optimal | 756.3386 | 47 | 4097 | 4.10* | 2.91* | 0.32 | 2.58 | 0.01 | 75 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 756.3386 | 41 | 4723 | 3.86* | 2.86* | 0.26 | 2.59 | 0.01 | 74 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 756.3386 | 41 | 4723 | 4.56 | 3.30 | 0.28 | 3.00 | 0.02 | 76 |
| layer-cpp-highspy-exact | 4 | Optimal | 756.3386 | 28 | 12671 | 9.72* | 8.68* | 1.02 | 7.66 | 0.00 | 90 |
| layer-purepython-highspy-exact | 4 | Optimal | 756.3386 | 28 | 12671 | 9.48* | 8.40* | 1.02 | 7.37 | 0.01 | 91 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 756.3386 | 31 | 11918 | 8.84* | 7.71* | 0.45 | 7.25 | 0.01 | 88 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 756.3386 | 31 | 11918 | 8.75* | 7.54* | 0.43 | 7.09 | 0.02 | 91 |
| layer-cpp-highspy-tabu | 4 | Optimal | 756.3386 | 47 | 4097 | 3.90* | 2.97* | 0.31 | 2.67 | 0.00 | 74 |
| layer-purepython-highspy-tabu | 4 | Optimal | 756.3386 | 47 | 4097 | 4.12* | 2.96* | 0.32 | 2.63 | 0.01 | 75 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 756.3386 | 41 | 4723 | 4.01 | 2.91 | 0.27 | 2.63 | 0.01 | 74 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 756.3386 | 41 | 4723 | 4.33* | 3.05* | 0.28 | 2.75 | 0.02 | 76 |

## By configuration (mean wall s)

| config | C101_25 | R101_25 | R201_25 | RC201_12 | C101_50 | R101_50 | RC101_50 | R201_50 |
|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 1.11 | 1.13 | 1.94 | 1.11 | 1.54 | 1.17 | 1.25 | 10.41 |
| layer-cpp-gurobipy-exact | 1.30 | 1.12 | 1.71 | 1.26 | 1.54 | 1.20 | 1.28 | 9.34 |
| layer-purepython-highspy-exact | 1.28 | 1.11 | 1.75 | 1.19 | 1.58 | 1.19 | 1.28 | 10.26 |
| layer-purepython-highspy-tabu | 2.14 | 1.11 | 1.51 | 1.44 | 2.79 | 1.32 | 2.40 | 4.18 |
| layer-cpp-highspy-tabu | 1.99 | 1.06 | 1.37 | 1.36 | 2.76 | 1.33 | 2.25 | 4.00 |
| layer-purepython-gurobipy-exact | 1.32 | 1.19 | 1.65 | 1.30 | 1.70 | 1.32 | 1.45 | 9.35 |
| layer-purepython-gurobipy-tabu | 2.45 | 1.34 | 1.48 | 1.53 | 4.63 | 1.70 | 2.50 | 4.24 |
| layer-cpp-gurobipy-tabu | 2.46 | 1.25 | 1.55 | 1.44 | 4.67 | 1.53 | 2.32 | 4.02 |

## By configuration (mean solve s)

| config | C101_25 | R101_25 | R201_25 | RC201_12 | C101_50 | R101_50 | RC101_50 | R201_50 |
|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0.05 | 0.01 | 0.57 | 0.09 | 0.37 | 0.06 | 0.11 | 9.35 |
| layer-cpp-gurobipy-exact | 0.18 | 0.03 | 0.58 | 0.15 | 0.46 | 0.11 | 0.25 | 8.19 |
| layer-purepython-highspy-exact | 0.07 | 0.02 | 0.53 | 0.10 | 0.46 | 0.08 | 0.14 | 9.10 |
| layer-purepython-highspy-tabu | 1.04 | 0.04 | 0.37 | 0.34 | 1.68 | 0.27 | 1.21 | 3.01 |
| layer-cpp-highspy-tabu | 0.97 | 0.03 | 0.34 | 0.31 | 1.64 | 0.25 | 1.19 | 2.92 |
| layer-purepython-gurobipy-exact | 0.19 | 0.03 | 0.56 | 0.15 | 0.49 | 0.12 | 0.26 | 8.10 |
| layer-purepython-gurobipy-tabu | 1.28 | 0.10 | 0.42 | 0.37 | 3.47 | 0.53 | 1.28 | 3.06 |
| layer-cpp-gurobipy-tabu | 1.30 | 0.09 | 0.43 | 0.36 | 3.55 | 0.48 | 1.17 | 2.92 |

## LP mismatches

none
