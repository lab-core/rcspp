// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
#include <list>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp {

/// @brief The labels one direction of a bidirectional search still has to extend.
///
/// Two modes:
///  - **FIFO** (the default): one queue, in arrival order. An exact search uses it.
///  - **Sweep**: one queue per node position, visited in position order -- ascending for the
///    forward search, descending for the backward one -- and wrapping round until every queue is
///    empty. When a node's queue is visited it is sorted, non-dominated labels first and then by
///    cost, and only the cheapest @c quota are kept, as @c PushingDominanceAlgorithm does. A
///    search with a per-node quota uses it, so the quota keeps a node's *cheapest* labels rather
///    than the first to arrive: by the time the sweep reaches a node, most of its predecessors
///    have delivered their labels.
///
/// A label trimmed from a queue stays in its node's container, so the join can still pair it, but
/// it is never extended. A trimmed label that was already dominated is released.
///
/// @tparam ResourceType The resource type carried by labels.
template <typename ResourceType>
class LabelFrontier {
    public:
        using Entry = LabelIteratorPair<ResourceType>;

        /// @brief Empties the frontier and puts it in FIFO mode.
        void reset_fifo() {
            clear();
            sweeping_ = false;
        }

        /// @brief Empties the frontier and puts it in sweep mode.
        ///
        /// @param num_nodes  The number of node positions.
        /// @param descending Whether the sweep visits positions from the last down (the backward
        ///                   search) rather than from the first up.
        void reset_sweep(size_t num_nodes, bool descending) {
            clear();
            sweeping_ = true;
            descending_ = descending;
            queues_.assign(num_nodes, {});
            started_ = false;  // the first advance lands on the sweep's first position
        }

        /// @brief Whether the frontier is in sweep mode.
        [[nodiscard]] bool sweeping() const { return sweeping_; }

        /// @brief Adds a label to be extended.
        void push(const Entry& entry) {
            if (sweeping_) {
                const size_t position = entry.first->get_end_node()->pos();
                if (started_ && position == position_) {
                    current_.push_back(entry);
                } else {
                    queues_[position].push_back(entry);
                }
            } else {
                current_.push_back(entry);
            }
            ++size_;
        }

        [[nodiscard]] bool empty() const { return size_ == 0; }

        /// @brief Labels waiting, trimmed ones excluded.
        [[nodiscard]] size_t size() const { return size_; }

        /// @brief Takes the next label to extend. The frontier must not be empty.
        ///
        /// In sweep mode, moving to a node's queue sorts it and trims it to @p quota.
        ///
        /// @param quota The per-node quota; read in sweep mode only.
        /// @param pool  Receives the dominated labels a trim drops.
        /// @return The label and its position in its node's container.
        Entry pop(size_t quota, LabelPool<ResourceType>* pool) {
            if (sweeping_) {
                while (current_.empty()) {
                    advance();
                    current_ = std::move(queues_[position_]);
                    queues_[position_].clear();
                    trim(&current_, quota, pool);
                }
            }
            Entry entry = current_.front();
            current_.pop_front();
            --size_;
            return entry;
        }

        /// @brief Sheds labels under memory pressure, keeping the cheapest.
        ///
        /// FIFO mode keeps @p per_node times the number of nodes overall; sweep mode keeps
        /// @p per_node in each node's queue.
        ///
        /// @param per_node  Labels to keep per node.
        /// @param num_nodes The number of nodes.
        /// @param pool      Receives the dominated labels dropped.
        void shed(size_t per_node, size_t num_nodes, LabelPool<ResourceType>* pool) {
            if (!sweeping_) {
                trim(&current_, per_node * num_nodes, pool);
                return;
            }
            trim(&current_, per_node, pool);
            for (auto& queue : queues_) {
                trim(&queue, per_node, pool);
            }
        }

        /// @brief Forgets every label, releasing none (the pool owns them).
        void clear() {
            current_.clear();
            for (auto& queue : queues_) {
                queue.clear();
            }
            size_ = 0;
            started_ = false;
        }

    private:
        /// @brief Moves the sweep to the next position, wrapping round.
        void advance() {
            const size_t count = queues_.size();
            if (!started_) {
                started_ = true;
                position_ = descending_ ? count - 1 : 0;
                return;
            }
            if (descending_) {
                position_ = position_ == 0 ? count - 1 : position_ - 1;
            } else {
                position_ = position_ + 1 == count ? 0 : position_ + 1;
            }
        }

        /// @brief Keeps the @p keep best entries of @p queue: non-dominated first, then cheapest.
        void trim(std::list<Entry>* queue, size_t keep, LabelPool<ResourceType>* pool) {
            if (queue->size() <= keep) {
                return;
            }
            queue->sort([](const Entry& lhs, const Entry& rhs) {
                if (lhs.first->dominated != rhs.first->dominated) {
                    return !lhs.first->dominated;
                }
                return lhs.first->get_cost() < rhs.first->get_cost();
            });
            while (queue->size() > keep) {
                auto* label = queue->back().first;
                if (label->dominated && pool != nullptr) {
                    pool->release_with_ref_count(label);
                }
                queue->pop_back();
                --size_;
            }
        }

        bool sweeping_ = false;
        bool descending_ = false;
        bool started_ = false;
        size_t position_ = 0;
        size_t size_ = 0;
        /// @brief FIFO mode: the whole queue. Sweep mode: the queue of the node being visited.
        std::list<Entry> current_;
        /// @brief Sweep mode: the waiting labels of every other node, by position.
        std::vector<std::list<Entry>> queues_;
};

}  // namespace rcspp
