# VRP column generation example: the application (vrp/), its CLI and the
# benchmark drivers.
#
# These executables demonstrate the rcspp library on the Solomon and Gehring &
# Homberger instances under instances/. The benchmarks link the reusable
# rcspp-vrp-lib target defined in vrp/CMakeLists.txt, which transitively provides
# the rcspp library, Gurobi, Boost (optional) and the include roots.

project(rcspp-vrp-examples LANGUAGES CXX)

# Build the VRP application + reusable library. This defines rcspp-vrp-lib and
# the rcspp-vrp CLI (only when Gurobi is found — see vrp/CMakeLists.txt).
add_subdirectory(vrp)

# The VRP library is only defined when Gurobi was found. If it is missing, there
# are no benchmark drivers to build.
if(NOT TARGET rcspp-vrp-lib)
  message(STATUS "rcspp-vrp-lib target not available; skipping VRP benchmarks.")
  return()
endif()

# Helper: declare one example executable linking the VRP library and copy the
# rcspp shared library next to it so it can be run from the build tree.
function(add_vrp_example name source)
  add_executable(${name} "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
  target_link_libraries(${name} PRIVATE rcspp-vrp-lib)
  add_custom_command(TARGET ${name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "$<TARGET_FILE:rcspp>"
              $<TARGET_FILE_DIR:${name}>
  )
endfunction()

# Benchmark (C1/R1/RC1 families)
add_vrp_example(rcspp-vrp-benchmark benchmark_main.cpp)

# Large benchmark (C2/R2/RC2 + Gehring & Homberger)
add_vrp_example(rcspp-vrp-benchmark-large benchmark_large_main.cpp)

# ng-path vs. no ng over a whole column generation. Requires kRouteRelaxation = NgPath in
# vrp/vrp.hpp (it static_asserts otherwise), so it is excluded from the default build:
#   cmake --build <build-dir> --target rcspp-ng-cg-comparison
add_vrp_example(rcspp-ng-cg-comparison ng_cg_comparison_main.cpp)
set_target_properties(rcspp-ng-cg-comparison PROPERTIES EXCLUDE_FROM_ALL TRUE)

# Forward against bidirectional over a whole column generation, one row per pricing solve.
# The instrument every "learns across solves" proposal is measured through.
add_vrp_example(rcspp-bidirectional-cg bidirectional_cg_main.cpp)

# Dynamic ng augmentation against fixed ng sizes, over whole column generations. Like the ng
# comparison, it requires kRouteRelaxation = NgPath (it static_asserts otherwise), so it is
# excluded from the default build:
#   cmake --build <build-dir> --target rcspp-ng-dynamic-cg
add_vrp_example(rcspp-ng-dynamic-cg ng_dynamic_cg_main.cpp)
set_target_properties(rcspp-ng-dynamic-cg PROPERTIES EXCLUDE_FROM_ALL TRUE)
