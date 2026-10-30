// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// A program built against an installed rcspp::column_generation_gurobi: the master solver's header
// compiles from the installed headers, and the master solver links, with Gurobi and column
// generation. Built, not run: running needs a Gurobi license.

#include <gurobi_c++.h>

#include "rcspp/column_generation.hpp"
#include "rcspp/column_generation/gurobi/gurobi_master_solver.hpp"

int main() {
    GRBEnv environment;
    GRBModel model(environment);
    rcspp::column_generation::GurobiMasterSolver solver(model, {});
    rcspp::column_generation::MasterProblem master(solver);
    return static_cast<int>(master.number_of_rows());
}
