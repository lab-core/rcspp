# Root CMakeLists.txt for rcspp

cmake_minimum_required(VERSION 3.26)

project(rcspp-lib LANGUAGES CXX)

include(GNUInstallDirs)

option(CXX "enable C++ compilation" ON)
option(USE_VRP "Enable vrp testing" OFF)
option(USE_PYTHON "Enable Python bindings via pybind11" OFF)
option(USE_TESTS "Enable tests for RCSPP" ON)
option(USE_COVERAGE "Instrument C++ targets with gcov-style coverage" OFF)
option(USE_GUROBI "Enable the Gurobi master solver of column generation (needs Gurobi)" OFF)
option(USE_HIGHS "Enable the HiGHS master solver of column generation (installed, else fetched)" OFF)

enable_testing()

# Find modules (FindGUROBI.cmake) shared by the library and the examples.
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Coverage instrumentation -----------------------------------------------------
# When enabled, every target that compiles cpp/rcspp or python/bindings code links
# this INTERFACE target so it emits .gcno/.gcda. The header-only templates are
# instantiated into BOTH tests-rcspp and _core; gcovr unions the two object trees.
# Force -O0 (keep NDEBUG from the build type) so line attribution is faithful. -g0 because gcov
# does not need debug info, and with -g MinGW overflows the COFF string table on the test binary.
if(USE_COVERAGE)
  add_library(rcspp_coverage INTERFACE)
  target_compile_options(rcspp_coverage INTERFACE --coverage -O0 -g0)
  target_link_options(rcspp_coverage INTERFACE --coverage)
  # Atomic counter updates avoid races in multi-threaded algorithm code (GCC only;
  # Clang accepts --coverage but not this flag).
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(rcspp_coverage INTERFACE -fprofile-update=atomic)
  endif()
  # Coverage instrumentation can exceed the default COFF section limit; MinGW's equivalent of
  # MSVC's /bigobj.
  if(MINGW)
    target_compile_options(rcspp_coverage INTERFACE -Wa,-mbig-obj)
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
# A -DCMAKE_CXX_CLANG_TIDY=... on the command line wins; the CI workflows pass an empty one to
# turn it off. A plain set() would shadow that cache entry, so only set it when there is none.
if(NOT SKBUILD AND NOT USE_COVERAGE AND NOT DEFINED CACHE{CMAKE_CXX_CLANG_TIDY})
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

# Gurobi master solver ----------------------------------------------------------
# Asked for explicitly, so a missing Gurobi or a missing C++ library is an error, not a skip.
if(USE_GUROBI)
  set(GUROBI_REQUIRE_CXX ON)
  find_package(GUROBI REQUIRED)
  if(NOT TARGET GUROBI::gurobi_cxx)
    message(FATAL_ERROR
            "USE_GUROBI needs the Gurobi C++ library (gurobi_c++); pass -DGUROBI_CXX_LIBRARY=<path>.")
  endif()
  add_subdirectory(cpp/rcspp_backends/gurobi)
endif()

# HiGHS master solver -----------------------------------------------------------
# An installed HiGHS when CMake finds one (pass -Dhighs_DIR=<dir> to point at it), else HiGHS
# v1.15.1, fetched and built with the project. Either way the target is highs::highs.
if(USE_HIGHS)
  include(FetchContent)
  FetchContent_Declare(highs
    URL https://github.com/ERGO-Code/HiGHS/archive/refs/tags/v1.15.1.tar.gz
    URL_HASH SHA256=a840d269dff2fafb371dd247df13ad5e026d7ce3b35ad3dc1eedd59bf0c2fb16
    FIND_PACKAGE_ARGS CONFIG
  )
  # Settings for a fetched HiGHS only: the library alone, static and position independent (it
  # ends up in the Python module too), and not linted as our code is.
  block(PROPAGATE highs_FOUND highs_VERSION highs_DIR)
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(BUILD_TESTING OFF)
    set(BUILD_CXX_EXE OFF)
    set(BUILD_EXAMPLES OFF)
    set(BUILD_SHARED_EXTRAS_LIB OFF)
    set(ZLIB OFF)
    set(CMAKE_CXX_CLANG_TIDY "")
    FetchContent_MakeAvailable(highs)
  endblock()
  if(NOT TARGET highs::highs)
    message(FATAL_ERROR "USE_HIGHS: the HiGHS found or fetched has no highs::highs target.")
  endif()
  if(highs_FOUND)
    message(STATUS "HiGHS: installed, ${highs_VERSION} (${highs_DIR})")
  else()
    message(STATUS "HiGHS: fetched, v1.15.1")
  endif()
  add_subdirectory(cpp/rcspp_backends/highs)
endif()

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
