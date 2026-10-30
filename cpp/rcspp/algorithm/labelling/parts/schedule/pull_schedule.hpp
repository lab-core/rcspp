// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cassert>
#include <cstddef>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/node_queues.hpp"
#include "rcspp/algorithm/labelling/parts/stored_label.hpp"

namespace rcspp::parts {

/// @brief Visits the nodes one at a time, in the sweep order of @p Direction, pulling into each
///        from the labels waiting at the nodes its in-arcs come from (Pulling's order).
///
/// A visit first puts the last node's new labels back in that node's queue, where its successors
/// pull them from; then moves to the next node; on the first sweep it keeps the
/// labels already waiting there, on later loops it drops them (they have been pulled from); then
/// pulls along each of the node's in-arcs (@c Direction::in_arcs, from @c Direction::tail) and
/// cuts the node's new labels to the per-node quota, unless the node is an end node. The caller
/// then filters those labels (@ref current, @ref remove_current).
///
/// @tparam ResourceType The resource type carried by labels.
/// @tparam Direction    @ref ForwardDirection or @ref BackwardDirection.
template <typename ResourceType, typename Direction>
class PullSchedule {
    public:
        using queue_type = typename NodeQueues<ResourceType, Direction>::queue_type;

        /// @brief Empties the queues and sizes them for the graph being solved, and starts on the
        ///        first sweep.
        void initialize(detail::SearchContext<ResourceType>& context) {
            queues_.reset(context.graph->get_number_of_nodes());
            first_sweep_ = true;
        }

        /// @brief Moves to the next node and pulls into it, calling `extend(label, arc)` for each
        ///        waiting label of each node an in-arc comes from, arc by arc.
        template <typename ExtendCallable>
        void visit_next_node(detail::SearchContext<ResourceType>& context,
                             const ExtendCallable& extend) {
            if (queues_.move_to_next_node()) {
                first_sweep_ = false;  // a new sweep starts
            }
            if (!first_sweep_) {
                // Once a loop has been made, the labels waiting here have been pulled from.
                queues_.drop_current();
            }

            const auto* node = context.graph->get_sorted_nodes().at(queues_.node_position());
            for (const auto* arc : Direction::in_arcs(*context.graph, node)) {
                // A path never continues past an end node: the graph refuses an arc that leaves a
                // sink or enters a source.
                assert(!Direction::is_end_node(Direction::tail(*arc)) &&
                       "an in-arc comes from an end node");
                for (const auto& stored : queues_.waiting_at(Direction::tail(*arc)->pos())) {
                    extend(stored.label, arc);
                }
            }

            // Cut the number of labels extended per node (unless an end node).
            if (!Direction::is_end_node(node)) {
                queues_.cut_to(context, *context.quota);
            }
        }

        /// @brief The new labels of the node being visited.
        queue_type& current() { return queues_.current(); }

        /// @brief Takes the label at @p position off the node's new labels.
        ///
        /// @return The position after it.
        typename queue_type::iterator remove_current(typename queue_type::iterator position) {
            return queues_.remove_current(position);
        }

        /// @brief The labels waiting.
        [[nodiscard]] size_t size() const { return queues_.size(); }

        /// @brief Queues a newly kept label at its node.
        void push(const StoredLabel<ResourceType>& stored) { queues_.push(stored); }

        /// @brief Starts a new first sweep and queues again what the cuts stored aside.
        void prepare_next_phase(detail::SearchContext<ResourceType>& /*context*/) {
            first_sweep_ = true;
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

        /// @brief Forgets every label, releasing none (the pool owns them), and starts on the
        ///        first sweep again.
        void clear() {
            queues_.clear();
            first_sweep_ = true;
        }

    private:
        NodeQueues<ResourceType, Direction> queues_;
        /// @brief Whether the visits are still on the first sweep.
        bool first_sweep_ = true;
};

}  // namespace rcspp::parts
