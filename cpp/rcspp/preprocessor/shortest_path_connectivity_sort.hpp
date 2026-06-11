// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cmath>
#include <unordered_map>
#include <utility>

#include "rcspp/preprocessor/bellman_ford_algorithm.hpp"
#include "rcspp/preprocessor/connectivity_matrix.hpp"
#include "rcspp/resource/concrete/numerical_resource.hpp"

namespace rcspp {

/// @brief Sorts graph nodes using shortest-path distances and connectivity heuristics.
///
/// Reorders the nodes of a `Graph` in place to improve the efficiency of subsequent
/// label-setting algorithms.  The ordering criterion is applied in priority order:
///
///  1. Source nodes first, sink nodes last.
///  2. Connectivity asymmetry: if `node1` can reach `node2` but not vice versa,
///     `node1` is placed earlier.
///  3. Fewer reachable successors first (more constrained nodes are expanded earlier).
///  4. Fewer reverse-reachable predecessors first.
///  5. Closer to sources (ascending distance from sources), then farther from sinks
///     (descending distance to sinks) when Bellman-Ford distances are available.
///  6. Fewer direct arcs from `node1` to `node2`.
///  7. Tie-break by node id.
///
/// @tparam CostResourceType Numerical resource type used to compute shortest-path
///         distances; must satisfy `is_numerical_resource_v`.  Defaults to
///         `RealResource`.
/// @tparam ResourceTypes    Remaining resource types that form the composition.
template <typename CostResourceType = RealResource, typename... ResourceTypes>
    requires is_numerical_resource_v<CostResourceType>
class ShortestPathConnectivitySort {
    private:
        /// @brief Hash functor for `std::pair<size_t, size_t>` arc keys.
        struct DirectArcKeyHash {
                /// @brief Computes a hash value for a directed arc identified by its
                ///        origin and destination node ids.
                ///
                /// @param key Pair of (origin_id, destination_id).
                /// @return Combined hash value.
                size_t operator()(const std::pair<size_t, size_t>& key) const noexcept {
                    return std::hash<size_t>{}(key.first) ^ (std::hash<size_t>{}(key.second) << 1);
                }
        };

    public:
        /// @brief Constructs the sorter and immediately reorders the graph's nodes.
        ///
        /// Bellman-Ford is run from sources and to sinks to obtain distance maps.  If a
        /// negative cycle is detected, distance-based tie-breaking is skipped.  The
        /// connectivity matrix is used for reachability heuristics.
        ///
        /// @param graph      Non-owning pointer to the graph whose nodes will be sorted.
        /// @param cm         Non-owning pointer to the precomputed connectivity matrix.
        /// @param cost_index Index of the cost component within the resource composition
        ///                   to use for shortest-path distances.  Pass `std::nullopt` to
        ///                   use the default cost component.
        explicit ShortestPathConnectivitySort(  // NOLINT
            Graph<ResourceTypeComposition<ResourceTypes...>>* graph,
            ConnectivityMatrix<ResourceTypeComposition<ResourceTypes...>>* cm,
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

            // Precompute direct arc multiplicities once so the comparator can avoid repeated
            // get_arcs() scans and temporary vector allocations during sort.
            std::unordered_map<std::pair<size_t, size_t>, size_t, DirectArcKeyHash>
                direct_arc_count;
            direct_arc_count.reserve(graph->get_number_of_arcs());
            graph->for_each_arc([&](const auto& arc) {
                ++direct_arc_count[{arc.origin->id, arc.destination->id}];
            });
            auto get_direct_arc_count = [&](size_t origin_id, size_t destination_id) -> size_t {
                auto it = direct_arc_count.find({origin_id, destination_id});
                return it == direct_arc_count.end() ? 0 : it->second;
            };

            // order based on shortest path distances
            graph->sort_nodes([&](const Node<ResourceTypeComposition<ResourceTypes...>>* node1,
                                  const Node<ResourceTypeComposition<ResourceTypes...>>* node2) {
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

                // check if one is the predecessor of the other
                const size_t arcs12 = get_direct_arc_count(node1->id, node2->id);
                const size_t arcs21 = get_direct_arc_count(node2->id, node1->id);
                if (arcs12 != arcs21) {
                    return arcs12 < arcs21;  // less arc going from node1 -> node2
                }

                // break ties by id
                return node1->id < node2->id;
            });
        }
};
}  // namespace rcspp
