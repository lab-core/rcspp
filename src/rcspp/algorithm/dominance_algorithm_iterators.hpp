// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <list>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm_with_iterators.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp {

template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class DominanceAlgorithmIterators : public AlgorithmWithIterators<ResourceType> {
    public:
        DominanceAlgorithmIterators(ResourceFactory<ResourceType>* resource_factory,
                                    const Graph<ResourceType>& graph, bool use_pool = true)
            : AlgorithmWithIterators<ResourceType>(resource_factory, graph, use_pool) {
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

        Label<ResourceType>& next_label() override { return *this->next_label_iterator().first; }

        bool test(const Label<ResourceType>& label) override {
            // Dominance check

            nb_test_iter_++;
            total_test_time_.start();

            bool non_dominated = update_non_dominated_labels(label);
            if (!non_dominated) {
                this->nb_dominated_labels_++;
            }

            // for (const auto non_dominated_label_ptr :
            //      non_dominated_labels_by_node_pos_.at(label.get_end_node()->pos())) {
            //     if (&label == non_dominated_label_ptr) {
            //         continue;
            //     }
            //     if ((*non_dominated_label_ptr) <= label) {
            //         non_dominated = false;
            //         this->nb_dominated_labels_++;
            //         break;
            //     }
            // }

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

        std::vector<size_t> get_path_node_ids(const Label<ResourceType>& label) override {
            std::vector<size_t> path_node_ids{label.get_end_node()->id};

            auto in_arc_ptr = label.get_in_arc();

            if (in_arc_ptr != nullptr) {
                auto prev_node_ptr = in_arc_ptr->origin;

                const Label<ResourceType>* current_label_ptr = &label;

                while (prev_node_ptr != nullptr) {
                    if (prev_node_ptr->source) {
                        path_node_ids.push_back(prev_node_ptr->id);
                        break;
                    }

                    // if empty, infinite loop
                    assert(!non_dominated_labels_by_node_pos_.at(prev_node_ptr->pos()).empty());

                    for (auto label_ptr :
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
                        path_node_ids.push_back(in_arc_ptr->destination->id);
                        prev_node_ptr = in_arc_ptr->origin;
                    } else {
                        prev_node_ptr = nullptr;
                    }
                }
            }

            std::ranges::reverse(path_node_ids);

            return path_node_ids;
        }

        std::vector<std::pair<std::vector<size_t>, std::vector<double>>> get_vector_paths_node_ids(
            const Label<ResourceType>& label) override {
            // LOG_TRACE(__FUNCTION__, '\n');

            std::vector<std::pair<std::vector<size_t>, std::vector<double>>> all_paths_node_ids;

            std::vector<size_t> path_node_ids{label.get_end_node()->id};

            std::vector<double> path_cost_differences;

            std::queue<Label<ResourceType>*> labels_queue;
            std::queue<std::vector<size_t>> partial_paths_node_ids_queue;
            std::queue<std::vector<double>> partial_paths_cost_differences_queue;

            auto in_arc_ptr = label.get_in_arc();

            if (in_arc_ptr != nullptr) {
                auto prev_node_ptr = in_arc_ptr->origin;

                const Label<ResourceType>* current_label_ptr = &label;

                while (prev_node_ptr != nullptr) {
                    if (prev_node_ptr->source) {
                        path_node_ids.push_back(prev_node_ptr->id);
                        std::ranges::reverse(path_node_ids);
                        all_paths_node_ids.emplace_back(path_node_ids, path_cost_differences);
                    } else {
                        for (auto label_ptr :
                             non_dominated_labels_by_node_pos_.at(prev_node_ptr->pos())) {
                            auto label_path_node_ids = path_node_ids;
                            auto label_path_cost_differences = path_cost_differences;

                            auto& next_label_ref =
                                this->label_pool_.get_next_label(in_arc_ptr->destination);
                            label_ptr->extend(*in_arc_ptr, &next_label_ref);

                            if (next_label_ref <= *current_label_ptr) {
                                auto next_label_cost = next_label_ref.get_cost();
                                auto current_label_cost = current_label_ptr->get_cost();

                                label_path_node_ids.push_back(
                                    label_ptr->get_in_arc()->destination->id);

                                label_path_cost_differences.push_back(next_label_cost -
                                                                      current_label_cost);

                                labels_queue.push(label_ptr);
                                partial_paths_node_ids_queue.push(label_path_node_ids);
                                partial_paths_cost_differences_queue.push(
                                    label_path_cost_differences);
                            }
                        }
                    }

                    if (!labels_queue.empty()) {
                        current_label_ptr = labels_queue.front();
                        labels_queue.pop();

                        path_node_ids = partial_paths_node_ids_queue.front();
                        partial_paths_node_ids_queue.pop();

                        path_cost_differences = partial_paths_cost_differences_queue.front();
                        partial_paths_cost_differences_queue.pop();

                        in_arc_ptr = current_label_ptr->get_in_arc();
                    } else {
                        in_arc_ptr = nullptr;
                    }

                    if (in_arc_ptr != nullptr) {
                        prev_node_ptr = in_arc_ptr->origin;
                    } else {
                        prev_node_ptr = nullptr;
                    }
                }
            }

            return all_paths_node_ids;
        }

        std::vector<size_t> get_path_arc_ids(const Label<ResourceType>& label) override {
            std::vector<size_t> path_arc_ids;

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

        void remove_label(const Label<ResourceType>& label) override {
            throw std::runtime_error(
                "DominanceAlgorithmIterators::remove_label(const Label<ResourceType>& label) not "
                "implemented.");
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
