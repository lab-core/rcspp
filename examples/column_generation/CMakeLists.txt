# The C++ column generation example: a column generation layer over rcspp, with its tests. Not
# part of the library: nothing here is installed. Built from rcspp's root with
# -DUSE_COLUMN_GENERATION_EXAMPLE=ON, over rcspp's own targets.

cmake_minimum_required(VERSION 3.26)

project(rcspp-column-generation-example LANGUAGES CXX)

# The core library the example's targets link: the core's objects, as the Python module links them
# (so that it does not depend on rcspp's shared library).
set(RCSPP_EXAMPLE_CORE rcspp_objects)

# MARK: Headers

# "rcspp/column_generation/...": the example's cpp/ folder; "rcspp/...": the core's headers.
add_library(rcspp_column_generation_headers INTERFACE)
target_include_directories(rcspp_column_generation_headers INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/cpp"
    $<TARGET_PROPERTY:rcspp_objects,INTERFACE_INCLUDE_DIRECTORIES>)

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
endif()

# MARK: Tests

if(USE_TESTS)
  add_subdirectory(tests/cpp)
endif()
