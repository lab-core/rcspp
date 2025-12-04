// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <list>
#include <utility>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp {

/**
 * @brief GreedyAlgorithm for Resource Constrained Shortest Path Problems (RCSPP).
 *
 * This algorithm attempts to extend the current path greedily, always choosing the next best label to extend.
 * If a greedy extension is not possible, the algorithm backtracks to previous labels and explores alternative siblings.
 * This approach combines greedy search with backtracking, allowing it to efficiently find feasible solutions
 * while avoiding exhaustive enumeration of all possible paths.
 *
 * Use this algorithm when you want a fast, heuristic approach to RCSPP that can quickly find good solutions,
 * but may not guarantee optimality in all cases. It is particularly useful for large graphs where full enumeration
 * is computationally expensive, and a balance between speed and solution quality is desired.
 */
template <typename ResourceType>
class GreedyAlgorithm : public Algorithm<ResourceType> {
    public:
        GreedyAlgorithm(ResourceFactory<ResourceType>* resource_factory, AlgorithmParams params)
            : Algorithm<ResourceType>(resource_factory, std::move(params)) {}

    protected:
        void main_loop() override {
            size_t i = 0;
            while (this->number_of_labels() > 0 && i < this->params_.max_iterations) {
                ++i;

                // get the label
                auto* label = path_.back().first;

                // check if we can update the best label or extend
                if (label->get_end_node()->sink) {
                    if (label->get_cost() < this->cost_upper_bound_) {
                        this->extract_solution(*label);
                        if (this->solutions_.size() >= this->params_.stop_after_X_solutions) {
                            LOG_DEBUG("Stopping after ", this->solutions_.size(), " solutions.\n");
                            break;
                        }
                    }
                }

                // next label to process
                extend();
            }

            LOG_DEBUG("RCSPP: WHILE nb iter: ", i, "\n");
        }

        void initialize_labels() override {
            std::list<Label<ResourceType>*> sources;
            for (auto source_node_id : this->graph_->get_source_node_ids()) {
                auto* source_node = this->graph_->get_node(source_node_id);
                auto& label = this->label_pool_->get_next_label(source_node);
                sources.push_back(&label);
            }

            if (sources.empty()) {
                return;
            }

            // store the sources: take the first element out, then move the remaining list
            path_.clear();
            add_labels_to_path(std::move(sources));
        }

        [[nodiscard]] size_t number_of_labels() const override { return path_.size(); }

        void extend() {
            // Note: do NOT keep a reference to path_.back() across pop_back() calls
            // (that'd be a dangling reference). Re-query path_.back() each loop.
            // try to extend the label greedily
            while (!path_.empty()) {
                if (extend_label(path_.back().first)) {
                    break;  // successfully extended
                }

                // need to backtrack until we find a node with remaining siblings
                while (!path_.empty() && path_.back().second.empty()) {
                    // release the label at this depth and pop
                    this->label_pool_->release_label(path_.back().first);
                    path_.pop_back();
                }

                if (path_.empty()) {
                    // no more labels to extend
                    return;
                }

                // there is at least one sibling at current depth: release current label and switch
                this->label_pool_->release_label(path_.back().first);
                auto next_label = path_.back().second.front();
                path_.back().second.pop_front();
                path_.back().first = next_label;
                // loop and try to extend the new current label
            }
        }

        bool extend_label(Label<ResourceType>* label) {
            // create all possible extensions
            auto* end_node = label->get_end_node();
            std::list<Label<ResourceType>*> all_labels;
            for (auto* arc : end_node->out_arcs) {
                // extend along arc
                auto& new_label = this->label_pool_->get_next_label(arc->destination);
                label->extend(*arc, &new_label);
                // check feasibility
                if (new_label.is_feasible()) {
                    // successful extension
                    all_labels.push_back(&new_label);
                } else {
                    // release label
                    this->label_pool_->release_label(&new_label);
                }
            }

            if (all_labels.empty()) {
                return false;
            }

            // sort the labels by cost
            all_labels.sort([](Label<ResourceType>* l1, Label<ResourceType>* l2) {
                return l1->get_cost() < l2->get_cost();
            });

            // keep best label first
            add_labels_to_path(std::move(all_labels));

            return true;
        }

        [[nodiscard]] std::list<Label<ResourceType>*> get_labels_at_sinks() const override {
            return {};
        }

        std::list<size_t> get_path_arc_ids(const Label<ResourceType>& label) override {
            std::list<size_t> path_arc_ids;
            for (const auto& p : path_) {
                auto* l = p.first;
                auto* in_arc = l->get_in_arc();
                if (in_arc != nullptr) {
                    path_arc_ids.push_back(in_arc->id);
                }
                if (l == &label) {
                    break;
                }
            }
            return path_arc_ids;
        }

        void add_labels_to_path(std::list<Label<ResourceType>*> labels) {
            auto first = labels.front();
            labels.pop_front();
            path_.emplace_back(first, std::move(labels));
        }

        std::list<std::pair<Label<ResourceType>*, std::list<Label<ResourceType>*>>> path_;
};

}  // namespace rcspp
