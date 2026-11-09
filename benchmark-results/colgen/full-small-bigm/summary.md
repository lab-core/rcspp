# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 191.8136 | 16 | 599 | 1.06 | 0.05 | 0.02 | 0.03 | 0.00 | 53 |
| layer-purepython-highspy-exact | 0 | Optimal | 191.8136 | 16 | 599 | 1.27 | 0.06 | 0.02 | 0.04 | 0.00 | 65 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 191.8136 | 23 | 691 | 1.26 | 0.14 | 0.09 | 0.05 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 191.8136 | 23 | 691 | 1.27 | 0.15 | 0.09 | 0.05 | 0.01 | 64 |
| layer-cpp-highspy-tabu | 0 | Optimal | 191.8136 | 53 | 434 | 1.99 | 0.90 | 0.03 | 0.87 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 0 | Optimal | 191.8136 | 53 | 434 | 2.11 | 1.00 | 0.04 | 0.95 | 0.01 | 65 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 191.8136 | 68 | 449 | 2.41 | 1.13 | 0.25 | 0.87 | 0.01 | 63 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 191.8136 | 68 | 449 | 2.09 | 1.12 | 0.25 | 0.86 | 0.01 | 64 |
| layer-cpp-highspy-exact | 1 | Optimal | 191.8136 | 16 | 599 | 1.05 | 0.04 | 0.01 | 0.02 | 0.00 | 64 |
| layer-purepython-highspy-exact | 1 | Optimal | 191.8136 | 16 | 599 | 1.05 | 0.05 | 0.02 | 0.03 | 0.00 | 65 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 191.8136 | 23 | 691 | 1.05 | 0.14 | 0.09 | 0.05 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 191.8136 | 23 | 691 | 1.05 | 0.14 | 0.08 | 0.05 | 0.00 | 64 |
| layer-cpp-highspy-tabu | 1 | Optimal | 191.8136 | 53 | 434 | 1.76* | 0.84* | 0.03 | 0.81 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 1 | Optimal | 191.8136 | 53 | 434 | 1.88 | 0.87 | 0.03 | 0.83 | 0.01 | 65 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 191.8136 | 68 | 449 | 2.09 | 1.09 | 0.24 | 0.84 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 191.8136 | 68 | 449 | 2.16 | 1.10 | 0.25 | 0.85 | 0.01 | 64 |
| layer-cpp-highspy-exact | 2 | Optimal | 191.8136 | 16 | 599 | 1.06 | 0.04 | 0.01 | 0.02 | 0.00 | 64 |
| layer-purepython-highspy-exact | 2 | Optimal | 191.8136 | 16 | 599 | 1.06 | 0.05 | 0.02 | 0.03 | 0.00 | 64 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 191.8136 | 23 | 691 | 1.05 | 0.13 | 0.09 | 0.05 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 191.8136 | 23 | 691 | 1.26 | 0.14 | 0.09 | 0.05 | 0.00 | 64 |
| layer-cpp-highspy-tabu | 2 | Optimal | 191.8136 | 53 | 434 | 1.80* | 0.85* | 0.03 | 0.82 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 2 | Optimal | 191.8136 | 53 | 434 | 1.89 | 0.86 | 0.03 | 0.82 | 0.01 | 65 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 191.8136 | 68 | 449 | 2.10 | 1.07 | 0.24 | 0.82 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 191.8136 | 68 | 449 | 2.20 | 1.15 | 0.25 | 0.90 | 0.01 | 64 |
| layer-cpp-highspy-exact | 3 | Optimal | 191.8136 | 16 | 599 | 1.05 | 0.04 | 0.01 | 0.02 | 0.00 | 64 |
| layer-purepython-highspy-exact | 3 | Optimal | 191.8136 | 16 | 599 | 1.09 | 0.05 | 0.02 | 0.03 | 0.00 | 65 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 191.8136 | 23 | 691 | 1.19 | 0.13 | 0.08 | 0.04 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 191.8136 | 23 | 691 | 1.26 | 0.14 | 0.09 | 0.05 | 0.00 | 65 |
| layer-cpp-highspy-tabu | 3 | Optimal | 191.8136 | 53 | 434 | 1.88 | 0.83 | 0.03 | 0.80 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 3 | Optimal | 191.8136 | 53 | 434 | 1.95* | 0.88* | 0.04 | 0.84 | 0.01 | 65 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 191.8136 | 68 | 449 | 2.09 | 1.08 | 0.24 | 0.83 | 0.01 | 63 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 191.8136 | 68 | 449 | 2.09 | 1.09 | 0.24 | 0.84 | 0.01 | 64 |
| layer-cpp-highspy-exact | 4 | Optimal | 191.8136 | 16 | 599 | 1.05 | 0.04 | 0.01 | 0.02 | 0.00 | 64 |
| layer-purepython-highspy-exact | 4 | Optimal | 191.8136 | 16 | 599 | 1.13 | 0.06 | 0.02 | 0.04 | 0.00 | 64 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 191.8136 | 23 | 691 | 1.05 | 0.13 | 0.09 | 0.05 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 191.8136 | 23 | 691 | 1.26 | 0.14 | 0.08 | 0.05 | 0.00 | 63 |
| layer-cpp-highspy-tabu | 4 | Optimal | 191.8136 | 53 | 434 | 1.67 | 0.81 | 0.03 | 0.78 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 4 | Optimal | 191.8136 | 53 | 434 | 1.94 | 0.88 | 0.04 | 0.83 | 0.01 | 65 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 191.8136 | 68 | 449 | 2.09 | 1.09 | 0.24 | 0.85 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 191.8136 | 68 | 449 | 2.09 | 1.10 | 0.24 | 0.85 | 0.01 | 64 |

## R101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 618.3299 | 10 | 147 | 0.84 | 0.01 | 0.00 | 0.01 | 0.00 | 50 |
| layer-purepython-highspy-exact | 0 | Optimal | 618.3299 | 10 | 147 | 1.05 | 0.02 | 0.01 | 0.01 | 0.00 | 64 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 618.3299 | 9 | 144 | 1.09 | 0.04 | 0.03 | 0.01 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 618.3299 | 9 | 144 | 1.20* | 0.05* | 0.03 | 0.01 | 0.00 | 64 |
| layer-cpp-highspy-tabu | 0 | Optimal | 618.3299 | 8 | 111 | 1.05 | 0.04 | 0.00 | 0.03 | 0.00 | 63 |
| layer-purepython-highspy-tabu | 0 | Optimal | 618.3299 | 8 | 111 | 1.05 | 0.04 | 0.00 | 0.04 | 0.00 | 64 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 618.3299 | 10 | 126 | 1.06 | 0.07 | 0.03 | 0.03 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 618.3299 | 10 | 126 | 1.11 | 0.08 | 0.04 | 0.04 | 0.00 | 64 |
| layer-cpp-highspy-exact | 1 | Optimal | 618.3299 | 10 | 147 | 0.84 | 0.01 | 0.00 | 0.01 | 0.00 | 51 |
| layer-purepython-highspy-exact | 1 | Optimal | 618.3299 | 10 | 147 | 1.05 | 0.02 | 0.01 | 0.01 | 0.00 | 64 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 618.3299 | 9 | 144 | 1.05 | 0.04 | 0.03 | 0.01 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 618.3299 | 9 | 144 | 1.05 | 0.05 | 0.03 | 0.01 | 0.00 | 64 |
| layer-cpp-highspy-tabu | 1 | Optimal | 618.3299 | 8 | 111 | 1.02* | 0.04* | 0.00 | 0.03 | 0.00 | 51 |
| layer-purepython-highspy-tabu | 1 | Optimal | 618.3299 | 8 | 111 | 1.05 | 0.04 | 0.00 | 0.04 | 0.00 | 64 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 618.3299 | 10 | 126 | 1.05 | 0.07 | 0.03 | 0.03 | 0.00 | 62 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 618.3299 | 10 | 126 | 1.05 | 0.07 | 0.04 | 0.03 | 0.00 | 63 |
| layer-cpp-highspy-exact | 2 | Optimal | 618.3299 | 10 | 147 | 1.06 | 0.01 | 0.00 | 0.01 | 0.00 | 50 |
| layer-purepython-highspy-exact | 2 | Optimal | 618.3299 | 10 | 147 | 1.05 | 0.02 | 0.01 | 0.01 | 0.00 | 64 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 618.3299 | 9 | 144 | 1.06 | 0.04 | 0.03 | 0.01 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 618.3299 | 9 | 144 | 1.10 | 0.05 | 0.03 | 0.01 | 0.00 | 64 |
| layer-cpp-highspy-tabu | 2 | Optimal | 618.3299 | 8 | 111 | 0.99 | 0.04 | 0.00 | 0.03 | 0.00 | 52 |
| layer-purepython-highspy-tabu | 2 | Optimal | 618.3299 | 8 | 111 | 1.06 | 0.04 | 0.00 | 0.03 | 0.00 | 64 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 618.3299 | 10 | 126 | 1.06 | 0.07 | 0.04 | 0.03 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 618.3299 | 10 | 126 | 1.05 | 0.07 | 0.04 | 0.03 | 0.00 | 63 |
| layer-cpp-highspy-exact | 3 | Optimal | 618.3299 | 10 | 147 | 1.13 | 0.01 | 0.00 | 0.01 | 0.00 | 50 |
| layer-purepython-highspy-exact | 3 | Optimal | 618.3299 | 10 | 147 | 1.05 | 0.02 | 0.01 | 0.01 | 0.00 | 64 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 618.3299 | 9 | 144 | 1.05 | 0.04 | 0.03 | 0.01 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 618.3299 | 9 | 144 | 1.05 | 0.05 | 0.03 | 0.01 | 0.00 | 62 |
| layer-cpp-highspy-tabu | 3 | Optimal | 618.3299 | 8 | 111 | 1.05 | 0.04 | 0.00 | 0.03 | 0.00 | 63 |
| layer-purepython-highspy-tabu | 3 | Optimal | 618.3299 | 8 | 111 | 1.13 | 0.05 | 0.01 | 0.05 | 0.00 | 64 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 618.3299 | 10 | 126 | 1.05 | 0.07 | 0.04 | 0.03 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 618.3299 | 10 | 126 | 1.05 | 0.07 | 0.04 | 0.03 | 0.00 | 63 |
| layer-cpp-highspy-exact | 4 | Optimal | 618.3299 | 10 | 147 | 1.14 | 0.01 | 0.00 | 0.01 | 0.00 | 63 |
| layer-purepython-highspy-exact | 4 | Optimal | 618.3299 | 10 | 147 | 1.05 | 0.02 | 0.01 | 0.01 | 0.00 | 64 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 618.3299 | 9 | 144 | 1.05 | 0.04 | 0.03 | 0.01 | 0.00 | 63 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 618.3299 | 9 | 144 | 1.05 | 0.05 | 0.03 | 0.01 | 0.00 | 63 |
| layer-cpp-highspy-tabu | 4 | Optimal | 618.3299 | 8 | 111 | 1.05 | 0.04 | 0.00 | 0.03 | 0.00 | 63 |
| layer-purepython-highspy-tabu | 4 | Optimal | 618.3299 | 8 | 111 | 1.11 | 0.04 | 0.00 | 0.04 | 0.00 | 64 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 618.3299 | 10 | 126 | 1.08* | 0.07* | 0.04 | 0.03 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 618.3299 | 10 | 126 | 1.05 | 0.07 | 0.04 | 0.04 | 0.00 | 63 |

## R201_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 449.6598 | 16 | 4185 | 1.47* | 0.59* | 0.11 | 0.48 | 0.00 | 70 |
| layer-purepython-highspy-exact | 0 | Optimal | 449.6598 | 16 | 4185 | 1.68 | 0.62 | 0.11 | 0.51 | 0.00 | 72 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 449.6598 | 16 | 4465 | 1.68 | 0.65 | 0.10 | 0.55 | 0.00 | 71 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 449.6598 | 16 | 4465 | 1.75 | 0.64 | 0.10 | 0.53 | 0.01 | 73 |
| layer-cpp-highspy-tabu | 0 | Optimal | 449.6598 | 27 | 836 | 1.26 | 0.41 | 0.02 | 0.38 | 0.00 | 65 |
| layer-purepython-highspy-tabu | 0 | Optimal | 449.6598 | 27 | 836 | 1.47 | 0.41 | 0.03 | 0.38 | 0.00 | 66 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 449.6598 | 26 | 965 | 1.51 | 0.49 | 0.10 | 0.39 | 0.00 | 64 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 449.6598 | 26 | 965 | 1.61* | 0.50* | 0.10 | 0.40 | 0.01 | 65 |
| layer-cpp-highspy-exact | 1 | Optimal | 449.6598 | 16 | 4185 | 1.52 | 0.57 | 0.11 | 0.46 | 0.00 | 70 |
| layer-purepython-highspy-exact | 1 | Optimal | 449.6598 | 16 | 4185 | 1.60* | 0.61* | 0.11 | 0.50 | 0.00 | 72 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 449.6598 | 16 | 4465 | 1.67 | 0.64 | 0.10 | 0.54 | 0.00 | 70 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 449.6598 | 16 | 4465 | 1.67 | 0.65 | 0.10 | 0.54 | 0.01 | 73 |
| layer-cpp-highspy-tabu | 1 | Optimal | 449.6598 | 27 | 836 | 1.33 | 0.40 | 0.02 | 0.37 | 0.00 | 65 |
| layer-purepython-highspy-tabu | 1 | Optimal | 449.6598 | 27 | 836 | 1.47 | 0.41 | 0.03 | 0.38 | 0.00 | 66 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 449.6598 | 26 | 965 | 1.46 | 0.51 | 0.10 | 0.40 | 0.00 | 64 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 449.6598 | 26 | 965 | 1.46 | 0.49 | 0.10 | 0.39 | 0.00 | 65 |
| layer-cpp-highspy-exact | 2 | Optimal | 449.6598 | 16 | 4185 | 1.54 | 0.60 | 0.12 | 0.48 | 0.00 | 70 |
| layer-purepython-highspy-exact | 2 | Optimal | 449.6598 | 16 | 4185 | 1.62* | 0.61* | 0.11 | 0.49 | 0.00 | 72 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 449.6598 | 16 | 4465 | 1.69 | 0.68 | 0.10 | 0.57 | 0.00 | 70 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 449.6598 | 16 | 4465 | 1.68 | 0.67 | 0.11 | 0.55 | 0.01 | 73 |
| layer-cpp-highspy-tabu | 2 | Optimal | 449.6598 | 27 | 836 | 1.38 | 0.40 | 0.02 | 0.38 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 2 | Optimal | 449.6598 | 27 | 836 | 1.47 | 0.41 | 0.03 | 0.38 | 0.00 | 66 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 449.6598 | 26 | 965 | 1.47 | 0.50 | 0.10 | 0.40 | 0.00 | 64 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 449.6598 | 26 | 965 | 1.47 | 0.50 | 0.10 | 0.39 | 0.00 | 66 |
| layer-cpp-highspy-exact | 3 | Optimal | 449.6598 | 16 | 4185 | 1.46 | 0.57 | 0.11 | 0.46 | 0.00 | 70 |
| layer-purepython-highspy-exact | 3 | Optimal | 449.6598 | 16 | 4185 | 1.74 | 0.62 | 0.12 | 0.50 | 0.00 | 72 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 449.6598 | 16 | 4465 | 1.70 | 0.64 | 0.10 | 0.54 | 0.00 | 71 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 449.6598 | 16 | 4465 | 1.68 | 0.65 | 0.10 | 0.54 | 0.01 | 73 |
| layer-cpp-highspy-tabu | 3 | Optimal | 449.6598 | 27 | 836 | 1.26 | 0.39 | 0.02 | 0.37 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 3 | Optimal | 449.6598 | 27 | 836 | 1.57* | 0.43* | 0.03 | 0.39 | 0.00 | 65 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 449.6598 | 26 | 965 | 1.47 | 0.48 | 0.10 | 0.38 | 0.00 | 64 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 449.6598 | 26 | 965 | 1.46 | 0.52 | 0.10 | 0.42 | 0.01 | 65 |
| layer-cpp-highspy-exact | 4 | Optimal | 449.6598 | 16 | 4185 | 1.47 | 0.59 | 0.11 | 0.48 | 0.00 | 70 |
| layer-purepython-highspy-exact | 4 | Optimal | 449.6598 | 16 | 4185 | 1.74 | 0.62 | 0.11 | 0.50 | 0.00 | 73 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 449.6598 | 16 | 4465 | 1.73 | 0.65 | 0.10 | 0.54 | 0.00 | 70 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 449.6598 | 16 | 4465 | 1.68 | 0.69 | 0.10 | 0.58 | 0.01 | 73 |
| layer-cpp-highspy-tabu | 4 | Optimal | 449.6598 | 27 | 836 | 1.26 | 0.38 | 0.02 | 0.36 | 0.00 | 65 |
| layer-purepython-highspy-tabu | 4 | Optimal | 449.6598 | 27 | 836 | 1.54 | 0.43 | 0.03 | 0.39 | 0.01 | 66 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 449.6598 | 26 | 965 | 1.47 | 0.49 | 0.10 | 0.39 | 0.00 | 64 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 449.6598 | 26 | 965 | 1.47 | 0.49 | 0.10 | 0.39 | 0.01 | 65 |

## RC201_12

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 427.2356 | 14 | 2553 | 1.05 | 0.18 | 0.04 | 0.13 | 0.00 | 66 |
| layer-purepython-highspy-exact | 0 | Optimal | 427.2356 | 14 | 2553 | 1.26 | 0.20 | 0.05 | 0.15 | 0.00 | 69 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 427.2356 | 20 | 2409 | 1.31 | 0.27 | 0.09 | 0.17 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 427.2356 | 20 | 2409 | 1.42* | 0.28* | 0.09 | 0.18 | 0.00 | 68 |
| layer-cpp-highspy-tabu | 0 | Optimal | 427.2356 | 21 | 509 | 1.26 | 0.24 | 0.01 | 0.23 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 0 | Optimal | 427.2356 | 21 | 509 | 1.26 | 0.26 | 0.01 | 0.24 | 0.00 | 65 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 427.2356 | 24 | 449 | 1.26 | 0.36 | 0.09 | 0.27 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 427.2356 | 24 | 449 | 1.34 | 0.37 | 0.09 | 0.28 | 0.00 | 64 |
| layer-cpp-highspy-exact | 1 | Optimal | 427.2356 | 14 | 2553 | 1.14 | 0.18 | 0.04 | 0.14 | 0.00 | 67 |
| layer-purepython-highspy-exact | 1 | Optimal | 427.2356 | 14 | 2553 | 1.26 | 0.20 | 0.05 | 0.15 | 0.00 | 69 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 427.2356 | 20 | 2409 | 1.26 | 0.27 | 0.09 | 0.18 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 427.2356 | 20 | 2409 | 1.25 | 0.27 | 0.09 | 0.18 | 0.00 | 69 |
| layer-cpp-highspy-tabu | 1 | Optimal | 427.2356 | 21 | 509 | 1.31 | 0.24 | 0.01 | 0.23 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 1 | Optimal | 427.2356 | 21 | 509 | 1.26 | 0.25 | 0.01 | 0.23 | 0.00 | 65 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 427.2356 | 24 | 449 | 1.25 | 0.36 | 0.09 | 0.27 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 427.2356 | 24 | 449 | 1.25 | 0.37 | 0.09 | 0.28 | 0.00 | 64 |
| layer-cpp-highspy-exact | 2 | Optimal | 427.2356 | 14 | 2553 | 1.16* | 0.19* | 0.05 | 0.14 | 0.00 | 66 |
| layer-purepython-highspy-exact | 2 | Optimal | 427.2356 | 14 | 2553 | 1.26 | 0.21 | 0.05 | 0.16 | 0.00 | 69 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 427.2356 | 20 | 2409 | 1.26 | 0.27 | 0.09 | 0.18 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 427.2356 | 20 | 2409 | 1.26 | 0.27 | 0.09 | 0.18 | 0.00 | 68 |
| layer-cpp-highspy-tabu | 2 | Optimal | 427.2356 | 21 | 509 | 1.31 | 0.24 | 0.01 | 0.23 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 2 | Optimal | 427.2356 | 21 | 509 | 1.41* | 0.26* | 0.01 | 0.24 | 0.00 | 65 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 427.2356 | 24 | 449 | 1.26 | 0.38 | 0.09 | 0.29 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 427.2356 | 24 | 449 | 1.47 | 0.36 | 0.08 | 0.27 | 0.00 | 64 |
| layer-cpp-highspy-exact | 3 | Optimal | 427.2356 | 14 | 2553 | 1.05 | 0.19 | 0.04 | 0.14 | 0.00 | 65 |
| layer-purepython-highspy-exact | 3 | Optimal | 427.2356 | 14 | 2553 | 1.33 | 0.24 | 0.05 | 0.19 | 0.00 | 69 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 427.2356 | 20 | 2409 | 1.26 | 0.26 | 0.09 | 0.17 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 427.2356 | 20 | 2409 | 1.26 | 0.27 | 0.09 | 0.18 | 0.00 | 68 |
| layer-cpp-highspy-tabu | 3 | Optimal | 427.2356 | 21 | 509 | 1.26 | 0.24 | 0.01 | 0.23 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 3 | Optimal | 427.2356 | 21 | 509 | 1.30 | 0.26 | 0.01 | 0.24 | 0.00 | 65 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 427.2356 | 24 | 449 | 1.41* | 0.36* | 0.09 | 0.27 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 427.2356 | 24 | 449 | 1.47 | 0.39 | 0.09 | 0.30 | 0.00 | 64 |
| layer-cpp-highspy-exact | 4 | Optimal | 427.2356 | 14 | 2553 | 1.05 | 0.18 | 0.04 | 0.13 | 0.00 | 66 |
| layer-purepython-highspy-exact | 4 | Optimal | 427.2356 | 14 | 2553 | 1.32 | 0.21 | 0.05 | 0.16 | 0.00 | 69 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 427.2356 | 20 | 2409 | 1.26 | 0.26 | 0.09 | 0.17 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 427.2356 | 20 | 2409 | 1.26 | 0.27 | 0.09 | 0.18 | 0.00 | 68 |
| layer-cpp-highspy-tabu | 4 | Optimal | 427.2356 | 21 | 509 | 1.26 | 0.24 | 0.01 | 0.23 | 0.00 | 64 |
| layer-purepython-highspy-tabu | 4 | Optimal | 427.2356 | 21 | 509 | 1.28 | 0.26 | 0.01 | 0.25 | 0.00 | 65 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 427.2356 | 24 | 449 | 1.43* | 0.35* | 0.08 | 0.27 | 0.00 | 63 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 427.2356 | 24 | 449 | 1.46 | 0.37 | 0.09 | 0.28 | 0.00 | 64 |

## C101_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 363.2468 | 31 | 1357 | 1.05 | 0.17 | 0.04 | 0.13 | 0.00 | 68 |
| layer-purepython-highspy-exact | 0 | Optimal | 363.2468 | 31 | 1357 | 1.26 | 0.21 | 0.04 | 0.16 | 0.01 | 70 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 363.2468 | 26 | 875 | 1.26 | 0.24 | 0.10 | 0.13 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 363.2468 | 26 | 875 | 1.33 | 0.27 | 0.11 | 0.15 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 0 | Optimal | 363.2468 | 94 | 697 | 3.34 | 2.40 | 0.07 | 2.32 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 0 | Optimal | 363.2468 | 94 | 697 | 3.42 | 2.46 | 0.08 | 2.36 | 0.02 | 69 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 363.2468 | 107 | 900 | 4.45 | 3.39 | 0.42 | 2.95 | 0.01 | 67 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 363.2468 | 107 | 900 | 4.39 | 3.44 | 0.42 | 3.01 | 0.02 | 68 |
| layer-cpp-highspy-exact | 1 | Optimal | 363.2468 | 31 | 1357 | 1.05 | 0.19 | 0.04 | 0.15 | 0.00 | 68 |
| layer-purepython-highspy-exact | 1 | Optimal | 363.2468 | 31 | 1357 | 1.33* | 0.21* | 0.04 | 0.15 | 0.01 | 69 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 363.2468 | 26 | 875 | 1.25 | 0.23 | 0.10 | 0.12 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 363.2468 | 26 | 875 | 1.25 | 0.24 | 0.10 | 0.13 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 1 | Optimal | 363.2468 | 94 | 697 | 3.39 | 2.48 | 0.07 | 2.41 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 1 | Optimal | 363.2468 | 94 | 697 | 3.77 | 2.72 | 0.09 | 2.61 | 0.02 | 69 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 363.2468 | 107 | 900 | 5.86* | 4.67* | 0.51 | 4.14 | 0.02 | 68 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 363.2468 | 107 | 900 | 5.88* | 4.35* | 0.48 | 3.85 | 0.03 | 69 |
| layer-cpp-highspy-exact | 2 | Optimal | 363.2468 | 31 | 1357 | 1.09 | 0.18 | 0.04 | 0.14 | 0.00 | 68 |
| layer-purepython-highspy-exact | 2 | Optimal | 363.2468 | 31 | 1357 | 1.20* | 0.20* | 0.04 | 0.15 | 0.01 | 69 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 363.2468 | 26 | 875 | 1.47 | 0.52 | 0.38 | 0.13 | 0.00 | 70 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 363.2468 | 26 | 875 | 1.26 | 0.26 | 0.11 | 0.14 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 2 | Optimal | 363.2468 | 94 | 697 | 3.41* | 2.39* | 0.07 | 2.32 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 2 | Optimal | 363.2468 | 94 | 697 | 3.57 | 2.50 | 0.09 | 2.40 | 0.02 | 69 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 363.2468 | 107 | 900 | 4.47* | 3.36* | 0.42 | 2.93 | 0.01 | 67 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 363.2468 | 107 | 900 | 4.44 | 3.42 | 0.41 | 2.99 | 0.02 | 69 |
| layer-cpp-highspy-exact | 3 | Optimal | 363.2468 | 31 | 1357 | 1.05 | 0.17 | 0.04 | 0.13 | 0.00 | 67 |
| layer-purepython-highspy-exact | 3 | Optimal | 363.2468 | 31 | 1357 | 1.26 | 0.21 | 0.04 | 0.16 | 0.01 | 69 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 363.2468 | 26 | 875 | 1.23 | 0.23 | 0.10 | 0.13 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 363.2468 | 26 | 875 | 1.26 | 0.24 | 0.10 | 0.13 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 3 | Optimal | 363.2468 | 94 | 697 | 3.43 | 2.51 | 0.08 | 2.43 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 3 | Optimal | 363.2468 | 94 | 697 | 3.56 | 2.45 | 0.08 | 2.35 | 0.02 | 69 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 363.2468 | 107 | 900 | 4.47 | 3.39 | 0.42 | 2.96 | 0.01 | 67 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 363.2468 | 107 | 900 | 4.50 | 3.45 | 0.42 | 3.02 | 0.02 | 68 |
| layer-cpp-highspy-exact | 4 | Optimal | 363.2468 | 31 | 1357 | 1.05 | 0.18 | 0.04 | 0.14 | 0.00 | 68 |
| layer-purepython-highspy-exact | 4 | Optimal | 363.2468 | 31 | 1357 | 1.26 | 0.20 | 0.04 | 0.15 | 0.01 | 70 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 363.2468 | 26 | 875 | 1.33* | 0.23* | 0.10 | 0.12 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 363.2468 | 26 | 875 | 1.26 | 0.24 | 0.10 | 0.13 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 4 | Optimal | 363.2468 | 94 | 697 | 3.39 | 2.41 | 0.07 | 2.34 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 4 | Optimal | 363.2468 | 94 | 697 | 3.51 | 2.46 | 0.08 | 2.36 | 0.02 | 69 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 363.2468 | 107 | 900 | 4.46 | 3.33 | 0.41 | 2.91 | 0.01 | 67 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 363.2468 | 107 | 900 | 4.70 | 3.59 | 0.43 | 3.15 | 0.02 | 68 |

## R101_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 1046.7011 | 20 | 456 | 1.15 | 0.08 | 0.02 | 0.06 | 0.00 | 68 |
| layer-purepython-highspy-exact | 0 | Optimal | 1046.7011 | 20 | 456 | 1.05 | 0.09 | 0.02 | 0.07 | 0.01 | 68 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 1046.7011 | 21 | 534 | 1.05 | 0.16 | 0.08 | 0.07 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 1046.7011 | 21 | 534 | 1.26 | 0.16 | 0.08 | 0.07 | 0.01 | 67 |
| layer-cpp-highspy-tabu | 0 | Optimal | 1046.7011 | 22 | 343 | 1.05 | 0.20 | 0.01 | 0.18 | 0.00 | 67 |
| layer-purepython-highspy-tabu | 0 | Optimal | 1046.7011 | 22 | 343 | 1.33 | 0.23 | 0.02 | 0.20 | 0.01 | 68 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 1046.7011 | 26 | 362 | 1.26 | 0.28 | 0.09 | 0.18 | 0.00 | 66 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 1046.7011 | 26 | 362 | 1.26 | 0.28 | 0.09 | 0.18 | 0.01 | 67 |
| layer-cpp-highspy-exact | 1 | Optimal | 1046.7011 | 20 | 456 | 1.33 | 0.08 | 0.02 | 0.06 | 0.00 | 68 |
| layer-purepython-highspy-exact | 1 | Optimal | 1046.7011 | 20 | 456 | 1.26* | 0.12* | 0.02 | 0.09 | 0.01 | 67 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 1046.7011 | 21 | 534 | 1.28 | 0.18 | 0.09 | 0.09 | 0.00 | 66 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 1046.7011 | 21 | 534 | 1.27 | 0.17 | 0.08 | 0.08 | 0.01 | 67 |
| layer-cpp-highspy-tabu | 1 | Optimal | 1046.7011 | 22 | 343 | 1.26 | 0.22 | 0.02 | 0.21 | 0.00 | 67 |
| layer-purepython-highspy-tabu | 1 | Optimal | 1046.7011 | 22 | 343 | 1.60 | 0.26 | 0.02 | 0.23 | 0.01 | 68 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 1046.7011 | 26 | 362 | 1.48 | 0.33 | 0.10 | 0.22 | 0.00 | 66 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 1046.7011 | 26 | 362 | 1.70 | 0.36 | 0.11 | 0.24 | 0.01 | 67 |
| layer-cpp-highspy-exact | 2 | Optimal | 1046.7011 | 20 | 456 | 1.05 | 0.07 | 0.02 | 0.05 | 0.00 | 67 |
| layer-purepython-highspy-exact | 2 | Optimal | 1046.7011 | 20 | 456 | 1.06 | 0.09 | 0.02 | 0.06 | 0.01 | 62 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 1046.7011 | 21 | 534 | 1.10 | 0.16 | 0.08 | 0.07 | 0.00 | 66 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 1046.7011 | 21 | 534 | 1.33 | 0.16 | 0.08 | 0.07 | 0.01 | 67 |
| layer-cpp-highspy-tabu | 2 | Optimal | 1046.7011 | 22 | 343 | 1.26 | 0.19 | 0.02 | 0.18 | 0.00 | 67 |
| layer-purepython-highspy-tabu | 2 | Optimal | 1046.7011 | 22 | 343 | 1.26 | 0.21 | 0.02 | 0.19 | 0.01 | 68 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 1046.7011 | 26 | 362 | 1.31 | 0.28 | 0.09 | 0.18 | 0.00 | 66 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 1046.7011 | 26 | 362 | 1.44* | 0.30* | 0.10 | 0.20 | 0.01 | 67 |
| layer-cpp-highspy-exact | 3 | Optimal | 1046.7011 | 20 | 456 | 1.05 | 0.07 | 0.02 | 0.05 | 0.00 | 67 |
| layer-purepython-highspy-exact | 3 | Optimal | 1046.7011 | 20 | 456 | 1.05 | 0.09 | 0.02 | 0.07 | 0.01 | 64 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 1046.7011 | 21 | 534 | 1.26 | 0.15 | 0.08 | 0.07 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 1046.7011 | 21 | 534 | 1.34* | 0.17* | 0.08 | 0.08 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 3 | Optimal | 1046.7011 | 22 | 343 | 1.26 | 0.20 | 0.02 | 0.19 | 0.00 | 67 |
| layer-purepython-highspy-tabu | 3 | Optimal | 1046.7011 | 22 | 343 | 1.27 | 0.21 | 0.02 | 0.19 | 0.01 | 68 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 1046.7011 | 26 | 362 | 1.26 | 0.28 | 0.09 | 0.18 | 0.00 | 66 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 1046.7011 | 26 | 362 | 1.29 | 0.29 | 0.09 | 0.19 | 0.01 | 67 |
| layer-cpp-highspy-exact | 4 | Optimal | 1046.7011 | 20 | 456 | 1.05 | 0.07 | 0.02 | 0.05 | 0.00 | 67 |
| layer-purepython-highspy-exact | 4 | Optimal | 1046.7011 | 20 | 456 | 1.05 | 0.09 | 0.02 | 0.06 | 0.01 | 63 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 1046.7011 | 21 | 534 | 1.26 | 0.15 | 0.08 | 0.07 | 0.00 | 66 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 1046.7011 | 21 | 534 | 1.35 | 0.18 | 0.08 | 0.08 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 4 | Optimal | 1046.7011 | 22 | 343 | 1.26 | 0.20 | 0.01 | 0.18 | 0.00 | 67 |
| layer-purepython-highspy-tabu | 4 | Optimal | 1046.7011 | 22 | 343 | 1.26 | 0.23 | 0.02 | 0.20 | 0.01 | 68 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 1046.7011 | 26 | 362 | 1.26 | 0.27 | 0.09 | 0.18 | 0.00 | 66 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 1046.7011 | 26 | 362 | 1.29 | 0.30 | 0.10 | 0.19 | 0.01 | 67 |

## RC101_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 828.3619 | 27 | 760 | 1.06 | 0.12 | 0.02 | 0.10 | 0.00 | 68 |
| layer-purepython-highspy-exact | 0 | Optimal | 828.3619 | 27 | 760 | 1.05 | 0.14 | 0.02 | 0.11 | 0.01 | 68 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 828.3619 | 27 | 748 | 1.34* | 0.22* | 0.10 | 0.12 | 0.00 | 66 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 828.3619 | 27 | 748 | 1.26 | 0.22 | 0.10 | 0.11 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 0 | Optimal | 828.3619 | 42 | 610 | 2.09 | 1.17 | 0.03 | 1.14 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 0 | Optimal | 828.3619 | 42 | 610 | 2.15* | 1.25* | 0.03 | 1.21 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 828.3619 | 40 | 561 | 2.30 | 1.27 | 0.15 | 1.12 | 0.00 | 66 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 828.3619 | 40 | 561 | 2.30 | 1.30 | 0.15 | 1.15 | 0.01 | 67 |
| layer-cpp-highspy-exact | 1 | Optimal | 828.3619 | 27 | 760 | 1.72 | 0.19 | 0.03 | 0.16 | 0.00 | 68 |
| layer-purepython-highspy-exact | 1 | Optimal | 828.3619 | 27 | 760 | 1.75* | 0.21* | 0.03 | 0.16 | 0.01 | 69 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 828.3619 | 27 | 748 | 1.71 | 0.31 | 0.13 | 0.18 | 0.01 | 67 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 828.3619 | 27 | 748 | 1.71 | 0.27 | 0.11 | 0.15 | 0.01 | 67 |
| layer-cpp-highspy-tabu | 1 | Optimal | 828.3619 | 42 | 610 | 4.44* | 1.56* | 0.03 | 1.53 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 1 | Optimal | 828.3619 | 42 | 610 | 2.55 | 1.37 | 0.04 | 1.32 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 828.3619 | 40 | 561 | 2.87* | 1.43* | 0.16 | 1.27 | 0.01 | 66 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 828.3619 | 40 | 561 | 2.76 | 1.50 | 0.16 | 1.33 | 0.01 | 67 |
| layer-cpp-highspy-exact | 2 | Optimal | 828.3619 | 27 | 760 | 1.05 | 0.11 | 0.02 | 0.09 | 0.00 | 68 |
| layer-purepython-highspy-exact | 2 | Optimal | 828.3619 | 27 | 760 | 1.26 | 0.15 | 0.02 | 0.11 | 0.01 | 67 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 828.3619 | 27 | 748 | 1.26 | 0.21 | 0.10 | 0.11 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 828.3619 | 27 | 748 | 1.37* | 0.23* | 0.10 | 0.12 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 2 | Optimal | 828.3619 | 42 | 610 | 2.09 | 1.18 | 0.03 | 1.15 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 2 | Optimal | 828.3619 | 42 | 610 | 2.30 | 1.23 | 0.03 | 1.18 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 828.3619 | 40 | 561 | 2.39* | 1.28* | 0.15 | 1.13 | 0.00 | 66 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 828.3619 | 40 | 561 | 2.30 | 1.29 | 0.15 | 1.13 | 0.01 | 67 |
| layer-cpp-highspy-exact | 3 | Optimal | 828.3619 | 27 | 760 | 1.21* | 0.11* | 0.02 | 0.09 | 0.00 | 68 |
| layer-purepython-highspy-exact | 3 | Optimal | 828.3619 | 27 | 760 | 1.26 | 0.15 | 0.02 | 0.11 | 0.01 | 69 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 828.3619 | 27 | 748 | 1.26 | 0.22 | 0.10 | 0.12 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 828.3619 | 27 | 748 | 1.27 | 0.23 | 0.10 | 0.12 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 3 | Optimal | 828.3619 | 42 | 610 | 2.38* | 1.23* | 0.03 | 1.20 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 3 | Optimal | 828.3619 | 42 | 610 | 2.30 | 1.24 | 0.03 | 1.19 | 0.01 | 68 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 828.3619 | 40 | 561 | 2.37 | 1.30 | 0.15 | 1.14 | 0.00 | 66 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 828.3619 | 40 | 561 | 2.31 | 1.30 | 0.15 | 1.14 | 0.01 | 67 |
| layer-cpp-highspy-exact | 4 | Optimal | 828.3619 | 27 | 760 | 1.19* | 0.12* | 0.02 | 0.10 | 0.00 | 68 |
| layer-purepython-highspy-exact | 4 | Optimal | 828.3619 | 27 | 760 | 1.05 | 0.14 | 0.02 | 0.11 | 0.01 | 68 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 828.3619 | 27 | 748 | 1.26 | 0.22 | 0.10 | 0.12 | 0.00 | 67 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 828.3619 | 27 | 748 | 1.26 | 0.24 | 0.10 | 0.13 | 0.01 | 68 |
| layer-cpp-highspy-tabu | 4 | Optimal | 828.3619 | 42 | 610 | 2.19* | 1.20* | 0.03 | 1.17 | 0.00 | 68 |
| layer-purepython-highspy-tabu | 4 | Optimal | 828.3619 | 42 | 610 | 2.30 | 1.22 | 0.03 | 1.18 | 0.01 | 69 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 828.3619 | 40 | 561 | 2.37 | 1.31 | 0.15 | 1.15 | 0.00 | 66 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 828.3619 | 40 | 561 | 2.30 | 1.29 | 0.15 | 1.14 | 0.01 | 67 |

## R201_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 756.3386 | 33 | 17929 | 21.84 | 20.94 | 1.96 | 18.98 | 0.00 | 99 |
| layer-purepython-highspy-exact | 0 | Optimal | 756.3386 | 33 | 17929 | 22.50 | 21.42 | 1.98 | 19.42 | 0.01 | 102 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 756.3386 | 26 | 17936 | 21.28 | 20.37 | 0.54 | 19.81 | 0.01 | 102 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 756.3386 | 26 | 17936 | 22.57 | 21.54 | 0.56 | 20.96 | 0.02 | 104 |
| layer-cpp-highspy-tabu | 0 | Optimal | 756.3386 | 44 | 5072 | 3.97 | 3.09 | 0.32 | 2.77 | 0.00 | 75 |
| layer-purepython-highspy-tabu | 0 | Optimal | 756.3386 | 44 | 5072 | 4.25 | 3.31 | 0.34 | 2.95 | 0.01 | 76 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 756.3386 | 51 | 4717 | 4.47 | 3.57 | 0.58 | 2.99 | 0.01 | 76 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 756.3386 | 51 | 4717 | 4.46 | 3.49 | 0.31 | 3.17 | 0.02 | 75 |
| layer-cpp-highspy-exact | 1 | Optimal | 756.3386 | 33 | 17929 | 30.53* | 29.31* | 2.05 | 27.26 | 0.00 | 98 |
| layer-purepython-highspy-exact | 1 | Optimal | 756.3386 | 33 | 17929 | 23.65* | 22.52* | 2.04 | 20.46 | 0.01 | 101 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 756.3386 | 26 | 17936 | 22.25 | 21.07 | 0.52 | 20.53 | 0.01 | 102 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 756.3386 | 26 | 17936 | 22.11 | 21.02 | 0.55 | 20.45 | 0.02 | 104 |
| layer-cpp-highspy-tabu | 1 | Optimal | 756.3386 | 44 | 5072 | 3.98 | 3.10 | 0.33 | 2.78 | 0.00 | 75 |
| layer-purepython-highspy-tabu | 1 | Optimal | 756.3386 | 44 | 5072 | 4.32 | 3.22 | 0.34 | 2.87 | 0.01 | 76 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 756.3386 | 51 | 4717 | 4.47 | 3.40 | 0.31 | 3.08 | 0.01 | 73 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 756.3386 | 51 | 4717 | 4.50 | 3.44 | 0.31 | 3.11 | 0.02 | 75 |
| layer-cpp-highspy-exact | 2 | Optimal | 756.3386 | 33 | 17929 | 22.38 | 21.40 | 1.96 | 19.44 | 0.00 | 99 |
| layer-purepython-highspy-exact | 2 | Optimal | 756.3386 | 33 | 17929 | 22.52 | 21.56 | 1.99 | 19.56 | 0.01 | 102 |
| layer-cpp-gurobipy-exact | 2 | Optimal | 756.3386 | 26 | 17936 | 21.77 | 20.67 | 0.53 | 20.13 | 0.01 | 103 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 756.3386 | 26 | 17936 | 21.76 | 20.62 | 0.54 | 20.06 | 0.02 | 104 |
| layer-cpp-highspy-tabu | 2 | Optimal | 756.3386 | 44 | 5072 | 4.24 | 3.17 | 0.33 | 2.84 | 0.00 | 75 |
| layer-purepython-highspy-tabu | 2 | Optimal | 756.3386 | 44 | 5072 | 4.49 | 3.45 | 0.35 | 3.09 | 0.01 | 76 |
| layer-cpp-gurobipy-tabu | 2 | Optimal | 756.3386 | 51 | 4717 | 4.19 | 3.26 | 0.30 | 2.95 | 0.01 | 73 |
| layer-purepython-gurobipy-tabu | 2 | Optimal | 756.3386 | 51 | 4717 | 4.46 | 3.34 | 0.31 | 3.02 | 0.02 | 75 |
| layer-cpp-highspy-exact | 3 | Optimal | 756.3386 | 33 | 17929 | 22.55 | 21.55 | 1.98 | 19.57 | 0.00 | 99 |
| layer-purepython-highspy-exact | 3 | Optimal | 756.3386 | 33 | 17929 | 22.50 | 21.54 | 2.00 | 19.52 | 0.01 | 102 |
| layer-cpp-gurobipy-exact | 3 | Optimal | 756.3386 | 26 | 17936 | 22.20 | 21.03 | 0.82 | 20.20 | 0.01 | 105 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 756.3386 | 26 | 17936 | 21.78 | 20.68 | 0.53 | 20.13 | 0.02 | 105 |
| layer-cpp-highspy-tabu | 3 | Optimal | 756.3386 | 44 | 5072 | 4.05 | 3.14 | 0.33 | 2.82 | 0.00 | 75 |
| layer-purepython-highspy-tabu | 3 | Optimal | 756.3386 | 44 | 5072 | 4.26 | 3.30 | 0.35 | 2.94 | 0.01 | 76 |
| layer-cpp-gurobipy-tabu | 3 | Optimal | 756.3386 | 51 | 4717 | 4.19 | 3.23 | 0.30 | 2.93 | 0.01 | 73 |
| layer-purepython-gurobipy-tabu | 3 | Optimal | 756.3386 | 51 | 4717 | 4.47 | 3.29 | 0.30 | 2.98 | 0.01 | 75 |
| layer-cpp-highspy-exact | 4 | Optimal | 756.3386 | 33 | 17929 | 22.42 | 21.32 | 2.02 | 19.30 | 0.00 | 99 |
| layer-purepython-highspy-exact | 4 | Optimal | 756.3386 | 33 | 17929 | 22.79 | 21.78 | 2.01 | 19.75 | 0.01 | 102 |
| layer-cpp-gurobipy-exact | 4 | Optimal | 756.3386 | 26 | 17936 | 21.69 | 20.70 | 0.53 | 20.16 | 0.01 | 102 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 756.3386 | 26 | 17936 | 21.70 | 20.61 | 0.54 | 20.05 | 0.02 | 104 |
| layer-cpp-highspy-tabu | 4 | Optimal | 756.3386 | 44 | 5072 | 4.22 | 3.16 | 0.34 | 2.82 | 0.00 | 75 |
| layer-purepython-highspy-tabu | 4 | Optimal | 756.3386 | 44 | 5072 | 4.24 | 3.19 | 0.33 | 2.85 | 0.01 | 77 |
| layer-cpp-gurobipy-tabu | 4 | Optimal | 756.3386 | 51 | 4717 | 4.19 | 3.25 | 0.30 | 2.94 | 0.01 | 73 |
| layer-purepython-gurobipy-tabu | 4 | Optimal | 756.3386 | 51 | 4717 | 4.26 | 3.30 | 0.30 | 2.99 | 0.01 | 76 |

## By configuration (mean wall s)

| config | C101_25 | R101_25 | R201_25 | RC201_12 | C101_50 | R101_50 | RC101_50 | R201_50 |
|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 1.05 | 1.00 | 1.49 | 1.09 | 1.06 | 1.13 | 1.25 | 23.94 |
| layer-cpp-gurobipy-exact | 1.12 | 1.06 | 1.69 | 1.27 | 1.31 | 1.19 | 1.36 | 21.84 |
| layer-purepython-highspy-exact | 1.12 | 1.05 | 1.68 | 1.29 | 1.26 | 1.09 | 1.27 | 22.79 |
| layer-purepython-highspy-tabu | 1.95 | 1.08 | 1.50 | 1.30 | 3.57 | 1.34 | 2.32 | 4.31 |
| layer-cpp-highspy-tabu | 1.82 | 1.03 | 1.30 | 1.28 | 3.39 | 1.22 | 2.64 | 4.09 |
| layer-purepython-gurobipy-exact | 1.22 | 1.09 | 1.69 | 1.29 | 1.27 | 1.31 | 1.37 | 21.99 |
| layer-purepython-gurobipy-tabu | 2.13 | 1.06 | 1.50 | 1.40 | 4.78 | 1.40 | 2.39 | 4.43 |
| layer-cpp-gurobipy-tabu | 2.16 | 1.06 | 1.48 | 1.33 | 4.74 | 1.31 | 2.46 | 4.30 |

## By configuration (mean solve s)

| config | C101_25 | R101_25 | R201_25 | RC201_12 | C101_50 | R101_50 | RC101_50 | R201_50 |
|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0.04 | 0.01 | 0.59 | 0.18 | 0.18 | 0.08 | 0.13 | 22.90 |
| layer-cpp-gurobipy-exact | 0.13 | 0.04 | 0.65 | 0.27 | 0.29 | 0.16 | 0.24 | 20.77 |
| layer-purepython-highspy-exact | 0.05 | 0.02 | 0.62 | 0.21 | 0.21 | 0.10 | 0.16 | 21.76 |
| layer-purepython-highspy-tabu | 0.90 | 0.05 | 0.42 | 0.26 | 2.52 | 0.23 | 1.26 | 3.29 |
| layer-cpp-highspy-tabu | 0.85 | 0.04 | 0.40 | 0.24 | 2.44 | 0.20 | 1.27 | 3.13 |
| layer-purepython-gurobipy-exact | 0.14 | 0.05 | 0.66 | 0.27 | 0.25 | 0.17 | 0.24 | 20.89 |
| layer-purepython-gurobipy-tabu | 1.11 | 0.07 | 0.50 | 0.37 | 3.65 | 0.31 | 1.34 | 3.37 |
| layer-cpp-gurobipy-tabu | 1.09 | 0.07 | 0.49 | 0.36 | 3.63 | 0.29 | 1.32 | 3.34 |

## LP mismatches

none
