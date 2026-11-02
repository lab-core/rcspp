# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highs-exact | 0 | Optimal | 191.8136 | 24 | 761 | 1.52 | 0.06 | 0.02 | 0.04 | 0.00 | 53 |
| layer-cpp-highs-heuristic | 0 | Optimal | 191.8136 | 41 | 2793 | 1.73 | 0.85 | 0.13 | 0.72 | 0.00 | 62 |
| layer-cpp-highspy-exact | 0 | Optimal | 191.8136 | 24 | 761 | 1.07 | 0.06 | 0.02 | 0.04 | 0.00 | 63 |
| layer-cpp-highspy-heuristic | 0 | Optimal | 191.8136 | 41 | 2793 | 1.57 | 0.76 | 0.13 | 0.64 | 0.00 | 65 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 191.8136 | 30 | 670 | 1.07 | 0.08 | 0.03 | 0.05 | 0.01 | 63 |
| layer-cpp-gurobipy-heuristic | 0 | Optimal | 191.8136 | 38 | 2548 | 1.71 | 0.71 | 0.05 | 0.65 | 0.01 | 65 |
| layer-python-highs-exact | 0 | Optimal | 191.8136 | 24 | 761 | 1.07 | 0.07 | 0.02 | 0.04 | 0.02 | 60 |
| layer-python-highs-heuristic | 0 | Optimal | 191.8136 | 41 | 2793 | 2.04 | 1.00 | 0.16 | 0.83 | 0.01 | 63 |
| layer-python-highspy-exact | 0 | Optimal | 191.8136 | 24 | 761 | 1.09 | 0.07 | 0.02 | 0.04 | 0.01 | 52 |
| layer-python-highspy-heuristic | 0 | Optimal | 191.8136 | 41 | 2793 | 1.71 | 0.75 | 0.12 | 0.62 | 0.01 | 66 |
| layer-python-gurobipy-exact | 0 | Optimal | 191.8136 | 30 | 670 | 1.07 | 0.08 | 0.01 | 0.05 | 0.01 | 52 |
| layer-python-gurobipy-heuristic | 0 | Optimal | 191.8136 | 38 | 2548 | 1.57 | 0.63 | 0.04 | 0.57 | 0.01 | 66 |

## R101_25

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highs-exact | 0 | Optimal | 618.3299 | 7 | 137 | 1.01* | 0.01* | 0.00 | 0.01 | 0.00 | 53 |
| layer-cpp-highs-heuristic | 0 | Optimal | 618.3299 | 11 | 501 | 0.87 | 0.03 | 0.01 | 0.02 | 0.00 | 59 |
| layer-cpp-highspy-exact | 0 | Optimal | 618.3299 | 7 | 137 | 0.86 | 0.01 | 0.00 | 0.01 | 0.00 | 53 |
| layer-cpp-highspy-heuristic | 0 | Optimal | 618.3299 | 11 | 501 | 0.86 | 0.02 | 0.01 | 0.01 | 0.00 | 54 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 618.3299 | 8 | 127 | 1.08 | 0.02 | 0.00 | 0.01 | 0.00 | 50 |
| layer-cpp-gurobipy-heuristic | 0 | Optimal | 618.3299 | 9 | 446 | 1.16 | 0.03 | 0.01 | 0.02 | 0.00 | 62 |
| layer-python-highs-exact | 0 | Optimal | 618.3299 | 7 | 137 | 1.04* | 0.02* | 0.00 | 0.01 | 0.00 | 59 |
| layer-python-highs-heuristic | 0 | Optimal | 618.3299 | 11 | 501 | 0.85 | 0.03 | 0.01 | 0.01 | 0.00 | 53 |
| layer-python-highspy-exact | 0 | Optimal | 618.3299 | 7 | 137 | 0.86 | 0.02 | 0.00 | 0.01 | 0.00 | 49 |
| layer-python-highspy-heuristic | 0 | Optimal | 618.3299 | 11 | 501 | 0.87 | 0.03 | 0.01 | 0.01 | 0.01 | 50 |
| layer-python-gurobipy-exact | 0 | Optimal | 618.3299 | 8 | 127 | 0.87 | 0.02 | 0.00 | 0.01 | 0.01 | 52 |
| layer-python-gurobipy-heuristic | 0 | Optimal | 618.3299 | 9 | 446 | 1.44 | 0.04 | 0.01 | 0.02 | 0.01 | 63 |

## By configuration (mean wall s)

| config | C101_25 | R101_25 |
|---|---|---|
| layer-cpp-highs-exact | 1.52 | 1.01 |
| layer-cpp-highs-heuristic | 1.73 | 0.87 |
| layer-cpp-highspy-exact | 1.07 | 0.86 |
| layer-cpp-highspy-heuristic | 1.57 | 0.86 |
| layer-cpp-gurobipy-exact | 1.07 | 1.08 |
| layer-cpp-gurobipy-heuristic | 1.71 | 1.16 |
| layer-python-highs-exact | 1.07 | 1.04 |
| layer-python-highs-heuristic | 2.04 | 0.85 |
| layer-python-highspy-exact | 1.09 | 0.86 |
| layer-python-highspy-heuristic | 1.71 | 0.87 |
| layer-python-gurobipy-exact | 1.07 | 0.87 |
| layer-python-gurobipy-heuristic | 1.57 | 1.44 |

## By configuration (mean solve s)

| config | C101_25 | R101_25 |
|---|---|---|
| layer-cpp-highs-exact | 0.06 | 0.01 |
| layer-cpp-highs-heuristic | 0.85 | 0.03 |
| layer-cpp-highspy-exact | 0.06 | 0.01 |
| layer-cpp-highspy-heuristic | 0.76 | 0.02 |
| layer-cpp-gurobipy-exact | 0.08 | 0.02 |
| layer-cpp-gurobipy-heuristic | 0.71 | 0.03 |
| layer-python-highs-exact | 0.07 | 0.02 |
| layer-python-highs-heuristic | 1.00 | 0.03 |
| layer-python-highspy-exact | 0.07 | 0.02 |
| layer-python-highspy-heuristic | 0.75 | 0.03 |
| layer-python-gurobipy-exact | 0.08 | 0.02 |
| layer-python-gurobipy-heuristic | 0.63 | 0.04 |

## LP mismatches

none
