// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <utility>

#include "rcspp/algorithm/dominance_algorithm.hpp"

namespace rcspp {

template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class PushingDominanceAlgorithm : public DominanceAlgorithm<ResourceType>,
                                  NodeUnprocessedLabelsManager<ResourceType> {
    public:
        PushingDominanceAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                                  AlgorithmParams params)
            : DominanceAlgorithm<ResourceType>(resource_factory, std::move(params)),
              NodeUnprocessedLabelsManager<ResourceType>() {}

        ~PushingDominanceAlgorithm() override = default;

    protected:
        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Algorithm<ResourceType>::initialize(graph, cost_upper_bound);
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
                this->resize_current_unprocessed_labels(this->params_.num_labels_to_extend_by_node,
                                                        this->label_pool_.get());
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
};
}  // namespace rcspp
