// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "path.hpp"

Path::Path(size_t id, double cost, const std::vector<size_t>& visited_nodes)
    : id(id), cost(cost), visited_nodes(visited_nodes) {}
