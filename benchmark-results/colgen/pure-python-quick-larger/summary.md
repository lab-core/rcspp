# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101_50

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 363.2468 | 54 | 1749 | 1.72 | 0.45 | 0.13 | 0.32 | 0.00 | 69 |
| layer-purepython-highspy-exact | 0 | Optimal | 363.2468 | 54 | 1749 | 1.70 | 0.44 | 0.13 | 0.30 | 0.02 | 70 |

## C201

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 591.5566 | 190 | 37974 | 44.07* | 42.44* | 29.70 | 12.73 | 0.00 | 147 |
| layer-purepython-highspy-exact | 0 | Optimal | 591.5566 | 190 | 37974 | 45.91* | 44.44* | 31.15 | 13.22 | 0.07 | 157 |

## By configuration (mean wall s)

| config | C101_50 | C201 |
|---|---|---|
| layer-cpp-highspy-exact | 1.72 | 44.07 |
| layer-purepython-highspy-exact | 1.70 | 45.91 |

## By configuration (mean solve s)

| config | C101_50 | C201 |
|---|---|---|
| layer-cpp-highspy-exact | 0.45 | 42.44 |
| layer-purepython-highspy-exact | 0.44 | 44.44 |

## LP mismatches

none
