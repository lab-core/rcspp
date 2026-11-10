// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/algorithm_params.hpp"
#include "rcspp/algorithm/arc_mask.hpp"
#include "rcspp/algorithm/limits.hpp"
#include "rcspp/algorithm/make_algorithm.hpp"
#include "rcspp/algorithm/proof_record.hpp"
#include "rcspp/algorithm/tabu/tabu_arcs.hpp"

namespace rcspp {

/// @brief DiversificationSearch: Tabu-based diversification algorithm for RCSPP.
///
/// This class implements a diversification strategy for resource-constrained shortest path problems
/// (RCSPP) using a tabu-based search. The algorithm wraps another Algorithm instance and repeatedly
/// solves the problem, each time leaving out the arcs used in previous solutions (tabu arcs), to
/// encourage exploration of new and diverse solutions and to escape local optima.
///
/// The tabu mechanism works by maintaining a list of recent tabu arcs (tabu list) with a
/// configurable tenure. Arcs used in a solution are hard in the inner algorithm's arc mask
/// (@ref ArcMask) for a number of iterations, preventing their reuse; the graph is not edited, so
/// arc ids and the arcs' order stay the graph's. Random noise can be added to the tenure to further
/// diversify the search. A mask the algorithm was given (@c Algorithm::set_arc_mask) is read too:
/// its hard arcs stay out.
///
/// Typical use cases include:
///   - Metaheuristic frameworks for RCSPP where solution diversity is important.
///   - Escaping local optima in iterative improvement algorithms.
///   - Generating a set of diverse solutions for post-processing or ensemble methods.
///
/// Reached through @c strategy::Diversification<Inner>, which names the inner strategy; the inner
/// algorithm is built from this algorithm's params, searching forward, with one solution and at
/// most 1000 iterations per solve (labels taken, for a labelling search such as Greedy).
///
/// @tparam ResourceType       The resource type carried by labels.
/// @tparam LabelContainerType The per-node container.
/// @tparam Inner              The strategy solved each time (a descriptor, as @c strategy::Greedy).
template <typename ResourceType, typename LabelContainerType, typename Inner>
class DiversificationSearch : public Algorithm<ResourceType, LabelContainerType> {
    public:
        DiversificationSearch(ResourceFactory<ResourceType>* resource_factory,
                              AlgorithmParams<LabelContainerType> params)
            : Algorithm<ResourceType, LabelContainerType>(resource_factory, std::move(params)),
              tabu_(this->params_, ArcMask::State::Hard) {
            auto inner_params = this->params_;
            inner_params.direction = SearchDirection::Forward;
            inner_params.max_solutions = 1;  // only need one solution per iteration
            // A bound on the work when no path is left, roomy enough for a first dive to reach a
            // sink: the inner search counts labels taken.
            inner_params.max_iterations = 1000;        // NOLINT(readability-magic-numbers)
            inner_params.release_after_solve = false;  // pool reused each iteration; skip shrink
            algo_ = detail::make_algorithm<Inner, ResourceType, LabelContainerType>(
                resource_factory,
                std::move(inner_params));
        }

        [[nodiscard]] bool is_optimal() const override { return false; }

        // Run diversification search using tabu-based strategy and collect solutions. The search
        // runs up to max_iterations or max_solutions.
    protected:
        /// @brief Starts the solve: the mask the inner algorithm reads starts as the one this
        ///        algorithm was given, or open.
        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Algorithm<ResourceType, LabelContainerType>::initialize(graph, cost_upper_bound);
            tabu_.begin(*graph, this->get_arc_mask());
            algo_->set_arc_mask(&tabu_.mask());
            // The first inner solve reads the graph afresh; the next ones, on the same graph
            // with only the mask changed, may keep what it computed from it (the bound).
            algo_->set_graph_unchanged(false);
        }

        void main_loop() override {
            // check stopping criteria
            if (this->params_.max_iterations >= kNoLimit) {
                LOG_ERROR(
                    "max_iterations needs to be set to a finite value for DiversificationSearch in "
                    "order to "
                    "stop.\n");
                return;
            }

            size_t i = 0;
            while (!this->should_end_loop(i)) {
                ++i;

                // solve, the tabu arcs hard in the inner algorithm's mask
                std::vector<Solution> sols =
                    algo_->solve(this->graph_, this->upper_bound_).solutions;
                algo_->set_graph_unchanged(true);
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
                    tabu_.list().grow_extra();
                }

                // decrement tenures, and put back the arcs whose tenure expired: each goes back
                // to its state in the mask given
                tabu_.age();
            }

            LOG_DEBUG("DiversificationSearch: WHILE nb iter: ", i, "\n");
        }

        /// @brief Leaves the arcs of @p sol out of the next iterations, but those with an exempt
        ///        end (@c tabu_exempt_node_ids).
        void tabu_solution(const Solution& sol) {
            if (tabu_.add(*this->graph_, sol.path_arc_ids)) {
                this->proof_.record(ProofLoss::HeuristicDriver);
            }
        }

        void initialize_labels() override {}

        [[nodiscard]] size_t number_of_waiting_labels() const override {
            return 1;
        }  // dummy -> never stop on labels

        [[nodiscard]] std::list<Label<ResourceType>*> labels_at_end_nodes() const override {
            return {};
        }

        std::vector<size_t> get_path_arc_ids(const Label<ResourceType>& label) override {
            throw std::runtime_error("No get_path_arc_ids");
        }

    private:
        std::unique_ptr<Algorithm<ResourceType, LabelContainerType>> algo_;
        detail::TabuArcs tabu_;  ///< the tabu arcs, hard in the mask the inner algorithm reads
};

}  // namespace rcspp
