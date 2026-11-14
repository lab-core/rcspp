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

// ─── Best first ─────────────────────────────────────────────────────────────────────────────

/// @brief Orders labels by their cost.
struct CostKey {
        /// @brief Whether the key reads the search's completion bound.
        static constexpr bool reads_completion_bound = false;

        template <typename ResourceType>
        [[nodiscard]] double operator()(
            const Label<ResourceType>& label,
            const detail::SearchContext<ResourceType>& /*context*/) const {
            return label.get_cost();
        }
};

}  // namespace rcspp::parts
