# RCSPP Tests CMakeLists.txt for rcspp

project(tests-rcspp LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Sources ---------------------------------------------------------------------
file(GLOB_RECURSE TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_connectivity.cpp")
file(GLOB_RECURSE LIB_SOURCES "${CMAKE_SOURCE_DIR}/src/rcspp/*.cpp")

set(SOURCE_FILES ${TEST_SOURCES} ${LIB_SOURCES})
list(APPEND SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/vrp/instance_reader.cpp
    ${CMAKE_SOURCE_DIR}/src/vrp/instance.cpp
    ${CMAKE_SOURCE_DIR}/src/vrp/customer.cpp
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${PROJECT_NAME} GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})