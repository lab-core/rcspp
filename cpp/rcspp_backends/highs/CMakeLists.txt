# HiGHS master solver of column generation (USE_HIGHS)

project(rcspp-highs LANGUAGES CXX)

# A static library of its own: the core library stays free of HiGHS.
add_library(rcspp_highs STATIC "${CMAKE_CURRENT_SOURCE_DIR}/highs_master_solver.cpp")
set_target_properties(rcspp_highs PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(rcspp_highs PUBLIC cxx_std_23)
target_include_directories(rcspp_highs PUBLIC
    ${CMAKE_SOURCE_DIR}/cpp  # "rcspp/..." and "rcspp_backends/..."
)
target_link_libraries(rcspp_highs PUBLIC highs::highs)
