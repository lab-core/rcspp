# Root CMakeLists.txt for rcspp

cmake_minimum_required(VERSION 3.26)

project(rcspp-lib LANGUAGES CXX)

include(GNUInstallDirs)

option(CXX "enable C++ compilation" ON)
option(USE_VRP "Enable vrp testing" OFF)
option(USE_PYTHON "Enable Python bindings via pybind11" OFF)
option(USE_TESTS "Enable tests for RCSPP" ON)
option(USE_COVERAGE "Instrument C++ targets with gcov-style coverage" OFF)
option(USE_COLUMN_GENERATION_EXAMPLE
       "Build the C++ column generation example (examples/column_generation) and, with USE_PYTHON, its bindings"
       OFF)
option(USE_COLUMN_GENERATION_EXAMPLE_GUROBI
       "Build the column generation example's Gurobi master solver (needs Gurobi)" OFF)
option(USE_COLUMN_GENERATION_EXAMPLE_HIGHS
       "Build the column generation example's HiGHS master solver (installed, else fetched)" OFF)

enable_testing()

# Find modules (FindGUROBI.cmake) shared by the examples.
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

# Build the core librairy ------------------------------------------------------
add_subdirectory(cpp/rcspp)

# C++ column generation example ------------------------------------------------
# Not part of the library: its C++ layer, master solvers and tests build from its own folder, and
# python/CMakeLists.txt adds its bindings to the Python module.
if(USE_COLUMN_GENERATION_EXAMPLE)
  add_subdirectory(examples/column_generation)
elseif(USE_COLUMN_GENERATION_EXAMPLE_GUROBI OR USE_COLUMN_GENERATION_EXAMPLE_HIGHS)
  message(FATAL_ERROR "USE_COLUMN_GENERATION_EXAMPLE_GUROBI and "
                      "USE_COLUMN_GENERATION_EXAMPLE_HIGHS build master solvers of the column "
                      "generation example: they need USE_COLUMN_GENERATION_EXAMPLE.")
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

# The headers, from the source folder (the umbrella header rcspp.hpp is generated there too).
install(DIRECTORY cpp/rcspp/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rcspp
    FILES_MATCHING PATTERN "*.hpp"
)

# The CMake package, to be used with:
# find_package(rcspp)
# target_link_libraries(app PRIVATE rcspp::rcspp)
include(CMakePackageConfigHelpers)
set(RCSPP_CMAKE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/rcspp)
install(EXPORT rcsppTargets
    FILE rcsppTargets.cmake
    NAMESPACE rcspp::
    DESTINATION ${RCSPP_CMAKE_INSTALL_DIR}
)
configure_package_config_file(cmake/rcsppConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/rcsppConfig.cmake"
    INSTALL_DESTINATION ${RCSPP_CMAKE_INSTALL_DIR}
)
get_target_property(RCSPP_VERSION rcspp VERSION)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/rcsppConfigVersion.cmake"
    VERSION ${RCSPP_VERSION}
    COMPATIBILITY SameMinorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/rcsppConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/rcsppConfigVersion.cmake"
    DESTINATION ${RCSPP_CMAKE_INSTALL_DIR}
)

# A check that the installed package works: install into a folder of the build, then build a small
# program against it with find_package(rcspp). rcspp's own build only: a project that adds rcspp
# with FetchContent does not get it.
if(USE_TESTS AND PROJECT_IS_TOP_LEVEL)
  # With the column generation example on, the check also builds it on its own against the
  # install, as its users would, and runs its tests: the layer only (its master solvers would fetch
  # and build HiGHS a second time), with the googletest already downloaded.
  set(example_check_arguments)
  if(USE_COLUMN_GENERATION_EXAMPLE)
    set(example_configure_arguments
        -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=${FETCHCONTENT_BASE_DIR}/googletest-src
        -DCMAKE_CXX_CLANG_TIDY=)
    list(JOIN example_configure_arguments "|" example_configure_arguments)
    set(example_check_arguments
        -DEXAMPLE_SOURCE_DIR=${PROJECT_SOURCE_DIR}/examples/column_generation
        -DEXAMPLE_BUILD_DIR=${PROJECT_BINARY_DIR}/install-check/example
        "-DEXAMPLE_ARGUMENTS=${example_configure_arguments}")
  endif()
  add_test(NAME install_and_find_package
      COMMAND ${CMAKE_COMMAND}
          -DBUILD_DIR=${PROJECT_BINARY_DIR}
          -DCONFIG=$<CONFIG>
          -DPREFIX=${PROJECT_BINARY_DIR}/install-check/prefix
          -DCONSUMER_SOURCE_DIR=${PROJECT_SOURCE_DIR}/tests/find_package
          -DCONSUMER_BUILD_DIR=${PROJECT_BINARY_DIR}/install-check/build
          -DGENERATOR=${CMAKE_GENERATOR}
          -DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}
          -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
          ${example_check_arguments}
          -P ${PROJECT_SOURCE_DIR}/tests/find_package/check_install.cmake
  )
endif()
endif()
