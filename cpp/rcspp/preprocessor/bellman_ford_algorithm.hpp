// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <unordered_map>
#include <vector>

#include "rcspp/graph/graph.hpp"
#include "rcspp/resource/concrete/numerical_resource.hpp"
#include "rcspp/resource/resource_traits.hpp"

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

        /// @brief Run the Bellman–Ford relaxation loop on a pre-built arc list.
        ///
        /// @param distance       Distance map initialised with target nodes set to 0.
        /// @param arc_relaxations  Arcs with weights; already reversed if backward.
        /// @param forward        If false, relaxation propagates from destination to origin.
        /// @param nodes_size     Number of nodes (loop iteration bound).
        static void run_relaxations(Distance& distance, std::vector<ArcRelaxation>& arc_relaxations,
                                    bool forward, size_t nodes_size) {
            for (size_t i = 0; i < nodes_size; ++i) {
                bool modified = false;
                bool last_iteration = (i == nodes_size - 1);
                for (const auto& relax : arc_relaxations) {
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
                    break;
                }
            }
        }

    public:
        /// @brief Compute shortest paths using @p arc.cost as the arc weight.
        ///
        /// Does not require a cost resource type — works with any graph composition.
        /// @p forward computes distances from the targets; !@p forward computes
        /// distances to the targets.
        ///
        /// @note @p forward has no default value to prevent ambiguity with the
        ///   @p cost_index overload when called with a plain @c bool argument.
        ///
        /// @param graph_      The graph.
        /// @param target_ids  Source nodes (forward) or sink nodes (!forward).
        /// @param forward     Direction of the shortest-path computation.
        /// @return Distance map keyed by node ID.
        template <typename... ResourceTypes>
        static Distance solve(const Graph<ResourceTypeComposition<ResourceTypes...>>& graph_,
                              const std::vector<size_t>& target_ids, bool forward) {
            Distance distance(target_ids, graph_);
            std::vector<ArcRelaxation> arc_relaxations;
            graph_.for_each_arc([&](const auto& arc) {
                arc_relaxations.emplace_back(arc.origin->id, arc.destination->id, arc.cost);
            });
            if (!forward) {
                std::ranges::reverse(arc_relaxations);
            }
            run_relaxations(distance, arc_relaxations, forward, graph_.get_nodes_size());
            return distance;
        }

        /// @brief Compute shortest paths from/to target nodes.
        ///
        /// When @p cost_index is set, the arc weight is derived from the
        /// @p CostResourceType resource component at that index (via arc extension).
        /// When @p cost_index is @c std::nullopt, @p arc.cost is used directly.
        ///
        /// @tparam CostResourceType  Numerical resource type used when cost_index is set.
        /// @param graph_      The graph.
        /// @param target_ids  Source nodes (forward) or sink nodes (!forward).
        /// @param cost_index  Resource component index for arc weights, or nullopt.
        /// @param forward     Direction of the shortest-path computation.
        /// @return Distance map keyed by node ID.
        template <typename CostResourceType = RealResource, typename... ResourceTypes>
            requires is_numerical_resource_v<CostResourceType>
        static Distance solve(const Graph<ResourceTypeComposition<ResourceTypes...>>& graph_,
                              const std::vector<size_t>& target_ids,
                              std::optional<size_t> cost_index = std::nullopt,
                              bool forward = true) {
            if (!cost_index.has_value()) {
                return solve(graph_, target_ids, forward);
            }

            // compute shortest paths from any of the given targets to all nodes (forward) or from
            // all nodes to any of the given targets (backward)
            Distance distance(target_ids, graph_);

            std::vector<ArcRelaxation> arc_relaxations;
            graph_.for_each_arc([&](const auto& arc) {
                // get the origin cost of the cost resource
                const auto& origin_cost_resource =
                    arc.origin->resource->template get_component<CostResourceType>(
                        cost_index.value());
                double origin_cost = origin_cost_resource.get_value().get_value();
                // extend the resource
                Resource<ResourceTypeComposition<ResourceTypes...>> resource(
                    *arc.destination->resource);
                arc.extender->extend(*arc.origin->resource, &resource);
                // fetch the new value of the cost resource
                const auto& cost_resource =
                    resource.template get_component<CostResourceType>(cost_index.value());
                double cost = cost_resource.get_value().get_value();
                // compute the weight, i.e., cost difference
                arc_relaxations.emplace_back(arc.origin->id,
                                             arc.destination->id,
                                             cost - origin_cost);
            });

            // In backward shortest path computation, we need to reverse the order of arc
            // relaxations to ensure that relaxation proceeds from destination to origin, correctly
            // propagating distances from all nodes to the target(s). The goal is to be more
            // efficient if the arcs are correctly ordered
            if (!forward) {
                std::ranges::reverse(arc_relaxations);
            }

            run_relaxations(distance, arc_relaxations, forward, graph_.get_nodes_size());
            return distance;
        }
};

}  // namespace rcspp
