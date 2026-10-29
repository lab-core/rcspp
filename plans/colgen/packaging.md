# Packaging and solver routes: study notes

Study notes of 2026-10-29 (discussion with the user and a colleague, after 6.3). **Nothing here is decided** except
`D-102`. Open points: `Q-37`–`Q-40`. Facts: `F-27`, `F-32`–`F-35`.

## 1. Master solvers today (after PR 6.3)

| solver | LP solved in | from a pip install | what the Python class does |
|---|---|---|---|
| HiGHS | C++ `HighsMasterSolver` (in the wheel) | yes | `HighspyMasterSolver`: copies the user's highspy model into arrays, `write_back` (`D-101`) |
| Gurobi, C++ user | C++ `GurobiMasterSolver` (`USE_GUROBI`) | — | — |
| Gurobi, Python user | Python `GurobipyMasterSolver`, a port of the C++ class (`D-86`) | yes | everything: drives gurobipy each iteration |

- Only Gurobi has a real duplicate. `GurobiMasterSolver` is not bound to Python at all, even in a source build.
- `OwnedHighsMasterSolver` (bindings) is about ownership: the C++ class holds a `Highs&`, a Python user has no C++
  `Highs` to give, so the binding owns one and forwards every call. Python sees it as `HighsMasterSolver`.
- The Python VRP example (`P-09`) does not use the layer: its own loop over python-mip, rcspp for pricing only. Its
  port is part of `T-1.6`; 6.4 is a different, new example (`D-90`).

## 2. Why the C++ Gurobi solver is not in the wheel

- Two problems were mixed. (1) A Python solver object cannot be handed to C++ (each package carries its own solver
  library, `F-16`): solved by copying the model's data, as for HiGHS. (2) **For Gurobi, the wheel has no C++ solver to
  copy into**: that is the blocker, not the copy.
- Building `GurobiMasterSolver` needs Gurobi's headers and C++ API library: not in gurobipy (`F-27`, `F-34`), not on
  the CI machines. Shipping Gurobi's closed library in our wheel would be redistribution (Gurobi sells distribution
  licenses; their terms not read). gurobipy can ship it: it is Gurobi's own package.
- Getting the library onto the user's machine is not the problem: the `[gurobipy]` extra (6.3) already brings it.
- gurobipy always loads its own copy, even with an install on `PATH` (`F-34`); pointing it at a manual install is not
  supported (hack: delete its copy). The license is a separate file read by whichever copy runs.

## 3. Gurobi routes (`T-1.11`)

| route | our C++ Gurobi code | library used | who | version dependence |
|---|---|---|---|---|
| A, today | none (Python port) | gurobipy's | anyone with gurobipy | gurobipy's Python API, a range |
| B1 | `GurobiMasterSolver`, built locally, bound | the install's **and** gurobipy's (two copies) | source builds | exact: the C++ API, build version, compiler, gurobipy's version |
| B2 | a C API master solver in the wheel, loading the library at run time | the one already loaded | anyone with gurobipy | the C API calls used, a range |

- Every route depends on some Gurobi API; none supports every version forever.
- B1: two copies of `gurobi130.dll` in one process, same version needed; Windows may reuse a loaded library of the same
  name (to check).
- **B2 mechanism:** compiling needs only the C function declarations (e.g. `int GRBoptimize(GRBmodel*)`); no link to
  Gurobi. At run time: open the library (`LoadLibrary`/`dlopen`), look up each function by name (`GetProcAddress`/
  `dlsym`), call through the pointers. The C++ API cannot be loaded this way (a separate compiled library gurobipy does
  not ship, mangled names per compiler), so B2 means the C API: a second Gurobi master solver, or
  `GurobiMasterSolver` rewritten on the C API to serve C++ and Python users (`Q-26`'s choice revisited).
- **Which library:** the one already loaded (gurobipy's, from Python: one copy, right version, nothing to configure);
  else `GUROBI_HOME` (a C++ program). Loading the install's copy from Python would bring back B1's two copies.
- **Override and diagnostics (ideas):** an environment variable (`RCSPP_GUROBI_LIBRARY=<path>`) or an argument to force
  a library; `python -m rcspp gurobi-info` printing the library, version and adapter that would be used.
- **A setup command** (`python -m rcspp --setup-gurobi 130`) was considered: it could only record which library to use
  (it cannot recompile the wheel's code, and declarations matter only when compiling). Automatic detection does the
  same without a step and cannot pick a wrong library; an override covers odd setups.
- **Adapters by version range (user's idea):** one master solver class written against a small table of the C
  functions it calls; adapters fill the table for a version range ("12.0–13.x"), a new one reusing the previous and
  replacing what changed. At load: `GRBversion` → the adapter for that version → check every function was found →
  else a clear error ("Gurobi 14.0 not supported yet; supported: 12.0–13.x"). The function check cannot catch a changed
  signature with the same name: the version rule and tests do. One adapter now (13.x, testable here), others when
  Gurobi changes something used. Assumption to verify: Gurobi changes the C API only in major/minor releases.
- **Prior art (as far as known, to verify):** OR-Tools loads Gurobi at run time from a list of library names
  (`gurobi130`, `gurobi120`…) and looks functions up by name; Gurobi.jl keeps generated C declarations per major
  version.
- **Run-time cost of B2:** opening the library, looking up ~30 functions and reading the version happen once per
  process (microseconds; ms if the library is not loaded yet); each call is one call through a pointer, as any call
  into a DLL on Windows. Expected faster than A (no Python per iteration, 1–15 % of an iteration, `F-17`) and than the
  C++ API (`GRBaddvars` takes the CSC batch in one call, `Q-26`).
- **B2 costs:** the C API rewrite, the adapters, tests per supported version, and the license question below.
- **To check:** may we write or include the C declarations (`gurobi_c.h`, or our own ~20–30); the C API's stability
  across versions (diff `gurobi_c.h` of a few versions; only 13.0.1 here).

## 4. Python packages (`Q-37`)

User and colleague: HiGHS, maybe Gurobi, maybe all of column generation in a separate package, to study.

| option | how a user chooses | cost |
|---|---|---|
| 1. one wheel, everything (today) | nothing to choose | ~5 MB unpacked (`F-35`), +1 min per wheel on CI |
| 2. one wheel, several modules (`_core`, `_highs`, maybe `_column_generation`) | nothing to choose | built together, so compatible; prepares a later split invisibly to users |
| 3. separate packages sharing C++ types (`rcspp`, `rcspp-highs`…), extras as aliases (`rcspp[highs]` installs `rcspp-highs`) | `pip install rcspp[highs]` | exact version pin between packages; CI matrix doubled; type identity across modules fragile with hidden symbols (macOS); duplicated globals (logger level, pricer registry); graph templates compiled twice |
| 4. separate packages over a stable C boundary (a master solver as a C function table, e.g. in a `PyCapsule`) | as 3 | design work; versions independent, compiler-free; the B2 adapter idea again |
| 5. source build with options | `pip install rcspp --no-binary rcspp --config-settings=cmake.define.USE_HIGHS=OFF` | works today; compiler needed |
| 6. conda-forge, linking conda's shared HiGHS | conda variants | one HiGHS shared with conda's highspy; another channel to maintain |

- Column generation split from Python is the most costly: the graph pricer is built in the graph bindings (each graph
  type registers its factory, `python/bindings/graph_impl.hpp`).
- Gurobi under B2 needs no split: a few hundred KB inert without gurobipy; "none or Gurobi" is the `[gurobipy]` extra.
- With 3, imports can stay `rcspp.column_generation` (a second package can install its module inside `rcspp/`).
- **Impossible, or not what it looks like:**
  - `pip install rcspp[column-generation]` adding or removing code inside rcspp's wheel: extras only install other
    packages (the syntax works only as an alias, option 3);
  - `pip install rcspp --without-highs`, or build options for a prebuilt wheel: a binary wheel is fixed;
  - an `rcspp-highs` working with any rcspp version while sharing C++ types (only option 4 frees versions);
  - linking our C++ to highspy's HiGHS: highspy ships no headers nor link library and exports no symbols;
  - Gurobi's library inside rcspp or an `rcspp-gurobi` (redistribution);
  - downloading HiGHS at first import (network at import, nothing to compile with, security).
- Timing: nothing is released; names (packages, import paths, C++ targets) become promises at the first release with
  column generation. Cheap now if a split seems likely: option 2, and keep `rcspp.column_generation` as the import path.
- Earlier recommendation (not a decision): keep one wheel; option 2 only if a split is likely.

## 5. C++ packaging (`D-102`, `T-1.12`)

- Today (`F-33`): column generation has no flag (in `rcspp`, in `rcspp.hpp`); the master solvers have no install
  rules; `cmake --install` installs no rcspp header at all (`I-11`, also on `main`).
- `USE_COLUMN_GENERATION` (default ON: no external dependency): a target `rcspp_column_generation`
  (`rcspp::column_generation`) built from `cpp/rcspp/column_generation/`, left out of `rcspp`; headers stay in place;
  `rcspp.hpp` stops including them, `rcspp/column_generation.hpp` gathers them. Follow the flag: the C++ test, the
  column generation bindings, `rcspp.cpp`'s init, the graph-pricer factory (`graph_impl.hpp`, behind a define), the
  Python package folder, the VRP example once ported (`T-1.5`).
- The master solver options depend on it (CMake error otherwise, as `USE_GUROBI`'s missing-Gurobi error), and are
  renamed `USE_COLUMN_GENERATION_GUROBI`, `USE_COLUMN_GENERATION_HIGHS` (define
  `RCSPP_USE_COLUMN_GENERATION_HIGHS`): the old names read as "use HiGHS". ~25 lines in 13 files (`CMakeLists.txt`,
  backend and test `CMakeLists.txt`, `python/CMakeLists.txt`, bindings, Python comments, `pyproject.toml`,
  `AGENT.md`); no CI workflow uses them; PR texts #69, #76 to update; local caches to reconfigure.
- No block for users who need Gurobi or HiGHS elsewhere: the options build only rcspp's master solvers, nothing else in
  rcspp uses a solver; such users link the solver themselves. With the option off, rcspp holds no HiGHS to clash with
  theirs.
- Components: `find_package(rcspp COMPONENTS column_generation column_generation_highs)`; targets maybe
  `rcspp::column_generation_highs`, `rcspp::column_generation_gurobi`; folder `cpp/rcspp_backends/` kept or moved (to
  decide in `T-1.12`).

## 6. Measured (Windows, Python 3.14, `F-35`)

- Importing `_core` takes ~310 ms with or without HiGHS (pybind11 registering the graph types); HiGHS adds nothing
  measurable. gurobipy installed adds ~16 ms to `import rcspp.column_generation` (6.2 imports it eagerly, `Q-39`).
- No run-time cost: HiGHS code runs only when a `HighsMasterSolver` is built.
