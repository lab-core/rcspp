// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <list>
#include <utility>
#include <vector>

#include "rcspp/algorithm/dominance_algorithm.hpp"

namespace rcspp {
template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class SimpleDominanceAlgorithm : public DominanceAlgorithm<ResourceType> {
    public:
        SimpleDominanceAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                                 const Graph<ResourceType>& graph, AlgorithmParams params)
            : DominanceAlgorithm<ResourceType>(resource_factory, graph, std::move(params)),
              number_of_extended_labels_per_node_(graph.get_number_of_nodes()) {}

        ~SimpleDominanceAlgorithm() override = default;

    private:
        LabelIteratorPair<ResourceType> next_label_iterator() override {
            LabelIteratorPair<ResourceType> label_iterator_pair;
            while (!unprocessed_labels_.empty()) {
                label_iterator_pair = unprocessed_labels_.front();
                unprocessed_labels_.pop_front();

                // if dominated, release the label
                if (label_iterator_pair.first->dominated) {
                    this->label_pool_->release_label(label_iterator_pair.first);
                } else {
                    // truncate/limit the number of labels extended per node
                    size_t& num_extended_labels_for_node = number_of_extended_labels_per_node_.at(
                        label_iterator_pair.first->get_end_node()->pos());
                    if (num_extended_labels_for_node < this->params_.num_labels_to_extend_by_node) {
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

        std::list<LabelIteratorPair<ResourceType>> unprocessed_labels_;
        std::list<LabelIteratorPair<ResourceType>> unprocessed_truncated_labels_;
        std::vector<size_t> number_of_extended_labels_per_node_;
};
}  // namespace rcspp
