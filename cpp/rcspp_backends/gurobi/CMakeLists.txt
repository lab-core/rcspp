# Gurobi master solver of column generation (USE_GUROBI)

project(rcspp-gurobi LANGUAGES CXX)

# A static library of its own: the core library stays free of Gurobi.
add_library(rcspp_gurobi STATIC "${CMAKE_CURRENT_SOURCE_DIR}/gurobi_master_solver.cpp")
set_target_properties(rcspp_gurobi PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(rcspp_gurobi PUBLIC cxx_std_23)
target_include_directories(rcspp_gurobi PUBLIC
    ${CMAKE_SOURCE_DIR}/cpp  # "rcspp/..." and "rcspp_backends/..."
)
target_link_libraries(rcspp_gurobi PUBLIC GUROBI::gurobi_cxx)
