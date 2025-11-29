// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <list>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp {

template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class DominanceAlgorithm : public Algorithm<ResourceType> {
    public:
        DominanceAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                           const Graph<ResourceType>& graph, AlgorithmParams params)
            : Algorithm<ResourceType>(resource_factory, graph, std::move(params)) {
            for (size_t i = 0; i < graph.get_number_of_nodes(); i++) {
                non_dominated_labels_by_node_pos_.push_back(std::list<Label<ResourceType>*>());
            }
        }

    protected:
        void initialize_labels() override {
            for (auto source_node_id : this->graph_.get_source_node_ids()) {
                auto& source_node = this->graph_.get_node(source_node_id);
                auto& label = this->label_pool_.get_next_label(&source_node);

                auto& labels = non_dominated_labels_by_node_pos_.at(source_node.pos());
                // it points to the newly inserted element
                auto label_it = labels.insert(labels.end(), &label);
                add_new_unprocessed_label(std::make_pair(&label, label_it));
            }
        }

        bool test(const Label<ResourceType>& label) override {
            // Dominance check
            nb_test_iter_++;
            total_test_time_.start();
            bool non_dominated = update_non_dominated_labels(label);
            if (!non_dominated) {
                this->nb_dominated_labels_++;
            }
            total_test_time_.stop();

            return non_dominated;
        }

        void extend(Label<ResourceType>* label_ptr) override {
            const auto& current_node = label_ptr->get_end_node();
            for (auto arc_ptr : current_node->out_arcs) {
                extend_label(label_ptr, arc_ptr);
            }
        }

        virtual void extend_label(Label<ResourceType>* label_ptr,
                                  const Arc<ResourceType>* arc_ptr) {
            auto& new_label = this->label_pool_.get_next_label(arc_ptr->destination);
            label_ptr->extend(*arc_ptr, &new_label);

            if (new_label.is_feasible() && test(new_label)) {
                // Add to unprocessed_labels_ and non_dominated_labels_by_node_id_ only if
                // feasible and non dominated.
                auto& non_dominated_labels =
                    non_dominated_labels_by_node_pos_.at(new_label.get_end_node()->pos());
                // points to the newly inserted element
                auto new_label_it =
                    non_dominated_labels.insert(non_dominated_labels.end(), &new_label);
                add_new_unprocessed_label(std::make_pair(&new_label, new_label_it));
            } else {
                this->label_pool_.release_label(&new_label);
            }
        }

        std::list<size_t> get_path_arc_ids(const Label<ResourceType>& label) override {
            std::list<size_t> path_arc_ids;

            auto in_arc_ptr = label.get_in_arc();

            if (in_arc_ptr != nullptr) {
                path_arc_ids.push_back(in_arc_ptr->id);

                auto prev_node_ptr = in_arc_ptr->origin;

                const Label<ResourceType>* current_label_ptr = &label;

                while (prev_node_ptr != nullptr && !prev_node_ptr->source) {
                    for (const auto label_ptr :
                         non_dominated_labels_by_node_pos_.at(prev_node_ptr->pos())) {
                        auto& next_label_ref =
                            this->label_pool_.get_next_label(in_arc_ptr->destination);
                        label_ptr->extend(*in_arc_ptr, &next_label_ref);

                        if (next_label_ref <= *current_label_ptr) {
                            current_label_ptr = label_ptr;
                            break;
                        }
                    }

                    in_arc_ptr = current_label_ptr->get_in_arc();

                    if (in_arc_ptr != nullptr) {
                        path_arc_ids.push_back(in_arc_ptr->id);
                        prev_node_ptr = in_arc_ptr->origin;
                    } else {
                        prev_node_ptr = nullptr;
                    }
                }
            }

            std::ranges::reverse(path_arc_ids);

            return path_arc_ids;
        }

        bool update_non_dominated_labels(const Label<ResourceType>& label) override {
            total_update_non_dom_time_.start();

            auto current_node_pos = label.get_end_node()->pos();
            auto& non_dominated_labels_list =
                non_dominated_labels_by_node_pos_.at(current_node_pos);

            // First, check if label is dominated by any existing non-dominated label
            bool label_dominated = false;
            for (const auto non_dominated_label_ptr : non_dominated_labels_list) {
                if (&label == non_dominated_label_ptr) {
                    continue;
                }
                if ((*non_dominated_label_ptr) <= label) {
                    label_dominated = true;
                    break;
                }
            }
            if (label_dominated) {
                total_update_non_dom_time_.stop();
                return false;
            }

            // Second, remove all existing non-dominated labels that are dominated by label
            for (auto non_dominated_label_it = non_dominated_labels_list.begin();
                 non_dominated_label_it != non_dominated_labels_list.end();) {
                if (&label != *non_dominated_label_it && label <= *(*non_dominated_label_it)) {
                    (*non_dominated_label_it)->dominated = true;
                    non_dominated_label_it =
                        non_dominated_labels_list.erase(non_dominated_label_it);
                } else {
                    ++non_dominated_label_it;
                }
            }

            total_update_non_dom_time_.stop();

            return true;
        }

        void remove_label(
            const std::list<Label<ResourceType>*>::iterator& label_iterator) override {
            auto current_node_pos = (*label_iterator)->get_end_node()->pos();
            non_dominated_labels_by_node_pos_.at(current_node_pos).erase(label_iterator);
        }

        [[nodiscard]] std::list<Label<ResourceType>*> get_labels_at_sinks() const override {
            std::list<Label<ResourceType>*> labels_at_sinks;
            for (auto sink_node_id : this->graph_.get_sink_node_ids()) {
                auto node_pos = this->graph_.get_node(sink_node_id).pos();
                const auto& labels_at_current_sink = non_dominated_labels_by_node_pos_.at(node_pos);
                labels_at_sinks.insert(labels_at_sinks.end(),
                                       labels_at_current_sink.begin(),
                                       labels_at_current_sink.end());
            }

            return labels_at_sinks;
        }

        virtual void add_new_unprocessed_label(
            const LabelIteratorPair<ResourceType>& label_iterator_pair) = 0;

        std::vector<std::list<Label<ResourceType>*>> non_dominated_labels_by_node_pos_;

        Timer total_label_time_;
        Timer total_extend_time_;
        Timer total_non_dominated_time_;
        Timer total_test_time_;
        Timer total_extend_inside_time_;
        Timer total_assign_label_time_;
        Timer total_for_time_;
        Timer total_iteration_time_;
        Timer total_update_non_dom_time_;
        Timer total_update_non_dom_v2_time_;
        Timer total_label_pool_time_;

        size_t nb_test_iter_ = 0;
        size_t nb_update_non_dom_iter_ = 0;
        size_t nb_extend_iter_ = 0;
};

template <typename ResourceType>
struct NodeUnprocessedLabelsManager {
        explicit NodeUnprocessedLabelsManager(size_t num_nodes) {
            for (size_t i = 0; i < num_nodes; i++) {
                unprocessed_labels_by_node_pos_.push_back(
                    std::list<LabelIteratorPair<ResourceType>>());
            }
            // ensure that the first call to populate_current_unprocessed_labels_if_needed advances
            // to the first node
            current_unprocessed_node_pos_ = num_nodes;
        }

        void add_new_label(const LabelIteratorPair<ResourceType>& label_iterator_pair) {
            unprocessed_labels_by_node_pos_.at(label_iterator_pair.first->get_end_node()->pos())
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
