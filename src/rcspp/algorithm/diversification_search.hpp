// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <list>
#include <map>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"

namespace rcspp {

/**
 * @brief DiversificationSearch: Tabu-based diversification algorithm for RCSPP.
 *
 * This class implements a diversification strategy for resource-constrained shortest path problems (RCSPP)
 * using a tabu-based search. The algorithm wraps another Algorithm instance and repeatedly solves the problem,
 * each time removing arcs from the graph that were used in previous solutions (tabu arcs), to encourage
 * exploration of new and diverse solutions and to escape local optima.
 *
 * The tabu mechanism works by maintaining a list of recently removed arcs (tabu list) with a configurable tenure.
 * Arcs used in a solution are removed from the graph for a number of iterations, preventing their immediate reuse.
 * Random noise can be added to the tenure to further diversify the search.
 *
 * Typical use cases include:
 *   - Metaheuristic frameworks for RCSPP where solution diversity is important.
 *   - Escaping local optima in iterative improvement algorithms.
 *   - Generating a set of diverse solutions for post-processing or ensemble methods.
 *
 * Usage: Construct with a resource factory, algorithm parameters, and a unique_ptr to the wrapped algorithm.
 */
template <typename ResourceType>
class DiversificationSearch : public Algorithm<ResourceType> {
    public:
        DiversificationSearch(ResourceFactory<ResourceType>* resource_factory,
                              AlgorithmParams params, std::unique_ptr<Algorithm<ResourceType>> algo)
            : Algorithm<ResourceType>(resource_factory, std::move(params)),
              algo_(std::move(algo)),
              rnd_(std::random_device{}()) {  // NOLINT(whitespace/braces)
            rnd_.seed(this->params_.seed);
        }

        [[nodiscard]] bool is_optimal() const override { return false; }

        // Run diversification search using tabu-based strategy and collect solutions. The search runs up to max_iterations or
        // stop_after_X_solutions.
    protected:
        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Algorithm<ResourceType>::initialize(graph, cost_upper_bound);
            graph_copy_ = std::move(graph->clone());
        }
        void main_loop() override {
            // check stopping criteria
            if (this->params_.max_iterations >= MAX_INT) {
                LOG_ERROR(
                    "max_iterations needs to be set to a finite value for DiversificationSearch in order to "
                    "stop.\n");
                return;
            }

            // create algorithm params
            auto alg_params = this->params_;
            alg_params.stop_after_X_solutions = 1;  // only need one solution per iteration

            size_t i = 0;
            while (i < this->params_.max_iterations &&
                   this->solutions_.size() < this->params_.stop_after_X_solutions) {
                ++i;

                // solve (important to clear the label pool, as the graph is changing)
                std::vector<Solution> sols =
                    algo_->solve(graph_copy_.get(), this->cost_upper_bound_);
                if (sols.empty()) {
                    break;
                }

                // process solutions
                bool added = false;
                for (auto& sol : sols) {
                    // make tabu
                    tabu_solution(sol);
                    // check if we found the solution
                    if (this->solutions_.contains(sol.get_hash())) {
                        continue;
                    }
                    // then add the solution
                    this->solutions_.emplace(sol.get_hash(), std::move(sol));
                    added = true;
                }

                // increase tenure
                if (!added) {
                    tabu_tenure_extra_ = 1 + (2 * tabu_tenure_extra_);
                }

                // decrease tenure and remove expired
                for (auto it = removed_tabu_arc_ids_.begin(); it != removed_tabu_arc_ids_.end();) {
                    if (it->second == 0) {
                        graph_copy_->restore_arc(it->first);
                        it = removed_tabu_arc_ids_.erase(it);
                    } else {
                        --(it->second);
                        ++it;
                    }
                }
            }

            LOG_DEBUG("DiversificationSearch: WHILE nb iter: ", i, "\n");
        }

        void tabu_solution(const Solution& sol) {
            // remove the following nodes from the graph for the next iteration
            for (auto arc_id : sol.path_arc_ids) {
                // check if arc is already removed or can be removed
                const auto* arc = graph_copy_->get_arc(arc_id);
                if (arc == nullptr || this->params_.forbidden_tabu.contains(arc->origin->id) ||
                    this->params_.forbidden_tabu.contains(arc->destination->id)) {
                    continue;
                }
                // remove arc and add to tabu list
                if (graph_copy_->remove_arc(arc_id)) {
                    size_t tenure = this->params_.tabu_tenure + tabu_tenure_extra_;
                    if (this->params_.tabu_random_noise) {
                        std::uniform_int_distribution<int> dist(tenure > 1 ? -1 : 0, 1);
                        tenure += dist(rnd_);
                    }
                    removed_tabu_arc_ids_[arc_id] = tenure;
                }
            }
        }

        void initialize_labels() override {}

        [[nodiscard]] size_t number_of_labels() const override {
            return 1;
        }  // dummy -> never stop on labels

        [[nodiscard]] std::list<Label<ResourceType>*> get_labels_at_sinks() const override {
            return {};
        }

        std::list<size_t> get_path_arc_ids(const Label<ResourceType>& label) override {
            throw std::runtime_error("No get_path_arc_ids");
        }

    private:
        std::unique_ptr<Graph<ResourceType>> graph_copy_;
        std::unique_ptr<Algorithm<ResourceType>> algo_;
        std::map<size_t, size_t> removed_tabu_arc_ids_;
        size_t tabu_tenure_extra_{0};
        std::mt19937_64 rnd_;
};

}  // namespace rcspp
