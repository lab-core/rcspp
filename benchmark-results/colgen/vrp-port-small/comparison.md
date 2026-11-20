# The Python VRP example against the pure layer, small tier

Quick check for the example's port (2026-11-20, laptop `leo-laptop` on power, not idle: Defender up to 75 % of a core
in three runs). Source checkout `../rcspp-dev` at #97's head `ee8b0272` (`_core` built 2026-11-16, no C++ change
since). `COLGEN_INIT=single` (the layer starts from the example's single-customer routes), 5 rounds, the two
configurations alternated inside each round:

```powershell
$env:COLGEN_INIT = "single"
.\run_matrix.ps1 --output vrp-port-small --instances "C101_25,R101_25,R201_25,RC201_12" `
    --configs "example-mip,layer-purepython-gurobipy-exact" --repeats 5
```

- old: `example-mip`, `examples/python/vrp` (python-mip over Gurobi, its own loop);
- new: `layer-purepython-gurobipy-exact`, `rcspp.column_generation` with `GurobipyMasterSolver`, exact pricing.

LP time = master + pricing + rest, the median of 5 [min–max]. The example's solve also has an integer solve
(4–19 ms), which the layer's driver does not run: left out.

| instance | LP (both) | iterations old / new | columns old / new | LP s old | LP s new | new − old | rule |
|---|---|---|---|---|---|---|---|
| C101_25 | 191.8136 | 24 / 29 | 722 / 723 | 0.099 | 0.058 [0.057–0.075] | −0.041 | pass |
| R101_25 | 618.3299 | 7 / 6 | 96 / 90 | 0.036 | 0.010 [0.009–0.015] | −0.026 | pass |
| R201_25 | 449.6598 | 14 / 11 | 2,894 / 3,247 | 0.526 | 0.351 [0.344–0.355] | −0.175 | pass |
| RC201_12 | 427.2356 | 16 / 16 | 1,085 / 1,046 | 0.141 | 0.068 [0.068–0.075] | −0.073 | pass |

- LP equal to 1e-12 relative everywhere; iterations and columns identical across the 5 rounds on each side.
- Median parts (s), old: master / pricing / rest; new the same:
  C101_25 0.059 / 0.017 / 0.023 against 0.012 / 0.043 / 0.003;
  R201_25 0.097 / 0.283 / 0.142 against 0.020 / 0.328 / 0.003;
  RC201_12 0.062 / 0.037 / 0.042 against 0.010 / 0.057 / 0.003.
  The example loses in its master (python-mip) and its rest (columns added through python-mip, prints); the layer's
  pricing is a little slower per run (columns turned into the master's arrays).
- The rule (LP equal, the new time within max(5 %, 0.25 s) of the old) passes on all four: the new side is faster.
  Under the 0.25 s floor this tier only settles the LP; the time verdict waits for the mid and long tiers.
