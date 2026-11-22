# Summary of r202-50

- 2 runs, 1 rounds, 1 processes
- 2 / 2 rounds finished (0 failed checks), 0 timeouts, 0 not finished
- last event: finished at 2026-11-21T21:16:05.974+00:00

## Flags

None.

## Records

- source.commit: e91db2963110807689e7134fefc37d5ce4cae045
- source.uncommitted_changes: no
- source.branch: feature/column-generation-cpp-vrp-example
- machine.label: home-server
- machine.processor: Intel(R) Celeron(R) CPU 2950M @ 2.00GHz
- versions: python 3.12.16, rcspp 0.0.2.dev1, numpy 2.5.5
- solvers: highs 1.15.1, gurobi 13.0.1

## Results

| run                                                                 | finished | status    | objective   | iterations | columns | artificial_columns | integer_status | integer_objective | gap | solution_check | solution_columns |
|---------------------------------------------------------------------|----------|-----------|-------------|------------|---------|--------------------|----------------|-------------------|-----|----------------|------------------|
| R202_50-cpp-native-highs-exact-artificial-column_generation_ad8532  | 1 / 1    | TimeLimit | 4.17253e+07 | 1          | 14193   | 50                 | Optimal        | 2.92081e+07       | -   | -              | 13               |
| R202_50-cpp-native-gurobi-exact-artificial-column_generation_ad8532 | 1 / 1    | TimeLimit | 4.17253e+07 | 1          | 13920   | 50                 | Optimal        | 2.92081e+07       | -   | -              | 13               |

## Times

Medians over the finished rounds, in seconds unless said; spread: (max - min) / median.

| run                                                                 | solve_seconds median | min      | max      | spread % | master_seconds | pricing_seconds | other_seconds | lp_solved_again_seconds | integer_seconds | process_peak_memory MiB | process_processor_seconds | ms / iteration | other CPU % |
|---------------------------------------------------------------------|----------------------|----------|----------|----------|----------------|-----------------|---------------|-------------------------|-----------------|-------------------------|---------------------------|----------------|-------------|
| R202_50-cpp-native-highs-exact-artificial-column_generation_ad8532  | 3300.354             | 3300.354 | 3300.354 | 0.0      | 0.001          | 3300.353        | 0.000         | 0.277                   | 0.155           | 174.9                   | 3299.996                  | 3300353.52     | 1           |
| R202_50-cpp-native-gurobi-exact-artificial-column_generation_ad8532 | 3300.397             | 3300.397 | 3300.397 | 0.0      | 0.001          | 3300.395        | 0.001         | 0.019                   | 0.034           | 181.5                   | 3307.697                  | 3300396.87     | 1           |
