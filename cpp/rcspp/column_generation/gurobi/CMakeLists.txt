# Gurobi master solver of column generation (USE_COLUMN_GENERATION_GUROBI)

project(rcspp-column-generation-gurobi LANGUAGES CXX)

# A static library of its own: the core library stays free of Gurobi.
add_library(rcspp_column_generation_gurobi STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/gurobi_master_solver.cpp")
add_library(rcspp::column_generation_gurobi ALIAS rcspp_column_generation_gurobi)
set_target_properties(rcspp_column_generation_gurobi PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(rcspp_column_generation_gurobi PUBLIC cxx_std_23)
# The folder above rcspp/, found from here so that the library also builds when rcspp is part of
# a larger project.
target_include_directories(rcspp_column_generation_gurobi PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..  # "rcspp/..."
)
target_link_libraries(rcspp_column_generation_gurobi PUBLIC GUROBI::gurobi_cxx)
