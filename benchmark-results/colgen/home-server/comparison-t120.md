# The home server's runs of 2026-11-21/22: the three layers at 50 customers, `T-1.20`'s matrix, R202_50

Home server (Celeron 2950M, `processes = 1`, threads 1), `e91db2963`, HiGHS 1.15.1, Gurobi 13.0.1 (WLS). Every
folder complete: 294 + 2 + 336 + 42 + 21 rounds, no timeout, no flag. Analysis: `analyze_legs.py <folder>`.
Single-customer start in `t120-*`, artificial in `layers-50-legs` and `r202-50`.

## Equality

Per master family, the three layers give the same objective, iterations and columns in every cell (`t120-exact` 32,
`t120-tabu-50` 4, `t120-tabu-r201` 2, `layers-50-legs` 28), but one: R201_50 over Gurobi, artificial start,
`cpp-native` `gurobi` 26 iterations / 18,587 columns against 25 / 18,249 for gurobipy on `package` and `cpp-bindings`
(same objective): Gurobi's C++ API and gurobipy take another path once. HiGHS: equal everywhere (one build).

## The pure layer against `cpp-bindings` over the same master (the layer alone)

`D-139` on totals: 1 failure in 66 cells, C103_50 highspy artificial +5.3 % (0.34 s; the same cell in `t120-exact`,
single-customer start, +2.1 %). Against `cpp-native` the comparison also holds the master's API (Gurobi's C++ API,
HiGHS in C++): 7 failures, all +5 to +12 % on cells of 4-12 s.

Per iteration (`D-268`), `t120-exact`, the pure layer's extra time against `cpp-bindings`:

| `cpp-bindings`' iteration | over highspy: extra ms (% of an iteration) | over gurobipy: extra ms (%) | instances |
|---|---|---|---|
| under 10 ms | +1.1 to +1.8 (+14 to +77 %), 6 cells | +0.2 to +0.6 (+3 to +23 %), 5 cells | C101_25, R101_25, C101_50, R101_50, R105_50, RC101_50 |
| 10-100 ms | +1.4 to +2.6 (+2 to +15 %), 8 cells | −0.1 to +1.6 (0 to +6 %), 9 cells | C102_25, R102_50, R103_50, R107_50, RC102_50, C101, R101, RC101 (R105_50 over gurobipy) |
| 160-210 ms | +3.6 to +7.8 (+2 to +4 %), 2 cells | +1.0 to +2.4 (+1 %), 2 cells | C102_50, C103_50 (the search's noise) |

The share is (pure − `cpp-bindings`) / `cpp-bindings` on the median solve times; both take the same iterations, so it
is also the share of an iteration. Against `cpp-native` the shares are larger (up to +83 %): they also hold the
master's API.

R101_50 over highspy, split: master +0.24 ms, pricing step +1.0 ms, loop +0.45 ms an iteration. Over gurobipy the
C++-backed layer pays the same Python master (its loop 0.17-0.24 ms an iteration calling it), so most of the gap over
highspy is the pure layer's highspy master and column adds (`F-47`: `add_columns`' fixed cost, the HiGHS options set
and restored per solve).

Tabu then exact: R201_50, RC201_50 +3.0 to +3.6 ms an iteration over highspy, +0.9 to +1.1 over gurobipy (3-4 % of
~100 ms); R201 −2.4 to −8.2 ms of 5.0-5.5 s an iteration (noise).

## The engine from Python against the engine in a C++ program

With tabu, `cpp-native` prices 5-6 % faster than `cpp-bindings` over the same masters (R201_50: 82.6 against 88.1 ms
an iteration over HiGHS, 100.9 against 104.6 over Gurobi); with exact pricing 0-6 %. Both run the same C++ search;
the difference is the call from Python (the bindings' interrupt checks, the module's build), not measured apart.

## R202_50

`cpp-native`, artificial start, stopped at 3,300 s: both masters `TimeLimit` after **one** iteration (14,193 and
13,920 columns, objective 4.17e7: the artificial columns' cost); the first exact search on the big-M duals takes
the whole hour. Integer master on those columns: 2.92e7, still artificial.

## Machine

Spread of solve times: median 0.7-1.9 % per folder. One outlier: R105_50 `package` gurobipy, one round 0.437 s
against 0.216-0.219 (101 %), likely a WLS token renewal (network) inside the run.
