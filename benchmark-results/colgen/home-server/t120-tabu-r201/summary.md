# Summary of t120-tabu-r201

- 7 runs, 3 rounds, 1 processes
- 21 / 21 rounds finished (0 failed checks), 0 timeouts, 0 not finished
- last event: finished at 2026-11-22T06:35:59.590+00:00

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

| run                                                                            | finished | status  | objective | iterations | columns | artificial_columns | integer_status | integer_objective | gap      | solution_check | solution_columns |
|--------------------------------------------------------------------------------|----------|---------|-----------|------------|---------|--------------------|----------------|-------------------|----------|----------------|------------------|
| R201-package-highspy-tabu-first-single-customer-column_generation_ad8532       | 3 / 3    | Optimal | 1084.99   | 67         | 37401   | 0                  | Optimal        | 1849.57           | 0.413381 | passed         | 66               |
| R201-package-gurobipy-tabu-first-single-customer-column_generation_ad8532      | 3 / 3    | Optimal | 1084.99   | 63         | 32801   | 0                  | Optimal        | 1859.11           | 0.416393 | passed         | 66               |
| R201-cpp-bindings-highspy-tabu-first-single-customer-column_generation_ad8532  | 3 / 3    | Optimal | 1084.99   | 67         | 37401   | 0                  | Optimal        | 1849.57           | 0.413381 | passed         | 66               |
| R201-cpp-bindings-gurobipy-tabu-first-single-customer-column_generation_ad8532 | 3 / 3    | Optimal | 1084.99   | 63         | 32801   | 0                  | Optimal        | 1859.11           | 0.416393 | passed         | 66               |
| R201-cpp-bindings-highs-tabu-first-single-customer-column_generation_ad8532    | 3 / 3    | Optimal | 1084.99   | 67         | 37401   | 0                  | Optimal        | 1849.57           | 0.413381 | passed         | 66               |
| R201-cpp-native-highs-tabu-first-single-customer-column_generation_ad8532      | 3 / 3    | Optimal | 1084.99   | 67         | 37401   | 0                  | Optimal        | 1849.57           | 0.413381 | passed         | 66               |
| R201-cpp-native-gurobi-tabu-first-single-customer-column_generation_ad8532     | 3 / 3    | Optimal | 1084.99   | 63         | 32801   | 0                  | Optimal        | 1859.11           | 0.416393 | passed         | 66               |

## Times

Medians over the finished rounds, in seconds unless said; spread: (max - min) / median.

| run                                                                            | solve_seconds median | min     | max     | spread % | master_seconds | pricing_seconds | other_seconds | lp_solved_again_seconds | integer_seconds | process_peak_memory MiB | process_processor_seconds | ms / iteration | other CPU % |
|--------------------------------------------------------------------------------|----------------------|---------|---------|----------|----------------|-----------------|---------------|-------------------------|-----------------|-------------------------|---------------------------|----------------|-------------|
| R201-package-highspy-tabu-first-single-customer-column_generation_ad8532       | 374.649              | 374.064 | 375.355 | 0.3      | 24.123         | 350.489         | 0.037         | -                       | 3.481           | 192.7                   | 378.770                   | 5591.78        | 1           |
| R201-package-gurobipy-tabu-first-single-customer-column_generation_ad8532      | 314.847              | 313.184 | 316.312 | 1.0      | 3.787          | 311.007         | 0.054         | -                       | 1.218           | 175.5                   | 317.640                   | 4997.57        | 1           |
| R201-cpp-bindings-highspy-tabu-first-single-customer-column_generation_ad8532  | 375.088              | 372.844 | 375.520 | 0.7      | 24.531         | 350.557         | 0.000         | -                       | 3.443           | 188.6                   | 379.260                   | 5598.33        | 1           |
| R201-cpp-bindings-gurobipy-tabu-first-single-customer-column_generation_ad8532 | 315.365              | 314.067 | 315.546 | 0.5      | 3.784          | 311.548         | 0.033         | -                       | 1.227           | 180.1                   | 318.280                   | 5005.80        | 1           |
| R201-cpp-bindings-highs-tabu-first-single-customer-column_generation_ad8532    | 374.809              | 374.044 | 376.775 | 0.7      | 24.608         | 350.250         | 0.000         | -                       | 3.450           | 183.3                   | 378.940                   | 5594.16        | 1           |
| R201-cpp-native-highs-tabu-first-single-customer-column_generation_ad8532      | 371.086              | 370.626 | 378.210 | 2.0      | 24.371         | 346.690         | 0.000         | -                       | 3.422           | 127.6                   | 374.462                   | 5538.60        | 1           |
| R201-cpp-native-gurobi-tabu-first-single-customer-column_generation_ad8532     | 315.328              | 314.917 | 316.622 | 0.5      | 3.795          | 311.538         | 0.003         | -                       | 1.211           | 118.8                   | 317.332                   | 5005.20        | 1           |
