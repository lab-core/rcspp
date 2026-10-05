// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>

struct MPSolution {
        std::map<size_t, double> value_by_var_id;
        std::map<size_t, double> dual_by_var_id;
        double cost;
        /// @brief False when column generation stopped on "no improving column" while the final
        /// pricing subproblem did not prove its result (timeout / memory / phase / solution cap /
        /// truncated join). The master cost is then an upper bound on the LP optimum, not a lower
        /// bound.
        bool proven_optimal = true;
};
