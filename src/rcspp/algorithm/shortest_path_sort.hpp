// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/bellman_ford_algorithm.hpp"
#include "rcspp/resource/concrete/real_resource.hpp"

namespace rcspp {
template <typename CostResourceType = RealResource, typename... ResourceTypes>
class ShortestPathSort {
  public:
    explicit ShortestPathSort(Graph<ResourceComposition<ResourceTypes...>>* graph,
                              size_t cost_index = 0) {
      Distance dist_from_sources = BellmanFordAlgorithm::solve<CostResourceType, ResourceTypes...>(
        *graph,
        graph->get_source_node_ids(),
        cost_index);
      Distance dist_to_sinks =
        BellmanFordAlgorithm::solve<CostResourceType, ResourceTypes...>(*graph,
                                                                        graph->get_sink_node_ids(),
                                                                        cost_index,
                                                                        false);

      // order based on shortest path distances
      graph->sort_nodes([&](const Node<ResourceComposition<ResourceTypes...>>* node1,
                            const Node<ResourceComposition<ResourceTypes...>>* node2) {
        // if a source, put last
        if (node1->source && !node2->source) {
          return true;
        }
        if (!node1->source && node2->source) {
          return false;
        }
        // if a sink, put last
        if (node1->sink && !node2->sink) {
          return false;
        }
        if (!node1->sink && node2->sink) {
          return true;
        }
        // otherwise, compare by distance from sources (increasing)
        double dist_src1 = dist_from_sources.at(node1->id);
        double dist_src2 = dist_from_sources.at(node2->id);
        if (abs(dist_src1 - dist_src2) > 1e-3) {  // NOLINT (readability-magic-numbers)
          return dist_src1 < dist_src2;
        }
        // break ties by distance to sinks (decreasing)
        return dist_to_sinks.at(node1->id) > dist_to_sinks.at(node2->id);
      });
    }
};
}  // namespace rcspp
