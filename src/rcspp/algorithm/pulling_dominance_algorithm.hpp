// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cmath>
#include <limits>
#include <list>
#include <utility>

#include "rcspp/algorithm/dominance_algorithm.hpp"

namespace rcspp {
template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class PullingDominanceAlgorithm : public DominanceAlgorithm<ResourceType>,
                                  NodeUnprocessedLabelsManager<ResourceType> {
    public:
        PullingDominanceAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                                  const Graph<ResourceType>& graph, AlgorithmParams params)
            : DominanceAlgorithm<ResourceType>(resource_factory, graph, std::move(params)),
              NodeUnprocessedLabelsManager<ResourceType>(graph.get_number_of_nodes()) {}

        ~PullingDominanceAlgorithm() override = default;

    protected:
        void main_loop() override {
            size_t i = 0;
            while (number_of_labels() > 0 && i < this->params_.max_iterations) {
                ++i;

                // save unprocessed labels for the current node
                assert(this->check_number_of_unprocessed_labels());
                this->unprocessed_labels_by_node_pos_.at(this->current_unprocessed_node_pos_) =
                    std::move(this->current_unprocessed_labels_);

                // pull to the new node
                pull_new_unprocessed_labels();

                // filter labels at current node
                for (auto it = this->current_unprocessed_labels_.begin();
                     it != this->current_unprocessed_labels_.end();) {
                    auto& label = *it->first;

                    // label dominated -> continue to next one
                    if (label.dominated) {
                        this->label_pool_->release_label(&label);
                        it = erase_unprocessed_label(it);  // erase label
                    } else if (std::isinf(label.get_cost())) {
                        // label cost too high -> continue to next one
                        this->remove_label(it->second);
                        this->label_pool_->release_label(&label);
                        it = erase_unprocessed_label(it);  // erase label
                    } else {
                        assert(this->update_non_dominated_labels(label));
                        // check if sink and update best solution
                        if (label.get_end_node()->sink &&
                            label.get_cost() < this->params_.cost_upper_bound &&
                            this->params_.return_dominated_solutions) {
                            this->extract_solution(label);
                            if (this->solutions_.size() >= this->params_.stop_after_X_solutions) {
                                LOG_DEBUG("Stopping after ",
                                          this->solutions_.size(),
                                          " solutions.\n");
                                return;
                            }
                        }
                        ++it;  // move to next label
                    }
                }
            }
        }

        LabelIteratorPair<ResourceType> next_label_iterator() override {
            throw std::runtime_error("next_label_iterator() not implemented");
        }

        void extend(Label<ResourceType>* label_ptr) override {
            throw std::runtime_error("extend(Label<ResourceType>* label_ptr) not implemented");
        }

        void pull_new_unprocessed_labels() {
            // move to the next node
            ++this->current_unprocessed_node_pos_;
            if (this->current_unprocessed_node_pos_ >= this->graph_.get_number_of_nodes()) {
                // start a new loop
                this->current_unprocessed_node_pos_ = 0;
                ++this->num_loops_;
                first_loop_ = false;
            }

            if (first_loop_) {
                // on first loop, add any pre-existing labels at the node to the current labels
                auto& labels_at_node =
                    this->unprocessed_labels_by_node_pos_.at(this->current_unprocessed_node_pos_);
                this->current_unprocessed_labels_.splice(this->current_unprocessed_labels_.end(),
                                                         labels_at_node);
            } else {
                // clear current labels before adding new ones once a loop has been performed
                // all unprocessed labels from the previous loop have been processed
                auto& labels_at_node =
                    this->unprocessed_labels_by_node_pos_.at(this->current_unprocessed_node_pos_);
                // mark all those labels as processed
                this->num_unprocessed_labels_ -= labels_at_node.size();
                labels_at_node.clear();
            }

            // pull labels for the current node
            pull_labels();
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

            // truncate/limit the number of labels extended per node (only if not a sink)
            if (!current_node->sink) {
                this->resize_current_unprocessed_labels(this->params_.num_labels_to_extend_by_node,
                                                        this->label_pool_.get());
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
            --this->num_unprocessed_labels_;
            return this->current_unprocessed_labels_.erase(label_iterator);
        }

        void prepareNextPhase() override {
            first_loop_ = true;
            this->restore_truncated_unprocessed_labels();
        }

        bool first_loop_ = true;
};
}  // namespace rcspp
