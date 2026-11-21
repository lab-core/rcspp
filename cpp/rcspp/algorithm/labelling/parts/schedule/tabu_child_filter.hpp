// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/parts/stored_label.hpp"

namespace rcspp::parts {

/// @brief Tabu's rule: a child reached along a soft arc of the search's mask (a tabu arc) is
///        dropped (released, not kept for a later episode), unless every child is: then every
///        child is kept, so the dive can always go on (aspiration by default). The others keep
///        their order.
struct TabuChildFilter {
        template <typename ResourceType>
        void select(std::vector<StoredLabel<ResourceType>>& children,
                    std::vector<StoredLabel<ResourceType>>& rejected,
                    const detail::SearchContext<ResourceType>& context) const {
            const ArcMask* mask = context.arc_mask;
            if (mask == nullptr || mask->empty()) {
                return;
            }
            // The arc a child was extended along: its in-arc forward, its out-arc backward.
            const auto is_tabu = [mask](const StoredLabel<ResourceType>& child) {
                const auto* arc = child.label->get_in_arc() != nullptr ? child.label->get_in_arc()
                                                                       : child.label->get_out_arc();
                return mask->is_soft(arc->id);
            };
            if (std::ranges::all_of(children, is_tabu)) {
                return;
            }
            const auto tabu = std::ranges::stable_partition(
                children,
                [&is_tabu](const StoredLabel<ResourceType>& child) { return !is_tabu(child); });
            rejected.insert(rejected.end(), tabu.begin(), tabu.end());
            children.erase(tabu.begin(), tabu.end());
        }
};

}  // namespace rcspp::parts
