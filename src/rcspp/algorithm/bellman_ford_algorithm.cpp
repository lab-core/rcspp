// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.
 
#include "bellman_ford_algorithm.hpp"

namespace rcspp {

// compute shortest paths from the given node to all nodes
template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
Distance BellmanFordAlgorithm::solve(const Graph<ResourceType>& graph_,
                                     const std::vector<size_t>& target_ids,
                                     bool forward) {
    auto node_ids = graph_.get_node_ids();
    auto arc_ids = graph_.get_arc_ids();

    // Distance from source to each node
    Distance distance(target_ids, graph_);

    // Relax arcs |N|-1 times, on |N| iteration -> check for negative-weight cycles
    for (size_t i = 0; i < node_ids.size(); ++i) {
        bool modified = false;
        bool last_iteration = (i == node_ids.size() - 1);
        for (const auto& [arc, arc_id] : graph_.get_arcs_by_id()) {
            size_t u = arc.origin->id;
            size_t v = arc.destination->id;
            double w = arc.cost;
            if (forward && distance[u] + w < distance[v]) {
                distance[v] = distance[u] + w;
                modified = true;
            } else if (!forward && distance[v] + w < distance[u]) {
                distance[u] = distance[v] + w;
                modified = true;
            }

            if (last_iteration && modified) {
                throw std::runtime_error("Graph contains a negative-weight cycle");
            }
        }
        if (!modified) {
            break;  // No changes in this iteration, so we can stop early
        }
    }

    return distance;
}
}  // namespace rcspp