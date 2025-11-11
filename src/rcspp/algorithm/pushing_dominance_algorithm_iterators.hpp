// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <list>
#include <utility>
#include <vector>

#include "rcspp/algorithm/dominance_algorithm_iterators.hpp"

namespace rcspp {

template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class PushingDominanceAlgorithmIterators : public DominanceAlgorithmIterators<ResourceType>,
                                           NodeUnprocessedLabelsManager<ResourceType> {
    public:
        PushingDominanceAlgorithmIterators(ResourceFactory<ResourceType>* resource_factory,
                                           const Graph<ResourceType>& graph, bool use_pool = true)
            : DominanceAlgorithmIterators<ResourceType>(resource_factory, graph, use_pool),
              NodeUnprocessedLabelsManager<ResourceType>(graph.get_number_of_nodes()) {}

        ~PushingDominanceAlgorithmIterators() override = default;

    protected:
        LabelIteratorPair<ResourceType> next_label_iterator() override {
            // if no more labels for the current node, move to the next node with labels
            while (this->current_unprocessed_labels_.empty()) {
                // move to the next node
                this->current_unprocessed_node_pos_++;
                // if we have looped over all nodes, start again from the beginning
                if (this->current_unprocessed_node_pos_ >= this->graph_.get_number_of_nodes()) {
                    this->current_unprocessed_node_pos_ = 0;
                    this->num_loops_++;
                }
                // move labels for the current node
                this->current_unprocessed_labels_ = std::move(
                    this->unprocessed_labels_by_node_pos_.at(this->current_unprocessed_node_pos_));
            }

            // get the next label
            auto label_iterator_pair = this->current_unprocessed_labels_.front();

            this->current_unprocessed_labels_.pop_front();
            this->num_unprocessed_labels_--;

            return label_iterator_pair;
        }

        [[nodiscard]] size_t number_of_labels() const override {
            return this->num_unprocessed_labels_;
        }

        void add_new_unprocessed_label(
            const LabelIteratorPair<ResourceType>& label_iterator_pair) override {
            this->add_new_label(label_iterator_pair);
        }
};
}  // namespace rcspp
