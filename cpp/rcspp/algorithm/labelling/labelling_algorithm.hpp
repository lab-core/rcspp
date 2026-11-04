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
            const bool reads_bound =
                parts::reads_completion_bound_v<typename search_type::schedule_type> ||
                context_.prune_adds_completion_bound;
            search_.completion_bound = reads_bound ? compute_completion_bound<Direction>(*graph)
                                                   : CompletionBound<ResourceType>{};
            if (!decltype(search_.admission)::is_exact(context_.admission)) {
                this->proof_.record(ProofLoss::RelaxedAdmission);
            }
            search_.schedule.initialize(context_);
        }

        void initialize_labels() override {
            // At the start of a solve the pool is already empty (Algorithm::initialize clears
            // it), so this releases nothing: it only makes the start safe to repeat.
            this->label_pool_.release_all_labels();
            search_.create_start_labels(*this->graph_, &this->label_pool_);
        }

        [[nodiscard]] size_t number_of_waiting_labels() const override {
            return search_.schedule.size();
        }

        /// @brief One phase: at most `max_iterations` steps. Each phase has its own count.
        void main_loop() override {
            size_t iteration = 0;
            while (number_of_waiting_labels() > 0 && !this->should_end_loop(iteration)) {
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
                    return;
                }
            }
        }

        [[nodiscard]] std::list<Label<ResourceType>*> labels_at_end_nodes() const override {
            return search_.labels_at_end_nodes(*this->graph_);
        }

        void print_labels() const override { search_.print(*this->graph_); }

        /// @brief Queues again what the schedule's cuts stored aside, and resets its per-node
        ///        counts.
        ///
        /// Called between phases (@c Algorithm::solve), and once more at the end of the solve, so
        /// that labels still held back count as waiting and the status reads Truncated.
        void prepare_next_phase() override { search_.schedule.prepare_next_phase(context_); }

        /// @brief Relieves memory pressure (@c Algorithm::relieve_memory_pressure): the quota
        ///        drops to `memory_pressure_max_labels_per_node`, and the schedule keeps about
        ///        that many labels per node and stores the rest aside for the next phase. From
        ///        the second call on, what the earlier calls stored aside is released first, and
        ///        is lost. Any call makes the result possibly not optimal
        ///        (@c Algorithm::memory_pressure_was_triggered).
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
};

}  // namespace rcspp::detail
