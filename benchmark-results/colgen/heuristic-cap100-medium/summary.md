# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## R201_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highs-exact | 0 | Optimal | 756.3386 | 28 | 12671 | 6.57 | 5.55 | 0.74 | 4.81 | 0.00 | 84 |
| example-mip | 0 | optimal | 756.3386 | 35 | 12821 | 10.56 | 9.33 | 1.61 | 6.61 | 0.88 | 111 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 756.3386 | 31 | 11918 | 8.08* | 7.17* | 0.34 | 6.82 | 0.01 | 88 |

## By configuration (mean wall s)

| config | R201_50 |
|---|---|
| example-mip | 10.56 |
| layer-cpp-highs-exact | 6.57 |
| layer-cpp-gurobipy-exact | 8.08 |

## By configuration (mean solve s)

| config | R201_50 |
|---|---|
| example-mip | 9.33 |
| layer-cpp-highs-exact | 5.55 |
| layer-cpp-gurobipy-exact | 7.17 |

## LP mismatches

none
