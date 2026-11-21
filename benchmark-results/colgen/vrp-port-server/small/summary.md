# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 191.8136 | 21 | 646 | 0.83 | 0.22 | 0.11 | 0.03 | 0.06 | 69 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 191.8136 | 21 | 709 | 0.83 | 0.10 | 0.03 | 0.07 | 0.01 | 65 |
| example-mip | 1 | optimal | 191.8136 | 21 | 646 | 0.83 | 0.22 | 0.11 | 0.03 | 0.06 | 69 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 191.8136 | 21 | 709 | 0.83 | 0.10 | 0.03 | 0.07 | 0.00 | 68 |
| example-mip | 2 | optimal | 191.8136 | 21 | 646 | 0.83 | 0.23 | 0.11 | 0.03 | 0.06 | 69 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 191.8136 | 21 | 709 | 0.83 | 0.10 | 0.03 | 0.07 | 0.00 | 69 |
| example-mip | 3 | optimal | 191.8136 | 21 | 646 | 0.83 | 0.22 | 0.11 | 0.03 | 0.06 | 69 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 191.8136 | 21 | 709 | 0.83 | 0.10 | 0.03 | 0.07 | 0.00 | 65 |
| example-mip | 4 | optimal | 191.8136 | 21 | 646 | 0.83 | 0.22 | 0.10 | 0.03 | 0.06 | 69 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 191.8136 | 21 | 709 | 0.83 | 0.10 | 0.03 | 0.07 | 0.00 | 69 |

## R101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 618.3299 | 9 | 97 | 0.62 | 0.08 | 0.06 | 0.01 | 0.01 | 56 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 618.3299 | 6 | 90 | 0.63 | 0.02 | 0.01 | 0.01 | 0.00 | 65 |
| example-mip | 1 | optimal | 618.3299 | 9 | 97 | 0.62 | 0.08 | 0.06 | 0.01 | 0.01 | 56 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 618.3299 | 6 | 90 | 0.62 | 0.02 | 0.01 | 0.01 | 0.00 | 64 |
| example-mip | 2 | optimal | 618.3299 | 9 | 97 | 0.62 | 0.08 | 0.06 | 0.01 | 0.01 | 56 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 618.3299 | 6 | 90 | 0.62 | 0.02 | 0.01 | 0.01 | 0.00 | 64 |
| example-mip | 3 | optimal | 618.3299 | 9 | 97 | 0.64 | 0.08 | 0.06 | 0.01 | 0.01 | 56 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 618.3299 | 6 | 90 | 0.63 | 0.02 | 0.01 | 0.01 | 0.00 | 65 |
| example-mip | 4 | optimal | 618.3299 | 9 | 97 | 0.63 | 0.08 | 0.06 | 0.01 | 0.01 | 56 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 618.3299 | 6 | 90 | 0.62 | 0.02 | 0.01 | 0.01 | 0.00 | 61 |

## R201_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 449.6598 | 16 | 2991 | 2.06 | 1.49 | 0.19 | 1.06 | 0.22 | 77 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 449.6598 | 15 | 3239 | 1.86 | 1.24 | 0.06 | 1.17 | 0.01 | 78 |
| example-mip | 1 | optimal | 449.6598 | 16 | 2991 | 2.06 | 1.50 | 0.20 | 1.07 | 0.21 | 77 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 449.6598 | 15 | 3239 | 1.86 | 1.19 | 0.06 | 1.13 | 0.01 | 78 |
| example-mip | 2 | optimal | 449.6598 | 16 | 2991 | 2.06 | 1.48 | 0.19 | 1.05 | 0.21 | 79 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 449.6598 | 15 | 3239 | 1.87 | 1.23 | 0.06 | 1.16 | 0.01 | 78 |
| example-mip | 3 | optimal | 449.6598 | 16 | 2991 | 2.06 | 1.39 | 0.19 | 0.97 | 0.21 | 79 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 449.6598 | 15 | 3239 | 1.86 | 1.24 | 0.06 | 1.17 | 0.01 | 78 |
| example-mip | 4 | optimal | 449.6598 | 16 | 2991 | 2.08 | 1.42 | 0.19 | 1.00 | 0.21 | 79 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 449.6598 | 15 | 3239 | 1.85 | 1.21 | 0.06 | 1.14 | 0.01 | 78 |

## RC201_12

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 427.2356 | 18 | 1076 | 1.03 | 0.32 | 0.12 | 0.10 | 0.08 | 68 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 427.2356 | 17 | 1047 | 0.83 | 0.17 | 0.03 | 0.14 | 0.00 | 71 |
| example-mip | 1 | optimal | 427.2356 | 18 | 1076 | 0.83 | 0.32 | 0.12 | 0.10 | 0.08 | 69 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 427.2356 | 17 | 1047 | 0.83 | 0.17 | 0.03 | 0.14 | 0.00 | 71 |
| example-mip | 2 | optimal | 427.2356 | 18 | 1076 | 1.03 | 0.32 | 0.12 | 0.10 | 0.09 | 68 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 427.2356 | 17 | 1047 | 0.83 | 0.17 | 0.03 | 0.14 | 0.00 | 71 |
| example-mip | 3 | optimal | 427.2356 | 18 | 1076 | 0.84 | 0.33 | 0.12 | 0.11 | 0.08 | 69 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 427.2356 | 17 | 1047 | 0.82 | 0.17 | 0.03 | 0.14 | 0.00 | 71 |
| example-mip | 4 | optimal | 427.2356 | 18 | 1076 | 0.83 | 0.32 | 0.12 | 0.10 | 0.09 | 69 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 427.2356 | 17 | 1047 | 0.82 | 0.17 | 0.03 | 0.14 | 0.00 | 71 |

## By configuration (mean wall s)

| config | C101_25 | R101_25 | R201_25 | RC201_12 |
|---|---|---|---|---|
| example-mip | 0.83 | 0.63 | 2.06 | 0.91 |
| layer-purepython-gurobipy-exact | 0.83 | 0.62 | 1.86 | 0.83 |

## By configuration (mean solve s)

| config | C101_25 | R101_25 | R201_25 | RC201_12 |
|---|---|---|---|---|
| example-mip | 0.22 | 0.08 | 1.46 | 0.32 |
| layer-purepython-gurobipy-exact | 0.10 | 0.02 | 1.22 | 0.17 |

## LP mismatches

none
