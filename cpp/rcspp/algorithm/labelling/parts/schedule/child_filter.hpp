// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/parts/stored_label.hpp"
#include "rcspp/graph/arc.hpp"
#include "rcspp/label/label.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp::parts {

// ─── Depth first ────────────────────────────────────────────────────────────────────────────

/// @brief Which of a label's children a depth-first schedule keeps: @c select moves the children
///        it drops from @p children to @p rejected, keeping the others in order.
template <typename FilterType, typename ResourceType>
concept ChildFilter = requires(FilterType filter, std::vector<StoredLabel<ResourceType>>& children,
                               std::vector<StoredLabel<ResourceType>>& rejected,
                               const detail::SearchContext<ResourceType>& context) {
    filter.select(children, rejected, context);
};  // NOLINT(whitespace/newline)

}  // namespace rcspp::parts
