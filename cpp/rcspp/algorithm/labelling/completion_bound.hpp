// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"
#include "rcspp/preprocessor/bellman_ford_algorithm.hpp"
#include "rcspp/resource/base/resource.hpp"

namespace rcspp::detail {

/// @brief The completion bound: for each node, a lower bound on the cost a label there still has
///        to pay to become a complete path.
///
/// Empty means no bound: it reads -infinity, so a prune reading it prunes nothing, and its order
/// term reads 0, as for a negative-cost cycle.
///
/// @tparam ResourceType The resource type carried by labels.
template <typename ResourceType>
struct CompletionBound {
        /// @brief The bound at each node position; empty when there is none.
        std::vector<double> by_position;

        /// @brief The bound at @p label's node: -infinity where the relaxation found a
        ///        negative-cost cycle or no bound was computed, so that a prune reading it prunes
        ///        nothing.
        [[nodiscard]] double operator()(const Label<ResourceType>& label) const {
            return by_position.empty() ? -std::numeric_limits<double>::infinity()
                                       : by_position[label.get_end_node()->pos()];
        }

        /// @brief The bound as a term of an order's key (cost plus bound, A*'s order): -infinity
        ///        counts as 0, so a bound that failed orders labels by their cost alone.
        [[nodiscard]] double order_term(const Label<ResourceType>& label) const {
            const double bound = (*this)(label);
            return bound == -std::numeric_limits<double>::infinity() ? 0.0 : bound;
        }
};

/// @brief For each node, a lower bound on the cost still to pay from it to an end node of
///        @p Direction.
///
/// One Bellman–Ford run, from the end nodes, over each arc's share of the objective: the cost a
/// label has after it is extended along the arc, minus the cost it had before. That is the cost
/// the search compares, so the bound holds whichever component or components the cost function
/// reads. Dropping the resource constraints can only lower a path's cost, so the relaxed distance
/// is a lower bound. The share is the same from any label only under an additive cost, which
/// every backward and bidirectional solve checks. Under another cost the share is computed from
/// the origin node's resource values and the result is not a bound: only a forward search's
/// order reads it then (A*), and an order needs no bound to stay exact.
///
/// A node from which no end node of @p Direction can be reached is bounded by +infinity. A
/// negative-cost cycle leaves no finite bound: the result is then -infinity everywhere, which a
/// prune reads as "prune nothing" and an order as 0 (@ref CompletionBound::order_term).
///
/// @tparam Direction    The search's direction policy: the bound is to its end nodes.
/// @tparam ResourceType The resource type carried by labels.
/// @param graph The graph.
/// @return The bound, by node position.
template <typename Direction, typename ResourceType>
CompletionBound<ResourceType> compute_completion_bound(const Graph<ResourceType>& graph) {
    // Each live arc's share of the cost. Extends the origin's start value along the arc, as a
    // label is extended; under an additive cost the difference does not depend on the value
    // extended from.
    std::vector<BellmanFordAlgorithm::WeightedArc> shares;
    std::unique_ptr<Resource<ResourceType>> start;
    std::unique_ptr<Resource<ResourceType>> extended;
    graph.for_each_arc([&](const Arc<ResourceType>& arc) {
        if (arc.extender == nullptr || arc.origin->resource == nullptr ||
            arc.destination->resource == nullptr) {
            return;
        }
        if (start == nullptr) {
            start = std::make_unique<Resource<ResourceType>>(*arc.origin->resource);
            extended = std::make_unique<Resource<ResourceType>>(*arc.destination->resource);
        }
        // reset() adopts a node's function objects without cloning them, as a pooled label does.
        start->reset(*arc.origin->resource);
        extended->reset(*arc.destination->resource);
        arc.extender->extend(*start, extended.get());
        shares.push_back({.origin_id = arc.origin->id,
                          .destination_id = arc.destination->id,
                          .weight = extended->get_cost() - start->get_cost()});
    });

    CompletionBound<ResourceType> bound;
    bound.by_position.assign(graph.get_number_of_nodes(), std::numeric_limits<double>::infinity());
    try {
        // From the end nodes against the search's direction: to the sinks going forward.
        const Distance distance =
            BellmanFordAlgorithm::solve_weighted(graph,
                                                 Direction::end_nodes(graph),
                                                 shares,
                                                 /*forward=*/Direction::backward);
        for (const size_t node_id : graph.get_node_ids()) {
            if (const auto it = distance.find(node_id); it != distance.end()) {
                bound.by_position.at(graph.get_node(node_id)->pos()) = it->second;
            }
        }
    } catch (const std::runtime_error&) {
        std::ranges::fill(bound.by_position, -std::numeric_limits<double>::infinity());
    }
    return bound;
}

}  // namespace rcspp::detail
