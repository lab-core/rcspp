# HiGHS master solver of the column generation example (USE_COLUMN_GENERATION_EXAMPLE_HIGHS)

project(rcspp-column-generation-highs LANGUAGES CXX)

# A static library of its own: the core library stays free of HiGHS.
add_library(rcspp_column_generation_highs STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/highs_master_solver.cpp")
add_library(rcspp::column_generation_highs ALIAS rcspp_column_generation_highs)
# Hidden symbols, as the fetched HiGHS's: the HiGHS functions inlined here must not be exported
# by the Python module either.
set_target_properties(rcspp_column_generation_highs PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
target_compile_features(rcspp_column_generation_highs PUBLIC cxx_std_23)
# The layer's headers and the core's; the programs that link this library link the layer too.
target_link_libraries(rcspp_column_generation_highs PUBLIC
    rcspp_column_generation_headers highs::highs)
