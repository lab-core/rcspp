// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/dominance_mask.hpp"
#include "rcspp/algorithm/labelling/parts/admission/admission.hpp"
#include "rcspp/algorithm/labelling/parts/admission/node_view.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp::parts {

/// @brief The admission by dominance, under the rule its context names
///        (@c AdmissionContext::rule). The rule is read at run time, so one instantiation
///        serves every rule; the exact rule is the default.
///
/// - @c Exact: a candidate is kept unless a label at its node dominates it. When it is kept,
///   it evicts the labels it dominates there.
/// - @c Relaxed: the same, but dominance ignores the resource components the context's mask
///   leaves out, so more labels are dropped.
/// - @c KBest: exact dominance first. Then, if the node already holds k labels, the candidate
///   takes the place of the costliest (which is evicted, and marked dominated) if it costs
///   less, and is dropped otherwise.
struct DominanceAdmission {
        /// @brief True if the rule keeps every label exact dominance keeps. @c Exact always
        ///        does. @c Relaxed does when its mask ignores nothing, and @c KBest when k is
        ///        @c kNoLimit.
        [[nodiscard]] static bool is_exact(const AdmissionContext& context) {
            switch (context.rule) {
                case AdmissionRule::Exact:
                    return true;
                case AdmissionRule::Relaxed:
                    return context.relaxed_mask() == nullptr;
                case AdmissionRule::KBest:
                    return context.max_labels_per_node >= kNoLimit;
            }
            return true;
        }

        /// @brief Returns true if @p candidate is kept at @p node. When it is kept, the labels
        ///        it dominates there are evicted (under @c Relaxed, dominance over the components
        ///        the mask leaves in). Under @c KBest, so is the costliest label, if the node was
        ///        full.
        ///
        /// @param candidate The extended label, feasible and not yet stored.
        /// @param node      The view of the node it would be stored at.
        /// @param context       The rule and what it reads.
        /// @return @c true when the candidate is kept.
        template <typename ResourceType, typename NodeViewType>
        bool admit(const Label<ResourceType>& candidate, NodeViewType& node,
                   const AdmissionContext& context) const {
            if (context.rule == AdmissionRule::Relaxed) {
                if (const DominanceMask* mask = context.relaxed_mask()) {
                    if (node.is_dominated(candidate, *mask)) {
                        return false;
                    }
                    node.evict_dominated_by(candidate, *mask);
                    return true;
                }
            }
            if (node.is_dominated(candidate)) {
                return false;
            }
            node.evict_dominated_by(candidate);
            // A node holds at most k labels, so a candidate that evicted one always finds room:
            // no candidate evicts and is then dropped.
            return context.rule != AdmissionRule::KBest ||
                   node.size() < context.max_labels_per_node ||
                   node.evict_costliest_above(candidate.get_cost());
        }
};

}  // namespace rcspp::parts
