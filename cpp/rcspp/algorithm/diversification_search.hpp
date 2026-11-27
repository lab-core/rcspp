// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
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
/// The tabu mechanism keeps a list of tabu arcs (the tabu list), each for a configurable tenure.
/// The arcs of each path found become hard arcs of the inner algorithm's arc mask (@ref ArcMask)
/// for a number of iterations, so that the next solves cannot use them. The graph itself is not
/// edited, so arc ids, and the order in which a search tries a node's arcs, stay the graph's.
/// Random noise can be added to the tenure to diversify the search further. If this algorithm
/// was given a mask (@c Algorithm::set_arc_mask), its hard arcs stay out as well.
///
/// Typical use cases include:
///   - Metaheuristic frameworks for RCSPP where solution diversity is important.
///   - Escaping local optima in iterative improvement algorithms.
///   - Generating a set of diverse solutions for post-processing or ensemble methods.
///
/// It is reached through @c strategy::Diversification<Inner>, which names the inner strategy.
/// The inner algorithm is built from this algorithm's params, except that it always searches
/// forward, keeps one solution per solve, and makes at most @ref inner_iteration_budget
/// iterations per solve:
/// - for a labelling search such as Greedy, an iteration is a label taken, and the budget is
///   20 per node of the graph, and never fewer than 1000;
/// - for a driven search such as Tabu, an iteration is an episode (a whole dive), and the
///   budget is 1000.
///
/// Its solve never ends @c Complete, because @ref number_of_waiting_labels always answers 1. The
/// status is @c MaxSolutions when it found @c max_solutions paths, the status of the limit that
/// stopped it (time limit, interrupt, memory limit), or @c Truncated otherwise, even when the
/// inner search found no path left. So it is never proven: @c is_optimal and
/// @c SolveResult::proven are always false.
///
/// @tparam ResourceType       The resource type carried by labels.
/// @tparam LabelContainerType The per-node container.
/// @tparam Inner              The strategy solved each time (a descriptor, as @c strategy::Greedy).
template <typename ResourceType, typename LabelContainerType, typename Inner>
class DiversificationSearch : public Algorithm<ResourceType, LabelContainerType> {
    public:
        /// @brief An algorithm that solves @p Inner, built with @p params, again and again.
        ///
        /// Only the inner algorithm prunes: this one keeps @p params with both prunes off, so that
        /// its own solve does not run the checks the prunes need (the cost's additivity, and the
        /// arcs' shares, read over every arc), which the inner algorithm runs at its first solve.
        ///
        /// @param resource_factory The factory the inner algorithm's labels are built with.
        /// @param params           The params; the inner algorithm's are built from them.
        DiversificationSearch(ResourceFactory<ResourceType>* resource_factory,
                              AlgorithmParams<LabelContainerType> params)
            : Algorithm<ResourceType, LabelContainerType>(resource_factory, without_prunes(params)),
              tabu_(this->params_, ArcMask::State::Hard),
              resource_factory_(resource_factory),
              prune_by_incumbent_(params.prune_by_incumbent),
              prune_by_threshold_(params.prune_by_threshold) {}

        /// @brief The most iterations an inner solve may make on a graph of @p number_of_nodes
        ///        nodes.
        ///
        /// The budget bounds the work of an inner solve when no path is left, yet leaves room for
        /// its dives to reach a sink. A labelling search counts the labels it takes, and a dive
        /// takes one label per node it passes, so its budget is 20 per node (about 20 dives
        /// through every node), and never fewer than 1000. A driven search
        /// (@c detail::DrivenStrategy) counts episodes, each a whole dive whatever the size of the
        /// graph, so its budget is 1000.
        [[nodiscard]] static size_t inner_iteration_budget(size_t number_of_nodes) {
            if constexpr (detail::DrivenStrategy<Inner>) {
                return kMinInnerIterations;
            } else {
                return std::max(kMinInnerIterations, kInnerIterationsPerNode * number_of_nodes);
            }
        }

        [[nodiscard]] bool is_optimal() const override { return false; }

    protected:
        /// @brief Starts the solve: builds the inner algorithm for the graph's size, and starts
        ///        its arc mask as a copy of the mask this algorithm was given (all open if none).
        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Algorithm<ResourceType, LabelContainerType>::initialize(graph, cost_upper_bound);
            build_inner(inner_iteration_budget(graph->get_number_of_nodes()));
            tabu_.begin(*graph, this->get_arc_mask());
            algo_->set_arc_mask(&tabu_.mask());
            // The first inner solve reads the graph afresh; the next ones, on the same graph
            // with only the mask changed, may keep what it computed from it (the bound).
            algo_->set_graph_unchanged(false);
        }

        /// @brief Builds the inner algorithm with @p max_iterations, unless the one built for the
        ///        last solve has it already (its pool is then reused).
        void build_inner(size_t max_iterations) {
            if (algo_ != nullptr && inner_max_iterations_ == max_iterations) {
                return;
            }
            auto inner_params = this->params_;
            inner_params.prune_by_incumbent = prune_by_incumbent_;
            inner_params.prune_by_threshold = prune_by_threshold_;
            inner_params.direction = SearchDirection::Forward;
            inner_params.max_solutions = 1;  // only need one solution per iteration
            inner_params.max_iterations = max_iterations;
            inner_params.release_after_solve = false;  // pool reused each iteration; skip shrink
            algo_ = detail::make_algorithm<Inner, ResourceType, LabelContainerType>(
                resource_factory_,
                std::move(inner_params));
            inner_max_iterations_ = max_iterations;
        }

        /// @brief Solves the inner algorithm again and again, up to max_iterations times or until
        ///        max_solutions paths are found, or until it finds none.
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

                // An inner solve is far shorter than memory_check_interval, so its own loop never
                // reads the memory: read it once per inner solve, and stop at the limit (the
                // status then reads MemoryLimit).
                if (this->memory_limit_.is_exceeded()) {
                    break;
                }
            }

            LOG_DEBUG("DiversificationSearch: WHILE nb iter: ", i, "\n");
        }

        /// @brief Makes the arcs of @p sol tabu, so that the next inner solves leave them out.
        ///        Arcs with an end in @c tabu_exempt_node_ids are not made tabu.
        ///
        /// When the first arc is made tabu, @c ProofLoss::HeuristicDriver is recorded. This is
        /// for the record only: this algorithm's solve never ends @c Complete, so it is never
        /// proven, and its @c proof_lost_by names its status instead.
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
        static constexpr size_t kMinInnerIterations = 1000;
        static constexpr size_t kInnerIterationsPerNode = 20;

        std::unique_ptr<Algorithm<ResourceType, LabelContainerType>> algo_;
        detail::TabuArcs tabu_;  ///< the tabu arcs, hard in the mask the inner algorithm reads
        ResourceFactory<ResourceType>* resource_factory_;
        size_t inner_max_iterations_ = 0;  // the budget algo_ was built with
        bool prune_by_incumbent_;          ///< the params', for the inner algorithm only
        bool prune_by_threshold_;          ///< the params', for the inner algorithm only

        /// @brief @p params with both prunes off: this algorithm's own.
        static AlgorithmParams<LabelContainerType> without_prunes(
            AlgorithmParams<LabelContainerType> params) {
            params.prune_by_incumbent = false;
            params.prune_by_threshold = false;
            return params;
        }
};

}  // namespace rcspp
