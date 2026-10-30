# Tests of the HiGHS master solver (USE_HIGHS).

project(tests-rcspp-highs LANGUAGES CXX)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(${PROJECT_NAME} ${TEST_SOURCES})
if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif()
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # the test fakes
)
# The core's and column generation's objects, compiled once for the libraries and every executable
# that needs them.
target_link_libraries(${PROJECT_NAME}
    rcspp_objects rcspp_column_generation_objects rcspp_highs GTest::gtest_main)
if(USE_COVERAGE)
  target_link_libraries(${PROJECT_NAME} rcspp_coverage)
endif()

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME} DISCOVERY_TIMEOUT 30)
