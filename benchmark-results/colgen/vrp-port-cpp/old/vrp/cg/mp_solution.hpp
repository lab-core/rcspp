// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>

struct MPSolution {
        std::map<size_t, double> value_by_var_id;
        std::map<size_t, double> dual_by_var_id;
        double cost;
        /// @brief False when column generation stopped on "no improving column" while the final
        /// pricing subproblem did not run to completion (timeout / memory / phase / solution cap).
        /// The master cost is then a valid bound but is NOT proven optimal.
        bool proven_optimal = true;
};
