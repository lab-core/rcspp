# fix(build): cmake --install installs no rcspp headers

<!-- markdownlint-disable MD013 -->

## Summary

`cmake --install` installs the rcspp library and its CMake package (`lib/cmake/rcspp`), but none of its headers. A project that finds the installed library with `find_package(rcspp)` and links `rcspp::rcspp` cannot compile against it. Projects that build rcspp themselves (`add_subdirectory`, `FetchContent`) are not affected: they use the source headers.

The cause is in `cpp/rcspp/generate_umbrella_include.cmake`. Its last step should copy every header into `build/include` (`BUILD_INCLUDE_DIR`, passed as `REPO_BUILD_DIR`), which the top-level `install(DIRECTORY ${BUILD_INCLUDE_DIR}/ ...)` then installs. But it computes the destination from `REPO_REL_DIR`, the source folder:

```cmake
set(dest "${REPO_REL_DIR}/${rel_path}")
```

so each header is copied onto itself, `build/include` stays empty, and the install copies nothing.

The same install also puts files there that are not rcspp's:

- GoogleTest's libraries, headers and CMake package, when `USE_TESTS` is on (GoogleTest is fetched and keeps its own install rules).
- The Python package (`rcspp/*.py`, the stubs and the `_core` module) at the root of the prefix, when `USE_PYTHON` is on: the install rules of `python/CMakeLists.txt` are meant for the wheel but also run in a plain install.

## How to reproduce

```bash
cmake -S . -B build -DUSE_TESTS=ON -DUSE_PYTHON=ON
cmake --build build --target rcspp
cmake --install build --prefix install
find install -path '*rcspp*' -name '*.hpp'   # nothing
```

`build/include` is empty after the build.

## Proposed change

- In `generate_umbrella_include.cmake`, copy to `${REPO_BUILD_DIR}/${rel_path}`. Or drop the copy and install the headers from the source folder (`install(DIRECTORY cpp/rcspp/ DESTINATION include/rcspp FILES_MATCHING PATTERN "*.hpp")`), plus the generated umbrella header.
- Keep third-party install rules out of rcspp's install: `INSTALL_GTEST OFF` before fetching GoogleTest.
- Run the Python install rules only for the wheel build (`if(SKBUILD)`), so that a plain install stays C++ only.
- Add a check that installs into a temporary prefix and builds a small project against it with `find_package(rcspp)`, so that the install cannot break silently again.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
