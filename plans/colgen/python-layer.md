# Pure-Python column generation layer (`T-1.15`): design

Status: **accepted 2026-11-03**, steps A–F done (commits 1–6; step F's review F.1–F.22 and its fixes, `D-131`–`D-135`),
PR branch `feature/column-generation-pure-python` made; step G done 2026-11-06 (`python-layer-runs.md`); next: step H.
Records of every step: `python-layer-records.md`.
On top of the stack (`D-119`), highspy in place (`D-120`), the C++ layer's features the Python way (`D-132`, superseding
`D-121`), VRP not ported (`D-122`), quick runs first (`D-123`), one highspy master solver (`D-124`), pool postponed
(`D-125`, §7), no standalone C++ run (`D-126`). Built on `D-111`, `D-112`, `D-114`, `D-116`, `D-117`, `D-118`.

## 1. The C++ layer's features, the Python way (`D-132`)

Every feature of the C++ layer (and of its Python API, `python/src/rcspp/column_generation/`), none removed, none
added; each written as Python would (`D-133`: ABCs, properties, frozen records, plain enums, no overloads). Not the
binding's signatures: no mirror test (`D-134`). The table lists the pieces; the deviations below are behavior, not form.

| C++-backed piece | pure-Python layer |
|---|---|
| `MasterSolver` (all methods, `add_rows` and `farkas` with their defaults), `Capabilities` | same |
| `MasterProblem` (`add_rows`, `add_pricer`, `add_columns`, columns, artificial ids…), `MasterColumn`, `ColumnTag`, `ColumnType`, `ConvexityRow`, `RowBatch`, `RowRange`, `RowSense`, `ColumnBatch` | same (`ColumnBatch`: numpy CSC arrays, `D-16`) |
| `Pricer` (`price`, `update`, `cost_scale`), `PricingRequest`, `PricingResult`, `PricingStatus`, `PricingChanges` | same |
| `PricingStage`, `GraphPricer`, the C++ `PricingStrategy` and `StageRequest` | stages run `ResourceGraph.solve` or the user's search (F.10); stages and last stage as properties (F.11) |
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
| the time turning `Solution` objects into a `ColumnBatch`: no attribute | it happens in `MasterProblem.add_columns`, not in the pricer; step G measures it with D.12 (`D-131`) |
| `ColumnGenerationParams.max_iterations` is `math.inf` by default | the C++ value is `SIZE_MAX` (`D-131`) |
| no GIL release or Ctrl-C machinery in the loop | plain Python: `KeyboardInterrupt` comes from `solve` itself, or from the master solver: `GurobipyMasterSolver` raises it for Gurobi's `INTERRUPTED` (D.20, `F-46`) |
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
44, fleet 2 / 1, four customers 40); none reads the C++-backed package (`D-130`, `D-134`).

| # | commit | contents |
|---|---|---|
| 1 | `feat(python): pure-Python column generation types and master solver contract` | types, enums, params, `ColumnBatch`, `RowBatch`, `MasterSolver`, `Pricer` |
| 2 | `feat(python): pure-Python master solver over highspy` | `HighspyMasterSolver` in place, fix A |
| 3 | `feat(python): pure-Python master solver over gurobipy` | the port; skips without a license (`D-50`) |
| 4 | `feat(python): pure-Python master problem, tolerances and initializations` | `MasterProblem`, fix B, the self-test |
| 5 | `feat(python): pure-Python graph pricer` | `PricingStage`, `GraphPricer` |
| 6 | `feat(python): pure-Python column generation loop` | `ColumnGeneration`, hooks, statistics; registry; end-to-end tests |

Coverage: the 90 % Python gate.

Step records (A–E, the reviews B, D, F and their fixes): `python-layer-records.md`.

## 6. Comparison with the C++ layer (`D-123`)

- **First, quick runs** as before the presentation: `drivers.py` gets a layer option for the pure-Python package;
  a few small instances (C101_25, R201_25, RC201_12), exact pricing, single-customer start, 1 repeat. Only to check
  that LP values match and nothing is far off.
- **Then** (to plan after the quick runs): the full matrix and the rule that decides `Q-41`.

## 7. The pool (`Q-47` → `D-125`: postponed to `T-2.2`; the analysis, kept for then)

- Today: `PricingPool` (`python/src/rcspp/pricing_pool.py`) = the C++ `SolutionPool` plus a numpy copy in shared
  memory (for worker processes); `price(duals, threshold)` returns pool ids and reduced costs, best first; activity
  (`age`, `use_count`), `remove_stale`, filters on rows and arcs (for branching). No loop uses it (`F-04`); the C++
  layer has none, so this layer has none either (`D-132`).
- A pool priced first only finds columns **not in the master** (an optimal LP leaves its own at reduced cost ≥ 0).
  They come from: a cap on the columns added per iteration, columns removed from the master (`D-29`, phase 2),
  branching (phase 3). Today none exists, so a pool alone is pure overhead.
- Proposed shape: `PooledPricer(pricer, pool, max_columns)`, a `Pricer` wrapping another: prices the pool first and
  returns its columns if any improve; else calls the wrapped pricer, returns its best `max_columns`, stores the rest.
  Only the wrapped pricer's exact stage proves anything. No change to the loop or the contract.
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
| F | Opus | review of E (close: the loop against `column_generation.cpp`); full suite once; the PR branch (done: F.1–F.22, fixes below, `D-135`) |
| G | Sonnet | quick runs (§6, `D-123`) (done 2026-11-06: `python-layer-runs.md`) |
| H | Opus | read the quick runs; plan the full comparison and `Q-41`'s rule |

Rules for every implementing chat:

- Commit on `feature/column-generation` (it has the stack's code, the plans and `build-python-highs`), as the earlier
  Python commits; step F copies them to `feature/column-generation-pure-python` (from
  `feature/column-generation-packaging`).
- Tests import only `rcspp.python_column_generation` and their own fakes (`python_column_generation_fakes.py`), and
  core rcspp (`D-130`); one small test checks the package imports nothing of `rcspp.column_generation` (`D-134`).
- Python idioms, not the binding's (`D-133`; step B, B.5–B.9): ABCs for required methods, properties for state,
  frozen option records, plain enums, no overloads, no GIL, no C++ type conversions highspy or gurobipy already make,
  no container classes where a `dict` does. Annotations run on Python 3.11 (`Generator[T, None, None]`).
- mypy `--strict` and pyright strict (`[tool.pyright]` in `pyproject.toml`) clean on the package and its tests.
- Record each commit in `python-layer-records.md` (`Done: <hash>`, test count, what differs from the plan) and stop
  at the step's end.

### Step F work (user, 2026-11-03: one chat or several, Opus implements; fixes folded into commits 1–6)

Findings in `python-layer-records.md` (F.n). Each batch: a failing test first where a behavior changes; folded into
the commit that introduced the code (`fixup!` + autosquash, nothing pushed); each commit passes its own tests.

| batch | findings | contents |
|---|---|---|
| 1 | F.1–F.7, F.9 | Python-way API: ABCs, frozen records, params dataclass, `add_rows` split, properties, plain enums, required fields, `column_generation_loop.py` |
| 2 | F.10–F.11 | custom stage search and `StageRequest`; `GraphPricer.stages` and the last stage's index |
| 3 | F.12–F.15 | Gurobi re-solve interrupt, deterministic Ctrl-C tests, `TypeError` for a wrong status, `_copied` |
| 4 | F.16–F.19 | `[tool.pyright]`, ignores, `graph.py` annotations, test fixes |
| 5 | F.8, F.20 | mirror test deleted (import check kept); the C++ cases mapped to Python tests, missing ones written |
| 6 | — | review pass of 1–5 (fresh agent); full suite once; `feature/column-generation-pure-python` from `feature/column-generation-packaging`, the layer's commits copied, built, tested, no `plans/colgen` mention |
