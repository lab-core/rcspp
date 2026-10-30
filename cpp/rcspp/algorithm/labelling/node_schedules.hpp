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

}  // namespace rcspp::parts
