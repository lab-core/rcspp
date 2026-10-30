// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <cstdint>

#include "rcspp/algorithm/labelling/parts/admission/node_view.hpp"
#include "rcspp/label/label.hpp"
#include "rcspp/label/label_list.hpp"

namespace rcspp::parts {

/// @brief The rule that decides whether a new label is kept at its node.
enum class AdmissionRule : std::uint8_t {
    Exact,  ///< dominance: a label dominated at its node is dropped
};

/// @brief What an admission rule reads besides the candidate and its node. Built once per solve
///        from the params.
struct AdmissionContext {
        AdmissionRule rule = AdmissionRule::Exact;
};

/// @brief Whether a candidate label is kept at its node.
///
/// @c admit reads the candidate and the node it would be stored at, through a node view
/// (@ref NodeView): it decides keep or reject, and may evict labels the candidate dominates there,
/// but touches nothing else. @c exact says whether the rule keeps every label exact dominance
/// keeps. It must answer @c true only for such a rule: a search whose admission is not exact is
/// never reported proven.
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
