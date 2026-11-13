# Fold review: the eight folded PRs read before they leave draft (`D-165`)

Read only, 2026-11-13 (Opus; five subagents read the commits of #61, #69 + #72, #76 + #77, #78 + #80, #88; every
finding below was re-checked by hand before it went in). Bodies, titles and checks 1, 3, 5 read directly.

## 1. Heads (`git fetch`, 2026-11-13)

| PR | branch | expected | `origin` | commits | base |
|---|---|---|---|---|---|
| #61 | `core` | `de93c73a` | `de93c73a` | 19 | `main` `6367ee56` (= `origin/main`) |
| #69 | `gurobi` | `c0d3284d` | `c0d3284d` | 18 | `core` |
| #72 | `pricer` | `613b18e3` | `613b18e3` | 6 | `gurobi` |
| #76 | `highs` | `128578aa` | `128578aa` | 15 | `pricer` |
| #77 | `python` | `4ea70d66` | `4ea70d66` | 13 | `highs` |
| #78 | `python-masters` | `efe39fb9` | `efe39fb9` | 8 | `python` |
| #80 | `packaging` | `02c2aa4b` | `02c2aa4b` | 4 | `python-masters` |
| #88 | `pure-python` | `f31257f0` | `f31257f0` | 6 | `packaging` |

No difference; 89 commits; all eight are drafts with the right base; `../rcspp-dev` is at `f31257f0`.

## 2. Findings

Severity: **B** blocker before the merge, **S** should fix, **N** nit. "fixup X" = a fixup of commit X on its branch;
"body" = PR description edit. `X` = `examples/column_generation/`; paths at the PR's head unless a commit is named.

### Across the stack

| # | where | what is wrong | sev | fix |
|---|---|---|---|---|
| V.1 | `CHANGELOG.md` | The stack never touches it, while every PR of `main`'s stacks adds entries (#56–#59, #29…; "each entry says what an existing model or caller sees differently"). Due: Added `rcspp.column_generation` (#88); Fixed the install, #79 (#80); Behaviour changes: a plain `cmake --install` no longer installs the Python module, GoogleTest no longer installed (#80); numpy now a dependency, extras `highspy` / `gurobipy` (#78) | B | one commit per PR, or one in #88 (question 1) |
| V.2 | titles | `main`'s stacked titles end with "(k of n)", which becomes the squash subject; none of the eight has it. #78's `feat(python): master solvers over the user's model` reads as a library feature, while the classes are the example's | S | body/title edits, e.g. "feat(column_generation): the example's master solvers over a highspy or gurobipy model (6 of 8)" |

### #61

| # | where | what is wrong | sev | fix |
|---|---|---|---|---|
| V.3 | `CMakeLists.txt:15` (`e5bc0345`) | Option help "Build the C++ column generation example … into the Python module": the option builds the layer and its tests without Python too (`X/README.md` says it right) | S | fixup `e5bc0345`: "…: its layer, its tests and, with USE_PYTHON, its bindings" |
| V.4 | `X/cpp/rcspp/column_generation/master_solver.hpp:42-43` | `add_rows` brief: "a master solver wrapping a user's model, whose rows exist, does not [override it]"; both wrapping master solvers do (`Capabilities{.add_rows = true}`). `de93c73a` fixed `types.hpp` for the same reason but not this; same premise in `X/tests/cpp/test_column_generation_fakes.hpp:134-135` | S | fixup `de93c73a` |
| V.5 | `master_problem.hpp:73` (`53a6754d`) | "`@param solver` … must not hold columns the master did not add", while #61's own `657c4122`, `6c2b63d5` and a test use starting columns in the user's model; `d089521e` (#69) corrects it | S | move that hunk from `d089521e` into `53a6754d`; drop its sentence from `d089521e`'s message |
| V.6 | `.pre-commit-config.yaml:76-78` | docformatter keeps `^(python\|tests)/`; black, isort, flake8, bandit cover `X`. Body and `e5bc0345` say the example's Python is linted "as rcspp's" | S | extend docformatter's `files` in `e5bc0345` (may reformat later example docstrings, untried) or reword body and message |
| V.7 | `X/tests/cpp/test_column_generation.cpp:4-5` (`7935ba47`) | "see the note at the top of test_main.cpp": no such file in `X`; core's note (MinGW TU split) does not apply to an executable of its own | N | fixup `7935ba47`: drop the pointer |
| V.8 | messages | `074abfe4` omits the object library `rcspp_column_generation_objects` it creates, and says "a component like the others" before any other; `fb27cc73` describes a move that does not happen in the PR; `9fa2271d`, `fb27cc73`, `de93c73a` only correct earlier #61 commits | N | reword, or fold (question 2) |
| V.9 | texts ahead | Beyond the accepted ones: `X/cpp/.../CMakeLists.txt:1-2` ("master solvers … in the folders below"), `X/tests/cpp/CMakeLists.txt:1-2,18`, the `(gurobi\|highs)` lint excludes, AGENT.md's padding for lines #69/#76 add. AGENT.md's layout block (31-34) never lists `X` | N | accept; layout line in `02c2aa4b` |

### #69, #72

| # | where | what is wrong | sev | fix |
|---|---|---|---|---|
| V.10 | `2e3d222e`, `0b54940a`, `1c5594a0`, `796297e5` | Messages cite the HiGHS master solver ("as … the HiGHS master solver already do", "drifted apart") that comes in #76 | N | reword (question 2) |
| V.11 | `5161f90f` → `0662a672` | `5161f90f`'s comment "auto-discover … when no hint is provided" does not hold until `0662a672` adds the hint check | N | fold `0662a672` into `5161f90f` |
| V.12 | `CMakeLists.txt:22` (`5161f90f`) | "Find modules (FindGUROBI.cmake) shared by the library and the examples": the library uses none | N | fixup: "shared by the examples" |
| V.13 | #69 body; `49241709` | The toy's 2 tests are inside the 21 but listed apart; rows may also be free (`add_rows` doc). `49241709`'s trailer says "Claude Opus 5.5 (1M context)" | N | body; reword if touched |
| V.14 | #72 body | "status `COMPLETE`" (main's name is `AlgorithmStatus::Complete`); the umbrella-header line sits under "What this adds" | N | body |
| V.15 | `a3dba26d`, `613b18e3` | Only rewrite or complete `91bdec1d` of the same PR; `613b18e3` has no body | N | fold into `91bdec1d` (question 2) |

### #76, #77

| # | where | what is wrong | sev | fix |
|---|---|---|---|---|
| V.16 | #76 body | "Main suite: 671 passed, 1 skipped" is the pre-rebase count (`phase-1-design.md:265`); at the head: core 611 unchanged, `tests-column-generation` 71, `-highs` 24, `-gurobi` 24 (730 on) | S | body |
| V.17 | `X/cpp/.../highs/highs_master_solver.cpp:312-326` (`5d236572`) | Bug: `restore_upper_bounds` passes `columns_.all()` (N+k) with `upper_bounds_` (N entries, saved by a binary `solve_integer`): columns added after a binary integer solve (`solve_restricted_master_integer`, then `MasterProblem::add_columns`) make `changeColsBounds` read past the vector (UB, garbage bounds). The pure layer keeps the columns with their bounds (`highspy_master_solver.py:510-519`) | S | fixup `5d236572`: keep the column indices with the bounds, as the pure layer does; a test |
| V.18 | #76 body | "Its own bound comes back at the next LP solve and when the run ends": after `run()` then the integer solve, at the next LP solve or `restore_user_model`; test list names 17 of 19 (`RowBoundsFromTheModel`, `ZeroTimeLeftIsATimeLimit`); "the stack has what it needs to leave draft" and "the solver choices … will be explained there" (the docs page covers the pure layer) | N | body |
| V.19 | `3ed958c8` | ci-full.yml and AGENT.md name "its HiGHS master solver" one commit before `2b92d40b` | N | accept |
| V.20 | #77 body | The factory is said to be in `X/bindings/graph_pricer.hpp`; `make_graph_pricer` and `register_graph_pricer` are in `graph_pricer_factory.hpp` (the hooks paragraph is right) | S | body |
| V.21 | `X/bindings/bindings.cmake:17` (`e8f82c6c`); #77 body | "HiGHS itself (static: it ends up in the module)" / "the static HiGHS": only the fetched one; an installed `highs::highs` may be shared | S | fixup `e8f82c6c` "(static when fetched)"; body |
| V.22 | #77 body | "`test_cpp_column_generation*.py`, 57 tests, plus `…_interrupt.py` (new: …)": the glob includes it, and every file is new | N | body: "59 tests in six files: 57 + 2 SIGINT (POSIX)" |
| V.23 | `pyproject.toml:96-97` (`cba359e3`) | Stub "kept in the example, beside the rest of rcspp's stubs": it is not beside them; pyright overlays it. `cba359e3`'s message omits its pyproject change; `bindings.cmake:11` names `graph_pricer_factory.hpp` before `7dec5690`; `cpp_column_generation.cpp:7` defines `PYBIND11_USE_SMART_HOLDER_AS_DEFAULT` in a TU that binds no class | N | fixup `cba359e3` (comment); rest optional |

### #78, #80

| # | where | what is wrong | sev | fix |
|---|---|---|---|---|
| V.24 | #80, `CMakeLists.txt:184-212` (`612b6d4d`, `9f47a7b4`) | `install_and_find_package` is added under `if(USE_TESTS)` (default ON) with `CMAKE_SOURCE_DIR` / `CMAKE_BINARY_DIR` paths: a project using `docs/installation.md`'s FetchContent recipe gets a test that installs its own build and looks for its own `tests/find_package` (read, not run) | B | `if(USE_TESTS AND PROJECT_IS_TOP_LEVEL)` and `CMAKE_CURRENT_*` paths: fixups `612b6d4d`, `9f47a7b4` |
| V.25 | #80, `docs/installation.md:31-36` | `-DRCSPP_BUILD_PYTHON=ON` (the option is `USE_PYTHON`) and `pip install python/` (no `pyproject.toml` there: `pip install .`); worse now that `cmake --install` leaves the Python package out. From `main`, but #80 edits the file | S | fixup `612b6d4d` or a docs commit in #80 |
| V.26 | #80, `X/cpp/rcspp/column_generation.hpp:8` (`56ebb16b`) | "Column generation is a library of its own, so rcspp.hpp does not include it" | S | fixup: "The column generation example is not part of the library, so…" |
| V.27 | #78, `pyproject.toml:33`, `9ea1bc79`, body | numpy's reason is the example's ("column generation returns numpy arrays"; "`HighspyMasterSolver` was silently absent"), not in the wheel. Real reasons: `main`'s `pricing_pool.py:33` already imports numpy at module level without declaring it, and #88's package | S | reword comment, message, body (`D-104`'s decision stands, its reason is dated) |
| V.28 | #78, `X/python/.../highspy_master_solver.py:36`, `gurobipy_master_solver.py:122,150` | The example's `ImportError` says `pip install rcspp[highspy]`: that installs a PyPI rcspp without the example. Test `test_a_master_solver_without_its_package_names_the_extra`; body "The wheel" says the extras serve the example's classes (they serve #88's) | S | `pip install highspy` / `gurobipy`, test regex and name, body; against `D-105` (written when the layer shipped) |
| V.29 | #78, `2decd783` → `efe39fb9` | `2decd783` adds a module `__getattr__` that `efe39fb9` removes 12 minutes later; its message says "importing rcspp imported gurobipy", which the example never was | N | squash into `17da21a1` (question 2) |
| V.30 | #78, `X/python/cpp_column_generation/__init__.py:6-8`, gurobipy master solver | "importing rcspp does not load it": means `cpp_column_generation`; test `test_importing_rcspp_loads_no_lp_solver_package` | N | fixup `efe39fb9` |
| V.31 | #78 `0659e66b`; `examples/README.md` (`e29ea678`) | `highspy` added to `tests/python/requirements.txt` (the required CI installs it) unmentioned; the README pointer omits the HiGHS option and highspy, and after #80 sits under the Python section, not the new one | N | message/body; move the pointer in `02c2aa4b` |
| V.32 | #80, `pyproject.toml` comment, `612b6d4d`, body | `install.components = ["python"]` is said to keep the C++ library, headers and CMake package out of the wheel; they are already under `if(NOT SKBUILD)` (`CMakeLists.txt:142`). `D-162`'s premise is false at the head | N | keep it as a guard, reword the reason (new `D-` superseding `D-162`'s reason) |
| V.33 | #80, `CMakeLists.txt:182` (`9f47a7b4`) | "then build small program": `9f47a7b4` dropped `612b6d4d`'s "a" | N | fixup `9f47a7b4` |
| V.34 | #80, `X/README.md:5` (`02c2aa4b`) | "the package's `rcspp.column_generation`": written ahead of #88 and unclear | N | "rcspp's Python package has `rcspp.column_generation`, the same layer in Python" |

### #88

| # | where | what is wrong | sev | fix |
|---|---|---|---|---|
| V.35 | #88 body | Hard-wrapped paragraphs (from "Suggested reading order" on); GitHub renders each break; the other bodies are unwrapped | S | body |
| V.36 | #88 body | "3,434 lines of package code and about 5,100 lines of tests": the diff adds 3,759 and 5,674 | S | body |
| V.37 | #88 body | "the small changes in `graph.py` and `_core/graph.pyi`": #88 does not touch `graph.pyi`; it touches `.flake8` (unmentioned, also in `c5df6912`, `f31257f0`) | S | body |
| V.38 | `types.py:30-52`, `pricer.py`; `c5df6912`; body | "enums numbered as in C++": `auto()` starts at 1, C++ at 0; only the order matches (and is tested) | S | "in the C++ order" in message and body, or explicit values |
| V.39 | `8f309716`; body Ctrl-C row | `_ctrl_c_held_back` (`highspy_master_solver.py:103`) holds SIGINT during each HiGHS solve and stops a timed MIP; no message or body line says so | S | message; body row |
| V.40 | `column_generation/__init__.py:1-11` | The shipped package's docstring defines it by the unshipped example ("the features of the C++ column generation example"); "importing rcspp does not load it" is trivially true (rcspp never imports it): `rcspp.column_generation` | N | fixup `c5df6912`: say what it is first, the example as a pointer |
| V.41 | #88 body, `c5df6912` | "typed for pyright and mypy in strict mode": no mypy config covers the package (mypy runs only as stubtest) | N | "checked by hand with mypy --strict", or drop |
| V.42 | `tests/python/test_column_generation.py:425` | `test_the_layer_imports_nothing_of_the_cpp_layer`: the old vocabulary | N | `…_imports_nothing_of_the_example` (fixup `c5df6912`) |
| V.43 | `graph_pricer.py:157-160,194`; `highspy_master_solver.py:186-187` | Docstring omits the stop when time is up; `ValueError` without `GraphPricer:` prefix; a line break mid-bullet | N | fixups `82d99958`, `8f309716` |
| V.44 | messages, body | `c5df6912` "default_relative_gap" (`DEFAULT_RELATIVE_GAP`); `7d7a2d4e` and body row 4 call the `Column.to_arrays` Protocol a "stub"; `f31257f0` has a verbless sentence | N | reword |
| V.45 | `column_generation/__init__.py` | `OptionValue`, `FloatArray`, `IdArray` (needed to type a `MasterSolver` subclass) are not exported | N | optional: export, or point to `types` |

Totals: 45 findings: 2 blockers (V.1, V.24), 18 should fix, 25 nits.

## 4. Questions before the fixups

1. V.1: one CHANGELOG commit per PR (each squash carries its own entry) or one in #88?
2. Merge method: if each PR is squash-merged as #56–#59 were, commit messages and in-PR churn (V.8, V.10, V.11,
   V.15, V.29, message halves of others) never reach `main`: skip them? If commits are kept, fold them.
3. V.17: the C++ example is frozen; fix the HiGHS bound bug anyway (a fixup and a test), or leave it as a note?
4. V.2: "(k of 8)" titles, or "(k of 7)" with #88 apart (`D-147`: the pure layer its own PR)?

## 3. Checked, nothing found

| check | coverage |
|---|---|
| 1 hard rule | plan paths, IDs (`D-`…`I-`, `PA-`, `DP-`), "phase/tier n", "fold", review IDs: 8 bodies, 89 messages, every commit's added lines: none ("phase" hits are "two-phase", "single-phase") |
| 2 bodies | shared note byte-identical in all eight; counts 41, 21/10/4, 6/10/24, 24/24, 57, 116 (14/21/2/24/3), 71, 612, 587 = 313 + 274 match; every path, option, target and code sample checked; titles otherwise fit |
| 3 stale names | old names, flags, `_core.column_generation`, `COMPONENTS` (only `find_package(Python COMPONENTS …)`), old test/fake names: none in files at any commit or in messages; `rcspp.column_generation` only in #88 and `02c2aa4b` (V.34) |
| 4 commits | all 89 messages read against their diffs; churn only V.8, V.11, V.15, V.29 |
| 5 boundary | shipped tree imports nothing of `X` (`cpp_column_generation` only in a docstring and the isolation test); wheel pins the example off, no HiGHS; nothing of `X` installed (no rule, HiGHS `EXCLUDE_FROM_ALL`, `INSTALL_GTEST` off); hooks in `python/CMakeLists.txt`, `rcspp.cpp`, `graph_impl.hpp` all behind the option or its define; `active_call.hpp` a verbatim move; `graph.py` helpers faithful; `ci.yml` and coverage gates untouched |
| 6 readability | pure package read in full: `__all__` equals the imports, no abbreviations, no bug found beyond V.17 (example); `.flake8` A005 lines explained |
| 7 docs | `AGENT.md` option lines, `X/README.md` commands and options (standalone build defines its own options), `examples/README.md` links, `docs/installation.md` install paragraph: right but V.25, V.31, V.34 |

## 5. Fixes (2026-11-13, `D-167`–`D-170`)

One fix commit at each PR's tip, later branches rebased onto it (git 2.33 has no `--update-refs`: one `rebase
--onto` per branch). Local branches `fix/<name>` in the worktree `../rcspp-fix`; not pushed; bodies and titles drafted
in the session's scratchpad (`pr-new/`), not applied.

| PR | `fix/` | tip | findings fixed |
|---|---|---|---|
| #61 | `core` | `a7ea58ff` | V.1 (entry), V.3, V.4, V.5, V.7; body V.6 |
| #69 | `gurobi` | `725960bc` | V.1, V.12; body V.13 |
| #72 | `pricer` | `974c9c18` | V.1; body V.14 |
| #76 | `highs` | `e756e592` | V.1, V.17 (+ test, fails without the fix: bound 0); body V.16, V.18 |
| #77 | `python` | `66ee16e8` | V.1, V.21, V.23 (comment); body V.20–V.22 |
| #78 | `python-masters` | `49f7590e` | V.1 (numpy under Fixed), V.27, V.28, V.30, V.31; body; title |
| #80 | `packaging` | `cc8f117c` | V.1 (install Fixed, plain install Behaviour), V.24, V.25 (+ `--config Release`), V.26, V.32–V.34, V.9 (layout line), V.31 (pointer moved) |
| #88 | `pure-python` | `d075c4c3` | V.1, V.40, V.42, V.43; body V.35–V.39, V.41, V.44 |

All titles "(k of 8)". Not done: message-only findings and churn (V.8, V.10, V.11, V.15, V.29, message halves; `D-167`
(2)); texts ahead accepted (V.9 rest, V.19); V.45 skipped; V.23's `PYBIND11_USE_SMART_HOLDER_AS_DEFAULT` kept (every
binding source of `_core` defines it, `rcspp.cpp` included). Conflicts while restacking: the #61 test comment
(`4e380fd3`), the `examples/README.md` pointer (`050664f1`), resolved to the fixed text.

Checks: pre-commit on each fix commit's files (MSVC environment for clang-tidy, cppcheck on `PATH`); MSVC 14.50, Release,
clang-tidy off, Gurobi 13.0.1, HiGHS fetched, at each head off and on (`USE_PYTHON=ON`):

| PR | C++ off / on | `tests/python` off / on | example off / on |
|---|---|---|---|
| #61 | 611 / 652 | 313 + 1 skip / 313 + 1 skip | — |
| #69 | 611 / 687 | 313 + 1 skip / 313 + 1 skip | — |
| #72 | 611 / 706 | 313 + 1 skip / 313 + 1 skip | — |
| #76 | 611 / 731 | 313 + 1 skip / 313 + 1 skip | — |
| #77 | 611 / 731 | 313 + 1 skip / 313 + 1 skip | 6 skip / 57 + 2 skip |
| #78 | 611 / 731 | 313 + 1 skip / 313 + 1 skip | 8 skip / 116 + 2 skip |
| #80 | 612 / 732 (install check with the example's standalone build: passed) | 313 + 1 skip / 313 + 1 skip | 8 skip / 116 + 2 skip |
| #88 | 612 / 732 | 587 + 1 skip / 587 + 1 skip | 8 skip / 116 + 2 skip |

`tests/python` runs from `python/src`, which finds `_core` only in a folder named `build` (`rcspp/__init__.py`): a
junction `build` → `build-off`/`build-on` per run, `_core` rebuilt per head (`--parallel 2`, BelowNormal priority:
the full-core build slowed the user's machine). Every count equals `fold-record.md` §2, plus the new HiGHS test from
PR #76 on. Linux not rerun. Pushed 2026-11-13 after the user's review (in stack order, `--force-with-lease` at the
old heads; `--no-verify`: the only pre-push hook is Git LFS, and no LFS file changed); the eight titles and bodies
applied with `gh pr edit` (the body's `#` title line too). Next: the user runs run-ci on #88.
