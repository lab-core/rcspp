# The Python VRP example against the pure layer, on the home server

Overnight 2026-11-20/21 on the home server (Celeron 2950M, 2 cores, idle: other processes 0-4 % of a core on
average), checkout `b6edcb20`, python-mip 2.0.0 over Gurobi 13.0.1 (every example log says `solver: GRB`), gurobipy
13.0.1, `COLGEN_INIT=single`, `run_matrix.py` with `example-mip` and `layer-purepython-gurobipy-exact` alternated per
round. Solve = master + pricing + rest; mean of the rounds.

| instance | rounds | LP (both) | it. old / new | col. old / new | solve s old | solve s new | new − old | rule |
|---|---|---|---|---|---|---|---|---|
| C101_25 | 5 | 191.8136 | 21 / 21 | 646 / 709 | 0.22 | 0.10 | −0.12 | pass |
| R101_25 | 5 | 618.3299 | 9 / 6 | 97 / 90 | 0.08 | 0.02 | −0.06 | pass |
| R201_25 | 5 | 449.6598 | 16 / 15 | 2,991 / 3,239 | 1.46 | 1.22 | −0.24 | pass |
| RC201_12 | 5 | 427.2356 | 18 / 17 | 1,076 / 1,047 | 0.32 | 0.17 | −0.15 | pass |
| C101_50 | 3 | 363.2468 | 43–55 / 49 | 1,705–1,786 / 1,792 | 0.65 | 0.45 | −0.20 | pass |
| R201_50 | 3 | 756.3386 | 30 / 28 | 12,619 / 13,171 | 22.59 | 21.16 | −1.43 (−6 %) | pass |
| RC101_50 | 3 | 828.3619 | 22 / 28 | 631 / 737 | 0.31 | 0.26 | −0.05 | pass |
| C201 | 3 | 591.5566 | 165–184 / 344 | 36,972–37,171 / 52,911 | 64.68 | 64.22 | −0.46 (−1 %) | pass |
| R201 | 2 | 1084.9907 | 42–46 / 43 | 81,520–82,640 / 73,741 | 3,302.4 | 2,234.5 | −1,068 (−32 %) | pass |

- LP equal everywhere; the layer repeats exactly; the example varies on C101_50, C201 and R201 (`F-59` (3)).
- C201: the layer takes 344 iterations here against 235 on the laptop (Gurobi's path differs between machines,
  `server-setup.md` step 7), so its master's lead (33 s against 41 s) is eaten by more pricing (31 s against 17 s).
- R201: pricing is 97-99 % of both; 52 s an iteration for the layer, 70-78 s for the example, on different dual
  sequences (no replay on the Python side to separate the code from the path); master 7.7 s against 58 s, rest 0.07 s
  against 8 s; peak memory 292-299 MB against 499-521 MB.
