// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"
#include "rcspp/preprocessor/bellman_ford_algorithm.hpp"
#include "rcspp/resource/base/resource.hpp"
#include "rcspp/validation/arc_pair_probe.hpp"
#include "rcspp/validation/threshold_probe.hpp"

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
/// is a lower bound, provided each share is too.
///
/// The share is measured once per arc, from the origin's start value. It is a valid term of a
/// lower bound only if the arc never adds less than that from any value a label can hold there:
/// true when the extension adds a fixed amount per arc, false under a floor, a ceiling or a
/// maximum (an @c AdditionExtensionFunction with a @c min_value, a bottleneck), even though such
/// a cost may be declared additive (@c Resource::is_cost_additive). A prune reads the bound only
/// after the solve has tested this by running the extension (@ref find_share_shortfall).
/// Otherwise the result is not a bound, and only an order reads it (A*), which needs no bound to
/// stay exact.
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
    // Each live arc's share of the cost: the origin's start value is extended along the arc, as a
    // label is. Whether a prune may sum the shares is decided elsewhere (see the doc above).
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

/// @brief Where an arc was seen adding less to the cost than its share: the second arc of a pair,
///        extended after the first (@ref find_share_shortfall).
struct ShareShortfall {
        size_t first_origin_id = 0;        ///< the first arc's origin
        size_t first_destination_id = 0;   ///< the first arc's destination
        size_t second_origin_id = 0;       ///< the second arc's origin
        size_t second_destination_id = 0;  ///< the second arc's destination
        double share = 0.0;  ///< what the bound counts for the second arc, from its start value
        double added = 0.0;  ///< what the second arc added to the cost after the first
};

/// @brief Describes @p shortfall in words, for a message: which arcs, and the two amounts.
[[nodiscard]] inline std::string to_string(const ShareShortfall& shortfall) {
    const auto arc = [](size_t origin_id, size_t destination_id) {
        return std::to_string(origin_id) + " -> " + std::to_string(destination_id);
    };
    return "arc " + arc(shortfall.second_origin_id, shortfall.second_destination_id) + " adds " +
           std::to_string(shortfall.share) + " to the cost from its start value, but " +
           std::to_string(shortfall.added) + " after arc " +
           arc(shortfall.first_origin_id, shortfall.first_destination_id);
}

/// @brief The first pair of arcs along which the second arc adds less to the cost than its share
///        (the cost it adds from its origin's start value, as @ref compute_completion_bound
///        measures it); none when every pair passes.
///
/// The completion bound sums shares, so it is a lower bound only if no arc ever adds less than
/// its share. Measured on the composed cost (@c Resource::get_cost), the quantity the bound sums,
/// so that it holds whatever components the cost function reads. One-sided: an arc that adds more
/// than its share leaves the bound valid, only looser. The pairs are the model's own arcs, every
/// arc first and second (@c arc_pair_probe::for_each_arc_pair): a sample, not a proof.
///
/// Costs two passes over the arcs, with three steps per pair.
///
/// @tparam ResourceType The resource type carried by labels.
/// @param graph The graph.
/// @return The first shortfall found, or @c std::nullopt.
template <typename ResourceType>
[[nodiscard]] std::optional<ShareShortfall> find_share_shortfall(const Graph<ResourceType>& graph) {
    const auto* node = threshold_probe::node_with_resource(graph);
    if (node == nullptr) {
        return std::nullopt;
    }
    Resource<ResourceType> start(*node->resource);
    Resource<ResourceType> first(*node->resource);
    Resource<ResourceType> second(*node->resource);
    Resource<ResourceType> second_start(*node->resource);
    Resource<ResourceType> second_alone(*node->resource);
    std::optional<ShareShortfall> shortfall;
    const auto removed = threshold_probe::index_removed_arcs(graph);
    arc_pair_probe::for_each_arc_pair(
        graph,
        removed,
        [&](const Arc<ResourceType>& first_arc, const Arc<ResourceType>& second_arc) {
            if (first_arc.origin->resource == nullptr ||
                first_arc.destination->resource == nullptr ||
                second_arc.origin->resource == nullptr ||
                second_arc.destination->resource == nullptr) {
                return true;
            }
            // reset() adopts a node's function objects without cloning them, as a label does.
            start.reset(*first_arc.origin->resource);
            first.reset(*first_arc.destination->resource);
            first_arc.extender->extend(start, &first);
            second.reset(*second_arc.destination->resource);
            second_arc.extender->extend(first, &second);
            second_start.reset(*second_arc.origin->resource);
            second_alone.reset(*second_arc.destination->resource);
            second_arc.extender->extend(second_start, &second_alone);

            const double share = second_alone.get_cost() - second_start.get_cost();
            const double added = second.get_cost() - first.get_cost();
            if (!std::isfinite(share) || !std::isfinite(added) || added >= share ||
                arc_pair_probe::is_close(added, share)) {
                return true;
            }
            shortfall = ShareShortfall{.first_origin_id = first_arc.origin->id,
                                       .first_destination_id = first_arc.destination->id,
                                       .second_origin_id = second_arc.origin->id,
                                       .second_destination_id = second_arc.destination->id,
                                       .share = share,
                                       .added = added};
            return false;
        });
    return shortfall;
}

}  // namespace rcspp::detail
