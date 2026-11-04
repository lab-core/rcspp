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
#include "rcspp/algorithm/tabu/tabu_list.hpp"

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
/// most @ref inner_iteration_budget iterations per solve (labels taken, for a labelling search such
/// as Greedy): 20 for each node of the graph, and never fewer than 1000.
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
              tabu_(this->params_.seed),
              resource_factory_(resource_factory) {}

        /// @brief The inner search's iteration budget per solve on a graph of @p number_of_nodes
        ///        nodes: a bound on the work when no path is left, roomy enough for the dives to
        ///        reach a sink. The inner search counts labels taken, and a dive takes one per node
        ///        it passes, so the budget is 20 per node (about 20 dives through every node), and
        ///        never fewer than 1000.
        [[nodiscard]] static size_t inner_iteration_budget(size_t number_of_nodes) {
            return std::max(kMinInnerIterations, kInnerIterationsPerNode * number_of_nodes);
        }

        [[nodiscard]] bool is_optimal() const override { return false; }

        // Run diversification search using tabu-based strategy and collect solutions. The search
        // runs up to max_iterations or max_solutions.
    protected:
        /// @brief Starts the solve: builds the inner algorithm for the graph's size, and the mask
        ///        it reads starts as the one this algorithm was given, or open.
        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Algorithm<ResourceType, LabelContainerType>::initialize(graph, cost_upper_bound);
            build_inner(inner_iteration_budget(graph->get_number_of_nodes()));
            const size_t number_of_arc_ids = graph->get_arc_id_bound();
            mask_.reset(number_of_arc_ids);
            if (const ArcMask* given = this->get_arc_mask(); given != nullptr) {
                for (size_t arc_id = 0; arc_id < number_of_arc_ids; ++arc_id) {
                    mask_.set(arc_id, given->state(arc_id));
                }
            }
            algo_->set_arc_mask(&mask_);
        }

        /// @brief Builds the inner algorithm with @p max_iterations, unless the one built for the
        ///        last solve has it already (its pool is then reused).
        void build_inner(size_t max_iterations) {
            if (algo_ != nullptr && inner_max_iterations_ == max_iterations) {
                return;
            }
            auto inner_params = this->params_;
            inner_params.direction = SearchDirection::Forward;
            inner_params.max_solutions = 1;  // only need one solution per iteration
            inner_params.max_iterations = max_iterations;
            inner_params.release_after_solve = false;  // pool reused each iteration; skip shrink
            algo_ = detail::make_algorithm<Inner, ResourceType, LabelContainerType>(
                resource_factory_,
                std::move(inner_params));
            inner_max_iterations_ = max_iterations;
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

                // decrement tenures, and put back the arcs whose tenure expired: each goes back
                // to its state in the mask given
                const ArcMask* given = this->get_arc_mask();
                tabu_.age([this, given](size_t arc_id) {
                    mask_.set(arc_id,
                              given != nullptr ? given->state(arc_id) : ArcMask::State::Open);
                });

                // An inner solve is far shorter than memory_check_interval, so its own loop never
                // reads the memory: read it once per inner solve, and stop at the limit (the
                // status then reads MemoryLimit).
                if (this->memory_limit_.is_exceeded()) {
                    break;
                }
            }

            LOG_DEBUG("DiversificationSearch: WHILE nb iter: ", i, "\n");
        }

        void tabu_solution(const Solution& sol) {
            // leave the following arcs out of the next iterations
            for (auto arc_id : sol.path_arc_ids) {
                // skip an arc already out, or exempt
                const auto* arc = this->graph_->get_arc(arc_id);
                if (mask_.is_hard(arc_id) ||
                    this->params_.tabu_exempt_node_ids.contains(arc->origin->id) ||
                    this->params_.tabu_exempt_node_ids.contains(arc->destination->id)) {
                    continue;
                }
                mask_.set(arc_id, ArcMask::State::Hard);
                tabu_.add(arc_id, this->params_.tabu_tenure, this->params_.tabu_random_noise);
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
        TabuList tabu_;
        ArcMask mask_;  ///< what the inner algorithm leaves out: the tabu arcs, hard
        ResourceFactory<ResourceType>* resource_factory_;
        size_t inner_max_iterations_ = 0;  // the budget algo_ was built with
};

}  // namespace rcspp
