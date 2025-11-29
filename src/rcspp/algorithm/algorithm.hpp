// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>  // NOLINT(build/include_order)
#include <iostream>
#include <limits>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "rcspp/algorithm/solution.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label_pool.hpp"
#include "rcspp/utils/timer.hpp"

namespace rcspp {

template <typename ResourceType>
using LabelIterator = std::list<Label<ResourceType>*>::iterator;

template <typename ResourceType>
using LabelIteratorPair =
    std::pair<Label<ResourceType>*, typename std::list<Label<ResourceType>*>::iterator>;

struct AlgorithmParams {
        AlgorithmParams& normalize() {
            if (stop_after_X_solutions < std::numeric_limits<size_t>::max() &&
                !return_dominated_solutions) {
                LOG_WARN(
                    "AlgorithmParams: return_dominated_solutions is set to true since "
                    "stop_after_X_solutions < MAX.\n");
                return_dominated_solutions = true;  // need to return dominated solutions
            }
            return *this;
        }

        double cost_upper_bound = std::numeric_limits<double>::infinity();
        size_t stop_after_X_solutions = std::numeric_limits<size_t>::max();
        bool return_dominated_solutions = false;
        bool use_pool = true;
};

template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class Algorithm {
    public:
        Algorithm(ResourceFactory<ResourceType>* resource_factory, const Graph<ResourceType>& graph,
                  AlgorithmParams params)
            : label_pool_(LabelPool<ResourceType>(
                  std::make_unique<LabelFactory<ResourceType>>(resource_factory), params.use_pool)),
              graph_(graph),
              params_(std::move(params.normalize())) {
            if (!graph_.get_sorted_nodes().empty() && !graph_.are_nodes_sorted()) {
                LOG_FATAL(
                    "Graph has a sorted nodes structure that is not correctly sorted. Do not "
                    "manipulate the pos index of the nodes.\n");
                throw std::runtime_error(
                    "Graph has a sorted nodes structure that is not correctly sorted. Do not "
                    "manipulate the pos index of the nodes.");
            }
        }

        virtual ~Algorithm() = default;

        virtual std::vector<Solution> solve() {
            Timer timer;
            this->initialize_labels();
            main_loop();

            if (solutions_.empty()) {
                solutions_ = extract_solutions();
            }

            std::ranges::sort(solutions_,
                              [](const Solution& a, const Solution& b) { return a.cost < b.cost; });

            LOG_DEBUG("Number of solutions: ", solutions_.size(), '\n');
            LOG_DEBUG("Min cost=",
                      solutions_.empty() ? params_.cost_upper_bound : solutions_.front().cost,
                      "\n");
            LOG_DEBUG("Total time=", timer.elapsed_seconds(), " sec.\n");

            return solutions_;
        }

    protected:
        bool print_{false};

        virtual void main_loop() {
            int i = 0;

            while (this->number_of_labels() > 0) {
                i++;

                // next label to process
                auto label_iterator_pair = next_label_iterator();

                // no more label -> break (useful when pulling)
                if (label_iterator_pair.first == nullptr) {
                    break;
                }

                // label dominated -> continue to next one
                auto& label = *label_iterator_pair.first;
                if (label.dominated) {
                    this->label_pool_.release_label(&label);
                    continue;
                }

                assert(label.get_end_node());

                // check if we can update the best label or extend
                if (label.get_end_node()->sink) {
                    if (label.get_cost() < params_.cost_upper_bound &&
                        params_.return_dominated_solutions) {
                        solutions_.push_back(extract_solution(label));
                        if (solutions_.size() >= params_.stop_after_X_solutions) {
                            LOG_DEBUG("Stopping after ", solutions_.size(), " solutions.\n");
                            break;
                        }
                    }
                } else if (!std::isinf(label.get_cost())) {
                    assert(update_non_dominated_labels(label));
                    this->total_full_extend_time_.start();
                    this->extend(&label);
                    this->total_full_extend_time_.stop();
                } else {
                    remove_label(label_iterator_pair.second);
                    this->label_pool_.release_label(&label);
                }
            }

            LOG_DEBUG("RCSPP: WHILE nb iter: ", i, "\n");
        }

        virtual void initialize_labels() = 0;

        virtual LabelIteratorPair<ResourceType> next_label_iterator() = 0;

        virtual bool test(const Label<ResourceType>& label) = 0;
        virtual void extend(Label<ResourceType>* label) = 0;

        [[nodiscard]] virtual size_t number_of_labels() const = 0;

        virtual void remove_label(const LabelIterator<ResourceType>& label_iterator) = 0;
        virtual void remove_label(const LabelIteratorPair<ResourceType>& label_iterator_pair) {
            this->remove_label(label_iterator_pair.second);
        }

        virtual bool update_non_dominated_labels(const Label<ResourceType>& label) = 0;

        [[nodiscard]] virtual std::list<Label<ResourceType>*> get_labels_at_sinks() const = 0;

        virtual std::list<size_t> get_path_arc_ids(const Label<ResourceType>& label) = 0;

        virtual Solution extract_solution(const Label<ResourceType>& end_label) {
            auto path_arc_ids = this->get_path_arc_ids(end_label);
            std::list<size_t> path_node_ids;
            for (size_t arc_id : path_arc_ids) {
                path_node_ids.push_back(this->graph_.get_arc(arc_id).origin->id);
            }
            path_node_ids.push_back(end_label.get_end_node()->id);
            return Solution{end_label.get_cost(),
                            std::move(path_node_ids),
                            std::move(path_arc_ids)};
        }

        std::vector<Solution> extract_solutions() {
            std::vector<Solution> solutions;
            auto labels_at_sinks = this->get_labels_at_sinks();
            for (const auto* sink_label : labels_at_sinks) {
                solutions.push_back(this->extract_solution(*sink_label));
            }
            return solutions;
        }

        LabelPool<ResourceType> label_pool_;
        const Graph<ResourceType>& graph_;
        const AlgorithmParams params_;

        std::vector<Solution> solutions_;

        size_t nb_dominated_labels_{0};
        Timer total_full_extend_time_;
};
}  // namespace rcspp
