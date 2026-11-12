# Fold map: items, PR heads, final state (`fold-map.md`)

`X` = `examples/column_generation/`. Commit codes and contents: `fold-map-commits.md`. §7's old hashes, rebased:
`956c3af3` → `d311287a`, `11fd42d7` → `e4a92fa3`, `60defc4c` → `77a43e27`, `ad06f467` → `286f412c`, `4a5a5589` →
`2e0bdb37`, `8fb5d102` → `348d3e44`.

## 1. Where each item of `separation.md` §7 goes

| item | goes into |
|---|---|
| layer and include paths | pass A, from `35f93188` on (#61) |
| example CMake (in-tree part) | new #61 commit; layer objects `e7402feb`; Gurobi `aac64776` (#69); HiGHS `24ad74c6`, `bfb07157` (#76); visibility `77a43e27` (b) (#78) |
| example CMake (standalone over `find_package(rcspp)`) | `6c79e917` rewritten (#80): needs `2e0bdb37`'s install, so not before #80 |
| hook `python/CMakeLists.txt` (one `include`) | `90af6ac9` (#77) |
| hook `rcspp.cpp` (one declaration, one call) | `90af6ac9` (#77); HiGHS registered inside the example's `cpp_column_generation.cpp` (`0d2ae1f3`) |
| hooks `graph_impl.hpp` (include, `register_graph_pricer` call) and `graph_pricer_factory.hpp` | `291ce063` (#77), the factory with `c63d010e`'s list of the names it uses |
| `bindings.cmake`, `cpp_column_generation.cpp` | `90af6ac9`, `0d2ae1f3` (#77); `--exclude-libs` in `77a43e27` (b) (#78) |
| `active_call.hpp` in core | `96819662` (#77), unchanged |
| moved C++ tests, own gtest CMake (3 executables) | pass A; executables in the new #61 commit, `aac64776`, `bfb07157` |
| moved Python tests, skips, `isort: split` | pass A and B, in the commits adding each file (#77, #78); names `Q-57` |
| `test_column_generation_interrupt.py` (`325bba34`) | new #77 commit |
| C++-backed package as `cpp_column_generation` | pass A (`X/python/cpp_column_generation/`), from `90af6ac9` |
| stub out of `rcspp._core` (`D-153`) | pass A: born at `X/python/typings/rcspp/_core/cpp_column_generation.pyi` (`90af6ac9`); main's stubtest never sees it; `_core/__init__.pyi` reset to main |
| `rcspp/__init__.py` import, `_core/__init__.pyi` line | never written (reset to main) |
| old-flag configure error | `Q-58`: dropped (else in the new #61 commit) |
| #80's install and components | kept: `2e0bdb37`'s plain install; gone: `6c79e917`'s install rules, components, `rcsppConfig.cmake.in` components, `tests/find_package` consumers |
| `FindGUROBI.cmake` in `cmake/` | `541286ad` (#69), kept |
| wheel without HiGHS (`D-145`) | `77a43e27` split (#78): extras stay; the `=OFF` pin in the new #61 commit; `install.components`: `Q-59` |
| CI on `ci-full.yml` only (`D-146`) | flags in the new #61 commit and `24ad74c6`; Python step new #77 commit; `ci.yml` untouched |
| lint and pre-commit extensions | new #61 commit (hooks); pyright paths `90af6ac9`; lint fixes pass B |
| `rcspp.column_generation` rename and references | `Q-56`: pass A over #88; residue `221af3f0`, `ca634a3c`; bodies of `221af3f0`, `41351aa8`, `dbe1c12c` |
| `docs/installation.md` | `6c79e917`'s components paragraph never written; `2e0bdb37`'s install paragraph only |
| `examples/README.md` | `435d720d` (pointer) and the new #80 README commit |
| `AGENT.md` | the option lines with `D-151` names in the new #61 commit, `aac64776`, `24ad74c6` |
| empty `COMPONENTS` machinery | never written (`6c79e917` rewritten) |
| `generate_umbrella_include.cmake`'s `EXCLUDED` | never written (`7e65fd28` dropped, `348d3e44` rewritten) |
| unused static `rcspp_column_generation` | not written (`D-157`) |
| `drivers.py` | not in the stack: one commit on `feature/column-generation` after the push (`--loop cpp`/`python` → `cpp_column_generation` and `X/python/column_generation_loop.py` on `PYTHONPATH`; `purepython` → `rcspp.column_generation`) |
| installed-HiGHS path (`FIND_PACKAGE_ARGS`), clang-tidy on the mid-file include | checks in step 3 at #76 and #77: one configure with `-Dhighs_DIR`, one pre-commit clang-tidy run on `graph_impl.hpp` |

## 2. Each PR head (`D-158`)

Off: default options. On: `USE_COLUMN_GENERATION_EXAMPLE{,_GUROBI,_HIGHS}=ON` as the PR allows, `USE_PYTHON=ON`.
"main" = the count measured once at `6367ee56` with the same build. The moved tests are the same tests, so with the
option on the totals must equal `separation.md` §8 (C++ ctest: core plus the example's executables; Python:
`tests/python` plus `X/tests/python`), plus the interrupt tests from #77 on POSIX.

| PR | head builds, off → on | tests: off / on | description changes |
|---|---|---|---|
| #61 | core only → + example headers, objects, `tests-column-generation` | C++ main / §8 652; Python main | the layer is a C++ example in `X`, not part of the library, built with `USE_COLUMN_GENERATION_EXAMPLE`; paths; how to build and run its tests |
| #69 | + `rcspp_column_generation_gurobi`, its tests | C++ main / 687 | option `_GUROBI`; `FindGUROBI.cmake` shared (stays for the VRP example) |
| #72 | + pricing headers, tests | C++ main / 706 | paths |
| #76 | + HiGHS fetched or found, `rcspp_column_generation_highs`, tests | C++ main / 730 | option `_HIGHS`; HiGHS not installed |
| #77 | + bindings in `_core` (`rcspp._core.cpp_column_generation`), package on `PYTHONPATH` | Python main, example skips / main + 57; off: no column generation symbol in `_core` (`F-50`), stubtest passes | bindings in the example, compiled into `_core` behind the option, inert hooks; `cpp_column_generation`; no `rcspp.column_generation` |
| #78 | + Python master solvers, loop script | Python main / main + 116 (§8 429) | extras only, the wheel has no HiGHS; the loop script's place |
| #80 | + plain install; on: `install_and_find_package` also builds and tests `X` alone | C++ main + 1 / 731 | install fixed (#79); no components; the example's standalone build and README |
| #88 | pure layer always | Python main + 274 / + 116 example (§8 703) ; pyright strict clean | `rcspp.column_generation` (`Q-56`); the only shipped column generation; base is #80 |

Everyone: Linux gcc 13 on at #77, #80, #88 (`fold-map.md` §3). Each PR description also says that the stack is
merged at once and that its C++ part is an example kept unmaintained-but-tested (no plan IDs, `D-55`).

## 3. Expected final state: #88's head against the prototype (`325bba34`)

`git diff prototype/column-generation-example <new #88 head>` must show only these:

| # | difference | why |
|---|---|---|
| 1 | everything `main` brought since `129b15e0` (#56–#59), in every file | the rebase (`separation.md` §8) |
| 2 | main's names in moved files: `AlgorithmStatus::Complete`…, `CriticalResourceType`, `ResourceGraphType`/`CompositionType`/`LabelContainerType` in `graph_pricer_factory.hpp`, `bucket_width=`, `number_of_arcs()` | §8's renames |
| 3 | `_core/graph.pyi` is main's generated stub; `master_problem.py` has `D-152`'s `Protocol` cast | `D-152` |
| 4 | `.flake8` A005 ignores for `types.py` and `statistics.py` | main's flake8-builtins (§8) |
| 5 | `python/src/rcspp/python_column_generation/` → `python/src/rcspp/column_generation/`; its tests and fakes renamed; docstrings and `master_problem.py:146` reworded | `Q-56` |
| 6 | the example's Python tests and fakes renamed | `Q-57` |
| 7 | `python/src/rcspp/__init__.py` equals main's (the prototype kept `348d3e44`'s `from . import _core`) | reset |
| 8 | no old-flag error in the root `CMakeLists.txt` | `Q-58` |
| 9 | `tests/find_package/{CMakeLists.txt,check_install.cmake}`: `2e0bdb37`'s plus the `EXAMPLE_*` part, no `COMPONENTS` | §7 left item |
| 10 | `generate_umbrella_include.cmake`, `cpp/rcspp/CMakeLists.txt`: main's plus `2e0bdb37`'s, no `EXCLUDED` | §7 left item |
| 11 | `docs/installation.md` without the components paragraph | §7 left item |
| 12 | `examples/README.md` points at `X`; `X/README.md` exists | `D-150`, §7 left item |
| 13 | `AGENT.md` names the `D-151` options | §7 left item |
| 14 | no static `rcspp_column_generation` or `rcspp::column_generation` alias in `X/cpp/.../CMakeLists.txt` | `D-157` |
| 15 | `pyproject.toml`: `install.components` comment reworded (`Q-59`); else equal | `Q-59` |
| 16 | `.github/workflows/ci.yml`, `pre-commit.yml` are main's (stubtest step, naming check), which the prototype predates | main |

Anything else is a rewrite error. The same diff restricted to `X` must be empty except rows 2, 6, 12, 14.
