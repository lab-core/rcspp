# Fold map: commit by commit (`fold-map.md`)

Codes: **K** pass A only (paths and rules; message kept) · **K0** untouched (engine or shipped code only; same diff)
· **R** rewritten in pass C (content below) · **D** dropped · **S** split · **N** new.
`X` = `examples/column_generation/`.
Pass-B fixups (`fold-map.md` §2) land in the commit named there and are not repeated here. Messages follow `D-55`;
"body" means the body is reworded to the new paths and option names, nothing else.

## #61 core: 18 commits → 19

| commit | code | content | message |
|---|---|---|---|
| `35f93188` | K | headers and tests to `X`; `rcspp.hpp` and `test_main.cpp` lines gone | kept |
| new, after it | N | root: `option(USE_COLUMN_GENERATION_EXAMPLE ... OFF)`, `if(...) add_subdirectory(examples/column_generation)`; `X/CMakeLists.txt` in-tree part only (headers target over `rcspp_objects`' include folders, `RCSPP_EXAMPLE_CORE`, `tests/cpp` under `USE_TESTS`); `X/tests/cpp/CMakeLists.txt` (googletest, `add_column_generation_test`, `tests-column-generation` over the headers); `.pre-commit-config.yaml`: the prototype's hook extension whole (Python hooks, cpplint, clang-tidy, cppcheck with `-I X/cpp` and the master solvers' exclude); `ci-full.yml`: `-DUSE_COLUMN_GENERATION_EXAMPLE=ON` and its comment; `pyproject.toml`: the `=OFF` pin in `cmake.args`; `AGENT.md`: the option's line | `build(column_generation): a C++ column generation example, off by default` |
| `e7402feb` | R | + `X/cpp/rcspp/column_generation/CMakeLists.txt` (object library only, `D-157`), its `add_subdirectory`, the tests link `rcspp_column_generation_objects` | kept |
| `a0d1b179` `f842295b` `0adcf191` `acdbe009` `6624bcf6` `5c9ddda2` `35326a78` `297db8a1` `5d4e95d3` `32d20c12` `ae36c1e0` `927932c6` `eba86bfa` `84e9308b` `09f653c6` `e764bc87` | K | to `X` | kept |

## #69 gurobi: 20 → 18

| commit | code | content | message |
|---|---|---|---|
| `541286ad` | R | keep the `FindGUROBI.cmake` move, the root module path, the VRP CMake line; the `USE_GUROBI` option, its find block and `AGENT.md` line go to `aac64776` | `build: FindGUROBI.cmake shared by the project`; body without the option paragraph |
| `aac64776` | R | root: `USE_COLUMN_GENERATION_EXAMPLE_GUROBI` option and the "needs `USE_COLUMN_GENERATION_EXAMPLE`" error; `X/CMakeLists.txt` Gurobi section (find, `GUROBI_REQUIRE_CXX`, subdirectory); `X/cpp/rcspp/column_generation/gurobi/CMakeLists.txt` final (`rcspp_column_generation_gurobi`); `tests-column-generation-gurobi`; `AGENT.md` line; no `tests/cpp` change | kept; body: "the `rcspp_column_generation_gurobi` library of the example, built with `USE_COLUMN_GENERATION_EXAMPLE_GUROBI`" |
| `36ff82d8` `a7889747` `523a63d5` `34cf6383` `628ba7f4` `1f6d242a` | K | | kept |
| `dad77827` | D | the solver test folders have their own executables from the start: nothing to leave out | — |
| `cfaf2d37` `8bd0c7bb` `6075a1f9` `c180accd` `2e5087b8` `b777a818` `b182a47e` `5d9780ad` | K | | kept |
| `1ae4e891` | R | `cmake/FindGUROBI.cmake` hunk only; the include-path hunk is moot (the library takes the headers target) | `fix(build): Gurobi from the hinted install only`; body's last sentence goes |
| `5632dced` | D | empty after pass A (the tests' CMake is the example's) | — |
| `00e8c0b9` | K | | kept |

## #72 pricer: 7 → 6

`8547d0b7`, `44c8e062`, `1b6a112a`, `fbd10d89`, `d577754d`, `76b873c5`: K. `ef5b40a7`: D (empty: it only added
includes to `rcspp.hpp`; the example's umbrella header comes whole with `348d3e44`).

## #76 highs: 17 → 15

| commit | code | content | message |
|---|---|---|---|
| `24ad74c6` | R | root: `USE_COLUMN_GENERATION_EXAMPLE_HIGHS` option (in the error of `aac64776` too); the fetch block in `X/CMakeLists.txt` with `EXCLUDE_FROM_ALL` (`77c327d4`), without the hidden-visibility lines (`77a43e27` (b)); `AGENT.md` line; `ci-full.yml` `-DUSE_COLUMN_GENERATION_EXAMPLE_HIGHS=ON` | `build(column_generation): HiGHS for the example, installed or fetched`; body: option name, "not installed" |
| `bfb07157` | R | `X/.../highs/CMakeLists.txt` final without visibility (`rcspp_column_generation_highs`); its subdirectory; `tests-column-generation-highs`; no root or `tests/cpp` change | kept; body: library and option names |
| `1c22697d` `f68de74b` `0cd09190` `837207b0` `e932a571` `bcb24aa2` `a5c074a9` `205c64d4` `135fbc0d` `ff577ab6` `798d9021` | K | | kept |
| `3934038c` `2684e8d5` | D | empty after pass A | — |
| `d2d306e6` `fce956b3` | K | | kept |

## #77 python: 10 → 13

| commit | code | content | message |
|---|---|---|---|
| new, first | N | `[tool.pyright]` of `221af3f0`, as it is there (`D-156`) | `style(python): strict pyright settings for the package and its tests` |
| `96819662` | K0 | `active_call.hpp` stays in core | kept |
| `90af6ac9` | R | `rcspp.cpp`: one declaration and one call under `#ifdef RCSPP_USE_COLUMN_GENERATION_EXAMPLE`; `python/CMakeLists.txt`: `include(X/bindings/bindings.cmake)` under the option; new `bindings.cmake` (core part), new `cpp_column_generation.cpp` (submodule `cpp_column_generation`, calls `init_column_generation`); `pyproject.toml`: the example's `extraPaths` and `stubPath`; skip blocks (pass B) | `feat(column_generation): the example's Python bindings: types, master and loop`; body: `rcspp._core.cpp_column_generation`, package `cpp_column_generation` |
| `9b33e3d2` `64ace93c` `37449515` | K | | kept |
| `d311287a` | K0 | `graph.py`'s helpers (#88 needs them) | kept |
| `291ce063` | R | `graph_impl.hpp`: the `#include "graph_pricer_factory.hpp"` and the `register_graph_pricer<...>(rg)` call, both `#ifdef`'d, no NOLINT; new `X/bindings/graph_pricer_factory.hpp` (`make_graph_pricer`, `register_graph_pricer`, the `graph_impl.hpp` names it uses; main's `ResourceGraphType`… names) | kept; body: "`cpp_column_generation.GraphPricer`", the factory's place |
| `0d2ae1f3` | R | `bindings.cmake` HiGHS part (no `--exclude-libs` yet); `cpp_column_generation.cpp` calls `init_column_generation_highs`; no `python/CMakeLists.txt` or `rcspp.cpp` change | kept; body: option and module names |
| `147e24bc` `2f7b0577` | K | | kept |
| new | N | `325bba34`'s `test_column_generation_interrupt.py` (name per `Q-57`) | `test(column_generation): Ctrl-C stops the example's pricer and loop` |
| new, last | N | `ci-full.yml`: the example's Python test step (`PYTHONPATH=build/python`) | `ci: run the column generation example's Python tests` |

## #78 python-masters: 7 → 8

| commit | code | content | message |
|---|---|---|---|
| `286f412c` | K | moved; `highspy` in `tests/python/requirements.txt` stays (#88 needs it) | kept |
| `5faf74b7` | K | | kept |
| `77a43e27` | S | (a) `pyproject.toml` extras `highspy`, `gurobipy` only; the wheel's HiGHS, `FETCHCONTENT_TRY_FIND_PACKAGE_MODE`, the `test-command` import and `install.components` go (`Q-59`) | (a) `build(python): extras for the master solvers over a user's model` |
| | | (b) hidden visibility of the fetched HiGHS and of `rcspp_column_generation_highs`, `--exclude-libs,ALL` in `bindings.cmake`, the test that the module exports no `Highs::run` | (b) `build(column_generation): the example's HiGHS hidden from highspy's` |
| `435d720d` | R | the loop script to `X/python/` (pass A); `examples/README.md`: a pointer to `X` instead of the `pip install rcspp[highspy]` paragraph | `docs(column_generation): a column generation loop written in Python`; body: run with the example built |
| `e4a92fa3` | K0 | numpy (#88 needs it) | kept |
| `297c263d` `5e58755b` | K | | kept |

## #80 packaging: 5 → 4

| commit | code | content | message |
|---|---|---|---|
| `2e0bdb37` | R | as it is, but `python/CMakeLists.txt`'s comment loses the fetched-HiGHS sentence; with `Q-59`: `install.components = ["python"]` and its comment here | kept (`Fixes #79` stays); with `Q-59` one body line |
| `7e65fd28` | D | after A nothing of it is left but exclusions of folders that never sit under `cpp/rcspp/` | — |
| `348d3e44` | R | only `X/cpp/rcspp/column_generation.hpp` (prototype's text); the option, objects, exclusions, conditional bindings and skips exist from #61/#77; no static library (`D-157`) | `feat(column_generation): one header for the example's whole layer` |
| `affc5da8` | D | the `D-151` names are used from the first commit | — |
| `6c79e917` | R | `X/CMakeLists.txt` standalone part (`find_package(rcspp)`, the two solver options, `USE_TESTS`, the module path); the Windows DLL copy in `X/tests/cpp`; root `install_and_find_package` passes `EXAMPLE_*`; `check_install.cmake` builds and tests the example (no `COMPONENTS`); no install rule, component or `rcsppConfig.cmake.in` change | `build(column_generation): the example builds on its own over an installed rcspp` |
| new, last | N | `X/README.md` (`D-150`: options, in-tree and standalone builds, `cpp_column_generation` with `PYTHONPATH`, tests, the loop script); `examples/README.md` section | `docs(column_generation): the example's README` |

## #88 pure-python: 6 → 6

All K under `Q-56` (born as `rcspp.column_generation`). `221af3f0` loses its `[tool.pyright]` hunk (`D-156`); its body
and those of `41351aa8`, `dbe1c12c`: "`rcspp.python_column_generation`" → "`rcspp.column_generation`", "the C++-backed
layer" → "the C++ column generation example (`cpp_column_generation`)". Subjects say "pure-Python": kept, still true.
`D-152`'s cast stays in `ca634a3c`. With `--keep-pure-name` (`Q-56` refused) nothing changes here and the rename is a
new last commit `refactor(python): rcspp.column_generation, the pure-Python layer's name`.

## Totals

90 commits → 89 (19 + 18 + 6 + 15 + 13 + 8 + 4 + 6) with `Q-56` accepted: 7 dropped (`dad77827`, `5632dced`,
`ef5b40a7`, `3934038c`, `2684e8d5`, `7e65fd28`, `affc5da8`), 5 new, 1 split in two, 13 rewritten plus `221af3f0`'s
hunk, the rest K or K0. With `Q-58` refused, the old-flag error goes into the new #61 commit (no count change).
