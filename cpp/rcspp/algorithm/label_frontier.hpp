// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <list>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp::detail {

/// @brief The labels one direction of a bidirectional search still has to extend.
///
/// Two modes:
///  - **FIFO** (the default): one first-in, first-out queue. @ref BidirectionalDominanceAlgorithm
///    keeps one per direction.
///  - **Clock-ordered**: a binary heap on the label's clock, smallest first for the forward search
///    and largest first for the backward one, ties in insertion order. A meeting half-way point
///    uses it, so each direction's front is the clock value its search has reached.
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
            clock_ordered_ = false;
        }

        /// @brief Empties the frontier and puts it in clock order.
        ///
        /// @param descending Whether the largest clock pops first (the backward search) rather than
        ///                   the smallest (the forward search).
        void reset_clock_order(bool descending) {
            clear();
            clock_ordered_ = true;
            descending_ = descending;
            next_sequence_ = 0;
        }

        /// @brief Whether the frontier is in clock order.
        [[nodiscard]] bool clock_ordered() const { return clock_ordered_; }

        /// @brief Adds a label to be extended.
        void push(const Entry& entry) {
            assert(!clock_ordered_ && "a clock-ordered frontier needs push(entry, key)");
            current_.push_back(entry);
            ++size_;
        }

        /// @brief Adds a label to be extended, with its clock as the ordering key.
        ///
        /// FIFO mode ignores @p key.
        void push(const Entry& entry, double key) {
            if (!clock_ordered_) {
                push(entry);
                return;
            }
            heap_.push_back(Keyed{.key = descending_ ? -key : key,
                                  .sequence = next_sequence_++,
                                  .entry = entry});
            std::ranges::push_heap(heap_, later);
            ++size_;
        }

        /// @brief The clock of the label @ref pop will return next. The frontier must be
        ///        clock-ordered and not empty.
        [[nodiscard]] double front_key() const {
            assert(clock_ordered_ && !heap_.empty());
            const double key = heap_.front().key;
            return descending_ ? -key : key;
        }

        [[nodiscard]] bool empty() const { return size_ == 0; }

        /// @brief Labels waiting, trimmed ones excluded.
        [[nodiscard]] size_t size() const { return size_; }

        /// @brief Takes the next label to extend. The frontier must not be empty.
        ///
        /// In clock order, the label with the smallest (forward) or largest (backward) clock; ties
        /// in insertion order.
        ///
        /// @return The label and its position in its node's container.
        Entry pop() {
            if (clock_ordered_) {
                std::ranges::pop_heap(heap_, later);
                Entry entry = heap_.back().entry;
                heap_.pop_back();
                --size_;
                return entry;
            }
            Entry entry = current_.front();
            current_.pop_front();
            --size_;
            return entry;
        }

        /// @brief Sheds labels under memory pressure, keeping the cheapest.
        ///
        /// Keeps @p per_node times the number of nodes overall.
        ///
        /// @param per_node  Labels to keep per node.
        /// @param num_nodes The number of nodes.
        /// @param pool      Receives the dominated labels dropped.
        void shed(size_t per_node, size_t num_nodes, LabelPool<ResourceType>* pool) {
            if (clock_ordered_) {
                shed_heap(per_node * num_nodes, pool);
                return;
            }
            trim(&current_, per_node * num_nodes, pool);
        }

        /// @brief Forgets every waiting label, releasing those already dominated.
        ///
        /// For a search that has finished: the labels stay in their node's containers, so the join
        /// can still pair them, but they will never be extended.
        ///
        /// @param pool Receives the dominated labels dropped.
        void discard(LabelPool<ResourceType>* pool) {
            const auto release = [pool](const Entry& entry) {
                if (pool != nullptr && entry.first != nullptr && entry.first->dominated) {
                    pool->release_with_ref_count(entry.first);
                }
            };
            for (const auto& keyed : heap_) {
                release(keyed.entry);
            }
            for (const auto& entry : current_) {
                release(entry);
            }
            clear();
        }

        /// @brief Forgets every label, releasing none (the pool owns them).
        void clear() {
            current_.clear();
            heap_.clear();
            size_ = 0;
        }

    private:
        /// @brief One clock-ordered entry.
        struct Keyed {
                double key;       ///< The clock, negated when the frontier is descending.
                size_t sequence;  ///< Insertion order, so ties pop first-in first-out.
                Entry entry;
        };

        /// @brief Heap order: the smallest key, then the earliest insertion, on top.
        static constexpr auto later = [](const Keyed& lhs, const Keyed& rhs) {
            if (lhs.key != rhs.key) {
                return lhs.key > rhs.key;
            }
            return lhs.sequence > rhs.sequence;
        };

        /// @brief Keeps the @p keep best entries of @p queue: non-dominated first, then cheapest.
        void trim(std::list<Entry>* queue, size_t keep, LabelPool<ResourceType>* pool) {
            const auto excess = split_off_excess_labels(queue, keep);
            // Worst first, as they come off the back of the queue.
            for (auto it = excess.rbegin(); it != excess.rend(); ++it) {
                if (it->first->dominated && pool != nullptr) {
                    pool->release_with_ref_count(it->first);
                }
            }
            size_ -= excess.size();
        }

        /// @brief Clock mode's @ref shed: keeps the @p keep best entries, then restores the heap.
        void shed_heap(size_t keep, LabelPool<ResourceType>* pool) {
            if (heap_.size() <= keep) {
                return;
            }
            std::ranges::sort(heap_, [](const Keyed& lhs, const Keyed& rhs) {
                if (lhs.entry.first->dominated != rhs.entry.first->dominated) {
                    return !lhs.entry.first->dominated;
                }
                return lhs.entry.first->get_cost() < rhs.entry.first->get_cost();
            });
            while (heap_.size() > keep) {
                auto* label = heap_.back().entry.first;
                if (label->dominated && pool != nullptr) {
                    pool->release_with_ref_count(label);
                }
                heap_.pop_back();
                --size_;
            }
            std::ranges::make_heap(heap_, later);
        }

        bool clock_ordered_ = false;
        bool descending_ = false;
        size_t size_ = 0;
        /// @brief FIFO mode: the labels waiting, in arrival order.
        std::list<Entry> current_;
        size_t next_sequence_ = 0;
        /// @brief Clock mode: the waiting labels, as a heap under @ref later.
        std::vector<Keyed> heap_;
};

}  // namespace rcspp::detail
