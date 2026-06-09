// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cmath>
#include <limits>
#include <list>
#include <utility>

#include "rcspp/algorithm/dominance_algorithm.hpp"

namespace rcspp {
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>>
    requires ResourceTypeConcept<ResourceType>
class PullingDominanceAlgorithm : public DominanceAlgorithm<ResourceType, LabelContainerType>,
                                  NodeUnprocessedLabelsManager<ResourceType> {
    public:
        PullingDominanceAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                                  AlgorithmParams<LabelContainerType> params)
            : DominanceAlgorithm<ResourceType, LabelContainerType>(resource_factory,
                                                                   std::move(params)),
              NodeUnprocessedLabelsManager<ResourceType>() {}

        ~PullingDominanceAlgorithm() override = default;

    protected:
        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Algorithm<ResourceType, LabelContainerType>::initialize(graph, cost_upper_bound);
            this->initialize_unprocessed_labels(graph->get_number_of_nodes());
        }

        void main_loop() override {  // NOLINT
            size_t i = 0;
            while (number_of_labels() > 0 && i < this->params_.max_iterations) {
                // Periodic memory check (pulling: each iteration processes one node).
                if (i > 0 && this->memory_limit_.effective_limit > 0 &&
                    i % this->params_.memory_check_interval == 0) {
                    if (this->memory_limit_.is_exceeded()) {
                        LOG_WARN("Memory limit (",
                                 this->memory_limit_.effective_limit / (1024ULL * 1024ULL),
                                 " MB) exceeded (current: ",
                                 MemoryInfo::process_bytes() / (1024ULL * 1024ULL),
                                 " MB). Stopping early.\n");
                        break;
                    }
                    if (this->memory_limit_.is_under_pressure()) {
                        LOG_INFO("Memory pressure: ",
                                 MemoryInfo::process_bytes() / (1024ULL * 1024ULL),
                                 " MB / ",
                                 this->memory_limit_.effective_limit / (1024ULL * 1024ULL),
                                 " MB. Trimming label queues.\n");
                        this->on_memory_pressure();
                    }
                }

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

                    // label dominated -> continue to next one.
                    // release_with_ref_count (not release_label): in pulling a label may have
                    // already served as an origin in a previous loop (ref_count > 0) and it
                    // pins its own predecessor, so the ref_count chain must be unwound here.
                    if (label.dominated) {
                        this->label_pool_.release_with_ref_count(&label);
                        it = erase_unprocessed_label(it);  // erase label
                    } else if (this->params_.prune_based_on_upper_bound_ &&
                               label.get_cost() >= this->best_cost_upper_bound_) {
                        // label cost too high -> continue to next one
                        this->remove_label(it->second);
                        this->label_pool_.release_with_ref_count(&label);
                        it = erase_unprocessed_label(it);  // erase label
                    } else if (std::isinf(label.get_cost())) {
                        // label cost too high -> continue to next one
                        this->remove_label(it->second);
                        this->label_pool_.release_with_ref_count(&label);
                        it = erase_unprocessed_label(it);  // erase label
                    } else {
                        // check if sink and update best solution
                        if (label.get_end_node()->sink) {
                            LOG_DEBUG("Found a solution with cost ", label.get_cost(), "\n");
                            if (label.get_cost() < this->cost_upper_bound_) {
                                if (label.get_cost() < this->best_cost_upper_bound_) {
                                    this->best_cost_upper_bound_ = label.get_cost();
                                }
                                if (this->params_.return_dominated_solutions) {
                                    this->extract_solution(label);
                                    if (this->solutions_.size() >=
                                        this->params_.stop_after_X_solutions) {
                                        LOG_DEBUG("Stopping after ",
                                                  this->solutions_.size(),
                                                  " solutions.\n");
                                        return;
                                    }
                                }
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
            if (this->current_unprocessed_node_pos_ >= this->graph_->get_number_of_nodes()) {
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
                this->graph_->get_sorted_nodes().at(this->current_unprocessed_node_pos_);
            for (auto arc_ptr : this->graph_->get_in_arcs(current_node)) {
                // pull all the unprocessed labels from the origin node
                const auto& unprocessed_labels =
                    this->unprocessed_labels_by_node_pos_.at(arc_ptr->origin->pos());
                for (const auto& label_iterator_pair : unprocessed_labels) {
                    this->extend_label(label_iterator_pair.first, arc_ptr);
                }
            }

            // truncate/limit the number of labels extended per node (only if not a sink)
            if (!current_node->sink) {
                this->resize_current_unprocessed_labels(this->effective_max_labels_per_node_,
                                                        &this->label_pool_);
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

        /// @brief Trim per-node queues when memory pressure is detected.
        ///
        /// Same two-phase behaviour as @ref PushingDominanceAlgorithm::on_memory_pressure():
        /// first call trims + stores aside; subsequent calls also release stored-aside labels.
        void on_memory_pressure() override {
            const size_t limit = this->params_.memory_pressure_max_labels_per_node;

            this->effective_max_labels_per_node_ = limit;

            if (this->memory_pressure_triggered_) {
                this->release_truncated_labels(
                    &this->label_pool_,
                    [this](const typename std::list<Label<ResourceType>*>::iterator& it) {
                        this->remove_label(it);
                    });
            }
            this->memory_pressure_triggered_ = true;

            this->trim_all_queues(limit, &this->label_pool_);
        }

        /// @brief Release label memory and clear all unprocessed queues.
        void release_label_memory() override {
            DominanceAlgorithm<ResourceType, LabelContainerType>::release_label_memory();
            this->clear_all_queues();
            first_loop_ = true;
        }

        bool first_loop_ = true;
};
}  // namespace rcspp
