// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// A program built against an installed rcspp::column_generation_highs: the master solver's header
// compiles from the installed headers, and the master solver links, with HiGHS and column
// generation.

#include <Highs.h>

#include "rcspp/column_generation.hpp"
#include "rcspp/column_generation/highs/highs_master_solver.hpp"

int main() {
    Highs highs;
    rcspp::column_generation::HighsMasterSolver solver(highs, {});
    rcspp::column_generation::MasterProblem master(solver);
    return static_cast<int>(master.number_of_rows());
}
