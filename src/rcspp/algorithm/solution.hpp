// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <list>

namespace rcspp {

struct Solution {
        double cost = std::numeric_limits<double>::infinity();
        std::list<size_t> path_node_ids;
        std::list<size_t> path_arc_ids;
};
}  // namespace rcspp
