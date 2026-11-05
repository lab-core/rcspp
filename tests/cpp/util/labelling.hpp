// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/rcspp.hpp"

namespace test_util {

/// @brief The class that searches for @p Strategy in @p Direction, for a test that wants it by
///        type: the class @c ResourceGraph::create_algorithm builds forward or backward.
///
/// @tparam Strategy           A labelling strategy (@c rcspp::strategy).
/// @tparam ResourceType       The resource type carried by labels.
/// @tparam LabelContainerType The per-node container of the params.
/// @tparam Direction          @c rcspp::ForwardDirection (the default) or
///                            @c rcspp::BackwardDirection.
template <typename Strategy, typename ResourceType,
          typename LabelContainerType = rcspp::LabelList<ResourceType>,
          typename Direction = rcspp::ForwardDirection>
using Labelling =
    rcspp::detail::LabellingAlgorithm<ResourceType, LabelContainerType, Direction,
                                      rcspp::detail::preset_t<Strategy, ResourceType, Direction>>;

/// @brief The class that searches bidirectionally for @p Strategy, with a
///        @p CriticalResourceType clock.
template <typename Strategy, typename ResourceType,
          typename LabelContainerType = rcspp::LabelList<ResourceType>,
          typename CriticalResourceType = rcspp::RealResource>
using BidirectionalLabelling = rcspp::detail::BidirectionalLabellingAlgorithm<
    ResourceType, LabelContainerType, CriticalResourceType,
    rcspp::detail::preset_t<Strategy, ResourceType, rcspp::ForwardDirection>,
    rcspp::detail::preset_t<Strategy, ResourceType, rcspp::BackwardDirection>,
    rcspp::detail::interleave_of_t<Strategy>>;

}  // namespace test_util
