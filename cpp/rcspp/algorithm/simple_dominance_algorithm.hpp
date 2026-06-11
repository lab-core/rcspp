// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <list>
#include <utility>
#include <vector>

#include "rcspp/algorithm/dominance_algorithm.hpp"

namespace rcspp {
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>>
    requires ResourceTypeConcept<ResourceType>
class SimpleDominanceAlgorithm : public DominanceAlgorithm<ResourceType, LabelContainerType> {
    public:
        SimpleDominanceAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                                 AlgorithmParams<LabelContainerType> params)
            : DominanceAlgorithm<ResourceType, LabelContainerType>(resource_factory,
                                                                   std::move(params)) {}

        ~SimpleDominanceAlgorithm() override = default;

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
            this->effective_max_labels_per_node_ = limit;

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
            if (unprocessed_labels_.size() <= max_total) {
                return;
            }
            unprocessed_labels_.sort([](const LabelIteratorPair<ResourceType>& a,
                                        const LabelIteratorPair<ResourceType>& b) {
                // Non-dominated first; among equal dominance, cheaper first.
                if (a.first->dominated != b.first->dominated) {
                    return !a.first->dominated;
                }
                return a.first->get_cost() < b.first->get_cost();
            });
            while (unprocessed_labels_.size() > max_total) {
                auto& p = unprocessed_labels_.back();
                if (p.first->dominated) {
                    this->label_pool_.release_with_ref_count(p.first);
                } else {
                    unprocessed_truncated_labels_.push_back(std::move(p));
                }
                unprocessed_labels_.pop_back();
            }
        }

        /// @brief Release label memory and clear all unprocessed label lists.
        void release_label_memory() override {
            DominanceAlgorithm<ResourceType, LabelContainerType>::release_label_memory();
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
}  // namespace rcspp
