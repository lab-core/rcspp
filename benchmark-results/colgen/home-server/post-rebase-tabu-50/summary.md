# Summary of post-rebase-tabu-50

- 14 runs, 3 rounds, 1 processes
- 42 / 42 rounds finished (0 failed checks), 0 timeouts, 0 not finished
- last event: finished at 2026-11-24T15:31:24.135+00:00

## Flags

None.

## Records

- source.commit: 71c1345176b03d943716e14e77a265bdd9b56bb9
- source.uncommitted_changes: no
- source.branch: feature/column-generation-vehicle-types
- machine.label: home-server
- machine.processor: Intel(R) Celeron(R) CPU 2950M @ 2.00GHz
- versions: python 3.12.16, rcspp 0.0.2.dev1, numpy 2.5.5
- solvers: highspy 1.15.1, highspy_highs 1.15.1, highs 1.15.1, gurobipy 13.0.1, gurobi 13.0.1

## Results

| run                                                                                | finished | status  | objective | iterations | columns | artificial_columns | integer_status | integer_objective | gap      | solution_check | solution_columns |
|------------------------------------------------------------------------------------|----------|---------|-----------|------------|---------|--------------------|----------------|-------------------|----------|----------------|------------------|
| R201_50-package-highspy-tabu-first-single-customer-column_generation_85d80e        | 3 / 3    | Optimal | 756.339   | 46         | 4048    | 0                  | Optimal        | 1207.17           | 0.373463 | passed         | 35               |
| R201_50-package-gurobipy-tabu-first-single-customer-column_generation_85d80e       | 3 / 3    | Optimal | 756.339   | 41         | 4905    | 0                  | Optimal        | 1120              | 0.324697 | passed         | 36               |
| R201_50-cpp-bindings-highspy-tabu-first-single-customer-column_generation_85d80e   | 3 / 3    | Optimal | 756.339   | 46         | 4048    | 0                  | Optimal        | 1207.17           | 0.373463 | passed         | 35               |
| R201_50-cpp-bindings-gurobipy-tabu-first-single-customer-column_generation_85d80e  | 3 / 3    | Optimal | 756.339   | 41         | 4905    | 0                  | Optimal        | 1120              | 0.324697 | passed         | 36               |
| R201_50-cpp-bindings-highs-tabu-first-single-customer-column_generation_85d80e     | 3 / 3    | Optimal | 756.339   | 46         | 4048    | 0                  | Optimal        | 1207.17           | 0.373463 | passed         | 35               |
| R201_50-cpp-native-highs-tabu-first-single-customer-column_generation_85d80e       | 3 / 3    | Optimal | 756.339   | 46         | 4048    | 0                  | Optimal        | 1207.17           | 0.373463 | passed         | 35               |
| R201_50-cpp-native-gurobi-tabu-first-single-customer-column_generation_85d80e      | 3 / 3    | Optimal | 756.339   | 41         | 4905    | 0                  | Optimal        | 1120              | 0.324697 | passed         | 36               |
| RC201_50-package-highspy-tabu-first-single-customer-column_generation_85d80e       | 3 / 3    | Optimal | 535.286   | 47         | 3712    | 0                  | Optimal        | 1984.83           | 0.730311 | passed         | 18               |
| RC201_50-package-gurobipy-tabu-first-single-customer-column_generation_85d80e      | 3 / 3    | Optimal | 535.286   | 52         | 4341    | 0                  | Optimal        | 2186.96           | 0.755237 | passed         | 20               |
| RC201_50-cpp-bindings-highspy-tabu-first-single-customer-column_generation_85d80e  | 3 / 3    | Optimal | 535.286   | 47         | 3712    | 0                  | Optimal        | 1984.83           | 0.730311 | passed         | 18               |
| RC201_50-cpp-bindings-gurobipy-tabu-first-single-customer-column_generation_85d80e | 3 / 3    | Optimal | 535.286   | 52         | 4341    | 0                  | Optimal        | 2186.96           | 0.755237 | passed         | 20               |
| RC201_50-cpp-bindings-highs-tabu-first-single-customer-column_generation_85d80e    | 3 / 3    | Optimal | 535.286   | 47         | 3712    | 0                  | Optimal        | 1984.83           | 0.730311 | passed         | 18               |
| RC201_50-cpp-native-highs-tabu-first-single-customer-column_generation_85d80e      | 3 / 3    | Optimal | 535.286   | 47         | 3712    | 0                  | Optimal        | 1984.83           | 0.730311 | passed         | 18               |
| RC201_50-cpp-native-gurobi-tabu-first-single-customer-column_generation_85d80e     | 3 / 3    | Optimal | 535.286   | 52         | 4341    | 0                  | Optimal        | 2186.96           | 0.755237 | passed         | 20               |

## Times

Medians over the finished rounds, in seconds unless said; spread: (max - min) / median.

| run                                                                                | solve_seconds median | min   | max   | spread % | master_seconds | pricing_seconds | other_seconds | lp_solved_again_seconds | integer_seconds | process_peak_memory MiB | process_processor_seconds | ms / iteration | other CPU % |
|------------------------------------------------------------------------------------|----------------------|-------|-------|----------|----------------|-----------------|---------------|-------------------------|-----------------|-------------------------|---------------------------|----------------|-------------|
| R201_50-package-highspy-tabu-first-single-customer-column_generation_85d80e        | 4.560                | 4.551 | 4.568 | 0.4      | 0.456          | 4.090           | 0.013         | -                       | 3.652           | 110.4                   | 8.840                     | 99.13          | 2           |
| R201_50-package-gurobipy-tabu-first-single-customer-column_generation_85d80e       | 4.489                | 4.472 | 4.695 | 5.0      | 0.216          | 4.245           | 0.015         | -                       | 0.039           | 84.1                    | 5.180                     | 109.49         | 2           |
| R201_50-cpp-bindings-highspy-tabu-first-single-customer-column_generation_85d80e   | 4.393                | 4.388 | 4.415 | 0.6      | 0.437          | 3.955           | 0.000         | -                       | 3.720           | 109.2                   | 8.720                     | 95.50          | 2           |
| R201_50-cpp-bindings-gurobipy-tabu-first-single-customer-column_generation_85d80e  | 4.442                | 4.429 | 4.445 | 0.4      | 0.218          | 4.217           | 0.010         | -                       | 0.039           | 83.0                    | 5.130                     | 108.34         | 2           |
| R201_50-cpp-bindings-highs-tabu-first-single-customer-column_generation_85d80e     | 4.411                | 4.406 | 4.431 | 0.6      | 0.437          | 3.974           | 0.000         | -                       | 3.718           | 104.1                   | 8.730                     | 95.90          | 2           |
| R201_50-cpp-native-highs-tabu-first-single-customer-column_generation_85d80e       | 4.126                | 4.116 | 4.132 | 0.4      | 0.442          | 3.683           | 0.000         | -                       | 3.704           | 59.1                    | 7.835                     | 89.69          | 2           |
| R201_50-cpp-native-gurobi-tabu-first-single-customer-column_generation_85d80e      | 4.270                | 4.221 | 4.284 | 1.5      | 0.206          | 4.063           | 0.001         | -                       | 0.036           | 41.4                    | 4.328                     | 104.14         | 3           |
| RC201_50-package-highspy-tabu-first-single-customer-column_generation_85d80e       | 5.357                | 5.324 | 5.361 | 0.7      | 0.305          | 5.039           | 0.014         | -                       | 0.056           | 74.9                    | 6.040                     | 113.98         | 2           |
| RC201_50-package-gurobipy-tabu-first-single-customer-column_generation_85d80e      | 6.251                | 6.236 | 6.268 | 0.5      | 0.202          | 6.033           | 0.016         | -                       | 0.021           | 82.5                    | 6.920                     | 120.21         | 2           |
| RC201_50-cpp-bindings-highspy-tabu-first-single-customer-column_generation_85d80e  | 5.172                | 5.146 | 5.181 | 0.7      | 0.286          | 4.885           | 0.000         | -                       | 0.055           | 73.4                    | 5.850                     | 110.03         | 2           |
| RC201_50-cpp-bindings-gurobipy-tabu-first-single-customer-column_generation_85d80e | 6.180                | 6.151 | 6.184 | 0.5      | 0.200          | 5.969           | 0.010         | -                       | 0.020           | 81.4                    | 6.840                     | 118.84         | 2           |
| RC201_50-cpp-bindings-highs-tabu-first-single-customer-column_generation_85d80e    | 5.172                | 5.171 | 5.193 | 0.4      | 0.286          | 4.885           | 0.000         | -                       | 0.056           | 68.3                    | 5.830                     | 110.04         | 2           |
| RC201_50-cpp-native-highs-tabu-first-single-customer-column_generation_85d80e      | 4.796                | 4.779 | 4.812 | 0.7      | 0.291          | 4.504           | 0.000         | -                       | 0.055           | 41.4                    | 4.859                     | 102.03         | 3           |
| RC201_50-cpp-native-gurobi-tabu-first-single-customer-column_generation_85d80e     | 5.967                | 5.952 | 5.973 | 0.3      | 0.186          | 5.780           | 0.001         | -                       | 0.017           | 41.2                    | 6.008                     | 114.75         | 2           |
