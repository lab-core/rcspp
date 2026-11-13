# Tests of the column generation example: the layer with scripted master solvers and pricers, and
# each master solver built. Out of rcspp's coverage gates.

project(tests-column-generation-example LANGUAGES CXX)

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

# One test executable over the core and the layer's objects; LIBRARIES: a master solver.
function(add_column_generation_test name)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;LIBRARIES")
  add_executable(${name} ${arg_SOURCES})
  if(MSVC)
    target_compile_options(${name} PRIVATE /bigobj)
  endif()
  if(MINGW)
    target_compile_options(${name} PRIVATE -Wa,-mbig-obj -O1 -g0)
  endif()
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(${name} PRIVATE
      ${RCSPP_EXAMPLE_CORE} rcspp_column_generation_objects ${arg_LIBRARIES} GTest::gtest_main)
  # Built on its own on Windows, the test links rcspp's DLL, which is not on PATH: copied next to
  # the executable, so that the test discovery below and the tests themselves find it.
  if(WIN32 AND NOT TARGET rcspp_objects)
    add_custom_command(TARGET ${name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:${name}>
                $<TARGET_FILE_DIR:${name}>
        COMMAND_EXPAND_LISTS)
  endif()
  gtest_discover_tests(${name} DISCOVERY_TIMEOUT 30)
endfunction()

add_column_generation_test(tests-column-generation SOURCES test_column_generation.cpp)

if(USE_COLUMN_GENERATION_EXAMPLE_GUROBI)
  file(GLOB gurobi_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/gurobi/*.cpp")
  add_column_generation_test(tests-column-generation-gurobi
      SOURCES ${gurobi_sources} LIBRARIES rcspp_column_generation_gurobi)
endif()

if(USE_COLUMN_GENERATION_EXAMPLE_HIGHS)
  file(GLOB highs_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/highs/*.cpp")
  add_column_generation_test(tests-column-generation-highs
      SOURCES ${highs_sources} LIBRARIES rcspp_column_generation_highs)
endif()
