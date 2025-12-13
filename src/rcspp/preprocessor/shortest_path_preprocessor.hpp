// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

#include "rcspp/preprocessor/bellman_ford_algorithm.hpp"
#include "rcspp/preprocessor/preprocessor.hpp"
#include "rcspp/resource/concrete/numerical_resource.hpp"

namespace rcspp {

template <typename CostResourceType = RealResource, typename... ResourceTypes>
class ShortestPathPreprocessor final
    : public Preprocessor<ResourceBaseComposition<ResourceTypes...>> {
    public:
        ShortestPathPreprocessor(Graph<ResourceBaseComposition<ResourceTypes...>>* graph,
                                 double upper_bound, size_t cost_index = 0)
            : Preprocessor<ResourceBaseComposition<ResourceTypes...>>(graph),
              graph_(graph),
              cost_index_(cost_index),
              upper_bound_(upper_bound) {
            if (std::isinf(upper_bound)) {
                Preprocessor<ResourceBaseComposition<ResourceTypes...>>::disable_preprocessing_ =
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
                        ResourceBaseComposition<ResourceTypes...>>::disable_preprocessing_ = true;
                }
            }
        }

    private:
        Distance dist_from_sources_, dist_to_sinks_;
        size_t cost_index_;
        double upper_bound_;
        // pointer to the graph for traversal and connectivity queries
        Graph<ResourceBaseComposition<ResourceTypes...>>* graph_;

        bool remove_arc(const Arc<ResourceBaseComposition<ResourceTypes...>>& arc) override {
            const CostResourceType& arc_cost_extender =
                arc.extender->template get_component<CostResourceType>(cost_index_);
            double arc_cost = arc_cost_extender.get_value();
            return dist_from_sources_.at(arc.origin->id) + arc_cost +
                       dist_to_sinks_.at(arc.destination->id) >
                   upper_bound_;
        }
};
}  // namespace rcspp
