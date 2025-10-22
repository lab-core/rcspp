// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/graph/graph.hpp"

#include <limits>
#include <unordered_map>

#include <vector>

namespace rcspp {

struct Distance : public std::unordered_map<size_t, double> {
        Distance() = default;
        template <typename ResourceType>
        Distance(const std::vector<size_t>& target_ids, const Graph<ResourceType>& graph) {
            for (const auto& id : graph.get_node_ids()) {
                this->operator[][id] = std::numeric_limits<double>::infinity();
            }
            for (const auto& node_id : target_ids) {
                this->operator[][node_id] = 0.0;
            }
        }
};

class BellmanFordAlgorithm {
    public:
        // compute shortest paths from any of the given targets to all nodes (forward) or from all
        // nodes to any of the given targets (backward)
        template <typename ResourceType>
            requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
        static Distance solve(const Graph<ResourceType>& graph_,
                              const std::vector<size_t>& target_ids, bool forward = true);
};

}  // namespace rcspp
