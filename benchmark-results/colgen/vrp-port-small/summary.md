# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 191.8136 | 24 | 722 | 1.69 | 0.28 | 0.19 | 0.02 | 0.03 | 65 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 191.8136 | 29 | 723 | 0.85 | 0.07 | 0.03 | 0.05 | 0.00 | 58 |
| example-mip | 1 | optimal | 191.8136 | 24 | 722 | 0.84 | 0.12 | 0.06 | 0.02 | 0.02 | 69 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 191.8136 | 29 | 723 | 0.63 | 0.06 | 0.01 | 0.04 | 0.00 | 50 |
| example-mip | 2 | optimal | 191.8136 | 24 | 722 | 0.84 | 0.12 | 0.06 | 0.02 | 0.02 | 70 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 191.8136 | 29 | 723 | 0.92* | 0.07* | 0.01 | 0.05 | 0.00 | 64 |
| example-mip | 3 | optimal | 191.8136 | 24 | 722 | 0.84 | 0.12 | 0.06 | 0.02 | 0.02 | 70 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 191.8136 | 29 | 723 | 0.63 | 0.06 | 0.01 | 0.04 | 0.00 | 50 |
| example-mip | 4 | optimal | 191.8136 | 24 | 722 | 0.84 | 0.12 | 0.06 | 0.02 | 0.02 | 70 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 191.8136 | 29 | 723 | 0.63 | 0.06 | 0.01 | 0.04 | 0.00 | 50 |

## R101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 618.3299 | 7 | 96 | 0.84 | 0.04 | 0.03 | 0.00 | 0.00 | 45 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 618.3299 | 6 | 90 | 0.84 | 0.01 | 0.00 | 0.01 | 0.00 | 55 |
| example-mip | 1 | optimal | 618.3299 | 7 | 96 | 0.63 | 0.04 | 0.03 | 0.00 | 0.00 | 47 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 618.3299 | 6 | 90 | 0.87* | 0.01* | 0.00 | 0.01 | 0.00 | 63 |
| example-mip | 2 | optimal | 618.3299 | 7 | 96 | 0.63 | 0.04 | 0.03 | 0.00 | 0.00 | 47 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 618.3299 | 6 | 90 | 0.63 | 0.01 | 0.00 | 0.01 | 0.00 | 49 |
| example-mip | 3 | optimal | 618.3299 | 7 | 96 | 0.84 | 0.04 | 0.03 | 0.00 | 0.00 | 66 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 618.3299 | 6 | 90 | 0.63 | 0.01 | 0.00 | 0.01 | 0.00 | 50 |
| example-mip | 4 | optimal | 618.3299 | 7 | 96 | 0.63 | 0.04 | 0.03 | 0.00 | 0.00 | 50 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 618.3299 | 6 | 90 | 0.84 | 0.01 | 0.00 | 0.01 | 0.00 | 42 |

## R201_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 449.6598 | 14 | 2894 | 1.31 | 0.57 | 0.11 | 0.29 | 0.15 | 72 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 449.6598 | 11 | 3247 | 1.05 | 0.36 | 0.02 | 0.33 | 0.00 | 70 |
| example-mip | 1 | optimal | 449.6598 | 14 | 2894 | 1.25 | 0.58 | 0.10 | 0.32 | 0.14 | 71 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 449.6598 | 11 | 3247 | 1.05 | 0.35 | 0.02 | 0.33 | 0.00 | 70 |
| example-mip | 2 | optimal | 449.6598 | 14 | 2894 | 1.25 | 0.51 | 0.09 | 0.27 | 0.14 | 72 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 449.6598 | 11 | 3247 | 1.04 | 0.35 | 0.02 | 0.33 | 0.00 | 70 |
| example-mip | 3 | optimal | 449.6598 | 14 | 2894 | 1.25 | 0.51 | 0.09 | 0.27 | 0.13 | 72 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 449.6598 | 11 | 3247 | 1.10 | 0.35 | 0.02 | 0.33 | 0.00 | 70 |
| example-mip | 4 | optimal | 449.6598 | 14 | 2894 | 1.29* | 0.54* | 0.10 | 0.28 | 0.14 | 72 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 449.6598 | 11 | 3247 | 1.04 | 0.34 | 0.02 | 0.32 | 0.00 | 70 |

## RC201_12

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 427.2356 | 16 | 1085 | 0.85 | 0.18 | 0.07 | 0.05 | 0.05 | 64 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 427.2356 | 16 | 1046 | 0.85 | 0.07 | 0.01 | 0.06 | 0.00 | 64 |
| example-mip | 1 | optimal | 427.2356 | 16 | 1085 | 0.84 | 0.15 | 0.06 | 0.04 | 0.04 | 65 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 427.2356 | 16 | 1046 | 0.84 | 0.07 | 0.01 | 0.06 | 0.00 | 50 |
| example-mip | 2 | optimal | 427.2356 | 16 | 1085 | 0.84 | 0.15 | 0.06 | 0.04 | 0.04 | 65 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 427.2356 | 16 | 1046 | 0.89 | 0.08 | 0.01 | 0.06 | 0.00 | 65 |
| example-mip | 3 | optimal | 427.2356 | 16 | 1085 | 0.91* | 0.15* | 0.06 | 0.04 | 0.04 | 68 |
| layer-purepython-gurobipy-exact | 3 | Optimal | 427.2356 | 16 | 1046 | 0.63 | 0.07 | 0.01 | 0.06 | 0.00 | 51 |
| example-mip | 4 | optimal | 427.2356 | 16 | 1085 | 0.83 | 0.14 | 0.06 | 0.04 | 0.04 | 68 |
| layer-purepython-gurobipy-exact | 4 | Optimal | 427.2356 | 16 | 1046 | 0.63 | 0.07 | 0.01 | 0.06 | 0.00 | 50 |

## By configuration (mean wall s)

| config | C101_25 | R101_25 | R201_25 | RC201_12 |
|---|---|---|---|---|
| example-mip | 1.01 | 0.71 | 1.27 | 0.85 |
| layer-purepython-gurobipy-exact | 0.73 | 0.76 | 1.06 | 0.77 |

## By configuration (mean solve s)

| config | C101_25 | R101_25 | R201_25 | RC201_12 |
|---|---|---|---|---|
| example-mip | 0.15 | 0.04 | 0.54 | 0.15 |
| layer-purepython-gurobipy-exact | 0.06 | 0.01 | 0.35 | 0.07 |

## LP mismatches

none
