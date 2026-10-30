# Column generation (USE_COLUMN_GENERATION): the master problem, the loop, the pricers and the
# tolerances, over the core library. Its master solvers are in the folders below, built apart.

project(rcspp-column-generation LANGUAGES CXX)

# This folder's sources only: not those of the master solvers below.
file(GLOB COLUMN_GENERATION_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# OBJECT library, as the core's: compiled once, for the static library and for the Python module,
# which links the objects so that it does not depend on rcspp's shared library.
add_library(rcspp_column_generation_objects OBJECT ${COLUMN_GENERATION_SOURCES})
add_dependencies(rcspp_column_generation_objects generate_umbrella_header)
set_target_properties(rcspp_column_generation_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(rcspp_column_generation_objects PUBLIC cxx_std_23)
target_include_directories(rcspp_column_generation_objects PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# A static library over the core's shared one.
add_library(rcspp_column_generation STATIC $<TARGET_OBJECTS:rcspp_column_generation_objects>)
target_compile_features(rcspp_column_generation PUBLIC cxx_std_23)
target_include_directories(rcspp_column_generation PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(rcspp_column_generation PUBLIC rcspp)
add_library(rcspp::column_generation ALIAS rcspp_column_generation)
