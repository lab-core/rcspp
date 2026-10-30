// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/dominance_algorithm.hpp"
#include "rcspp/algorithm/half_way_policy.hpp"
#include "rcspp/algorithm/label_frontier.hpp"
#include "rcspp/algorithm/label_join.hpp"
#include "rcspp/algorithm/labelling/bounds.hpp"
#include "rcspp/algorithm/labelling/half_way.hpp"
#include "rcspp/validation/threshold_probe.hpp"

namespace rcspp::detail {

/// @brief A stop that ends a bidirectional search while labels are still waiting to be extended.
///
/// `max_solutions` and `max_iterations` are not run-level stops: the join always runs
/// after either.
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

/// @brief Bidirectional labeling: a forward search, a backward search, and a join.
///
/// The class is the forward search (a @ref DominanceAlgorithm going forward) plus a backward
/// container set and frontier, so both searches share one label pool and one upper bound.
/// Extension, dominance and release use the base's direction-templated helpers.
///
/// Internal: a caller asks for it with `direction = SearchDirection::Bidirectional` on
/// @c SimpleDominanceAlgorithm, through @c ResourceGraph::create_algorithm or
/// @c ResourceGraph::solve, and the model checks it needs (@c BackwardExtensionCheck and
/// @c JoinCheck) run before the solve.
///
/// @tparam ResourceType         The resource type carried by labels.
/// @tparam LabelContainerType   The forward per-node container.
/// @tparam CriticalResourceType The critical resource's type (the clock).
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>,
          typename CriticalResourceType = RealResource>
    requires ResourceTypeConcept<ResourceType>
class BidirectionalDominanceAlgorithm
    : public DominanceAlgorithm<ResourceType, LabelContainerType, ForwardDirection> {
        using Base = DominanceAlgorithm<ResourceType, LabelContainerType, ForwardDirection>;

        /// @brief The backward container: the caller's container, turned backward (see
        ///        @ref make_directed_container).
        using BackwardContainer = directed_container_t<LabelContainerType, BackwardDirection>;

    public:
        BidirectionalDominanceAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                                        AlgorithmParams<LabelContainerType> params)
            : Base(resource_factory, std::move(params)) {}

        ~BidirectionalDominanceAlgorithm() override = default;

        [[nodiscard]] std::vector<SearchDirection> supported_directions() const override {
            return {SearchDirection::Bidirectional};
        }

        /// @brief Read-only access to the backward label sets, for diagnostics and tests.
        [[nodiscard]] const std::vector<BackwardContainer>& get_backward_labels_by_node_pos()
            const {
            return backward_labels_by_node_pos_;
        }

    protected:
        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Base::initialize(graph, cost_upper_bound);

            const size_t num_nodes = graph->get_number_of_nodes();
            forward_extended_per_node_.assign(num_nodes, 0);
            backward_extended_per_node_.assign(num_nodes, 0);
            forward_work_ = 0;
            backward_work_ = 0;
            abandoned_labels_ = 0;
            forward_frontier_.clear();
            backward_frontier_.clear();

            backward_labels_by_node_pos_.clear();
            backward_labels_by_node_pos_.reserve(num_nodes);
            for (size_t i = 0; i < num_nodes; ++i) {
                backward_labels_by_node_pos_.emplace_back(
                    make_directed_container<BackwardDirection>(this->params_.labels));
            }

            joined_paths_ = 0;

            // The model checks ran before the solve. A half-way setting that does not fit the
            // model turns the bound off rather than throwing.
            configure_half_way(*graph);
            compute_completion_bounds(*graph);
        }

        void initialize_labels() override {
            this->label_pool_.release_all_labels();

            this->non_dominated_labels_by_node_pos_.clear();
            this->non_dominated_labels_by_node_pos_.reserve(this->graph_->get_number_of_nodes());
            for (size_t i = 0; i < this->graph_->get_number_of_nodes(); ++i) {
                this->non_dominated_labels_by_node_pos_.emplace_back(
                    make_directed_container<ForwardDirection>(this->params_.labels));
            }

            create_start_labels<ForwardDirection>(this->non_dominated_labels_by_node_pos_,
                                                  &forward_frontier_);
            create_start_labels<BackwardDirection>(backward_labels_by_node_pos_,
                                                   &backward_frontier_);
        }

        [[nodiscard]] size_t waiting_labels() const override {
            return forward_frontier_.size() + backward_frontier_.size();
        }

        /// @brief Whether the quota or memory pressure abandoned a label this solve.
        ///
        /// A bidirectional search runs one phase, so what a cut takes off a frontier is never
        /// extended: the frontiers can empty without every label having been processed.
        [[nodiscard]] bool has_abandoned_labels() const override { return abandoned_labels_ > 0; }

        /// @brief Interleaves the two searches, taking from whichever has done less work so far.
        ///
        /// Work is counted in arcs extended (see @ref extend_into). The two searches share no
        /// labels, so the interleaving changes neither search's labels, only what exists when a
        /// run stops early: a timeout, `max_iterations` or the memory limit then finds both halves
        /// partly built, so the join has pairs to test. With the incumbent-based prune on, the
        /// halves also share the incumbent, so the order can change which labels are pruned.
        ///
        /// Frontier size would be a poor measure: the backward frontier holds a single start
        /// label until the backward search starts, so choosing by frontier size would run the
        /// whole forward half first.
        void main_loop() override {  // NOLINT(readability-function-cognitive-complexity)
            size_t iteration = 0;
            while (waiting_labels() > 0 && !this->should_stop(iteration)) {
                if (this->check_memory(iteration)) {
                    break;
                }
                ++iteration;

                const bool take_forward =
                    !forward_frontier_.empty() &&
                    (backward_frontier_.empty() || forward_work_ <= backward_work_);

                if (take_forward) {
                    step<ForwardDirection>(this->non_dominated_labels_by_node_pos_,
                                           &forward_frontier_,
                                           &forward_extended_per_node_);
                } else if (!backward_frontier_.empty()) {
                    step<BackwardDirection>(backward_labels_by_node_pos_,
                                            &backward_frontier_,
                                            &backward_extended_per_node_);
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
            for (const auto* label : this->template labels_at_end_nodes<ForwardDirection>(
                     this->non_dominated_labels_by_node_pos_)) {
                this->record_solution(*label);
            }

            for (const auto* label : this->template labels_at_end_nodes<BackwardDirection>(
                     backward_labels_by_node_pos_)) {
                this->template extract_path_solution<BackwardDirection>(*label);
            }

            run_join_pass();
        }

        /// @brief Puts the bidirectional diagnostics on the result the caller receives.
        void annotate(SolveResult* result) const override {
            Base::annotate(result);
            result->bidirectional = BidirectionalStats{
                .half_way_used = half_way_.enabled(),
                .half_way_off_reason = half_way_off_reason_,
                .joined_paths = joined_paths_,
            };
        }

        /// @brief Trims both frontiers and tightens the per-node quota, never loosening one the
        ///        caller set tighter.
        ///
        /// Lowering the quota keeps the frontiers from refilling. Dropped entries stay in their
        /// containers but are never extended, so there is nothing to release on a later event.
        /// The result may be non-optimal; `memory_pressure_was_triggered()` reports it.
        void on_memory_pressure() override {
            Base::on_memory_pressure();
            this->quota_ =
                std::min(this->quota_, this->params_.memory_pressure_max_labels_per_node);
            this->memory_pressure_triggered_ = true;
            const size_t per_node = this->params_.memory_pressure_max_labels_per_node;
            const size_t num_nodes = this->graph_->get_number_of_nodes();
            abandoned_labels_ += forward_frontier_.shed(per_node, num_nodes, &this->label_pool_);
            abandoned_labels_ += backward_frontier_.shed(per_node, num_nodes, &this->label_pool_);
        }

        void release_label_memory() override {
            Base::release_label_memory();
            backward_labels_by_node_pos_.clear();
            forward_frontier_.clear();
            backward_frontier_.clear();
        }

        LabelIteratorPair<ResourceType> next_label_iterator() override {
            // Unused: main_loop() drives both frontiers directly.
            return {};
        }

        void add_new_unprocessed_label(
            const LabelIteratorPair<ResourceType>& label_iterator_pair) override {
            // Routed by step<Direction>() into the correct frontier; see extend_into().
            pending_frontier_->push(label_iterator_pair);
        }

    private:
        /// @brief Creates the start labels for one direction and puts them on its frontier.
        template <typename Direction, typename Container>
        void create_start_labels(std::vector<Container>& containers,
                                 LabelFrontier<ResourceType>* frontier) {
            for (auto start_node_id : Direction::start_nodes(*this->graph_)) {
                auto* start_node = this->graph_->get_node(start_node_id);
                auto& label = this->label_pool_.get_next_label(start_node);
                Direction::set_start(*this->graph_, start_node, label);
                auto label_it = containers.at(start_node->pos()).add_label(&label);
                frontier->push(std::make_pair(&label, label_it));
            }
        }

        /// @brief Processes one label from @p frontier: the first that is neither dominated nor
        ///        over its node's quota, which is counted against the quota.
        ///
        /// A dominated label is released and one over the quota abandoned, within the same call:
        /// neither costs the search an iteration. Abandoning a label later (beyond the completion
        /// bound, past `H`) ends the iteration but never stops the solve; run-level stops are
        /// checked in `main_loop`.
        template <typename Direction, typename Container>
        void step(std::vector<Container>& containers, LabelFrontier<ResourceType>* frontier,
                  std::vector<size_t>* extended_per_node) {
            LabelIteratorPair<ResourceType> label_iterator_pair{};
            while (!frontier->empty()) {
                const auto popped = frontier->pop();
                if (popped.first->dominated) {
                    this->label_pool_.release_with_ref_count(popped.first);
                    continue;
                }
                size_t& extended_count = extended_per_node->at(popped.first->get_end_node()->pos());
                if (extended_count >= this->quota_) {
                    // Known gap: an abandoned label never yields a boundary label, so the join
                    // sees fewer forward halves. Only reachable under truncated labeling or memory
                    // pressure, and such labels are never revisited.
                    ++abandoned_labels_;
                    continue;
                }
                ++extended_count;
                label_iterator_pair = popped;
                break;
            }
            auto* label_ptr = label_iterator_pair.first;
            if (label_ptr == nullptr) {
                return;
            }
            const size_t node_pos = label_ptr->get_end_node()->pos();

            // Prune on cost plus completion bound: with reduced costs a half's own cost is not a
            // lower bound on the full path.
            if (this->params_.prune_by_incumbent &&
                label_ptr->get_cost() + completion_bound<Direction>(node_pos) >= this->incumbent_) {
                this->template remove_label<Direction>(label_iterator_pair.second, containers);
                this->label_pool_.release_with_ref_count(label_ptr);
                return;
            }

            if (Direction::is_end_node(label_ptr->get_end_node())) {
                if (label_ptr->get_cost() < this->incumbent_) {
                    this->incumbent_ = label_ptr->get_cost();
                }
                // Record now: a terminal label dominated later is gone by the final sweep, and
                // `max_solutions` needs `solutions_` to grow during the search.
                if (this->params_.return_dominated_solutions) {
                    this->template extract_path_solution<Direction>(*label_ptr);
                }
                return;
            }

            if (std::isinf(label_ptr->get_cost())) {
                this->template remove_label<Direction>(label_iterator_pair.second, containers);
                this->label_pool_.release_with_ref_count(label_ptr);
                return;
            }

            // The half-way bound: forward labels stop above H, backward labels below it. With the
            // bound off, the clock (and its index) is never read.
            if (half_way_.enabled()) {
                const double critical =
                    critical_value<CriticalResourceType>(label_ptr->get_resource(),
                                                         this->params_.critical_resource_index);
                if constexpr (Direction::backward) {
                    if (half_way_.should_stop_backward(critical)) {
                        return;
                    }
                } else {
                    if (half_way_.should_stop_forward(critical)) {
                        return;
                    }
                }
            }

            extend_into<Direction>(label_ptr, containers, frontier);
        }

        /// @brief Extends one label along its direction's arcs into @p containers.
        ///
        /// Adds the arcs to its direction's work count, which @ref main_loop balances.
        template <typename Direction, typename Container>
        void extend_into(Label<ResourceType>* label_ptr, std::vector<Container>& containers,
                         LabelFrontier<ResourceType>* frontier) {
            pending_frontier_ = frontier;
            const auto arcs = Direction::out_arcs(*this->graph_, label_ptr->get_end_node());
            (Direction::backward ? backward_work_ : forward_work_) += arcs.size();
            for (auto* arc_ptr : arcs) {
                this->template extend_label<Direction>(label_ptr, arc_ptr, containers);
            }
            pending_frontier_ = nullptr;
        }

        /// @brief Which run-level stop, if any, ended the search with labels still to extend.
        ///
        /// The frontier is tested first, so an exhausted search is never reported as stopped and
        /// `is_time_out()` (which sets `timed_out_`) is not called. An interrupt is reported ahead
        /// of a timeout, so a run that is both is treated as interrupted.
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

        /// @brief Runs the join pass, feeding every accepted pair to `record_path`.
        ///
        /// After a timeout or the memory limit the join still runs (see @ref join_runs_after), with
        /// the incumbent cutoff forced on. It is skipped after an interrupt, or after any run-level
        /// stop when `join_after_early_stop` is false.
        void run_join_pass() {
            const RunLevelStop stop = run_level_stop();
            const bool stopped_early = stop != RunLevelStop::None;
            if (stopped_early && (!this->params_.join_after_early_stop || !join_runs_after(stop))) {
                LOG_DEBUG(
                    "BidirectionalDominanceAlgorithm: the search stopped early, so the join pass "
                    "is skipped.\n");
                return;
            }

            // Counts what the join adds: record_path drops a path it already holds.
            auto record = [this](double cost, std::vector<size_t> arc_ids, size_t end_node_id) {
                const size_t before = this->solutions_.size();
                this->record_path(cost, std::move(arc_ids), end_node_id);
                joined_paths_ += this->solutions_.size() - before;
            };
            const JoinOptions options{
                .half_way = half_way_,
                .critical_resource_index = this->params_.critical_resource_index,
                // After an early stop, prune against the incumbent whatever the caller asked: the
                // join only owes the improving paths, and that keeps its output to a handful.
                .prune_requested = this->params_.prune_by_incumbent || stopped_early,
                .cost_upper_bound = this->upper_bound_,
                .max_solutions = this->params_.max_solutions,
            };
            joiner_.join(*this->graph_,
                         this->non_dominated_labels_by_node_pos_,
                         backward_labels_by_node_pos_,
                         options,
                         this->incumbent_,
                         record);
        }

        /// @brief A lower bound on the cost a label at @p node_pos still has to add to become a
        ///        complete path: to a sink going forward, from a source going backward.
        ///
        /// 0 when @c prune_by_incumbent is off; minus infinity when no bound could be
        /// computed.
        template <typename Direction>
        [[nodiscard]] double completion_bound(size_t node_pos) const {
            if constexpr (Direction::backward) {
                return h_from_source_.empty() ? 0.0 : h_from_source_.at(node_pos);
            } else {
                return h_to_sink_.empty() ? 0.0 : h_to_sink_.at(node_pos);
            }
        }

        /// @brief Builds the half-way policy and validates the clock, disabling rather than
        ///        throwing.
        void configure_half_way(const Graph<ResourceType>& graph) {
            half_way_ = HalfWayPolicy(this->params_.half_way_point, resource_upper_bound(graph));
            half_way_off_reason_.clear();

            if constexpr (!is_cost_in_composition_v<CriticalResourceType, ResourceType>) {
                turn_half_way_off(HalfWayOff::CriticalTypeAbsent,
                                  "the critical resource type is not in the model");
            } else {
                const std::string clock =
                    "critical resource " + std::to_string(this->params_.critical_resource_index);
                if (!half_way_.enabled()) {
                    turn_half_way_off(HalfWayOff::NoHalfWayPoint,
                                      "no half_way_point was given; set it to about half the "
                                      "critical resource's range");
                    return;
                }
                const size_t clocks = components_of<CriticalResourceType>(graph);
                if (this->params_.critical_resource_index >= clocks) {
                    turn_half_way_off(HalfWayOff::IndexOutOfRange,
                                      clock + " does not exist: the model has " +
                                          std::to_string(clocks) +
                                          " component(s) of the critical resource's type");
                    return;
                }
                if (critical_directional_resource_kind(graph) !=
                    DirectionalResourceKind::Threshold) {
                    turn_half_way_off(HalfWayOff::NotAThreshold,
                                      clock +
                                          " does not extend backwards as a threshold, so its "
                                          "backward values are not on the forward scale");
                    return;
                }
                if (const auto sink = sink_without_a_ceiling(graph)) {
                    turn_half_way_off(HalfWayOff::NoCeiling,
                                      clock + " has no ceiling at sink " + std::to_string(*sink) +
                                          ": its extension sets no backward start there, so a "
                                          "backward label would start at the type default, "
                                          "below H");
                    return;
                }
                const std::vector<double> probes{0.0, half_way_.h()};
                if (!critical_resource_is_monotone<CriticalResourceType>(
                        graph,
                        this->params_.critical_resource_index,
                        probes)) {
                    turn_half_way_off(HalfWayOff::NotMonotone, clock + " is not monotone");
                    return;
                }
                if (!critical_resource_dominance_is_increasing<CriticalResourceType>(
                        graph,
                        this->params_.critical_resource_index)) {
                    turn_half_way_off(HalfWayOff::NotIncreasing,
                                      clock +
                                          " does not take part in the dominance order as an "
                                          "increasing one, so a dominator may sit above H while "
                                          "the label it evicted sat below it");
                }
            }
        }

        /// @brief Turns the half-way bound off for this solve, and says why.
        ///
        /// Logs at WARN the first time @p reason occurs in this process, at DEBUG after that.
        ///
        /// @param reason Why, as a category.
        /// @param why    Why, as a sentence; also kept for
        ///               @c BidirectionalStats::half_way_off_reason.
        void turn_half_way_off(HalfWayOff reason, std::string why) {
            half_way_.disable();
            half_way_off_reason_ = std::move(why);
            if (first_report_of(reason)) {
                LOG_WARN(
                    "BidirectionalDominanceAlgorithm: the half-way bound is off because ",
                    half_way_off_reason_,
                    ". The solve stays correct, but it runs a full forward search, a full "
                    "backward search and a join -- more work than a forward algorithm on the "
                    "same model. (Reported once per process; later solves log it at DEBUG.)\n");
            } else {
                LOG_DEBUG("BidirectionalDominanceAlgorithm: the half-way bound is off because ",
                          half_way_off_reason_,
                          ".\n");
            }
        }

        /// @brief How many components of type @p ComponentType the model has, read off one arc's
        /// extender.
        ///
        /// Uses the arc @c threshold_probe::model_arc picks (a removed arc when no live one is
        /// left), so the count does not depend on what preprocessing removed.
        ///
        /// @param graph The graph whose arcs carry the extenders.
        /// @return The count, or 0 when @p ComponentType is not in the model or no arc carries an
        /// extender.
        template <typename ComponentType>
        [[nodiscard]] static size_t components_of(const Graph<ResourceType>& graph) {
            if constexpr (is_cost_in_composition_v<ComponentType, ResourceType>) {
                const auto* arc = threshold_probe::model_arc(graph);
                return arc == nullptr
                           ? 0
                           : arc->extender->template get_components<ComponentType>().size();
            } else {
                return 0;
            }
        }

        /// @brief The directional resource kind of the critical component, read off any arc's
        /// extender.
        ///
        /// Only a `Threshold` backward value is on the forward scale that `H` uses; any other kind
        /// disables the bound. Use e.g. `TimeWindowExtensionFunction` or
        /// `CapacityExtensionFunction`.
        ///
        /// @param graph The graph whose arcs carry the extenders.
        /// @return The critical component's declared directional resource kind, or `Unspecified`
        ///         when the graph has no arc carrying an extender.
        [[nodiscard]] DirectionalResourceKind critical_directional_resource_kind(
            const Graph<ResourceType>& graph) const {
            // One arc suffices: all extenders come from one factory, so they share one component
            // layout (assumed, not checked). It is the arc the model checks read, live or removed.
            const auto* arc = threshold_probe::model_arc(graph);
            if (arc == nullptr) {
                return DirectionalResourceKind::Unspecified;
            }
            return arc->extender
                ->template get_component<CriticalResourceType>(
                    this->params_.critical_resource_index)
                .directional_resource_kind();
        }

        /// @brief A sink at which the clock's extension sets no backward start, if any.
        ///
        /// A backward label there starts at the type default, below `H`, and would stop at once.
        /// The start is read off the arc @c BackwardDirection::start_arc returns; a
        /// sink no live arc enters starts no backward search.
        ///
        /// @param graph The graph.
        /// @return The first such sink's id, or @c std::nullopt.
        [[nodiscard]] std::optional<size_t> sink_without_a_ceiling(
            const Graph<ResourceType>& graph) const {
            const size_t index = this->params_.critical_resource_index;
            for (const size_t sink_id : graph.get_sink_node_ids()) {
                const auto* sink = graph.get_node(sink_id);
                if (sink == nullptr || sink->resource == nullptr) {
                    continue;
                }
                const auto* entering = BackwardDirection::start_arc(graph, sink);
                if (entering == nullptr) {
                    continue;
                }
                auto scratch = sink->resource->template get_component<CriticalResourceType>(index);
                if (!threshold_probe::observe_start(
                        entering->extender->template get_component<CriticalResourceType>(index),
                        &scratch)) {
                    return sink_id;
                }
            }
            return std::nullopt;
        }

        /// @brief The clock's upper end `R` given to @ref HalfWayPolicy: twice `half_way_point`
        ///        when that is set, infinity otherwise.
        ///
        /// With an infinite `R` the policy derives no `H`, so `half_way_point = 0` always turns
        /// the bound off.
        [[nodiscard]] double resource_upper_bound(const Graph<ResourceType>& /*graph*/) const {
            return this->params_.half_way_point > 0.0 ? this->params_.half_way_point * 2.0
                                                      : std::numeric_limits<double>::infinity();
        }

        /// @brief Computes both completion bounds (cost-to-sink and cost-from-source), with
        ///        @ref detail::completion_bounds.
        void compute_completion_bounds(const Graph<ResourceType>& graph) {
            const size_t num_nodes = graph.get_number_of_nodes();
            h_to_sink_.assign(num_nodes, 0.0);
            h_from_source_.assign(num_nodes, 0.0);

            // Only the completion-bound prune reads these; skip the relaxations when it is off.
            // The vectors stay sized above because `completion_bound` uses `.at()`.
            if (!this->params_.prune_by_incumbent) {
                return;
            }

            h_to_sink_ = detail::completion_bounds<ForwardDirection>(graph).by_position;
            h_from_source_ = detail::completion_bounds<BackwardDirection>(graph).by_position;
        }

        std::vector<BackwardContainer> backward_labels_by_node_pos_;

        LabelFrontier<ResourceType> forward_frontier_;
        LabelFrontier<ResourceType> backward_frontier_;
        /// @brief The frontier @ref add_new_unprocessed_label pushes to while @ref extend_into
        ///        runs; null otherwise.
        LabelFrontier<ResourceType>* pending_frontier_ = nullptr;

        std::vector<size_t> forward_extended_per_node_;
        std::vector<size_t> backward_extended_per_node_;

        /// @brief Arcs each search has extended this solve; @ref main_loop takes the smaller.
        size_t forward_work_ = 0;
        size_t backward_work_ = 0;

        /// @brief Labels a quota or memory pressure took off a frontier this solve, never to be
        ///        extended; see @ref has_abandoned_labels.
        size_t abandoned_labels_ = 0;

        /// @brief Lower bounds, by node position, on the cost from each node to a sink (forward)
        ///        and from a source to each node (backward); see @ref completion_bound.
        std::vector<double> h_to_sink_;
        std::vector<double> h_from_source_;

        HalfWayPolicy half_way_{0.0, std::numeric_limits<double>::infinity()};
        std::string half_way_off_reason_;

        size_t joined_paths_ = 0;
        Joiner<ResourceType, CriticalResourceType> joiner_;
};

}  // namespace rcspp::detail
