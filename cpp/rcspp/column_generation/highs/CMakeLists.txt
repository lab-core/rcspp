# HiGHS master solver of column generation (USE_COLUMN_GENERATION_HIGHS)

project(rcspp-column-generation-highs LANGUAGES CXX)

# A static library of its own: the core library stays free of HiGHS.
add_library(rcspp_column_generation_highs STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/highs_master_solver.cpp")
add_library(rcspp::column_generation_highs ALIAS rcspp_column_generation_highs)
set_target_properties(rcspp_column_generation_highs PROPERTIES
    EXPORT_NAME column_generation_highs)
# Hidden symbols, as the fetched HiGHS's: the HiGHS functions inlined here must not be exported
# by the Python module either.
set_target_properties(rcspp_column_generation_highs PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
target_compile_features(rcspp_column_generation_highs PUBLIC cxx_std_23)
# "rcspp/...": the folder above rcspp/ in a build, found from here so that the library also builds
# when rcspp is part of a larger project; the installed headers once installed.
target_include_directories(rcspp_column_generation_highs PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
# Once installed it brings column generation with it. In a build, link rcspp::column_generation
# too: the Python module links column generation's objects instead, so that it does not depend on
# rcspp's shared library.
target_link_libraries(rcspp_column_generation_highs PUBLIC
    highs::highs
    $<INSTALL_INTERFACE:rcspp::column_generation>
)
