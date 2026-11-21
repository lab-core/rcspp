# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 363.2468 | 43 | 1705 | 1.24 | 0.60 | 0.30 | 0.17 | 0.09 | 73 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 363.2468 | 49 | 1792 | 1.24 | 0.47 | 0.11 | 0.34 | 0.01 | 72 |
| example-mip | 1 | optimal | 363.2468 | 43 | 1705 | 1.26 | 0.61 | 0.31 | 0.17 | 0.09 | 73 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 363.2468 | 49 | 1792 | 1.03 | 0.45 | 0.11 | 0.33 | 0.01 | 72 |
| example-mip | 2 | optimal | 363.2468 | 55 | 1786 | 1.47 | 0.73 | 0.39 | 0.21 | 0.09 | 73 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 363.2468 | 49 | 1792 | 1.03 | 0.45 | 0.11 | 0.33 | 0.01 | 72 |

## R201_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 756.3386 | 30 | 12619 | 23.85 | 23.22 | 1.63 | 20.50 | 0.86 | 115 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 756.3386 | 28 | 13171 | 21.77 | 21.13 | 0.55 | 20.57 | 0.02 | 100 |
| example-mip | 1 | optimal | 756.3386 | 30 | 12619 | 22.62 | 22.05 | 1.61 | 19.33 | 0.88 | 122 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 756.3386 | 28 | 13171 | 22.38 | 21.63 | 0.55 | 21.06 | 0.02 | 100 |
| example-mip | 2 | optimal | 756.3386 | 30 | 12619 | 23.21 | 22.52 | 1.58 | 19.85 | 0.87 | 115 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 756.3386 | 28 | 13171 | 21.36 | 20.73 | 0.55 | 20.17 | 0.02 | 100 |

## RC101_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 828.3619 | 22 | 631 | 1.03 | 0.31 | 0.13 | 0.10 | 0.03 | 70 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 828.3619 | 28 | 737 | 1.03 | 0.26 | 0.03 | 0.22 | 0.01 | 72 |
| example-mip | 1 | optimal | 828.3619 | 22 | 631 | 1.03 | 0.31 | 0.13 | 0.10 | 0.03 | 70 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 828.3619 | 28 | 737 | 1.03 | 0.26 | 0.03 | 0.22 | 0.01 | 72 |
| example-mip | 2 | optimal | 828.3619 | 22 | 631 | 1.03 | 0.32 | 0.13 | 0.11 | 0.03 | 71 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 828.3619 | 28 | 737 | 1.03 | 0.26 | 0.03 | 0.22 | 0.01 | 72 |

## C201

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 591.5566 | 165 | 37171 | 64.51 | 63.86 | 40.67 | 17.25 | 3.37 | 307 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 591.5566 | 344 | 52911 | 64.86 | 63.92 | 33.02 | 30.77 | 0.12 | 212 |
| example-mip | 1 | optimal | 591.5566 | 184 | 36972 | 65.62 | 64.87 | 41.27 | 17.41 | 3.60 | 301 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 591.5566 | 344 | 52911 | 65.03 | 64.08 | 33.29 | 30.66 | 0.13 | 215 |
| example-mip | 2 | optimal | 591.5566 | 184 | 36972 | 66.13 | 65.33 | 41.54 | 17.65 | 3.50 | 312 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 591.5566 | 344 | 52911 | 65.68 | 64.67 | 33.24 | 31.31 | 0.12 | 217 |

## By configuration (mean wall s)

| config | C101_50 | R201_50 | RC101_50 | C201 |
|---|---|---|---|---|
| example-mip | 1.32 | 23.23 | 1.03 | 65.42 |
| layer-purepython-gurobipy-exact | 1.10 | 21.83 | 1.03 | 65.19 |

## By configuration (mean solve s)

| config | C101_50 | R201_50 | RC101_50 | C201 |
|---|---|---|---|---|
| example-mip | 0.65 | 22.59 | 0.31 | 64.68 |
| layer-purepython-gurobipy-exact | 0.45 | 21.16 | 0.26 | 64.22 |

## LP mismatches

none
