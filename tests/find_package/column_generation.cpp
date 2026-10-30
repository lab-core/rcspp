// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// A program built against an installed rcspp::column_generation: its umbrella header compiles from
// the installed headers, and a function of the library links.

#include "rcspp/column_generation.hpp"

int main() {
    const rcspp::column_generation::AbsoluteTolerances tolerances;
    return tolerances.reduced_cost(rcspp::column_generation::ToleranceContext{}) > 0.0 ? 0 : 1;
}
