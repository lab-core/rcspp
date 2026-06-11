# Root CMakeLists.txt for rcspp

cmake_minimum_required(VERSION 3.26)

project(rcspp-lib LANGUAGES CXX)

include(GNUInstallDirs)

option(CXX "enable C++ compilation" ON)
option(USE_VRP "Enable vrp testing" OFF)
option(USE_PYTHON "Enable Python bindings via pybind11" OFF)
option(USE_TESTS "Enable tests for RCSPP" ON)
option(USE_COVERAGE "Instrument C++ targets with gcov-style coverage" OFF)

enable_testing()

# Coverage instrumentation -----------------------------------------------------
# When enabled, every target that compiles cpp/rcspp or python/bindings code links
# this INTERFACE target so it emits .gcno/.gcda. The header-only templates are
# instantiated into BOTH tests-rcspp and _core; gcovr unions the two object trees.
# Force -O0 (keep NDEBUG from the build type) so line attribution is faithful.
if(USE_COVERAGE)
  add_library(rcspp_coverage INTERFACE)
  target_compile_options(rcspp_coverage INTERFACE --coverage -O0 -g)
  target_link_options(rcspp_coverage INTERFACE --coverage)
  # Atomic counter updates avoid races in multi-threaded algorithm code (GCC only;
  # Clang accepts --coverage but not this flag).
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(rcspp_coverage INTERFACE -fprofile-update=atomic)
  endif()
endif()

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
endif()

if (POLICY CMP0148)
  cmake_policy(SET CMP0148 NEW)
endif()

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# /usr/local/include is a cmake-detected "implicit" path on macOS but the arm64
# compiler does not actually search it — remove it so cmake adds it explicitly
# when targets (Gurobi, Boost) declare it in their interface includes.
list(REMOVE_ITEM CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES "/usr/local/include")

# Clang-Tidy & compile_commands ------------------------------------------------
# Skipped under USE_COVERAGE: the coverage gate doesn't need linting (pre-commit
# handles that) and clang-tidy roughly doubles build time on the template headers.
if(NOT SKBUILD AND NOT USE_COVERAGE)
  set(CMAKE_CXX_CLANG_TIDY
      "clang-tidy;-config-file=${CMAKE_SOURCE_DIR}/.clang-tidy"
  )
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set global output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")   # .a, .lib
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")   # .so, .dylib
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")   # executables
set(CMAKE_PDB_OUTPUT_DIRECTORY     "${CMAKE_BINARY_DIR}/bin")   # .pdb (Windows)
set(BUILD_INCLUDE_DIR          "${CMAKE_BINARY_DIR}/include")   # for headers
file(MAKE_DIRECTORY ${BUILD_INCLUDE_DIR})

# Build the core librairy ------------------------------------------------------
add_subdirectory(cpp/rcspp)

# VRP column generation example + benchmarks -----------------------------------
# The VRP application is an example consumer of the rcspp library; it and its
# benchmark drivers live entirely under examples/cpp/.
if(USE_VRP)
  message("Building vrp column generation example")
  add_subdirectory(examples/cpp)
endif()

# Python bindings --------------------------------------------------------------
if(USE_PYTHON)
  message("Building python wrapper")

  set(PYBIND11_FINDPYTHON ON)
  find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)

  find_package(pybind11 CONFIG QUIET)
  if(pybind11_FOUND)
    message(STATUS "Using installed pybind11 package")
  elseif(EXISTS "${CMAKE_SOURCE_DIR}/extern/pybind11/CMakeLists.txt")
    message(STATUS "Using vendored pybind11 submodule")
    add_subdirectory(extern/pybind11)
  else()
    message(FATAL_ERROR "pybind11 was not found. Install it in the Python build environment or initialize extern/pybind11.")
  endif()

  add_subdirectory(python)
endif()

# RCSPP tests --------------------------------------------------
if(USE_TESTS)
  message("Building tests for RCSPP")
  add_subdirectory(tests/cpp)
endif()

# (Optional) If you are seeing -nostartfiles / -nostdlib unexpectedly, check:
#   - CMAKE_EXE_LINKER_FLAGS
#   - Toolchain file overrides
# and remove those flags unless you *really* intend to supply your own CRT startup.

# target_compile_options(rcspp PRIVATE /W4 /WX)  # Re-enable if desired

# To install the library -------------------------------------------------------
# to be used with: cmake --install build --prefix /usr/local (or .)
if(NOT SKBUILD)
install(TARGETS rcspp
    EXPORT rcsppTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${BUILD_INCLUDE_DIR}/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# set cmake file for package
# to be used with:
# find_package(rcspp)
# target_link_libraries(app PRIVATE rcspp::rcspp)
install(EXPORT rcsppTargets
    FILE rcsppTargets.cmake
    NAMESPACE rcspp::
    DESTINATION lib/cmake/rcspp
)
endif()
