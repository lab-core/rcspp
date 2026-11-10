# Separation: Python layer shipped, C++ layer as an example (`D-142`, `D-143`)

Source: the colleagues' meeting of 2026-11-09 (feedback below), the analysis given to the user the same day. Facts
checked on `origin/feature/column-generation-pure-python` (head `82fa4e58`, 2026-11-09); re-check before relying on them.

## 1. The meeting's feedback (user's words, condensed)

- Keep the C++ side, but as an example; its bindings are part of the example. Isolate them as much as possible to
  stress "example". Only the pure-Python side is maintained and part of rcspp; the C++ and bindings column generation
  is an example in the repo, in nothing shipped. Find a clean separation.
- Compare the Python VRP example with the pure-Python layer on speed; the same for the C++ VRP example with the C++
  layer. If equivalent, replace (Python VRP on the pure-Python layer, C++ VRP on the C++ layer). Remove the old
  column generation leftovers, except what the new layer does not have (pools, for instance).

## 2. Analysis (given to the user, 2026-11-09)

- Agreed: one maintained layer; the comparison found no cell where the C++-backed layer is meaningfully faster
  (`F-49`).
- Risk: an unmaintained example rots silently when the engine's API changes. Proposal: built behind a CMake option,
  its tests runnable, out of the coverage gates (`Q-52`).
- The C++ library moves easily: it is already a CMake library of its own with install components (#80).
- The bindings are the hard part (§3): they are compiled into `rcspp._core`; isolated, they become an extension
  module of their own that must take `rcspp._core`'s graph objects. The one real technical unknown: prototype first
  (`Q-50`).
- Bonus: the wheel builds HiGHS in only for the C++ HiGHS master solver; the pure layer uses `highspy` (`Q-51`).
- VRP comparisons are half done: `F-38` (Python example on a par with the C++-backed layer, R201 1745 s against
  1730–1980 s) and `F-49` (pure ≈ C++-backed) make "equivalent" very likely on the Python side.
- Plan text to update: the Goal's "usable from C++ and Python" becomes "Python, with a C++ example to copy"
  (`README.md` Goal; `D-02` superseded by `D-141`/`D-142`).

## 3. What moves (inventory, from the pure branch)

| piece | where now | becomes |
|---|---|---|
| C++ layer | `cpp/rcspp/column_generation/` (+ `gurobi/`, `highs/`), `cpp/rcspp/column_generation.hpp` | `examples/column_generation/cpp/` (`D-150`) |
| CMake options | `CMakeLists.txt:14-17` `USE_COLUMN_GENERATION{,_GUROBI,_HIGHS}`; HiGHS fetch `:114-150`; install rules `:209-232`; components `:264-266`; `cmake/rcsppConfig.cmake.in:4-21` | the example's own CMake over `find_package(rcspp)`; the root keeps none of them |
| bindings | `python/bindings/column_generation.cpp` (1022 lines), `column_generation_highs.cpp` (316), `graph_pricer.hpp` (55), `active_call.hpp` (68), registration in `rcspp.cpp:15-47` | sources under `examples/`, compiled into `_core` behind the example's option (`D-144`) |
| graph pricer hook | `graph_impl.hpp:619` fills `graph_pricer_factories()` (`graph_pricer.hpp:52`), keyed by `PyTypeObject*`, per bound graph type | stays as an inert hook in core, filled only with the option on (`D-144`) |
| interrupt state | `g_active_calls` defined in `graph.cpp:12`, handler `graph.cpp:22,50` | shared for free: same `_core` (`D-144`) |
| C++-backed package | `python/src/rcspp/column_generation/` (`graph_pricer.py`, `master_solvers/`, `params.py`, `registry.py`), `_core/column_generation.pyi` | with the example module, under another name |
| pure layer | `python/src/rcspp/python_column_generation/` | renamed `rcspp.column_generation` (`D-141`) |
| C++ tests | `tests/cpp/test_column_generation*.{cpp,hpp}`, `tests/cpp/highs/`, `tests/cpp/gurobi/` | with the example |
| Python tests | `tests/python/test_column_generation*.py`, `column_generation_fakes.py` | with the example; `test_python_column_generation*.py` renamed |
| loop example | `examples/python/column_generation_loop.py` (C++-backed pieces) | with the C++ example, or rewritten on the pure layer |
| wheel | `pyproject.toml:56` `-DUSE_COLUMN_GENERATION_HIGHS=ON`; extras `:37-40` | HiGHS out of the wheel (`Q-51`); extras stay |
| drivers | `benchmark-results/colgen/drivers.py` (`--loop` cpp, python or purepython) | not shipped; adapt to the new names |

The pure layer imports only `rcspp._core.graph` (`Algorithm`, `AlgorithmParams`, `Solution`…) and `rcspp.graph`; it
calls `graph.update_reduced_costs` (`graph_pricer.py:261`). The stack changed `graph_impl.hpp` (132 lines) and
`cpp/rcspp/CMakeLists.txt`: which of those engine changes the pure layer needs decides the PR shape (`Q-54`).

## 4. Questions (in `questions.md`; all decided 2026-11-10)

| ID | question | outcome |
|---|---|---|
| `Q-50` | The example's bindings as a separate module: pybind11 shares types across modules only with the same internals (pybind11 version, compiler ABI, no `module_local`); else a C-level handle exported by `_core`, or the example binds its own graphs | `D-144`: compiled into `_core` behind an option, sources apart, inert hooks |
| `Q-51` | The wheel without HiGHS (pure layer over `highspy`)? | `D-145`: yes |
| `Q-52` | Is the example built and tested by default (option name, on/off), out of the coverage gates? | `D-146`: off by default, on in CI |
| `Q-53` | Layout and names: `examples/cpp/column_generation/` + `examples/python/...`? Python package name of the C++-backed layer | `D-150`: `examples/column_generation/`, `cpp_column_generation`, C++ names kept |
| `Q-54` | The stack's shape (#61, #69, #72, #76, PRs 5–6, #80, #88) | `D-147`: merged at once, PRs kept, paths rewritten |
| `Q-55` | VRP equivalence rule: `D-139`'s (LP equal, time within max(5 %, 0.25 s)); iterations may differ | `D-148`: yes |

`Q-54` options (superseded by `D-147`):

| option | how | for | against |
|---|---|---|---|
| A (proposed) | shipped first: engine + packaging changes + pure layer onto `main`; then one PR adding only `examples/` with the C++ layer and its bindings | reviewers see what ships; the example clearly apart | rework of 8 branches; the C++ commits rewritten to new paths |
| B | one last "move to examples" PR on top of the stack | cheap, history kept | the library first gets the C++ layer, then loses it |

## 5. VRP comparisons (`D-143`)

| side | old | new | notes |
|---|---|---|---|
| Python | `examples/python/vrp` (python-mip over Gurobi, own loop); `drivers.py example` | the same VRP on the pure layer, gurobipy master; `drivers.py layer --loop purepython` | python-mip leaves the example; `T-1.16` then adds the ng option |
| C++ | `examples/cpp/vrp` (Gurobi C++ master, own loop; `rcspp-vrp-benchmark`, `-large`, `rcspp-ng-cg-comparison`) | the same VRP on the C++ example layer, Gurobi master | kept: per-algorithm timing over one dual sequence, Boost, duals dump, ng size; only the master and loop go |

No pool is used by either VRP example; `rcspp.pricing_pool` and `SolutionPool` stay as they are (`T-2.2`).

## 6. Steps

1. Done 2026-11-10: `D-139`–`D-143` accepted; `D-144`–`D-149` from `Q-50`–`Q-52`, `Q-54`, `Q-55`.
2. `T-1.17`: layout `D-150`, options `D-151`; prototype `D-144`: with the option on, `_core`
   builds with the binding sources taken from `examples/`; off, it has no column generation symbols; a
   `rcspp.graph` graph goes into the example's pricer and Ctrl-C interrupts it; MSVC and Linux. Then check
   which stack changes (`graph_impl.hpp`, packaging) the pure PR needs at its head (`D-147`).
3. `T-1.18` (Python VRP) and `T-1.19` (C++ VRP): can run beside step 2 on the current builds.
4. The last PR(s): rename, move, README and wheel tidied (`python-layer.md` §2).
