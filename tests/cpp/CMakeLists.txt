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
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_connectivity.cpp")
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/cpp/rcspp/*.cpp")

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

# Docs samples ----------------------------------------------------------------
# Every C++ code block of the docs must compile. scripts/extract_cpp_doc_samples.py writes them into
# one translation unit, whose #line directives point an error at the Markdown page, and docs-samples
# compiles it. Compiling is the test, so the target is neither linked nor instrumented for coverage.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  file(GLOB_RECURSE DOC_PAGES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/docs/*.md")
  list(FILTER DOC_PAGES EXCLUDE REGEX "/docs/_build/")
  list(APPEND DOC_PAGES "${CMAKE_SOURCE_DIR}/AGENT.md" "${CMAKE_SOURCE_DIR}/README.md")
  set(DOC_SAMPLES_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/extract_cpp_doc_samples.py")
  set(DOC_SAMPLES_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/docs_samples.cpp")
  add_custom_command(
    OUTPUT "${DOC_SAMPLES_SOURCE}"
    COMMAND "${Python3_EXECUTABLE}" "${DOC_SAMPLES_SCRIPT}"
            --output "${DOC_SAMPLES_SOURCE}" --root "${CMAKE_SOURCE_DIR}" ${DOC_PAGES}
    DEPENDS "${DOC_SAMPLES_SCRIPT}" ${DOC_PAGES}
    COMMENT "Extracting the C++ samples of the docs"
    VERBATIM
  )
  add_library(docs-samples OBJECT "${DOC_SAMPLES_SOURCE}")
  target_include_directories(docs-samples PRIVATE ${CMAKE_SOURCE_DIR}/cpp)
  add_dependencies(docs-samples generate_umbrella_header)
  set_target_properties(docs-samples PROPERTIES CXX_CLANG_TIDY "")
  if(MSVC)
    target_compile_options(docs-samples PRIVATE /bigobj)
  endif()
  if(MINGW)
    target_compile_options(docs-samples PRIVATE -Wa,-mbig-obj -O1 -g0)
  endif()
else()
  message(STATUS "Python not found: the C++ samples of the docs are not compiled")
endif()
