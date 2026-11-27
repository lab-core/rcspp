<!-- markdownlint-disable -->
<!-- The PR description, paste-ready: the first line is the title, the rest the body. -->

feat(python): debug checks of a column generation run

# Debug checks of a column generation run

A master solver or a pricer of one's own can be wrong in ways that still let a run end with a plausible LP value:
duals of the wrong sign, a pricer that applies the duals to the wrong rows, duals read from another solve. Four
checks now compare each answer against the others, after every LP solve and every pricing call, and stop the run at
the first that fails with an error naming the check, the iteration and the values. Each check has its own switch, all
off by default, and with every switch off a run costs no more than before. The Python package, the C++ column
generation example and the benchmark all get them.

Base: `feature/column-generation-all-features` (#128).

## The checks

| switch | what must hold | cost per iteration when on |
|---|---|---|
| `pricer_reduced_costs` | each new column's reduced cost (`Solution.cost`) is its cost minus the duals of its rows, the convexity row's left out | the new columns' entries |
| `dual_signs` | a `<=` row's dual is at most 0, a `>=` row's at least 0, a row with no bound's 0 | one pass over the rows |
| `master_reduced_costs` | no column of the master has a negative reduced cost, and the columns the LP solution uses have 0 | every entry of the master, and one read of every column's value |
| `objective` | the LP value is the cost of its columns' values; checked only when the user's own variables cost nothing | one read of every column's value |

All use the run's reduced-cost tolerance (the larger of the tolerances' `reduced_cost` and the master solver's
`dual_tolerance()`), scaled by the size of the costs and duals involved. What they cannot see is left out on purpose
and documented: a row with two different finite bounds (its dual's sign depends on the bound the LP reaches), the
variables a user adds to the master's model themselves, and a constant in that model's objective (which fails
`objective`). A starting column's reported reduced cost is not checked: it was never priced.

## What changes

**The search engine**

- `Solution.cost`'s comment said the opposite of what the code does: `Solution.cost` is the cost resource's value at
  the end of the path, the path's reduced cost once `update_reduced_costs` has written the duals into it, while
  `column.cost` stays the sum of the arcs' original costs. Comment only.

**The package** (`rcspp.column_generation`)

- `DebugChecks`, a frozen record of the four switches, as `ColumnGenerationParams.debug_checks`.
- `DebugCheckError(RuntimeError)`, with `check`, `iteration` and `message`; the message names the row or column, both
  values, the tolerance, and how many more failed the same check in that iteration. It is raised before the iteration
  hook, and the master solver still restores the user's model.
- The checks over the whole master keep the master's columns as numpy arrays (no `scipy.sparse`), built only when a
  check needs them and grown with the master; the rows' bounds and the user's own costs are read once per run.
- An artificial column's `MasterColumn.solution` now holds its column (its cost and its one entry) instead of nothing,
  so every column of the master reads the same way. The tolerances' cost scale still reads the real columns only.

**The C++ column generation example** (`examples/column_generation/`)

- The same in C++: `DebugChecks` as `ColumnGenerationParams::debug_checks`, `DebugCheckError` deriving from
  `std::runtime_error` with `check()`, `iteration()` and `message()`, the checks in `debug_checks.hpp/.cpp`; artificial
  columns keep their entry there too. The bindings raise a Python `DebugCheckError(RuntimeError)` with the same three
  fields; stubs and `cpp_column_generation`'s `ColumnGenerationParams` follow.

**The benchmark** (`benchmarks/column_generation/`)

- `column_generation.debug_checks`: a list of the checks' names, in any order, kept in one order so that a run's name
  does not depend on how they were written; left out or empty, the run's name is as before. `cpp-native` takes
  `--debug-checks` and refuses an unknown name. A failed check ends the run as `crashed`, its output holding the
  error.
- Both all-features files now make every check in every run, and their test checks that each run did.

**Docs**

- `docs/advanced/column-generation.md`: a section "Checking a master solver or a pricer".
- `benchmarks/column_generation/OPTIONS.md` (`debug_checks`), `CHANGELOG.md` (one entry under Added).

## Tests

- **Package** (725 pass, +20): each check fails on a scripted master or pricer that breaks it (a reported reduced cost
  off by 1; a `>=` then a `<=` dual of the wrong sign; the duals of the previous solve; a used column whose reduced
  cost is not 0; an LP value 1 above its columns' cost), stays silent on noise below the tolerance and on what it skips
  (starting columns, `=` and ranged rows, a model whose own variables cost something), and misses the same faults when
  off; the error's fields and message; a failure before the hook, with the model restored; columns added between two
  runs are checked by the second. The package's all-features test runs with every check on over highspy and gurobipy.
- **C++**: `test_column_generation_debug_checks.hpp` (17 tests, the package's cases); ctest passes (896 run). The
  example's Python tests: 124 pass, 2 skipped (nine new: the error raised
  through the bindings with its fields, a fault per check over a master solver written in Python, and every check on
  real masters: HiGHS, highspy and gurobipy).
- **Benchmark** (536 pass, +15): the option's refusals and order, no change to a run's name without it, the
  `cpp-native` refusal, a run with every check per layer and start, and the quick all-features file with every check
  in every run.
- **Cost when off**, by hand: the package before and after this PR over the same build, interleaved rounds in fresh
  processes (highspy, one exact stage, big-M start). Medians: C101_25 (10 rounds) 0.0361 s before, 0.0365 s after,
  0.0485 s with every check on (+34 %); R201_25 (5 rounds) 0.3998 s, 0.4037 s and 0.4769 s (+19 %). Iterations and LP
  values were equal in every round, and every check passed.
- pre-commit and Pyright pass on the changed files. The two package tests that failed now and then in local full runs,
  `test_a_repeated_row_warns_once_per_master` and `test_a_thread_count_highs_refuses_runs_with_the_count_in_force`,
  fail on the drive letter's case only (`c:\` against `C:\` between `__file__` and the warning's file name, when
  pytest starts from a directory spelled `c:\`); not this PR's code.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
