# perf(column-generation): the highspy master costs 1-2.6 ms more per iteration than the C++ example's HiGHS master

<!-- markdownlint-disable MD013 -->

## Summary

Over HiGHS, the pure-Python column generation layer (`rcspp.column_generation` with `HighspyMasterSolver`) spends 1.1 to 2.6 ms more per iteration than the C++ column generation example driven from Python, on the same instances, with the same iterations and columns. Over gurobipy the same gap is 0.2 to 1.6 ms. On cheap iterations (under 10 ms, exact pricing on 50-customer C1, R1 and RC1 Solomon instances) this is +14 to +77 % per iteration over highspy, against +3 to +23 % over gurobipy. Totals stay within the benchmark's equivalence rule, so nothing is broken; this is a speed-up for users whose iterations are cheap.

## Where the time goes

Measured with timers around the layer's components (R101_50, exact pricing, single-customer start), per iteration: the master solve +0.24 ms, the pricing step +1.0 ms, the loop +0.45 ms. Most of the gap over highspy is in the master solver and in adding columns:

1. `MasterProblem.add_columns`: a fixed cost of about 0.4 ms per call (the master solver's checks, building the `ColumnBatch`, de-duplication), whatever the number of columns.
2. `HighspyMasterSolver.solve` sets and restores about 8 HiGHS options at every solve.
3. The `duals` and `objective` reads copy arrays at each iteration.

## Target

Over highspy, the extra time per iteration at or below the gurobipy one, with LP values, iterations and columns unchanged. Measure with the column generation benchmark (`benchmarks/column_generation/`): the `package` and `cpp-bindings` layers over `highspy` and `highs`, exact pricing, single-customer start, on C101_50, R101_50, RC101_50, one process, several rounds on an idle machine.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
