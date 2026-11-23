# perf(examples): the C++ column generation example's column adds slow down as the master grows

<!-- markdownlint-disable MD013 -->

## Summary

In the C++ column generation example (`examples/column_generation/`), adding columns to the master costs more per column as the master grows, while the older C++ VRP example's own loop stays flat. Measured on Solomon VRPTW instances with the Gurobi master, the C++ VRP example on the example layer against the same example before its port (same pricing, same columns at every iteration):

| instance | per column, example layer | per column, old loop | per iteration, example layer | per iteration, old loop |
|---|---|---|---|---|
| small and mid instances | 2-3 µs | 0.5-1.8 µs | | |
| C201 (100 customers) | 15 µs | 1.25 µs | 4.9 ms | 0.3 ms |

Everything else in the example layer is as fast as or faster than the old loop, and the totals are far ahead (R201: 1,253 s against 5,721 s) because it takes fewer iterations; adding columns is its one slower part. The slowdown is in `GurobiMasterSolver::add_columns` or in what the master problem does around it; not traced yet.

## To do

1. Trace where the time goes as the master grows: the master problem's bookkeeping, the batch built for the solver, or the Gurobi C++ calls (`GRBaddvars` once per batch, or one call per column).
2. Fix it, or document why it cannot be cheaper.

The column generation benchmark (`benchmarks/column_generation/`, layer `cpp-native` or `cpp-bindings`, master `gurobi`) on C201 shows it; per-iteration timings would make the growth visible directly.

This concerns the C++ example only: the Python package does not use this code.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
