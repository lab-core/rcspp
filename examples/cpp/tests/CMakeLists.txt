# Tests of the C++ VRP example: its instance reader, its pricing graph, its column generation over
# each master solver built, and its command line. Out of rcspp's coverage gates.

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
set_target_properties(gtest gtest_main gmock gmock_main PROPERTIES CXX_CLANG_TIDY "")

include(GoogleTest)

add_executable(tests-vrp test_vrp.cpp)
if(MSVC)
  target_compile_options(tests-vrp PRIVATE /bigobj)
endif()
target_link_libraries(tests-vrp PRIVATE ${RCSPP_VRP_LINK} GTest::gtest_main)
# The macros the example's sources see, for the tests that need a master.
if(TARGET rcspp_column_generation_gurobi)
  target_compile_definitions(tests-vrp PRIVATE RCSPP_VRP_GUROBI)
endif()
if(TARGET rcspp_column_generation_highs)
  target_compile_definitions(tests-vrp PRIVATE RCSPP_VRP_HIGHS)
endif()
gtest_discover_tests(tests-vrp DISCOVERY_TIMEOUT 30)

# The command line, run from the repository's root on the first master built.
if(TARGET rcspp_column_generation_highs)
  set(master highs)
else()
  set(master gurobi)
endif()
cmake_path(SET root NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

add_test(NAME vrp.command_line.subproblem_only
         COMMAND rcspp-vrp C101_5 --subproblem-only WORKING_DIRECTORY "${root}")
set_tests_properties(vrp.command_line.subproblem_only PROPERTIES
    PASS_REGULAR_EXPRESSION "C101_5: 5 customers.*Routes found: ")

add_test(NAME vrp.command_line.full_run
         COMMAND rcspp-vrp instances/C101_25.txt --master ${master} WORKING_DIRECTORY "${root}")
set_tests_properties(vrp.command_line.full_run PROPERTIES
    PASS_REGULAR_EXPRESSION "iteration    1.*Optimal: LP 191\.8136.*Integer solve: Optimal")

add_test(NAME vrp.command_line.without_integer_solve
         COMMAND rcspp-vrp C101_5 --master ${master} --no-integer WORKING_DIRECTORY "${root}")
set_tests_properties(vrp.command_line.without_integer_solve PROPERTIES
    PASS_REGULAR_EXPRESSION "Optimal: LP" FAIL_REGULAR_EXPRESSION "Integer solve")

add_test(NAME vrp.command_line.needs_a_master
         COMMAND rcspp-vrp C101_5 WORKING_DIRECTORY "${root}")
set_tests_properties(vrp.command_line.needs_a_master PROPERTIES
    WILL_FAIL TRUE)
add_test(NAME vrp.command_line.says_a_master_is_needed
         COMMAND rcspp-vrp C101_5 WORKING_DIRECTORY "${root}")
set_tests_properties(vrp.command_line.says_a_master_is_needed PROPERTIES
    PASS_REGULAR_EXPRESSION "--master is required")
