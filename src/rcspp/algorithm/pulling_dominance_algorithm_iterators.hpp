// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <list>
#include <utility>
#include <vector>

#include "rcspp/algorithm/pushing_dominance_algorithm_iterators.hpp"

namespace rcspp {
template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class PullingDominanceAlgorithmIterators : public DominanceAlgorithmIterators<ResourceType> {
    public:
        PullingDominanceAlgorithmIterators(ResourceFactory<ResourceType>* resource_factory,
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

        ~PullingDominanceAlgorithmIterators() override = default;

    protected:
        void main_loop() override {
            int i = 0;
            while (number_of_labels() > 0) {
                i++;

                // don't do anything on first iteration (i.e. num_loops_ == -1)
                if (num_loops_ >= 0) {
                    // save unprocessed labels for the current node
                    unprocessed_labels_by_node_pos_[current_unprocessed_node_pos_] =
                        std::move(current_unprocessed_labels_);
                }

                // pull to the new node
                pull_new_unprocessed_labels();

                // set the iterator to the beginning
                auto it = current_unprocessed_labels_.begin();
                while (it != current_unprocessed_labels_.end()) {
                    auto& label = *it->first;

                    // label dominated -> continue to next one
                    if (label.dominated) {
                        this->label_pool_.release_label(&label);
                        it = erase_unprocessed_label(it);  // erase label
                        continue;
                    }

                    assert(label.get_end_node());

                    // check if we can update the best label or expand
                    if (this->graph_.is_sink(label.get_end_node()->id) &&
                        (label.get_cost() < this->cost_upper_bound_)) {
                        this->cost_upper_bound_ = label.get_cost();
                        this->best_label_ = &label;
                        ++it;  // move to next label
                    } else if (!this->graph_.is_sink(label.get_end_node()->id) &&
                               label.get_cost() < std::numeric_limits<double>::infinity()) {
                        bool label_non_dominated = this->update_non_dominated_labels(*it);
                        if (label_non_dominated) {
                            this->expanded_labels_by_node_id_[label.get_end_node()->id].push_back(
                                &label);
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

        void expand(Label<ResourceType>* label_ptr) override {
            throw std::runtime_error("expand(Label<ResourceType>* label_ptr) not implemented");
        }

        void pull_new_unprocessed_labels() {
            // move to the next node
            ++current_unprocessed_node_pos_;
            if (current_unprocessed_node_pos_ >= this->graph_.get_number_of_nodes()) {
                current_unprocessed_node_pos_ = 0;
                num_loops_++;
            }

            // clear current labels before adding new ones once a loop has been performed
            // all unprocessed labels from the previous loop have been processed
            if (num_loops_ > 0) {
                auto& labels_at_node =
                    unprocessed_labels_by_node_pos_[current_unprocessed_node_pos_];
                // mark all those labels as processed
                num_unprocessed_labels_ -= labels_at_node.size();
                labels_at_node.clear();
            }

            // pull labels for the current node
            pull_labels();

            // move labels for the current node
            current_unprocessed_labels_ =
                std::move(unprocessed_labels_by_node_pos_[current_unprocessed_node_pos_]);
        }

        void pull_labels() {
            // in pulling, we do not expand to nodes
            this->total_full_expand_time_.start();

            const auto& current_node = this->graph_.get_node(current_unprocessed_node_pos_);

            for (auto arc_ptr : current_node.in_arcs) {
                // pull all the unprocessed labels from the origin node
                const auto& unprocessed_labels =
                    unprocessed_labels_by_node_pos_.at(arc_ptr->origin->pos);
                for (const auto& label_iterator_pair : unprocessed_labels) {
                    this->expand_label(label_iterator_pair.first, arc_ptr);
                }
            }

            this->total_full_expand_time_.stop();
        }

        [[nodiscard]] size_t number_of_labels() const override { return num_unprocessed_labels_; }

        void add_new_unprocessed_label(
            const LabelIteratorPair<ResourceType>& label_iterator_pair) override {
            unprocessed_labels_by_node_pos_[label_iterator_pair.first->get_end_node()->pos]
                .push_back(label_iterator_pair);
            num_unprocessed_labels_++;
        }

        std::list<LabelIteratorPair<ResourceType>>::iterator erase_unprocessed_label(
            const std::list<LabelIteratorPair<ResourceType>>::iterator& label_iterator) {
            num_unprocessed_labels_--;
            return current_unprocessed_labels_.erase(label_iterator);
        }

        size_t num_unprocessed_labels_ = 0;
        size_t current_unprocessed_node_pos_;
        int num_loops_ = -1;  // as starting from the end -> do not count first loop
        std::list<LabelIteratorPair<ResourceType>> current_unprocessed_labels_;
        std::vector<std::list<LabelIteratorPair<ResourceType>>> unprocessed_labels_by_node_pos_;
};
}  // namespace rcspp
