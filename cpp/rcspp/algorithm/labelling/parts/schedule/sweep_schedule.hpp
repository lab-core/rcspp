// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/node_queues.hpp"
#include "rcspp/algorithm/labelling/parts/stored_label.hpp"

namespace rcspp::parts {

/// @brief Hands labels out node by node, in the sweep order of @p Direction: all of one node's
///        waiting labels, then the next node's, wrapping around (Pushing's order).
///
/// Moving to a node cuts its labels to the per-node quota, keeping the cheapest. The first node of
/// a sweep is loaded without a cut. A label handed out may have been dominated since it was
/// queued: the take rule releases it.
///
/// @tparam ResourceType The resource type carried by labels.
/// @tparam Direction    @ref ForwardDirection or @ref BackwardDirection.
template <typename ResourceType, typename Direction>
class SweepSchedule {
    public:
        /// @brief Empties the queues and sizes them for the graph being solved.
        void initialize(detail::SearchContext<ResourceType>& context) {
            queues_.reset(context.graph->get_number_of_nodes());
        }

        /// @brief The next waiting label of the node being visited, moving on to the next node
        ///        with waiting labels (cutting it to the quota) when this one has none; a null
        ///        label when none is left.
        StoredLabel<ResourceType> next(detail::SearchContext<ResourceType>& context) {
            while (queues_.current().empty()) {
                if (queues_.size() == 0) {
                    return {};  // a cut took the last labels
                }
                queues_.move_to_next_node();
                queues_.cut_to(context, *context.quota);
            }
            auto& current = queues_.current();
            const StoredLabel<ResourceType> stored = current.front();
            queues_.remove_current(current.begin());
            return stored;
        }

        /// @brief The labels waiting to be handed out.
        [[nodiscard]] size_t size() const { return queues_.size(); }

        /// @brief Queues a newly kept label at its node.
        void push(const StoredLabel<ResourceType>& stored) { queues_.push(stored); }

        /// @brief Queues again what the cuts stored aside, and restarts the sweep at its first
        ///        node.
        void prepare_next_phase(detail::SearchContext<ResourceType>& /*context*/) {
            queues_.restore_stored_aside();
        }

        /// @brief Cuts every node's queue to its @p limit cheapest labels.
        void trim(detail::SearchContext<ResourceType>& context, size_t limit) {
            queues_.trim(context, limit);
        }

        /// @brief Erases (through @p erase) and releases what the cuts stored aside.
        template <typename EraseCallable>
        void release_stored_aside(detail::SearchContext<ResourceType>& context,
                                  const EraseCallable& erase) {
            queues_.release_stored_aside(context.pool, erase);
        }

        /// @brief Forgets every label, releasing none (the pool owns them).
        void clear() { queues_.clear(); }

    private:
        NodeQueues<ResourceType, Direction> queues_;
};

}  // namespace rcspp::parts
