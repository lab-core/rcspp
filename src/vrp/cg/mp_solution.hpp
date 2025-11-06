// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>

struct MPSolution {
    std::map<size_t, double> value_by_var_id;
    std::map<size_t, double> dual_by_var_id;
    double cost;
};
