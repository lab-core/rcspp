// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

#include "rcspp/preprocessor/bellman_ford_algorithm.hpp"
#include "rcspp/preprocessor/preprocessor.hpp"
#include "rcspp/resource/composition/extender_composition.hpp"
#include "rcspp/resource/concrete/numerical_resource.hpp"
#include "rcspp/resource/resource_traits.hpp"

namespace rcspp {

/// @brief Preprocessor that removes arcs whose cost cannot be part of any optimal path.
///
/// Uses Bellman-Ford shortest-path distances from sources and to sinks to prune arcs:
/// an arc `(u, v)` with cost `c` is removed when
/// `dist_from_source[u] + c + dist_to_sink[v] > upper_bound`.
///
/// Preprocessing is automatically disabled when `upper_bound` is infinite or when
/// Bellman-Ford detects a negative-cost cycle.
///
/// @tparam CostResourceType Numerical resource type used to measure arc cost; must
///         satisfy `is_numerical_resource_v`.  Defaults to `RealResource`.
/// @tparam ResourceTypes    Remaining resource types that form the composition.
template <typename CostResourceType = RealResource, typename... ResourceTypes>
    requires is_numerical_resource_v<CostResourceType>
class ShortestPathPreprocessor final
    : public Preprocessor<ResourceTypeComposition<ResourceTypes...>> {
    public:
        /// @brief Constructs the preprocessor and runs Bellman-Ford in both directions.
        ///
        /// If `upper_bound` is infinite, preprocessing is disabled.  If Bellman-Ford
        /// detects a negative cycle, preprocessing is also disabled.
        ///
        /// @param graph       Non-owning pointer to the graph to preprocess.
        /// @param upper_bound Known upper bound on the total path cost.  Arcs that
        ///                    cannot belong to a path with cost at most this value are
        ///                    removed.
        /// @param cost_index  Index of the cost component within the resource
        ///                    composition.  Defaults to `0`.
        ShortestPathPreprocessor(Graph<ResourceTypeComposition<ResourceTypes...>>* graph,
                                 double upper_bound, size_t cost_index = 0)
            : Preprocessor<ResourceTypeComposition<ResourceTypes...>>(graph),
              graph_(graph),
              cost_index_(cost_index),
              upper_bound_(upper_bound) {
            if (std::isinf(upper_bound)) {
                Preprocessor<ResourceTypeComposition<ResourceTypes...>>::disable_preprocessing_ =
                    true;
            } else {
                try {
                    dist_from_sources_ =
                        BellmanFordAlgorithm::solve<CostResourceType, ResourceTypes...>(
                            *graph,
                            graph->get_source_node_ids(),
                            cost_index);
                    dist_to_sinks_ =
                        BellmanFordAlgorithm::solve<CostResourceType, ResourceTypes...>(
                            *graph,
                            graph->get_sink_node_ids(),
                            cost_index,
                            false);
                } catch (const std::runtime_error&) {
                    Preprocessor<
                        ResourceTypeComposition<ResourceTypes...>>::disable_preprocessing_ = true;
                }
            }
        }

    private:
        Distance dist_from_sources_, dist_to_sinks_;
        size_t cost_index_;
        double upper_bound_;
        // pointer to the graph for traversal and connectivity queries
        Graph<ResourceTypeComposition<ResourceTypes...>>* graph_;

        bool remove_arc(const Arc<ResourceTypeComposition<ResourceTypes...>>& arc) override {
            const auto& arc_cost_extender =
                arc.extender->template get_component<CostResourceType>(cost_index_);
            double arc_cost = arc_cost_extender.get_value().get_value();
            return dist_from_sources_.at(arc.origin->id) + arc_cost +
                       dist_to_sinks_.at(arc.destination->id) >
                   upper_bound_;
        }
};
}  // namespace rcspp
