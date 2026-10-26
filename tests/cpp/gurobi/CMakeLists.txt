# Tests of the Gurobi master solver (USE_GUROBI). Without a license they are skipped, not failed.

project(tests-rcspp-gurobi LANGUAGES CXX)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/cpp/rcspp/*.cpp")

add_executable(${PROJECT_NAME} ${TEST_SOURCES} ${LIB_SOURCES})
if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif()
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # the test fakes
)
target_link_libraries(${PROJECT_NAME} rcspp_gurobi GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME} DISCOVERY_TIMEOUT 30)
