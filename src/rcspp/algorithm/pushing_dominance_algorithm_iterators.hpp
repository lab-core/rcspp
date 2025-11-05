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
                unprocessed_labels_by_node_id_.push_back(
                    std::list<LabelIteratorPair<ResourceType>>());
            }

            // ensure that the first call to populate_current_unprocessed_labels_if_needed advances
            // to the first node
            current_unprocessed_node_id = graph.get_number_of_nodes();
        }

        ~PushingDominanceAlgorithmIterators() override = default;

    protected:
        std::pair<Label<ResourceType>*, typename std::list<Label<ResourceType>*>::iterator>
        next_label_iterator() override {
            // if no more labels for the current node, move to the next node with labels
            while (current_unprocessed_labels_.empty()) {
                // move to the next node
                current_unprocessed_node_id++;
                // if we have looped over all nodes, start again from the beginning
                if (current_unprocessed_node_id >= unprocessed_labels_by_node_id_.size()) {
                    current_unprocessed_node_id = 0;
                    num_loops_++;
                }
                // move labels for the current node
                // TODO(Antoine): verify if could be improved
                current_unprocessed_labels_ =
                    unprocessed_labels_by_node_id_[current_unprocessed_node_id];
                unprocessed_labels_by_node_id_[current_unprocessed_node_id].clear();
            }

            // get the next label
            auto label_iterator_pair = current_unprocessed_labels_.front();

            current_unprocessed_labels_.pop_front();
            --num_unprocessed_labels;

            return label_iterator_pair;
        }

        [[nodiscard]] size_t number_of_labels() const override { return num_unprocessed_labels; }

        void add_new_unprocessed_label(
            const LabelIteratorPair<ResourceType>& label_iterator_pair) override {
            unprocessed_labels_by_node_id_[label_iterator_pair.first->get_end_node()->id].push_back(
                label_iterator_pair);
            num_unprocessed_labels++;
        }

        size_t num_unprocessed_labels = 0;
        size_t current_unprocessed_node_id;
        int num_loops_ = -1;  // as starting from the end -> do not count first loop
        std::list<LabelIteratorPair<ResourceType>> current_unprocessed_labels_;
        std::vector<std::list<LabelIteratorPair<ResourceType>>> unprocessed_labels_by_node_id_;
};
}  // namespace rcspp
