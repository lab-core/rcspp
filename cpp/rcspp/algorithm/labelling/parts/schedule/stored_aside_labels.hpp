// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/parts/stored_label.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp::parts {

/// @brief The labels a cut stored aside. A cut is the per-node quota, or the trim under memory
///        pressure: it takes a non-dominated label out of the work for now.
///
/// In a single-direction search, the label is stored aside: it stays in its node's container,
/// and @c prepare_next_phase hands it back for the next phase (@ref restore_into). The exception
/// is memory pressure: from its second event on, @c Algorithm::relieve_memory_pressure first has
/// the schedule release everything stored aside (@ref release, through
/// @c release_stored_aside), the quota's labels included. Those labels are never extended; the
/// result may then not be optimal (@c Algorithm::memory_pressure_was_triggered), although the
/// status may read @c Complete. On a side of a bidirectional search, which runs one phase, a cut
/// label is abandoned instead: left in its container, never extended, and counted.
///
/// @tparam ResourceType The resource type carried by labels.
template <typename ResourceType>
class StoredAsideLabels {
    public:
        /// @brief Stores a non-dominated label aside for the next phase (until a later
        ///        memory-pressure event releases it: see the class); on a bidirectional side
        ///        (@c context.abandons()), abandons it and counts it instead.
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
