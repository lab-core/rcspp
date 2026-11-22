# Summary of t120-tabu-50

- 14 runs, 3 rounds, 1 processes
- 42 / 42 rounds finished (0 failed checks), 0 timeouts, 0 not finished
- last event: finished at 2026-11-22T04:32:31.430+00:00

## Flags

None.

## Records

- source.commit: e91db2963110807689e7134fefc37d5ce4cae045
- source.uncommitted_changes: no
- source.branch: feature/column-generation-cpp-vrp-example
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
| R201_50-package-highspy-tabu-first-single-customer-column_generation_85d80e        | 4.661                | 4.604 | 4.664 | 1.3      | 0.454          | 4.194           | 0.013         | -                       | 3.654           | 110.4                   | 8.940                     | 101.33         | 2           |
| R201_50-package-gurobipy-tabu-first-single-customer-column_generation_85d80e       | 4.590                | 4.589 | 4.626 | 0.8      | 0.216          | 4.359           | 0.015         | -                       | 0.039           | 84.0                    | 5.290                     | 111.94         | 2           |
| R201_50-cpp-bindings-highspy-tabu-first-single-customer-column_generation_85d80e   | 4.494                | 4.472 | 4.504 | 0.7      | 0.441          | 4.052           | 0.000         | -                       | 3.714           | 109.1                   | 8.810                     | 97.69          | 2           |
| R201_50-cpp-bindings-gurobipy-tabu-first-single-customer-column_generation_85d80e  | 4.555                | 4.516 | 4.763 | 5.4      | 0.217          | 4.290           | 0.009         | -                       | 0.039           | 83.0                    | 5.230                     | 111.09         | 3           |
| R201_50-cpp-bindings-highs-tabu-first-single-customer-column_generation_85d80e     | 4.514                | 4.510 | 4.563 | 1.2      | 0.443          | 4.071           | 0.000         | -                       | 3.720           | 102.7                   | 8.830                     | 98.12          | 2           |
| R201_50-cpp-native-highs-tabu-first-single-customer-column_generation_85d80e       | 4.242                | 4.233 | 4.243 | 0.2      | 0.443          | 3.798           | 0.000         | -                       | 3.732           | 57.5                    | 7.977                     | 92.22          | 2           |
| R201_50-cpp-native-gurobi-tabu-first-single-customer-column_generation_85d80e      | 4.342                | 4.329 | 4.371 | 1.0      | 0.207          | 4.135           | 0.001         | -                       | 0.036           | 41.5                    | 4.400                     | 105.90         | 3           |
| RC201_50-package-highspy-tabu-first-single-customer-column_generation_85d80e       | 5.455                | 5.443 | 5.472 | 0.5      | 0.303          | 5.140           | 0.013         | -                       | 0.056           | 74.9                    | 6.150                     | 116.07         | 2           |
| RC201_50-package-gurobipy-tabu-first-single-customer-column_generation_85d80e      | 6.355                | 6.349 | 6.361 | 0.2      | 0.202          | 6.137           | 0.016         | -                       | 0.020           | 82.4                    | 7.030                     | 122.21         | 2           |
| RC201_50-cpp-bindings-highspy-tabu-first-single-customer-column_generation_85d80e  | 5.286                | 5.277 | 5.296 | 0.4      | 0.290          | 4.996           | 0.000         | -                       | 0.055           | 73.2                    | 5.950                     | 112.47         | 2           |
| RC201_50-cpp-bindings-gurobipy-tabu-first-single-customer-column_generation_85d80e | 6.296                | 6.273 | 6.322 | 0.8      | 0.200          | 6.086           | 0.011         | -                       | 0.020           | 81.4                    | 6.960                     | 121.08         | 2           |
| RC201_50-cpp-bindings-highs-tabu-first-single-customer-column_generation_85d80e    | 5.312                | 5.306 | 5.328 | 0.4      | 0.292          | 5.020           | 0.000         | -                       | 0.055           | 68.2                    | 5.960                     | 113.02         | 2           |
| RC201_50-cpp-native-highs-tabu-first-single-customer-column_generation_85d80e      | 4.873                | 4.865 | 4.900 | 0.7      | 0.292          | 4.581           | 0.000         | -                       | 0.055           | 41.5                    | 4.930                     | 103.68         | 3           |
| RC201_50-cpp-native-gurobi-tabu-first-single-customer-column_generation_85d80e     | 6.074                | 6.044 | 6.113 | 1.1      | 0.187          | 5.887           | 0.001         | -                       | 0.017           | 41.6                    | 6.115                     | 116.82         | 3           |
