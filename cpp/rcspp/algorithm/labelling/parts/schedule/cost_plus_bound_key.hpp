// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/parts/extension_outcome.hpp"
#include "rcspp/algorithm/labelling/parts/stored_label.hpp"
#include "rcspp/label/label.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp::parts {

/// @brief Orders labels by their cost plus their completion bound, g + h: A*'s order.
///
/// The bound enters as its order term (@c detail::CompletionBound::order_term), which counts a
/// failed bound (-infinity, a negative-cost cycle) as 0, so an order never collapses.
struct CostPlusBoundKey {
        /// @brief Whether the key reads the search's completion bound.
        static constexpr bool reads_completion_bound = true;

        template <typename ResourceType>
        [[nodiscard]] double operator()(const Label<ResourceType>& label,
                                        const detail::SearchContext<ResourceType>& context) const {
            return label.get_cost() + context.completion_bound->order_term(label);
        }
};

}  // namespace rcspp::parts
