// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

#include "rcspp/algorithm/bellman_ford_algorithm.hpp"
#include "rcspp/algorithm/preprocessor.hpp"
#include "rcspp/resource/concrete/real_resource.hpp"

namespace rcspp {

template <typename CostResourceType = RealResource, typename... ResourceTypes>
class ShortestPathPreprocessor final : public Preprocessor<ResourceComposition<ResourceTypes...>> {
    public:
        ShortestPathPreprocessor(Graph<ResourceComposition<ResourceTypes...>>* graph,
                                 double upper_bound, size_t cost_index = 0)
            : Preprocessor<ResourceComposition<ResourceTypes...>>(graph),
              graph_(graph),
              upper_bound_(upper_bound) {
            if (std::isinf(upper_bound)) {
                Preprocessor<ResourceComposition<ResourceTypes...>>::disable_preprocessing_ = true;
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
                    Preprocessor<ResourceComposition<ResourceTypes...>>::disable_preprocessing_ =
                        true;
                }
            }
        }

    private:
        Distance dist_from_sources_, dist_to_sinks_;
        double upper_bound_;
        // pointer to the graph for traversal and connectivity queries
        Graph<ResourceComposition<ResourceTypes...>>* graph_;

        bool remove_arc(const Arc<ResourceComposition<ResourceTypes...>>& arc) override {
            return dist_from_sources_.at(arc.origin->id) + arc.cost +
                       dist_to_sinks_.at(arc.destination->id) >
                   upper_bound_;
        }
};
}  // namespace rcspp
