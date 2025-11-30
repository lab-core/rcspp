// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>  // NOLINT(build/include_order)
#include <iostream>
#include <limits>
#include <list>
#include <map>
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

constexpr int MAX_INT = std::numeric_limits<int>::max() / 2;  // to avoid overflow

struct AlgorithmParams {
        AlgorithmParams& check() {
            if (num_max_phases > 1 && num_labels_to_extend_by_node >= MAX_INT) {
                LOG_WARN(
                    "AlgorithmParams: num_labels_to_extend_by_node == MAX and num_max_phases > 1. "
                    "num_max_phases will not have any effects, set num_labels_to_extend_by_node to "
                    "a lower value.\n");
            }
            if (num_max_phases > 1 && stop_after_X_solutions >= MAX_INT) {
                LOG_WARN(
                    "AlgorithmParams: stop_after_X_solutions == MAX and num_max_phases > 1. "
                    "num_max_phases will not have any effects, set stop_after_X_solutions to a "
                    "lower value.\n");
            }
            if (return_dominated_solutions && stop_after_X_solutions >= MAX_INT) {
                LOG_WARN(
                    "AlgorithmParams: stop_after_X_solutions == MAX and return_dominated_solutions "
                    "is set to true. return_dominated_solutions will not have any effects, set "
                    "stop_after_X_solutions to a lower value.\n");
            }
            return *this;
        }

        [[nodiscard]] bool could_be_non_optimal() const {
            if (stop_after_X_solutions < MAX_INT) {
                return true;
            }
            if (num_labels_to_extend_by_node < MAX_INT) {
                return false;
            }
            return false;
        }

        // upper bound on the cost of solutions to find
        double cost_upper_bound = std::numeric_limits<double>::infinity();

        // stop after finding X solutions (not going to optimality
        size_t stop_after_X_solutions = MAX_INT;

        // whether to also return dominated solutions found at the sink nodes
        bool return_dominated_solutions = false;

        // for using label pool (should normally always be true)
        bool use_pool = true;

        // for truncated labeling
        size_t num_labels_to_extend_by_node = MAX_INT;

        // number maximum of passes for the resolution if previous pass ended early with not enough
        // solutions
        size_t num_max_phases = 1;
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
              params_(std::move(params.check())) {
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

            size_t num_phases = 0;
            while (solutions_.size() < params_.stop_after_X_solutions && number_of_labels() > 0) {
                // main labeling loop
                main_loop();

                // extract solutions any remaining solutions
                extract_remaining_solutions();

                // prepare next phase (if any)
                if (++num_phases < params_.num_max_phases) {
                    prepareNextPhase();
                } else {
                    break;
                }
            }

            // recover solutions
            std::vector<Solution> solutions;
            solutions.reserve(solutions_.size());
            for (auto& [id, solution] : solutions_) {
                solutions.push_back(std::move(solution));
            }

            // sort solutions
            std::ranges::sort(solutions,
                              [](const Solution& a, const Solution& b) { return a.cost < b.cost; });

            LOG_DEBUG("Number of solutions before resize: ", solutions.size(), '\n');
            LOG_DEBUG("Min cost=",
                      solutions.empty() ? params_.cost_upper_bound : solutions.front().cost,
                      "\n");
            LOG_DEBUG("Total time=", timer.elapsed_seconds(), " sec.\n");

            // resize solutions if needed
            if (solutions.size() > params_.stop_after_X_solutions) {
                solutions.resize(params_.stop_after_X_solutions);
            }

            return solutions;
        }

    protected:
        bool print_{false};

        virtual void main_loop() {
            int i = 0;

            while (this->number_of_labels() > 0) {
                ++i;

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
                        extract_solution(label);
                        if (solutions_.size() >= params_.stop_after_X_solutions) {
                            LOG_DEBUG("Stopping after ", solutions_.size(), " solutions.\n");
                            break;
                        }
                    }
                } else if (!std::isinf(label.get_cost())) {
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

        virtual void prepareNextPhase() {}

        [[nodiscard]] virtual size_t number_of_labels() const = 0;

        virtual LabelIteratorPair<ResourceType> next_label_iterator() = 0;

        virtual void extend(Label<ResourceType>* label) = 0;

        virtual void remove_label(
            const std::list<Label<ResourceType>*>::iterator& label_iterator) = 0;

        [[nodiscard]] virtual std::list<Label<ResourceType>*> get_labels_at_sinks() const = 0;

        virtual std::list<size_t> get_path_arc_ids(const Label<ResourceType>& label) = 0;

        virtual void extract_solution(const Label<ResourceType>& end_label) {
            // already found solution for this label id
            if (solutions_.find(end_label.id) != solutions_.end()) {
                return;
            }

            if (end_label.get_cost() >= params_.cost_upper_bound) {
                return;
            }

            auto path_arc_ids = this->get_path_arc_ids(end_label);
            if (path_arc_ids.empty()) {
                return;
            }

            std::list<size_t> path_node_ids;
            for (size_t arc_id : path_arc_ids) {
                path_node_ids.push_back(this->graph_.get_arc(arc_id).origin->id);
            }
            path_node_ids.push_back(end_label.get_end_node()->id);
            solutions_.try_emplace(end_label.id,
                                   end_label.get_cost(),
                                   std::move(path_node_ids),
                                   std::move(path_arc_ids));
        }

        void extract_remaining_solutions() {
            auto labels_at_sinks = this->get_labels_at_sinks();
            for (const auto* sink_label : labels_at_sinks) {
                this->extract_solution(*sink_label);
            }
        }

        LabelPool<ResourceType> label_pool_;
        const Graph<ResourceType>& graph_;
        const AlgorithmParams params_;

        std::map<size_t, Solution> solutions_;

        size_t nb_dominated_labels_{0};
        Timer total_full_extend_time_;
};
}  // namespace rcspp
