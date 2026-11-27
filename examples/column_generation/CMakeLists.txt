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

# MARK: Tests

if(USE_TESTS)
  add_subdirectory(tests/cpp)
endif()
