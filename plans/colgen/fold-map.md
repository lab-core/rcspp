# Fold map: the stack rewritten into `examples/column_generation/` (`T-1.17`, `D-147`)

Written 2026-11-12, no branch rewritten. Inputs: the rebased stack (`separation.md` §8; the eight local branches checked
equal to `origin` the same day: `e764bc87`, `00e8c0b9`, `76b873c5`, `fce956b3`, `2f7b0577`, `5e58755b`, `6c79e917`,
`c623f120`), the prototype (`separation.md` §7, `prototype/column-generation-example` `325bba34`), `D-142`,
`D-144`–`D-153`. Proposed here: `D-154`–`D-158`; findings `F-54`, `F-55`; for the user: `Q-56`–`Q-59`.

| file | contains |
|---|---|
| this one | the method, the filter's rules and dry run, the rewrite order and its checks, what to raise |
| `fold-map-commits.md` | every commit of the eight PRs: keep, rewrite, drop, split, new; new messages |
| `fold-map-targets.md` | where each §7 item goes; each PR head's builds, tests and description; the final state |
| `tools/fold_filter.py` | the snapshot filter (writes objects only, never a ref) |
| `fold-record.md` | the rewrite as done (2026-11-12): heads, checks, departures, what is pending |

## 1. Method (`D-154`)

Three passes, in this order, from the eight current branches (never from the prototype):

| pass | what | how | conflicts |
|---|---|---|---|
| A. filter | every path move, every mechanical content change, the files reset to `main` | `tools/fold_filter.py` transforms each commit's whole tree (a snapshot), so a file that later commits edit moves in all of them at once and the diffs follow it | none: snapshots, not patches |
| B. residue | hand edits in moved files (skip blocks, `isort: split`, comments, docstrings) | `git commit --fixup=<sha>` on top of the filtered chain, `<sha>` from `git blame` of the line on the filtered tip | few: a fixup only moves lines a later commit may touch |
| C. manual | build files, hooks, new files, new and split commits, messages | one `git rebase -i --autosquash --update-refs` over the filtered chain (todo written by a script through `GIT_SEQUENCE_EDITOR`): `edit`, `drop`, `exec`, new commits, `reword` | only in shared build files, all in the manual set |

Why snapshots: 90 commits, about 60 that only touch moved files; a patch-based rebase would replay each move. The
filter is deterministic and re-runnable from the original branches: tune its rules until pass B's residue is the
short list below, and only then start B and C (re-running A discards B and C).

## 2. The filter (`tools/fold_filter.py`)

`python plans/colgen/tools/fold_filter.py origin/main feature/column-generation-pure-python [--keep-pure-name]`:
prints `old new subject` per commit (`empty` when its tree equals its new parent's: dropped) and the new tip. Copies
author, committer and message; blobs it does not change (LFS pointers included) are reused as they are.

**Moves** (first match on the original path; `X` = `examples/column_generation/`):

| from | to |
|---|---|
| `cpp/rcspp/column_generation.hpp`, `cpp/rcspp/column_generation/` | `X/cpp/rcspp/column_generation.hpp`, `X/cpp/rcspp/column_generation/` |
| `cpp/rcspp_backends/{gurobi,highs}/` (#69, #76; moved by #80 later) | `X/cpp/rcspp/column_generation/{gurobi,highs}/` |
| `tests/cpp/test_column_generation*`, `tests/cpp/{gurobi,highs}/` | `X/tests/cpp/` |
| `python/bindings/column_generation{,_highs}.cpp`, `python/bindings/graph_pricer.hpp` | `X/bindings/` |
| `python/src/rcspp/column_generation/` | `X/python/cpp_column_generation/` |
| `python/src/rcspp/_core/column_generation.pyi` | `X/python/typings/rcspp/_core/cpp_column_generation.pyi` (`D-153`) |
| `tests/python/test_column_generation*.py`, `tests/python/column_generation_fakes.py` | `X/tests/python/` (names: `Q-57`) |
| `examples/python/column_generation_loop.py` | `X/python/column_generation_loop.py` |
| `python/src/rcspp/python_column_generation/` (`Q-56`) | `python/src/rcspp/column_generation/` |
| `tests/python/test_python_column_generation*`, `python_column_generation_fakes.py` (`Q-56`) | `tests/python/test_column_generation*`, `column_generation_fakes.py` |

**Drops:** the stack's CMake files of the layer, its master solvers and their tests (`cpp/rcspp_backends/*/`,
`cpp/rcspp/column_generation/{,gurobi/,highs/}`, `tests/cpp/{gurobi,highs}/` `CMakeLists.txt`: pass C writes the
example's) and `tests/find_package/column_generation*.cpp` (#80's component consumers).
**Reset to `main`'s blob** (every stack change to them is column generation's): `cpp/rcspp/rcspp.hpp` (its
column generation includes), `tests/cpp/test_main.cpp` (one comment line), `python/src/rcspp/__init__.py`,
`python/src/rcspp/_core/__init__.pyi` (`D-153`).
**Content rules**, on files that land under `X/`: `rcspp_backends/{gurobi,highs}/` → `rcspp/column_generation/...`;
relative imports of `rcspp` made absolute (`.._core.column_generation` → `rcspp._core.cpp_column_generation`,
`..graph`, `.._core.graph`, the stub's `.graph`); `rcspp._core.column_generation` → `rcspp._core.cpp_column_generation`;
`rcspp.column_generation` → `cpp_column_generation`; `USE_{GUROBI,HIGHS}`, `USE_COLUMN_GENERATION{,_GUROBI,_HIGHS}`
and their `RCSPP_` defines → the `D-151` names. With `Q-56`, on `python/src/rcspp/`, `tests/python/`, `.flake8`:
`rcspp.python_column_generation` → `rcspp.column_generation`, `python_column_generation_fakes` →
`column_generation_fakes`, the A005 paths.

**Dry run (2026-11-12, `F-54`):** 35 s; tip `d243d76c` (dangling objects only). Emptied by the filter alone:
`5632dced`, `ef5b40a7`, `3934038c`, `2684e8d5`. Collisions: none. Outside the moved paths the filtered tip differs from
`c623f120` only in the reset and dropped files and `.flake8`. Against the prototype, the example's files differ in:
the hand-written files (not produced by A: the five CMake files, `bindings.cmake`, `cpp_column_generation.cpp`,
`graph_pricer_factory.hpp`, the interrupt test), main's renames (intended), and the pass-B residue:

| file (under `X/`) | residue (prototype side) | first commit (blame) |
|---|---|---|
| `tests/python/column_generation_fakes.py`, `test_column_generation_{end_to_end,highs}.py` | the path insert to `X/python`, `import rcspp._core as _core`, the skip on `cpp_column_generation`, its message | `90af6ac9`, `147e24bc`, `0d2ae1f3` (stack's skip: `348d3e44`) |
| `tests/python/test_column_generation{,_gurobipy,_highspy,_graph_pricer,_registry}.py`, `_end_to_end.py` | `# isort: split` and its comment; `rcspp` imports after the package's | the commit adding each file |
| docstrings and comments of the fakes, `_end_to_end.py`, `_gurobipy.py`, the loop script, `column_generation_highs.cpp` (line wrap), `column_generation.hpp` (`@brief`), `__init__.py` and `master_solvers/__init__.py` ("as the wheel is" goes), `params.py` (blank line) | `c63d010e`'s wording | the commit that wrote the line |
| the loop script's run instructions and import order | build with the example, `PYTHONPATH=build/python` | `435d720d` |
| `gurobipy_master_solver.py` two annotations | `0dc28c12`'s pyright fixes | `5faf74b7` |

With `Q-56`, pure-layer residue: `__init__.py:3-8` (written against the C++-backed `rcspp.column_generation`;
`221af3f0`), `master_problem.py:146` comment ("the C++-backed layer's" → the example's; `ca634a3c`).

## 3. Rewrite order and checks (`D-158`)

| step | do | check |
|---|---|---|
| 0 | in `../rcspp-rebase`: check the eight branches still equal `origin`; local refs `backup/colgen-<name>-before-fold` at the current heads | `git rev-parse` both sides |
| 1 | run the filter (rules tuned, `Q-56`/`Q-57` answered); temporary refs `fold/<name>` at the mapped heads | 4 empty commits; `git diff -M --stat` old vs new head per PR: renames only, plus the rules' lines; `git range-diff` lists every commit |
| 2 | pass B: fixups on `fold/pure-python` (table above) | `git diff <prototype> fold/pure-python -- X` shows only hand-written files and intended differences |
| 3 | pass C, one PR at a time from #61 (`fold-map-commits.md`), `--update-refs` moving the `fold/*` refs | at each PR head: `fold-map-targets.md` §2 (off and on, MSVC); pre-commit on the PR's files; stop at the first failure |
| 4 | Linux gcc 13 in Docker (`F-52`: `-j2`, no clang-tidy) at #77, #80, #88 heads, option on | as §8 |
| 5 | final state: `fold-map-targets.md` §3 | every difference from the prototype is on the list |
| 6 | `git branch -f feature/column-generation-<name> fold/<name>`; the user force-pushes; PR descriptions updated (`fold-map-targets.md` §2) | each branch's base is the previous head |

Build folders (`F-51`, `F-52`): a fresh MSVC folder per head with the option on (exports.def, junction), one off
folder reused; delete `rcspp.dir/Release/exports.def` when the objects change.

## 4. Raised with the user (decided 2026-11-12: `D-159`–`D-162`)

| ID | question | outcome |
|---|---|---|
| `Q-56` | Rename the pure package to `rcspp.column_generation` in #88? | yes, as the user prefers: the filter maps it from its first commit, so #88 adds `rcspp.column_generation` directly and no rename commit exists; #77 never adds a C++-backed `rcspp.column_generation` → `D-159` |
| `Q-57` | New names (`D-12`): the example's Python tests share basenames with the renamed pure tests (`F-55`) | `X/tests/python/test_cpp_column_generation*.py` and `cpp_column_generation_fakes.py`, after the package → `D-160` |
| `Q-58` | The old-flag configure error (`USE_COLUMN_GENERATION*` stops the configure) | drop it: after the fold no commit ever had those options (`F-55`); the PR descriptions say it once → `D-161` |
| `Q-59` | Shipped: the wheel installs only its `python` component (`install.components`), which came with the wheel's HiGHS (`77a43e27`) | keep it, moved to `2e0bdb37` with its own reason: without it the wheel also gets the C++ library, headers and CMake package that the plain install adds → `D-162` |

Decided in this map, accepted by the user 2026-11-12: `D-154` (method), `D-155` (the example's CMake written in
its final form where it first appears), `D-156` (`[tool.pyright]` moves to #77), `D-157` (no static
`rcspp_column_generation`), `D-158` (order, CI per PR, README in #80).
