// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
#include <queue>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/cost_plus_bound_key.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/per_node_quota.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/stored_aside_labels.hpp"
#include "rcspp/algorithm/labelling/parts/stored_label.hpp"

namespace rcspp::parts {

/// @brief Hands labels out lowest key first (by default g + h, A*'s order), up to the per-node
///        quota.
///
/// Among equal keys the older label (smaller id) comes first, so the order is total and the same
/// with every standard library. As in @ref FifoSchedule, a label taken while dominated is
/// released, and one over its node's quota is cut; @ref next goes on to the next label either
/// way.
///
/// @tparam ResourceType The resource type carried by labels.
/// @tparam KeyType      The order: @ref CostPlusBoundKey (the default) or @ref CostKey.
template <typename ResourceType, typename KeyType = CostPlusBoundKey>
class BestFirstSchedule {
        /// @brief A min-heap on the key, then on the label id.
        ///
        /// The id tie-break makes the order total. Without it, which of two labels with the same
        /// key comes out first depends on the standard library's heap algorithm (libc++'s differs
        /// from libstdc++'s), and so do the labels extended and, under a truncation, the solutions
        /// returned. Label ids come from the pool's counter, so they are unique and follow
        /// creation order.
        struct Comparator {
                const detail::SearchContext<ResourceType>* context = nullptr;
                KeyType key;

                bool operator()(const StoredLabel<ResourceType>& a,
                                const StoredLabel<ResourceType>& b) const {
                    const double ka = key(*a.label, *context);
                    const double kb = key(*b.label, *context);
                    // std::priority_queue is a max-heap; invert the comparison for a min-heap.
                    if (ka != kb) {
                        return ka > kb;
                    }
                    return a.label->id > b.label->id;
                }
        };

        using Heap = std::priority_queue<StoredLabel<ResourceType>,
                                         std::vector<StoredLabel<ResourceType>>, Comparator>;

    public:
        /// @brief Whether the order reads the search's completion bound, which the search must
        ///        then compute.
        static constexpr bool reads_completion_bound = KeyType::reads_completion_bound;

        /// @brief Sizes the per-node counts for the graph being solved, and starts an empty heap
        ///        whose order reads @p context (its bound).
        void initialize(detail::SearchContext<ResourceType>& context) {
            context_ = &context;
            taken_.reset(context.graph->get_number_of_nodes());
            heap_ = Heap(Comparator{.context = context_, .key = {}});
            stored_aside_.clear();
        }

        /// @brief The label of lowest key and its position in its node's container; a null label
        ///        when none is left.
        StoredLabel<ResourceType> next(detail::SearchContext<ResourceType>& context) {
            while (!heap_.empty()) {
                const StoredLabel<ResourceType> stored = heap_.top();
                heap_.pop();

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
        [[nodiscard]] size_t size() const { return heap_.size(); }

        /// @brief Queues a newly kept label.
        void push(const StoredLabel<ResourceType>& stored) { heap_.push(stored); }

        /// @brief Resets the per-node counts and queues again what the quota stored aside.
        void prepare_next_phase(detail::SearchContext<ResourceType>& /*context*/) {
            taken_.restart();
            stored_aside_.restore_into(
                [this](const StoredLabel<ResourceType>& stored) { push(stored); });
        }

        /// @brief When the heap holds more than @p limit × nodes labels, keeps that many of the
        ///        best (non-dominated first, then lowest key); of the rest, best first, a dominated
        ///        label is released and a non-dominated one is cut.
        void trim(detail::SearchContext<ResourceType>& context, size_t limit) {
            const size_t max_total = limit * context.graph->get_number_of_nodes();
            if (heap_.size() <= max_total) {
                return;
            }

            // Drain the heap into a flat vector, in key order, for sorting.
            std::vector<StoredLabel<ResourceType>> flat;
            flat.reserve(heap_.size());
            while (!heap_.empty()) {
                flat.push_back(heap_.top());
                heap_.pop();
            }

            // Non-dominated labels first; among equal dominance, lowest key first.
            const KeyType key{};
            std::ranges::sort(flat, [&key, &context](const auto& a, const auto& b) {
                if (a.label->dominated != b.label->dominated) {
                    return !a.label->dominated;
                }
                return key(*a.label, context) < key(*b.label, context);
            });

            for (size_t i = max_total; i < flat.size(); ++i) {
                const auto& stored = flat[i];
                if (stored.label->dominated) {
                    context.pool->release_with_ref_count(stored.label);
                } else {
                    stored_aside_.store_aside(context, stored);
                }
            }
            flat.resize(max_total);

            heap_ = Heap(flat.begin(), flat.end(), Comparator{.context = context_, .key = {}});
        }

        /// @brief Erases (through @p erase) and releases what the cuts stored aside.
        template <typename EraseCallable>
        void release_stored_aside(detail::SearchContext<ResourceType>& context,
                                  const EraseCallable& erase) {
            stored_aside_.release(context.pool, erase);
        }

        /// @brief Forgets every label, releasing none (the pool owns them), and resets the counts.
        void clear() {
            heap_ = Heap(Comparator{.context = context_, .key = {}});
            stored_aside_.clear();
            taken_.restart();
        }

    private:
        const detail::SearchContext<ResourceType>* context_ = nullptr;
        Heap heap_{Comparator{}};
        StoredAsideLabels<ResourceType> stored_aside_;
        PerNodeQuota taken_;
};

}  // namespace rcspp::parts
