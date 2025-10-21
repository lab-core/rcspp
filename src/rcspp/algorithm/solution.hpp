// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <vector>

namespace rcspp {

struct Solution {
        double cost;
        std::vector<size_t> path_node_ids;
        std::vector<size_t> path_arc_ids;
};
}  // namespace rcspp