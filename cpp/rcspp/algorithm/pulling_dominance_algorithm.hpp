// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/labelling/labelling_algorithm.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/pull_schedule.hpp"
#include "rcspp/algorithm/labelling/search.hpp"
#include "rcspp/label/label_list.hpp"

namespace rcspp {

/// @brief Extends labels by pulling them into one node at a time: each iteration visits the next
///        node in the sorted order, extends into it every label waiting at its predecessors, and
///        filters what arrived, up to @c max_extended_labels_per_node labels per node and visit.
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>>
    requires ResourceTypeConcept<ResourceType>
class PullingDominanceAlgorithm
    : public detail::LabellingAlgorithm<
          ResourceType, LabelContainerType, ForwardDirection,
          detail::SearchParts<parts::PullSchedule<ResourceType, ForwardDirection>>> {
        using Base = detail::LabellingAlgorithm<
            ResourceType, LabelContainerType, ForwardDirection,
            detail::SearchParts<parts::PullSchedule<ResourceType, ForwardDirection>>>;

    public:
        using Base::Base;
};
}  // namespace rcspp
