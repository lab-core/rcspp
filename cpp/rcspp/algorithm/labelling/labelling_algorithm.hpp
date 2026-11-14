// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <list>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/algorithm_params.hpp"
#include "rcspp/algorithm/container_direction.hpp"
#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/labelling/completion_bound.hpp"
#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/parts/extension_outcome.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/schedule.hpp"
#include "rcspp/algorithm/labelling/search.hpp"
#include "rcspp/algorithm/labelling/step.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp::detail {

/// @brief A single-direction labelling search assembled from @p PartsType: one @ref Search, run
///        by the one @ref step, in @p Direction.
///
/// Each solve starts with an empty schedule and one start label per start node. The loop
/// (@ref main_loop) runs one @c step per iteration until the schedule is empty or a stop applies:
/// that is one phase. A label the per-node quota (`max_extended_labels_per_node`) or memory
/// pressure cuts is not lost. The schedule stores it aside, and @ref prepare_next_phase queues it
/// again for the next phase, up to `max_phases` (@c Algorithm::solve). Solutions are recorded
/// when a step takes an end-node label, if the retention records at take or
/// `return_dominated_solutions` is set, and after each phase from the labels stored at the end
/// nodes. A forward label holds the path from a source to its node; a backward label, the path
/// from its node to a sink.
///
/// @tparam ResourceType       The resource type carried by labels.
/// @tparam LabelContainerType The per-node container its params hold; a backward search keeps it
///                            turned backward (@ref make_directed_container).
/// @tparam Direction          @ref ForwardDirection or @ref BackwardDirection.
/// @tparam PartsType         The parts (@ref SearchParts).
template <typename ResourceType, typename LabelContainerType, typename Direction,
          typename PartsType>
    requires ResourceTypeConcept<ResourceType>
class LabellingAlgorithm : public Algorithm<ResourceType, LabelContainerType> {
        using Base = Algorithm<ResourceType, LabelContainerType>;

    public:
        /// @brief The per-node container this search keeps: @p LabelContainerType turned to
        ///        @p Direction.
        using container_type = directed_container_t<LabelContainerType, Direction>;
        using search_type = Search<ResourceType, Direction, container_type, PartsType>;

        LabellingAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                           AlgorithmParams<LabelContainerType> params)
            : Base(resource_factory, std::move(params)) {}

        [[nodiscard]] std::vector<SearchDirection> supported_directions() const override {
            return {Direction::backward ? SearchDirection::Backward : SearchDirection::Forward};
        }

    protected:
        /// @brief Starts the solve: an empty search and its context, and the completion bound
        ///        when the schedule's order or the incumbent prune reads it
        ///        (@ref compute_completion_bound). An admission rule that is not exact loses the
        ///        proof
        ///        (@c ProofLoss::RelaxedAdmission).
        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Base::initialize(graph, cost_upper_bound);
            search_.reset(graph->get_number_of_nodes(),
                          make_directed_container<Direction>(this->params_.labels));
            context_ = this->make_context(search_, Role::Single);
            // The incumbent prune adds the completion bound whenever it is valid (an additive
            // cost), so the bound is computed then, as well as for an order that reads it (A*).
            refresh_completion_bound(
                *graph,
                parts::reads_completion_bound_v<typename search_type::schedule_type> ||
                    context_.prune_adds_completion_bound);
            if (!decltype(search_.admission)::is_exact(context_.admission)) {
                this->proof_.record(ProofLoss::RelaxedAdmission);
            }
            search_.schedule.initialize(context_);
        }

        /// @brief Gives the search its completion bound when @p needed, and none otherwise.
        ///
        /// The bound reads the graph alone (not the arc mask): when the graph is promised
        /// unchanged since the last solve (@ref Algorithm::set_graph_unchanged), the bound that
        /// solve computed on it is kept, and not computed again.
        void refresh_completion_bound(const Graph<ResourceType>& graph, bool needed) {
            if (!needed) {
                search_.completion_bound = {};
                completion_bound_graph_ = nullptr;
            } else if (!this->is_graph_unchanged() || completion_bound_graph_ != &graph) {
                search_.completion_bound = compute_completion_bound<Direction>(graph);
                completion_bound_graph_ = &graph;
            }
        }

        /// @brief Empties the search for another run of this solve (@ref Search::restart): every
        ///        node's container and the schedule are emptied, so that the start labels can be
        ///        stored and queued again; the completion bound and the work count are kept.
        void restart_search() {
            search_.restart(this->graph_->get_number_of_nodes(),
                            make_directed_container<Direction>(this->params_.labels));
        }

        void initialize_labels() override {
            // At the start of a solve the pool is already empty (Algorithm::initialize clears it).
            // This matters when a driver starts another run (Driven::reseed): every label of the
            // previous run goes back to the pool, those waiting for a successor's release
            // (pending_release) included.
            this->label_pool_.release_all_labels();
            search_.create_start_labels(*this->graph_, &this->label_pool_);
        }

        [[nodiscard]] size_t number_of_waiting_labels() const override {
            return search_.schedule.size();
        }

        /// @brief One phase: at most `max_iterations` steps. Each phase has its own count.
        void main_loop() override { run_loop(this->params_.max_iterations); }

        /// @brief Runs @ref step until no label waits, a stop applies, or @p max_steps steps are
        ///        done.
        ///
        /// A step takes one label or, with a pulling schedule, visits one node. Before each step
        /// the memory check runs (@c Algorithm::check_memory). It reads the memory only every
        /// `memory_check_interval` iterations of this call, and the count restarts at each call, so
        /// a driven run shorter than that interval never reads it.
        ///
        /// @param max_steps The most steps this call takes: `max_iterations` for a phase,
        ///                   @c kNoLimit for a driven run.
        /// @return @c true when a step ended the loop (@ref SearchContext::should_end_run).
        bool run_loop(size_t max_steps) {
            size_t iteration = 0;
            while (number_of_waiting_labels() > 0 && iteration < max_steps &&
                   !this->should_end_loop()) {
                if (this->check_memory(iteration)) {
                    break;
                }
                ++iteration;
                if (step(
                        search_,
                        context_,
                        [this](const Label<ResourceType>& label) { this->record_solution(label); },
                        [this](parts::ExtensionOutcome outcome) {
                            this->count_extension(outcome);
                        })) {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] std::list<Label<ResourceType>*> labels_at_end_nodes() const override {
            return search_.labels_at_end_nodes(*this->graph_);
        }

        void print_labels() const override { search_.print(*this->graph_); }

        /// @brief Queues again what the schedule's cuts stored aside, and resets its per-node
        ///        counts.
        ///
        /// Called between phases (@c Algorithm::solve), and once more at the end of the solve, so
        /// that labels still stored aside count as waiting and the status reads Truncated.
        /// A driven run also calls it between its phases (@c Driven::main_loop).
        void prepare_next_phase() override { search_.schedule.prepare_next_phase(context_); }

        /// @brief Relieves memory pressure (@c Algorithm::relieve_memory_pressure): the quota
        ///        drops to `memory_pressure_max_labels_per_node`, and the schedule keeps about
        ///        that many labels per node and stores the rest aside for the next phase. From
        ///        the second call on, what the earlier calls stored aside is released first, and
        ///        is lost. Any call makes the result not a proof
        ///        (@c ProofLoss::MemoryPressure).
        void on_memory_pressure() override { this->relieve_memory_pressure(search_, context_); }

        /// @brief Releases the pool, then forgets the containers and the schedule, so that no
        ///        pointer to a released label remains.
        void release_label_memory() override {
            Base::release_label_memory();
            search_.clear();
        }

        /// @brief The arc ids of the path @p label holds, in path order.
        std::vector<size_t> get_path_arc_ids(const Label<ResourceType>& label) override {
            if constexpr (Direction::backward) {
                std::vector<size_t> arc_ids;
                append_suffix_arc_ids(label, &arc_ids);
                return arc_ids;
            } else {
                return prefix_arc_ids(label);
            }
        }

        /// @brief Records the path @p label holds: from a source to its node going forward, from
        ///        its node to a sink going backward.
        void record_solution(const Label<ResourceType>& label) override {
            if constexpr (Direction::backward) {
                this->record_backward_solution(label);
            } else {
                Base::record_solution(label);
            }
        }

        search_type search_;
        SearchContext<ResourceType> context_;
        /// @brief The graph @c search_.completion_bound was computed on; null when it holds none.
        const Graph<ResourceType>* completion_bound_graph_ = nullptr;
};

}  // namespace rcspp::detail
