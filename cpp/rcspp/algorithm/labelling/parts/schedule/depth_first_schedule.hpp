// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/child_filter.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/cost_key.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/keep_all_children.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/per_node_quota.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/stored_aside_labels.hpp"
#include "rcspp/algorithm/labelling/parts/stored_label.hpp"

namespace rcspp::parts {

/// @brief Hands labels out depth first: the children of the label extended last, lowest key
///        first, before any label waiting from earlier; up to the per-node quota (Greedy's order).
///
/// Once the step has extended a label along every arc, it calls @ref place_children
/// (@ref PlacesChildren), which handles the label's children together. The filter drops some
/// (they are erased from their node and released at once; see @ref place_children). The others
/// are sorted by key, ties kept in the order of their arcs, and put on top of the stack. So each
/// child's own children are handed out before its next sibling. When a label has no child left
/// to hand out, the search goes back to the most recently pushed label still waiting: a
/// backtracking dive. The start labels, pushed before any label is extended, are handed out in
/// the order pushed.
///
/// As in @ref FifoSchedule, a label that comes up while dominated is released, and one over its
/// node's quota is stored aside for the next phase, until a later memory-pressure event releases
/// it (@ref StoredAsideLabels). Neither is handed out: @ref next goes on to the next label.
///
/// @tparam ResourceType The resource type carried by labels.
/// @tparam KeyType      The order among a label's children: @ref CostKey (the default) or
///                      @ref CostPlusBoundKey.
/// @tparam FilterType   Which children are kept (@ref ChildFilter): @ref KeepAllChildren (the
///                      default) or @ref TabuChildFilter.
template <typename ResourceType, typename KeyType = CostKey, typename FilterType = KeepAllChildren>
class DepthFirstSchedule {
        static_assert(ChildFilter<FilterType, ResourceType>,
                      "the filter does not satisfy rcspp::parts::ChildFilter: it needs "
                      "select(children, rejected, context)");

    public:
        /// @brief Whether the order reads the search's completion bound, which the search must
        ///        then compute.
        static constexpr bool reads_completion_bound = KeyType::reads_completion_bound;

        /// @brief Sizes the per-node counts for the graph being solved, and empties the stack.
        void initialize(detail::SearchContext<ResourceType>& context) {
            taken_.reset(context.graph->get_number_of_nodes());
            stack_.clear();
            children_.clear();
            stored_aside_.clear();
        }

        /// @brief The next label in depth-first order and its position in its node's container; a
        ///        null label when none is left.
        StoredLabel<ResourceType> next(detail::SearchContext<ResourceType>& context) {
            // Labels pushed and never placed are the start labels: handed out in the order
            // pushed.
            stack_.insert(stack_.end(), children_.rbegin(), children_.rend());
            children_.clear();
            while (!stack_.empty()) {
                const StoredLabel<ResourceType> stored = stack_.back();
                stack_.pop_back();

                // A label still pins the predecessor it was extended from: release it through the
                // reference counts, or that predecessor leaks.
                if (stored.label->dominated) {
                    context.pool->release_with_ref_count(stored.label);
                    continue;
                }
                if (taken_.try_take(stored.label->get_end_node()->pos(), *context.quota)) {
                    return stored;
                }
                stored_aside_.store_aside(context, stored);
            }
            // Every label taken was released or cut: none to process.
            return {};
        }

        /// @brief The labels waiting to be handed out.
        [[nodiscard]] size_t size() const { return stack_.size() + children_.size(); }

        /// @brief Takes a newly kept label: a child of the label being extended, until
        ///        @ref place_children.
        void push(const StoredLabel<ResourceType>& stored) { children_.push_back(stored); }

        /// @brief Places the children pushed since the last placing: erases from its node
        ///        (through @p erase) and releases each child the filter drops, sorts the others by
        ///        key (stable: their arcs' order breaks ties), and puts them on top of the stack,
        ///        the lowest key to be handed out first.
        ///
        /// The search stored every child at its node before this runs, so a dropped child must
        /// be erased there too, or a retention that stores labels would keep a released label. A
        /// dropped child was admitted first, so labels it dominated at its node were evicted and
        /// stay evicted.
        ///
        /// @param erase Callable `(const LabelIterator&)` erasing a label from its node.
        template <typename EraseCallable>
        void place_children(detail::SearchContext<ResourceType>& context,
                            const EraseCallable& erase) {
            rejected_.clear();
            filter_.select(children_, rejected_, context);
            for (const auto& stored : rejected_) {
                erase(stored.position);
                context.pool->release_with_ref_count(stored.label);
            }
            const KeyType key{};
            std::ranges::stable_sort(children_, [&key, &context](const auto& a, const auto& b) {
                return key(*a.label, context) < key(*b.label, context);
            });
            stack_.insert(stack_.end(), children_.rbegin(), children_.rend());
            children_.clear();
        }

        /// @brief Resets the per-node counts and hands out next what the quota stored aside, in
        ///        the order it was cut.
        void prepare_next_phase(detail::SearchContext<ResourceType>& /*context*/) {
            taken_.restart();
            std::vector<StoredLabel<ResourceType>> restored;
            stored_aside_.restore_into([&restored](const StoredLabel<ResourceType>& stored) {
                restored.push_back(stored);
            });
            stack_.insert(stack_.end(), restored.rbegin(), restored.rend());
        }

        /// @brief Keeps the @p limit × nodes labels to be handed out first. Of the rest, which a
        ///        dive would reach last, a dominated label is released and a non-dominated one is
        ///        stored aside for the next phase, until a later memory-pressure event releases it
        ///        (@ref StoredAsideLabels).
        void trim(detail::SearchContext<ResourceType>& context, size_t limit) {
            const size_t max_total = limit * context.graph->get_number_of_nodes();
            stack_.insert(stack_.end(), children_.rbegin(), children_.rend());
            children_.clear();
            if (stack_.size() <= max_total) {
                return;
            }
            // The bottom of the stack is reached last.
            const auto excess = static_cast<std::ptrdiff_t>(stack_.size() - max_total);
            for (auto it = stack_.begin(); it != stack_.begin() + excess; ++it) {
                if (it->label->dominated) {
                    context.pool->release_with_ref_count(it->label);
                } else {
                    stored_aside_.store_aside(context, *it);
                }
            }
            stack_.erase(stack_.begin(), stack_.begin() + excess);
        }

        /// @brief Erases (through @p erase) and releases what the cuts stored aside.
        template <typename EraseCallable>
        void release_stored_aside(detail::SearchContext<ResourceType>& context,
                                  const EraseCallable& erase) {
            stored_aside_.release(context.pool, erase);
        }

        /// @brief Forgets every label, releasing none (the pool owns them), and resets the counts.
        void clear() {
            stack_.clear();
            children_.clear();
            stored_aside_.clear();
            taken_.restart();
        }

    private:
        std::vector<StoredLabel<ResourceType>> stack_;     ///< next label at the back
        std::vector<StoredLabel<ResourceType>> children_;  ///< pushed since the last placing
        std::vector<StoredLabel<ResourceType>> rejected_;  ///< the filter's, during a placing
        StoredAsideLabels<ResourceType> stored_aside_;
        PerNodeQuota taken_;
        FilterType filter_;
};

}  // namespace rcspp::parts
