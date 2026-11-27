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

/// @brief Decides which of a label's children a depth-first schedule keeps
///        (@ref DepthFirstSchedule).
///
/// The schedule calls @c select(children, rejected, context) once a label has been extended
/// along every arc. @p children then holds the label's children that the admission rule kept,
/// in the order of their arcs, and @p rejected is empty. @c select moves the children to drop
/// to @p rejected, and leaves the others in @p children, in their order. The schedule then
/// erases each rejected child from its node and releases it: it is lost for the rest of the
/// run, not stored aside.
///
/// Nothing records that a dropped child might have led to the best path. The library's filters
/// are safe: @ref KeepAllChildren drops nothing, and @ref TabuChildFilter drops only children
/// reached along a soft arc, which already marks the result as not a proof. A filter of one's
/// own that drops children for another reason would leave a @c Complete solve reported as a
/// proof.
template <typename FilterType, typename ResourceType>
concept ChildFilter = requires(FilterType filter, std::vector<StoredLabel<ResourceType>>& children,
                               std::vector<StoredLabel<ResourceType>>& rejected,
                               const detail::SearchContext<ResourceType>& context) {
    filter.select(children, rejected, context);
};

}  // namespace rcspp::parts
