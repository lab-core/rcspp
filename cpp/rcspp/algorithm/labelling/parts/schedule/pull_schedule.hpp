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

/// @brief Visits the nodes one at a time, in the sweep order of @p Direction, and pulls into
///        each: along each arc into the node, it extends the labels waiting at the node the arc
///        comes from (Pulling's order).
///
/// A visit (@ref visit_next_node) does, in order:
/// 1. It puts the previous node's new labels back in that node's queue, where the nodes its
///    arcs lead to will pull them from.
/// 2. It moves to the next node, wrapping around at the end of the sweep.
/// 3. On the first sweep, it keeps the labels already waiting at the node (start
///    labels, or labels a cut stored aside for this phase). On a later sweep, it drops them from
///    the schedule: every node they lead to has been visited, and has pulled from them, since
///    this node's last visit. A dropped label stays stored at its node (the retention), where
///    dominance and the end of the solve read it.
/// 4. It pulls along each of the node's in-arcs in the direction's sense
///    (@c Direction::in_arcs, from @c Direction::tail).
/// 5. It cuts the node's labels to the per-node quota, unless the node is an end node. An
///    end-node label is never extended, and the quota limits the labels extended.
///
/// The caller then runs the take rule on the node's labels (@ref current), and takes off
/// those it drops (@ref remove_current; @c detail::step_node). The first visit is to the sweep's
/// second node. The first node is pulled from on the first sweep, and pulled into only from the
/// second loop.
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
                // Every node these labels lead to has been visited since this node's last visit,
                // and has pulled from them; they stay stored at the node.
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

            // The quota limits the labels extended: an end-node label is never extended, so an
            // end node is not cut.
            if (!Direction::is_end_node(node)) {
                queues_.cut_to(context, *context.quota);
            }
        }

        /// @brief The new labels of the node being visited.
        queue_type& current() { return queues_.current(); }

        /// @brief Takes the label at @p position off the node's new labels. It stays stored at
        ///        its node: this does not erase it from the node's container.
        ///
        /// @return The position after it.
        typename queue_type::iterator remove_current(typename queue_type::iterator position) {
            return queues_.remove_current(position);
        }

        /// @brief The labels it holds, the current node's included.
        [[nodiscard]] size_t size() const { return queues_.size(); }

        /// @brief Queues a newly kept label at its node.
        void push(const StoredLabel<ResourceType>& stored) { queues_.push(stored); }

        /// @brief Starts a new first sweep and queues again what the cuts stored aside.
        void prepare_next_phase(detail::SearchContext<ResourceType>& /*context*/) {
            first_sweep_ = true;
            queues_.restore_stored_aside();
        }

        /// @brief Cuts every node's queue to its @p limit best labels.
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
