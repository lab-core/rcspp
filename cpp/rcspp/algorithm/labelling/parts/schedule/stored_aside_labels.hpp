// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <vector>

#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/parts/stored_label.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp::parts {

/// @brief What a cut (the per-node quota, memory pressure) takes out of a schedule: stored aside
///        for the next phase by a single-direction search, abandoned (left in its container,
///        never extended) and counted by a bidirectional side.
///
/// @tparam ResourceType The resource type carried by labels.
template <typename ResourceType>
class StoredAsideLabels {
    public:
        /// @brief Takes a non-dominated label out of the work, as @p context's role says.
        void store_aside(detail::SearchContext<ResourceType>& context,
                         const StoredLabel<ResourceType>& stored) {
            if (!context.abandons()) {
                labels_.push_back(stored);
            } else if (context.number_of_abandoned_labels != nullptr) {
                ++*context.number_of_abandoned_labels;
            }
        }

        /// @brief Hands every label stored aside to @p push, in the order they were cut.
        ///
        /// @param push Callable `(const StoredLabel&)`.
        template <typename PushCallable>
        void restore_into(const PushCallable& push) {
            for (const auto& stored : labels_) {
                push(stored);
            }
            labels_.clear();
        }

        /// @brief Erases every label stored aside, through @p erase, and releases it; one that
        ///        dominance removed from its node since it was stored aside is only released,
        ///        as that erased the position it was stored with.
        ///
        /// @param erase Callable `(const LabelIterator&)` erasing a label from its node.
        template <typename EraseCallable>
        void release(LabelPool<ResourceType>* pool, const EraseCallable& erase) {
            for (const auto& stored : labels_) {
                if (!stored.label->dominated) {
                    erase(stored.position);
                }
                // A stored label pins a predecessor and may itself be pinned: releasing through
                // the reference counts keeps the chain balanced.
                pool->release_with_ref_count(stored.label);
            }
            labels_.clear();
        }

        /// @brief Forgets every label stored aside, releasing none (the pool owns them).
        void clear() { labels_.clear(); }

    private:
        std::vector<StoredLabel<ResourceType>> labels_;
};

}  // namespace rcspp::parts
