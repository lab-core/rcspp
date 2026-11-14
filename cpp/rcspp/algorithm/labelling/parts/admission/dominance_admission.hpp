// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/labelling/parts/admission/admission.hpp"
#include "rcspp/algorithm/labelling/parts/admission/node_view.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp::parts {

/// @brief Dominance, by the rule its context names: a candidate is kept unless a label at its
///        node dominates it, and evicts the labels it dominates there. The rule is chosen at run
///        time, so one instantiation serves every rule; the exact rule is the default.
struct DominanceAdmission {
        /// @brief Whether the rule keeps every label exact dominance keeps.
        [[nodiscard]] static bool is_exact(const AdmissionContext& context) {
            return context.rule == AdmissionRule::Exact;
        }

        /// @brief Whether @p candidate is kept at @p node; under the exact rule, the labels it
        ///        dominates there are evicted when it is.
        ///
        /// @param candidate The extended label, feasible and not yet stored.
        /// @param node      The view of the node it would be stored at.
        /// @param context       The rule and what it reads.
        /// @return @c true when the candidate is kept.
        template <typename ResourceType, typename NodeViewType>
        bool admit(const Label<ResourceType>& candidate, NodeViewType& node,
                   const AdmissionContext& context) const {
            switch (context.rule) {
                case AdmissionRule::Exact:
                    if (node.is_dominated(candidate)) {
                        return false;
                    }
                    node.evict_dominated_by(candidate);
                    return true;
            }
            return true;
        }
};

}  // namespace rcspp::parts
