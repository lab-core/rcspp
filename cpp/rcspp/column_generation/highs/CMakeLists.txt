# HiGHS master solver of column generation (USE_HIGHS)

project(rcspp-highs LANGUAGES CXX)

# A static library of its own: the core library stays free of HiGHS.
add_library(rcspp_highs STATIC "${CMAKE_CURRENT_SOURCE_DIR}/highs_master_solver.cpp")
# Hidden symbols, as the fetched HiGHS's: the HiGHS functions inlined here must not be exported
# by the Python module either.
set_target_properties(rcspp_highs PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
target_compile_features(rcspp_highs PUBLIC cxx_std_23)
# The folder above rcspp/, found from here so that the library also builds when rcspp is part of
# a larger project.
target_include_directories(rcspp_highs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..  # "rcspp/..."
)
target_link_libraries(rcspp_highs PUBLIC highs::highs)
