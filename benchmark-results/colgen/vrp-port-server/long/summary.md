# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## R201

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| example-mip | 0 | optimal | 1084.9907 | 46 | 81520 | 3271.98 | 3271.31 | 57.64 | 3204.53 | 8.05 | 521 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 1084.9907 | 43 | 73741 | 2231.82 | 2230.79 | 7.68 | 2223.04 | 0.07 | 292 |
| example-mip | 1 | optimal | 1084.9907 | 42 | 82640 | 3334.12 | 3333.43 | 59.54 | 3264.59 | 8.18 | 499 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 1084.9907 | 43 | 73741 | 2239.12 | 2238.22 | 7.67 | 2230.47 | 0.07 | 299 |

## By configuration (mean wall s)

| config | R201 |
|---|---|
| example-mip | 3303.05 |
| layer-purepython-gurobipy-exact | 2235.47 |

## By configuration (mean solve s)

| config | R201 |
|---|---|
| example-mip | 3302.37 |
| layer-purepython-gurobipy-exact | 2234.50 |

## LP mismatches

none
