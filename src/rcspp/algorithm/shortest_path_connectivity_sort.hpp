// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <unordered_map>

#include "rcspp/algorithm/bellman_ford_algorithm.hpp"
#include "rcspp/algorithm/connectivity_matrix.hpp"
#include "rcspp/resource/concrete/numerical_resource.hpp"

namespace rcspp {
template <typename CostResourceType = RealResource, typename... ResourceTypes>
class ShortestPathConnectivitySort {
    public:
        explicit ShortestPathConnectivitySort(
            Graph<ResourceComposition<ResourceTypes...>>* graph,
            ConnectivityMatrix<ResourceComposition<ResourceTypes...>>* cm,
            std::optional<size_t> cost_index = std::nullopt) {  // use default cost if nullopt
            // compute shortest path distances from sources and to sinks
            bool distances_computed = true;
            Distance dist_from_sources;
            Distance dist_to_sinks;
            try {
                dist_from_sources = BellmanFordAlgorithm::solve<CostResourceType, ResourceTypes...>(
                    *graph,
                    graph->get_source_node_ids(),
                    cost_index);
                dist_to_sinks = BellmanFordAlgorithm::solve<CostResourceType, ResourceTypes...>(
                    *graph,
                    graph->get_sink_node_ids(),
                    cost_index,
                    false);
            } catch (const std::runtime_error& e) {
                // unable to compute distances (negative cycle)
                distances_computed = false;
            }

            // compute reachability
            const auto connectivity_map =
                cm->compute_connectivity();  // source_id -> vector<reachable_ids>
            std::unordered_map<size_t, size_t> reachable_count;
            std::unordered_map<size_t, size_t> reverse_reachable_count;
            for (const auto& p : connectivity_map) {
                reachable_count[p.first] = p.second.size();
                for (size_t tgt : p.second) {
                    reverse_reachable_count[tgt] += 1;
                }
            }

            // order based on shortest path distances
            graph->sort_nodes([&](const Node<ResourceComposition<ResourceTypes...>>* node1,
                                  const Node<ResourceComposition<ResourceTypes...>>* node2) {
                // sources first
                if (node1->source ^ node2->source) {
                    return node1->source;
                }
                // sinks last
                if (node1->sink ^ node2->sink) {
                    return node2->sink;
                }

                // --- connectivity heuristics ---
                // 1) connectivity asymmetry: if node1 -> node2 but not reverse, prefer node1
                const bool n1_to_n2 = cm->is_connected(node1->id, node2->id);
                const bool n2_to_n1 = cm->is_connected(node2->id, node1->id);
                if (n1_to_n2 != n2_to_n1) {
                    return n1_to_n2;
                }

                // 2) reachable count heuristic: fewer reachable nodes => more constrained =>
                // earlier
                const size_t rc1 = reachable_count[node1->id];
                const size_t rc2 = reachable_count[node2->id];
                if (rc1 != rc2) {
                    return rc1 < rc2;
                }

                // 3) reverse reachable count: fewer nodes that can reach this node => earlier
                const size_t rrc1 = reverse_reachable_count[node1->id];
                const size_t rrc2 = reverse_reachable_count[node2->id];
                if (rrc1 != rrc2) {
                    return rrc1 < rrc2;
                }

                // fallback to distance from sources (increasing), then sinks (decreasing)
                if (distances_computed) {
                    double dist_src1 = dist_from_sources.at(node1->id);
                    double dist_src2 = dist_from_sources.at(node2->id);
                    if (std::fabs(dist_src1 - dist_src2) >
                        1e-3) {  // NOLINT (readability-magic-numbers)
                        return dist_src1 < dist_src2;
                    }
                    double dist_sink1 = dist_to_sinks.at(node1->id);
                    double dist_sink2 = dist_to_sinks.at(node2->id);
                    if (std::fabs(dist_sink1 - dist_sink2) >
                        1e-3) {  // NOLINT (readability-magic-numbers)
                        return dist_sink1 > dist_sink2;
                    }
                }

                // break ties by id
                return node1->id < node2->id;
            });
        }
};
}  // namespace rcspp
