// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/half_way_policy.hpp"

namespace rcspp {

/// @brief What a caller can reach on a bidirectional search beyond @c Algorithm: the controller
///        that moves its half-way point between solves.
///
/// The class that searches bidirectionally is internal; @ref as_bidirectional finds this
/// interface on the algorithm @c ResourceGraph::create_algorithm returned.
///
/// @tparam ResourceType The resource type carried by labels.
template <typename ResourceType>
class BidirectionalSearch {
    public:
        virtual ~BidirectionalSearch() = default;

        /// @brief The controller that moves `H` between solves when `dynamic_half_way` is set.
        [[nodiscard]] virtual const HalfWayController& half_way_controller() const = 0;

        /// @brief Mutable access, to freeze the controller, reset what it learned, or tune it.
        [[nodiscard]] virtual HalfWayController& half_way_controller() = 0;
};

/// @brief The bidirectional interface of @p algorithm, or null when it is not a bidirectional
///        search.
///
/// @param algorithm An algorithm, typically from @c ResourceGraph::create_algorithm.
/// @return @p algorithm as a @ref BidirectionalSearch, or null.
template <typename ResourceType, typename LabelContainerType>
[[nodiscard]] BidirectionalSearch<ResourceType>* as_bidirectional(
    Algorithm<ResourceType, LabelContainerType>* algorithm) {
    return dynamic_cast<BidirectionalSearch<ResourceType>*>(algorithm);
}

/// @copydoc as_bidirectional
template <typename ResourceType, typename LabelContainerType>
[[nodiscard]] const BidirectionalSearch<ResourceType>* as_bidirectional(
    const Algorithm<ResourceType, LabelContainerType>* algorithm) {
    return dynamic_cast<const BidirectionalSearch<ResourceType>*>(algorithm);
}

/// @brief Reduces a bidirectional `SolveResult` to what @ref HalfWayController reads.
///
/// `exact` is `COMPLETE` and untrimmed by memory pressure, and not @p truncated. The status alone
/// cannot say the last part: a per-node extension quota truncates a bidirectional search while it
/// still reports `COMPLETE`, so a caller who set `num_labels_to_extend_by_node` says so here.
///
/// @param result    A bidirectional solve's result.
/// @param truncated Whether the caller capped the search in a way the status does not show.
/// @return The observation.
[[nodiscard]] inline HalfWayObservation half_way_observation(const SolveResult& result,
                                                             bool truncated = false) {
    return HalfWayObservation{
        .forward_labels = result.forward_labels,
        .backward_labels = result.backward_labels,
        .joined_paths = result.number_of_joined_paths,
        .solutions = result.solutions.size(),
        .bounded = result.bounded_by_half_way,
        .exact = result.status == AlgorithmStatus::COMPLETE && !result.memory_pressure_triggered &&
                 !truncated,
    };
}

}  // namespace rcspp
