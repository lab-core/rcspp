// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/label_buckets.hpp"
#include "rcspp/algorithm/labelling/labelling_algorithm.hpp"
#include "rcspp/algorithm/labelling/node_schedules.hpp"
#include "rcspp/algorithm/labelling/search.hpp"

namespace rcspp {

/// @brief Extends labels node by node: all the waiting labels of one node, then the next node's,
///        sweeping the nodes in their sorted order and starting again, up to
///        @c max_extended_labels_per_node labels per node and visit.
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>>
    requires ResourceTypeConcept<ResourceType>
class PushingDominanceAlgorithm
    : public detail::LabellingAlgorithm<
          ResourceType, LabelContainerType, ForwardDirection,
          detail::Preset<parts::SweepSchedule<ResourceType, ForwardDirection>>> {
        using Base = detail::LabellingAlgorithm<
            ResourceType, LabelContainerType, ForwardDirection,
            detail::Preset<parts::SweepSchedule<ResourceType, ForwardDirection>>>;

    public:
        using Base::Base;
};
}  // namespace rcspp
