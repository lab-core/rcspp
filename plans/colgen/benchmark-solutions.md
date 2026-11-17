# Benchmark runner: solutions, integer solve, per-iteration data

Design chat 2026-11-17, after step 5 (`F-63`); decided by the user (`D-203`–`D-206`), expected to change as the
work goes. Builds on `benchmark-runner.md` (§4 output, §5 instances, §6 statistics). Three parts, in this order:

| # | part | when | decisions |
|---|---|---|---|
| 1 | `--overwrite`; `solution.json` with the last LP solution; `results/` and the default `--output` | fixup of step 5 (`61cf282c`), before step 6 | `D-203`, `D-207`, `Q-61` |
| 2 | the integer solve at the end; the instance's resolved solution and its check | after step 6, both runners at once | `D-204`, `D-205` |
| 3 | per-iteration data | with `T-1.20` | `D-206` |

## 1. Collisions and the LP solution (`D-203`)

- **Collision:** the output folder holds a run, meaning any file the runner writes (`run.toml`, `command.txt`,
  `metadata.json`, `result.json`, `solution.json`). Refused (exit 2, nothing touched) unless `--overwrite`. An empty
  folder or one with other files only (a benchmark's `log.txt`) is no collision.
- **`--overwrite`:** removes the whole folder, then writes it again; reached only after a collision, so a folder
  without run files is never removed (`--output . --overwrite` deletes nothing).
- `benchmark.py` never passes it: it skips finished folders, removes unfinished ones itself before restarting them.
  Rerunning a `command.txt` in place is refused without `--overwrite`; the round-trip tests rerun into a copy.
- **`solution.json`:** the last master LP: objective; each column with a non-zero value (value, cost, path as node
  ids, rows covered); artificial columns in use, apart; the duals, one per row. `result.json` gains only counts.
- A `solution` collector in `statistics` turns it on (`Q-61`: on by default?); its cost: one `primal` and one
  `duals` call after the run.
- Done 2026-11-17 (`F-64`, folded into step 5, `374d2ea6`); on by default (`D-208`); `results/` and the default
  `--output` (`D-207`). Both masters keep the values after `restore_user_model()`, but lose them once columns were
  added after the last solve: a run stopped at a limit after pricing has `"lp": null` and `missing`.

## 2. Integer solve and resolved solution (`D-204`, `D-205`)

The integer master over the generated columns: a feasible integer solution and an upper bound, **not** the integer
optimum (no branching). Named so everywhere ("integer master over the generated columns"), with the gap to the LP.

| piece | where | contents |
|---|---|---|
| on or off, limits | run option, a new block `integer` | `solve` (bool), `timeout_s`, `relative_gap`, `column_type` (`integer`, `binary`), `threads`: the package's `IntegerParams` |
| default | the instance | whether the integer solve is on and its `column_type`, as an instance type declares its starts (`D-191`); Solomon: on, `binary` |
| raw solutions | the layer, in `solution.json` | `lp` (§1) and `integer`: status, objective, columns with non-zero values |
| flat fields | `result.json` | `integer_status`, `integer_objective`, `integer_seconds`, `gap` (null when off) |
| resolved solution | the instance, in `solution.json` | Solomon: routes as customer sequences, vehicles, total distance |
| check | the instance | Solomon: each customer once, windows and capacity, cost recomputed from the file; a failed check is an error status, not a crash |

- `column_type`: with `= 1` rows, `integer` and `binary` give the same feasible set; `binary` gives the solver the
  bounds. Kept as a parameter because another instance type may need general integers; costs one field, the
  package has it (`IntegerParams.column_type`, default `Integer`).
- `integer_seconds` stays out of `solve_seconds`: column generation timings compare with the integer solve on or off.
- A time limit always: on R2 instances with many columns the integer solve can take far longer than the LP.
- Artificial columns still in the master: the integer solve may use them; reported as artificial in use, no
  resolved solution.
- Masters build different column sets, so their integer results differ: the layers' drift test stays on the LP.
- To check first: that `solve_integer` makes the generated columns integer on both masters and on `cpp-bindings`;
  the loop's `solve_restricted_master_integer` (`F-64` (3)) may already do it. After a limit hit after pricing, the
  last LP's values are gone (`F-64` (2)): decide whether the integer solve (which uses the final master) also
  re-solves the LP for `solution.json`, labelled as such.
- `cpp-native` (`T-1.19`) gets the integer solve and the raw solution; the resolved solution and its check stay in
  Python (`benchmark.py` can run them on its `solution.json`).
- Done 2026-11-17 (`094beacd`, `F-67`): both checks pass (`F-67` (1)–(2)); the LP solved again after a limit
  (`D-211`), the gap (`D-212`), defaults and a failed check (`D-213`). `solution.json`: `lp` (with `solved_again`),
  `lp_missing`, `integer`, `resolved` (with its `errors`), `resolved_missing`; `result.json` also has
  `lp_solved_again_seconds` and `solution_check`. For `benchmark.py`: `SolomonInstance.check_solution(resolved,
  objective)` reruns the check on a `solution.json`; exit code 1 is a finished run with a wrong solution.

## 3. Per-iteration data (`D-206`)

A collector off by default, its own file (`iterations.jsonl`, one line per iteration), read through the package's
`on_iteration` hook (iteration, objective, new and total columns, elapsed time) and the master the runner holds.

| data | size | default in the collector |
|---|---|---|
| objective, columns, elapsed | small | on |
| duals | one per row | on |
| new columns with their paths | up to ~140,000 per call (uncapped greedy, `F-48`) | off, a parameter |
| later (`T-2.5`): Lagrangian bound, per-stage counts, most negative reduced cost | small | each its own parameter |

- It costs the loop time: the summary flags runs with it on, and does not compare their times with runs without.
- To check: gurobipy's duals after columns were added in the iteration (as §1); whether the C++ loop on
  `cpp-bindings` has such a hook; `cpp-native` needs its own.
