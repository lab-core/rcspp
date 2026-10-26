# The numbers of the theory pages

This package recomputes every number stated in the theory pages ([`docs/theory/`](../../../docs/theory/)) by
running `rcspp`, one module per page. The master LPs are solved with HiGHS, through SciPy.

## 1. Set up an environment (once)

The scripts need the `rcspp` Python package built from **this checkout**: they use features that a published
package may not have yet, such as the presets and the bidirectional search. Building it compiles the C++ library, so
the prerequisites of the main [README](../../../README.md#prerequisites) apply: Python 3.11 or newer, CMake 3.26 or
newer, a C++23 compiler, and the `pybind11` submodule (`git submodule update --init --recursive`).

From the repository root:

Linux, macOS:

```sh
python3 -m venv .venv
source .venv/bin/activate
pip install . scipy
```

Windows (PowerShell):

```powershell
python -m venv .venv
.venv\Scripts\Activate.ps1
pip install . scipy
```

`pip install .` compiles the library and can take several minutes. Run it again after changing the C++ code.
`.venv` is ignored by git. If PowerShell refuses to run `Activate.ps1`, allow local scripts once with
`Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`.

`ModuleNotFoundError: No module named 'rcspp'` means the Python running the scripts is not the one `rcspp` was
installed into: activate the environment first (the `source` or `Activate.ps1` line).

## 2. Print the numbers

From `examples/python`, with the environment active:

```sh
cd examples/python
python -m theory            # every page
python -m theory --page 3   # one page
```

Each page's module (`page1.py` to `page6.py`) has a `numbers()` function that returns the values as a dictionary,
and a `report()` function that formats them. `toy.py` holds what the pages share: the toy instance, the pricing
graphs, the master problem and the column generation loop.

A few lines starting with `[WARN ]` are logged by the library itself. They are expected: page 4 and page 5 discuss
the settings that trigger them.

## 3. Run the tests

`tests/python/test_theory_numbers.py` checks these numbers against the values the pages state, and
`tests/cpp/test_theory_docs.hpp` checks the few that only C++ can see.

The Python tests, like the repository's other Python tests, import `rcspp` from `python/src`, which loads the compiled
module from a CMake build folder (`build/`, `out/` or `cmake-build-*`). So they need the Python bindings built with
CMake, as CI does, from the repository root:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DUSE_PYTHON=ON -DUSE_TESTS=ON
cmake --build build --config Release
pip install pytest -r tests/python/requirements.txt
cd tests/python
pytest test_theory_numbers.py
```

The C++ tests are part of the test executable that the same build makes: run them with
`ctest --test-dir build -C Release -R TheoryDocs`.

If the tests fail with an `ImportError` from `rcspp._core`, the build folder holds a compiled module older than the
sources: build it again.
