// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/label_buckets.hpp"
#include "rcspp/algorithm/labelling/labelling_algorithm.hpp"
#include "rcspp/algorithm/labelling/schedules.hpp"
#include "rcspp/algorithm/labelling/search.hpp"

namespace rcspp {

namespace detail {

/// @brief The search of @ref SimpleDominanceAlgorithm in either direction: one search on a
///        first-in, first-out schedule.
///
/// @tparam ResourceType       The resource type carried by labels.
/// @tparam LabelContainerType The per-node container its params hold.
/// @tparam Direction          @ref ForwardDirection or @ref BackwardDirection.
template <typename ResourceType, typename LabelContainerType, typename Direction>
using DirectedSimpleDominanceAlgorithm =
    LabellingAlgorithm<ResourceType, LabelContainerType, Direction,
                       Preset<parts::FifoSchedule<ResourceType>>>;

/// @brief The backward search of @ref SimpleDominanceAlgorithm.
///
/// @c create_algorithm<SimpleDominanceAlgorithm> builds it for @c SearchDirection::Backward.
template <typename ResourceType, typename LabelContainerType>
using BackwardSimpleDominanceAlgorithm =
    DirectedSimpleDominanceAlgorithm<ResourceType, LabelContainerType, BackwardDirection>;

}  // namespace detail

/// @brief Extends labels first in, first out, up to @c max_extended_labels_per_node labels per
///        node.
///
/// Searches forward, backward or in both directions, as its params' @c direction asks (through
/// @c ResourceGraph::create_algorithm or @c ResourceGraph::solve).
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>>
    requires ResourceTypeConcept<ResourceType>
class SimpleDominanceAlgorithm
    : public detail::DirectedSimpleDominanceAlgorithm<ResourceType, LabelContainerType,
                                                      ForwardDirection> {
        using Base = detail::DirectedSimpleDominanceAlgorithm<ResourceType, LabelContainerType,
                                                              ForwardDirection>;

    public:
        using Base::Base;
};
}  // namespace rcspp
