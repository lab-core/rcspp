project(rcspp VERSION 0.1 LANGUAGES CXX)

# Gather all C++ sources/headers for the library -------------------------------
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE HEADERS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")

# Generate the umbrella header rcspp.hpp ---------------------------------------
set(RCSPP_UMBRELLA_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/rcspp.hpp")
list(REMOVE_ITEM HEADERS_FILES "${RCSPP_UMBRELLA_HEADER}")

# Previously the umbrella header was generated at configure time using file(WRITE)/file(APPEND).
# That approach can race in parallel builds (when multiple build jobs attempt generation at the
# same time). Instead create a proper build-step (custom command + custom target) which:
#  - declares the umbrella header as an OUTPUT of a custom command
#  - creates a custom target depending on that output
#  - makes the library depend on that target so the header is generated before compilation
#  - (optionally) serializes the target with a JOB_POOL so it cannot run concurrently with
#    other pooled jobs when the build system is invoked with -j.

# Create a small helper CMake script (generate_umbrella.cmake) that will be invoked by the
# custom command to produce the umbrella header. The script lives next to this CMakeLists.

# Custom command that generates the umbrella header using the helper script
add_custom_command(
  OUTPUT "${RCSPP_UMBRELLA_HEADER}"
  # Regenerate when any header under this directory changes
  DEPENDS ${HEADERS_FILES}
  COMMAND ${CMAKE_COMMAND}
          -DREPO_SRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}
          -DREPO_REL_DIR=${CMAKE_CURRENT_SOURCE_DIR}/..
          -DREPO_BUILD_DIR=${BUILD_INCLUDE_DIR}
          -DOUT_FILE=${RCSPP_UMBRELLA_HEADER}
          -P "${CMAKE_CURRENT_SOURCE_DIR}/generate_umbrella_include.cmake"
  COMMENT "Generating umbrella header ${RCSPP_UMBRELLA_HEADER}"
  VERBATIM
)

# Custom target that depends on the generated umbrella header
add_custom_target(generate_umbrella_header DEPENDS "${RCSPP_UMBRELLA_HEADER}")

# If CMake supports job pools, create a small pool with a single slot and assign it to
# the generator target, ensuring the command will not run in parallel with other jobs
# that use the same pool.
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.15")
  # Define a global job pool named 'genpool' with 1 slot
  set_property(GLOBAL PROPERTY JOB_POOLS genpool=1)
  # Assign the target to that pool (serialize execution)
  set_property(TARGET generate_umbrella_header PROPERTY JOB_POOL genpool)
endif()

# Make sure the library build depends on the umbrella header generation so it is created
# before compilation starts.
# NOTE: ${LIB} is created further down in this CMakeLists, so we'll add the dependency
# after the target is declared. To keep this edit minimal we will remember to add
# add_dependencies(${LIB} generate_umbrella_header) right after the add_library call below.

# Compile the dynamic library --------------------------------------------------
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(LIB ${PROJECT_NAME})

# OBJECT library: compiled once, shared by rcspp shared lib and _core.pyd.
# Linking _core.pyd against rcspp_objects bakes the symbols in directly so the
# Python extension has no runtime dependency on rcspp.dll.
add_library(rcspp_objects OBJECT ${SOURCE_FILES})
add_dependencies(rcspp_objects generate_umbrella_header)
set_target_properties(rcspp_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(rcspp_objects PUBLIC cxx_std_23)
target_include_directories(rcspp_objects PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_library(${LIB} SHARED ${HEADERS_FILES})
target_link_libraries(${LIB} PRIVATE rcspp_objects)
add_dependencies(${LIB} generate_umbrella_header)
set_target_properties(${LIB} PROPERTIES VERSION ${PROJECT_VERSION})
target_compile_features(${LIB} PUBLIC cxx_std_23)
