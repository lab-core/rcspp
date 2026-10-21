// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <list>
#include <utility>
#include <vector>

#include "rcspp/algorithm/dominance_algorithm.hpp"

namespace rcspp {

namespace detail {

/// @brief The FIFO frontier of @ref SimpleDominanceAlgorithm, in either direction.
///
/// @tparam ResourceType       The resource type carried by labels.
/// @tparam LabelContainerType The per-node container its params hold.
/// @tparam Direction          The direction policy: @ref ForwardDirection or
///                           @ref BackwardDirection.
template <typename ResourceType, typename LabelContainerType, typename Direction>
class SimpleFrontier : public DominanceAlgorithm<ResourceType, LabelContainerType, Direction> {
        using Base = DominanceAlgorithm<ResourceType, LabelContainerType, Direction>;

    public:
        using Base::Base;

        ~SimpleFrontier() override = default;

    private:
        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Algorithm<ResourceType, LabelContainerType>::initialize(graph, cost_upper_bound);
            number_of_extended_labels_per_node_.resize(graph->get_number_of_nodes());
        }
        LabelIteratorPair<ResourceType> next_label_iterator() override {
            LabelIteratorPair<ResourceType> label_iterator_pair;
            while (!unprocessed_labels_.empty()) {
                label_iterator_pair = unprocessed_labels_.front();
                unprocessed_labels_.pop_front();

                // if dominated, release the label. Use release_with_ref_count (not
                // release_label): a dequeued label still pins the predecessor it was extended
                // from, whose ref_count must be decremented to avoid leaking it.
                if (label_iterator_pair.first->dominated) {
                    this->label_pool_.release_with_ref_count(label_iterator_pair.first);
                } else {
                    // truncate/limit the number of labels extended per node
                    size_t& num_extended_labels_for_node = number_of_extended_labels_per_node_.at(
                        label_iterator_pair.first->get_end_node()->pos());
                    if (num_extended_labels_for_node < this->effective_max_labels_per_node_) {
                        ++num_extended_labels_for_node;
                        break;  // found a label to process
                    }
                    // otherwise, store truncated label for next phase
                    unprocessed_truncated_labels_.push_back(label_iterator_pair);
                }
            }

            return label_iterator_pair;
        }

        [[nodiscard]] size_t number_of_labels() const override {
            return unprocessed_labels_.size();
        }

        void add_new_unprocessed_label(
            const LabelIteratorPair<ResourceType>& label_iterator_pair) override {
            unprocessed_labels_.push_back(label_iterator_pair);
        }

        void prepareNextPhase() override {
            std::ranges::fill(number_of_extended_labels_per_node_.begin(),
                              number_of_extended_labels_per_node_.end(),
                              0);
            unprocessed_labels_.splice(unprocessed_labels_.end(), unprocessed_truncated_labels_);
        }

        /// @brief Trim the flat unprocessed list when memory pressure is detected.
        ///
        /// **First call**: keeps the cheapest
        /// @ref AlgorithmBaseParams::memory_pressure_max_labels_per_node × num_nodes
        /// entries (non-dominated first, then sorted by cost).  Dominated excess
        /// labels are recycled; non-dominated excess labels are stored in
        /// @ref unprocessed_truncated_labels_ for the next phase.  Also tightens
        /// the per-node extension cap permanently.
        ///
        /// **Subsequent calls**: additionally releases labels stored aside on the
        /// first call (removes them from the non-dominated set + recycles to pool).
        void on_memory_pressure() override {
            const size_t limit = this->params_.memory_pressure_max_labels_per_node;

            // Permanently tighten the per-node extension cap.
            this->effective_max_labels_per_node_ =
                std::min(this->effective_max_labels_per_node_, limit);

            if (this->memory_pressure_triggered_) {
                // Release labels stored aside on the previous call.
                for (auto& [label_ptr, label_iter] : unprocessed_truncated_labels_) {
                    this->remove_label(label_iter);
                    this->label_pool_.release_with_ref_count(label_ptr);
                }
                unprocessed_truncated_labels_.clear();
            }
            this->memory_pressure_triggered_ = true;

            const size_t max_total = limit * this->graph_->get_number_of_nodes();
            auto excess = split_off_excess_labels(&unprocessed_labels_, max_total);
            // Worst first, as they come off the back of the queue.
            for (auto it = excess.rbegin(); it != excess.rend(); ++it) {
                if (it->first->dominated) {
                    this->label_pool_.release_with_ref_count(it->first);
                } else {
                    unprocessed_truncated_labels_.push_back(*it);
                }
            }
        }

        /// @brief Release label memory and clear all unprocessed label lists.
        void release_label_memory() override {
            Base::release_label_memory();
            unprocessed_labels_.clear();
            unprocessed_truncated_labels_.clear();
            std::ranges::fill(number_of_extended_labels_per_node_.begin(),
                              number_of_extended_labels_per_node_.end(),
                              0);
        }

        std::list<LabelIteratorPair<ResourceType>> unprocessed_labels_;
        std::list<LabelIteratorPair<ResourceType>> unprocessed_truncated_labels_;
        std::vector<size_t> number_of_extended_labels_per_node_;
};

/// @brief The backward search of @ref SimpleDominanceAlgorithm.
///
/// @c create_algorithm<SimpleDominanceAlgorithm> builds it for @c SearchDirection::Backward.
template <typename ResourceType, typename LabelContainerType>
using BackwardSimple = SimpleFrontier<ResourceType, LabelContainerType, BackwardDirection>;

}  // namespace detail

/// @brief Extends labels first in, first out, up to @c num_labels_to_extend_by_node labels per
///        node.
///
/// Searches forward, backward or in both directions, as its params' @c direction asks (through
/// @c ResourceGraph::create_algorithm or @c ResourceGraph::solve).
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>>
    requires ResourceTypeConcept<ResourceType>
class SimpleDominanceAlgorithm
    : public detail::SimpleFrontier<ResourceType, LabelContainerType, ForwardDirection> {
    public:
        using detail::SimpleFrontier<ResourceType, LabelContainerType,
                                     ForwardDirection>::SimpleFrontier;
};
}  // namespace rcspp
