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
class PushingDominanceAlgorithmIterators : public DominanceAlgorithmIterators<ResourceType> {
    public:
        PushingDominanceAlgorithmIterators(ResourceFactory<ResourceType>* resource_factory,
                                           const Graph<ResourceType>& graph, bool use_pool = true)
            : DominanceAlgorithmIterators<ResourceType>(resource_factory, graph, use_pool) {
            for (size_t i = 0; i < graph.get_number_of_nodes(); i++) {
                unprocessed_labels_by_node_pos_.push_back(
                    std::list<LabelIteratorPair<ResourceType>>());
            }

            // ensure that the first call to populate_current_unprocessed_labels_if_needed advances
            // to the first node
            current_unprocessed_node_pos_ = graph.get_number_of_nodes();
        }

        ~PushingDominanceAlgorithmIterators() override = default;

    protected:
        LabelIteratorPair<ResourceType> next_label_iterator() override {
            // if no more labels for the current node, move to the next node with labels
            while (current_unprocessed_labels_.empty()) {
                // move to the next node
                current_unprocessed_node_pos_++;
                // if we have looped over all nodes, start again from the beginning
                if (current_unprocessed_node_pos_ >= this->graph_.get_number_of_nodes()) {
                    current_unprocessed_node_pos_ = 0;
                    num_loops_++;
                }
                // move labels for the current node
                // TODO(Antoine): verify if could be improved
                current_unprocessed_labels_ =
                    std::move(unprocessed_labels_by_node_pos_[current_unprocessed_node_pos_]);
            }

            // get the next label
            auto label_iterator_pair = current_unprocessed_labels_.front();

            current_unprocessed_labels_.pop_front();
            num_unprocessed_labels_--;

            return label_iterator_pair;
        }

        [[nodiscard]] size_t number_of_labels() const override { return num_unprocessed_labels_; }

        void add_new_unprocessed_label(
            const LabelIteratorPair<ResourceType>& label_iterator_pair) override {
            unprocessed_labels_by_node_pos_[label_iterator_pair.first->get_end_node()->pos]
                .push_back(label_iterator_pair);
            num_unprocessed_labels_++;
        }

        size_t num_unprocessed_labels_ = 0;
        size_t current_unprocessed_node_pos_;
        int num_loops_ = -1;  // as starting from the end -> do not count first loop
        std::list<LabelIteratorPair<ResourceType>> current_unprocessed_labels_;
        std::vector<std::list<LabelIteratorPair<ResourceType>>> unprocessed_labels_by_node_pos_;
};
}  // namespace rcspp
