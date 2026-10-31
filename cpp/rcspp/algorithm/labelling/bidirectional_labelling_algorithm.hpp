// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/container_direction.hpp"
#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/label_buckets.hpp"
#include "rcspp/algorithm/labelling/bounds.hpp"
#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/half_way.hpp"
#include "rcspp/algorithm/labelling/join.hpp"
#include "rcspp/algorithm/labelling/policies.hpp"
#include "rcspp/algorithm/labelling/schedules.hpp"
#include "rcspp/algorithm/labelling/search.hpp"
#include "rcspp/algorithm/labelling/step.hpp"
#include "rcspp/utils/logger.hpp"

namespace rcspp::detail {

/// @brief A stop that ends a bidirectional search while labels are still waiting to be extended.
///
/// `max_solutions` and `max_iterations` are not run-level stops: the join always runs after
/// either.
enum class RunLevelStop { None, Timeout, Interrupted, MemoryLimit };

/// @brief Whether the join still runs after @p stop.
///
/// A timeout or the memory limit means "stop searching", but the paths both halves already hold
/// are valid columns that a pricing loop needs. So the join still runs, keeping only paths
/// cheaper than the best one found so far, which keeps its output small. An interrupt means
/// "stop now", so the join is skipped.
///
/// @param stop Why the search stopped.
/// @return @c true unless the search was interrupted.
[[nodiscard]] constexpr bool join_runs_after(RunLevelStop stop) {
    return stop != RunLevelStop::Interrupted;
}

/// @brief Bidirectional labelling: a forward search, a backward search, and a join.
///
/// Two searches (@ref Search), one per direction, share one label pool and one incumbent. Each
/// iteration runs the one @ref step on the side @p InterleaveType picks. Each side keeps a label
/// past the half-way point stored, unextended, and prunes on its cost plus its completion bound;
/// what a quota or memory pressure cuts is abandoned (left in its container, never extended, and
/// counted), since the search runs one phase. The join then pairs the two halves.
///
/// The model checks it needs (@c BackwardExtensionCheck and @c JoinCheck) run before the solve.
///
/// @tparam ResourceType         The resource type carried by labels.
/// @tparam LabelContainerType   The per-node container of the params; each side keeps it turned
///                              to its direction (@ref make_directed_container).
/// @tparam CriticalResourceType The critical resource's type (the clock).
/// @tparam ForwardPresetType    The forward side's parts (@ref Preset).
/// @tparam BackwardPresetType   The backward side's parts.
/// @tparam InterleaveType       Which side goes next (@c parts::Interleave).
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>,
          typename CriticalResourceType = RealResource,
          typename ForwardPresetType = Preset<parts::FifoSchedule<ResourceType>>,
          typename BackwardPresetType = ForwardPresetType,
          typename InterleaveType = parts::LeastWorkFirst>
    requires ResourceTypeConcept<ResourceType>
class BidirectionalLabellingAlgorithm : public Algorithm<ResourceType, LabelContainerType> {
        using Base = Algorithm<ResourceType, LabelContainerType>;

    public:
        /// @brief The forward container: the caller's container, turned forward.
        using forward_container_type = directed_container_t<LabelContainerType, ForwardDirection>;
        /// @brief The backward container: the caller's container, turned backward.
        using backward_container_type = directed_container_t<LabelContainerType, BackwardDirection>;

        using forward_search_type =
            Search<ResourceType, ForwardDirection, forward_container_type, ForwardPresetType>;
        using backward_search_type =
            Search<ResourceType, BackwardDirection, backward_container_type, BackwardPresetType>;

        BidirectionalLabellingAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                                        AlgorithmParams<LabelContainerType> params)
            : Base(resource_factory, std::move(params)) {}

        [[nodiscard]] std::vector<SearchDirection> supported_directions() const override {
            return {SearchDirection::Bidirectional};
        }

        /// @brief The forward labels, by node position, for diagnostics and tests.
        [[nodiscard]] const std::vector<forward_container_type>& forward_labels() const {
            return forward_.labels.containers();
        }

        /// @brief The backward labels, by node position, for diagnostics and tests.
        [[nodiscard]] const std::vector<backward_container_type>& backward_labels() const {
            return backward_.labels.containers();
        }

    protected:
        /// @brief Starts the solve: the half-way point (off, with a reason, when it does not fit
        ///        the model), both sides' completion bounds when a side reads one, and the two
        ///        contexts.
        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Base::initialize(graph, cost_upper_bound);

            const size_t num_nodes = graph->get_number_of_nodes();
            forward_.reset(num_nodes,
                           make_directed_container<ForwardDirection>(this->params_.labels));
            backward_.reset(num_nodes,
                            make_directed_container<BackwardDirection>(this->params_.labels));
            abandoned_labels_ = 0;
            joined_paths_ = 0;

            half_way_ = make_half_way<CriticalResourceType, ResourceType>(this->params_);
            half_way_off_reason_.clear();
            if (const auto reason =
                    detail::half_way_off_reason<CriticalResourceType>(*graph, this->params_)) {
                half_way_.turn_off();
                half_way_off_reason_ = *reason;
            }

            // A side's bound is read by the completion-bound prune, and by its schedule when the
            // order reads it (A*'s). A side neither reads keeps an empty bound, and no relaxation
            // runs for it.
            const bool prune = this->params_.prune_by_incumbent;
            forward_.bound = {};
            backward_.bound = {};
            if (prune || parts::reads_bound_v<typename forward_search_type::schedule_type>) {
                forward_.bound = completion_bounds<ForwardDirection>(*graph);
            }
            if (prune || parts::reads_bound_v<typename backward_search_type::schedule_type>) {
                backward_.bound = completion_bounds<BackwardDirection>(*graph);
            }

            forward_ctx_ = this->make_context(forward_,
                                              Role::BidirectionalSide,
                                              &abandoned_labels_,
                                              &half_way_);
            backward_ctx_ = this->make_context(backward_,
                                               Role::BidirectionalSide,
                                               &abandoned_labels_,
                                               &half_way_);
            forward_.schedule.initialize(forward_ctx_);
            backward_.schedule.initialize(backward_ctx_);
        }

        void initialize_labels() override {
            // Release every label of the previous solve (pending ones included), so the pool
            // starts afresh.
            this->label_pool_.release_all_labels();
            forward_.create_start_labels(*this->graph_, &this->label_pool_);
            backward_.create_start_labels(*this->graph_, &this->label_pool_);
        }

        [[nodiscard]] size_t waiting_labels() const override {
            return forward_.schedule.size() + backward_.schedule.size();
        }

        /// @brief Whether the quota or memory pressure abandoned a label this solve.
        ///
        /// A bidirectional search runs one phase, so what a cut takes off a schedule is never
        /// extended: the schedules can empty without every label having been processed.
        [[nodiscard]] bool has_abandoned_labels() const override { return abandoned_labels_ > 0; }

        /// @brief Interleaves the two searches, one @ref step at a time, on the side
        ///        @p InterleaveType picks.
        ///
        /// The memory is checked first in each iteration; a spent budget (`max_solutions`) ends
        /// the loop.
        void main_loop() override {
            size_t iteration = 0;
            while (waiting_labels() > 0 && !this->should_stop(iteration)) {
                if (this->check_memory(iteration)) {
                    break;
                }
                ++iteration;

                const auto count = [this](parts::ExtensionOutcome outcome) {
                    this->count_extension(outcome);
                };
                const bool spent = interleave_.forward_next(forward_, backward_)
                                       ? step(
                                             forward_,
                                             forward_ctx_,
                                             [this](const Label<ResourceType>& label) {
                                                 this->record_solution(label);
                                             },
                                             count)
                                       : step(
                                             backward_,
                                             backward_ctx_,
                                             [this](const Label<ResourceType>& label) {
                                                 this->record_backward_solution(label);
                                             },
                                             count);
                if (spent) {
                    return;
                }
            }
        }

        /// @brief Collects the complete paths, from three places.
        ///
        ///  1. Forward labels at a sink: paths the forward search finished on its own.
        ///  2. Backward labels at a source: paths the backward search finished on its own, turned
        ///     into forward order.
        ///  3. The join: a forward label that crossed `H` on its last arc, paired with a backward
        ///     label at the same node.
        ///
        /// 1 and 2 are needed because the join only sees paths that cross `H`. They run first, so
        /// the join prunes against the best path found so far. A path found twice is kept once.
        /// The join is skipped after an interrupt.
        void record_remaining_solutions() override {
            for (const auto* label : forward_.labels_at_end_nodes(*this->graph_)) {
                this->record_solution(*label);
            }
            for (const auto* label : backward_.labels_at_end_nodes(*this->graph_)) {
                this->record_backward_solution(*label);
            }
            run_join();
        }

        /// @brief The forward labels at the sinks.
        [[nodiscard]] std::list<Label<ResourceType>*> labels_at_end_nodes() const override {
            return forward_.labels_at_end_nodes(*this->graph_);
        }

        /// @brief The arc ids of the path a forward label holds, from a source to its node.
        std::vector<size_t> get_path_arc_ids(const Label<ResourceType>& label) override {
            return prefix_arc_ids(label);
        }

        void print_labels() const override {
            forward_.print(*this->graph_);
            backward_.print(*this->graph_);
        }

        /// @brief Puts the bidirectional diagnostics on the result the caller receives.
        void annotate(SolveResult* result) const override {
            Base::annotate(result);
            result->bidirectional = BidirectionalStats{
                .half_way_used = half_way_.on(),
                .half_way_off_reason = half_way_off_reason_,
                .joined_paths = joined_paths_,
            };
        }

        /// @brief Relieves each side's memory pressure, the forward side's first: what the trims
        ///        cut is abandoned and counted.
        void on_memory_pressure() override {
            this->relieve_memory_pressure(forward_, forward_ctx_);
            this->relieve_memory_pressure(backward_, backward_ctx_);
        }

        /// @brief Releases the pool, then forgets both sides' containers and schedules, so that
        ///        no pointer to a released label remains.
        void release_label_memory() override {
            Base::release_label_memory();
            forward_.clear();
            backward_.clear();
        }

    private:
        /// @brief Which run-level stop, if any, ended the search with labels still to extend.
        ///
        /// The schedules are tested first, so an exhausted search is never reported as stopped
        /// and `is_time_out()` (which sets `timed_out_`) is not called. An interrupt is reported
        /// ahead of a timeout, so a run that is both is treated as interrupted.
        ///
        /// @return The stop, or @c RunLevelStop::None.
        [[nodiscard]] RunLevelStop run_level_stop() {
            if (waiting_labels() == 0) {
                return RunLevelStop::None;
            }
            if (this->is_interrupted()) {
                return RunLevelStop::Interrupted;
            }
            if (this->is_time_out()) {
                return RunLevelStop::Timeout;
            }
            if (this->memory_limit_.effective_limit > 0 && this->memory_limit_.is_exceeded()) {
                return RunLevelStop::MemoryLimit;
            }
            return RunLevelStop::None;
        }

        /// @brief Runs the join, feeding every accepted pair to `record_path`.
        ///
        /// After a timeout or the memory limit the join still runs (see @ref join_runs_after),
        /// with the incumbent cutoff forced on. It is skipped after an interrupt, or after any
        /// run-level stop when `join_after_early_stop` is false.
        void run_join() {
            const RunLevelStop stop = run_level_stop();
            const bool stopped_early = stop != RunLevelStop::None;
            if (stopped_early && (!this->params_.join_after_early_stop || !join_runs_after(stop))) {
                LOG_DEBUG(
                    "BidirectionalLabellingAlgorithm: the search stopped early, so the join is "
                    "skipped.\n");
                return;
            }

            // Counts what the join adds: record_path drops a path it already holds.
            auto record = [this](double cost, std::vector<size_t> arc_ids, size_t end_node_id) {
                const size_t before = this->solutions_.size();
                this->record_path(cost, std::move(arc_ids), end_node_id);
                joined_paths_ += this->solutions_.size() - before;
            };
            const JoinOptions<ResourceType> options{
                .half_way = half_way_,
                // After an early stop, prune against the incumbent whatever the caller asked: the
                // join only owes the improving paths, and that keeps its output to a handful.
                .prune_by_incumbent = this->params_.prune_by_incumbent || stopped_early,
                .upper_bound = this->upper_bound_,
                .max_solutions = this->params_.max_solutions,
            };
            join(*this->graph_,
                 forward_.labels.containers(),
                 backward_.labels.containers(),
                 options,
                 this->incumbent_,
                 record);
        }

        forward_search_type forward_;
        backward_search_type backward_;
        InterleaveType interleave_;
        SearchContext<ResourceType> forward_ctx_;
        SearchContext<ResourceType> backward_ctx_;

        /// @brief Labels a quota or memory pressure took off a schedule this solve, never to be
        ///        extended; see @ref has_abandoned_labels.
        size_t abandoned_labels_ = 0;

        HalfWay<ResourceType> half_way_;
        std::string half_way_off_reason_;
        size_t joined_paths_ = 0;
};

}  // namespace rcspp::detail
