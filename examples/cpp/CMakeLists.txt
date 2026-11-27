# The C++ VRP example: solve_vrp.cpp solves a vehicle routing problem with time windows by column
# generation on the C++ column generation example (examples/column_generation), with a master
# solver over Gurobi or HiGHS. Built from rcspp's root with USE_COLUMN_GENERATION_EXAMPLE and at
# least one of its master solvers.

project(rcspp-vrp-example LANGUAGES CXX)

if(NOT TARGET rcspp_column_generation_gurobi AND NOT TARGET rcspp_column_generation_highs)
  message(STATUS "C++ VRP example: skipped, it needs USE_COLUMN_GENERATION_EXAMPLE_GUROBI or "
                 "USE_COLUMN_GENERATION_EXAMPLE_HIGHS")
  return()
endif()

# The core's objects, as the column generation example links them.
set(RCSPP_VRP_CORE rcspp_objects)

# The instance, the graph and the column generation, for the program and its tests.
add_library(rcspp_vrp_example_objects OBJECT
    vrp/instance.cpp vrp/graph.cpp vrp/column_generation.cpp)
target_compile_features(rcspp_vrp_example_objects PUBLIC cxx_std_23)
target_include_directories(rcspp_vrp_example_objects PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(rcspp_vrp_example_objects PUBLIC rcspp_column_generation_headers)
cmake_path(SET RCSPP_VRP_INSTANCES NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/../../instances")
target_compile_definitions(rcspp_vrp_example_objects PRIVATE
    RCSPP_VRP_INSTANCES_DIRECTORY="${RCSPP_VRP_INSTANCES}")

# The master solvers built: each a library to link and a macro the code tests.
set(RCSPP_VRP_MASTER_LIBRARIES "")
if(TARGET rcspp_column_generation_gurobi)
  target_compile_definitions(rcspp_vrp_example_objects PRIVATE RCSPP_VRP_GUROBI)
  target_link_libraries(rcspp_vrp_example_objects PUBLIC rcspp_column_generation_gurobi)
  list(APPEND RCSPP_VRP_MASTER_LIBRARIES rcspp_column_generation_gurobi)
endif()
if(TARGET rcspp_column_generation_highs)
  target_compile_definitions(rcspp_vrp_example_objects PRIVATE RCSPP_VRP_HIGHS)
  target_link_libraries(rcspp_vrp_example_objects PUBLIC rcspp_column_generation_highs)
  list(APPEND RCSPP_VRP_MASTER_LIBRARIES rcspp_column_generation_highs)
endif()

# Every object a VRP program links.
set(RCSPP_VRP_LINK rcspp_vrp_example_objects ${RCSPP_VRP_CORE} rcspp_column_generation_objects
    ${RCSPP_VRP_MASTER_LIBRARIES})

add_executable(rcspp-vrp solve_vrp.cpp)
target_link_libraries(rcspp-vrp PRIVATE ${RCSPP_VRP_LINK})

if(USE_TESTS)
  add_subdirectory(tests)
endif()
