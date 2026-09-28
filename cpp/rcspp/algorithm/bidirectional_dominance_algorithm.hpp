// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "rcspp/algorithm/bidirectional_search.hpp"
#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/directional_dominance_algorithm.hpp"
#include "rcspp/algorithm/half_way_policy.hpp"
#include "rcspp/algorithm/label_frontier.hpp"
#include "rcspp/algorithm/label_join.hpp"
#include "rcspp/preprocessor/bellman_ford_algorithm.hpp"
#include "rcspp/validation/threshold_probe.hpp"

namespace rcspp::detail {

/// @brief A stop that ends a bidirectional search while labels are still waiting to be extended.
///
/// `stop_after_X_solutions` and `max_iterations` are not run-level stops: after either, the join
/// has always run.
enum class RunLevelStop { None, Timeout, Interrupted, MemoryLimit };

/// @brief Whether the join still runs after @p stop.
///
/// A timeout or the memory limit means "stop searching", and the paths both halves already hold
/// are real columns that a pricing loop needs; the join hands them back, with the incumbent cutoff
/// on, so its output stays small. An interrupt means "stop now", so it is honoured.
///
/// @param stop Why the search stopped.
/// @return @c true unless the search was interrupted.
[[nodiscard]] constexpr bool join_runs_after(RunLevelStop stop) {
    return stop != RunLevelStop::Interrupted;
}

/// @brief Bidirectional labeling: a forward search, a backward search, and a join.
///
/// The class is the forward search (a @ref DirectionalDominanceAlgorithm bound to
/// @ref ForwardDirection) plus a backward container set and frontier, so both searches share one
/// label pool and one upper bound. Extension, dominance and release use the base's
/// direction-templated helpers.
///
/// Internal: a caller asks for it with `direction = SearchDirection::Bidirectional` on
/// @c SimpleDominanceAlgorithm, which takes labels in arrival order, or on
/// @c PushingDominanceAlgorithm, which sweeps the nodes (@ref BidirectionalPushing), through
/// @c ResourceGraph::create_algorithm or @c ResourceGraph::solve, and the model checks it needs
/// (@c BackwardExtensionCheck and @c JoinCheck) run before the solve.
///
/// @tparam ResourceType       The resource type carried by labels.
/// @tparam LabelContainerType The forward per-node container.
/// @tparam CriticalRC         The critical resource's type (the clock).
/// @tparam CostRC             The cost resource's type, used by the completion bounds; its index
///                            is @c params_.heuristic_cost_index. Defaults to @c RealResource, as
///                            @c ResourceGraph::solve does, whatever the clock's type.
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>,
          typename CriticalRC = RealResource, typename CostRC = RealResource>
    requires ResourceTypeConcept<ResourceType>
class BidirectionalDominanceAlgorithm
    : public DirectionalDominanceAlgorithm<ResourceType, LabelContainerType, ForwardDirection>,
      public BidirectionalSearch<ResourceType> {
        using Base =
            DirectionalDominanceAlgorithm<ResourceType, LabelContainerType, ForwardDirection>;

        /// @brief The backward container, fixed because `LabelBuckets` is forward-only.
        using BackwardContainer = LabelList<ResourceType, BackwardDirection>;

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

        /// @brief Whether the half-way bound was in force for the last solve.
        ///
        /// A disabled bound costs speed, not optimality, so it does not set
        /// `could_be_non_optimal()`.
        [[nodiscard]] bool bounded_by_half_way() const { return half_way_.enabled(); }

        /// @brief Why the half-way bound was off for the last solve; empty when it was in force.
        [[nodiscard]] const std::string& half_way_off_reason() const {
            return half_way_off_reason_;
        }

        /// @brief How many distinct complete paths only the join pass produced during the last
        ///        solve.
        ///
        /// A path the join produced at several nodes, or that a search also reached on its own,
        /// counts once or not at all.
        ///
        /// @return The number of paths the join added, reset at the start of each solve.
        [[nodiscard]] size_t number_of_joined_paths() const { return joined_paths_; }

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
            if (this->params_.dynamic_half_way && half_way_controller_.seeded()) {
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
        /// Seeded from `half_way_point` by the first solve that runs with the flag; unseeded
        /// (and inert) before that, and always when `half_way_point` is 0. `h()` is the value
        /// the next solve will use.
        [[nodiscard]] const HalfWayController& half_way_controller() const override {
            return half_way_controller_;
        }

        /// @brief Mutable access, to freeze the controller, reset what it learned, or tune it.
        ///
        /// RouteOpt adapts during the root node's column generation and freezes for the whole
        /// branch-and-bound tree: `half_way_controller().set_frozen(true)` once the root has
        /// converged is the same move.
        ///
        /// The first solve seeds a default-tuned controller only if none is seeded yet, so
        /// assigning one beforehand is how to change the knobs:
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
            // Pushing's frontiers sweep node by node, so a per-node quota keeps each node's
            // cheapest labels; Simple's keep arrival order, and a quota counts what a node extends.
            if (order_ == FrontierOrder::Sweep) {
                forward_frontier_.reset_sweep(num_nodes, /*descending=*/false);
                backward_frontier_.reset_sweep(num_nodes, /*descending=*/true);
            } else {
                forward_frontier_.reset_fifo();
                backward_frontier_.reset_fifo();
            }

            backward_labels_by_node_pos_.clear();
            backward_labels_by_node_pos_.reserve(num_nodes);
            for (size_t i = 0; i < num_nodes; ++i) {
                backward_labels_by_node_pos_.emplace_back(BackwardContainer{});
            }

            // The model checks ran before the solve; a bad half-way bound only disables it.
            joined_paths_ = 0;
            join_pairs_tested_ = 0;
            join_truncated_ = false;

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

            seed_direction<ForwardDirection>(this->non_dominated_labels_by_node_pos_,
                                             &forward_frontier_);
            seed_direction<BackwardDirection>(backward_labels_by_node_pos_, &backward_frontier_);
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
        /// Frontier size is no measure: the backward frontier holds a single seed until the
        /// backward search starts, so taking the larger frontier ran the whole forward half first.
        void main_loop() override {  // NOLINT(readability-function-cognitive-complexity)
            size_t iteration = 0;
            while (number_of_labels() > 0 && !this->should_stop(iteration)) {
                if (iteration > 0 && this->memory_limit_.effective_limit > 0 &&
                    iteration % this->params_.memory_check_interval == 0) {
                    if (this->memory_limit_.is_exceeded()) {
                        LOG_WARN("Memory limit exceeded. Stopping early.\n");
                        break;
                    }
                    if (this->memory_limit_.is_under_pressure()) {
                        this->on_memory_pressure();
                    }
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

        /// @brief Collects terminal labels from BOTH directions, then runs the join pass.
        ///
        /// A path whose clock never crosses `H` is found only by a search reaching a terminal, not
        /// by the join. Duplicates across the sources are absorbed by `Solution`'s hash.
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
            result->bounded_by_half_way = half_way_.enabled();
            result->half_way_off_reason = half_way_off_reason_;
            result->number_of_joined_paths = joined_paths_;
            result->join_pairs_tested = join_pairs_tested_;
            result->join_truncated = join_truncated_;
            // 0 when the bound is off, so a caller reading this without also reading
            // `bounded_by_half_way` gets the value that means "no bound" rather than a number
            // that was never applied.
            result->half_way_point_used = half_way_.enabled() ? half_way_.h() : 0.0;

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
        /// @brief Creates the seed labels for one direction and puts them on its frontier.
        template <typename Dir, typename Container>
        void seed_direction(std::vector<Container>& containers,
                            LabelFrontier<ResourceType>* frontier) {
            for (auto seed_node_id : Dir::seeds(*this->graph_)) {
                auto* seed_node = this->graph_->get_node(seed_node_id);
                auto& label = this->label_pool_.get_next_label(seed_node);
                Dir::seed(*this->graph_, seed_node, label);
                auto label_it = containers.at(seed_node->pos()).add_label(&label);
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
            // bound off the clock, and so its index, is never read.
            if (half_way_.enabled()) {
                const double critical = critical_value(label_ptr->get_resource());
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
            // The join keeps at most the tighter of the two budgets. `stop_after_X_solutions`
            // also stops the search; `join_column_budget` does not.
            const size_t budget =
                std::min(this->params_.stop_after_X_solutions, this->params_.join_column_budget);
            const JoinStats stats = joiner_.join(
                *this->graph_,
                this->non_dominated_labels_by_node_pos_,
                backward_labels_by_node_pos_,
                half_way_,
                this->params_.critical_resource_index,
                this->best_cost_upper_bound_,
                // After an early stop, prune against the incumbent whatever the caller asked:
                // the join only owes the improving paths, and that keeps its output to a handful.
                this->params_.prune_based_on_upper_bound_ || stopped_early,
                this->cost_upper_bound_,
                record,
                budget < MAX_INT ? budget : std::numeric_limits<size_t>::max(),
                this->params_.max_join_pairs < MAX_INT ? this->params_.max_join_pairs
                                                       : std::numeric_limits<size_t>::max());
            join_pairs_tested_ = stats.pairs_tested;
            join_truncated_ = stats.truncated;
        }

        /// @brief The critical resource's scalar value, or 0 when the type is absent.
        [[nodiscard]] double critical_value(const Resource<ResourceType>& resource) const {
            if constexpr (is_cost_in_composition_v<CriticalRC, ResourceType>) {
                const auto& component = resource.template get_component<CriticalRC>(
                    this->params_.critical_resource_index);
                return static_cast<double>(component.get_value().get_value());
            } else {
                return 0.0;
            }
        }

        /// @brief The remaining cost a half still has to pay before it is a complete path.
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
            half_way_ = HalfWayPolicy(half_way_point_for_this_solve(), resource_upper_bound(graph));
            half_way_off_reason_.clear();

            if constexpr (!is_cost_in_composition_v<CriticalRC, ResourceType>) {
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
                const std::vector<double> probes{0.0, half_way_.h()};
                if (!critical_is_monotone(graph, probes)) {
                    turn_half_way_off(HalfWayOff::NotMonotone, clock + " is not monotone");
                    return;
                }
                if (!critical_dominance_is_increasing(graph)) {
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
        /// Logs at WARN the first time @p reason occurs in this process, at DEBUG after that.
        ///
        /// @param reason Why, as a category.
        /// @param why    Why, as a sentence; also kept for @ref half_way_off_reason.
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

        /// @brief How many components of type @p T the model has, read off any arc's extender.
        ///
        /// @param graph The graph whose arcs carry the extenders.
        /// @return The count, or 0 when @p T is not in the model or no arc carries an extender.
        template <typename T>
        [[nodiscard]] static size_t components_of(const Graph<ResourceType>& graph) {
            if constexpr (is_cost_in_composition_v<T, ResourceType>) {
                const auto* arc = graph.first_arc();
                return arc == nullptr || arc->extender == nullptr
                           ? 0
                           : arc->extender->template get_components<T>().size();
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
            // One arc suffices: all extenders share a factory, hence a layout (not enforced).
            const auto* arc = graph.first_arc();
            if (arc == nullptr || arc->extender == nullptr) {
                return BackwardKind::Unspecified;
            }
            return arc->extender
                ->template get_component<CriticalRC>(this->params_.critical_resource_index)
                .backward_kind();
        }

        /// @brief A sink at which the clock's extension sets no backward start, if any.
        ///
        /// A backward label there starts at the type default, below `H`, and would stop at once.
        /// The start is read off the arc the seed reads it from (@c BackwardDirection::seed); a
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
                const auto in_arcs = graph.get_in_arcs(sink);
                const auto entering = std::ranges::find_if(in_arcs, [](const auto* arc) {
                    return arc->extender != nullptr;
                });
                if (entering == in_arcs.end()) {
                    continue;
                }
                auto scratch = sink->resource->template get_component<CriticalRC>(index);
                if (!threshold_probe::observe_start(
                        (*entering)->extender->template get_component<CriticalRC>(index),
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

        /// @brief Dispatches the monotonicity probe, which needs the graph's resource pack.
        template <typename... Ts>
        [[nodiscard]] static bool monotone_impl(const Graph<ResourceTypeComposition<Ts...>>& graph,
                                                size_t index, std::span<const double> probes) {
            return critical_resource_is_monotone<CriticalRC, Ts...>(graph, index, probes);
        }

        [[nodiscard]] bool critical_is_monotone(const Graph<ResourceType>& graph,
                                                const std::vector<double>& probes) const {
            return monotone_impl(graph, this->params_.critical_resource_index, probes);
        }

        /// @brief Dispatches the dominance probe, which needs the graph's resource pack.
        template <typename... Ts>
        [[nodiscard]] static bool dominance_increasing_impl(
            const Graph<ResourceTypeComposition<Ts...>>& graph, size_t index) {
            return critical_resource_dominance_is_increasing<CriticalRC, Ts...>(graph, index);
        }

        /// @brief Whether the clock's dominance order is the increasing one.
        [[nodiscard]] bool critical_dominance_is_increasing(
            const Graph<ResourceType>& graph) const {
            return dominance_increasing_impl(graph, this->params_.critical_resource_index);
        }

        /// @brief `R`, derived as `2H` when `H` is given and infinity otherwise.
        ///
        /// Hence `half_way_point = 0` always means "bound off" here.
        [[nodiscard]] double resource_upper_bound(const Graph<ResourceType>& /*graph*/) const {
            return this->params_.half_way_point > 0.0 ? this->params_.half_way_point * 2.0
                                                      : std::numeric_limits<double>::infinity();
        }

        /// @brief The `H` this solve starts from: the controller's when it adapts, else the
        ///        caller's.
        ///
        /// Seeds the controller on the first solve that runs with `dynamic_half_way`, from the
        /// params that solve sees, so a caller who never sets the flag never pays for one. With
        /// `half_way_point = 0` there is nothing to adapt: the bound is off, and
        /// @ref configure_half_way reports it once per process (`HalfWayOff::NoHalfWayPoint`).
        ///
        /// `R` (see @ref resource_upper_bound) stays `2 * half_way_point` however far the
        /// controller moves `H`; the controller clamps `H` inside that same range.
        [[nodiscard]] double half_way_point_for_this_solve() {
            const double requested = this->params_.half_way_point;
            if (!this->params_.dynamic_half_way || !(requested > 0.0)) {
                return requested;
            }
            if (!half_way_controller_.seeded()) {
                half_way_controller_ = HalfWayController(requested);
            }
            return half_way_controller_.h();
        }

        /// @brief Computes both completion bounds (cost-to-sink and cost-from-source).
        ///
        /// With no @p CostRC component in the pack the bounds stay at zero. Unlike A*, this does
        /// not fall back to `arc.cost`, which could over-estimate and prune the optimum.
        void compute_completion_bounds(const Graph<ResourceType>& graph) {
            const size_t num_nodes = graph.get_number_of_nodes();
            h_to_sink_.assign(num_nodes, 0.0);
            h_from_source_.assign(num_nodes, 0.0);

            // Only the completion-bound prune reads these; skip the relaxations when it is off.
            // The vectors stay sized above because `completion_bound` uses `.at()`.
            if (!this->params_.prune_based_on_upper_bound_) {
                return;
            }

            if constexpr (is_numerical_resource_v<CostRC> &&
                          is_cost_in_composition_v<CostRC, ResourceType>) {
                if constexpr (std::is_same_v<CostRC, CriticalRC>) {
                    if (this->params_.heuristic_cost_index ==
                        this->params_.critical_resource_index) {
                        // The cost slot is the clock: a bound on it would be time, not cost.
                        LOG_WARN(
                            "BidirectionalDominanceAlgorithm: heuristic_cost_index names the "
                            "critical resource, so the completion bound is off. Give the clock "
                            "its own type (ClockRC, the last template parameter of "
                            "ResourceGraph::solve and create_algorithm), e.g. IntResource beside a "
                            "RealResource cost.\n");
                        std::ranges::fill(h_to_sink_, -std::numeric_limits<double>::infinity());
                        std::ranges::fill(h_from_source_, -std::numeric_limits<double>::infinity());
                        return;
                    }
                }
                fill_bound(graph, graph.get_sink_node_ids(), /*forward=*/false, &h_to_sink_);
                fill_bound(graph, graph.get_source_node_ids(), /*forward=*/true, &h_from_source_);
            }
        }

        /// @brief Runs one Bellman-Ford pass over the LABELING cost slot.
        ///
        /// Relaxes on the cost component, not `arc.cost`: `arc.cost` keeps the original weight,
        /// which is not an admissible bound under reduced costs.
        void fill_bound(const Graph<ResourceType>& graph, const std::vector<size_t>& targets,
                        bool forward, std::vector<double>* out) {
            if (this->params_.heuristic_cost_index >= components_of<CostRC>(graph)) {
                // No such cost slot, so no bound: prune nothing, as for a negative cycle.
                LOG_WARN("BidirectionalDominanceAlgorithm: heuristic_cost_index ",
                         this->params_.heuristic_cost_index,
                         " names no component of the cost's type, so the completion bound is "
                         "off.\n");
                std::ranges::fill(*out, -std::numeric_limits<double>::infinity());
                return;
            }
            try {
                auto distance =
                    BellmanFordAlgorithm::solve<CostRC>(graph,
                                                        targets,
                                                        this->params_.heuristic_cost_index,
                                                        forward);
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
        LabelFrontier<ResourceType>* pending_frontier_ = nullptr;

        std::vector<size_t> forward_extended_per_node_;
        std::vector<size_t> backward_extended_per_node_;

        /// @brief Arcs each search has extended this solve; @ref main_loop takes the smaller.
        size_t forward_work_ = 0;
        size_t backward_work_ = 0;

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
/// @tparam CostRC             The cost resource's type.
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>,
          typename CriticalRC = RealResource, typename CostRC = RealResource>
    requires ResourceTypeConcept<ResourceType>
class BidirectionalPushing
    : public BidirectionalDominanceAlgorithm<ResourceType, LabelContainerType, CriticalRC, CostRC> {
    public:
        BidirectionalPushing(ResourceFactory<ResourceType>* resource_factory,
                             AlgorithmParams<LabelContainerType> params)
            : BidirectionalDominanceAlgorithm<ResourceType, LabelContainerType, CriticalRC, CostRC>(
                  resource_factory, std::move(params), FrontierOrder::Sweep) {}
};

}  // namespace rcspp::detail
