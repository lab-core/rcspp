# RCSPP Tests CMakeLists.txt for rcspp

project(tests-rcspp LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# GoogleTest is for the tests only: rcspp's install leaves it out.
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
# Disable clang-tidy for GTest/GMock targets — they are third-party sources.
set_target_properties(gtest gtest_main gmock gmock_main PROPERTIES CXX_CLANG_TIDY "")

# Sources ---------------------------------------------------------------------
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_connectivity.cpp")
# The Gurobi and HiGHS master solver tests are separate executables (below): these need neither.
# Their folders' files are left out by name; a pattern on the full path would also match a checkout
# that happens to sit in a folder named gurobi or highs, and leave out every test.
file(GLOB SOLVER_TEST_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/gurobi/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/highs/*.cpp")
list(REMOVE_ITEM TEST_SOURCES ${SOLVER_TEST_SOURCES})
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/cpp/rcspp/*.cpp")
# Column generation's own sources with USE_COLUMN_GENERATION only; its master solvers are libraries
# of their own, linked by their tests below.
file(GLOB_RECURSE column_generation_sources CONFIGURE_DEPENDS "${RCSPP_COLUMN_GENERATION_DIR}/*.cpp")
list(REMOVE_ITEM LIB_SOURCES ${column_generation_sources})
if(USE_COLUMN_GENERATION)
  file(GLOB column_generation_sources CONFIGURE_DEPENDS "${RCSPP_COLUMN_GENERATION_DIR}/*.cpp")
  list(APPEND LIB_SOURCES ${column_generation_sources})
else()
  list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_column_generation.cpp")
endif()

set(SOURCE_FILES ${TEST_SOURCES} ${LIB_SOURCES})
# The vrp_subproblem test reuses a few VRP data classes from the VRP example.
list(APPEND SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/examples/cpp/vrp/instance_reader.cpp
    ${CMAKE_SOURCE_DIR}/examples/cpp/vrp/instance.cpp
    ${CMAKE_SOURCE_DIR}/examples/cpp/vrp/customer.cpp
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif()
# MinGW writes COFF objects, whose limits the larger test TUs exceed. -mbig-obj (MinGW's /bigobj)
# lifts the section count, but every out-of-line template instantiation still adds a long-named
# section (.text$<mangled>, and .debug_frame$<mangled> with debug info) to a string table that
# cannot pass 10 MB. Unoptimised, nothing is inlined and several TUs overflow it; so on MinGW the
# tests build with -O1 and without debug info, whatever the build type. NDEBUG is not touched, so a
# Debug build still runs every assert.
if(MINGW)
  target_compile_options(${PROJECT_NAME} PRIVATE -Wa,-mbig-obj -O1 -g0)
endif()
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/cpp
    ${CMAKE_SOURCE_DIR}/examples/cpp  # for "vrp/..." headers used by vrp_subproblem test
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${PROJECT_NAME} GTest::gtest_main)
if(USE_COVERAGE)
  target_link_libraries(${PROJECT_NAME} rcspp_coverage)
endif()

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME} DISCOVERY_TIMEOUT 30)

if(USE_COLUMN_GENERATION_GUROBI)
  add_subdirectory(gurobi)
endif()

if(USE_COLUMN_GENERATION_HIGHS)
  add_subdirectory(highs)
endif()
