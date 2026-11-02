# Summary

`*` after a time: other processes used more than 50% of a core on average
during the run (see machine.csv), so the timing may be inflated.
Solve = master + pricing + rest, without Python start-up.

## C101

Baseline LP (current C++ example): 828.94

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highs-exact | 0 | Optimal | 828.9369 | 72 | 3699 | 2.95 | 1.67 | 0.39 | 1.28 | 0.00 | 77 |
| layer-cpp-highspy-exact | 0 | Optimal | 828.9369 | 72 | 3699 | 2.88 | 1.77 | 0.40 | 1.36 | 0.00 | 79 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 828.9369 | 95 | 3915 | 3.37 | 2.26 | 0.57 | 1.68 | 0.01 | 79 |
| layer-python-highs-exact | 0 | Optimal | 828.9369 | 72 | 3699 | 2.87* | 1.86* | 0.41 | 1.44 | 0.01 | 77 |
| layer-python-highspy-exact | 0 | Optimal | 828.9369 | 72 | 3699 | 2.74 | 1.60 | 0.39 | 1.20 | 0.01 | 79 |
| layer-python-gurobipy-exact | 0 | Optimal | 828.9369 | 95 | 3915 | 3.68* | 2.47* | 0.59 | 1.86 | 0.02 | 79 |

## R101

Baseline LP (current C++ example): 1636.39

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highs-exact | 0 | Optimal | 1636.3887 | 26 | 1521 | 1.48 | 0.46 | 0.10 | 0.36 | 0.00 | 75 |
| layer-cpp-highspy-exact | 0 | Optimal | 1636.3887 | 26 | 1521 | 1.69 | 0.53 | 0.10 | 0.43 | 0.00 | 78 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 1636.3887 | 30 | 1512 | 1.78 | 0.60 | 0.15 | 0.45 | 0.00 | 77 |
| layer-python-highs-exact | 0 | Optimal | 1636.3887 | 26 | 1521 | 1.48 | 0.45 | 0.10 | 0.35 | 0.01 | 76 |
| layer-python-highspy-exact | 0 | Optimal | 1636.3887 | 26 | 1521 | 1.69 | 0.53 | 0.10 | 0.42 | 0.01 | 78 |
| layer-python-gurobipy-exact | 0 | Optimal | 1636.3887 | 30 | 1512 | 1.69 | 0.59 | 0.15 | 0.43 | 0.01 | 77 |

## RC101

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highs-exact | 0 | Optimal | 1572.7390 | 33 | 2513 | 2.18* | 0.95* | 0.19 | 0.76 | 0.00 | 77 |
| layer-cpp-highspy-exact | 0 | Optimal | 1572.7390 | 33 | 2513 | 2.12 | 1.04 | 0.19 | 0.85 | 0.00 | 79 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 1572.7390 | 36 | 2320 | 2.37 | 1.11 | 0.21 | 0.89 | 0.01 | 78 |
| layer-python-highs-exact | 0 | Optimal | 1572.7390 | 33 | 2513 | 2.12 | 1.02 | 0.20 | 0.82 | 0.01 | 77 |
| layer-python-highspy-exact | 0 | Optimal | 1572.7390 | 33 | 2513 | 2.11 | 0.96 | 0.19 | 0.76 | 0.01 | 79 |
| layer-python-gurobipy-exact | 0 | Optimal | 1572.7390 | 36 | 2320 | 2.19* | 1.12* | 0.21 | 0.89 | 0.01 | 78 |

## C201

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highs-exact | 0 | Optimal | 591.5566 | 190 | 37974 | 38.05* | 36.70* | 26.71 | 10.00 | 0.00 | 144 |
| layer-cpp-highspy-exact | 0 | Optimal | 591.5566 | 190 | 37974 | 38.16* | 37.08* | 27.05 | 10.04 | 0.00 | 146 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 591.5566 | 235 | 45755 | 26.51* | 25.37* | 12.39 | 12.92 | 0.05 | 166 |
| layer-python-highs-exact | 0 | Optimal | 591.5566 | 190 | 37974 | 38.05* | 36.90* | 26.73 | 10.15 | 0.02 | 145 |
| layer-python-highspy-exact | 0 | Optimal | 591.5566 | 190 | 37974 | 38.34* | 37.27* | 27.03 | 10.22 | 0.02 | 146 |
| layer-python-gurobipy-exact | 0 | Optimal | 591.5566 | 235 | 45755 | 26.58* | 25.29* | 12.20 | 13.03 | 0.06 | 166 |

## RC201

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highs-exact | 0 | Optimal | 1112.8414 | 47 | 49819 | 993.94* | 992.85* | 19.60 | 973.25 | 0.00 | 241 |
| layer-cpp-highspy-exact | 0 | Optimal | 1112.8414 | 47 | 49819 | 1003.25* | 1001.96* | 19.65 | 982.31 | 0.00 | 244 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 1112.8414 | 55 | 50006 | 1002.91* | 1001.33* | 3.52 | 997.79 | 0.03 | 252 |
| layer-python-highs-exact | 0 | Optimal | 1112.8414 | 47 | 49819 | 1020.77* | 1019.52* | 20.96 | 998.54 | 0.01 | 242 |
| layer-python-highspy-exact | 0 | Optimal | 1112.8414 | 47 | 49819 | 1192.48* | 1191.33* | 22.81 | 1168.51 | 0.01 | 245 |
| layer-python-gurobipy-exact | 0 | Optimal | 1112.8414 | 55 | 50006 | 1068.95* | 1067.18* | 3.65 | 1063.49 | 0.04 | 253 |

## R201

| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layer-cpp-highs-exact | 0 | Optimal | 1084.9907 | 37 | 70966 | 1954.39* | 1953.25* | 32.75 | 1920.50 | 0.00 | 243 |
| layer-cpp-highspy-exact | 0 | Optimal | 1084.9907 | 37 | 70966 | 1979.88* | 1978.75* | 33.10 | 1945.66 | 0.00 | 246 |
| layer-cpp-gurobipy-exact | 0 | Optimal | 1084.9907 | 46 | 73127 | 1774.89* | 1773.21* | 5.53 | 1767.65 | 0.03 | 251 |
| layer-python-highs-exact | 0 | Optimal | 1084.9907 | 37 | 70966 | 1960.47* | 1959.18* | 32.73 | 1926.45 | 0.01 | 243 |
| layer-python-highspy-exact | 0 | Optimal | 1084.9907 | 37 | 70966 | 1931.35* | 1930.18* | 32.39 | 1897.78 | 0.01 | 246 |
| layer-python-gurobipy-exact | 0 | Optimal | 1084.9907 | 46 | 73127 | 1733.77* | 1732.17* | 5.42 | 1726.70 | 0.05 | 251 |

## By configuration (mean wall s)

| config | C101 | R101 | RC101 | C201 | RC201 | R201 |
|---|---|---|---|---|---|---|
| layer-cpp-highs-exact | 2.95 | 1.48 | 2.18 | 38.05 | 993.94 | 1954.39 |
| layer-cpp-highspy-exact | 2.88 | 1.69 | 2.12 | 38.16 | 1003.25 | 1979.88 |
| layer-cpp-gurobipy-exact | 3.37 | 1.78 | 2.37 | 26.51 | 1002.91 | 1774.89 |
| layer-python-highs-exact | 2.87 | 1.48 | 2.12 | 38.05 | 1020.77 | 1960.47 |
| layer-python-highspy-exact | 2.74 | 1.69 | 2.11 | 38.34 | 1192.48 | 1931.35 |
| layer-python-gurobipy-exact | 3.68 | 1.69 | 2.19 | 26.58 | 1068.95 | 1733.77 |

## By configuration (mean solve s)

| config | C101 | R101 | RC101 | C201 | RC201 | R201 |
|---|---|---|---|---|---|---|
| layer-cpp-highs-exact | 1.67 | 0.46 | 0.95 | 36.70 | 992.85 | 1953.25 |
| layer-cpp-highspy-exact | 1.77 | 0.53 | 1.04 | 37.08 | 1001.96 | 1978.75 |
| layer-cpp-gurobipy-exact | 2.26 | 0.60 | 1.11 | 25.37 | 1001.33 | 1773.21 |
| layer-python-highs-exact | 1.86 | 0.45 | 1.02 | 36.90 | 1019.52 | 1959.18 |
| layer-python-highspy-exact | 1.60 | 0.53 | 0.96 | 37.27 | 1191.33 | 1930.18 |
| layer-python-gurobipy-exact | 2.47 | 0.59 | 1.12 | 25.29 | 1067.18 | 1732.17 |

## LP mismatches

none
