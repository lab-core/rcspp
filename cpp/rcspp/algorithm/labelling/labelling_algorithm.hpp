// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <list>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/container_direction.hpp"
#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/policies.hpp"
#include "rcspp/algorithm/labelling/schedules.hpp"
#include "rcspp/algorithm/labelling/search.hpp"
#include "rcspp/algorithm/labelling/step.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp::detail {

/// @brief A single-direction labelling search assembled from @p PresetType: one @ref Search, run
///        by the one @ref step, in @p Direction.
///
/// Each solve starts with an empty schedule and one start label per start node; the loop takes
/// one unit of work per iteration until the schedule is empty or a stop applies, checking the
/// memory first; the solutions are read from the labels at the end nodes at the end. A forward
/// label holds the path from a source to its node; a backward label, the path from its node to a
/// sink.
///
/// @tparam ResourceType       The resource type carried by labels.
/// @tparam LabelContainerType The per-node container its params hold; a backward search keeps it
///                            turned backward (@ref make_directed_container).
/// @tparam Direction          @ref ForwardDirection or @ref BackwardDirection.
/// @tparam PresetType         The parts (@ref Preset).
template <typename ResourceType, typename LabelContainerType, typename Direction,
          typename PresetType>
    requires ResourceTypeConcept<ResourceType>
class LabellingAlgorithm : public Algorithm<ResourceType, LabelContainerType> {
        using Base = Algorithm<ResourceType, LabelContainerType>;

    public:
        /// @brief The per-node container this search keeps: @p LabelContainerType turned to
        ///        @p Direction.
        using container_type = directed_container_t<LabelContainerType, Direction>;
        using search_type = Search<ResourceType, Direction, container_type, PresetType>;

        LabellingAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                           AlgorithmParams<LabelContainerType> params)
            : Base(resource_factory, std::move(params)) {}

        [[nodiscard]] std::vector<SearchDirection> supported_directions() const override {
            return {Direction::backward ? SearchDirection::Backward : SearchDirection::Forward};
        }

    protected:
        /// @brief Starts the solve: an empty search and its context.
        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Base::initialize(graph, cost_upper_bound);
            search_.reset(graph->get_number_of_nodes(),
                          make_directed_container<Direction>(this->params_.labels));
            ctx_ = this->make_context(search_, Role::Single);
            search_.schedule.initialize(ctx_);
        }

        void initialize_labels() override {
            // Release every label of the previous solve (pending ones included), so the pool
            // starts afresh.
            this->label_pool_.release_all_labels();
            search_.create_start_labels(*this->graph_, &this->label_pool_);
        }

        [[nodiscard]] size_t waiting_labels() const override { return search_.schedule.size(); }

        void main_loop() override {
            size_t iteration = 0;
            while (waiting_labels() > 0 && !this->should_stop(iteration)) {
                if (this->check_memory(iteration)) {
                    break;
                }
                ++iteration;
                if (step(
                        search_,
                        ctx_,
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

        void prepare_next_phase() override { search_.schedule.prepare_next_phase(ctx_); }

        void on_memory_pressure() override { this->relieve_memory_pressure(search_, ctx_); }

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
        SearchContext<ResourceType> ctx_;
};

}  // namespace rcspp::detail
