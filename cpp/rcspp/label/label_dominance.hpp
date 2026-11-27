// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>

#include "rcspp/algorithm/dominance_mask.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp::detail {

/// @brief Whether @p lhs dominates @p rhs in @p Direction: forward, by the resources' `operator<=`;
///        backward, by their `back_dominates`. What a label container compares its labels with.
///
/// @tparam Direction    The direction policy supplying the dominance order.
/// @tparam ResourceType The resource type carried by labels.
/// @param lhs The candidate dominating label.
/// @param rhs The label being tested.
/// @return `true` when @p lhs dominates @p rhs in @p Direction.
template <class Direction, class ResourceType>
[[nodiscard]] bool label_dominates(const Label<ResourceType>& lhs, const Label<ResourceType>& rhs) {
    return Direction::template dominates<ResourceType>(lhs, rhs);
}

/// @brief Dominance over the components @p mask leaves in: each applies its own rule, in
///        @p Direction, as `CompositionDominanceFunction` does, whatever composition dominance
///        function the model uses. A resource that is not a composition has one component.
///
/// @tparam Direction    The direction policy supplying the dominance order.
/// @tparam ResourceType The resource type carried by labels.
/// @param lhs  The candidate dominating label.
/// @param rhs  The label being tested.
/// @param mask The components left out.
/// @return `true` when every component left in of @p lhs dominates that of @p rhs.
template <class Direction, class ResourceType>
[[nodiscard]] bool label_dominates(const Label<ResourceType>& lhs, const Label<ResourceType>& rhs,
                                   const DominanceMask& mask) {
    if constexpr (requires { lhs.get_resource().get_components(); }) {
        size_t component = 0;
        return lhs.get_resource().for_each_component_and(
            rhs.get_resource(),
            [&component, &mask](const auto& lhs_component, const auto& rhs_component) {
                if (mask.ignores(component++)) {
                    return true;
                }
                if constexpr (Direction::backward) {
                    return lhs_component.back_dominates(rhs_component);
                } else {
                    return lhs_component <= rhs_component;
                }
            });
    } else {
        return mask.ignores(0) || label_dominates<Direction>(lhs, rhs);
    }
}

}  // namespace rcspp::detail
