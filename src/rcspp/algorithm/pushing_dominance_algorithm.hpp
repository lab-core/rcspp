// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <list>
#include <utility>

#include "rcspp/algorithm/dominance_algorithm.hpp"

namespace rcspp {

template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>>
    requires ResourceTypeConcept<ResourceType>
class PushingDominanceAlgorithm : public DominanceAlgorithm<ResourceType, LabelContainerType>,
                                  NodeUnprocessedLabelsManager<ResourceType> {
    public:
        PushingDominanceAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                                  AlgorithmParams<LabelContainerType> params)
            : DominanceAlgorithm<ResourceType, LabelContainerType>(resource_factory,
                                                                   std::move(params)),
              NodeUnprocessedLabelsManager<ResourceType>() {}

        ~PushingDominanceAlgorithm() override = default;

    protected:
        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Algorithm<ResourceType, LabelContainerType>::initialize(graph, cost_upper_bound);
            this->initialize_unprocessed_labels(graph->get_number_of_nodes());
        }

        LabelIteratorPair<ResourceType> next_label_iterator() override {
            // if no more labels for the current node, move to the next node with labels
            while (this->current_unprocessed_labels_.empty()) {
                // move to the next node
                ++this->current_unprocessed_node_pos_;
                // if we have looped over all nodes, start again from the beginning
                if (this->current_unprocessed_node_pos_ >= this->graph_->get_number_of_nodes()) {
                    this->current_unprocessed_node_pos_ = 0;
                    ++this->num_loops_;
                }
                // move labels for the current node
                this->current_unprocessed_labels_ = std::move(
                    this->unprocessed_labels_by_node_pos_.at(this->current_unprocessed_node_pos_));
                // truncate/limit the number of labels extended per node
                this->resize_current_unprocessed_labels(this->effective_max_labels_per_node_,
                                                        &this->label_pool_);
            }

            // get the next label
            auto label_iterator_pair = this->current_unprocessed_labels_.front();

            this->current_unprocessed_labels_.pop_front();
            --this->num_unprocessed_labels_;

            return label_iterator_pair;
        }

        [[nodiscard]] size_t number_of_labels() const override {
            return this->num_unprocessed_labels_;
        }

        void add_new_unprocessed_label(
            const LabelIteratorPair<ResourceType>& label_iterator_pair) override {
            this->add_new_label(label_iterator_pair);
        }

        void prepareNextPhase() override { this->restore_truncated_unprocessed_labels(); }

        /// @brief Trim per-node queues when memory pressure is detected.
        ///
        /// **First call**: trims all per-node queues to
        /// @ref AlgorithmBaseParams::memory_pressure_max_labels_per_node labels.
        /// Dominated excess labels are immediately recycled; non-dominated excess
        /// labels are stored aside for the next phase (like truncated labeling).
        /// Also tightens the per-node extension cap permanently so that future
        /// extensions do not re-inflate the queues.
        ///
        /// **Subsequent calls**: additionally releases the labels that were stored
        /// aside on the first call, since memory is still under pressure and they
        /// would only be restored at the next phase anyway.
        void on_memory_pressure() override {
            const size_t limit = this->params_.memory_pressure_max_labels_per_node;

            // Permanently tighten the per-node extension cap.
            this->effective_max_labels_per_node_ = limit;

            if (this->memory_pressure_triggered_) {
                // Second+ call: labels stored aside are still consuming memory.
                // Release them (remove from non-dominated set + return to pool).
                this->release_truncated_labels(
                    &this->label_pool_,
                    [this](const typename std::list<Label<ResourceType>*>::iterator& it) {
                        this->remove_label(it);
                    });
            }
            this->memory_pressure_triggered_ = true;

            // Trim the current unprocessed queues.
            this->trim_all_queues(limit, &this->label_pool_);
        }

        /// @brief Release label memory and clear all unprocessed queues.
        void release_label_memory() override {
            DominanceAlgorithm<ResourceType, LabelContainerType>::release_label_memory();
            this->clear_all_queues();
        }
};
}  // namespace rcspp
