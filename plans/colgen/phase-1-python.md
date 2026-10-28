# Phase 1 Python layer: design (PRs 5 and 6)

Replaces `phase-1-design.md` §3. Status: **accepted 2026-10-28** (`Q-30`–`Q-36` → `D-86`–`D-92`). Built on
`D-36`–`D-39`, `D-47`–`D-49`, `D-53`, `D-54`, `D-60`, `D-68`, `D-71`, `D-75`; facts `F-03`, `F-14`, `F-16`, `F-17`,
`F-27`. The C++ API is the code on `feature/column-generation-highs` (PRs 1–4).

## 1. Why §3 was redone

| §3 said | now |
|---|---|
| `SolverBackend` and its trampoline | `MasterSolver` (`D-67`, `D-69`) |
| user-model masters are Python subclasses called by the C++ loop | by default the user's model is **copied** into a C++ master solver and the run stays in C++ (`D-60` B); a Python subclass stays the extension point (`D-60` A) |
| stages `{algorithm, AlgorithmParams, exact}` | a C++ stage holds a search function (`D-71`); the Python graph builds that function from the algorithm it already picks at run time |
| `_make_pricer(stages, cost_index)` | also `preprocess` (`D-75`) |
| — | Gurobi from a wheel: `D-60` B needs a C++ Gurobi master solver the wheel cannot ship (`F-27`); route A first, B tested later (`D-86`, T-1.11) |
| — | the Python loop of `D-68` |

## 2. What a user writes (sketch)

```python
import highspy
from rcspp.column_generation import (ColumnGeneration, ColumnGenerationParams, ConvexityRow, GraphPricer,
                                     HighspyMasterSolver, MasterProblem, PricingStage)

model = highspy.Highs()                                      # the user's master, built as usual
for _ in customers:
    model.addRow(1, 1, 0, [], [])                            # "visited once": the columns fill it
model.addRow(-highspy.kHighsInf, vehicles, 0, [], [])        # fleet (addRow returns a status, not an index)

solver = HighspyMasterSolver(model, rows=range(model.getNumRow()))   # copies the model into C++ (D-92)
master = MasterProblem(solver)
pricer = GraphPricer(graph, stages=[PricingStage("greedy"), PricingStage("simple")])
master.add_pricer(pricer, ConvexityRow(existing_row=len(customers)))
result = ColumnGeneration(master, ColumnGenerationParams(timeout_s=60)).run()   # no Python inside
solver.write_back()                                          # optional: the generated columns into `model`
```

## 3. Pieces

| piece | where | notes |
|---|---|---|
| bindings of the C++ API | `python/bindings/column_generation.cpp`, submodule `rcspp._core.column_generation` | types, enums, params, results, `MasterProblem`, `ColumnGeneration`, tolerances, initializations, `Pricer`, `MasterSolver`, `master_solver_self_test`; same names as C++ (`D-12`) |
| trampolines | same file | `MasterSolver`, `Pricer`, `Tolerances`, `MasterInitialization` subclassable from Python |
| graph pricers | `bind_rg_methods` in `python/bindings/graph_impl.hpp` gains `_make_pricer(stages, cost_index, preprocess)` (`D-48`) | every bound graph type gets it; Python `GraphPricer` calls it |
| C++ HiGHS master solver | `python/bindings/column_generation_highs.cpp`, built with `USE_HIGHS` | a `Highs` owned by the binding, filled from arrays, wrapped by `HighsMasterSolver` |
| Python API | `python/src/rcspp/column_generation/` | re-exports, `PricingStage`, `GraphPricer`, registry (`D-89`) |
| master solvers over the user's model | `python/src/rcspp/column_generation/master_solvers/` | `HighspyMasterSolver`, `GurobipyMasterSolver` (`D-86`, `D-92`) |
| Python loop (`D-68`) | `examples/python/` (`D-90`) | written with the bound pieces |

### Bindings

- **Arrays (`D-16`, `D-91`):** duals, column values and results as numpy `float64`. A `ColumnBatch` reaches a Python
  master solver as numpy arrays (`starts`, `row_indices`, `values`: the CSC layout of `D-53`).
- **Ownership (`D-38`):** `keep_alive` from the master to its master solver, from the master to every pricer added,
  from the loop to the master, and from a `GraphPricer` to its graph. Tolerances and initializations are
  `shared_ptr` in the params (smart holder, as the graph bindings).
- **GIL:** `run()` releases it inside `ActiveCall::run_interruptible`, as `solve` does. Trampolines
  (`PYBIND11_OVERRIDE`) and hooks (the `std::function` caster) take it back for each call. A `GraphPricer` never
  calls Python.
- **Ctrl-C:** each stage gets `should_stop`, as `solve` does. An interrupted stage is not `COMPLETE`, so it proves
  nothing: with no column, the run ends `NotProven`, then `KeyboardInterrupt` is raised. A stage interrupted after it
  found columns, or an LP solve, does not stop the run at once (`D-87`).
- **Exceptions:** C++ ones map as in the graph bindings (`invalid_argument` → `ValueError`…). A Python exception
  raised in a trampoline or a hook leaves `run()` unchanged.
- **Self-test:** bound, so a user checks their own Python master solver: `master_solver_self_test(minimizing,
  maximizing, tolerance)` returns the report with every failed check.

### Graph pricers (`D-48`)

- `PricingStage(algorithm="simple", params=None, direction=None, exact=None)`: the arguments of `ResourceGraph.solve`
  (`params` is `AlgorithmParams` or `BucketAlgorithmParams`) and whether the stage is exact. `exact=None` takes it
  from the algorithm (`D-88`).
- `GraphPricer(graph, stages=None, cost_index=0, preprocess=True)`: flushes the Python graph's buffered nodes and arcs,
  then calls `_make_pricer` on its C++ graph (`F-03`); the result is a C++ `GraphPricer<ResourceTypes...>` seen as a
  `Pricer`. No stage given: one exact `simple` stage, the C++ default.
- `_make_pricer` builds one C++ `PricingStage` per stage: its search copies the params, caps `timeout_s` by the
  request (as `PricingStage::make`) and calls the dispatch `solve` already uses (`dispatch_algorithm`,
  `run_bucket_solve`). No new list of algorithms.
- Not offered: a Python function as a stage (pricing stays free of the GIL); a Python `Pricer` subclass is the way to
  price in Python.
- The pricer holds the graph: solving that graph elsewhere during `run()` is not supported (as in C++).

### Master solvers (`D-54`, `D-60`)

| solver | route | how |
|---|---|---|
| HiGHS | B, default | `HighspyMasterSolver(model, rows)` reads `model.getLp()` (costs, bounds, matrix, types, sense, row bounds; a row-wise matrix is converted) and builds the C++ HiGHS master solver over a copy. `write_back()` adds the generated columns (cost, bounds, entries, type) to the user's model in one `addCols` (`F-17`); values come from the results |
| Gurobi | A (`D-86`) | `GurobipyMasterSolver`, a Python subclass (section 7); B1 and B2 tested and compared later (T-1.11) |
| any solver | A | a Python `MasterSolver` subclass (trampoline): CPLEX, Xpress, or gurobipy itself; must pass the self-test |

- The copy is the model as it was copied: later changes to the user's model are not seen (`D-92`). Options of the
  user's `Highs` (time limit, threads…) are not copied; `set_option` reaches the copy.
- HiGHS is in the wheel (`D-14`), so `HighspyMasterSolver` needs `highspy` only because the user's model is a
  `highspy.Highs`.

### Registry (`D-39`, `D-49`, `D-89`)

Names accepted where an object is: tolerances `"absolute"`, `"relative"`; initializations `"artificial_columns"`,
`"none"`; stage algorithms (the graph's names). Master solvers by class only: each wraps a model, so a name alone
cannot build one.

## 4. Build and packaging

- `_core` links `rcspp_highs` when `USE_HIGHS` is on; `column_generation_highs.cpp` is left out otherwise (the module
  sources are a glob today).
- Wheel: `-DUSE_HIGHS=ON` in `pyproject.toml` `cmake.args`; HiGHS fetched and static (`D-82`), about a minute more
  per wheel.
- **Risk, to check in PR 6:** our static HiGHS and highspy's own HiGHS in one process (`F-16`). On Linux and macOS an
  extension exports the symbols of a static library unless told not to; build the fetched HiGHS with hidden
  visibility (or link with `--exclude-libs`), and test a run with `highspy` imported.
- Extras `[highspy]`, `[gurobipy]` (named after the packages).
- Coverage (`F-14`): `python/bindings/` is gated at 100 %; the HiGHS binding is tested by the Python tests with
  `USE_HIGHS` in the coverage build. Gurobi code: `D-50`.

## 5. Tests (`tests/python/test_column_generation*.py`)

| test | expected |
|---|---|
| the C++ end-to-end tests, in Python, over every master solver: C++ HiGHS from arrays (PR 5), `HighspyMasterSolver`, a Python subclass over `highspy` (route A, the trampolines with a real LP), Gurobi (skip without a license) | the C++ values: toy LP 37.5 and duals (11.5, 12.5, 13.5), MIP 44; from scratch 37.5; fleet 2 / 1 `Optimal` / `Infeasible`; four customers 40 |
| self-test on every master solver above | report passed |
| scripted Python master solver and pricers (as the C++ fakes) | the loop's statuses reach Python; hooks called; a Python exception leaves `run()` |
| Python `Tolerances` and `MasterInitialization` | used by the loop |
| graph pricer stages | heuristic then exact; bucket params; `exact` default (`D-88`) |
| `write_back()` | the user's model holds the columns, its LP gives the same value |
| both loops (`D-90`) | same results |

## 6. PRs and commits

| PR | # | commit | contents |
|---|---|---|---|
| 5 | 5.0 | `refactor(python): the GIL release and Ctrl-C check in their own header` | `active_call.hpp`, so the column generation bindings need not include the graph templates (`48c4afa1`) |
| 5 | 5.1 | `feat(python): column generation types, master and loop` | bindings; `MasterSolver` and `Pricer` trampolines (needed to test anything without a solver); scripted Python fakes. Done: `d1541637`, typed for a strict check in `e8ed2b81` and `05209cc2` (24 tests; pure overrides fail under the GIL, `F-30`) |
| 5 | 5.2 | `feat(python): tolerances and initializations from Python` | their trampolines; the self-test. Done: `af741808` (30 tests; a Python object held by the params' `shared_ptr` stays alive) |
| 5 | 5.3 | `feat(python): graph pricers` | `_make_pricer`, `PricingStage`, `GraphPricer`. Done: `60acebaf` (`solve`'s algorithm and direction rules shared), `5a063470` (10 tests). Built differently: each graph type registers a factory looked up by its Python type, instead of a `_make_pricer` method; a stage's direction is passed separately, so two stages may share params. Ctrl-C measured: `D-100` |
| 5 | 5.4 | `feat(python): HiGHS master solver` | `USE_HIGHS` binding over a copied model; `_core` links `rcspp_highs` |
| 5 | 5.5 | `test(python): column generation end to end` | section 5 on the C++ HiGHS master solver |
| 5 | 5.6 | `feat(python): components by name` | registry (`D-89`) |
| 6 | 6.1 | `feat(python): master solver over a highspy model` | `HighspyMasterSolver`, `write_back` |
| 6 | 6.2 | `feat(python): master solver over a gurobipy model` | `GurobipyMasterSolver` (route A, `D-86`); tests skip without a license (`D-50`) |
| 6 | 6.3 | `build(python): the wheel with HiGHS, extras` | `pyproject.toml`; the symbol check of section 4 |
| 6 | 6.4 | `docs(examples): a column generation loop written in Python` | `D-90`; a test that it matches the C++ loop on the toy |

## 7. Research: route A over gurobipy or highspy (2026-10-21, was `phase-1-design.md` §3)

What a Python `MasterSolver` subclass calls in each library (key URLs below). For highspy, route B (copy) replaces it.

| contract method | gurobipy | highspy |
|---|---|---|
| map our row id → solver row | keep the user's `Constr` list (`Constr.index` shifts on model changes, so never cache it) | `highs_cons.index` or plain row ints |
| `add_columns(batch)` | loop: `Column(coeffs, constrs)` + `addVar(obj, lb, ub, column=…)`, then `update()`; no batch call exists | native CSC `addCols(n, cost, lower, upper, nnz, starts, indices, values)` |
| relax in place | set `VType='C'` on the user's integer variables, remember them (`model.relax()` copies the model: avoid) | `changeColsIntegrality`, or option `solve_relaxation=true` |
| `solve_relaxation` | `Method=1/0`, `optimize()` (releases the GIL); warm start is automatic after adding columns | `solver="simplex"`, `run()` (releases the GIL; avoid `solve()`'s extra thread) |
| `duals` | `model.getAttr("Pi", constrs)`; flip if `ModelSense = MAXIMIZE` | `getSolution().row_dual`; flip if maximizing |
| `solve_integer` | restore `VType`, set ours to `I`/`B`, `optimize()` | restore integrality, `run()` |
| `farkas` (optional) | `InfUnbdInfo=1`, `FarkasDual` (sign differs: normalize) | `getDualRay()` |

Support (`D-47`):

- **First:** gurobipy, highspy.
- **Later:** python-mip (no batch add, undocumented warm start) and OR-Tools MathOpt (`IncrementalSolver`, stable
  ids).
- **Not supported:**
  - PuLP: rebuilds the model on every solve.
  - pywraplp: no duals from MIP solvers, no column delete.
  - Pyomo persistent: one Pyomo `Var` per column, heavy overhead.
  - PySCIPOpt as a master: `freeTransform()` drops the LP every iteration, and duals need presolve off. It belongs
    in the SCIP adapter (phase 4).
- Gurobi's own cutting-stock example copies the model with `relax()` every iteration: our wrappers must not.

Key sources: <https://docs.gurobi.com/projects/optimizer/en/current/reference/python/column.html>,
<https://docs.gurobi.com/projects/optimizer/en/current/reference/attributes/constraintlinear.html>,
<https://support.gurobi.com/hc/en-us/articles/360051966772-Releasing-the-GIL-in-Python-API>,
<https://raw.githubusercontent.com/ERGO-Code/HiGHS/master/highspy/highspy/highs.py>,
<https://raw.githubusercontent.com/ERGO-Code/HiGHS/master/examples/branch-and-price.py>,
<https://ergo-code.github.io/HiGHS/dev/terminology/>.
