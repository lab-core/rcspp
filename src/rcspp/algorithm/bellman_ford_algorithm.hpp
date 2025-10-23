// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/graph/graph.hpp"

#include <limits>
#include <unordered_map>
#include <vector>

#include "rcspp/resource/concrete/real_resource.hpp"

namespace rcspp {

struct Distance : public std::unordered_map<size_t, double> {
        Distance() = default;
        template <typename ResourceType>
        Distance(const std::vector<size_t>& target_ids, const Graph<ResourceType>& graph) {
            for (const auto& id : graph.get_node_ids()) {
                this->operator[](id) = std::numeric_limits<double>::infinity();
            }
            for (const auto& node_id : target_ids) {
                this->operator[](node_id) = 0.0;
            }
        }
};

class BellmanFordAlgorithm {
    public:
        // compute shortest paths from any of the given targets to all nodes (forward) or from all
        // nodes to any of the given targets (backward)
        template <typename CostResourceType, typename... ResourceTypes>
        static Distance solve(const Graph<ResourceComposition<ResourceTypes...>>& graph_,
                              const std::vector<size_t>& target_ids, size_t cost_index = 0,
                              bool forward = true) {
            auto node_ids = graph_.get_node_ids();
            auto arc_ids = graph_.get_arc_ids();

            // Distance from source to each node
            Distance distance(target_ids, graph_);

            // Relax arcs |N|-1 times, on |N| iteration -> check for negative-weight cycles
            for (size_t i = 0; i < node_ids.size(); ++i) {
                bool modified = false;
                bool last_iteration = (i == node_ids.size() - 1);
                for (const auto& [arc_id, arc] : graph_.get_arcs_by_id()) {
                    // fetch cost
                    // get the origin cost of the cost resource
                    const CostResourceType& origin_cost_resource =
                        arc->origin->resource->template get_resource_component<CostResourceType>(
                            cost_index);
                    double origin_cost = origin_cost_resource.get_value();
                    // expand the resource
                    Resource<ResourceComposition<ResourceTypes...>> resource(
                        *arc->destination->resource);
                    arc->expander->expand(*arc->origin->resource, resource);
                    // fetch the new value of the cost resource
                    const CostResourceType& cost_resource =
                        resource.template get_resource_component<CostResourceType>(
                            cost_index);
                    double cost = cost_resource.get_value();
                    // compute the weight, i.e., cost difference
                    double w = cost - origin_cost;
                    size_t u = arc->origin->id;
                    size_t v = arc->destination->id;
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
};

}  // namespace rcspp
