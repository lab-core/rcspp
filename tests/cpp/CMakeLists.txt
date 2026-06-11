# RCSPP Tests CMakeLists.txt for rcspp

project(tests-rcspp LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
# Disable clang-tidy for GTest/GMock targets — they are third-party sources.
set_target_properties(gtest gtest_main gmock gmock_main PROPERTIES CXX_CLANG_TIDY "")

# Sources ---------------------------------------------------------------------
file(GLOB_RECURSE TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_connectivity.cpp")
file(GLOB_RECURSE LIB_SOURCES "${CMAKE_SOURCE_DIR}/cpp/rcspp/*.cpp")

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
