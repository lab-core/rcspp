// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <list>

struct Path {
        Path(size_t id, double cost, const std::list<size_t>& visited_nodes);

        size_t id;
        double cost;
        std::list<size_t> visited_nodes;
};
