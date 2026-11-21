// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <list>

#include "rcspp/label/label.hpp"

/// @brief The parts a labelling strategy is built from, and the concepts they meet.
///
/// A search is built from a schedule, an admission rule, a retention and an observer (each a
/// template on the resource type and the search's direction); a strategy may also name an
/// interleave and a driver (plain types). The library's parts are in the
/// subdirectories of `rcspp/algorithm/labelling/parts/`, one per kind of part, each with its
/// concept; a part of one's own meets the same concept. How the parts work together: the comment
/// at the top of `labelling/search.hpp`.
namespace rcspp::parts {

/// @brief One label and where it is stored at its node.
///
/// @tparam ResourceType The resource type carried by labels.
template <typename ResourceType>
struct StoredLabel {
        /// The label; null: no label (what a schedule's @c next() returns once none is left).
        Label<ResourceType>* label = nullptr;
        /// @brief Its position in its node's container.
        typename std::list<Label<ResourceType>*>::iterator position;
};

}  // namespace rcspp::parts
