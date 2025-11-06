// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <vector>

struct Path {
    Path(size_t id, double cost, const std::vector<size_t>& visited_nodes);

    size_t id;
    double cost;
    std::vector<size_t> visited_nodes;
};
