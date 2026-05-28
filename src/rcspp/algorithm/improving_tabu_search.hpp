// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <limits>
#include <list>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/greedy.hpp"
#include "rcspp/algorithm/tabu_search.hpp"

namespace rcspp {

/// @brief Two-phase improving tabu search for RCSPP.
///
/// Phase 1 — construction: @ref GreedyAlgorithm runs with an infinite upper
/// bound to obtain an initial feasible solution and its cost.
///
/// Phase 2 — improvement: @ref TabuSearchAlgorithm runs with the best cost
/// from phase 1 as the upper bound, driving the tabu-guided DFS towards
/// strictly improving solutions.
///
/// All solutions found in both phases are collected and deduplicated.  The
/// algorithm is non-optimal by design; it is intended as a fast primal
/// heuristic to seed tighter upper bounds for exact dominance solvers.
///
/// @tparam ResourceType      Composed resource type (must satisfy ResourceTypeConcept).
/// @tparam LabelContainerType Non-dominated label container (default: LabelList).
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>>
    requires ResourceTypeConcept<ResourceType>
class ImprovingTabuSearch : public Algorithm<ResourceType, LabelContainerType> {
    public:
        /// @brief Construct with a resource factory and algorithm parameters.
        ///
        /// The same @p params are forwarded to both the inner greedy and tabu
        /// algorithms.  Relevant fields:
        ///   - @c max_iterations  — controls the tabu improvement phase.
        ///   - @c tabu_tenure, @c tabu_random_noise  — tabu list settings.
        ///   - @c stop_after_X_solutions  — early stop across both phases.
        ///
        /// @param resource_factory Factory for source-node label initialisation.
        /// @param params           Algorithm configuration.
        ImprovingTabuSearch(ResourceFactory<ResourceType>* resource_factory,
                            AlgorithmParams<LabelContainerType> params)
            : Algorithm<ResourceType, LabelContainerType>(resource_factory, params),
              greedy_(resource_factory, make_greedy_params(params)),
              tabu_(resource_factory, std::move(params)) {}

        ~ImprovingTabuSearch() override = default;

        [[nodiscard]] bool is_optimal() const override { return false; }

    protected:
        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Algorithm<ResourceType, LabelContainerType>::initialize(graph, cost_upper_bound);
        }

        void initialize_labels() override {}

        [[nodiscard]] size_t number_of_labels() const override { return 0; }

        [[nodiscard]] std::list<Label<ResourceType>*> get_labels_at_sinks() const override {
            return {};
        }

        std::vector<size_t> get_path_arc_ids(const Label<ResourceType>& /*label*/) override {
            throw std::runtime_error("ImprovingTabuSearch: no label-based path reconstruction");
        }

        void main_loop() override {
            // Phase 1: greedy construction with no upper bound.
            auto phase1 = greedy_.solve(this->graph_, std::numeric_limits<double>::infinity());
            double best_cost = this->cost_upper_bound_;
            for (auto& sol : phase1.solutions) {
                best_cost = std::min(best_cost, sol.cost);
                this->solutions_.insert(std::move(sol));
            }

            if (this->solutions_.size() >= this->params_.stop_after_X_solutions ||
                this->is_interrupted()) {
                return;
            }

            // Phase 2: tabu improvement starting from best-known upper bound.
            auto phase2 = tabu_.solve(this->graph_, best_cost);
            for (auto& sol : phase2.solutions) {
                this->solutions_.insert(std::move(sol));
            }
        }

    private:
        /// @brief Build greedy params: single solution, short iteration budget.
        static AlgorithmParams<LabelContainerType> make_greedy_params(
            const AlgorithmParams<LabelContainerType>& base) {
            auto p = base;
            p.stop_after_X_solutions = 1;
            p.release_after_solve = false;
            return p;
        }

        GreedyAlgorithm<ResourceType, LabelContainerType> greedy_;
        TabuSearchAlgorithm<ResourceType, LabelContainerType> tabu_;
};

}  // namespace rcspp
