// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <list>
#include <memory>
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
                non_dominated_labels_by_node_id_.push_back(std::list<Label<ResourceType>*>());
                expanded_labels_by_node_id_.push_back(std::list<Label<ResourceType>*>());
            }
        }

        ~DominanceAlgorithmIterators() override = default;

    private:
        void initialize_labels() override {
            for (auto source_node_id : this->graph_.get_source_node_ids()) {
                auto& source_node = this->graph_.get_node(source_node_id);
                auto& label = this->label_pool_.get_next_label(&source_node);

                non_dominated_labels_by_node_id_[label.get_end_node()->id].push_back(&label);
                auto label_it = non_dominated_labels_by_node_id_[label.get_end_node()->id].end();
                --label_it;

                unprocessed_labels_.push_back(std::make_pair(&label, label_it));
            }
        }

        std::pair<Label<ResourceType>*, typename std::list<Label<ResourceType>*>::iterator>
        next_label_iterator() override {
            auto label_iterator_pair = unprocessed_labels_.front();

            unprocessed_labels_.pop_front();

            return label_iterator_pair;
        }

        Label<ResourceType>& next_label() override {
            auto& label = *unprocessed_labels_.front().first;

            unprocessed_labels_.pop_front();

            return label;
        }

        bool test(const Label<ResourceType>& label) override {
            // Dominance check

            nb_test_iter_++;

            auto time_start = std::chrono::high_resolution_clock::now();

            bool non_dominated = true;
            for (const auto non_dominated_label_ptr :
                 non_dominated_labels_by_node_id_[label.get_end_node()->id]) {
                if (&label == non_dominated_label_ptr) {
                    continue;
                }
                if ((*non_dominated_label_ptr) <= label) {
                    non_dominated = false;
                    this->nb_dominated_labels_++;
                    break;
                }
            }

            auto time_end = std::chrono::high_resolution_clock::now();

            total_test_time_ +=
                std::chrono::duration_cast<std::chrono::nanoseconds>(time_end - time_start).count();

            return non_dominated;
        }

        void expand(Label<ResourceType>* label_ptr) override {
            expanded_labels_by_node_id_[label_ptr->get_end_node()->id].push_back(label_ptr);

            const auto& current_node = label_ptr->get_end_node();

            for (auto arc_ptr : current_node->out_arcs) {
                auto& new_label = this->label_pool_.get_next_label(&arc_ptr->destination);

                label_ptr->expand(*arc_ptr, &new_label);

                if (new_label.is_feasible() && test(new_label)) {
                    // Add to unprocessed_labels_ and non_dominated_labels_by_node_id_ only if
                    // feasible and non dominated.

                    non_dominated_labels_by_node_id_[new_label.get_end_node()->id].push_back(
                        &new_label);

                    auto new_label_it =
                        non_dominated_labels_by_node_id_[new_label.get_end_node()->id].end();
                    --new_label_it;

                    unprocessed_labels_.push_back(std::make_pair(&new_label, new_label_it));
                } else {
                    this->label_pool_.release_label(&new_label);
                }
            }
        }

        [[nodiscard]] size_t number_of_labels() const override {
            return unprocessed_labels_.size();
        }

        std::vector<size_t> get_path_node_ids(const Label<ResourceType>& label) override {
            std::vector<size_t> path_node_ids{label.get_end_node()->id};

            auto in_arc_ptr = label.get_in_arc();

            if (in_arc_ptr != nullptr) {
                auto prev_node_ptr = &in_arc_ptr->origin;

                const Label<ResourceType>* current_label_ptr = &label;

                while (prev_node_ptr != nullptr) {
                    if (this->graph_.is_source(prev_node_ptr->id)) {
                        path_node_ids.push_back(prev_node_ptr->id);
                        break;
                    }

                    for (auto label_ptr : expanded_labels_by_node_id_.at(prev_node_ptr->id)) {
                        auto& next_label_ref =
                            this->label_pool_.get_next_label(&in_arc_ptr->destination);
                        label_ptr->expand(*in_arc_ptr, &next_label_ref);

                        if (next_label_ref <= *current_label_ptr) {
                            current_label_ptr = label_ptr;
                            break;
                        }
                    }

                    in_arc_ptr = current_label_ptr->get_in_arc();

                    if (in_arc_ptr != nullptr) {
                        path_node_ids.push_back(in_arc_ptr->destination.id);
                        prev_node_ptr = &in_arc_ptr->origin;
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
            // std::cout << __FUNCTION__ << std::endl;

            std::vector<std::pair<std::vector<size_t>, std::vector<double>>> all_paths_node_ids;

            std::vector<size_t> path_node_ids{label.get_end_node()->id};

            std::vector<double> path_cost_differences;

            std::queue<Label<ResourceType>*> labels_queue;
            std::queue<std::vector<size_t>> partial_paths_node_ids_queue;
            std::queue<std::vector<double>> partial_paths_cost_differences_queue;

            auto in_arc_ptr = label.get_in_arc();

            if (in_arc_ptr != nullptr) {
                auto prev_node_ptr = &in_arc_ptr->origin;

                const Label<ResourceType>* current_label_ptr = &label;

                while (prev_node_ptr != nullptr) {
                    if (this->graph_.is_source(prev_node_ptr->id)) {
                        path_node_ids.push_back(prev_node_ptr->id);
                        std::ranges::reverse(path_node_ids);
                        all_paths_node_ids.emplace_back(path_node_ids, path_cost_differences);
                    } else {
                        for (auto label_ptr :
                             non_dominated_labels_by_node_id_.at(prev_node_ptr->id)) {
                            auto label_path_node_ids = path_node_ids;
                            auto label_path_cost_differences = path_cost_differences;

                            auto& next_label_ref =
                                this->label_pool_.get_next_label(&in_arc_ptr->destination);
                            label_ptr->expand(*in_arc_ptr, &next_label_ref);

                            if (next_label_ref <= *current_label_ptr) {
                                auto next_label_cost = next_label_ref.get_cost();
                                auto current_label_cost = current_label_ptr->get_cost();

                                label_path_node_ids.push_back(
                                    label_ptr->get_in_arc()->destination.id);

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
                        prev_node_ptr = &in_arc_ptr->origin;
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

                auto prev_node_ptr = &in_arc_ptr->origin;

                const Label<ResourceType>* current_label_ptr = &label;

                while (prev_node_ptr != nullptr && !this->graph_.is_source(prev_node_ptr->id)) {
                    for (const auto label_ptr : expanded_labels_by_node_id_.at(prev_node_ptr->id)) {
                        auto& next_label_ref =
                            this->label_pool_.get_next_label(&in_arc_ptr->destination);
                        label_ptr->expand(*in_arc_ptr, &next_label_ref);

                        if (next_label_ref <= *current_label_ptr) {
                            current_label_ptr = label_ptr;
                            break;
                        }
                    }

                    in_arc_ptr = current_label_ptr->get_in_arc();

                    if (in_arc_ptr != nullptr) {
                        path_arc_ids.push_back(in_arc_ptr->id);
                        prev_node_ptr = &in_arc_ptr->origin;
                    } else {
                        prev_node_ptr = nullptr;
                    }
                }
            }

            std::ranges::reverse(path_arc_ids);

            return path_arc_ids;
        }

        bool update_non_dominated_labels(
            std::pair<Label<ResourceType>*, typename std::list<Label<ResourceType>*>::iterator>
                label_iterator_pair) override {
            auto time_start = std::chrono::high_resolution_clock::now();

            auto label_ptr = label_iterator_pair.first;

            auto current_node_id = label_ptr->get_end_node()->id;

            bool label_non_dominated = true;
            auto& non_dominated_labels_list = non_dominated_labels_by_node_id_[current_node_id];
            for (auto non_dominated_label_it = non_dominated_labels_list.begin();
                 non_dominated_label_it != non_dominated_labels_list.end();
                 non_dominated_label_it++) {
                if (label_ptr == *non_dominated_label_it) {
                    continue;
                }
                if (*label_ptr <= *(*non_dominated_label_it)) {
                    (*non_dominated_label_it)->dominated = true;
                    non_dominated_label_it =
                        non_dominated_labels_list.erase(non_dominated_label_it);
                    // cppcheck-suppress knownConditionTrueFalse
                } else if (*(*non_dominated_label_it) <= *label_ptr) {
                    label_non_dominated = false;
                    break;
                }
            }

            if (!label_non_dominated) {
                remove_label(label_iterator_pair.second);
            }

            auto time_end = std::chrono::high_resolution_clock::now();

            total_update_non_dom_time_ +=
                std::chrono::duration_cast<std::chrono::nanoseconds>(time_end - time_start).count();

            return label_non_dominated;
        }

        void remove_label(
            const std::list<Label<ResourceType>*>::iterator& label_iterator) override {
            auto current_node_id = (*label_iterator)->get_end_node()->id;

            non_dominated_labels_by_node_id_[current_node_id].erase(label_iterator);
        }

        void remove_label(const Label<ResourceType>& label) override {
            // NOT USED
        }

        [[nodiscard]] std::list<Label<ResourceType>*> get_labels_at_sinks() const override {
            std::list<Label<ResourceType>*> labels_at_sinks;
            for (auto sink_node_id : this->graph_.get_sink_node_ids()) {
                const auto& labels_at_current_sink =
                    non_dominated_labels_by_node_id_.at(sink_node_id);
                labels_at_sinks.insert(labels_at_sinks.end(),
                                       labels_at_current_sink.begin(),
                                       labels_at_current_sink.end());
            }

            return labels_at_sinks;
        }

        std::list<
            std::pair<Label<ResourceType>*, typename std::list<Label<ResourceType>*>::iterator>>
            unprocessed_labels_;

        std::vector<std::list<Label<ResourceType>*>> non_dominated_labels_by_node_id_;

        std::vector<std::list<Label<ResourceType>*>> expanded_labels_by_node_id_;

        int64_t total_label_time_ = 0;
        int64_t total_expand_time_ = 0;
        int64_t total_non_dominated_time_ = 0;
        int64_t total_test_time_ = 0;
        int64_t total_expand_inside_time_ = 0;
        int64_t total_assign_label_time_ = 0;
        int64_t total_for_time_ = 0;
        int64_t total_iteration_time_ = 0;
        int64_t total_update_non_dom_time_ = 0;
        int64_t total_update_non_dom_v2_time_ = 0;
        int64_t total_label_pool_time_ = 0;

        size_t nb_test_iter_ = 0;
        size_t nb_update_non_dom_iter_ = 0;
        size_t nb_expand_iter_ = 0;
};
}  // namespace rcspp
