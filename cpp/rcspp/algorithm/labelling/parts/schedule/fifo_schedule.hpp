// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <list>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/per_node_quota.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/stored_aside_labels.hpp"
#include "rcspp/algorithm/labelling/parts/stored_label.hpp"

namespace rcspp::parts {

// ─── First in, first out ────────────────────────────────────────────────────────────────────

/// @brief Hands labels out first in, first out, up to the per-node quota (Simple's order).
///
/// A label that comes up while dominated is released, and one over its node's quota is stored
/// aside for the next phase, until a later memory-pressure event releases it
/// (@ref StoredAsideLabels). Neither is handed out: @ref next goes on to the next label.
///
/// @tparam ResourceType The resource type carried by labels.
template <typename ResourceType>
class FifoSchedule {
    public:
        /// @brief Sizes the per-node counts for the graph being solved.
        void initialize(detail::SearchContext<ResourceType>& context) {
            taken_.reset(context.graph->get_number_of_nodes());
        }

        /// @brief The next label to extend and its position in its node's container; a null
        ///        label when none is left.
        StoredLabel<ResourceType> next(detail::SearchContext<ResourceType>& context) {
            while (!queue_.empty()) {
                const StoredLabel<ResourceType> stored = queue_.front();
                queue_.pop_front();

                // A dequeued label still pins the predecessor it was extended from: release it
                // through the reference counts, or that predecessor leaks.
                if (stored.label->dominated) {
                    context.pool->release_with_ref_count(stored.label);
                    continue;
                }
                if (taken_.try_take(stored.label->get_end_node()->pos(), *context.quota)) {
                    return stored;
                }
                stored_aside_.store_aside(context, stored);
            }
            // Every label taken was released or cut: none to process. The last one must not be
            // handed back, or the caller would release it again or extend a label cut.
            return {};
        }

        /// @brief The labels waiting to be handed out.
        [[nodiscard]] size_t size() const { return queue_.size(); }

        /// @brief Queues a newly kept label.
        void push(const StoredLabel<ResourceType>& stored) { queue_.push_back(stored); }

        /// @brief Resets the per-node counts and queues again what the quota stored aside.
        void prepare_next_phase(detail::SearchContext<ResourceType>& /*context*/) {
            taken_.restart();
            stored_aside_.restore_into(
                [this](const StoredLabel<ResourceType>& stored) { push(stored); });
        }

        /// @brief Keeps the queue's @p limit × nodes best labels (non-dominated first, then
        ///        cheapest), and leaves the queue in that order: after a trim it is no longer
        ///        first in, first out. Of the rest, a dominated label is released and a
        ///        non-dominated one is stored aside for the next phase, until a later
        ///        memory-pressure event releases it (@ref StoredAsideLabels); worst first, so the
        ///        next phase queues them worst first.
        void trim(detail::SearchContext<ResourceType>& context, size_t limit) {
            const size_t max_total = limit * context.graph->get_number_of_nodes();
            auto excess = detail::split_off_excess(&queue_, max_total);
            // Worst first, as they come off the back of the queue: the base's Simple did the
            // same. The order only decides the order the next phase queues them in.
            for (auto it = excess.rbegin(); it != excess.rend(); ++it) {
                if (it->label->dominated) {
                    context.pool->release_with_ref_count(it->label);
                } else {
                    stored_aside_.store_aside(context, *it);
                }
            }
        }

        /// @brief Erases (through @p erase) and releases what the cuts stored aside.
        template <typename EraseCallable>
        void release_stored_aside(detail::SearchContext<ResourceType>& context,
                                  const EraseCallable& erase) {
            stored_aside_.release(context.pool, erase);
        }

        /// @brief Forgets every label, releasing none (the pool owns them), and resets the counts.
        void clear() {
            queue_.clear();
            stored_aside_.clear();
            taken_.restart();
        }

    private:
        std::list<StoredLabel<ResourceType>> queue_;
        StoredAsideLabels<ResourceType> stored_aside_;
        PerNodeQuota taken_;
};

}  // namespace rcspp::parts
