# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## R201

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 1084.9907 | 37 | 70966 | 1846.79 | 1845.50 | 31.71 | 1813.79 | 0.00 | 246 |
| layer-purepython-highspy-exact | 0 | Optimal | 1084.9907 | 37 | 70966 | 1789.16 | 1787.87 | 32.22 | 1755.62 | 0.03 | 256 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 1084.9907 | 46 | 73127 | 1625.94 | 1624.45 | 5.56 | 1618.86 | 0.04 | 251 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 1084.9907 | 46 | 73127 | 1628.26 | 1626.90 | 5.43 | 1621.39 | 0.07 | 256 |
| layer-cpp-highspy-tabu | 0 | Optimal | 1084.9907 | 65 | 37345 | 191.08 | 189.93 | 14.59 | 175.34 | 0.00 | 153 |
| layer-purepython-highspy-tabu | 0 | Optimal | 1084.9907 | 65 | 37345 | 195.15 | 193.89 | 15.05 | 178.81 | 0.04 | 152 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 1084.9907 | 65 | 34496 | 164.62 | 163.16 | 2.51 | 160.63 | 0.02 | 154 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 1084.9907 | 65 | 34496 | 161.55 | 160.38 | 2.50 | 157.83 | 0.05 | 153 |
| layer-cpp-highspy-exact | 1 | Optimal | 1084.9907 | 37 | 70966 | 1749.63 | 1748.33 | 31.04 | 1717.29 | 0.00 | 247 |
| layer-purepython-highspy-exact | 1 | Optimal | 1084.9907 | 37 | 70966 | 1769.46 | 1768.24 | 31.42 | 1736.79 | 0.04 | 256 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 1084.9907 | 46 | 73127 | 1594.79 | 1591.72 | 5.15 | 1586.53 | 0.04 | 251 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 1084.9907 | 46 | 73127 | 1599.85 | 1598.61 | 5.22 | 1593.33 | 0.06 | 257 |
| layer-cpp-highspy-tabu | 1 | Optimal | 1084.9907 | 65 | 37345 | 188.03 | 186.74 | 14.16 | 172.58 | 0.00 | 153 |
| layer-purepython-highspy-tabu | 1 | Optimal | 1084.9907 | 65 | 37345 | 188.70 | 187.58 | 14.55 | 172.99 | 0.04 | 153 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 1084.9907 | 65 | 34496 | 155.50 | 153.82 | 2.39 | 151.41 | 0.02 | 154 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 1084.9907 | 65 | 34496 | 158.03 | 156.71 | 2.41 | 154.25 | 0.05 | 153 |

## RC201

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highspy-exact | 0 | Optimal | 1112.8414 | 47 | 49819 | 973.35 | 972.17 | 19.76 | 952.41 | 0.00 | 245 |
| layer-purepython-highspy-exact | 0 | Optimal | 1112.8414 | 47 | 49819 | 968.51 | 967.27 | 20.15 | 947.09 | 0.04 | 244 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 1112.8414 | 55 | 50006 | 954.19 | 952.79 | 3.51 | 949.25 | 0.03 | 252 |
| layer-purepython-gurobipy-exact | 0 | Optimal | 1112.8414 | 55 | 50006 | 954.81 | 953.59 | 3.50 | 950.02 | 0.07 | 254 |
| layer-cpp-highspy-tabu | 0 | Optimal | 1112.8414 | 71 | 21411 | 50.07 | 49.04 | 5.81 | 43.24 | 0.00 | 114 |
| layer-purepython-highspy-tabu | 0 | Optimal | 1112.8414 | 71 | 21411 | 52.39 | 51.30 | 6.06 | 45.19 | 0.04 | 114 |
| layer-cpp-gurobipy-tabu | 0 | Optimal | 1112.8414 | 72 | 20889 | 69.20 | 68.11 | 1.56 | 66.54 | 0.02 | 127 |
| layer-purepython-gurobipy-tabu | 0 | Optimal | 1112.8414 | 72 | 20889 | 70.88 | 69.69 | 1.54 | 68.10 | 0.04 | 127 |
| layer-cpp-highspy-exact | 1 | Optimal | 1112.8414 | 47 | 49819 | 946.24 | 945.13 | 19.74 | 925.38 | 0.00 | 244 |
| layer-purepython-highspy-exact | 1 | Optimal | 1112.8414 | 47 | 49819 | 945.43 | 944.21 | 19.86 | 924.31 | 0.04 | 244 |
| layer-cpp-gurobipy-exact | 1 | Optimal | 1112.8414 | 55 | 50006 | 937.40 | 935.78 | 3.40 | 932.35 | 0.03 | 253 |
| layer-purepython-gurobipy-exact | 1 | Optimal | 1112.8414 | 55 | 50006 | 930.71 | 929.35 | 3.43 | 925.86 | 0.05 | 253 |
| layer-cpp-highspy-tabu | 1 | Optimal | 1112.8414 | 71 | 21411 | 48.10 | 47.03 | 5.78 | 41.25 | 0.00 | 113 |
| layer-purepython-highspy-tabu | 1 | Optimal | 1112.8414 | 71 | 21411 | 48.06 | 46.83 | 5.88 | 40.92 | 0.04 | 118 |
| layer-cpp-gurobipy-tabu | 1 | Optimal | 1112.8414 | 72 | 20889 | 67.51 | 65.92 | 1.54 | 64.37 | 0.02 | 130 |
| layer-purepython-gurobipy-tabu | 1 | Optimal | 1112.8414 | 72 | 20889 | 67.28 | 66.03 | 1.51 | 64.47 | 0.05 | 127 |

## By configuration (mean wall s)

| config | R201 | RC201 |
|---|---|---|
| layer-cpp-highspy-exact | 1798.21 | 959.80 |
| layer-cpp-gurobipy-exact | 1610.37 | 945.79 |
| layer-purepython-highspy-exact | 1779.31 | 956.97 |
| layer-purepython-highspy-tabu | 191.93 | 50.22 |
| layer-cpp-highspy-tabu | 189.55 | 49.08 |
| layer-purepython-gurobipy-exact | 1614.06 | 942.76 |
| layer-purepython-gurobipy-tabu | 159.79 | 69.08 |
| layer-cpp-gurobipy-tabu | 160.06 | 68.35 |

## By configuration (mean solve s)

| config | R201 | RC201 |
|---|---|---|
| layer-cpp-highspy-exact | 1796.91 | 958.65 |
| layer-cpp-gurobipy-exact | 1608.08 | 944.28 |
| layer-purepython-highspy-exact | 1778.06 | 955.74 |
| layer-purepython-highspy-tabu | 190.74 | 49.06 |
| layer-cpp-highspy-tabu | 188.34 | 48.04 |
| layer-purepython-gurobipy-exact | 1612.75 | 941.47 |
| layer-purepython-gurobipy-tabu | 158.55 | 67.86 |
| layer-cpp-gurobipy-tabu | 158.49 | 67.02 |

## LP mismatches

none
