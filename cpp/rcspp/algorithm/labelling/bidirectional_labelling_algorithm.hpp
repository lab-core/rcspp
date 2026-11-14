// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/algorithm_params.hpp"
#include "rcspp/algorithm/container_direction.hpp"
#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/labelling/completion_bound.hpp"
#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/half_way.hpp"
#include "rcspp/algorithm/labelling/half_way_checks.hpp"
#include "rcspp/algorithm/labelling/join.hpp"
#include "rcspp/algorithm/labelling/parts/extension_outcome.hpp"
#include "rcspp/algorithm/labelling/parts/interleave/least_work_first.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/fifo_schedule.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/schedule.hpp"
#include "rcspp/algorithm/labelling/search.hpp"
#include "rcspp/algorithm/labelling/step.hpp"
#include "rcspp/algorithm/solve_result.hpp"
#include "rcspp/label/label_list.hpp"
#include "rcspp/utils/logger.hpp"

namespace rcspp::detail {

/// @brief A stop that ends a bidirectional search while labels are still waiting to be extended.
///
/// `max_solutions` and `max_iterations` are not early stops here: the join always runs after
/// either.
enum class EarlyStop { None, Timeout, Interrupted, MemoryLimit };

/// @brief Whether the join still runs after @p stop, by default (`join_after_early_stop = true`;
///        false skips it after every early stop, @ref BidirectionalLabellingAlgorithm::run_join).
///
/// A timeout or the memory limit means "stop searching", but the paths both halves already hold
/// are valid columns that a pricing loop needs. So the join still runs, keeping only paths
/// cheaper than the best one found so far, which keeps its output small. An interrupt means
/// "stop now", so the join is skipped.
///
/// @param stop Why the search stopped.
/// @return @c true unless the search was interrupted.
[[nodiscard]] constexpr bool join_runs_after(EarlyStop stop) {
    return stop != EarlyStop::Interrupted;
}

/// @brief Bidirectional labelling: a forward search, a backward search, and a join.
///
/// Two searches (@ref Search), one per direction, share one label pool and one incumbent. Each
/// iteration runs the one @ref step on the side @p InterleaveType picks. The sides meet at the
/// half-way point `H` (@ref HalfWay), a value of the critical resource set by `half_way_point`.
/// It is off by default, and when it does not fit the model (@ref half_way_off_reason). A label
/// past `H` (forward: above it; backward: below it) stays stored at its node, unextended. With
/// `prune_by_incumbent`, a side prunes a label on its cost plus its completion bound. What a
/// quota or memory pressure cuts is abandoned (left in its container, never extended, and
/// counted), since the search runs one phase.
///
/// After the main loop, @ref record_remaining_solutions records the paths each search finished
/// on its own, then runs the join (@ref join, join.hpp), which pairs the forward labels past `H`
/// with the backward labels at the same node.
///
/// The model checks it needs (@c BackwardExtensionCheck and @c JoinCheck) run before the solve.
///
/// @tparam ResourceType         The resource type carried by labels.
/// @tparam LabelContainerType   The per-node container of the params; each side keeps it turned
///                              to its direction (@ref make_directed_container).
/// @tparam CriticalResourceType The critical resource's type (the clock).
/// @tparam ForwardPartsType    The forward side's parts (@ref SearchParts).
/// @tparam BackwardPartsType   The backward side's parts.
/// @tparam InterleaveType       Which side goes next (@c parts::Interleave).
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>,
          typename CriticalResourceType = RealResource,
          typename ForwardPartsType = SearchParts<parts::FifoSchedule<ResourceType>>,
          typename BackwardPartsType = ForwardPartsType,
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
            Search<ResourceType, ForwardDirection, forward_container_type, ForwardPartsType>;
        using backward_search_type =
            Search<ResourceType, BackwardDirection, backward_container_type, BackwardPartsType>;

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

            const size_t number_of_nodes = graph->get_number_of_nodes();
            forward_.reset(number_of_nodes,
                           make_directed_container<ForwardDirection>(this->params_.labels));
            backward_.reset(number_of_nodes,
                            make_directed_container<BackwardDirection>(this->params_.labels));
            number_of_abandoned_labels_ = 0;
            number_of_joined_paths_ = 0;

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
            forward_.completion_bound = {};
            backward_.completion_bound = {};
            if (prune ||
                parts::reads_completion_bound_v<typename forward_search_type::schedule_type>) {
                forward_.completion_bound = compute_completion_bound<ForwardDirection>(*graph);
            }
            if (prune ||
                parts::reads_completion_bound_v<typename backward_search_type::schedule_type>) {
                backward_.completion_bound = compute_completion_bound<BackwardDirection>(*graph);
            }

            // Both sides share the label pool and the incumbent. As a bidirectional side, each
            // abandons what a cut takes, prunes on cost plus bound, and stops at the half-way
            // point.
            forward_context_ = this->make_context(forward_,
                                                  Role::BidirectionalSide,
                                                  &number_of_abandoned_labels_,
                                                  &half_way_);
            backward_context_ = this->make_context(backward_,
                                                   Role::BidirectionalSide,
                                                   &number_of_abandoned_labels_,
                                                   &half_way_);
            if (!decltype(forward_.admission)::is_exact(forward_context_.admission) ||
                !decltype(backward_.admission)::is_exact(backward_context_.admission)) {
                this->proof_.record(ProofLoss::RelaxedAdmission);
            }
            forward_.schedule.initialize(forward_context_);
            backward_.schedule.initialize(backward_context_);
        }

        void initialize_labels() override {
            // No-op: Algorithm::initialize already emptied the pool.
            this->label_pool_.release_all_labels();
            forward_.create_start_labels(*this->graph_, &this->label_pool_);
            backward_.create_start_labels(*this->graph_, &this->label_pool_);
        }

        [[nodiscard]] size_t number_of_waiting_labels() const override {
            return forward_.schedule.size() + backward_.schedule.size();
        }

        /// @brief One phase, whatever @c max_phases is: the quota and memory pressure abandon
        ///        labels instead of storing them aside, so a later phase would only restart the
        ///        @c max_iterations count and run the join again.
        [[nodiscard]] size_t max_phases() const override { return 1; }

        /// @brief Whether the quota or memory pressure abandoned a label this solve.
        ///
        /// A bidirectional search runs one phase, so what a cut takes off a schedule is never
        /// extended: the schedules can empty without every label having been processed.
        [[nodiscard]] bool has_abandoned_labels() const override {
            return number_of_abandoned_labels_ > 0;
        }

        /// @brief Interleaves the two searches, one @ref step at a time, on the side
        ///        @p InterleaveType picks.
        ///
        /// The memory is checked first in each iteration; a spent budget (`max_solutions`) ends
        /// the loop.
        void main_loop() override {
            size_t iteration = 0;
            while (number_of_waiting_labels() > 0 && !this->should_end_loop(iteration)) {
                if (this->check_memory(iteration)) {
                    break;
                }
                ++iteration;

                const auto count = [this](parts::ExtensionOutcome outcome) {
                    this->count_extension(outcome);
                };
                // One unit of work on the side the interleave picks. A backward label holds its
                // path from its node to a sink, so it records the path the other way round
                // (record_backward_solution).
                bool spent = false;
                if (interleave_.should_step_forward(forward_, backward_)) {
                    spent = step(
                        forward_,
                        forward_context_,
                        [this](const Label<ResourceType>& label) { this->record_solution(label); },
                        count);
                } else {
                    spent = step(
                        backward_,
                        backward_context_,
                        [this](const Label<ResourceType>& label) {
                            this->record_backward_solution(label);
                        },
                        count);
                }
                if (spent) {
                    return;  // a recorded path ends the run (max_solutions)
                }
            }
        }

        /// @brief Collects the complete paths, from three places.
        ///
        ///  1. Forward labels at a sink: paths the forward search finished on its own.
        ///  2. Backward labels at a source: paths the backward search finished on its own, turned
        ///     into forward order.
        ///  3. The join: a forward label past `H` (any non-start forward label when the point is
        ///     off), paired with a backward label at the same node.
        ///
        /// 1 and 2 are needed because the join only sees paths that cross `H`. They run first so
        /// that `joined_paths` counts only the paths the join alone adds: `record_path` drops a
        /// path it already holds. The join prunes against the incumbent, which a search lowers
        /// when it takes an end-node label (`TakeRule::fixed`), not when 1 and 2 record one; after
        /// an early stop, a label still waiting at an end node has not lowered it. The join is
        /// skipped after an interrupt.
        void record_remaining_solutions() override {
            // 1. The forward labels at a sink.
            for (const auto* label : forward_.labels_at_end_nodes(*this->graph_)) {
                this->record_solution(*label);
            }
            // 2. The backward labels at a source, recorded in forward order.
            for (const auto* label : backward_.labels_at_end_nodes(*this->graph_)) {
                this->record_backward_solution(*label);
            }
            // 3. The join.
            run_join();
        }

        /// @brief The forward labels at the sinks. Only the base `record_remaining_solutions`
        ///        reads this, and this class overrides that to read both sides and the join; the
        ///        override is here because `Algorithm` requires it.
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
                .half_way_used = half_way_.is_on(),
                .half_way_off_reason = half_way_off_reason_,
                .number_of_joined_paths = number_of_joined_paths_,
            };
        }

        /// @brief Relieves each side's memory pressure, the forward side's first: what the trims
        ///        cut is abandoned and counted.
        void on_memory_pressure() override {
            this->relieve_memory_pressure(forward_, forward_context_);
            this->relieve_memory_pressure(backward_, backward_context_);
        }

        /// @brief Releases the pool, then forgets both sides' containers and schedules, so that
        ///        no pointer to a released label remains.
        void release_label_memory() override {
            Base::release_label_memory();
            forward_.clear();
            backward_.clear();
        }

    private:
        /// @brief Which early stop, if any, ended the search with labels still to extend.
        ///
        /// The schedules are tested first, so an exhausted search is never reported as stopped
        /// and `is_time_out()` (which sets `timed_out_`) is not called. An interrupt is reported
        /// ahead of a timeout, so a run that is both is treated as interrupted.
        ///
        /// @return The stop, or @c EarlyStop::None.
        [[nodiscard]] EarlyStop early_stop() {
            if (number_of_waiting_labels() == 0) {
                return EarlyStop::None;  // an exhausted search did not stop early
            }
            if (this->is_interrupted()) {
                return EarlyStop::Interrupted;
            }
            if (this->is_time_out()) {
                return EarlyStop::Timeout;
            }
            // An effective limit of 0: no memory limit was set.
            if (this->memory_limit_.effective_limit > 0 && this->memory_limit_.is_exceeded()) {
                return EarlyStop::MemoryLimit;
            }
            return EarlyStop::None;
        }

        /// @brief Runs the join, feeding every accepted pair to `record_path`.
        ///
        /// After a timeout or the memory limit the join still runs (see @ref join_runs_after),
        /// with the incumbent cutoff forced on. It is skipped after an interrupt, or after any
        /// early stop when `join_after_early_stop` is false.
        void run_join() {
            // Why the search stopped with labels still waiting, if it did. After an interrupt the
            // join is skipped; after a timeout or the memory limit it runs, pruning against the
            // incumbent (below), unless join_after_early_stop is off.
            const EarlyStop stop = early_stop();
            const bool stopped_early = stop != EarlyStop::None;
            if (stopped_early && (!this->params_.join_after_early_stop || !join_runs_after(stop))) {
                LOG_DEBUG(
                    "BidirectionalLabellingAlgorithm: the search stopped early, so the join is "
                    "skipped.\n");
                return;
            }

            // Counts the paths the join adds. Algorithm::record_path drops a path already
            // recorded (same arc ids) and one at or above the upper bound, so the change in the
            // number of solutions counts only the new ones.
            auto record = [this](double cost, std::vector<size_t> arc_ids, size_t end_node_id) {
                const size_t before = this->solutions_.size();
                this->record_path(cost, std::move(arc_ids), end_node_id);
                number_of_joined_paths_ += this->solutions_.size() - before;
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
        SearchContext<ResourceType> forward_context_;
        SearchContext<ResourceType> backward_context_;

        /// @brief Labels a quota or memory pressure took off a schedule this solve, never to be
        ///        extended; see @ref has_abandoned_labels.
        size_t number_of_abandoned_labels_ = 0;

        HalfWay<ResourceType> half_way_;
        std::string half_way_off_reason_;
        size_t number_of_joined_paths_ = 0;
};

}  // namespace rcspp::detail
