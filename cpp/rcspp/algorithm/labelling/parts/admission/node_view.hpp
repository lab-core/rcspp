// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>

#include "rcspp/algorithm/dominance_mask.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp::parts {

/// @brief One node's container, as an admission rule sees it: the rule can ask whether a label
///        there dominates the candidate, and evict labels.
///
/// To evict a label is to erase it from the container and mark it dominated; it is not released
/// here. A label still waiting in its schedule is released when the schedule or the take rule
/// finds it marked (@c TakeRule::live). One already extended is not released during the solve:
/// it stays in the pool, pinned by the labels extended from it, until the pool is reset at the
/// next solve.
///
/// @tparam ContainerType The per-node label container.
template <typename ContainerType>
class NodeView {
    public:
        explicit NodeView(ContainerType* container) : container_(container) {}

        /// @brief Whether a label at the node dominates @p label.
        template <typename ResourceType>
        [[nodiscard]] bool is_dominated(const Label<ResourceType>& label) const {
            return container_->is_dominated(label);
        }

        /// @brief Removes from the node the labels @p label dominates, marking them dominated.
        template <typename ResourceType>
        void evict_dominated_by(const Label<ResourceType>& label) {
            container_->remove_dominated_labels(label);
        }

        /// @brief Whether a label at the node dominates @p label over the components @p mask
        ///        leaves in.
        template <typename ResourceType>
        [[nodiscard]] bool is_dominated(const Label<ResourceType>& label,
                                        const DominanceMask& mask) const {
            return container_->is_dominated(label, mask);
        }

        /// @brief Removes from the node the labels @p label dominates over the components @p mask
        ///        leaves in, marking them dominated.
        template <typename ResourceType>
        void evict_dominated_by(const Label<ResourceType>& label, const DominanceMask& mask) {
            container_->remove_dominated_labels(label, mask);
        }

        /// @brief Removes the node's costliest label when it costs more than @p cost, and marks
        ///        it dominated, as an evicted label is: a schedule that still holds it releases
        ///        it when it comes up, instead of extending it.
        ///
        /// @return Whether a label was removed.
        bool evict_costliest_above(double cost) { return container_->remove_costliest_above(cost); }

        /// @brief The labels stored at the node.
        [[nodiscard]] size_t size() const { return container_->get_labels().size(); }

    private:
        ContainerType* container_;
};

/// @brief The view of a node where nothing is stored (@ref KeepWhileWaiting): no label there
///        dominates a candidate, and there is nothing to evict.
struct EmptyNodeView {
        template <typename ResourceType>
        [[nodiscard]] bool is_dominated(const Label<ResourceType>& /*label*/) const {
            return false;
        }

        template <typename ResourceType>
        void evict_dominated_by(const Label<ResourceType>& /*label*/) {}

        template <typename ResourceType>
        [[nodiscard]] bool is_dominated(const Label<ResourceType>& /*label*/,
                                        const DominanceMask& /*mask*/) const {
            return false;
        }

        template <typename ResourceType>
        void evict_dominated_by(const Label<ResourceType>& /*label*/,
                                const DominanceMask& /*mask*/) {}

        static bool evict_costliest_above(double /*cost*/) { return false; }

        [[nodiscard]] static size_t size() { return 0; }
};

}  // namespace rcspp::parts
