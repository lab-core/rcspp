// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <list>
#include <utility>

#include "rcspp/algorithm/dominance_algorithm_iterators.hpp"

namespace rcspp {
template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class PullingDominanceAlgorithmIterators : public DominanceAlgorithmIterators<ResourceType>,
                                           NodeUnprocessedLabelsManager<ResourceType> {
    public:
        PullingDominanceAlgorithmIterators(ResourceFactory<ResourceType>* resource_factory,
                                           const Graph<ResourceType>& graph, bool use_pool = true)
            : DominanceAlgorithmIterators<ResourceType>(resource_factory, graph, use_pool),
              NodeUnprocessedLabelsManager<ResourceType>(graph.get_number_of_nodes()) {}

        ~PullingDominanceAlgorithmIterators() override = default;

    protected:
        void main_loop() override {
            int i = 0;
            while (number_of_labels() > 0) {
                i++;

                // don't do anything on first iteration (i.e. num_loops_ == -1)
                if (this->num_loops_ >= 0) {
                    // save unprocessed labels for the current node
                    this->unprocessed_labels_by_node_pos_.at(this->current_unprocessed_node_pos_) =
                        std::move(this->current_unprocessed_labels_);
                }

                // pull to the new node
                pull_new_unprocessed_labels();

                // set the iterator to the beginning
                auto it = this->current_unprocessed_labels_.begin();
                while (it != this->current_unprocessed_labels_.end()) {
                    auto& label = *it->first;

                    // label dominated -> continue to next one
                    if (label.dominated) {
                        this->label_pool_.release_label(&label);
                        it = erase_unprocessed_label(it);  // erase label
                        continue;
                    }

                    assert(label.get_end_node());

                    // check if we can update the best label or extend
                    if (label.get_end_node()->sink && label.get_cost() < this->cost_upper_bound_) {
                        this->cost_upper_bound_ = label.get_cost();
                        this->best_label_ = &label;
                        ++it;  // move to next label
                    } else if (!label.get_end_node()->sink &&
                               label.get_cost() < std::numeric_limits<double>::infinity()) {
                        bool label_non_dominated = this->update_non_dominated_labels(*it->first);
                        if (label_non_dominated) {
                            // this->extended_labels_by_node_pos_.at(label.get_end_node()->pos())
                            //     .push_back(&label);
                            ++it;  // move to next label
                        } else {
                            this->label_pool_.release_label(&label);
                            it = erase_unprocessed_label(it);  // erase label
                        }
                    } else {
                        this->label_pool_.release_label(&label);
                        this->remove_label(it->second);
                        it = erase_unprocessed_label(it);  // erase label
                    }
                }
            }

            LOG_DEBUG("RCSPP: WHILE nb iter: ", i, "\n");
            LOG_TRACE("best_label_=", this->best_label_, "\n");
        }

        LabelIteratorPair<ResourceType> next_label_iterator() override {
            throw std::runtime_error("next_label_iterator() not implemented");
        }

        void extend(Label<ResourceType>* label_ptr) override {
            throw std::runtime_error("extend(Label<ResourceType>* label_ptr) not implemented");
        }

        void pull_new_unprocessed_labels() {
            // move to the next node
            this->current_unprocessed_node_pos_++;
            if (this->current_unprocessed_node_pos_ >= this->graph_.get_number_of_nodes()) {
                this->current_unprocessed_node_pos_ = 0;
                this->num_loops_++;
            }

            // clear current labels before adding new ones once a loop has been performed
            // all unprocessed labels from the previous loop have been processed
            if (this->num_loops_ > 0) {
                auto& labels_at_node =
                    this->unprocessed_labels_by_node_pos_.at(this->current_unprocessed_node_pos_);
                // mark all those labels as processed
                this->num_unprocessed_labels_ -= labels_at_node.size();
                labels_at_node.clear();
            }

            // pull labels for the current node
            pull_labels();

            // move labels for the current node
            this->current_unprocessed_labels_ = std::move(
                this->unprocessed_labels_by_node_pos_.at(this->current_unprocessed_node_pos_));
        }

        void pull_labels() {
            // in pulling, we do not extend to nodes
            this->total_full_extend_time_.start();

            const auto& current_node =
                this->graph_.get_sorted_nodes().at(this->current_unprocessed_node_pos_);
            for (auto arc_ptr : current_node->in_arcs) {
                // pull all the unprocessed labels from the origin node
                const auto& unprocessed_labels =
                    this->unprocessed_labels_by_node_pos_.at(arc_ptr->origin->pos());
                for (const auto& label_iterator_pair : unprocessed_labels) {
                    this->extend_label(label_iterator_pair.first, arc_ptr);
                }
            }

            this->total_full_extend_time_.stop();
        }

        [[nodiscard]] size_t number_of_labels() const override {
            return this->num_unprocessed_labels_;
        }

        void add_new_unprocessed_label(
            const LabelIteratorPair<ResourceType>& label_iterator_pair) override {
            this->add_new_label(label_iterator_pair);
        }

        std::list<LabelIteratorPair<ResourceType>>::iterator erase_unprocessed_label(
            const std::list<LabelIteratorPair<ResourceType>>::iterator& label_iterator) {
            this->num_unprocessed_labels_--;
            return this->current_unprocessed_labels_.erase(label_iterator);
        }
};
}  // namespace rcspp
