// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <list>
#include <map>
#include <random>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/greedy.hpp"

namespace rcspp {

// Generic TabuSearch wrapper.
// AlgorithmFactory must be callable with no arguments and return std::unique_ptr<Algorithm>.
// The Algorithm produced must expose a method:
//   std::vector<Solution> solve();
// extract_arcs: given a Solution returns the vector of arc ids used by the solution.
template <typename ResourceType, template <typename> class AlgorithmType = GreedyAlgorithm>
class TabuSearch : public Algorithm<ResourceType> {
    public:
        using SolutionT = Solution;

        TabuSearch(ResourceFactory<ResourceType>* resource_factory,
                   Graph<ResourceType>& graph,  // NOLINT
                   AlgorithmParams params)
            : Algorithm<ResourceType>(resource_factory, graph, std::move(params)),
              resource_factory_(resource_factory),
              graph_ptr_(&graph),
              rnd_(std::random_device{}()) {  // NOLINT(whitespace/braces)
            rnd_.seed(this->params_.seed);
        }

        ~TabuSearch() override {
            // restore all removed arcs
            for (const auto& p : removed_tabu_arc_ids_) {
                graph_ptr_->restore_arc(p.first);
            }
        }

        // Run tabu search and collect solutions. The search runs up to max_iterations or
        // stop_after_X_solutions
    protected:
        void main_loop() override {
            // check stopping criteria
            if (this->params_.max_iterations >= MAX_INT) {
                LOG_ERROR(
                    "max_iterations needs to be set to a finite value for TabuSearch in order to "
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

                // solve
                AlgorithmType<ResourceType> alg(resource_factory_, this->graph_, alg_params);
                std::vector<Solution> sols = alg.solve();
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
                    ++tabu_tenure_extra_;
                }

                // decrease tenure and remove expired
                for (auto it = removed_tabu_arc_ids_.begin(); it != removed_tabu_arc_ids_.end();) {
                    if (it->second == 0) {
                        graph_ptr_->restore_arc(it->first);
                        it = removed_tabu_arc_ids_.erase(it);
                    } else {
                        --(it->second);
                        ++it;
                    }
                }
            }

            LOG_DEBUG("RCSPP: WHILE nb iter: ", i, "\n");
        }

        void tabu_solution(const Solution& sol) {
            // remove the following nodes from the graph for the next iteration
            for (auto arc_id : sol.path_arc_ids) {
                // check if arc can be removed
                const auto& arc = this->graph_.get_arc(arc_id);
                if (this->params_.forbidden_tabu.contains(arc.origin->id) ||
                    this->params_.forbidden_tabu.contains(arc.destination->id)) {
                    continue;
                }
                // remove arc and add to tabu list
                if (graph_ptr_->remove_arc(arc_id)) {
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
        ResourceFactory<ResourceType>* resource_factory_;
        Graph<ResourceType>* graph_ptr_;
        std::map<size_t, size_t> removed_tabu_arc_ids_;
        size_t tabu_tenure_extra_{0};
        std::mt19937_64 rnd_;
};

}  // namespace rcspp
