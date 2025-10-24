// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/bellman_ford_algorithm.hpp"
#include "rcspp/algorithm/preprocessor.hpp"
#include "rcspp/resource/concrete/real_resource.hpp"

namespace rcspp {

template <typename CostResourceType, typename... ResourceTypes>
class ShortestPathPreprocessor : public Preprocessor<ResourceComposition<ResourceTypes...>> {
    public:
        ShortestPathPreprocessor(Graph<ResourceComposition<ResourceTypes...>>* graph,
                                 double upper_bound, size_t cost_index = 0)
            : Preprocessor<ResourceComposition<ResourceTypes...>>(graph),
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
                } catch (const std::runtime_error& e) {
                    Preprocessor<ResourceComposition<ResourceTypes...>>::disable_preprocessing_ =
                        true;
                }
            }
        }

    protected:
        Distance dist_from_sources_, dist_to_sinks_;
        double upper_bound_;

        bool remove_arc(const Arc<ResourceComposition<ResourceTypes...>>& arc) override {
            return static_cast<bool>(dist_from_sources_.at(arc.origin->id) + arc.cost +
                                         dist_to_sinks_.at(arc.destination->id) >
                                     upper_bound_);
        }
};
}  // namespace rcspp
