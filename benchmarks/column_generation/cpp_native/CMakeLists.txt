# The benchmark's cpp-native layer: the C++ column generation example (examples/column_generation)
# in a C++ program, which runners/cpp_native_runner.py starts for each run. Built with the example
# and at least one of its master solvers; nothing here is installed.

project(rcspp-cpp-native-solver LANGUAGES CXX)

if(NOT TARGET rcspp_column_generation_gurobi AND NOT TARGET rcspp_column_generation_highs)
  message(STATUS "Benchmark's cpp-native layer: skipped, it needs "
                 "USE_COLUMN_GENERATION_EXAMPLE_GUROBI or USE_COLUMN_GENERATION_EXAMPLE_HIGHS")
  return()
endif()

add_executable(cpp-native-solver cpp_native_solver.cpp)
target_link_libraries(cpp-native-solver PRIVATE rcspp_objects rcspp_column_generation_objects)
# The configuration it was built in, for the run's record.
target_compile_definitions(cpp-native-solver PRIVATE RCSPP_BUILD_CONFIG="$<CONFIG>")
if(TARGET rcspp_column_generation_gurobi)
  target_link_libraries(cpp-native-solver PRIVATE rcspp_column_generation_gurobi)
  target_compile_definitions(cpp-native-solver PRIVATE RCSPP_CPP_NATIVE_GUROBI)
endif()
if(TARGET rcspp_column_generation_highs)
  target_link_libraries(cpp-native-solver PRIVATE rcspp_column_generation_highs)
  target_compile_definitions(cpp-native-solver PRIVATE RCSPP_CPP_NATIVE_HIGHS)
endif()
if(WIN32)
  # GetProcessMemoryInfo, for the process's peak memory.
  target_link_libraries(cpp-native-solver PRIVATE psapi)
endif()
