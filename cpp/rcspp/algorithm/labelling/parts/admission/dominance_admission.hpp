// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/dominance_mask.hpp"
#include "rcspp/algorithm/labelling/parts/admission/admission.hpp"
#include "rcspp/algorithm/labelling/parts/admission/node_view.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp::parts {

/// @brief Dominance, by the rule its context names. The rule is chosen at run time, so one
///        instantiation serves every rule; the exact rule is the default.
///
/// - @c Exact: a candidate is kept unless a label at its node dominates it, and evicts the labels
///   it dominates there.
/// - @c Relaxed: the same, over the components the context's mask leaves in.
/// - @c KBest: exact dominance; then, at a node that already holds k labels, the candidate takes
///   the place of the costliest when it costs less, and is dropped otherwise.
struct DominanceAdmission {
        /// @brief Whether the rule keeps every label exact dominance keeps: @c Exact does, and
        ///        the others at their limit (a mask that leaves nothing out, no limit on k).
        [[nodiscard]] static bool is_exact(const AdmissionContext& context) {
            switch (context.rule) {
                case AdmissionRule::Exact:
                    return true;
                case AdmissionRule::Relaxed:
                    return context.relaxed_mask() == nullptr;
                case AdmissionRule::KBest:
                    return context.max_labels_per_node == AdmissionContext::kAllLabels;
            }
            return true;
        }

        /// @brief Whether @p candidate is kept at @p node; the labels it dominates there (over
        ///        the components left in, under @c Relaxed) are evicted when it is, and so is the
        ///        costliest when it takes its place under @c KBest.
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
