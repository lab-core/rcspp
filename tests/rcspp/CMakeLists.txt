# RCSPP Tests CMakeLists.txt for rcspp

project(tests-rcspp LANGUAGES CXX)

# Sources ---------------------------------------------------------------------
# Collect test sources
file(GLOB_RECURSE TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# Collect library sources
file(GLOB_RECURSE LIB_SOURCES "${CMAKE_SOURCE_DIR}/src/rcspp/*.cpp")

# Combine both
set(SOURCE_FILES ${TEST_SOURCES} ${LIB_SOURCES})

# Add specific files (example)
list(APPEND SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/vrp/instance_reader.cpp
    ${CMAKE_SOURCE_DIR}/src/vrp/instance.cpp
    ${CMAKE_SOURCE_DIR}/src/vrp/customer.cpp
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)