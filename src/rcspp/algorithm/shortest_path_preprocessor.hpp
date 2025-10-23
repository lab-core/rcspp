// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/preprocessor.hpp"
#include "rcspp/algorithm/bellman_ford_algorithm.hpp"
#include "rcspp/resource/concrete/real_resource.hpp"

namespace rcspp {

template <typename CostResourceType, typename... ResourceTypes>
class ShortestPathPreprocessor : public Preprocessor<ResourceComposition<ResourceTypes...>> {
    public:
        ShortestPathPreprocessor(Graph<ResourceComposition<ResourceTypes...>>* graph,
                                 double upper_bound, size_t cost_index = 0)
            : Preprocessor<ResourceComposition<ResourceTypes...>>(graph),
              upper_bound_(upper_bound) {
            if (!std::isinf(upper_bound)) {
                dist_from_sources_ =
                    BellmanFordAlgorithm::solve<CostResourceType, ResourceTypes...>(
                        *graph,
                        graph->get_source_node_ids(),
                        cost_index);
                dist_to_sinks_ = BellmanFordAlgorithm::solve<CostResourceType, ResourceTypes...>(
                    *graph,
                    graph->get_sink_node_ids(),
                    cost_index,
                    false);
            }
        }

    protected:
        Distance dist_from_sources_, dist_to_sinks_;
        double upper_bound_;

        bool remove_arc(const Arc<ResourceComposition<ResourceTypes...>>& arc) override {
            if (dist_from_sources_.at(arc.origin->id) + arc.cost +
                    dist_to_sinks_.at(arc.destination->id) >
                upper_bound_) {
                return true;
            }
            return false;
        }
};
}  // namespace rcspp
