// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <list>

#include "rcspp/label/label.hpp"

/// @brief The parts a labelling strategy is built from, and the concepts they meet.
///
/// A strategy (@c rcspp::strategy) names its schedule, and may name its admission rule, its
/// retention, its observer and its interleave; each is given the resource type and the search's
/// direction. The library's parts are in the headers of `rcspp/algorithm/labelling/parts/`, one
/// per kind of part; a part of one's own meets the same concept.
namespace rcspp::parts {

/// @brief One label and where it is stored at its node.
///
/// @tparam ResourceType The resource type carried by labels.
template <typename ResourceType>
struct StoredLabel {
        Label<ResourceType>* label = nullptr;  ///< none: no label
        /// @brief Its position in its node's container.
        typename std::list<Label<ResourceType>*>::iterator position;
};

}  // namespace rcspp::parts
