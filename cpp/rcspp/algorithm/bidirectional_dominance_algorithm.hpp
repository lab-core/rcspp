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

#include "rcspp/algorithm/bidirectional_search.hpp"
#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/dominance_algorithm.hpp"
#include "rcspp/algorithm/half_way_policy.hpp"
#include "rcspp/algorithm/label_frontier.hpp"
#include "rcspp/algorithm/label_join.hpp"
#include "rcspp/preprocessor/bellman_ford_algorithm.hpp"
#include "rcspp/validation/threshold_probe.hpp"

namespace rcspp::detail {

/// @brief A stop that ends a bidirectional search while labels are still waiting to be extended.
///
/// `stop_after_X_solutions` and `max_iterations` are not run-level stops: the join always runs
/// after either.
enum class RunLevelStop { None, Timeout, Interrupted, MemoryLimit };

/// @brief Whether the join still runs after @p stop.
///
/// A timeout or the memory limit means "stop searching", but the paths both halves already hold
/// are valid columns that a pricing loop needs. So the join still runs, keeping only paths
/// cheaper than the best one found so far, which keeps its output small. An interrupt means
/// "stop now", so the join is skipped.
///
/// Why the forced cutoff does not honour a looser caller bound instead: the incumbent starts at the
/// caller's bound (`best_cost_upper_bound_ = cost_upper_bound`) and tightens only once a path is
/// found. When a stopped search found nothing improving -- the solve that stalls a pricing loop --
/// the forced cutoff *is* the caller's bound, so the two rules run the same join. What ends that
/// stall is re-solving with a larger budget (the VRP example's `PricingBudget`), not a different
/// cutoff.
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
/// @c PushingDominanceAlgorithm asks for it the same way, as @ref BidirectionalPushing, whose
/// searches sweep the nodes; Simple's take their labels in arrival order.
///
/// @tparam ResourceType       The resource type carried by labels.
/// @tparam LabelContainerType The forward per-node container.
/// @tparam CriticalRC         The critical resource's type (the clock).
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>,
          typename CriticalRC = RealResource>
    requires ResourceTypeConcept<ResourceType>
class BidirectionalDominanceAlgorithm
    : public DominanceAlgorithm<ResourceType, LabelContainerType, ForwardDirection>,
      public BidirectionalSearch<ResourceType> {
        using Base = DominanceAlgorithm<ResourceType, LabelContainerType, ForwardDirection>;

        /// @brief The backward container: the caller's container, turned backward (see
        ///        @ref rebind_direction).
        using BackwardContainer = rebind_direction_t<LabelContainerType, BackwardDirection>;

    public:
        /// @param resource_factory The graph's resource factory.
        /// @param params           The algorithm's params.
        /// @param order            The order both searches take their labels in.
        BidirectionalDominanceAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                                        AlgorithmParams<LabelContainerType> params,
                                        FrontierOrder order = FrontierOrder::Arrival)
            : Base(resource_factory, std::move(params)), order_(order) {}

        ~BidirectionalDominanceAlgorithm() override = default;

        [[nodiscard]] std::vector<SearchDirection> supported_directions() const override {
            return {SearchDirection::Bidirectional};
        }

        /// @brief The order both searches take their labels in.
        [[nodiscard]] FrontierOrder frontier_order() const { return order_; }

        /// @brief Read-only access to the backward label sets, for diagnostics and tests.
        [[nodiscard]] const std::vector<BackwardContainer>& get_backward_labels_by_node_pos()
            const {
            return backward_labels_by_node_pos_;
        }

        /// @brief Solves, then -- with `dynamic_half_way` set -- lets the controller learn from
        ///        the result.
        ///
        /// The update runs **after** the solve has finished, never during it: `H` is fixed for
        /// the whole of one search (see @ref HalfWayPolicy), and what moves is the value the
        /// *next* solve on this object will start from. It reads the returned `SolveResult`
        /// rather than the containers, so it works under the default `release_after_solve`.
        ///
        /// Only a caller that keeps this object alive between solves benefits --
        /// `ResourceGraph::create_algorithm` plus `solve(algorithm*, ...)`, or the pre-built
        /// algorithm vector `VRP::solve` accepts. The one-shot `graph.solve<Algo>(params)` path
        /// builds a fresh algorithm per call, so the learned `H` is discarded with it.
        SolveResult solve(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            SolveResult result = Base::solve(graph, cost_upper_bound);
            if (this->params_.dynamic_half_way && half_way_controller_.has_initial_h()) {
                // A per-node extension quota is truncation the status cannot show: this
                // algorithm's phase loop always runs once and reports COMPLETE. See step().
                // So is a relaxed dominance: its label counts are not the exact search's.
                const bool truncated = this->params_.num_labels_to_extend_by_node < MAX_INT ||
                                       !this->params_.dominance_ignored_components.empty();
                half_way_controller_.update(half_way_observation(result, truncated));
            }
            return result;
        }

        /// @brief The controller that moves `H` between solves when `dynamic_half_way` is set.
        ///
        /// Started from `half_way_point` by the first solve that runs with the flag; without an
        /// initial `H` (and inert) before that, and always when `half_way_point` is 0. `h()` is
        /// the value the next solve will use.
        [[nodiscard]] const HalfWayController& half_way_controller() const override {
            return half_way_controller_;
        }

        /// @brief Mutable access, to freeze the controller, reset what it learned, or tune it.
        ///
        /// RouteOpt adapts during the root node's column generation and freezes for the whole
        /// branch-and-bound tree: `half_way_controller().set_frozen(true)` once the root has
        /// converged is the same move.
        ///
        /// The first solve starts a default-tuned controller only if none has an initial `H` yet,
        /// so assigning one beforehand is how to change the knobs:
        /// `as_bidirectional(algo.get())->half_way_controller() = HalfWayController(500.0, p);`.
        [[nodiscard]] HalfWayController& half_way_controller() override {
            return half_way_controller_;
        }

    protected:
        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Base::initialize(graph, cost_upper_bound);

            const size_t num_nodes = graph->get_number_of_nodes();
            forward_extended_per_node_.assign(num_nodes, 0);
            backward_extended_per_node_.assign(num_nodes, 0);
            forward_work_ = 0;
            backward_work_ = 0;
            forward_frontier_.clear();
            backward_frontier_.clear();
            // Pushing's frontiers sweep node by node, so a per-node quota keeps each node's
            // cheapest labels; Simple's keep arrival order, and a quota counts what a node extends.
            if (order_ == FrontierOrder::Sweep) {
                forward_frontier_.reset_sweep(num_nodes, /*descending=*/false);
                backward_frontier_.reset_sweep(num_nodes, /*descending=*/true);
            }

            backward_labels_by_node_pos_.clear();
            backward_labels_by_node_pos_.reserve(num_nodes);
            for (size_t i = 0; i < num_nodes; ++i) {
                backward_labels_by_node_pos_.emplace_back(
                    rebind_direction<LabelContainerType, BackwardDirection>::convert(
                        this->params_.labels));
            }

            joined_paths_ = 0;
            join_pairs_tested_ = 0;
            join_truncated_ = false;

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
                    rebind_direction<LabelContainerType, ForwardDirection>::convert(
                        this->params_.labels));
            }

            const auto mask = this->dominance_mask();
            Base::apply_dominance_mask(this->non_dominated_labels_by_node_pos_, mask);
            Base::apply_dominance_mask(backward_labels_by_node_pos_, mask);

            create_start_labels<ForwardDirection>(this->non_dominated_labels_by_node_pos_,
                                                  &forward_frontier_);
            create_start_labels<BackwardDirection>(backward_labels_by_node_pos_,
                                                   &backward_frontier_);
        }

        [[nodiscard]] size_t number_of_labels() const override {
            return forward_frontier_.size() + backward_frontier_.size();
        }

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
            while (number_of_labels() > 0 && !this->should_stop(iteration)) {
                if (this->memory_limit_reached(iteration)) {
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
        void extract_remaining_solutions() override {
            for (const auto* label : this->template get_labels_at_terminals<ForwardDirection>(
                     this->non_dominated_labels_by_node_pos_)) {
                this->extract_solution(*label);
            }

            for (const auto* label : this->template get_labels_at_terminals<BackwardDirection>(
                     backward_labels_by_node_pos_)) {
                this->template extract_path_solution<BackwardDirection>(*label);
            }

            run_join_pass();
        }

        /// @brief Puts the bidirectional diagnostics on the result the caller receives.
        void annotate(SolveResult* result) const override {
            // Also fills forward_labels and dominance_checks, from the forward containers.
            Base::annotate(result);
            result->bidirectional = BidirectionalStats{
                .bounded_by_half_way = half_way_.enabled(),
                .half_way_off_reason = half_way_off_reason_,
                // 0 when the bound is off, so a caller reading this without also reading
                // `bounded_by_half_way` gets the value that means "no bound" rather than a number
                // that was never applied.
                .half_way_point_used = half_way_.enabled() ? half_way_.h() : 0.0,
                .number_of_joined_paths = joined_paths_,
                .join_pairs_tested = join_pairs_tested_,
                .join_truncated = join_truncated_,
            };

            // The backward half. `Base::annotate` cannot see these containers; they belong to
            // this class.
            size_t backward_labels = 0;
            size_t backward_checks = 0;
            Base::accumulate_container_stats(backward_labels_by_node_pos_,
                                             &backward_labels,
                                             &backward_checks);
            result->backward_labels += backward_labels;
            result->dominance_checks += backward_checks;
        }

        /// @brief Trims both frontiers and tightens the per-node quota, never loosening one the
        ///        caller set tighter.
        ///
        /// Lowering the quota keeps the frontiers from refilling. Dropped entries stay in their
        /// containers but are never extended, so there is nothing to release on a later event.
        /// The result may be non-optimal; `memory_pressure_was_triggered()` reports it.
        void on_memory_pressure() override {
            Base::on_memory_pressure();
            this->effective_max_labels_per_node_ =
                std::min(this->effective_max_labels_per_node_,
                         this->params_.memory_pressure_max_labels_per_node);
            this->memory_pressure_triggered_ = true;
            const size_t per_node = this->params_.memory_pressure_max_labels_per_node;
            const size_t num_nodes = this->graph_->get_number_of_nodes();
            forward_frontier_.shed(per_node, num_nodes, &this->label_pool_);
            backward_frontier_.shed(per_node, num_nodes, &this->label_pool_);
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
            // Routed by step<Dir>() into the correct frontier; see extend_into().
            pending_frontier_->push(label_iterator_pair);
        }

    private:
        /// @brief Creates the start labels for one direction and puts them on its frontier.
        template <typename Dir, typename Container>
        void create_start_labels(std::vector<Container>& containers,
                                 LabelFrontier<ResourceType>* frontier) {
            for (auto start_node_id : Dir::start_nodes(*this->graph_)) {
                auto* start_node = this->graph_->get_node(start_node_id);
                auto& label = this->label_pool_.get_next_label(start_node);
                Dir::apply_start(*this->graph_, start_node, label);
                auto label_it = containers.at(start_node->pos()).add_label(&label);
                frontier->push(std::make_pair(&label, label_it));
            }
        }

        /// @brief Processes one label from @p frontier.
        ///
        /// Abandoning a label (dominated, over quota, beyond the completion bound, past `H`) never
        /// stops the solve; run-level stops are checked in `main_loop`.
        template <typename Dir, typename Container>
        void step(std::vector<Container>& containers, LabelFrontier<ResourceType>* frontier,
                  std::vector<size_t>* extended_per_node) {
            auto label_iterator_pair =
                frontier->pop(this->effective_max_labels_per_node_, &this->label_pool_);

            auto* label_ptr = label_iterator_pair.first;
            if (label_ptr == nullptr) {
                return;
            }
            if (label_ptr->dominated) {
                this->label_pool_.release_with_ref_count(label_ptr);
                return;
            }

            const size_t node_pos = label_ptr->get_end_node()->pos();
            // A sweeping frontier applies the quota itself, to each node's queue as it visits it.
            // In FIFO order the quota counts the labels extended at a node so far. Either way an
            // abandoned label never yields a boundary label, so the join sees fewer forward halves
            // (a known gap, only reachable under truncated labeling or memory pressure).
            if (!frontier->sweeping()) {
                size_t& extended_count = extended_per_node->at(node_pos);
                if (extended_count >= this->effective_max_labels_per_node_) {
                    return;
                }
                ++extended_count;
            }

            // Prune on cost plus completion bound: with reduced costs a half's own cost is not a
            // lower bound on the full path.
            if (this->params_.prune_based_on_upper_bound_ &&
                label_ptr->get_cost() + completion_bound<Dir>(node_pos) >=
                    this->best_cost_upper_bound_) {
                this->template remove_label<Dir>(label_iterator_pair.second, containers);
                this->label_pool_.release_with_ref_count(label_ptr);
                return;
            }

            if (Dir::is_terminal(label_ptr->get_end_node())) {
                if (label_ptr->get_cost() < this->best_cost_upper_bound_) {
                    this->best_cost_upper_bound_ = label_ptr->get_cost();
                }
                // Record now: a terminal label dominated later is gone by the final sweep, and
                // `stop_after_X_solutions` needs `solutions_` to grow during the search.
                if (this->params_.return_dominated_solutions) {
                    this->template extract_path_solution<Dir>(*label_ptr);
                }
                return;
            }

            if (std::isinf(label_ptr->get_cost())) {
                this->template remove_label<Dir>(label_iterator_pair.second, containers);
                this->label_pool_.release_with_ref_count(label_ptr);
                return;
            }

            // The half-way bound: forward labels stop above H, backward labels below it. With the
            // bound off, the clock (and its index) is never read.
            if (half_way_.enabled()) {
                const double critical =
                    critical_value<CriticalRC>(label_ptr->get_resource(),
                                               this->params_.critical_resource_index);
                if constexpr (Dir::backward) {
                    if (half_way_.should_stop_backward(critical)) {
                        return;
                    }
                } else {
                    if (half_way_.should_stop_forward(critical)) {
                        return;
                    }
                }
            }

            extend_into<Dir>(label_ptr, containers, frontier);
        }

        /// @brief Extends one label along its direction's arcs into @p containers.
        ///
        /// Adds the arcs to its direction's work count, which @ref main_loop balances.
        template <typename Dir, typename Container>
        void extend_into(Label<ResourceType>* label_ptr, std::vector<Container>& containers,
                         LabelFrontier<ResourceType>* frontier) {
            pending_frontier_ = frontier;
            const auto arcs = Dir::arcs(*this->graph_, label_ptr->get_end_node());
            (Dir::backward ? backward_work_ : forward_work_) += arcs.size();
            for (auto* arc_ptr : arcs) {
                this->template extend_label<Dir>(label_ptr, arc_ptr, containers);
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
            if (number_of_labels() == 0) {
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

        /// @brief Runs the join pass, feeding every accepted pair to `extract_solution`.
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

            // Counts what the join adds: extract_solution drops a path it already holds.
            auto record = [this](double cost, std::vector<size_t> arc_ids, size_t end_node_id) {
                const size_t before = this->solutions_.size();
                this->extract_solution(cost, std::move(arc_ids), end_node_id);
                joined_paths_ += this->solutions_.size() - before;
            };
            const JoinOptions options{
                .half_way = half_way_,
                .critical_resource_index = this->params_.critical_resource_index,
                // After an early stop, prune against the incumbent whatever the caller asked: the
                // join only owes the improving paths, and that keeps its output to a handful.
                .prune_requested = this->params_.prune_based_on_upper_bound_ || stopped_early,
                .cost_upper_bound = this->cost_upper_bound_,
                // The join keeps at most the tighter of the two budgets. `stop_after_X_solutions`
                // also stops the search; `join_column_budget` does not.
                .max_solutions = std::min(this->params_.stop_after_X_solutions,
                                          this->params_.join_column_budget),
                .max_pairs = this->params_.max_join_pairs,
            };
            const JoinStats stats = joiner_.join(*this->graph_,
                                                 this->non_dominated_labels_by_node_pos_,
                                                 backward_labels_by_node_pos_,
                                                 options,
                                                 this->best_cost_upper_bound_,
                                                 record);
            join_pairs_tested_ = stats.pairs_tested;
            join_truncated_ = stats.truncated;
        }

        /// @brief A lower bound on the cost a label at @p node_pos still has to add to become a
        ///        complete path: to a sink going forward, from a source going backward.
        ///
        /// 0 when @c prune_based_on_upper_bound_ is off; minus infinity when no bound could be
        /// computed.
        template <typename Dir>
        [[nodiscard]] double completion_bound(size_t node_pos) const {
            if constexpr (Dir::backward) {
                return h_from_source_.empty() ? 0.0 : h_from_source_.at(node_pos);
            } else {
                return h_to_sink_.empty() ? 0.0 : h_to_sink_.at(node_pos);
            }
        }

        /// @brief Builds the half-way policy and validates the clock, disabling rather than
        ///        throwing.
        ///
        /// With `dynamic_half_way` set, `H` comes from the controller rather than straight from
        /// `half_way_point`; everything after that -- the validation, and disabling on failure --
        /// is identical, because a learned `H` has to pass the same checks a static one does.
        void configure_half_way(const Graph<ResourceType>& graph) {
            const double range = resource_upper_bound(graph);
            half_way_ = HalfWayPolicy(half_way_point_for_this_solve(range), range);
            half_way_off_reason_.clear();

            if constexpr (!is_cost_in_composition_v<CriticalRC, ResourceType>) {
                turn_half_way_off(HalfWayOff::CriticalTypeAbsent,
                                  "the critical resource type is not in the model");
            } else {
                const std::string clock =
                    "critical resource " + std::to_string(this->params_.critical_resource_index);
                // Deriving H: a derivation that failed says why below, not "no H given".
                const bool deriving =
                    this->params_.derive_half_way_point && !(this->params_.half_way_point > 0.0);
                if (!half_way_.enabled() && !deriving) {
                    turn_half_way_off(HalfWayOff::NoHalfWayPoint,
                                      "no half_way_point was given; set it to about half the "
                                      "critical resource's range");
                    return;
                }
                if (threshold_probe::model_arc(graph) == nullptr) {
                    turn_half_way_off(HalfWayOff::NoArcs,
                                      "the graph has no arcs, so there is nothing to bound");
                    return;
                }
                const size_t clocks = components_of<CriticalRC>(graph);
                if (this->params_.critical_resource_index >= clocks) {
                    turn_half_way_off(HalfWayOff::IndexOutOfRange,
                                      clock + " does not exist: the model has " +
                                          std::to_string(clocks) +
                                          " component(s) of the critical resource's type");
                    return;
                }
                if (critical_backward_kind(graph) != BackwardKind::Threshold) {
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
                if (!half_way_.enabled()) {
                    turn_half_way_off(HalfWayOff::NoHalfWayPoint,
                                      "derive_half_way_point is set, but " + clock +
                                          " states no positive ceiling at the sinks to derive H "
                                          "from");
                    return;
                }
                const std::vector<double> probes{0.0, half_way_.h()};
                if (!critical_resource_is_monotone<CriticalRC>(
                        graph,
                        this->params_.critical_resource_index,
                        probes)) {
                    turn_half_way_off(HalfWayOff::NotMonotone, clock + " is not monotone");
                    return;
                }
                if (!critical_resource_dominance_is_increasing<CriticalRC>(
                        graph,
                        this->params_.critical_resource_index)) {
                    turn_half_way_off(HalfWayOff::NotIncreasing,
                                      clock +
                                          " does not take part in the dominance order as an "
                                          "increasing one, so a dominator may sit above H while "
                                          "the label it evicted sat below it");
                    return;
                }
                if (const auto component = critical_component_index(graph);
                    component && this->params_.dominance_ignored_components.contains(*component)) {
                    turn_half_way_off(HalfWayOff::ClockRelaxed,
                                      clock + " (component " + std::to_string(*component) +
                                          ") is left out of dominance by "
                                          "dominance_ignored_components, so a dominator may sit "
                                          "above H while the label it evicted sat below it");
                }
            }
        }

        /// @brief Turns the half-way bound off for this solve, and says why.
        ///
        /// Logs at WARN the first time @p reason occurs in this process, at DEBUG after that;
        /// `NoArcs` only ever at DEBUG, since an empty graph is not a configuration problem.
        ///
        /// @param reason Why, as a category.
        /// @param why    Why, as a sentence; also kept for
        ///               @c BidirectionalStats::half_way_off_reason.
        void turn_half_way_off(HalfWayOff reason, std::string why) {
            half_way_.disable();
            half_way_off_reason_ = std::move(why);
            if (reason != HalfWayOff::NoArcs && first_report_of(reason)) {
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

        /// @brief How many components of type @p T the model has, read off one arc's extender.
        ///
        /// Uses the arc @c threshold_probe::model_arc picks (a removed arc when no live one is
        /// left), so the count does not depend on what preprocessing removed.
        ///
        /// @param graph The graph whose arcs carry the extenders.
        /// @return The count, or 0 when @p T is not in the model or no arc carries an extender.
        template <typename T>
        [[nodiscard]] static size_t components_of(const Graph<ResourceType>& graph) {
            if constexpr (is_cost_in_composition_v<T, ResourceType>) {
                const auto* arc = threshold_probe::model_arc(graph);
                return arc == nullptr ? 0 : arc->extender->template get_components<T>().size();
            } else {
                return 0;
            }
        }

        /// @brief The backward kind of the critical component, read off any arc's extender.
        ///
        /// Only a `Threshold` backward value is on the forward scale that `H` uses; any other kind
        /// disables the bound. Use e.g. `TimeWindowExtensionFunction` or
        /// `CapacityExtensionFunction`.
        ///
        /// @param graph The graph whose arcs carry the extenders.
        /// @return The critical component's declared backward kind, or `Unspecified` when the
        ///         graph has no arc carrying an extender.
        [[nodiscard]] BackwardKind critical_backward_kind(const Graph<ResourceType>& graph) const {
            // One arc suffices: all extenders come from one factory, so they share one component
            // layout (assumed, not checked). It is the arc the model checks read, live or removed.
            const auto* arc = threshold_probe::model_arc(graph);
            if (arc == nullptr) {
                return BackwardKind::Unspecified;
            }
            return arc->extender
                ->template get_component<CriticalRC>(this->params_.critical_resource_index)
                .backward_kind();
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
                auto scratch = sink->resource->template get_component<CriticalRC>(index);
                if (!threshold_probe::observe_start(
                        entering->extender->template get_component<CriticalRC>(index),
                        &scratch)) {
                    return sink_id;
                }
            }
            return std::nullopt;
        }

        /// @brief The clock's index among all components, in the numbering the model checks
        ///        and @c AlgorithmBaseParams::dominance_ignored_components use.
        ///
        /// Found by address on any node's resource, so it needs no knowledge of the pack's layout.
        ///
        /// @param graph The graph.
        /// @return The index, or @c std::nullopt when no node carries a resource or the clock's
        ///         type is absent.
        [[nodiscard]] std::optional<size_t> critical_component_index(
            const Graph<ResourceType>& graph) const {
            if constexpr (is_cost_in_composition_v<CriticalRC, ResourceType>) {
                for (const size_t node_id : graph.get_node_ids()) {
                    const auto* node = graph.get_node(node_id);
                    if (node == nullptr || node->resource == nullptr) {
                        continue;
                    }
                    const void* clock = &node->resource->template get_component<CriticalRC>(
                        this->params_.critical_resource_index);
                    std::optional<size_t> found;
                    size_t index = 0;
                    node->resource->for_each_component([&](const auto& component) {
                        if (static_cast<const void*>(&component) == clock) {
                            found = index;
                        }
                        ++index;
                    });
                    return found;
                }
            }
            return std::nullopt;
        }

        /// @brief The clock's upper end `R` given to @ref HalfWayPolicy: read off the model
        ///        under `derive_half_way_point`, else twice `half_way_point` when that is set, and
        ///        infinity otherwise.
        ///
        /// With an infinite `R` the policy derives no `H`, so `half_way_point = 0` turns the bound
        /// off unless `derive_half_way_point` is set.
        [[nodiscard]] double resource_upper_bound(const Graph<ResourceType>& graph) const {
            if (this->params_.derive_half_way_point) {
                if (const auto range = read_resource_upper_bound(graph)) {
                    return *range;
                }
            }
            return this->params_.half_way_point > 0.0 ? this->params_.half_way_point * 2.0
                                                      : std::numeric_limits<double>::infinity();
        }

        /// @brief `R` read off the model: the largest backward start of the clock at any sink.
        ///
        /// Under a threshold clock a backward label starts at its sink's upper bound (see
        /// `ThresholdForm::start_back`). The start is read off the arc
        /// @c BackwardDirection::start_arc returns, as @ref sink_without_a_ceiling does; a sink no
        /// live arc enters starts no backward search, and is skipped.
        ///
        /// @param graph The graph.
        /// @return `std::nullopt` when the clock type is absent, the index names no component (a
        ///         graph with no arcs included), a sink sets no start, or the largest is not
        ///         positive and finite.
        [[nodiscard]] std::optional<double> read_resource_upper_bound(
            const Graph<ResourceType>& graph) const {
            if constexpr (!is_cost_in_composition_v<CriticalRC, ResourceType>) {
                return std::nullopt;
            } else {
                const size_t index = this->params_.critical_resource_index;
                // Before touching a sink's components: get_component on a bad index is unsafe.
                if (index >= components_of<CriticalRC>(graph)) {
                    return std::nullopt;
                }
                double largest = -std::numeric_limits<double>::infinity();
                for (const size_t sink_id : graph.get_sink_node_ids()) {
                    const auto* sink = graph.get_node(sink_id);
                    if (sink == nullptr || sink->resource == nullptr) {
                        continue;
                    }
                    const auto* entering = BackwardDirection::start_arc(graph, sink);
                    if (entering == nullptr) {
                        continue;
                    }
                    auto scratch = sink->resource->template get_component<CriticalRC>(index);
                    const auto start = threshold_probe::observe_start(
                        entering->extender->template get_component<CriticalRC>(index),
                        &scratch);
                    if (!start) {
                        return std::nullopt;
                    }
                    largest = std::max(largest, *start);
                }
                if (!std::isfinite(largest) || !(largest > 0.0)) {
                    return std::nullopt;
                }
                return largest;
            }
        }

        /// @brief The `H` this solve starts from: the controller's when it adapts, else the
        ///        caller's.
        ///
        /// Starts the controller on the first solve that runs with `dynamic_half_way`, from the
        /// params that solve sees, so a caller who never sets the flag never pays for one. With
        /// `half_way_point = 0` there is nothing to adapt: the bound is off, and
        /// @ref configure_half_way reports it once per process (`HalfWayOff::NoHalfWayPoint`).
        ///
        /// `R` (see @ref resource_upper_bound) stays `2 * half_way_point` however far the
        /// controller moves `H`, unless `derive_half_way_point` supplied it; the controller clamps
        /// `H` inside that same range. Under `derive_half_way_point` with `half_way_point = 0`, the
        /// controller starts from `R / 2`, the `H` the policy would derive.
        ///
        /// @param range `R` for this solve, as @ref resource_upper_bound computed it.
        [[nodiscard]] double half_way_point_for_this_solve(double range) {
            const bool derived_range =
                this->params_.derive_half_way_point && std::isfinite(range) && range > 0.0;
            double requested = this->params_.half_way_point;
            if (!(requested > 0.0) && derived_range) {
                requested = range / 2.0;
            }
            if (!this->params_.dynamic_half_way || !(requested > 0.0)) {
                // HalfWayPolicy derives H from R itself, if at all.
                return this->params_.half_way_point;
            }
            if (!half_way_controller_.has_initial_h()) {
                half_way_controller_ = derived_range && requested < range
                                           ? HalfWayController::over_range(requested, range)
                                           : HalfWayController(requested);
            }
            return half_way_controller_.h();
        }

        /// @brief Computes both completion bounds (cost-to-sink and cost-from-source).
        ///
        /// The bounds are shortest distances over each arc's share of the label cost, the cost
        /// the search itself compares, so they hold whichever component or components the cost
        /// function reads. That share is well defined because the model checks require an
        /// additive cost before any bidirectional solve. Unlike A*, this reads neither `arc.cost`
        /// (the original weight, not a bound under reduced costs) nor a cost slot index.
        void compute_completion_bounds(const Graph<ResourceType>& graph) {
            const size_t num_nodes = graph.get_number_of_nodes();
            h_to_sink_.assign(num_nodes, 0.0);
            h_from_source_.assign(num_nodes, 0.0);

            // Only the completion-bound prune reads these; skip the relaxations when it is off.
            // The vectors stay sized above because `completion_bound` uses `.at()`.
            if (!this->params_.prune_based_on_upper_bound_) {
                return;
            }

            const auto weights = arc_cost_shares(graph);
            fill_bound(graph, weights, graph.get_sink_node_ids(), /*forward=*/false, &h_to_sink_);
            fill_bound(graph,
                       weights,
                       graph.get_source_node_ids(),
                       /*forward=*/true,
                       &h_from_source_);
        }

        /// @brief Each live arc's share of the label cost: what extending a label along it adds.
        ///
        /// Extends the origin's start value along the arc, as a label is extended, and takes the
        /// difference of the two costs. Under an additive cost the difference does not depend on
        /// the value extended from.
        ///
        /// @param graph The graph.
        /// @return One weighted arc per live arc with an extender.
        [[nodiscard]] static std::vector<BellmanFordAlgorithm::WeightedArc> arc_cost_shares(
            const Graph<ResourceType>& graph) {
            std::vector<BellmanFordAlgorithm::WeightedArc> weights;
            std::unique_ptr<Resource<ResourceType>> start;
            std::unique_ptr<Resource<ResourceType>> extended;
            graph.for_each_arc([&](const Arc<ResourceType>& arc) {
                if (arc.extender == nullptr || arc.origin->resource == nullptr ||
                    arc.destination->resource == nullptr) {
                    return;
                }
                if (start == nullptr) {
                    start = std::make_unique<Resource<ResourceType>>(*arc.origin->resource);
                    extended = std::make_unique<Resource<ResourceType>>(*arc.destination->resource);
                }
                // reset() adopts a node's function objects without cloning them, as a pooled
                // label does.
                start->reset(*arc.origin->resource);
                extended->reset(*arc.destination->resource);
                arc.extender->extend(*start, extended.get());
                weights.push_back({.origin_id = arc.origin->id,
                                   .destination_id = arc.destination->id,
                                   .weight = extended->get_cost() - start->get_cost()});
            });
            return weights;
        }

        /// @brief Fills @p out with the shortest distances to (or from) @p targets over
        ///        @p weights, by node position.
        ///
        /// A negative-cost cycle leaves no finite bound: @p out is then minus infinity, which
        /// prunes nothing.
        void fill_bound(const Graph<ResourceType>& graph,
                        const std::vector<BellmanFordAlgorithm::WeightedArc>& weights,
                        const std::vector<size_t>& targets, bool forward,
                        std::vector<double>* out) {
            try {
                auto distance =
                    BellmanFordAlgorithm::solve_weighted(graph, targets, weights, forward);
                for (size_t node_id : graph.get_node_ids()) {
                    const auto* node = graph.get_node(node_id);
                    auto it = distance.find(node_id);
                    out->at(node->pos()) = (it != distance.end())
                                               ? it->second
                                               : std::numeric_limits<double>::infinity();
                }
            } catch (const std::runtime_error&) {
                // Negative-cost cycle: no finite lower bound, so use -inf, which prunes nothing
                // (0 would prune on the half's own cost).
                std::ranges::fill(*out, -std::numeric_limits<double>::infinity());
            }
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

        /// @brief Lower bounds, by node position, on the cost from each node to a sink (forward)
        ///        and from a source to each node (backward); see @ref completion_bound.
        std::vector<double> h_to_sink_;
        std::vector<double> h_from_source_;

        HalfWayPolicy half_way_{0.0, std::numeric_limits<double>::infinity()};
        std::string half_way_off_reason_;

        /// @brief Survives across solves -- unlike everything above it, which `initialize`
        ///        rebuilds. That persistence is the whole of the dynamic half-way point.
        HalfWayController half_way_controller_;

        size_t joined_paths_ = 0;
        size_t join_pairs_tested_ = 0;
        bool join_truncated_ = false;
        Joiner<ResourceType, CriticalRC> joiner_;

        const FrontierOrder order_;
};

/// @brief The bidirectional search of @c PushingDominanceAlgorithm: both searches sweep the nodes
///        in position order, as Pushing does, so a per-node quota keeps each node's cheapest
///        labels.
///
/// @tparam ResourceType       The resource type carried by labels.
/// @tparam LabelContainerType The forward per-node container.
/// @tparam CriticalRC         The critical resource's type (the clock).
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>,
          typename CriticalRC = RealResource>
    requires ResourceTypeConcept<ResourceType>
class BidirectionalPushing
    : public BidirectionalDominanceAlgorithm<ResourceType, LabelContainerType, CriticalRC> {
    public:
        BidirectionalPushing(ResourceFactory<ResourceType>* resource_factory,
                             AlgorithmParams<LabelContainerType> params)
            : BidirectionalDominanceAlgorithm<ResourceType, LabelContainerType, CriticalRC>(
                  resource_factory, std::move(params), FrontierOrder::Sweep) {}
};

}  // namespace rcspp::detail
