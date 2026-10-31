// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cassert>
#include <cstddef>
#include <list>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/policies.hpp"
#include "rcspp/algorithm/labelling/schedules.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp::parts {

/// @brief The labels waiting to be extended, one queue per node, visited node by node in the
///        order of @p Direction: ascending positions going forward, descending going backward.
///
/// The queue of the node being visited is held apart, as the current labels. A cut (the
/// per-node quota, memory pressure) keeps a node's cheapest labels; of the excess, a dominated
/// label is released, and a non-dominated one is cut (@ref CutLabels).
///
/// @tparam ResourceType The resource type carried by labels.
/// @tparam Direction    @ref ForwardDirection or @ref BackwardDirection.
template <typename ResourceType, typename Direction>
class NodeQueues {
    public:
        using Queue = std::list<StoredLabel<ResourceType>>;

        /// @brief Empties every queue and sizes them for @p num_nodes nodes, for a new solve: what
        ///        an earlier solve left in them names labels the new one is about to recycle. The
        ///        visit starts at the sweep's first node.
        void reset(size_t num_nodes) {
            clear();
            waiting_.resize(num_nodes);
            restart();
        }

        /// @brief Queues a newly kept label at its node: among the current labels when it is the
        ///        node being visited.
        void push(const StoredLabel<ResourceType>& stored) {
            const size_t pos = stored.label->get_end_node()->pos();
            if (pos == position_) {
                current_.push_back(stored);
            } else {
                waiting_.at(pos).push_back(stored);
            }
            ++count_;
        }

        /// @brief The labels waiting, the current ones included.
        [[nodiscard]] size_t waiting_count() const { return count_; }

        /// @brief The labels of the node being visited.
        [[nodiscard]] Queue& current() { return current_; }

        /// @brief The position of the node being visited.
        [[nodiscard]] size_t position() const { return position_; }

        /// @brief The labels waiting at node position @p pos (not the one being visited).
        [[nodiscard]] const Queue& waiting_at(size_t pos) const { return waiting_.at(pos); }

        /// @brief Puts the current labels back in their node's queue, moves to the next node of
        ///        the sweep, wrapping around at the end, and takes that node's queue as the
        ///        current labels.
        ///
        /// @return @c true when it wrapped: a new loop over the nodes starts.
        bool move_to_next_node() {
            waiting_.at(position_).splice(waiting_.at(position_).end(), current_);
            const size_t num_nodes = waiting_.size();
            bool wrapped = false;
            if constexpr (Direction::backward) {
                if (position_ == 0) {
                    position_ = num_nodes - 1;
                    wrapped = true;
                } else {
                    --position_;
                }
            } else {
                if (++position_ >= num_nodes) {
                    position_ = 0;
                    wrapped = true;
                }
            }
            current_.splice(current_.end(), waiting_.at(position_));
            return wrapped;
        }

        /// @brief Takes the label at @p position off the current labels.
        ///
        /// @return The position after it.
        typename Queue::iterator erase_current(typename Queue::iterator position) {
            --count_;
            return current_.erase(position);
        }

        /// @brief Drops the current labels, releasing none: they have been pulled from.
        void drop_current() {
            count_ -= current_.size();
            current_.clear();
        }

        /// @brief Cuts the current labels to the @p limit cheapest.
        void cut_to(detail::SearchContext<ResourceType>& ctx, size_t limit) {
            cut_queue(ctx, &current_, limit);
        }

        /// @brief Cuts every node's queue, the current one included, to its @p limit cheapest.
        void trim(detail::SearchContext<ResourceType>& ctx, size_t limit) {
            cut_queue(ctx, &current_, limit);
            for (auto& queue : waiting_) {
                cut_queue(ctx, &queue, limit);
            }
        }

        /// @brief Queues again, each at its node, what the cuts stored aside, and restarts the
        ///        sweep at its first node.
        void restore_stored_aside() {
            cut_.restore_into([this](const StoredLabel<ResourceType>& stored) {
                waiting_.at(stored.label->get_end_node()->pos()).push_back(stored);
                ++count_;
            });
            restart();
        }

        /// @brief Erases (through @p erase) and releases what the cuts stored aside.
        template <typename EraseCallable>
        void release_stored_aside(LabelPool<ResourceType>* pool, const EraseCallable& erase) {
            cut_.release(pool, erase);
        }

        /// @brief Forgets every label, releasing none (the pool owns them).
        void clear() {
            current_.clear();
            for (auto& queue : waiting_) {
                queue.clear();
            }
            cut_.clear();
            count_ = 0;
            position_ = 0;
        }

    private:
        /// @brief Puts the current labels back and takes the first node of the sweep as current.
        void restart() {
            waiting_.at(position_).splice(waiting_.at(position_).end(), current_);
            if constexpr (Direction::backward) {
                position_ = waiting_.size() - 1;
            } else {
                position_ = 0;
            }
            current_.splice(current_.end(), waiting_.at(position_));
        }

        /// @brief Cuts @p queue to its @p limit cheapest; of the excess, a dominated label is
        ///        released and a non-dominated one is cut.
        void cut_queue(detail::SearchContext<ResourceType>& ctx, Queue* queue, size_t limit) {
            auto excess = detail::split_off_excess(queue, limit);
            for (const auto& stored : excess) {
                if (stored.label->dominated) {
                    ctx.pool->release_with_ref_count(stored.label);
                } else {
                    cut_.cut(ctx, stored);
                }
            }
            count_ -= excess.size();
        }

        std::vector<Queue> waiting_;
        Queue current_;
        size_t position_ = 0;
        size_t count_ = 0;
        CutLabels<ResourceType> cut_;
};

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
        void initialize(detail::SearchContext<ResourceType>& ctx) {
            queues_.reset(ctx.graph->get_number_of_nodes());
        }

        /// @brief The next waiting label of the node being visited, moving on to the next node
        ///        with waiting labels (cutting it to the quota) when this one has none; a null
        ///        label when none is left.
        StoredLabel<ResourceType> next(detail::SearchContext<ResourceType>& ctx) {
            while (queues_.current().empty()) {
                if (queues_.waiting_count() == 0) {
                    return {};  // a cut took the last labels
                }
                queues_.move_to_next_node();
                queues_.cut_to(ctx, *ctx.quota);
            }
            auto& current = queues_.current();
            const StoredLabel<ResourceType> stored = current.front();
            queues_.erase_current(current.begin());
            return stored;
        }

        /// @brief The labels waiting to be handed out.
        [[nodiscard]] size_t size() const { return queues_.waiting_count(); }

        /// @brief Queues a newly kept label at its node.
        void push(const StoredLabel<ResourceType>& stored) { queues_.push(stored); }

        /// @brief Queues again what the cuts stored aside, and restarts the sweep at its first
        ///        node.
        void prepare_next_phase(detail::SearchContext<ResourceType>& /*ctx*/) {
            queues_.restore_stored_aside();
        }

        /// @brief Cuts every node's queue to its @p limit cheapest labels.
        void trim(detail::SearchContext<ResourceType>& ctx, size_t limit) {
            queues_.trim(ctx, limit);
        }

        /// @brief Erases (through @p erase) and releases what the cuts stored aside.
        template <typename EraseCallable>
        void release_stored_aside(detail::SearchContext<ResourceType>& ctx,
                                  const EraseCallable& erase) {
            queues_.release_stored_aside(ctx.pool, erase);
        }

        /// @brief Forgets every label, releasing none (the pool owns them).
        void clear() { queues_.clear(); }

    private:
        NodeQueues<ResourceType, Direction> queues_;
};

/// @brief Visits the nodes one at a time, in the sweep order of @p Direction, pulling into each
///        from the labels waiting at the nodes its in-arcs come from (Pulling's order).
///
/// A visit first puts the last node's new labels back in that node's queue, where its successors
/// pull them from; then moves to the next node; on the first loop over the nodes it keeps the
/// labels already waiting there, on later loops it drops them (they have been pulled from); then
/// pulls along each of the node's in-arcs (@c Direction::in_arcs, from @c Direction::tail) and
/// cuts the node's new labels to the per-node quota, unless the node is an end node. The caller
/// then filters those labels (@ref current, @ref erase_current).
///
/// @tparam ResourceType The resource type carried by labels.
/// @tparam Direction    @ref ForwardDirection or @ref BackwardDirection.
template <typename ResourceType, typename Direction>
class PullSchedule {
    public:
        using Queue = typename NodeQueues<ResourceType, Direction>::Queue;

        /// @brief Empties the queues and sizes them for the graph being solved, and starts on the
        ///        first loop.
        void initialize(detail::SearchContext<ResourceType>& ctx) {
            queues_.reset(ctx.graph->get_number_of_nodes());
            first_loop_ = true;
        }

        /// @brief Moves to the next node and pulls into it, calling `extend(label, arc)` for each
        ///        waiting label of each node an in-arc comes from, arc by arc.
        template <typename ExtendCallable>
        void visit_next_node(detail::SearchContext<ResourceType>& ctx,
                             const ExtendCallable& extend) {
            if (queues_.move_to_next_node()) {
                first_loop_ = false;  // a new loop starts
            }
            if (!first_loop_) {
                // Once a loop has been made, the labels waiting here have been pulled from.
                queues_.drop_current();
            }

            const auto* node = ctx.graph->get_sorted_nodes().at(queues_.position());
            for (const auto* arc : Direction::in_arcs(*ctx.graph, node)) {
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
                queues_.cut_to(ctx, *ctx.quota);
            }
        }

        /// @brief The new labels of the node being visited.
        Queue& current() { return queues_.current(); }

        /// @brief Takes the label at @p position off the node's new labels.
        ///
        /// @return The position after it.
        typename Queue::iterator erase_current(typename Queue::iterator position) {
            return queues_.erase_current(position);
        }

        /// @brief The labels waiting.
        [[nodiscard]] size_t size() const { return queues_.waiting_count(); }

        /// @brief Queues a newly kept label at its node.
        void push(const StoredLabel<ResourceType>& stored) { queues_.push(stored); }

        /// @brief Starts a new first loop and queues again what the cuts stored aside.
        void prepare_next_phase(detail::SearchContext<ResourceType>& /*ctx*/) {
            first_loop_ = true;
            queues_.restore_stored_aside();
        }

        /// @brief Cuts every node's queue to its @p limit cheapest labels.
        void trim(detail::SearchContext<ResourceType>& ctx, size_t limit) {
            queues_.trim(ctx, limit);
        }

        /// @brief Erases (through @p erase) and releases what the cuts stored aside.
        template <typename EraseCallable>
        void release_stored_aside(detail::SearchContext<ResourceType>& ctx,
                                  const EraseCallable& erase) {
            queues_.release_stored_aside(ctx.pool, erase);
        }

        /// @brief Forgets every label, releasing none (the pool owns them), and starts on the
        ///        first loop again.
        void clear() {
            queues_.clear();
            first_loop_ = true;
        }

    private:
        NodeQueues<ResourceType, Direction> queues_;
        /// @brief Whether the visits are still on the first loop over the nodes.
        bool first_loop_ = true;
};

}  // namespace rcspp::parts
