# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 191.8136 | 18 | 666 | 1.06 | 0.04 | 0.02 | 0.03 | 0.00 | 56 |
| layer-purepython-highspy-exact | 0 | Optimal | 191.8136 | 18 | 666 | 1.06 | 0.06 | 0.02 | 0.04 | 0.00 | 56 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 191.8136 | 29 | 723 | 1.05 | 0.16 | 0.11 | 0.05 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 191.8136 | 29 | 723 | 1.27 | 0.17 | 0.11 | 0.06 | 0.00 | 65 |
| layer-cpp-highspy-tabu | 0 | Optimal | 191.8136 | 66 | 412 | 1.98 | 0.92 | 0.03 | 0.89 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 0 | Optimal | 191.8136 | 66 | 412 | 1.89 | 0.92 | 0.04 | 0.88 | 0.01 | 65 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 191.8136 | 64 | 527 | 2.10 | 1.16 | 0.24 | 0.92 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 191.8136 | 64 | 527 | 2.16* | 1.17* | 0.24 | 0.93 | 0.01 | 64 |
| layer-cpp-highspy-exact | 1 | Optimal | 191.8136 | 18 | 666 | 1.05 | 0.05 | 0.02 | 0.03 | 0.00 | 64 |
| layer-purepython-highspy-exact | 1 | Optimal | 191.8136 | 18 | 666 | 1.26 | 0.07 | 0.02 | 0.04 | 0.00 | 65 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 191.8136 | 29 | 723 | 1.09 | 0.17 | 0.11 | 0.06 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 191.8136 | 29 | 723 | 1.22* | 0.18* | 0.11 | 0.06 | 0.00 | 64 |
| layer-cpp-highspy-tabu | 1 | Optimal | 191.8136 | 66 | 412 | 1.88 | 0.90 | 0.03 | 0.87 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 1 | Optimal | 191.8136 | 66 | 412 | 1.88 | 0.95 | 0.04 | 0.90 | 0.01 | 65 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 191.8136 | 64 | 527 | 2.21* | 1.14* | 0.23 | 0.91 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 191.8136 | 64 | 527 | 2.30 | 1.17 | 0.23 | 0.93 | 0.01 | 64 |
| layer-cpp-highspy-exact | 2 | Optimal | 191.8136 | 18 | 666 | 1.11 | 0.04 | 0.02 | 0.03 | 0.00 | 64 |
| layer-purepython-highspy-exact | 2 | Optimal | 191.8136 | 18 | 666 | 1.18 | 0.06 | 0.02 | 0.04 | 0.00 | 65 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 191.8136 | 29 | 723 | 1.26 | 0.18 | 0.12 | 0.06 | 0.00 | 64 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 191.8136 | 29 | 723 | 1.26 | 0.18 | 0.11 | 0.06 | 0.00 | 65 |
| layer-cpp-highspy-tabu | 2 | Optimal | 191.8136 | 66 | 412 | 1.96 | 0.91 | 0.03 | 0.88 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 2 | Optimal | 191.8136 | 66 | 412 | 1.89* | 0.94* | 0.04 | 0.89 | 0.01 | 65 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 191.8136 | 64 | 527 | 2.30 | 1.21 | 0.24 | 0.97 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 191.8136 | 64 | 527 | 2.18* | 1.23* | 0.24 | 0.98 | 0.01 | 64 |
| layer-cpp-highspy-exact | 3 | Optimal | 191.8136 | 18 | 666 | 1.05 | 0.05 | 0.02 | 0.03 | 0.00 | 64 |
| layer-purepython-highspy-exact | 3 | Optimal | 191.8136 | 18 | 666 | 1.06 | 0.06 | 0.02 | 0.04 | 0.00 | 65 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 191.8136 | 29 | 723 | 1.05 | 0.17 | 0.11 | 0.05 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 191.8136 | 29 | 723 | 1.16* | 0.18* | 0.11 | 0.06 | 0.00 | 65 |
| layer-cpp-highspy-tabu | 3 | Optimal | 191.8136 | 66 | 412 | 1.89 | 0.89 | 0.03 | 0.86 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 3 | Optimal | 191.8136 | 66 | 412 | 1.89 | 0.93 | 0.04 | 0.89 | 0.01 | 65 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 191.8136 | 64 | 527 | 2.19* | 1.19* | 0.24 | 0.94 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 191.8136 | 64 | 527 | 2.32 | 1.24 | 0.25 | 0.98 | 0.01 | 64 |
| layer-cpp-highspy-exact | 4 | Optimal | 191.8136 | 18 | 666 | 1.09 | 0.05 | 0.02 | 0.03 | 0.00 | 64 |
| layer-purepython-highspy-exact | 4 | Optimal | 191.8136 | 18 | 666 | 0.99* | 0.06* | 0.02 | 0.04 | 0.00 | 54 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 191.8136 | 29 | 723 | 1.05 | 0.16 | 0.11 | 0.06 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 191.8136 | 29 | 723 | 1.26 | 0.17 | 0.11 | 0.06 | 0.00 | 65 |
| layer-cpp-highspy-tabu | 4 | Optimal | 191.8136 | 66 | 412 | 1.95 | 0.91 | 0.03 | 0.88 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 4 | Optimal | 191.8136 | 66 | 412 | 2.05* | 0.93* | 0.04 | 0.89 | 0.01 | 65 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 191.8136 | 64 | 527 | 2.11 | 1.18 | 0.23 | 0.94 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 191.8136 | 64 | 527 | 2.20* | 1.17* | 0.23 | 0.93 | 0.01 | 65 |

## R101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 618.3299 | 7 | 99 | 1.05 | 0.01 | 0.00 | 0.01 | 0.00 | 64 |
| layer-purepython-highspy-exact | 0 | Optimal | 618.3299 | 7 | 99 | 1.05 | 0.02 | 0.00 | 0.01 | 0.00 | 64 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 618.3299 | 6 | 90 | 1.07 | 0.03 | 0.02 | 0.01 | 0.00 | 58 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 618.3299 | 6 | 90 | 1.11 | 0.04 | 0.02 | 0.01 | 0.00 | 54 |
| layer-cpp-highspy-tabu | 0 | Optimal | 618.3299 | 7 | 96 | 1.12* | 0.03* | 0.00 | 0.03 | 0.00 | 63 |
| layer-purepython-highspy-tabu | 0 | Optimal | 618.3299 | 7 | 96 | 1.06 | 0.03 | 0.00 | 0.03 | 0.00 | 64 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 618.3299 | 9 | 95 | 1.28 | 0.09 | 0.03 | 0.06 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 618.3299 | 9 | 95 | 1.05 | 0.09 | 0.03 | 0.06 | 0.00 | 64 |
| layer-cpp-highspy-exact | 1 | Optimal | 618.3299 | 7 | 99 | 1.05 | 0.01 | 0.00 | 0.01 | 0.00 | 63 |
| layer-purepython-highspy-exact | 1 | Optimal | 618.3299 | 7 | 99 | 1.13 | 0.02 | 0.00 | 0.01 | 0.00 | 64 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 618.3299 | 6 | 90 | 1.05 | 0.03 | 0.02 | 0.01 | 0.00 | 61 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 618.3299 | 6 | 90 | 1.05 | 0.03 | 0.02 | 0.01 | 0.00 | 64 |
| layer-cpp-highspy-tabu | 1 | Optimal | 618.3299 | 7 | 96 | 1.05 | 0.03 | 0.00 | 0.03 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 1 | Optimal | 618.3299 | 7 | 96 | 1.05 | 0.04 | 0.00 | 0.03 | 0.00 | 64 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 618.3299 | 9 | 95 | 1.10 | 0.09 | 0.03 | 0.05 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 618.3299 | 9 | 95 | 1.17* | 0.09* | 0.03 | 0.05 | 0.00 | 64 |
| layer-cpp-highspy-exact | 2 | Optimal | 618.3299 | 7 | 99 | 1.05 | 0.01 | 0.00 | 0.01 | 0.00 | 63 |
| layer-purepython-highspy-exact | 2 | Optimal | 618.3299 | 7 | 99 | 1.05 | 0.02 | 0.00 | 0.01 | 0.00 | 64 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 618.3299 | 6 | 90 | 1.05 | 0.03 | 0.02 | 0.01 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 618.3299 | 6 | 90 | 1.05 | 0.03 | 0.02 | 0.01 | 0.00 | 64 |
| layer-cpp-highspy-tabu | 2 | Optimal | 618.3299 | 7 | 96 | 0.93* | 0.03* | 0.00 | 0.03 | 0.00 | 51 |
| layer-purepython-highspy-tabu | 2 | Optimal | 618.3299 | 7 | 96 | 1.05 | 0.04 | 0.00 | 0.03 | 0.00 | 65 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 618.3299 | 9 | 95 | 1.05 | 0.08 | 0.03 | 0.05 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 618.3299 | 9 | 95 | 1.05 | 0.08 | 0.03 | 0.05 | 0.00 | 62 |
| layer-cpp-highspy-exact | 3 | Optimal | 618.3299 | 7 | 99 | 0.84 | 0.01 | 0.00 | 0.01 | 0.00 | 50 |
| layer-purepython-highspy-exact | 3 | Optimal | 618.3299 | 7 | 99 | 1.05 | 0.02 | 0.00 | 0.01 | 0.00 | 64 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 618.3299 | 6 | 90 | 1.12* | 0.03* | 0.02 | 0.01 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 618.3299 | 6 | 90 | 1.05 | 0.04 | 0.02 | 0.01 | 0.00 | 62 |
| layer-cpp-highspy-tabu | 3 | Optimal | 618.3299 | 7 | 96 | 1.05 | 0.03 | 0.00 | 0.03 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 3 | Optimal | 618.3299 | 7 | 96 | 1.05 | 0.04 | 0.00 | 0.03 | 0.00 | 64 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 618.3299 | 9 | 95 | 1.05 | 0.08 | 0.03 | 0.05 | 0.00 | 62 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 618.3299 | 9 | 95 | 1.14 | 0.09 | 0.03 | 0.05 | 0.00 | 60 |
| layer-cpp-highspy-exact | 4 | Optimal | 618.3299 | 7 | 99 | 1.05 | 0.01 | 0.00 | 0.01 | 0.00 | 64 |
| layer-purepython-highspy-exact | 4 | Optimal | 618.3299 | 7 | 99 | 1.05 | 0.02 | 0.00 | 0.01 | 0.00 | 64 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 618.3299 | 6 | 90 | 1.05 | 0.03 | 0.02 | 0.01 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 618.3299 | 6 | 90 | 1.05 | 0.03 | 0.02 | 0.01 | 0.00 | 62 |
| layer-cpp-highspy-tabu | 4 | Optimal | 618.3299 | 7 | 96 | 1.15* | 0.04* | 0.00 | 0.03 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 4 | Optimal | 618.3299 | 7 | 96 | 1.05 | 0.03 | 0.00 | 0.03 | 0.00 | 64 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 618.3299 | 9 | 95 | 1.05 | 0.08 | 0.03 | 0.05 | 0.00 | 57 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 618.3299 | 9 | 95 | 1.05 | 0.09 | 0.03 | 0.05 | 0.00 | 64 |

## R201_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 449.6598 | 12 | 3024 | 1.55* | 0.47* | 0.06 | 0.41 | 0.00 | 71 |
| layer-purepython-highspy-exact | 0 | Optimal | 449.6598 | 12 | 3024 | 1.47 | 0.50 | 0.06 | 0.44 | 0.00 | 71 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 449.6598 | 11 | 3247 | 1.68 | 0.52 | 0.06 | 0.46 | 0.00 | 70 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 449.6598 | 11 | 3247 | 1.47 | 0.53 | 0.06 | 0.46 | 0.00 | 71 |
| layer-cpp-highspy-tabu | 0 | Optimal | 449.6598 | 22 | 947 | 1.54* | 0.39* | 0.02 | 0.37 | 0.00 | 65 |
| layer-purepython-highspy-tabu | 0 | Optimal | 449.6598 | 22 | 947 | 1.26 | 0.38 | 0.03 | 0.35 | 0.00 | 66 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 449.6598 | 22 | 831 | 1.47 | 0.39 | 0.08 | 0.30 | 0.00 | 64 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 449.6598 | 22 | 831 | 1.47 | 0.40 | 0.08 | 0.31 | 0.00 | 65 |
| layer-cpp-highspy-exact | 1 | Optimal | 449.6598 | 12 | 3024 | 1.47 | 0.45 | 0.06 | 0.40 | 0.00 | 71 |
| layer-purepython-highspy-exact | 1 | Optimal | 449.6598 | 12 | 3024 | 1.47 | 0.48 | 0.06 | 0.42 | 0.00 | 71 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 449.6598 | 11 | 3247 | 1.57 | 0.60 | 0.06 | 0.53 | 0.00 | 70 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 449.6598 | 11 | 3247 | 1.47 | 0.54 | 0.06 | 0.47 | 0.00 | 71 |
| layer-cpp-highspy-tabu | 1 | Optimal | 449.6598 | 22 | 947 | 1.26 | 0.32 | 0.02 | 0.30 | 0.00 | 65 |
| layer-purepython-highspy-tabu | 1 | Optimal | 449.6598 | 22 | 947 | 1.26 | 0.34 | 0.02 | 0.32 | 0.00 | 66 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 449.6598 | 22 | 831 | 1.57 | 0.41 | 0.09 | 0.33 | 0.00 | 64 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 449.6598 | 22 | 831 | 1.47 | 0.39 | 0.08 | 0.31 | 0.00 | 65 |
| layer-cpp-highspy-exact | 2 | Optimal | 449.6598 | 12 | 3024 | 1.47 | 0.45 | 0.06 | 0.39 | 0.00 | 71 |
| layer-purepython-highspy-exact | 2 | Optimal | 449.6598 | 12 | 3024 | 1.59* | 0.49* | 0.06 | 0.43 | 0.00 | 72 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 449.6598 | 11 | 3247 | 1.47 | 0.53 | 0.06 | 0.47 | 0.00 | 70 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 449.6598 | 11 | 3247 | 1.68 | 0.59 | 0.06 | 0.52 | 0.00 | 71 |
| layer-cpp-highspy-tabu | 2 | Optimal | 449.6598 | 22 | 947 | 1.26 | 0.32 | 0.02 | 0.30 | 0.00 | 65 |
| layer-purepython-highspy-tabu | 2 | Optimal | 449.6598 | 22 | 947 | 1.35* | 0.35* | 0.02 | 0.32 | 0.00 | 66 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 449.6598 | 22 | 831 | 1.26 | 0.39 | 0.08 | 0.30 | 0.00 | 64 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 449.6598 | 22 | 831 | 1.47 | 0.40 | 0.08 | 0.31 | 0.00 | 65 |
| layer-cpp-highspy-exact | 3 | Optimal | 449.6598 | 12 | 3024 | 1.48 | 0.46 | 0.06 | 0.40 | 0.00 | 71 |
| layer-purepython-highspy-exact | 3 | Optimal | 449.6598 | 12 | 3024 | 1.47 | 0.47 | 0.06 | 0.41 | 0.00 | 72 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 449.6598 | 11 | 3247 | 1.47 | 0.52 | 0.06 | 0.46 | 0.00 | 70 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 449.6598 | 11 | 3247 | 1.58* | 0.64* | 0.07 | 0.56 | 0.00 | 71 |
| layer-cpp-highspy-tabu | 3 | Optimal | 449.6598 | 22 | 947 | 1.26 | 0.33 | 0.02 | 0.31 | 0.00 | 65 |
| layer-purepython-highspy-tabu | 3 | Optimal | 449.6598 | 22 | 947 | 1.26 | 0.34 | 0.02 | 0.32 | 0.00 | 66 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 449.6598 | 22 | 831 | 1.47 | 0.40 | 0.08 | 0.32 | 0.00 | 64 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 449.6598 | 22 | 831 | 1.57* | 0.49* | 0.10 | 0.39 | 0.01 | 65 |
| layer-cpp-highspy-exact | 4 | Optimal | 449.6598 | 12 | 3024 | 1.47 | 0.44 | 0.06 | 0.39 | 0.00 | 71 |
| layer-purepython-highspy-exact | 4 | Optimal | 449.6598 | 12 | 3024 | 1.56* | 0.49* | 0.06 | 0.43 | 0.00 | 72 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 449.6598 | 11 | 3247 | 1.68 | 0.52 | 0.06 | 0.46 | 0.00 | 70 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 449.6598 | 11 | 3247 | 1.47 | 0.54 | 0.06 | 0.48 | 0.00 | 71 |
| layer-cpp-highspy-tabu | 4 | Optimal | 449.6598 | 22 | 947 | 1.29 | 0.32 | 0.02 | 0.30 | 0.00 | 65 |
| layer-purepython-highspy-tabu | 4 | Optimal | 449.6598 | 22 | 947 | 1.43* | 0.38* | 0.03 | 0.35 | 0.00 | 66 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 449.6598 | 22 | 831 | 1.47 | 0.39 | 0.08 | 0.31 | 0.00 | 64 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 449.6598 | 22 | 831 | 1.47 | 0.41 | 0.09 | 0.32 | 0.00 | 65 |

## RC201_12

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 427.2356 | 16 | 1050 | 1.12* | 0.09* | 0.02 | 0.06 | 0.00 | 65 |
| layer-purepython-highspy-exact | 0 | Optimal | 427.2356 | 16 | 1050 | 1.05 | 0.10 | 0.03 | 0.07 | 0.00 | 65 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 427.2356 | 16 | 1046 | 1.05 | 0.14 | 0.06 | 0.07 | 0.00 | 64 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 427.2356 | 16 | 1046 | 1.26 | 0.14 | 0.06 | 0.08 | 0.00 | 65 |
| layer-cpp-highspy-tabu | 0 | Optimal | 427.2356 | 22 | 483 | 1.26 | 0.26 | 0.01 | 0.25 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 0 | Optimal | 427.2356 | 22 | 483 | 1.34* | 0.28* | 0.02 | 0.26 | 0.00 | 65 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 427.2356 | 22 | 392 | 1.26 | 0.33 | 0.08 | 0.25 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 427.2356 | 22 | 392 | 1.26 | 0.34 | 0.08 | 0.25 | 0.00 | 64 |
| layer-cpp-highspy-exact | 1 | Optimal | 427.2356 | 16 | 1050 | 1.05 | 0.08 | 0.02 | 0.06 | 0.00 | 65 |
| layer-purepython-highspy-exact | 1 | Optimal | 427.2356 | 16 | 1050 | 1.05 | 0.10 | 0.03 | 0.07 | 0.00 | 66 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 427.2356 | 16 | 1046 | 1.26 | 0.14 | 0.06 | 0.07 | 0.00 | 61 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 427.2356 | 16 | 1046 | 1.16* | 0.15* | 0.06 | 0.08 | 0.00 | 65 |
| layer-cpp-highspy-tabu | 1 | Optimal | 427.2356 | 22 | 483 | 1.26 | 0.28 | 0.01 | 0.26 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 1 | Optimal | 427.2356 | 22 | 483 | 1.26 | 0.30 | 0.02 | 0.28 | 0.00 | 65 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 427.2356 | 22 | 392 | 1.48 | 0.34 | 0.08 | 0.26 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 427.2356 | 22 | 392 | 1.37 | 0.36 | 0.08 | 0.27 | 0.00 | 64 |
| layer-cpp-highspy-exact | 2 | Optimal | 427.2356 | 16 | 1050 | 1.05 | 0.08 | 0.02 | 0.06 | 0.00 | 65 |
| layer-purepython-highspy-exact | 2 | Optimal | 427.2356 | 16 | 1050 | 1.14 | 0.11 | 0.03 | 0.08 | 0.00 | 66 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 427.2356 | 16 | 1046 | 1.05* | 0.14* | 0.06 | 0.07 | 0.00 | 64 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 427.2356 | 16 | 1046 | 1.26 | 0.16 | 0.06 | 0.09 | 0.00 | 65 |
| layer-cpp-highspy-tabu | 2 | Optimal | 427.2356 | 22 | 483 | 1.26 | 0.26 | 0.01 | 0.25 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 2 | Optimal | 427.2356 | 22 | 483 | 1.26 | 0.28 | 0.02 | 0.26 | 0.00 | 65 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 427.2356 | 22 | 392 | 1.35* | 0.33* | 0.08 | 0.25 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 427.2356 | 22 | 392 | 1.47 | 0.35 | 0.08 | 0.26 | 0.00 | 64 |
| layer-cpp-highspy-exact | 3 | Optimal | 427.2356 | 16 | 1050 | 1.05 | 0.08 | 0.02 | 0.06 | 0.00 | 65 |
| layer-purepython-highspy-exact | 3 | Optimal | 427.2356 | 16 | 1050 | 1.05 | 0.10 | 0.02 | 0.07 | 0.00 | 65 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 427.2356 | 16 | 1046 | 1.05 | 0.14 | 0.06 | 0.08 | 0.00 | 64 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 427.2356 | 16 | 1046 | 1.05 | 0.15 | 0.07 | 0.09 | 0.00 | 65 |
| layer-cpp-highspy-tabu | 3 | Optimal | 427.2356 | 22 | 483 | 1.38 | 0.28 | 0.02 | 0.27 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 3 | Optimal | 427.2356 | 22 | 483 | 1.26 | 0.29 | 0.02 | 0.27 | 0.00 | 65 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 427.2356 | 22 | 392 | 1.26 | 0.34 | 0.08 | 0.26 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 427.2356 | 22 | 392 | 1.26 | 0.34 | 0.08 | 0.25 | 0.00 | 64 |
| layer-cpp-highspy-exact | 4 | Optimal | 427.2356 | 16 | 1050 | 1.10 | 0.08 | 0.02 | 0.06 | 0.00 | 65 |
| layer-purepython-highspy-exact | 4 | Optimal | 427.2356 | 16 | 1050 | 1.15* | 0.10* | 0.02 | 0.07 | 0.00 | 66 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 427.2356 | 16 | 1046 | 1.05 | 0.14 | 0.06 | 0.08 | 0.00 | 64 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 427.2356 | 16 | 1046 | 1.26 | 0.14 | 0.06 | 0.08 | 0.00 | 65 |
| layer-cpp-highspy-tabu | 4 | Optimal | 427.2356 | 22 | 483 | 1.26 | 0.27 | 0.01 | 0.25 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 4 | Optimal | 427.2356 | 22 | 483 | 1.36* | 0.29* | 0.02 | 0.27 | 0.00 | 65 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 427.2356 | 22 | 392 | 1.26 | 0.33 | 0.08 | 0.25 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 427.2356 | 22 | 392 | 1.26 | 0.34 | 0.08 | 0.26 | 0.00 | 64 |

## C101_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 363.2468 | 54 | 1749 | 1.26 | 0.32 | 0.11 | 0.21 | 0.00 | 69 |
| layer-purepython-highspy-exact | 0 | Optimal | 363.2468 | 54 | 1749 | 1.57 | 0.42 | 0.12 | 0.29 | 0.01 | 70 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 363.2468 | 43 | 1702 | 1.47 | 0.41 | 0.18 | 0.22 | 0.00 | 68 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 363.2468 | 43 | 1702 | 1.47 | 0.42 | 0.18 | 0.23 | 0.01 | 69 |
| layer-cpp-highspy-tabu | 0 | Optimal | 363.2468 | 67 | 675 | 2.63 | 1.56 | 0.06 | 1.51 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 0 | Optimal | 363.2468 | 67 | 675 | 2.51 | 1.49 | 0.06 | 1.42 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 363.2468 | 111 | 815 | 4.11 | 3.18 | 0.43 | 2.74 | 0.01 | 67 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 363.2468 | 111 | 815 | 4.23 | 3.21 | 0.42 | 2.77 | 0.02 | 68 |
| layer-cpp-highspy-exact | 1 | Optimal | 363.2468 | 54 | 1749 | 1.26 | 0.32 | 0.11 | 0.21 | 0.00 | 69 |
| layer-purepython-highspy-exact | 1 | Optimal | 363.2468 | 54 | 1749 | 1.26 | 0.36 | 0.11 | 0.24 | 0.01 | 70 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 363.2468 | 43 | 1702 | 1.47 | 0.40 | 0.19 | 0.22 | 0.00 | 68 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 363.2468 | 43 | 1702 | 1.55 | 0.45 | 0.19 | 0.25 | 0.01 | 69 |
| layer-cpp-highspy-tabu | 1 | Optimal | 363.2468 | 67 | 675 | 2.51 | 1.49 | 0.05 | 1.44 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 1 | Optimal | 363.2468 | 67 | 675 | 2.55 | 1.53 | 0.06 | 1.46 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 363.2468 | 111 | 815 | 4.55* | 3.39* | 0.70 | 2.68 | 0.01 | 70 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 363.2468 | 111 | 815 | 4.33* | 3.25* | 0.43 | 2.81 | 0.02 | 68 |
| layer-cpp-highspy-exact | 2 | Optimal | 363.2468 | 54 | 1749 | 1.26 | 0.34 | 0.11 | 0.23 | 0.00 | 69 |
| layer-purepython-highspy-exact | 2 | Optimal | 363.2468 | 54 | 1749 | 1.47 | 0.42 | 0.12 | 0.28 | 0.02 | 70 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 363.2468 | 43 | 1702 | 1.56* | 0.42* | 0.19 | 0.23 | 0.00 | 68 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 363.2468 | 43 | 1702 | 1.47 | 0.42 | 0.18 | 0.23 | 0.01 | 69 |
| layer-cpp-highspy-tabu | 2 | Optimal | 363.2468 | 67 | 675 | 2.51 | 1.47 | 0.05 | 1.42 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 2 | Optimal | 363.2468 | 67 | 675 | 2.65* | 1.63* | 0.06 | 1.56 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 363.2468 | 111 | 815 | 4.25 | 3.10 | 0.42 | 2.67 | 0.01 | 67 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 363.2468 | 111 | 815 | 4.22 | 3.15 | 0.42 | 2.71 | 0.02 | 68 |
| layer-cpp-highspy-exact | 3 | Optimal | 363.2468 | 54 | 1749 | 1.32 | 0.32 | 0.11 | 0.21 | 0.00 | 69 |
| layer-purepython-highspy-exact | 3 | Optimal | 363.2468 | 54 | 1749 | 1.43* | 0.37* | 0.11 | 0.25 | 0.01 | 70 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 363.2468 | 43 | 1702 | 1.47 | 0.40 | 0.18 | 0.22 | 0.00 | 68 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 363.2468 | 43 | 1702 | 1.47 | 0.44 | 0.19 | 0.24 | 0.01 | 69 |
| layer-cpp-highspy-tabu | 3 | Optimal | 363.2468 | 67 | 675 | 2.59* | 1.52* | 0.05 | 1.47 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 3 | Optimal | 363.2468 | 67 | 675 | 2.52 | 1.56 | 0.06 | 1.49 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 363.2468 | 111 | 815 | 4.49* | 3.42* | 0.69 | 2.71 | 0.01 | 70 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 363.2468 | 111 | 815 | 4.27 | 3.15 | 0.42 | 2.71 | 0.02 | 68 |
| layer-cpp-highspy-exact | 4 | Optimal | 363.2468 | 54 | 1749 | 1.26 | 0.31 | 0.10 | 0.21 | 0.00 | 69 |
| layer-purepython-highspy-exact | 4 | Optimal | 363.2468 | 54 | 1749 | 1.58 | 0.44 | 0.12 | 0.31 | 0.01 | 70 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 363.2468 | 43 | 1702 | 1.47 | 0.41 | 0.18 | 0.22 | 0.00 | 68 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 363.2468 | 43 | 1702 | 1.47 | 0.42 | 0.19 | 0.23 | 0.01 | 69 |
| layer-cpp-highspy-tabu | 4 | Optimal | 363.2468 | 67 | 675 | 2.61 | 1.56 | 0.05 | 1.50 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 4 | Optimal | 363.2468 | 67 | 675 | 2.52 | 1.51 | 0.06 | 1.44 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 363.2468 | 111 | 815 | 4.07* | 3.15* | 0.42 | 2.72 | 0.01 | 67 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 363.2468 | 111 | 815 | 4.23 | 3.20 | 0.42 | 2.76 | 0.02 | 68 |

## R101_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 1046.7011 | 16 | 410 | 1.20 | 0.07 | 0.01 | 0.06 | 0.00 | 67 |
| layer-purepython-highspy-exact | 0 | Optimal | 1046.7011 | 16 | 410 | 1.05 | 0.08 | 0.01 | 0.05 | 0.01 | 68 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 1046.7011 | 14 | 363 | 1.05 | 0.11 | 0.05 | 0.05 | 0.00 | 66 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 1046.7011 | 14 | 363 | 1.26 | 0.11 | 0.05 | 0.05 | 0.01 | 67 |
| layer-cpp-highspy-tabu | 0 | Optimal | 1046.7011 | 24 | 352 | 1.36 | 0.24 | 0.02 | 0.23 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 0 | Optimal | 1046.7011 | 24 | 352 | 1.26 | 0.27 | 0.02 | 0.24 | 0.01 | 68 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 1046.7011 | 32 | 335 | 1.47 | 0.45 | 0.11 | 0.33 | 0.00 | 66 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 1046.7011 | 32 | 335 | 1.48 | 0.46 | 0.11 | 0.34 | 0.01 | 67 |
| layer-cpp-highspy-exact | 1 | Optimal | 1046.7011 | 16 | 410 | 1.05 | 0.06 | 0.01 | 0.05 | 0.00 | 68 |
| layer-purepython-highspy-exact | 1 | Optimal | 1046.7011 | 16 | 410 | 1.05 | 0.07 | 0.01 | 0.05 | 0.01 | 68 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 1046.7011 | 14 | 363 | 1.16* | 0.10* | 0.05 | 0.05 | 0.00 | 66 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 1046.7011 | 14 | 363 | 1.26 | 0.11 | 0.05 | 0.05 | 0.01 | 67 |
| layer-cpp-highspy-tabu | 1 | Optimal | 1046.7011 | 24 | 352 | 1.26 | 0.23 | 0.02 | 0.21 | 0.00 | 67 |
| layer-purepython-highspy-tabu | 1 | Optimal | 1046.7011 | 24 | 352 | 1.26 | 0.26 | 0.02 | 0.24 | 0.01 | 68 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 1046.7011 | 32 | 335 | 1.52 | 0.45 | 0.12 | 0.33 | 0.00 | 67 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 1046.7011 | 32 | 335 | 1.47 | 0.46 | 0.12 | 0.34 | 0.01 | 67 |
| layer-cpp-highspy-exact | 2 | Optimal | 1046.7011 | 16 | 410 | 1.11 | 0.06 | 0.01 | 0.05 | 0.00 | 68 |
| layer-purepython-highspy-exact | 2 | Optimal | 1046.7011 | 16 | 410 | 1.16 | 0.08 | 0.01 | 0.06 | 0.01 | 69 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 1046.7011 | 14 | 363 | 1.06 | 0.12 | 0.06 | 0.07 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 1046.7011 | 14 | 363 | 1.27 | 0.13 | 0.06 | 0.06 | 0.01 | 65 |
| layer-cpp-highspy-tabu | 2 | Optimal | 1046.7011 | 24 | 352 | 1.27 | 0.24 | 0.02 | 0.22 | 0.00 | 67 |
| layer-purepython-highspy-tabu | 2 | Optimal | 1046.7011 | 24 | 352 | 1.38* | 0.26* | 0.02 | 0.23 | 0.01 | 68 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 1046.7011 | 32 | 335 | 1.47 | 0.45 | 0.11 | 0.34 | 0.00 | 66 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 1046.7011 | 32 | 335 | 1.47 | 0.47 | 0.11 | 0.34 | 0.01 | 67 |
| layer-cpp-highspy-exact | 3 | Optimal | 1046.7011 | 16 | 410 | 1.05 | 0.06 | 0.01 | 0.05 | 0.00 | 68 |
| layer-purepython-highspy-exact | 3 | Optimal | 1046.7011 | 16 | 410 | 1.05 | 0.09 | 0.01 | 0.06 | 0.01 | 63 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 1046.7011 | 14 | 363 | 1.26 | 0.10 | 0.05 | 0.05 | 0.00 | 66 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 1046.7011 | 14 | 363 | 1.12 | 0.12 | 0.06 | 0.06 | 0.01 | 67 |
| layer-cpp-highspy-tabu | 3 | Optimal | 1046.7011 | 24 | 352 | 1.26 | 0.25 | 0.02 | 0.23 | 0.00 | 67 |
| layer-purepython-highspy-tabu | 3 | Optimal | 1046.7011 | 24 | 352 | 1.26 | 0.25 | 0.02 | 0.22 | 0.01 | 68 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 1046.7011 | 32 | 335 | 1.47 | 0.45 | 0.12 | 0.33 | 0.00 | 66 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 1046.7011 | 32 | 335 | 1.59 | 0.48 | 0.12 | 0.36 | 0.01 | 67 |
| layer-cpp-highspy-exact | 4 | Optimal | 1046.7011 | 16 | 410 | 1.17 | 0.06 | 0.01 | 0.05 | 0.00 | 68 |
| layer-purepython-highspy-exact | 4 | Optimal | 1046.7011 | 16 | 410 | 1.05 | 0.08 | 0.01 | 0.05 | 0.01 | 67 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 1046.7011 | 14 | 363 | 1.05 | 0.10 | 0.05 | 0.05 | 0.00 | 66 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 1046.7011 | 14 | 363 | 1.05 | 0.11 | 0.05 | 0.05 | 0.01 | 65 |
| layer-cpp-highspy-tabu | 4 | Optimal | 1046.7011 | 24 | 352 | 1.39 | 0.25 | 0.02 | 0.23 | 0.00 | 67 |
| layer-purepython-highspy-tabu | 4 | Optimal | 1046.7011 | 24 | 352 | 1.27 | 0.25 | 0.02 | 0.22 | 0.01 | 68 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 1046.7011 | 32 | 335 | 1.47 | 0.44 | 0.12 | 0.32 | 0.00 | 66 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 1046.7011 | 32 | 335 | 1.47 | 0.47 | 0.11 | 0.35 | 0.01 | 67 |

## RC101_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 828.3619 | 22 | 678 | 1.37 | 0.15 | 0.03 | 0.13 | 0.00 | 68 |
| layer-purepython-highspy-exact | 0 | Optimal | 828.3619 | 22 | 678 | 1.06 | 0.13 | 0.02 | 0.10 | 0.01 | 68 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 828.3619 | 28 | 715 | 1.26 | 0.24 | 0.11 | 0.13 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 828.3619 | 28 | 715 | 1.27 | 0.24 | 0.10 | 0.12 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 0 | Optimal | 828.3619 | 38 | 612 | 2.17* | 1.09* | 0.03 | 1.06 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 0 | Optimal | 828.3619 | 38 | 612 | 2.10 | 1.18 | 0.03 | 1.14 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 828.3619 | 34 | 585 | 2.10 | 1.08 | 0.12 | 0.95 | 0.00 | 67 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 828.3619 | 34 | 585 | 2.18 | 1.13 | 0.13 | 0.99 | 0.01 | 68 |
| layer-cpp-highspy-exact | 1 | Optimal | 828.3619 | 22 | 678 | 1.05 | 0.10 | 0.02 | 0.08 | 0.00 | 68 |
| layer-purepython-highspy-exact | 1 | Optimal | 828.3619 | 22 | 678 | 1.05 | 0.13 | 0.02 | 0.10 | 0.01 | 68 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 828.3619 | 28 | 715 | 1.26 | 0.22 | 0.10 | 0.11 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 828.3619 | 28 | 715 | 1.38* | 0.24* | 0.11 | 0.13 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 1 | Optimal | 828.3619 | 38 | 612 | 2.09 | 1.10 | 0.03 | 1.07 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 1 | Optimal | 828.3619 | 38 | 612 | 2.09 | 1.14 | 0.03 | 1.10 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 828.3619 | 34 | 585 | 2.18 | 1.12 | 0.13 | 0.98 | 0.00 | 67 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 828.3619 | 34 | 585 | 2.09 | 1.11 | 0.12 | 0.97 | 0.01 | 68 |
| layer-cpp-highspy-exact | 2 | Optimal | 828.3619 | 22 | 678 | 1.06 | 0.10 | 0.02 | 0.08 | 0.00 | 67 |
| layer-purepython-highspy-exact | 2 | Optimal | 828.3619 | 22 | 678 | 1.12 | 0.14 | 0.02 | 0.11 | 0.01 | 69 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 828.3619 | 28 | 715 | 1.26 | 0.22 | 0.10 | 0.11 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 828.3619 | 28 | 715 | 1.26 | 0.23 | 0.10 | 0.12 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 2 | Optimal | 828.3619 | 38 | 612 | 2.10 | 1.07 | 0.03 | 1.05 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 2 | Optimal | 828.3619 | 38 | 612 | 2.17* | 1.15* | 0.03 | 1.11 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 828.3619 | 34 | 585 | 2.10 | 1.14 | 0.13 | 1.01 | 0.00 | 67 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 828.3619 | 34 | 585 | 2.14 | 1.10 | 0.13 | 0.96 | 0.01 | 68 |
| layer-cpp-highspy-exact | 3 | Optimal | 828.3619 | 22 | 678 | 1.05 | 0.10 | 0.02 | 0.08 | 0.00 | 68 |
| layer-purepython-highspy-exact | 3 | Optimal | 828.3619 | 22 | 678 | 1.06 | 0.13 | 0.02 | 0.10 | 0.01 | 68 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 828.3619 | 28 | 715 | 1.26 | 0.22 | 0.10 | 0.12 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 828.3619 | 28 | 715 | 1.26 | 0.24 | 0.10 | 0.12 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 3 | Optimal | 828.3619 | 38 | 612 | 2.18* | 1.08* | 0.03 | 1.05 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 3 | Optimal | 828.3619 | 38 | 612 | 2.11 | 1.12 | 0.03 | 1.08 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 828.3619 | 34 | 585 | 2.21 | 1.08 | 0.13 | 0.95 | 0.00 | 67 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 828.3619 | 34 | 585 | 2.31 | 1.19 | 0.13 | 1.04 | 0.01 | 68 |
| layer-cpp-highspy-exact | 4 | Optimal | 828.3619 | 22 | 678 | 1.14 | 0.11 | 0.02 | 0.09 | 0.00 | 68 |
| layer-purepython-highspy-exact | 4 | Optimal | 828.3619 | 22 | 678 | 1.28* | 0.12* | 0.02 | 0.09 | 0.01 | 69 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 828.3619 | 28 | 715 | 1.26 | 0.22 | 0.10 | 0.12 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 828.3619 | 28 | 715 | 1.26 | 0.23 | 0.10 | 0.12 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 4 | Optimal | 828.3619 | 38 | 612 | 2.18* | 1.12* | 0.03 | 1.09 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 4 | Optimal | 828.3619 | 38 | 612 | 2.10 | 1.10 | 0.03 | 1.06 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 828.3619 | 34 | 585 | 2.32 | 1.13 | 0.13 | 1.00 | 0.00 | 67 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 828.3619 | 34 | 585 | 2.18* | 1.09* | 0.12 | 0.96 | 0.01 | 68 |

## R201_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 756.3386 | 28 | 12671 | 8.74* | 7.67* | 0.94 | 6.73 | 0.00 | 89 |
| layer-purepython-highspy-exact | 0 | Optimal | 756.3386 | 28 | 12671 | 8.63* | 7.65* | 1.02 | 6.62 | 0.01 | 91 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 756.3386 | 31 | 11918 | 7.52* | 6.56* | 0.40 | 6.15 | 0.01 | 89 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 756.3386 | 31 | 11918 | 7.64 | 6.62 | 0.39 | 6.21 | 0.02 | 92 |
| layer-cpp-highspy-tabu | 0 | Optimal | 756.3386 | 47 | 4097 | 3.64* | 2.66* | 0.29 | 2.38 | 0.00 | 73 |
| layer-purepython-highspy-tabu | 0 | Optimal | 756.3386 | 47 | 4097 | 3.87 | 2.71 | 0.30 | 2.40 | 0.01 | 75 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 756.3386 | 41 | 4723 | 3.77 | 2.71 | 0.26 | 2.45 | 0.01 | 74 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 756.3386 | 41 | 4723 | 3.65* | 2.67* | 0.25 | 2.41 | 0.01 | 75 |
| layer-cpp-highspy-exact | 1 | Optimal | 756.3386 | 28 | 12671 | 8.52 | 7.57 | 0.98 | 6.60 | 0.00 | 90 |
| layer-purepython-highspy-exact | 1 | Optimal | 756.3386 | 28 | 12671 | 8.70 | 7.64 | 0.97 | 6.66 | 0.01 | 91 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 756.3386 | 31 | 11918 | 7.82 | 6.76 | 0.39 | 6.36 | 0.01 | 88 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 756.3386 | 31 | 11918 | 7.92 | 6.76 | 0.42 | 6.33 | 0.02 | 91 |
| layer-cpp-highspy-tabu | 1 | Optimal | 756.3386 | 47 | 4097 | 3.55 | 2.54 | 0.29 | 2.26 | 0.00 | 74 |
| layer-purepython-highspy-tabu | 1 | Optimal | 756.3386 | 47 | 4097 | 3.72 | 2.68 | 0.30 | 2.37 | 0.01 | 75 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 756.3386 | 41 | 4723 | 3.63 | 2.72 | 0.26 | 2.45 | 0.01 | 74 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 756.3386 | 41 | 4723 | 3.55 | 2.61 | 0.25 | 2.36 | 0.01 | 76 |
| layer-cpp-highspy-exact | 2 | Optimal | 756.3386 | 28 | 12671 | 8.44 | 7.40 | 0.94 | 6.46 | 0.00 | 89 |
| layer-purepython-highspy-exact | 2 | Optimal | 756.3386 | 28 | 12671 | 8.48 | 7.52 | 0.93 | 6.58 | 0.01 | 91 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 756.3386 | 31 | 11918 | 8.03 | 6.84 | 0.40 | 6.43 | 0.01 | 89 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 756.3386 | 31 | 11918 | 7.85 | 6.78 | 0.40 | 6.36 | 0.02 | 92 |
| layer-cpp-highspy-tabu | 2 | Optimal | 756.3386 | 47 | 4097 | 3.65* | 2.67* | 0.29 | 2.38 | 0.00 | 74 |
| layer-purepython-highspy-tabu | 2 | Optimal | 756.3386 | 47 | 4097 | 3.87 | 2.84 | 0.31 | 2.52 | 0.01 | 75 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 756.3386 | 41 | 4723 | 3.56 | 2.63 | 0.25 | 2.37 | 0.01 | 74 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 756.3386 | 41 | 4723 | 3.66* | 2.65* | 0.25 | 2.39 | 0.01 | 76 |
| layer-cpp-highspy-exact | 3 | Optimal | 756.3386 | 28 | 12671 | 8.76* | 7.71* | 0.93 | 6.78 | 0.00 | 89 |
| layer-purepython-highspy-exact | 3 | Optimal | 756.3386 | 28 | 12671 | 8.67 | 7.47 | 0.95 | 6.51 | 0.01 | 91 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 756.3386 | 31 | 11918 | 7.86* | 6.72* | 0.42 | 6.29 | 0.01 | 89 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 756.3386 | 31 | 11918 | 7.71 | 6.62 | 0.40 | 6.21 | 0.02 | 92 |
| layer-cpp-highspy-tabu | 3 | Optimal | 756.3386 | 47 | 4097 | 3.57 | 2.69 | 0.29 | 2.40 | 0.00 | 74 |
| layer-purepython-highspy-tabu | 3 | Optimal | 756.3386 | 47 | 4097 | 3.66 | 2.70 | 0.30 | 2.39 | 0.01 | 75 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 756.3386 | 41 | 4723 | 3.63 | 2.62 | 0.25 | 2.36 | 0.01 | 74 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 756.3386 | 41 | 4723 | 3.78 | 2.72 | 0.25 | 2.45 | 0.01 | 75 |
| layer-cpp-highspy-exact | 4 | Optimal | 756.3386 | 28 | 12671 | 8.47* | 7.46* | 0.95 | 6.50 | 0.00 | 90 |
| layer-purepython-highspy-exact | 4 | Optimal | 756.3386 | 28 | 12671 | 8.54 | 7.63 | 0.97 | 6.65 | 0.01 | 91 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 756.3386 | 31 | 11918 | 7.84 | 6.69 | 0.40 | 6.29 | 0.01 | 89 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 756.3386 | 31 | 11918 | 7.90 | 6.85 | 0.41 | 6.42 | 0.02 | 92 |
| layer-cpp-highspy-tabu | 4 | Optimal | 756.3386 | 47 | 4097 | 3.63 | 2.58 | 0.28 | 2.30 | 0.00 | 75 |
| layer-purepython-highspy-tabu | 4 | Optimal | 756.3386 | 47 | 4097 | 3.87* | 2.77* | 0.30 | 2.46 | 0.01 | 75 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 756.3386 | 41 | 4723 | 3.60 | 2.59 | 0.25 | 2.33 | 0.01 | 74 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 756.3386 | 41 | 4723 | 3.93* | 2.72* | 0.26 | 2.45 | 0.01 | 76 |

## By configuration (mean wall s)

| config | C101_25 | R101_25 | R201_25 | RC201_12 | C101_50 | R101_50 | RC101_50 | R201_50 |
|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 1.07 | 1.01 | 1.49 | 1.08 | 1.27 | 1.12 | 1.14 | 8.59 |
| layer-cpp-gurobipy-exact | 1.10 | 1.07 | 1.57 | 1.09 | 1.49 | 1.12 | 1.26 | 7.81 |
| layer-purepython-highspy-exact | 1.11 | 1.07 | 1.51 | 1.09 | 1.46 | 1.07 | 1.11 | 8.60 |
| layer-purepython-highspy-tabu | 1.92 | 1.05 | 1.31 | 1.29 | 2.55 | 1.29 | 2.11 | 3.80 |
| layer-cpp-highspy-tabu | 1.93 | 1.06 | 1.32 | 1.28 | 2.57 | 1.31 | 2.15 | 3.61 |
| layer-purepython-gurobipy-exact | 1.23 | 1.06 | 1.53 | 1.20 | 1.49 | 1.19 | 1.29 | 7.80 |
| layer-purepython-gurobipy-tabu | 2.23 | 1.09 | 1.49 | 1.32 | 4.26 | 1.50 | 2.18 | 3.72 |
| layer-cpp-gurobipy-tabu | 2.18 | 1.11 | 1.45 | 1.32 | 4.29 | 1.48 | 2.18 | 3.64 |

## By configuration (mean solve s)

| config | C101_25 | R101_25 | R201_25 | RC201_12 | C101_50 | R101_50 | RC101_50 | R201_50 |
|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0.05 | 0.01 | 0.45 | 0.08 | 0.32 | 0.06 | 0.11 | 7.56 |
| layer-cpp-gurobipy-exact | 0.17 | 0.03 | 0.54 | 0.14 | 0.41 | 0.11 | 0.22 | 6.71 |
| layer-purepython-highspy-exact | 0.06 | 0.02 | 0.49 | 0.10 | 0.40 | 0.08 | 0.13 | 7.58 |
| layer-purepython-highspy-tabu | 0.94 | 0.04 | 0.36 | 0.29 | 1.55 | 0.26 | 1.14 | 2.74 |
| layer-cpp-highspy-tabu | 0.91 | 0.03 | 0.34 | 0.27 | 1.52 | 0.24 | 1.09 | 2.63 |
| layer-purepython-gurobipy-exact | 0.17 | 0.03 | 0.57 | 0.15 | 0.43 | 0.12 | 0.24 | 6.72 |
| layer-purepython-gurobipy-tabu | 1.20 | 0.09 | 0.42 | 0.34 | 3.19 | 0.47 | 1.12 | 2.68 |
| layer-cpp-gurobipy-tabu | 1.18 | 0.08 | 0.40 | 0.34 | 3.25 | 0.45 | 1.11 | 2.65 |

## LP mismatches

none
