# Pure-Python column generation layer (`T-1.15`): step records

Records of the steps of `python-layer.md` §5 and §8, oldest first: what each commit holds, the reviews' findings
(B., D., F.) and their fixes. Hashes are those at the time of each record; folding later fixes changes them.

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
| D.20 | ✔ done in step E: Gurobi turns Ctrl-C into status `INTERRUPTED` (`Other`), no `KeyboardInterrupt`: the loop would have ended with `SolverError` and the interruption been lost (measured, `F-46`) | `GurobipyMasterSolver` raises `KeyboardInterrupt` for `INTERRUPTED` (relaxation and integer solves); HiGHS needs nothing (`D-131`) |
| D.21 | ✘ default instances (`RelaxationParams()`, `PricingRequest()`, `ConvexityRow()`) are shared between calls; pybind11 builds its defaults once too | none: the mirror needs them |

Fixes (2026-11-03, Opus): a failing test first for D.1, D.2, D.4–D.7; folded into commits 1–4 (`fixup!` and edit stops,
nothing pushed), the messages reworded; the user's lint fixes folded in too. Each commit passes its own tests
(pure-Python files: 53, 101, 128, 223; with the C++-backed column generation tests 169, 217, 244, 339), mypy `--strict`
clean on 3.11 and 3.14, pre-commit clean. D.3 kept as measured: only a batch with two different mistakes differs.
Second pass fixes (2026-11-03, Opus): D.15–D.19, a test first for D.17 and D.18, folded into commits 1–4 the same
way. Each commit passes its own tests (pure-Python files: 53, 102, 129, 225; the whole Python suite 474, 523, 550, 646,
1 skipped); 100 % lines on the package.

Step E record (2026-11-03, Sonnet; whole Python suite 784 passed, 1 skipped, run once at the end; 363 in the nine
pure-Python test files; 100 % lines on the package; mypy `--strict` clean on 3.11 and 3.14; Gurobi had a license):

- Commit 5 `a409cf56`: `graph_pricer.py` (`PricingStage`, `GraphPricer`); 24 tests in
  `test_python_column_generation_graph_pricer.py`, 255 in the pure-Python files then. Stages run `ResourceGraph.solve`
  from the first until one finds a path, proves, or the time is out; a search's limit is the smaller of the stage's
  and the time left.
- Commit 6 `d524baaa`: `statistics.py`, `convergence.py`, `params.py`, `loop.py`, `registry.py`; 108 more tests (loop 53,
  registry 7, end to end 14, the Gurobi master's Ctrl-C 1, the rest the mirror's new cases). End to end: the toy LP
  37.5 and duals (11.5, 12.5, 13.5), MIP 44, fleet 2 / 1, four customers 40 over highspy and gurobipy, with a tabu stage
  first too, and a Ctrl-C test per solver. The mirror test's `PENDING` is gone; a module name (`registry`) compares its
  functions' signatures, a Python function by `inspect`.
- Differences from the plan:
  - `GraphPricer` copies the stage params by attribute (their classes cannot be copied) and prices arcs added after it
    was built; `cost_scale` reads the arcs once (`get_arc` per arc). No conversion-time attribute: the conversion is in
    `add_columns`.
  - `loop.py`, not `column_generation.py`: the mirror test refuses any import of that module name.
  - `ColumnGenerationParams` is a dataclass with its own `__init__` (the C++-backed signature); `max_iterations`
    defaults to `math.inf`. `ColumnGeneration` keeps a copy (`relaxation`, `statistics`), the defaults filled in.
    Attributes are not type-checked, as the other options.
  - The loop turns the duals into a `float64` array and refuses a wrong count (`ValueError`, the C++-backed message);
    a master or params of the wrong type is a `TypeError`. A converged run with no artificial column does not call
    `primal`.
  - Not ported: the C++-backed end-to-end test that compares the loop of the Python example with the C++ loop.
- D.20 (`F-46`): measured on Windows with a real console Ctrl-C. Gurobi (assignment LP 700 × 700, Ctrl-C at 1 s):
  `optimize()` returns status 11 `INTERRUPTED` at 1.0 s, no `KeyboardInterrupt`, the signal is gone. HiGHS (1500 ×
  1500, Ctrl-C at 0.4 s): `run()` finishes (6 s, optimal), then Python raises `KeyboardInterrupt`. `GraphPricer`:
  `KeyboardInterrupt` 0.02 s after the signal. Fix: Gurobi's `INTERRUPTED` raises `KeyboardInterrupt`; `run()` restores
  the model in a `finally`.

Accepted as they are (step C's list): `add_rows` overloads (the mirror needs both call forms), no batch for no
artificial column, `NotImplementedError`, `columns()` shallow copy (frozen records; the `Solution` shared, as `B.14`),
one `RuntimeWarning` per master. `B.16`'s widths (`uint32`/`uint64`) are the other C++ concept left, already ⏸.

Step F review (2026-11-03, Opus; decided by the user, `D-131`–`D-134`: ✔ to fix, ✘ no fix, ⏸ later). Paths under
`python/src/rcspp/python_column_generation/`. The loop matches `column_generation.cpp` statement for statement (proof
rule, tolerance floor, both time checks, failed-solve statuses, hooks, clock started before initialization); the graph
pricer matches `graph_pricer.hpp`. No `D-55` breach in code, tests or the six commit messages. Pyright 1.1.414 strict
(the user's Pylance setting): 35 errors, 8 warnings, none from mypy.

| # | finding | fix |
|---|---|---|
| F.1 | ✔ C++ "pure virtual" kept: `Pricer`, `MasterSolver`, `Tolerances`, `MasterInitialization` raise `NotImplementedError("X.y must be overridden")` | `abc.ABC` + `@abstractmethod` on the required methods; `MasterSolver.add_rows` keeps `NotImplementedError` (a missing capability) |
| F.2 | ✔ default instances shared between calls (D.21) | frozen option records (`PricingRequest`, `RelaxationParams`, `IntegerParams`, `ConvexityRow`, `StatisticsParams`, `ToleranceContext`), changed with `replace`; `ColumnGeneration` copies nothing |
| F.3 | ✔ `ColumnGenerationParams`: `dataclass(init=False)`, `None` meaning "default" (the binding's signature) | keyword-only dataclass with real defaults (`field(default_factory=…)`); a name (`str`) for tolerances or initialization converted when built; no `_tolerances()` / `_initialization()` fallbacks in the loop |
| F.4 | ✔ `MasterProblem.add_rows` overloads dispatched on `*args` (C++ overloading, D.4) | two methods: `add_rows(count, sense, right_hand_side)` and one for ranges (name: full words, `D-12`) |
| F.5 | ✔ getters as C++ accessors | properties for what an object holds or derives cheaply (`master.solver`, `columns`, `artificial_column_ids`, `artificial_cost`, `initialized`, `number_of_rows`, `number_of_columns`, `number_of_pricers`, `loop.params`, `loop.tolerance_context`, `report.passed`); methods for the `MasterSolver` and `Pricer` contracts and anything with arguments; no `__len__` on `MasterProblem` (`D-133`) |
| F.6 | ✔ `NumberedEnum`'s `__int__` / `__index__` only for the binding (one test checks them) | plain `Enum`; CamelCase members kept, as rcspp's (`AlgorithmStatus.Complete`) |
| F.7 | ✔ records whose fields all default, as C++ structs (`MasterColumn`, `ColumnGenerationResult`, `RowRange`) | required fields where the code always sets them; `RowBatch` / `ColumnBatch` stay classes with properties (`D-129`) |
| F.8 | ✔ mirror test: signatures, defaults and members equal to the binding's; misses a field inside a dataclass default | deleted; one small test keeps "imports nothing of `rcspp.column_generation`"; parity by one AI check and by the C++ unit tests' behaviors replicated in Python (F.20) |
| F.9 | ✔ `loop.py` named after the mirror test's refusal | `column_generation_loop.py` |
| F.10 | ✔ missing C++ feature: a `PricingStage` holding any search (`PricingStage(search, exact)`, `StageRequest`) | a stage takes a callable `search(graph, request) -> SolveResult` or an algorithm; `StageRequest` record |
| F.11 | ✔ missing C++ feature: `PricingStrategy` (stages, cursor), `GraphPricer.strategy()` | `GraphPricer.stages` and the index of the last stage run, as properties |
| F.12 | ✔ Ctrl-C lost in the INF_OR_UNBD re-solve: `gurobipy_master_solver.py:436-442` maps `INTERRUPTED` to `Other` → run `SolverError` | check `INTERRUPTED` after the second solve too; a test |
| F.13 | ✔ Ctrl-C tests timing-based: Gurobi primal 0.99 s vs terminate at 0.3 s, HiGHS 0.40 s vs interrupt at 0.3 s (measured here) | Gurobi: terminate every 50 ms from t = 0; HiGHS: `interrupt_main` once `cbSimplexInterrupt` fired |
| F.14 | ✔ a custom master solver's non-`SolverStatus` → bare `KeyError` (`loop.py:203`) | `TypeError` naming the method (B.12's rule) |
| F.15 | ✔ `_copied` (`graph_pricer.py:55`) would fail on a read-only property of `AlgorithmParams` | skip read-only properties; a comment |
| F.16 | ✔ pyright: no config; the Pylance interpreter path is `venv/`, the repo's `.venv/` (user setting) | `[tool.pyright]` in `pyproject.toml`: strict, `extraPaths` `python/src` and `tests/python`, `.venv`, `reportMissingModuleSource` off |
| F.17 | ✔ pyright: 5 `reportUnnecessaryIsInstance` on the deliberate type checks (`graph_pricer.py:96,108`, `loop.py:111,113`, `types.py:125`) | `# pyright: ignore[reportUnnecessaryIsInstance]` per line |
| F.18 | ✔ pyright: `graph.py:602,694` unannotated, so `solve(Algorithm…)` is an error (`graph_pricer.py:152,177-178`) | annotate `algorithm: str \| Algorithm` and `duals` |
| F.19 | ✔ pyright: 27 test errors (wrong-type tests without ignore, fakes' `_solve` / `_respond`, `vars(module)` on `Any`) | ignores on the wrong-type lines, public fake methods; the mirror one goes with F.8 |
| F.20 | ✔ tests: no check that the layer behaves as C++ beyond end to end | list what the 77 C++ cases (12 files `tests/cpp/test_column_generation_*.hpp`) check, map each to a Python test, write the missing ones (what they test, not how) |
| F.21 | ✘ `import column_generation_loop` in the C++-backed `test_column_generation_end_to_end.py` loads `examples/python/column_generation_loop.py` by a `sys.path` hack | none for now (user); may go later |
| F.22 | ✔ `D-131`'s three deviations | accepted (`D-131`) |

Step F record (2026-11-03, Opus; all of F.1–F.20 fixed, `D-135` for the choices made while fixing). Batches 1–5 and the
review's fixes were made on top of the branch, then each file was put in the commit that introduced it (a rebuilt
history: each file at its final content from its last commit on, a three-way merge before, conflicts by hand); every
rebuilt commit passes its own tests, mypy `--strict`, pyright strict and pre-commit. Full suite once: 676 passed,
1 skipped (784 − the 130 mirror cases + 22 new), 100 % of the package's lines. WIP history kept on the local branch
`backup/step-f-wip`.

| # | commit (`feature/column-generation`) | on `feature/column-generation-pure-python` | tests in the layer's files at that commit |
|---|---|---|---|
| 1 | `df83c093` | `45a3c19b` | 12 |
| 2 | `be1bbdf0` | `e2468e7b` | 59 |
| 3 | `8c8e9285` | `10c935dc` | 83 |
| 4 | `c8561444` | `a2807bb4` | 138 |
| 5 | `620fdde9` | `3333a077` | 171 |
| 6 | `79bc41b7` | `13665eb3` | 255 |

What differs from the batch plan:

- The mirror test went in batch 1 (it checked the binding's signatures this batch changes on purpose); the import check
  moved to `test_python_column_generation.py` (commit 1).
- F.15 (`_copied`) was fixed with F.10 in batch 2; its test came in batch 3.
- F.12: the interrupt check moved after the re-solve in commit 6 (it came with Ctrl-C); commits 3–5 keep `_optimize`
  without it.
- `[tool.pyright]` in commit 1, `graph.py`'s annotations in commit 5; no `pythonVersion` pin (numpy's stubs then report
  "partially unknown"; mypy checks 3.11).
- `python_column_generation_fakes.py` has its pricer fakes from commit 4 (the master tests now use a scripted pricer,
  `Pricer` being abstract).
- F.20: 76 C++ cases (71 `TEST` + 5 typed; two of the 12 files hold helpers): 59 covered, 1 deviation (the strategy's
  cursor), 2 missing (self-test: an ignored column, an integer solve that solves the relaxation) and 14 partial; all
  written (20 tests or checks).
- Review pass (fresh agent): 3 bugs the diff opened, fixed with a failing test first: `add_rows` with a sense of
  another type made equality rows (now `TypeError`); `tolerances=None` failed inside `run` (now `TypeError` when the
  loop is built); `PricingStage(search=42)` or with another algorithm was accepted (now `TypeError` / `ValueError`).
  Also: the HiGHS Ctrl-C test waits 50 ms after the callback (a GIL switch inside it would abort the solve), and the
  registry docstring says where a name is built. Left: `solve_restricted_master_integer` returns the master solver's
  status unchecked (as C++; nothing reads it).

Python-way review (2026-11-06, Opus; the user's question: any C++ notion left where Python has its own? `D-136`,
all ✔). The loop still matches `column_generation.cpp` (proof rule, tolerance floor, time checks, hooks).

| # | finding | fix |
|---|---|---|
| P.1 | `ArtificialColumns(cost=0.0)`: 0 means automatic (a placeholder value, as D.8) | `cost=None`; ≤ 0, `inf`, NaN refused |
| P.2 | `ConvexityRow.max_paths = inf` means "no row"; "not both" checked in `add_pricer` only | `float \| None`, checked in `__post_init__` |
| P.3 | `MasterProblem.pricer(i)`, `convexity_row(i)`, `number_of_pricers`: getters by index | `pricers`: tuple of `PricerEntry` |
| P.4 | `default_*` constants named as C++ `constexpr`; rcspp's Python uses capitals | `DEFAULT_*`, `AUTOMATIC_COST_FACTOR` |
| P.5 | tolerances, initializations: private numbers, no `repr`, no `==` (grids print `<object at 0x…>`) | hand-written `repr` (`eval` builds it back), `==`, `hash` |
| P.6 | unknown column id (`variable`, `column`, `primal`): `IndexError`, pybind11's `std::out_of_range` | `KeyError` |
| P.7 | `_solution_list_factory`, `_string_list_factory` only for pyright | `field(default_factory=list[Solution])` |
| P.8 | `artificial_cost` 0, `ColumnGenerationResult.objective` `inf` when none | `None` |
| P.9 | enum values 0…N only mattered for the binding's `__int__` | `auto()` |
| P.10 | `MasterSolverSelfTestReport` frozen but its list appended to | `failures` a tuple, built first |
| P.11 | `RowBatch` / `ColumnBatch`: the arrays (shared with the caller's) and `names` writable through the getters; no `repr` | read-only views, `names` a tuple, `repr` (`ColumnBatch(3 columns, 7 entries)`); the dataclass form kept out (no simpler); widths B.16 and `scipy.sparse` (`D-16`) wait for step G |

Record: a fixup per commit, autosquashed (conflicts in `__init__.py` only); the final tree equals the tested one.
Each commit passes its own tests and mypy `--strict` (13, 60, 84, 145, 178, 262 tests); bandit wants `# nosec B307`
on the two `eval` of a `repr`. Whole Python suite 683 passed, 1 skipped; 100 % of the package's lines; pyright strict
clean. Not pushed: both branches need a force push. Backups: `backup/python-way-fixes`,
`backup/pure-python-before-fixes`.

| # | `feature/column-generation` | `feature/column-generation-pure-python` |
|---|---|---|
| 1–6 | `25fa89b9`, `2164eae7`, `84f8c104`, `b05216d8`, `de07cb88`, `50d0cfe6` | `40579d5e`, `44da43a9`, `3de00ca8`, `b3d16425`, `c50d13ec`, `a69dfccf` |
