# Summary of truncated-50

- 21 runs, 1 rounds, 1 processes
- 21 / 21 rounds finished (0 failed checks), 0 timeouts, 0 not finished
- last event: finished at 2026-11-21T15:53:11.708+00:00

## Flags

None.

## Records

- source.commit: e91db2963110807689e7134fefc37d5ce4cae045
- source.uncommitted_changes: no
- source.branch: feature/column-generation-cpp-vrp-example
- machine.label: leo-laptop
- machine.processor: 11th Gen Intel(R) Core(TM) i7-1165G7 @ 2.80GHz
- versions: python 3.14.0, rcspp 0.0.2.dev1, numpy 2.5.3
- solvers: highspy 1.15.1, highspy_highs 1.15.1, highs 1.15.1, gurobipy 13.0.1, gurobi 13.0.1

## Results

| run                                                                            | finished | status  | objective | iterations | columns | artificial_columns | integer_status | integer_objective | gap      | solution_check | solution_columns |
|--------------------------------------------------------------------------------|----------|---------|-----------|------------|---------|--------------------|----------------|-------------------|----------|----------------|------------------|
| R201_50-package-highspy-exact-single-customer-column_generation_85d80e         | 1 / 1    | Optimal | 756.339   | 28         | 12671   | 0                  | Optimal        | 1029.4            | 0.265266 | passed         | 31               |
| R201_50-package-highspy-first-1-single-customer-column_generation_85d80e       | 1 / 1    | Optimal | 756.339   | 445        | 444     | 0                  | Optimal        | 1890.53           | 0.599933 | passed         | 36               |
| R201_50-package-highspy-first-5-single-customer-column_generation_85d80e       | 1 / 1    | Optimal | 756.339   | 162        | 800     | 0                  | Optimal        | 1714.79           | 0.558931 | passed         | 34               |
| R201_50-package-gurobipy-exact-single-customer-column_generation_85d80e        | 1 / 1    | Optimal | 756.339   | 31         | 11918   | 0                  | Optimal        | 1079.45           | 0.299331 | passed         | 35               |
| R201_50-package-gurobipy-first-1-single-customer-column_generation_85d80e      | 1 / 1    | Optimal | 756.339   | 447        | 446     | 0                  | Optimal        | 1867.43           | 0.594983 | passed         | 34               |
| R201_50-package-gurobipy-first-5-single-customer-column_generation_85d80e      | 1 / 1    | Optimal | 756.339   | 153        | 750     | 0                  | Optimal        | 1754.45           | 0.568902 | passed         | 34               |
| R201_50-cpp-bindings-highspy-exact-single-customer-column_generation_85d80e    | 1 / 1    | Optimal | 756.339   | 28         | 12671   | 0                  | Optimal        | 1029.4            | 0.265266 | passed         | 31               |
| R201_50-cpp-bindings-highspy-first-1-single-customer-column_generation_85d80e  | 1 / 1    | Optimal | 756.339   | 445        | 444     | 0                  | Optimal        | 1890.53           | 0.599933 | passed         | 36               |
| R201_50-cpp-bindings-highspy-first-5-single-customer-column_generation_85d80e  | 1 / 1    | Optimal | 756.339   | 162        | 800     | 0                  | Optimal        | 1714.79           | 0.558931 | passed         | 34               |
| R201_50-cpp-bindings-gurobipy-exact-single-customer-column_generation_85d80e   | 1 / 1    | Optimal | 756.339   | 31         | 11918   | 0                  | Optimal        | 1079.45           | 0.299331 | passed         | 35               |
| R201_50-cpp-bindings-gurobipy-first-1-single-customer-column_generation_85d80e | 1 / 1    | Optimal | 756.339   | 447        | 446     | 0                  | Optimal        | 1867.43           | 0.594983 | passed         | 34               |
| R201_50-cpp-bindings-gurobipy-first-5-single-customer-column_generation_85d80e | 1 / 1    | Optimal | 756.339   | 153        | 750     | 0                  | Optimal        | 1754.45           | 0.568902 | passed         | 34               |
| R201_50-cpp-bindings-highs-exact-single-customer-column_generation_85d80e      | 1 / 1    | Optimal | 756.339   | 28         | 12671   | 0                  | Optimal        | 1029.4            | 0.265266 | passed         | 31               |
| R201_50-cpp-bindings-highs-first-1-single-customer-column_generation_85d80e    | 1 / 1    | Optimal | 756.339   | 445        | 444     | 0                  | Optimal        | 1890.53           | 0.599933 | passed         | 36               |
| R201_50-cpp-bindings-highs-first-5-single-customer-column_generation_85d80e    | 1 / 1    | Optimal | 756.339   | 162        | 800     | 0                  | Optimal        | 1714.79           | 0.558931 | passed         | 34               |
| R201_50-cpp-native-highs-exact-single-customer-column_generation_85d80e        | 1 / 1    | Optimal | 756.339   | 28         | 12671   | 0                  | Optimal        | 1029.4            | 0.265266 | passed         | 31               |
| R201_50-cpp-native-highs-first-1-single-customer-column_generation_85d80e      | 1 / 1    | Optimal | 756.339   | 445        | 444     | 0                  | Optimal        | 1890.53           | 0.599933 | passed         | 36               |
| R201_50-cpp-native-highs-first-5-single-customer-column_generation_85d80e      | 1 / 1    | Optimal | 756.339   | 162        | 800     | 0                  | Optimal        | 1714.79           | 0.558931 | passed         | 34               |
| R201_50-cpp-native-gurobi-exact-single-customer-column_generation_85d80e       | 1 / 1    | Optimal | 756.339   | 31         | 11918   | 0                  | Optimal        | 1079.45           | 0.299331 | passed         | 35               |
| R201_50-cpp-native-gurobi-first-1-single-customer-column_generation_85d80e     | 1 / 1    | Optimal | 756.339   | 447        | 446     | 0                  | Optimal        | 1867.43           | 0.594983 | passed         | 34               |
| R201_50-cpp-native-gurobi-first-5-single-customer-column_generation_85d80e     | 1 / 1    | Optimal | 756.339   | 153        | 750     | 0                  | Optimal        | 1754.45           | 0.568902 | passed         | 34               |

## Times

Medians over the finished rounds, in seconds unless said; spread: (max - min) / median.

| run                                                                            | solve_seconds median | min    | max    | spread % | master_seconds | pricing_seconds | other_seconds | lp_solved_again_seconds | integer_seconds | process_peak_memory MiB | process_processor_seconds | ms / iteration | other CPU % |
|--------------------------------------------------------------------------------|----------------------|--------|--------|----------|----------------|-----------------|---------------|-------------------------|-----------------|-------------------------|---------------------------|----------------|-------------|
| R201_50-package-highspy-exact-single-customer-column_generation_85d80e         | 5.966                | 5.966  | 5.966  | 0.0      | 0.747          | 5.209           | 0.009         | -                       | 0.223           | 98.0                    | 6.562                     | 213.05         | 15          |
| R201_50-package-highspy-first-1-single-customer-column_generation_85d80e       | 35.365               | 35.365 | 35.365 | 0.0      | 0.318          | 35.008          | 0.040         | -                       | 0.015           | 80.6                    | 35.203                    | 79.47          | 16          |
| R201_50-package-highspy-first-5-single-customer-column_generation_85d80e       | 26.085               | 26.085 | 26.085 | 0.0      | 0.225          | 25.842          | 0.017         | -                       | 0.016           | 88.9                    | 26.422                    | 161.02         | 11          |
| R201_50-package-gurobipy-exact-single-customer-column_generation_85d80e        | 5.889                | 5.889  | 5.889  | 0.0      | 0.288          | 5.587           | 0.014         | -                       | 0.167           | 91.3                    | 6.172                     | 189.97         | 21          |
| R201_50-package-gurobipy-first-1-single-customer-column_generation_85d80e      | 34.657               | 34.657 | 34.657 | 0.0      | 0.208          | 34.420          | 0.029         | -                       | 0.004           | 89.9                    | 34.594                    | 77.53          | 7           |
| R201_50-package-gurobipy-first-5-single-customer-column_generation_85d80e      | 23.428               | 23.428 | 23.428 | 0.0      | 0.122          | 23.287          | 0.019         | -                       | 0.002           | 90.5                    | 23.812                    | 153.13         | 7           |
| R201_50-cpp-bindings-highspy-exact-single-customer-column_generation_85d80e    | 5.416                | 5.416  | 5.416  | 0.0      | 0.676          | 4.740           | 0.000         | -                       | 0.195           | 91.5                    | 6.047                     | 193.43         | 7           |
| R201_50-cpp-bindings-highspy-first-1-single-customer-column_generation_85d80e  | 30.542               | 30.542 | 30.542 | 0.0      | 0.257          | 30.285          | 0.000         | -                       | 0.011           | 78.3                    | 30.750                    | 68.63          | 5           |
| R201_50-cpp-bindings-highspy-first-5-single-customer-column_generation_85d80e  | 24.825               | 24.825 | 24.825 | 0.0      | 0.202          | 24.623          | 0.000         | -                       | 0.012           | 86.8                    | 25.219                    | 153.24         | 6           |
| R201_50-cpp-bindings-gurobipy-exact-single-customer-column_generation_85d80e   | 4.859                | 4.859  | 4.859  | 0.0      | 0.229          | 4.625           | 0.005         | -                       | 0.114           | 92.6                    | 5.312                     | 156.75         | 9           |
| R201_50-cpp-bindings-gurobipy-first-1-single-customer-column_generation_85d80e | 35.252               | 35.252 | 35.252 | 0.0      | 0.218          | 35.012          | 0.022         | -                       | 0.001           | 87.9                    | 35.438                    | 78.86          | 8           |
| R201_50-cpp-bindings-gurobipy-first-5-single-customer-column_generation_85d80e | 23.763               | 23.763 | 23.763 | 0.0      | 0.136          | 23.618          | 0.009         | -                       | 0.002           | 88.6                    | 24.172                    | 155.31         | 7           |
| R201_50-cpp-bindings-highs-exact-single-customer-column_generation_85d80e      | 5.452                | 5.452  | 5.452  | 0.0      | 0.692          | 4.760           | 0.000         | -                       | 0.193           | 87.9                    | 6.188                     | 194.72         | 7           |
| R201_50-cpp-bindings-highs-first-1-single-customer-column_generation_85d80e    | 31.081               | 31.081 | 31.081 | 0.0      | 0.272          | 30.808          | 0.000         | -                       | 0.010           | 75.2                    | 31.234                    | 69.84          | 7           |
| R201_50-cpp-bindings-highs-first-5-single-customer-column_generation_85d80e    | 25.054               | 25.054 | 25.054 | 0.0      | 0.204          | 24.850          | 0.000         | -                       | 0.012           | 83.6                    | 25.234                    | 154.66         | 6           |
| R201_50-cpp-native-highs-exact-single-customer-column_generation_85d80e        | 5.588                | 5.588  | 5.588  | 0.0      | 0.723          | 4.865           | 0.000         | -                       | 0.194           | 44.0                    | 5.734                     | 199.57         | 8           |
| R201_50-cpp-native-highs-first-1-single-customer-column_generation_85d80e      | 34.101               | 34.101 | 34.101 | 0.0      | 0.320          | 33.781          | 0.000         | -                       | 0.012           | 30.3                    | 33.562                    | 76.63          | 12          |
| R201_50-cpp-native-highs-first-5-single-customer-column_generation_85d80e      | 32.881               | 32.881 | 32.881 | 0.0      | 0.231          | 32.650          | 0.000         | -                       | 0.012           | 38.9                    | 32.328                    | 202.97         | 13          |
| R201_50-cpp-native-gurobi-exact-single-customer-column_generation_85d80e       | 4.964                | 4.964  | 4.964  | 0.0      | 0.233          | 4.731           | 0.000         | -                       | 0.109           | 34.4                    | 5.047                     | 160.12         | 11          |
| R201_50-cpp-native-gurobi-first-1-single-customer-column_generation_85d80e     | 43.921               | 43.921 | 43.921 | 0.0      | 0.207          | 43.712          | 0.001         | -                       | 0.003           | 39.0                    | 43.391                    | 98.26          | 11          |
| R201_50-cpp-native-gurobi-first-5-single-customer-column_generation_85d80e     | 27.827               | 27.827 | 27.827 | 0.0      | 0.130          | 27.696          | 0.001         | -                       | 0.004           | 40.0                    | 27.562                    | 181.87         | 10          |
