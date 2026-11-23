# perf(column-generation): column adds through gurobipy cost 20-80 % more per iteration than through python-mip

<!-- markdownlint-disable MD013 -->

## Summary

`GurobipyMasterSolver.add_columns` spends 20 to 80 % more per iteration than the old Python VRP example spent adding the same columns to its python-mip model over the same Gurobi. Measured with timers around the LP solver's column adds in both programs, on Solomon VRPTW instances (C101_50, R201_25, R201_50, C201), with the columns equal at every iteration.

The rest of the package's pricing step is 3 to 7 times cheaper per column than the old example's (5.6-12 µs against 18-47 µs), so the package is faster overall; the column adds are the one part where it is slower. The totals stay within the benchmark's equivalence rule, so this is a speed-up, not a defect.

## Where to look

- How `GurobipyMasterSolver` builds each column: one `Model.addVar` with a `gp.Column` per column, against python-mip's bulk path.
- The checks and conversions around it in `MasterProblem.add_columns` (a fixed cost per call, also seen over highspy).
- Whether gurobipy's matrix API (`addMVar` with a sparse column block) gives a cheaper path.

## Target

Column adds through gurobipy no more expensive per column than python-mip's on the same instances, with LP values, iterations and columns unchanged.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
