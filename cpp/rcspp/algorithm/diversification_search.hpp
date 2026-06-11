// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/greedy.hpp"
#include "rcspp/algorithm/tabu_list.hpp"

namespace rcspp {

/**
 * @brief DiversificationSearch: Tabu-based diversification algorithm for RCSPP.
 *
 * This class implements a diversification strategy for resource-constrained shortest path problems
 * (RCSPP) using a tabu-based search. The algorithm wraps another Algorithm instance and repeatedly
 * solves the problem, each time removing arcs from the graph that were used in previous solutions
 * (tabu arcs), to encourage exploration of new and diverse solutions and to escape local optima.
 *
 * The tabu mechanism works by maintaining a list of recently removed arcs (tabu list) with a
 * configurable tenure. Arcs used in a solution are removed from the graph for a number of
 * iterations, preventing their immediate reuse. Random noise can be added to the tenure to further
 * diversify the search.
 *
 * Typical use cases include:
 *   - Metaheuristic frameworks for RCSPP where solution diversity is important.
 *   - Escaping local optima in iterative improvement algorithms.
 *   - Generating a set of diverse solutions for post-processing or ensemble methods.
 *
 * Usage: Construct with a resource factory, algorithm parameters, and a unique_ptr to the wrapped
 * algorithm.
 */
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>>
class DiversificationSearch : public Algorithm<ResourceType, LabelContainerType> {
    public:
        DiversificationSearch(
            ResourceFactory<ResourceType>* resource_factory,
            AlgorithmParams<LabelContainerType> params,
            std::unique_ptr<Algorithm<ResourceType, LabelContainerType>> algo = nullptr)
            : Algorithm<ResourceType, LabelContainerType>(resource_factory, std::move(params)),
              algo_(std::move(algo)),
              tabu_(this->params_.seed) {
            if (algo_ == nullptr) {
                // create algorithm params
                auto alg_params = this->params_;
                alg_params.stop_after_X_solutions = 1;  // only need one solution per iteration
                alg_params.max_iterations = 20;  // ensure early termination if needed // NOLINT
                alg_params.release_after_solve = false;  // pool reused each iteration; skip shrink
                algo_ = std::make_unique<GreedyAlgorithm<ResourceType, LabelContainerType>>(
                    resource_factory,
                    alg_params);
            }
        }

        [[nodiscard]] bool is_optimal() const override { return false; }

        // Run diversification search using tabu-based strategy and collect solutions. The search
        // runs up to max_iterations or stop_after_X_solutions.
    protected:
        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Algorithm<ResourceType, LabelContainerType>::initialize(graph, cost_upper_bound);
            graph_copy_ = std::move(graph->clone());
        }
        void main_loop() override {
            // check stopping criteria
            if (this->params_.max_iterations >= MAX_INT) {
                LOG_ERROR(
                    "max_iterations needs to be set to a finite value for DiversificationSearch in "
                    "order to "
                    "stop.\n");
                return;
            }

            size_t i = 0;
            while (!this->should_stop(i)) {
                ++i;

                // Rebuild the CSR index so GreedyAlgorithm::get_out_arcs() returns correct
                // data.  remove_arc() / restore_arc() both invalidate csr_valid_; build_csr()
                // is a no-op when the index is already current.
                graph_copy_->build_csr();

                // solve (important to clear the label pool, as the graph is changing)
                std::vector<Solution> sols =
                    algo_->solve(graph_copy_.get(), this->cost_upper_bound_).solutions;
                if (sols.empty()) {
                    break;
                }

                // process solutions
                bool added = false;
                for (auto& sol : sols) {
                    // make tabu
                    tabu_solution(sol);
                    // check if we found the solution
                    if (this->solutions_.contains(sol)) {
                        continue;
                    }
                    // then add the solution
                    this->solutions_.insert(std::move(sol));
                    added = true;
                }

                // grow extra tenure when no novel solution was added (matches the
                // original heuristic; success leaves the extra tenure unchanged)
                if (!added) {
                    tabu_.grow_extra();
                }

                // decrement tenures and restore arcs whose tenure expired
                tabu_.age([&](size_t arc_id) { graph_copy_->restore_arc(arc_id); });
            }

            LOG_DEBUG("DiversificationSearch: WHILE nb iter: ", i, "\n");
        }

        void tabu_solution(const Solution& sol) {
            // remove the following arcs from the graph for the next iteration
            for (auto arc_id : sol.path_arc_ids) {
                // check if arc is already removed or can be removed
                const auto* arc = graph_copy_->get_arc(arc_id);
                if (arc == nullptr || this->params_.forbidden_tabu.contains(arc->origin->id) ||
                    this->params_.forbidden_tabu.contains(arc->destination->id)) {
                    continue;
                }
                if (graph_copy_->remove_arc(arc_id)) {
                    tabu_.add(arc_id, this->params_.tabu_tenure, this->params_.tabu_random_noise);
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

        std::vector<size_t> get_path_arc_ids(const Label<ResourceType>& label) override {
            throw std::runtime_error("No get_path_arc_ids");
        }

    private:
        std::unique_ptr<Graph<ResourceType>> graph_copy_;
        std::unique_ptr<Algorithm<ResourceType, LabelContainerType>> algo_;
        TabuList tabu_;
};

}  // namespace rcspp
