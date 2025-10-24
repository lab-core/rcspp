// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <random>
#include <unordered_map>
#include <vector>

#include "rcspp/graph/graph.hpp"
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
        struct ArcRelaxation {
                size_t origin_id;
                size_t destination_id;
                double weight;
        };

    public:
        // compute shortest paths from any of the given targets to all nodes (forward) or from all
        // nodes to any of the given targets (backward)
        template <typename CostResourceType, typename... ResourceTypes>
        static Distance solve(const Graph<ResourceComposition<ResourceTypes...>>& graph_,
                              const std::vector<size_t>& target_ids, size_t cost_index = 0,
                              bool forward = true) {
            // Distance from source to each node
            Distance distance(target_ids, graph_);

            // Prepare distance table
            std::vector<ArcRelaxation> arc_relations;
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
                arc->expander->expand(*arc->origin->resource, &resource);
                // fetch the new value of the cost resource
                const CostResourceType& cost_resource =
                    resource.template get_resource_component<CostResourceType>(cost_index);
                double cost = cost_resource.get_value();
                // compute the weight, i.e., cost difference
                arc_relations.emplace_back(arc->origin->id,
                                           arc->destination->id,
                                           cost - origin_cost);
            }

            if (!forward) {
                std::ranges::reverse(arc_relations);
            }

            // Relax arcs |N|-1 times, on |N| iteration -> check for negative-weight cycles
            const size_t nodes_size = graph_.get_node_ids().size();
            for (size_t i = 0; i < nodes_size; ++i) {
                bool modified = false;
                bool last_iteration = (i == nodes_size - 1);
                for (const auto& relax : arc_relations) {
                    if (forward &&
                        distance[relax.origin_id] + relax.weight < distance[relax.destination_id]) {
                        distance[relax.destination_id] = distance[relax.origin_id] + relax.weight;
                        modified = true;
                    } else if (!forward && distance[relax.destination_id] + relax.weight <
                                               distance[relax.origin_id]) {
                        distance[relax.origin_id] = distance[relax.destination_id] + relax.weight;
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
