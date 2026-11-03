# Pure-Python column generation layer (`T-1.15`): design

Status: **accepted 2026-11-03**, code started: step A done (§5 commits 1–2, record under §5), step B done (review
B.1–B.17 under §5), step C done (commits 3–4, record under §5), step D done (review D.1–D.21 under §5, fixed and folded
into commits 1–4, `D-130`); next is E. On top of the stack (`D-119`), highspy in place (`D-120`), mirror the C++ layer (`D-121`),
VRP not ported (`D-122`), quick runs first (`D-123`), one highspy master solver (`D-124`), pool postponed (`D-125`, §7),
no standalone C++ run (`D-126`). Built on `D-111`, `D-112`, `D-114`, `D-116`, `D-117`, `D-118`.

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
| an artificial column's `pricer` is `None`; no `MasterProblem.no_pricer` | `D-130`: the C++ value is `SIZE_MAX` |

## 2. Where it lives (`D-119`)

- On top of the stack: branch `feature/column-generation-pure-python` from `feature/column-generation-packaging`
  (the top of the stack, #80: same build options as the measurements' `build-python-highs`).
- Package `python/src/rcspp/python_column_generation/`, a sibling of `column_generation/`: no file and no
  import shared, so no collision; it imports nothing of `rcspp.column_generation` (`import rcspp` still loads that).
  Same class names in both.
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

Each commit passes its own tests (pytest `-k` while iterating, the full suite once at the end). Tests: the cases of
the C++-backed package's tests, on this package only, with the same values (toy LP 37.5, duals (11.5, 12.5, 13.5), MIP
44, fleet 2 / 1, four customers 40); only the mirror test reads the C++-backed package (`D-130`).

| # | commit | contents |
|---|---|---|
| 1 | `feat(python): pure-Python column generation types and master solver contract` | types, enums, params, `ColumnBatch`, `RowBatch`, `MasterSolver`, `Pricer` |
| 2 | `feat(python): pure-Python master solver over highspy` | `HighspyMasterSolver` in place, fix A |
| 3 | `feat(python): pure-Python master solver over gurobipy` | the port; skips without a license (`D-50`) |
| 4 | `feat(python): pure-Python master problem, tolerances and initializations` | `MasterProblem`, fix B, the self-test |
| 5 | `feat(python): pure-Python graph pricer` | `PricingStage`, `GraphPricer` |
| 6 | `feat(python): pure-Python column generation loop` | `ColumnGeneration`, hooks, statistics; registry; end-to-end tests |

Coverage: the 90 % Python gate.

Step A record (2026-11-03, on `feature/column-generation`; 112 tests in the three new files after step B's fixes, run
alone, none of the full suite; mypy `--strict` clean on the package). Hashes after step B's fixes were folded in:

- Commit 1 `06ff1f5b`: `types.py`, `pricer.py`, `master_solver.py`, `__init__.py`; 64 tests (mirror + shared behavior,
  the shared ones run on both layers). `ColumnBatch` and `RowBatch` are classes with read-only numpy properties and
  the same checks and messages; the options are keyword-only dataclasses; required methods raise
  `NotImplementedError` (a `RuntimeError`): `MasterSolver.duals must be overridden`.
- Commit 2 `d19fd24b`: `master_solver_support.py` (the row and column checks of `master_solver_support.hpp`, for step
  C's gurobipy port too; the id → column map is a `dict`), `master_solvers/highspy_master_solver.py`; 48 tests,
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
  - highspy 1.15.1 has no `zeroAllClocks`: a solve sets `time_limit` to `getRunTime()` + its timeout. The MIP solver
    overruns it (`F-43`): the integer solve also has a deadline in a `cbMipInterrupt` handler (`D-128`, `D-129`).
  - The thread-count warning is `warnings.warn(RuntimeWarning)`, not the C++ logger. `ColumnBatch.starts` and
    `row_indices` are `uint32` as in C++.
  - Fix A is the cold retry of `D-118` (`clearSolver()` then `run()`; the C++ layer also restores the basis). The
    `F-41` LP was not added to the tests: the R201_25 master is 2032 columns, too large for a test file, so the retry
    is tested with patched statuses (error, unknown, not set, a refused run, a second failure, no retry for the
    integer solve).
  - Pre-commit's `docformatter` rewraps docstrings: run `pre-commit run --files …` before committing (the venv's
    `Scripts` folder must be on `PATH`).

Step B review (2026-11-03, Opus; user agreed to fix before step C). Checked: tests pass, mypy and pre-commit clean,
no `D-55` breach. Paths under `python/src/rcspp/python_column_generation/`. ✔ to fix before step C; ⏸ decided after
step G; ✘ no fix (`D-127`):

| # | finding | fix |
|---|---|---|
| B.1 | ✔ integer solve overruns its time limit 4–12× once LP solves have run (`F-43`) | done: a deadline in a `cbMipInterrupt` handler (`D-128`; `D-129`: HiGHS's limit kept as a backstop) |
| B.2 | ✔ package docstring: "never loads `_core.column_generation`" is false (`import rcspp` loads it); §2 fixed | "imports nothing of `rcspp.column_generation`" |
| B.3 | ✔ thread warning `stacklevel=4` points above the caller (checked) | 3 |
| B.4 | ✔ fix A comment says a cold solve cures the cycling case (`F-21`: the known cure keeps the basis); a refused run is retried too, within what is left of the limit: undocumented | comment and docstring |
| B.5 | ✔ "pure virtual" message: a pybind11 notion; the shared test pins it (`test_python_column_generation.py:223-227`, `:270`) | `NotImplementedError("MasterSolver.duals must be overridden")`; test: `RuntimeError` + method name |
| B.6 | ✔ `set_option` ports the C++ `std::visit` (bool/int split, int → double, `_INT32_MAX`): highspy does all of it (`F-45`) | one `setOptionValue` + status check |
| B.7 | ✔ `MasterSolver.add_rows`: `::` in the message, `RuntimeError` | `NotImplementedError`, `MasterSolver.add_rows` |
| B.8 | ✔ `HighspyMasterSolver` docstring: "the loop calls with the GIL held": binding notion | drop |
| B.9 | ✔ `SolverColumns`: C++ map + vector (generic, `at`, `all`); bounds rule written twice (scalar + numpy) | `dict` + `check_columns(batch, rows, known_ids)`; one vectorized rule. Before step C reuses it |
| B.10 | ✘ `RowBatch`, `ColumnBatch`: `__slots__` + one property per field | dropped (`D-129`): a dataclass field has one type, so lists in or arrays out would not type-check; the properties are plain Python |
| B.11 | ✔ mirror bug: `int(SolverStatus.Optimal)` works on the C++-backed enums, `TypeError` here (all four enums) | `__int__`/`__index__`; not `IntEnum` (`== 0` is `False` on pybind11 enums) |
| B.12 | ✔ mirror bug: `RowBatch(..., names="ab")` gives `['a', 'b']`; pybind11 refuses a `str` | refuse a `str` |
| B.13 | ✔ mirror test: comment on `ONLY_IN_CPP_LAYER` says it "counts the solves it ran again", the set is empty | fix the comment |
| B.14 | ✘ `RowBatch.names`, `PricingResult.columns` return the live list; the C++-backed ones a copy | none for now (`D-127`) |
| B.15 | ⏸ `duals()` copies the whole solution (`getSolution()`), `row_bounds`, `largest_user_cost` the whole LP (`getLp()`), each call: biases the speed comparison | decided after step G (`D-127`) |
| B.16 | ⏸ `ColumnBatch` `uint32`/`uint64` (C++ widths): `add_columns` converts to `int32` per batch | decided after step G (`D-127`) |
| B.17 | ✘ C++ layer (frozen, `D-116`): `HighsMasterSolver::restore_upper_bounds` reads past `upper_bounds_` (`F-44`) | recorded only, no fix (`D-127`) |

Order of the fixes: a failing test first for B.1, B.3, B.11, B.12; B.6, B.9, B.10 are refactors the existing tests
cover; B.5, B.7 a Python-layer test next to the shared one; B.2, B.4, B.8, B.13 text only.

Step B fixes record (2026-11-03, Opus): B.1–B.9, B.11–B.13 done, folded into commits 1–2 (`fixup!` + autosquash, the
branch was not pushed); each commit passes its own tests (64, then 112). New tests: the enums' `int()`, a string of
names refused, the Python layer's `NotImplementedError`, the warning's line, the integer solve's limit after 10 LP solves
(0.2 s asked, under 1 s; 2.4 s before; about 1.4 s per run). B.10 dropped (`D-129`). The user's lint fixes folded in
too (`Generator` return types of the context managers, typed lists in the mirror test); `Generator` takes its three
arguments: one argument needs Python 3.13, the package supports 3.11.

Step C record (2026-11-03, Sonnet; 285 tests in the five pure-Python test files, run alone; mypy `--strict` clean; 100 %
lines on the new modules from their own tests; Gurobi had a license here, so nothing skipped):

- Commit 3 `125f78ab`: `master_solvers/gurobipy_master_solver.py`; 50 tests on both layers (the C++-backed file's
  cases, columns as batches). Left to gurobipy (`F-45`'s rule): infinite bounds and sides, a bool for a number
  parameter, lists. Kept: a text value read as the parameter's type (gurobipy refuses `"1"`).
- Commit 4 `21b30ecb`: `tolerances.py`, `master_problem.py`, `master_initialization.py`, `master_solver_self_test.py`;
  `automatic_cost_factor` 1e4 (fix B). 74 tests on both layers (`test_python_column_generation_master.py`): the C++
  cases, a random column batch against a dict reference, a master problem and the real self-test over both solvers.
- Differences from the plan:
  - `add_columns` builds one CSC batch with numpy from `Column.to_arrays()` (one C++ call per column): rows sorted and
    a repeated row summed when needed, one `RuntimeWarning` per master. `Column.to_arrays` got a stub in `graph.pyi`.
  - Python's way: no capability → `NotImplementedError`, as `Tolerances` and `MasterInitialization` when not
    overridden; `columns()` is a shallow copy; `RowRange`, `MasterColumn`, the report are frozen dataclasses;
    `no_pricer` keeps the C++ value (2^64 − 1, not `None`).
  - `add_artificial_columns` sends nothing for no column (C++ sends an empty batch). The self-test reports an answer of
    the wrong size as a failed check (C++ raises while copying it).
  - `add_rows` is two `typing.overload` stubs with docstrings (flake8's E704 refuses `...` bodies, which black forces)
    over one dispatching method. The mirror test reads them with `typing.get_overloads`, compares module functions,
    skips constructors pybind11 cannot build, takes a frozen field as read-only, lists `DIFFERENT_CONSTANTS`; object
    defaults compare by class only. Registry names stay `PENDING` for commit 6.
  - The fakes: `ScriptedMasterSolverBehavior`, `SelfTestMasterSolverBehavior` in `column_generation_fakes.py`, one base
    per layer (`python_column_generation_fakes.py`).
- For step D: the vectorized batch (`_sorted_without_repeats`, `_with_entry_at_end`) and its cost against the C++ loop
  (`B.16`: `int64` rows converted to `uint32` per batch; one `to_arrays` call per column).

Step D review (2026-11-03, Opus; decided by the user, `D-130`: ✔ fixed, ✘ no fix, ⏸ later). D.15–D.21 come from a
second pass over commits 1–4, asked by the user. Checked before the
fixes: each commit passed its own tests, mypy `--strict` clean (3.14), no `D-55` breach, `PENDING` shrunk to the commit
5–6 names. Measured equal on both layers: row order, a repeated row summed, convexity entry last, nothing recorded when
the solver refuses, empty columns with a convexity row, `no_pricer`, the read-only records. No "pure virtual", `::`, GIL
or `std::` text left. Paths under `python_column_generation/`:

| # | finding (measured) | fix |
|---|---|---|
| D.1 | ✔ self-test docstring: "an answer of the wrong size ends the checks"; false: 2 duals → the next steps run (4 failures), 2 row bounds → the LPs run | `return` after a failed size check |
| D.2 | ✔ `add_artificial_columns([0.7, 1.2], …)` adds rows 0 and 1 (`np.asarray(dtype=int64)` truncates); C++-backed: `TypeError` | refuse a non-integer array (`TypeError`) |
| D.3 | ✘ `add_columns` checks all rows, then the convexity: column 0 in the convexity row + column 1 with a bad row → `IndexError` (C++: `ValueError`, column order) | raise the first offender's error |
| D.4 | ✔ `add_rows` dispatch on `int`: `add_rows(2.0, …)` → "RowBatch.lower: expected a one-dimensional array", `add_rows(True, …)` → numpy `TypeError` (C++ 1 row); size errors say "RowBatch:", not "MasterProblem.add_rows" | `TypeError` naming both forms; the master's message |
| D.5 | ✔ `add_columns` needs `len`: a generator → `TypeError` (C++-backed takes any iterable) | `list(columns)` once |
| D.6 | ✔ gurobipy `set_option("LogFile", True)` → `AttributeError` from gurobipy (bool conversion dropped), not the documented `ValueError` | catch it, or refuse a bool for a text parameter |
| D.7 | ✔ C++ concept: `_text` copies `std::format`, wrongly for whole numbers ≥ 1e16 (`1e+20` → `100000000000000000000`) and −0 | Python's `repr`; shared tests compare numbers, not C++ text |
| D.8 | ✔ C++ concept: `no_pricer = 2**64 − 1` is `SIZE_MAX`; Python says "none" with `None` | `pricer: int \| None`, a §1 deviation |
| D.9 | ✔ mirror test: a C++ writable field made read-only is not caught (none today); class constants never compared but `DIFFERENT_CONSTANTS`; object defaults by class (known) | compare class constants and writable → writable |
| D.10 | ✔ `ArtificialColumns` docstring names "the C++-backed layer" (stale after the last PR's tidy-up); "a tabu stage makes up for it" overstates `F-41` (R201 big-M + tabu 1295 s, single-customer + tabu 206 s) | "a tabu stage first is the suggested setup"; the 1e6 story in the commit only |
| D.11 | ✔ mypy `--strict --python-version 3.11`: 3 errors (`.max()`/`.min()` on `Any` arrays, `master_problem.py:267`, `master_solver_support.py:45`); typing only | `np.max(a)`, not `a.max()` (numpy's stubs for 3.11) |
| D.12 | ⏸ cost: `add_columns` 95 µs vs 13 (1 × 10 rows), 1.1 ms vs 0.12 (200 × 25), 15 ms vs 3.0 (2000 × 50); `to_arrays` is 60–85 % of it (≈ 6 µs per column) | none now: step G, `D-112` |
| D.13 | ✘ a repeated row's sum can differ in the last bit (0.1 + 0.2 + 0.3: 0.6 here, 0.6000000000000001 in C++) | none |
| D.14 | ✔ the tests ran on both layers (a `cpp` fixture; commit 4 split the C++ layer's fakes into mixins): they needed the C++ layer built and tested it again | this layer only, fakes of its own; the shared fakes as before commit 4 |
| D.15 | ✔ `HighspyMasterSolver` docstring (commit 2) still says "a tabu stage … makes up for it" (D.10 fixed only `ArtificialColumns`) | D.10's wording |
| D.16 | ✔ after `solve_integer(Binary)` + `restore_user_model` a generated column stays binary (gurobipy `B`, both layers) or integer (highspy); the gurobipy docstrings say "continuous" | the docstrings (user): the columns keep the integer solve's type |
| D.17 | ✔ 2 lines not covered: the self-test's wrong-size stop after x4 (`master_solver_self_test.py:180`), HiGHS refusing `add_rows` (`highspy_master_solver.py:233`) | one test each |
| D.18 | ✔ `RowBatch(..., names=[1, 2])` keeps the ints; C++-backed `TypeError` (`B.12`'s family) | refuse names that are not texts |
| D.19 | ✔ `Pricer.cost_scale` docstring cut by docformatter: a summary ending in ":", the rest after a blank line | one sentence |
| D.20 | ⏸ for step E, not measured: Gurobi turns Ctrl-C into status `INTERRUPTED` (`Other`), not `KeyboardInterrupt`, which §1's last deviation counts on | check in step E |
| D.21 | ✘ default instances (`RelaxationParams()`, `PricingRequest()`, `ConvexityRow()`) are shared between calls; pybind11 builds its defaults once too | none: the mirror needs them |

Fixes (2026-11-03, Opus): a failing test first for D.1, D.2, D.4–D.7; folded into commits 1–4 (`fixup!` and edit stops,
nothing pushed), the messages reworded; the user's lint fixes folded in too. Each commit passes its own tests
(pure-Python files: 53, 101, 128, 223; with the C++-backed column generation tests 169, 217, 244, 339), mypy `--strict`
clean on 3.11 and 3.14, pre-commit clean. D.3 kept as measured: only a batch with two different mistakes differs.
Second pass fixes (2026-11-03, Opus): D.15–D.19, a test first for D.17 and D.18, folded into commits 1–4 the same
way. Each commit passes its own tests (pure-Python files: 53, 102, 129, 225; the whole Python suite 474, 523, 550, 646,
1 skipped); 100 % lines on the package.

Accepted as they are (step C's list): `add_rows` overloads (the mirror needs both call forms), no batch for no
artificial column, `NotImplementedError`, `columns()` shallow copy (frozen records; the `Solution` shared, as `B.14`),
one `RuntimeWarning` per master. `B.16`'s widths (`uint32`/`uint64`) are the other C++ concept left, already ⏸.

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
| C | Sonnet | commits 3–4 (done) |
| D | Opus | review of C (close: master problem, artificial cost, self-test) (done: D.1–D.21) |
| E | Sonnet | commits 5–6 |
| F | Opus | review of E (close: the loop against `column_generation.cpp`); full suite once; the PR branch |
| G | Sonnet | quick runs (§6, `D-123`) |
| H | Opus | read the quick runs; plan the full comparison and `Q-41`'s rule |

Rules for every implementing chat:

- Commit on `feature/column-generation` (it has the stack's code, the plans and `build-python-highs`), as the earlier
  Python commits; step F copies them to `feature/column-generation-pure-python` (from
  `feature/column-generation-packaging`).
- Tests import only `rcspp.python_column_generation` and their own fakes (`python_column_generation_fakes.py`);
  only the mirror test reads `rcspp.column_generation` (`D-130`).
- Mirror test (commit 1, grown with each commit): every public name of `rcspp.python_column_generation` exists in
  `rcspp.column_generation` with the same constructor and method signatures, except the deviations of §1.
- Python idioms, not the binding's (step B, B.5–B.9): no "pure virtual" or `::` in messages (`NotImplementedError("X.y
  must be overridden")`), no GIL, no C++ type conversions highspy or gurobipy already make, no container classes where a
  `dict` does. Annotations run on Python 3.11 (`Generator[T, None, None]`, not `Generator[T]`).
- Record each commit in §5 (`Done: <hash>`, test count, what differs from the plan) and stop at the step's end.
