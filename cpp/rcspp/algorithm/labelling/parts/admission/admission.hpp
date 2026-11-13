// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "rcspp/algorithm/dominance_mask.hpp"
#include "rcspp/algorithm/labelling/parts/admission/node_view.hpp"
#include "rcspp/algorithm/limits.hpp"
#include "rcspp/label/label.hpp"
#include "rcspp/label/label_list.hpp"

namespace rcspp::parts {

/// @brief The rule that decides whether a new label is kept at its node.
enum class AdmissionRule : std::uint8_t {
    Exact,    ///< dominance: a label dominated at its node is dropped
    Relaxed,  ///< dominance over the components a mask leaves in: more labels are dropped
    KBest,    ///< dominance, then at most k labels at each node, the cheapest
};

/// @brief What an admission rule reads besides the candidate and its node. Built once per solve
///        from the params.
struct AdmissionContext {
        AdmissionRule rule = AdmissionRule::Exact;
        /// @c Relaxed: the components left out of dominance; none, or a mask that leaves nothing
        /// out: dominance is exact.
        const DominanceMask* dominance_mask = nullptr;
        /// @c KBest: the most labels a node keeps; @c kNoLimit (the default): every label
        ///        dominance keeps.
        size_t max_labels_per_node = kNoLimit;

        /// @brief The mask @c Relaxed compares over, or none when it would leave nothing out.
        [[nodiscard]] const DominanceMask* relaxed_mask() const {
            return dominance_mask != nullptr && !dominance_mask->empty() ? dominance_mask : nullptr;
        }
};

/// @brief Whether a candidate label is kept at its node.
///
/// @c admit reads the candidate and the node it would be stored at, through a node view
/// (@ref NodeView). It decides keep or reject, and it may evict labels from the node: those the
/// candidate dominates, and under @c KBest the costliest. It touches nothing else. An evicted
/// label is erased from its node's container and marked dominated, but not released here (see
/// @ref NodeView). @c exact says
/// whether the rule keeps every label exact dominance keeps. It must answer @c true only for such
/// a rule: a search whose admission is not exact is never reported proven.
///
/// @c admit is a const member function; @c exact is static: whether a rule is exact depends only
/// on the params (@ref AdmissionContext), not on the rule object.
///
/// @tparam AdmissionType The rule.
/// @tparam ResourceType  The resource type carried by labels.
/// @tparam NodeViewType  The node view it is handed; a list's by default.
template <typename AdmissionType, typename ResourceType,
          typename NodeViewType = NodeView<LabelList<ResourceType>>>
concept Admission = requires(const AdmissionType admission, const Label<ResourceType>& candidate,
                             NodeViewType& node, const AdmissionContext& context) {
    { admission.admit(candidate, node, context) } -> std::same_as<bool>;
    { AdmissionType::is_exact(context) } -> std::same_as<bool>;
};

}  // namespace rcspp::parts
