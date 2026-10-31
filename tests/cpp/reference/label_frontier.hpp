// Frozen copy of cpp/rcspp/algorithm/label_frontier.hpp at 9c80f174,
// for the parity tests. Do not edit.
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
#include "reference/base_types.hpp"

namespace rcspp::reference::detail {
using namespace ::rcspp::detail;  // NOLINT(google-build-using-namespace)

/// @brief The labels one direction of a bidirectional search still has to extend.
///
/// One first-in, first-out queue. @ref BidirectionalDominanceAlgorithm keeps one per direction.
///
/// A label trimmed from a queue stays in its node's container, so the join can still pair it, but
/// it is never extended. A trimmed label that was already dominated is released.
///
/// @tparam ResourceType The resource type carried by labels.
template <typename ResourceType>
class LabelFrontier {
    public:
        using Entry = LabelIteratorPair<ResourceType>;

        /// @brief Adds a label to be extended.
        void push(const Entry& entry) {
            current_.push_back(entry);
            ++size_;
        }

        [[nodiscard]] bool empty() const { return size_ == 0; }

        /// @brief Labels waiting, trimmed ones excluded.
        [[nodiscard]] size_t size() const { return size_; }

        /// @brief Takes the next label to extend. The frontier must not be empty.
        ///
        /// @return The label and its position in its node's container.
        Entry pop() {
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
        /// @return How many non-dominated labels were dropped: they stay in their containers but
        ///         are never extended.
        size_t shed(size_t per_node, size_t num_nodes, LabelPool<ResourceType>* pool) {
            return trim(&current_, per_node * num_nodes, pool);
        }

        /// @brief Forgets every label, releasing none (the pool owns them).
        void clear() {
            current_.clear();
            size_ = 0;
        }

    private:
        /// @brief Keeps the @p keep best entries of @p queue: non-dominated first, then cheapest.
        ///
        /// @return How many non-dominated entries were dropped.
        size_t trim(std::list<Entry>* queue, size_t keep, LabelPool<ResourceType>* pool) {
            const auto excess = split_off_excess(queue, keep);
            size_t non_dominated_dropped = 0;
            // Worst first, as they come off the back of the queue.
            for (auto it = excess.rbegin(); it != excess.rend(); ++it) {
                if (!it->first->dominated) {
                    ++non_dominated_dropped;
                } else if (pool != nullptr) {
                    pool->release_with_ref_count(it->first);
                }
            }
            size_ -= excess.size();
            return non_dominated_dropped;
        }

        size_t size_ = 0;
        /// @brief The labels waiting, in arrival order.
        std::list<Entry> current_;
};

}  // namespace rcspp::reference::detail
