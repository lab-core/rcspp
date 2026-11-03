# Pure-Python column generation layer (`T-1.15`): design

Status: **accepted 2026-11-03**, code started: step A done (§5 commits 1–2, record under §5). On top of the stack
(`D-119`), highspy in place (`D-120`), mirror the C++ layer (`D-121`), VRP not ported (`D-122`), quick runs first
(`D-123`), one highspy master solver (`D-124`), pool postponed (`D-125`, §7), no standalone C++ run (`D-126`). Built on
`D-111`, `D-112`, `D-114`, `D-116`, `D-117`, `D-118`.

## 1. Mirror of the C++ layer (`D-121`)

"Mirror" means the **Python API of the C++-backed package** (`python/src/rcspp/column_generation/__init__.py`):
same names, arguments, defaults, statuses; numpy arrays where the bindings return them. Internals are Python.

| C++-backed piece | pure-Python mirror |
|---|---|
| `MasterSolver` (all methods, `add_rows` and `farkas` with their defaults), `Capabilities` | same |
| `MasterProblem` (`add_rows`, `add_pricer`, `add_columns`, columns, artificial ids…), `MasterColumn`, `ColumnTag`, `ColumnType`, `ConvexityRow`, `RowBatch`, `RowRange`, `RowSense`, `ColumnBatch` | same (`ColumnBatch`: numpy CSC arrays, `D-16`) |
| `Pricer` (`price`, `update`, `cost_scale`), `PricingRequest`, `PricingResult`, `PricingStatus`, `PricingChanges` | same |
| `PricingStage`, `GraphPricer` (C++ stages and `PricingStrategy` inside) | same Python signatures; stages run `ResourceGraph.solve` |
| `Tolerances`, `AbsoluteTolerances`, `RelativeTolerances`, `ToleranceContext`, `default_*` | same |
| `MasterInitialization`, `ArtificialColumns` (`automatic_cost_factor`), `NoInitialization` | same, factor 1e4 (fix B) |
| `ColumnGeneration`, `ColumnGenerationParams` (hooks `on_iteration`, `on_converged`), `ColumnGenerationResult`, `ColumnGenerationStatus`, `ConvergenceAction`, `ConvergenceContext`, `IterationInfo`, `Statistics`, `StatisticsParams` | same |
| `master_solver_self_test`, `MasterSolverSelfTestReport`, `default_self_test_tolerance` | ported (`master_solver_self_test.cpp`, 232 lines) |
| registry (`make_tolerances`, `register_*`, `*_names`) | same module, pointed at the Python classes |
| `GurobipyMasterSolver` | the same class, base swapped |
| `HighspyMasterSolver` | in place, fix A (`D-120`) |

Deviations that stay (accepted 2026-11-03):

| deviation | why |
|---|---|
| `HighspyMasterSolver` in place, no `write_back` | `D-120` |
| no `HighsMasterSolver`: a user without a model passes an empty `highspy.Highs()` | `D-124`: the C++ one existed only to fill the C++ HiGHS from arrays |
| fix A (cold retry) and fix B (factor 1e4) only here | the C++ layer is frozen (`D-116`, `D-118`) |
| retried solves counted on `HighspyMasterSolver` (an attribute), not in `Statistics` | keeps `Statistics` identical |
| the time turning `Solution` objects into a `ColumnBatch`, on `GraphPricer` (an attribute) | `D-114` asks to measure it; keeps `Statistics` identical |
| no GIL release or Ctrl-C machinery in the loop | plain Python: `KeyboardInterrupt` comes from `solve` itself |

## 2. Where it lives (`D-119`)

- On top of the stack: branch `feature/column-generation-pure-python` from `feature/column-generation-packaging`
  (the top of the stack, #80: same build options as the measurements' `build-python-highs`).
- Package `python/src/rcspp/python_column_generation/`, a sibling of `column_generation/`: no file and no
  import shared, so no collision; importing it never loads `_core.column_generation`. Same class names in both.
- One build runs both layers: the comparison driver picks the package by an option.
- The last PR tidies up: drops the C++ layer and renames this package to `column_generation`, or the reverse.

## 3. Data across the boundary (`D-16`, `D-114`)

| data | form |
|---|---|
| duals | numpy `float64` from the master solver, passed as is to `update_reduced_costs` |
| columns found | `solve` returns one `Solution` per path (`D-114`'s first candidate); `MasterProblem.add_columns` takes the list, as in C++, and builds one CSC `ColumnBatch` from `solution.column.to_arrays()` |
| into highspy | one `addCols` with the batch arrays |
| into gurobipy | one `addVar(column=...)` per column (no batch call exists, `phase-1-python.md` §7) |

- No `scipy.sparse`. A C++ helper returning a whole batch only if the conversion time shows it costs (`D-112`).
- The column's cost is `solution.column.cost` (the real cost), not `solution.cost` (the reduced cost, `F-01`).

## 4. Fix A + B (`D-118`)

- **A**, in `HighspyMasterSolver.solve_relaxation`: status `kSolveError`, `kUnknown` or `kNotset` → `clearSolver()`,
  `run()` once more. Test: a `Highs` whose first `run` fails (patched); the `F-41` LP too if a small dump reproduces it.
- **B**: `ArtificialColumns.automatic_cost_factor = 1e4`. C stays out until a run converges on artificial columns.
- Docstrings: the big-M start slows exact pricing; a tabu stage first is the suggested setup (`D-117`, `F-41`).

## 5. Steps and commits

Each commit passes its own tests (pytest `-k` while iterating, the full suite once at the end). Tests: the C++-backed
package's tests, run again on this package (same values: toy LP 37.5, duals (11.5, 12.5, 13.5), MIP 44, fleet 2 / 1,
four customers 40).

| # | commit | contents |
|---|---|---|
| 1 | `feat(python): pure-Python column generation types and master solver contract` | types, enums, params, `ColumnBatch`, `RowBatch`, `MasterSolver`, `Pricer` |
| 2 | `feat(python): pure-Python master solver over highspy` | `HighspyMasterSolver` in place, fix A |
| 3 | `feat(python): pure-Python master solver over gurobipy` | the port; skips without a license (`D-50`) |
| 4 | `feat(python): pure-Python master problem, tolerances and initializations` | `MasterProblem`, fix B, the self-test |
| 5 | `feat(python): pure-Python graph pricer` | `PricingStage`, `GraphPricer` |
| 6 | `feat(python): pure-Python column generation loop` | `ColumnGeneration`, hooks, statistics; registry; end-to-end tests |

Coverage: the 90 % Python gate.

Step A record (2026-11-03, on `feature/column-generation`; 108 tests in the three new files, run alone, none of the
full suite; mypy `--strict` clean on the package):

- Commit 1 `efe90cb1`: `types.py`, `pricer.py`, `master_solver.py`, `__init__.py`; 61 tests (mirror + shared behavior,
  the shared ones run on both layers). `ColumnBatch` and `RowBatch` are classes with read-only numpy properties and
  the same checks and messages; the options are keyword-only dataclasses; required methods raise
  `NotImplementedError` (a `RuntimeError`) with the C++ message `Tried to call pure virtual function "…"`.
- Commit 2 `a54acd0a`: `master_solver_support.py` (ports `master_solver_support.hpp`: the column checks and the
  id → solver column map, for step C's gurobipy port too), `master_solvers/highspy_master_solver.py`; 47 tests,
  including the master solver self-test's LP replayed by hand (the real self-test is commit 4).
- **Mirror test** (`tests/python/test_python_column_generation_mirror.py`): reads the C++ layer's parameters from the
  pybind11 docstrings (no `inspect.signature` there). `PENDING` lists the names not written yet: each later step
  removes its names; at the end it is empty. Deviations that stay are `ABSENT` and `ONLY_IN_CPP_LAYER` /
  `ONLY_IN_PYTHON_LAYER`. The test fails when a default, a keyword-only marker or a parameter name differs
  (checked by changing one default).
- Differences from the plan:
  - `HighspyMasterSolver` keeps `column(id)` (the user's model column of an id) and the refusal of semi-continuous
    and semi-integer columns (HiGHS's `solve_relaxation` leaves them semi: measured); it drops `write_back`,
    `model_columns`, `model_rows`; `retried_solves` is the one extra attribute (`F-42`).
  - highspy 1.15.1 has no `zeroAllClocks`: a solve sets `time_limit` to `getRunTime()` + its timeout. The MIP solver's
    own clock (which starts at 0) may then overrun the integer solve's limit; not measured.
  - The thread-count warning is `warnings.warn(RuntimeWarning)`, not the C++ logger. `ColumnBatch.starts` and
    `row_indices` are `uint32` as in C++.
  - Fix A is the cold retry of `D-118` (`clearSolver()` then `run()`; the C++ layer also restores the basis). The
    `F-41` LP was not added to the tests: the R201_25 master is 2032 columns, too large for a test file, so the retry
    is tested with patched statuses (error, unknown, not set, a refused run, a second failure, no retry for the
    integer solve).
  - Pre-commit's `docformatter` rewraps docstrings: run `pre-commit run --files …` before committing (the venv's
    `Scripts` folder must be on `PATH`).

## 6. Comparison with the C++ layer (`D-123`)

- **First, quick runs** as before the presentation: `drivers.py` gets a layer option for the pure-Python package;
  a few small instances (C101_25, R201_25, RC201_12), exact pricing, single-customer start, 1 repeat. Only to check
  that LP values match and nothing is far off.
- **Then** (to plan after the quick runs): the full matrix and the rule that decides `Q-41`.

## 7. The pool (`Q-47` → `D-125`: postponed to `T-2.2`; the analysis, kept for then)

- Today: `PricingPool` (`python/src/rcspp/pricing_pool.py`) = the C++ `SolutionPool` plus a numpy copy in shared
  memory (for worker processes); `price(duals, threshold)` returns pool ids and reduced costs, best first; activity
  (`age`, `use_count`), `remove_stale`, filters on rows and arcs (for branching). No loop uses it (`F-04`); the C++
  layer has none, so the mirror (`D-121`) has none either.
- A pool priced first only finds columns **not in the master** (an optimal LP leaves its own at reduced cost ≥ 0).
  They come from: a cap on the columns added per iteration, columns removed from the master (`D-29`, phase 2),
  branching (phase 3). Today none exists, so a pool alone is pure overhead.
- Proposed shape: `PooledPricer(pricer, pool, max_columns)`, a `Pricer` wrapping another: prices the pool first and
  returns its columns if any improve; else calls the wrapped pricer, returns its best `max_columns`, stores the rest.
  Only the wrapped pricer's exact stage proves anything. No change to the mirrored loop or contract.
- To decide: in `T-1.15` (commit 7) or later with column management (`T-2.2`); the existing `PricingPool` or a small
  numpy pool (no shared memory); `max_columns` default; a cleanup (`remove_stale`) or none.

## 8. Work split (user, 2026-11-03)

Sonnet implements, Opus reviews; one step per chat, in order. Prompts: the README starter + the task line below.

| step | model | task |
|---|---|---|
| A | Sonnet | commits 1–2 and the mirror test |
| B | Opus | review of A |
| C | Sonnet | commits 3–4 |
| D | Opus | review of C (close: master problem, artificial cost, self-test) |
| E | Sonnet | commits 5–6 |
| F | Opus | review of E (close: the loop against `column_generation.cpp`); full suite once; the PR branch |
| G | Sonnet | quick runs (§6, `D-123`) |
| H | Opus | read the quick runs; plan the full comparison and `Q-41`'s rule |

Rules for every implementing chat:

- Commit on `feature/column-generation` (it has the stack's code, the plans and `build-python-highs`), as the earlier
  Python commits; step F copies them to `feature/column-generation-pure-python` (from
  `feature/column-generation-packaging`).
- Mirror test (commit 1, grown with each commit): every public name of `rcspp.python_column_generation` exists in
  `rcspp.column_generation` with the same constructor and method signatures, except the deviations of §1.
- Record each commit in §5 (`Done: <hash>`, test count, what differs from the plan) and stop at the step's end.
