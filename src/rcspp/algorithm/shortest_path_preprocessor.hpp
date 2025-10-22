// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/preprocessor.hpp"
#include "rcspp/algorithm/bellman_ford_algorithm.hpp"

namespace rcspp {

template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class ShortestPathPreprocessor : public Preprocessor<ResourceType> {
    public:
        ShortestPathPreprocessor(Graph<ResourceType>* graph, double upper_bound)
            : Preprocessor<ResourceType>(graph), upper_bound_(upper_bound) {
            dist_from_sources_ = BellmanFordAlgorithm::solve(*graph, graph->get_source_node_ids());
            dist_to_sinks_ = BellmanFordAlgorithm::solve(*graph, graph->get_sink_node_ids(), false);
        }

    protected:
        Distance dist_from_sources_, dist_to_sinks_;
        double upper_bound_;

        bool remove_arc(const Arc<ResourceType>& arc) override {
            if (dist_from_sources_.at(arc.destination->id) + arc.cost +
                    dist_to_sinks_.at(arc.origin->id) >
                upper_bound_) {
                return true;
            }
            return false;
        }
};
}  // namespace rcspp
