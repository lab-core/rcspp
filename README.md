# RCSPP

**RCSPP (Resource Constrained Shortest Path Problem)** is a C++ library
(built as a shared library: DLL on Windows, .so on Linux, .dylib on macOS)
for solving resource-constrained shortest path problems, with optional Python
bindings (via [pybind11]). Moreover, the project includes vehicle routing
examples solved by column generation, in C++ and in Python, to illustrate how to use the library.

---

## Prerequisites

### RCSPP library

- **CMake >= 3.26**
- **C++ compiler** with C++23 support (e.g., Clang 21.1.0, or a compatible compiler)

### Python bindings

- **Python >= 3.11** (for Python bindings and pre-commit hooks)
- **Git** (to have pybind11 as a submodule)
- **[pybind11]** (as a submodule – auto-included when you follow instructions below)

### VRP examples

- **HiGHS** (open source; the C++ example fetches it when it is not installed) or
  **Gurobi >= 12.03** (a licence), for the master's LP

---

## Getting the Source Code

For the C++ library and the VRP examples, clone the RCSPP repository as follows:

```sh
git clone https://github.com/lab-core/rcspp.git
cd rcspp
```

If you plan to use the optional Python bindings, you must also clone the
repository with submodules to ensure pybind11 is present (as a submodule
under `extern/pybind11`):

```sh
git clone --recursive https://github.com/lab-core/rcspp.git
cd rcspp
```

If you already cloned without `--recursive` and now want to enable Python
bindings, run:  

```sh
git submodule update --init --recursive
```

---

## Getting the instances

To download the instances that can be used with the VRP examples, use the
following command:

```sh
git lfs pull
```

---

## Building RCSPP

### Building the C++ library

To compile the library only:

```sh
mkdir build
cd build
cmake ..
cmake --build .
```

### Building the Python bindings

To compile the Python bindings, use the CMake option `USE_PYTHON=ON` (set to `OFF`
by default):

```sh
cmake -DUSE_PYTHON=ON ..
cmake --build .
```

### Building the C++ VRP example

The C++ VRP example runs on the C++ column generation example, with one of its master solvers
(HiGHS, found installed or else fetched, or Gurobi):

```sh
cmake -DUSE_COLUMN_GENERATION_EXAMPLE=ON -DUSE_COLUMN_GENERATION_EXAMPLE_HIGHS=ON ..
cmake --build .
```

`-DUSE_COLUMN_GENERATION_EXAMPLE_GUROBI=ON` adds the Gurobi master. How to run it:
[`examples/cpp/vrp/README.md`](examples/cpp/vrp/README.md). The Python VRP example needs only the
Python package: [`examples/python/vrp/README.md`](examples/python/vrp/README.md).

Note that these options can be combined:

```sh
cmake -DUSE_PYTHON=ON -DUSE_COLUMN_GENERATION_EXAMPLE=ON -DUSE_COLUMN_GENERATION_EXAMPLE_HIGHS=ON ..
cmake --build .
```

---

## Pre-commit and hooks

### Prerequisites for pre-commit

- **Git**
- **Python >= 3.11**
- **Node.js** (Required for `markdownlint-cli` (Markdown formatting))
- **C++ tools**
  - **clang-format** (for formatting C/C++ code)
  - **clang-tidy** (for C++ linting)
  - **cppcheck** (for static analysis)
  - **cpplint** (for C++ style checks)
  > On many systems, these can be installed via your package manager:
  > - Ubuntu:  
  >   `sudo apt-get install clang-format clang-tidy cppcheck cpplint`
  > - macOS (with Homebrew):  
  >   `brew install clang-format clang-tidy cppcheck cpplint`
  > - Windows:  
  >   Provided via [LLVM releases](https://releases.llvm.org/) or
 via [Chocolatey](https://chocolatey.org/).

### Installation

1. **Install pre-commit** (ensure you have Python >= 3.11):

   ```sh
   pip install pre-commit
   ```

2. **Install all hooks from [.pre-commit-config.yaml]** (to ensure hooks
run on every commit):

   ```sh
   pre-commit install --hook-type pre-commit
   ```

3. **[Optional] Update hooks** (to keep hooks up to date):

   ```sh
   pre-commit autoupdate
   ```

4. **[Optional] Run all hooks manually on all files:**

   ```sh
   pre-commit run --all-files
   ```

---

[pybind11]: https://github.com/pybind/pybind11
