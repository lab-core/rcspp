# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 363.2468 | 42 | 1690 | 1.04 | 0.31 | 0.15 | 0.08 | 0.05 | 77 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 363.2468 | 43 | 1702 | 1.06 | 0.20 | 0.04 | 0.16 | 0.01 | 68 |
| example-mip | 1 | optimal | 363.2468 | 42 | 1690 | 1.04 | 0.30 | 0.14 | 0.08 | 0.06 | 77 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 363.2468 | 43 | 1702 | 0.83 | 0.21 | 0.04 | 0.17 | 0.01 | 67 |
| example-mip | 2 | optimal | 363.2468 | 42 | 1690 | 1.04 | 0.29 | 0.14 | 0.08 | 0.05 | 77 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 363.2468 | 43 | 1702 | 0.83 | 0.21 | 0.04 | 0.17 | 0.01 | 67 |

## R201_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 756.3386 | 35 | 12821 | 10.05 | 9.20 | 1.81 | 6.21 | 0.94 | 110 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 756.3386 | 31 | 11918 | 6.52 | 5.80 | 0.33 | 5.47 | 0.01 | 91 |
| example-mip | 1 | optimal | 756.3386 | 35 | 12821 | 7.57 | 6.68 | 1.01 | 4.80 | 0.70 | 110 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 756.3386 | 31 | 11918 | 5.73* | 5.00* | 0.27 | 4.72 | 0.01 | 90 |
| example-mip | 2 | optimal | 756.3386 | 35 | 12821 | 7.11 | 6.41 | 1.05 | 4.57 | 0.68 | 111 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 756.3386 | 31 | 11918 | 5.64 | 5.02 | 0.24 | 4.77 | 0.01 | 91 |

## RC101_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 828.3619 | 28 | 649 | 0.86 | 0.15 | 0.06 | 0.05 | 0.02 | 67 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 828.3619 | 28 | 715 | 0.87 | 0.10 | 0.01 | 0.09 | 0.01 | 67 |
| example-mip | 1 | optimal | 828.3619 | 28 | 649 | 1.05 | 0.16 | 0.07 | 0.06 | 0.02 | 75 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 828.3619 | 28 | 715 | 0.83 | 0.11 | 0.01 | 0.09 | 0.01 | 67 |
| example-mip | 2 | optimal | 828.3619 | 28 | 649 | 0.84 | 0.16 | 0.06 | 0.06 | 0.02 | 67 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 828.3619 | 28 | 715 | 0.88 | 0.13 | 0.01 | 0.10 | 0.01 | 67 |

## C201

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 591.5566 | 184 | 35556 | 47.16* | 46.38* | 32.18 | 8.48 | 3.98 | 306 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 591.5566 | 235 | 45755 | 24.91* | 23.84* | 10.19 | 13.57 | 0.09 | 182 |
| example-mip | 1 | optimal | 591.5566 | 158 | 35339 | 31.30* | 30.52* | 21.54 | 5.59 | 2.39 | 302 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 591.5566 | 235 | 45755 | 19.54 | 18.74 | 8.90 | 9.78 | 0.07 | 182 |
| example-mip | 2 | optimal | 591.5566 | 158 | 35339 | 29.37 | 28.67 | 20.09 | 5.14 | 2.31 | 306 |
| layer-purepython-gurobipy-exact | 2 | Optimal | 591.5566 | 235 | 45755 | 19.11 | 18.41 | 8.71 | 9.63 | 0.07 | 181 |

## By configuration (mean wall s)

| config | C101_50 | R201_50 | RC101_50 | C201 |
|---|---|---|---|---|
| example-mip | 1.04 | 8.24 | 0.91 | 35.94 |
| layer-purepython-gurobipy-exact | 0.91 | 5.96 | 0.86 | 21.19 |

## By configuration (mean solve s)

| config | C101_50 | R201_50 | RC101_50 | C201 |
|---|---|---|---|---|
| example-mip | 0.30 | 7.43 | 0.16 | 35.19 |
| layer-purepython-gurobipy-exact | 0.21 | 5.27 | 0.11 | 20.33 |

## LP mismatches

none
