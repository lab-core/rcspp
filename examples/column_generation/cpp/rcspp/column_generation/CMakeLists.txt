# The example's column generation layer: the master problem, the loop, the pricers and the
# tolerances, over the core library. Its master solvers are in the folders below, built apart.

project(rcspp-column-generation LANGUAGES CXX)

# This folder's sources only: not those of the master solvers below.
file(GLOB COLUMN_GENERATION_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# OBJECT library, as the core's: compiled once, for the tests and the Python module, which links
# the objects so that it does not depend on rcspp's shared library.
add_library(rcspp_column_generation_objects OBJECT ${COLUMN_GENERATION_SOURCES})
if(TARGET generate_umbrella_header)
  add_dependencies(rcspp_column_generation_objects generate_umbrella_header)
endif()
set_target_properties(rcspp_column_generation_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(rcspp_column_generation_objects PUBLIC cxx_std_23)
target_link_libraries(rcspp_column_generation_objects PUBLIC rcspp_column_generation_headers)
