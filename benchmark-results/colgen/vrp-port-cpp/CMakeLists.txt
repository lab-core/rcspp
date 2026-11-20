# MEASUREMENT ONLY: the old C++ VRP example (timed copy) against the same VRP on the C++ column
# generation example layer, built over an rcspp checkout without changing it.
#
#   cmake -S . -B build -DRCSPP_SOURCE=../../../../rcspp-dev -DCMAKE_CXX_CLANG_TIDY=
#   cmake --build build --config Release --target old-vrp new-vrp

cmake_minimum_required(VERSION 3.26)
project(vrp-port-cpp LANGUAGES CXX)

set(RCSPP_SOURCE "" CACHE PATH "the rcspp checkout")
set(USE_TESTS OFF CACHE BOOL "" FORCE)
set(USE_PYTHON OFF CACHE BOOL "" FORCE)
set(USE_VRP OFF CACHE BOOL "" FORCE)
set(USE_COLUMN_GENERATION_EXAMPLE ON CACHE BOOL "" FORCE)
set(USE_COLUMN_GENERATION_EXAMPLE_GUROBI ON CACHE BOOL "" FORCE)
set(USE_COLUMN_GENERATION_EXAMPLE_HIGHS OFF CACHE BOOL "" FORCE)
add_subdirectory("${RCSPP_SOURCE}" rcspp)

# Imported targets are scoped to the folder that found them.
list(PREPEND CMAKE_MODULE_PATH "${RCSPP_SOURCE}/cmake")
set(GUROBI_REQUIRE_CXX ON)
find_package(GUROBI REQUIRED)

file(GLOB OLD_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/old/vrp/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/old/vrp/cg/*.cpp")

add_executable(old-vrp old_main.cpp ${OLD_SOURCES})
target_include_directories(old-vrp PRIVATE old/vrp old "${RCSPP_SOURCE}/cpp")
target_link_libraries(old-vrp PRIVATE rcspp GUROBI::gurobi_cxx)

add_executable(new-vrp new_main.cpp old/vrp/instance.cpp old/vrp/instance_reader.cpp
               old/vrp/customer.cpp)
target_include_directories(new-vrp PRIVATE old/vrp "${RCSPP_SOURCE}/cpp")
target_link_libraries(new-vrp PRIVATE rcspp rcspp_column_generation_objects
                      rcspp_column_generation_gurobi)

foreach(program old-vrp new-vrp)
  add_custom_command(TARGET ${program} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:rcspp>"
              $<TARGET_FILE_DIR:${program}>)
endforeach()

add_executable(replay-vrp replay_main.cpp ${OLD_SOURCES})
target_include_directories(replay-vrp PRIVATE old/vrp old "${RCSPP_SOURCE}/cpp")
target_link_libraries(replay-vrp PRIVATE rcspp rcspp_column_generation_objects
                      rcspp_column_generation_gurobi)
add_custom_command(TARGET replay-vrp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:rcspp>"
            $<TARGET_FILE_DIR:replay-vrp>)
