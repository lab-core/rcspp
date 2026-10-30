// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <cstddef>
#include <list>

#include "rcspp/label/label.hpp"

namespace rcspp::parts {

/// @brief Where the labels a search keeps live, and what an admission rule may do to a node.
///
/// @c node gives the view an admission rule reads and evicts through; @c store keeps a label at
/// its node and returns its position there; @c erase takes a stored label out.
/// @c records_at_take says whether solutions are recorded when an end-node label is taken,
/// because nothing stays at the end nodes to read them from at the end.
///
/// @tparam RetentionType The retention.
/// @tparam ResourceType  The resource type carried by labels.
template <typename RetentionType, typename ResourceType>
concept Retention = requires(RetentionType retention, size_t pos, Label<ResourceType>* label,
                             const typename std::list<Label<ResourceType>*>::iterator& position) {
    retention.node(pos);
    {
        retention.store(pos, label)
    } -> std::same_as<typename std::list<Label<ResourceType>*>::iterator>;
    retention.erase(position);
    { RetentionType::records_at_take } -> std::convertible_to<bool>;
};

}  // namespace rcspp::parts
