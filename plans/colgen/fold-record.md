# Fold record: the stack rewritten (`T-1.17`, `D-147`, `fold-map.md` §3)

Done 2026-11-12 in `../rcspp-rebase`, steps 0–5 of `fold-map.md` §3 (step 4, Linux gcc 13: §5). Step 6 (§6): branches
moved and force-pushed 2026-11-13, after the `Q-60` fixup (`D-166`), then the PR descriptions. The eight
`feature/column-generation-*` branches are untouched and equal `origin`; the result is on the local refs `fold/<name>`.

## 1. Heads

Old = `origin` (checked equal 2026-11-12); filter = pass A's mapped head; fold = final. Backups:
`backup/colgen-<name>-before-fold` at the old heads (local, no upstream).

| PR | `fold/` | old | filter | fold | commits (old → fold) |
|---|---|---|---|---|---|
| #61 | `core` | `e764bc87` | `8c1c5499` | `de93c73a` | 18 → 19 |
| #69 | `gurobi` | `00e8c0b9` | `9b0cd003` | `c0d3284d` | 20 → 18 |
| #72 | `pricer` | `76b873c5` | `ec56789b` | `613b18e3` | 7 → 6 |
| #76 | `highs` | `fce956b3` | `bf490af4` | `128578aa` | 17 → 15 |
| #77 | `python` | `2f7b0577` | `d63686c4` | `4ea70d66` | 10 → 13 |
| #78 | `python-masters` | `5e58755b` | `403256e9` | `efe39fb9` | 7 → 8 |
| #80 | `packaging` | `6c79e917` | `d262ac9d` | `02c2aa4b` | 5 → 4 |
| #88 | `pure-python` | `c623f120` | `fcc7f442` (pass B: `dc81246e`) | `f31257f0` | 6 → 6 |

90 → 89 commits, as `fold-map-commits.md` counts. Each fold head's base is the previous fold head.

## 2. Checks (MSVC 14.51, Release, Gurobi 13.0.1 and HiGHS 1.15.1 fetched; `fold-map-targets.md` §2)

C++ = ctest (8 disabled benchmarks not run); Python = `tests/python`; example = `examples/column_generation/tests/python`.
"main" counts: the off builds compile exactly `main`'s sources until #80 (611 C++, 313 Python, 1 skip).

| PR | off: C++ / Python / example | on: C++ / Python / example | pre-commit (PR's files) |
|---|---|---|---|
| #61 | 611 / 313 / — | 652 / 313 / — | pass |
| #69 | 611 / 313 / — | 687 / 313 / — | pass |
| #72 | 611 / 313 / — | 706 / 313 / — | pass |
| #76 | 611 / 313 / — | 730 / 313 / — | pass |
| #77 | 611 / 313 / 6 modules skip | 730 / 313 / 57 (+2 POSIX skips) | pass |
| #78 | 611 / 313 / skip | 730 / 313 / 116 (+2) | pass |
| #80 | 612 / 313 / skip | 731 / 313 / 116 (+2) | pass |
| #88 | 612 / 587 / skip | 731 / 587 / 116 (+2) | pass |

On-totals equal `separation.md` §8 (652, 687, 706, 730, 730 + 370, 429, 731, 703). Also:

- #76: one configure and build over an installed HiGHS (`-Dhighs_DIR`, the fetched one installed): "HiGHS: installed".
- #77 off: `_core` holds no column generation string, has no `cpp_column_generation`; `mypy.stubtest rcspp._core`
  (mypy 2.4.0, CI's arguments) passes. clang-tidy on `graph_impl.hpp`, with and without the example's define: no
  error, the same warnings both ways.
- #80 on: `install_and_find_package` builds the example's layer alone over the install and runs its 71 tests.
- #88: pyright 1.1.408 strict: 0 errors on the pure package, its tests and the example.
- Linux stand-in at #77, #80, #88: every quoted include and CMake path of the example and of the core hooks names a
  tracked file with its exact case; no header or feature newer than libstdc++ 13 (script: §7).
- Final state (§3 of the targets): the prototype's changes over `129b15e0` against the fold's over `main`, file by
  file: only the 16 listed differences, plus departure 3 below.

## 3. Departures from the map

1. **Filter rules (step 1, D-160):** the example's tests move to `test_cpp_column_generation*`, the fakes to
   `cpp_column_generation_fakes.py`, and two content rules follow the names (`column_generation_fakes` and
   `from/import test_column_generation` in the example). The dry run of `F-54` predated `D-160`.
2. **Pass B residue beyond §2's table:** four pure tests' docstrings named the old C++-backed test files (now the pure
   tests' own names); isort's order after the renames (pure tests: `rcspp.column_generation` before `rcspp.graph`).
3. **`# isort: split` in three more example tests** (graph pricer, registry, interrupt) with the prototype's
   "First: importing the fakes…" comment: renamed `cpp_column_generation_fakes` sorts after `cpp_column_generation`,
   and the fakes import must come first (it puts the package on the path and skips when off).
4. **Skip blocks in the commits adding each file** (as §2 says), with `import pytest` added to the fakes at
   `90af6ac9`; the end-to-end test keeps the fakes first (with the split) until `435d720d`, which brings the loop
   script and gives its import block the final form.
5. **Include order** (`F-57`): every commit touching an example C++ file gets its quoted include runs sorted (a step
   after each commit), and conflicts on them are resolved as the union, sorted.
6. **Blank lines** isort wants, placed in the commit adding the line: before `from . import registry` (`2f7b0577`),
   before `from .master_solvers import HighspyMasterSolver` (`286f412c`); the fakes' imports one block (`64ace93c`).
7. **Texts written ahead:** the root option and comment have the prototype's final text from the new #61 commit (they
   mention the bindings and master solvers before those commits); the example's `CMakeLists.txt` header is short in
   #61 and final in `6c79e917`; `ci-full.yml`'s comment is short in #61, full in `24ad74c6`.
8. **Main's names** in the interrupt test (`SupersetDominanceFunction`) and in `graph_pricer_factory.hpp`'s list of
   the `graph_impl.hpp` names it uses (`CostResourceType`, `LabelContainerType`…): row 2 of the targets' §3.
9. **Hooks at commit time:** pass B's fixups and pass C's scripted commits skip the pre-commit hook, as rebase picks
   do (all hooks were run on pass B's files first); every hook runs on each PR's files at its head (§2).
10. **Messages** of the new commits and `77a43e27`'s split written for the map's subjects; `24ad74c6`'s body says the
   fetched HiGHS is not installed; `2e0bdb37` gains one bullet line (`D-162`); new commits carry the user's identity.
11. **`Q-60` fixup (`D-166`), after the Linux run:** #88 rebased alone onto `fold/packaging` with a fixup of
    `ae69347f` (now `c5df6912`); every #88 commit differs from its old one only in `tests/python/test_column_generation.py`,
    messages unchanged. The test file passes at each of the six commits; `tests/python` at the head 587 passed, 1 skip;
    pre-commit passes on the file; a planted `from rcspp._core import cpp_column_generation` fails the test; the
    final-state check differs from before only in that file's line counts. Linux was not rerun (one pure test).

Conflict resolutions (all in the example's or the pure layer's Python, or include runs): the session notes; none
changes behaviour.

## 4. Findings

`F-57` (in `inventory.md`); `Q-60` (in `questions.md`, decided: `D-166`).

## 5. Linux gcc 13 (step 4), 2026-11-12

`linux-fold-check-run2.sh` (the user's session folder, `rcspp-fold-session`): Docker `ubuntu:24.04`, gcc 13.3, a
clean clone of the main repo per head (no LFS fetch, pybind11 submodule fetched), `-j2`, no clang-tidy, the example
and HiGHS on, no Gurobi; ctest, `tests/python`, the example's tests. About 20 minutes per head.

| head | C++ | `tests/python` | example |
|---|---|---|---|
| #77 `4ea70d66` | 706 / 706 passed | 314 passed | 59 passed (57 + 2 SIGINT) |
| #80 `02c2aa4b` | 707 / 707 passed | 314 passed | 89 passed, 9 skipped (+ gurobipy module skipped) |
| #88 `16a89b1c` | 707 / 707 passed | 552 passed, 8 skipped | 89 passed, 9 skipped (+ gurobipy module skipped) |

No failure or error; C++ totals exact; #88's 641 passed and 17 skipped = §8's 639 + the 2 SIGINT tests. The example's
skips are all gurobipy; `tests/python`'s 8 at #88 were not listed (no `-rs`); its 314 = Windows' 313 + 1 skip.
A first run with LFS pointers for `instances/*.txt` failed the 43 instance-reading C++ tests and `test_vrp.py` at
each head; the second copies the host's instance files (unchanged by the stack) into each clone.

## 6. Step 6, 2026-11-13

`git branch -f feature/column-generation-<name> fold/<name>` for the eight, then the force-push in stack order (#61
first), each with `--force-with-lease` at its old head (`step6.sh` in the session folder); all eight accepted. Then the
eight PR descriptions (`fold-map-targets.md` §2): the shared note at the top, new paths, options and test names; new
titles for #61, #77, #80; #80's body and #88's opening rewritten; the Linux results added (#78, #80, #88); the old
bodies kept in the session folder (`pr-bodies/before`). Then the cleanup: `fold/*`, `refs/fold-target/*`, the
`cmake-build-fold-*` folders and the junction removed, `../rcspp-rebase` back on its branch. The backups
`backup/colgen-<name>-before-fold` stay.

## 7. Tools (session folder)

`stages.py` and `todo.py` (pass C: one function per R/N/S commit, the todo through `GIT_SEQUENCE_EDITOR`), `fixups.py`
(pass B: hunks to the oldest blamed commit, in memory), `cont.sh`, `headcheck.sh` (MSVC off/on), `casecheck.py`
(case, libstdc++ 13), `finalcheck.py` (§3 check), `resolutions.md`, `results.txt`, logs.
