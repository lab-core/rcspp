# The C++ column generation example: a column generation layer over rcspp (cpp/), its master
# solvers over Gurobi and HiGHS, its Python bindings (bindings/) and its tests. Not part of the
# library: nothing here is installed.
#
# Two ways to build it:
#   - from rcspp's root with -DUSE_COLUMN_GENERATION_EXAMPLE=ON: the layer builds over rcspp's own
#     targets, and python/CMakeLists.txt adds the bindings to rcspp's Python module;
#   - on its own over an installed rcspp (cmake -S examples/column_generation, with
#     CMAKE_PREFIX_PATH at the install): the C++ layer, its master solvers and its tests only.

cmake_minimum_required(VERSION 3.26)

project(rcspp-column-generation-example LANGUAGES CXX)

# MARK: Standalone build

if(NOT TARGET rcspp::rcspp)
  option(USE_COLUMN_GENERATION_EXAMPLE_GUROBI "Build the Gurobi master solver (needs Gurobi)" OFF)
  option(USE_COLUMN_GENERATION_EXAMPLE_HIGHS
         "Build the HiGHS master solver (installed, else fetched)" OFF)
  option(USE_TESTS "Build the tests" ON)
  set(CMAKE_CXX_STANDARD 23)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  enable_testing()
  find_package(rcspp REQUIRED)
  # FindGUROBI.cmake, from rcspp's source folder.
  list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
endif()

# The core library the example's targets link: in rcspp's build, the core's objects, as the Python
# module links them (so that it does not depend on rcspp's shared library); else the installed
# rcspp.
if(TARGET rcspp_objects)
  set(RCSPP_EXAMPLE_CORE rcspp_objects)
else()
  set(RCSPP_EXAMPLE_CORE rcspp::rcspp)
endif()

# MARK: Headers

# "rcspp/column_generation/...": the example's cpp/ folder; "rcspp/...": the core's headers.
add_library(rcspp_column_generation_headers INTERFACE)
target_include_directories(rcspp_column_generation_headers INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/cpp")
if(TARGET rcspp_objects)
  target_include_directories(rcspp_column_generation_headers INTERFACE
      $<TARGET_PROPERTY:rcspp_objects,INTERFACE_INCLUDE_DIRECTORIES>)
else()
  target_link_libraries(rcspp_column_generation_headers INTERFACE rcspp::rcspp)
endif()

# MARK: Layer

add_subdirectory(cpp/rcspp/column_generation)

# MARK: Gurobi master solver

# Asked for explicitly, so a missing Gurobi or a missing C++ library is an error, not a skip.
if(USE_COLUMN_GENERATION_EXAMPLE_GUROBI)
  set(GUROBI_REQUIRE_CXX ON)
  find_package(GUROBI REQUIRED)
  if(NOT TARGET GUROBI::gurobi_cxx)
    message(FATAL_ERROR
            "USE_COLUMN_GENERATION_EXAMPLE_GUROBI needs the Gurobi C++ library (gurobi_c++); "
            "pass -DGUROBI_CXX_LIBRARY=<path>.")
  endif()
  add_subdirectory(cpp/rcspp/column_generation/gurobi)
endif()

# MARK: HiGHS master solver

# An installed HiGHS when CMake finds one (pass -Dhighs_DIR=<dir> to point at it), else HiGHS
# v1.15.1, fetched and built with the project. Either way the target is highs::highs.
if(USE_COLUMN_GENERATION_EXAMPLE_HIGHS)
  include(FetchContent)
  FetchContent_Declare(highs
    URL https://github.com/ERGO-Code/HiGHS/archive/refs/tags/v1.15.1.tar.gz
    URL_HASH SHA256=a840d269dff2fafb371dd247df13ad5e026d7ce3b35ad3dc1eedd59bf0c2fb16
    FIND_PACKAGE_ARGS CONFIG
  )
  # Settings for a fetched HiGHS only: the library alone, static and position independent (it
  # ends up in the Python module too), and not linted as our code is. Its symbols are hidden, so
  # that the Python module does not export them on Linux and macOS: another HiGHS in the same
  # process, such as highspy's, keeps its own.
  block(PROPAGATE highs_FOUND highs_VERSION highs_DIR)
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(CMAKE_C_VISIBILITY_PRESET hidden)
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
    set(BUILD_TESTING OFF)
    set(BUILD_CXX_EXE OFF)
    set(BUILD_EXAMPLES OFF)
    set(BUILD_SHARED_EXTRAS_LIB OFF)
    set(ZLIB OFF)
    set(CMAKE_CXX_CLANG_TIDY "")
    FetchContent_MakeAvailable(highs)
    # Nothing of the example is installed, so neither is a fetched HiGHS: cmake --install leaves
    # out the install rules of a folder excluded from all, and the targets the example links are
    # still built.
    if(NOT highs_FOUND)
      set_property(DIRECTORY "${highs_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL ON)
    endif()
  endblock()
  if(NOT TARGET highs::highs)
    message(FATAL_ERROR
            "USE_COLUMN_GENERATION_EXAMPLE_HIGHS: the HiGHS found or fetched has no highs::highs "
            "target.")
  endif()
  if(highs_FOUND)
    message(STATUS "HiGHS: installed, ${highs_VERSION} (${highs_DIR})")
  else()
    message(STATUS "HiGHS: fetched, v1.15.1")
  endif()
  add_subdirectory(cpp/rcspp/column_generation/highs)
endif()

# MARK: Tests

if(USE_TESTS)
  add_subdirectory(tests/cpp)
endif()
