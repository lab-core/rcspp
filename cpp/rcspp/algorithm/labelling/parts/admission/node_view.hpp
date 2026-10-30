// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>

#include "rcspp/label/label.hpp"

namespace rcspp::parts {

/// @brief One node's container, as an admission rule sees it.
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

        /// @brief The labels stored at the node.
        [[nodiscard]] size_t size() const { return container_->get_labels().size(); }

    private:
        ContainerType* container_;
};

}  // namespace rcspp::parts
