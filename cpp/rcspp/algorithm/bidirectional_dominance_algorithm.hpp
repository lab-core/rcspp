// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <functional>
#include <limits>
#include <list>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/directional_dominance_algorithm.hpp"
#include "rcspp/algorithm/half_way_policy.hpp"
#include "rcspp/algorithm/label_join.hpp"
#include "rcspp/preprocessor/bellman_ford_algorithm.hpp"

namespace rcspp {

/// @brief Reduces a bidirectional `SolveResult` to what @ref HalfWayController reads.
///
/// `exact` is `COMPLETE` and untrimmed by memory pressure, and not @p truncated. The status alone
/// cannot say the last part: a per-node extension quota truncates a bidirectional search while it
/// still reports `COMPLETE`, so a caller who set `num_labels_to_extend_by_node` says so here.
/// `join_truncated` counts as inexact too: the zero-join guard reads `joined_paths`, which a
/// capped join understates.
///
/// @param result    A bidirectional solve's result.
/// @param truncated Whether the caller capped the search in a way the status does not show.
/// @return The observation.
[[nodiscard]] inline HalfWayObservation half_way_observation(const SolveResult& result,
                                                             bool truncated = false) {
    return HalfWayObservation{
        .forward_labels = result.forward_labels,
        .backward_labels = result.backward_labels,
        .joined_paths = result.number_of_joined_paths,
        .solutions = result.solutions.size(),
        .bounded = result.bounded_by_half_way,
        .exact = result.status == AlgorithmStatus::COMPLETE && !result.memory_pressure_triggered &&
                 !result.join_truncated && !truncated,
    };
}

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
    : public DirectionalDominanceAlgorithm<ResourceType, LabelContainerType, ForwardDirection> {
        using Base =
            DirectionalDominanceAlgorithm<ResourceType, LabelContainerType, ForwardDirection>;

        /// @brief The backward container, fixed because `LabelBuckets` is forward-only.
        using BackwardContainer = LabelList<ResourceType, BackwardDirection>;

    public:
        BidirectionalDominanceAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                                        AlgorithmParams<LabelContainerType> params)
            : Base(resource_factory, std::move(params)) {}

        ~BidirectionalDominanceAlgorithm() override = default;

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
                const bool truncated = this->params_.num_labels_to_extend_by_node < MAX_INT;
                half_way_controller_.update(half_way_observation(result, truncated));
            }
            return result;
        }

        /// @brief The controller that moves `H` between solves when `dynamic_half_way` is set.
        ///
        /// Seeded from `half_way_point` by the first solve that runs with the flag; unseeded
        /// (and inert) before that, and always when `half_way_point` is 0. `h()` is the value
        /// the next solve will use.
        [[nodiscard]] const HalfWayController& half_way_controller() const {
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
        /// `algo->half_way_controller() = HalfWayController(500.0, my_params);`.
        [[nodiscard]] HalfWayController& half_way_controller() { return half_way_controller_; }

    protected:
        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Base::initialize(graph, cost_upper_bound);

            const size_t num_nodes = graph->get_number_of_nodes();
            forward_extended_per_node_.assign(num_nodes, 0);
            backward_extended_per_node_.assign(num_nodes, 0);
            forward_frontier_.clear();
            backward_frontier_.clear();

            backward_labels_by_node_pos_.clear();
            backward_labels_by_node_pos_.reserve(num_nodes);
            for (size_t i = 0; i < num_nodes; ++i) {
                backward_labels_by_node_pos_.emplace_back(BackwardContainer{});
            }

            // Throw on a model without backward semantics; a bad half-way bound only disables it.
            joined_paths_ = 0;
            join_pairs_tested_ = 0;
            join_truncated_ = false;

            validate_backward_semantics(*graph);

            configure_half_way(*graph);
            compute_completion_bounds(*graph);
        }

        void initialize_labels() override {
            this->label_pool_.release_all_labels();

            this->non_dominated_labels_by_node_pos_.clear();
            this->non_dominated_labels_by_node_pos_.reserve(this->graph_->get_number_of_nodes());
            for (size_t i = 0; i < this->graph_->get_number_of_nodes(); ++i) {
                this->non_dominated_labels_by_node_pos_.emplace_back(this->params_.labels.copy());
            }

            seed_direction<ForwardDirection>(this->non_dominated_labels_by_node_pos_,
                                             &forward_frontier_);
            seed_direction<BackwardDirection>(backward_labels_by_node_pos_, &backward_frontier_);
        }

        [[nodiscard]] size_t number_of_labels() const override {
            return forward_frontier_.size() + backward_frontier_.size();
        }

        /// @brief Alternates the two searches, taking from whichever frontier is larger.
        ///
        /// Balancing by frontier size keeps the halves meeting near the middle without a schedule.
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

                const bool take_forward = !forward_frontier_.empty() &&
                                          (backward_frontier_.empty() ||
                                           forward_frontier_.size() >= backward_frontier_.size());

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
                extract_backward_solution(*label);
            }

            run_join_pass();
        }

        /// @brief Puts the bidirectional diagnostics on the result the caller receives.
        void annotate(SolveResult* result) const override {
            // Also fills forward_labels and dominance_checks, from the forward containers.
            Base::annotate(result);
            result->bounded_by_half_way = half_way_.enabled();
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
        void on_memory_pressure() override {
            Base::on_memory_pressure();
            this->effective_max_labels_per_node_ =
                std::min(this->effective_max_labels_per_node_,
                         this->params_.memory_pressure_max_labels_per_node);
            this->memory_pressure_triggered_ = true;
            trim_frontier(&forward_frontier_);
            trim_frontier(&backward_frontier_);
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
            pending_frontier_->push_back(label_iterator_pair);
        }

    private:
        /// @brief Creates the seed labels for one direction and puts them on its frontier.
        template <typename Dir, typename Container>
        void seed_direction(std::vector<Container>& containers,
                            std::list<LabelIteratorPair<ResourceType>>* frontier) {
            for (auto seed_node_id : Dir::seeds(*this->graph_)) {
                auto* seed_node = this->graph_->get_node(seed_node_id);
                auto& label = this->label_pool_.get_next_label(seed_node);
                Dir::seed(label);
                auto label_it = containers.at(seed_node->pos()).add_label(&label);
                frontier->push_back(std::make_pair(&label, label_it));
            }
        }

        /// @brief Processes one label from @p frontier.
        ///
        /// Abandoning a label (dominated, over quota, beyond the completion bound, past `H`) never
        /// stops the solve; run-level stops are checked in `main_loop`.
        template <typename Dir, typename Container>
        void step(std::vector<Container>& containers,
                  std::list<LabelIteratorPair<ResourceType>>* frontier,
                  std::vector<size_t>* extended_per_node) {
            auto label_iterator_pair = frontier->front();
            frontier->pop_front();

            auto* label_ptr = label_iterator_pair.first;
            if (label_ptr == nullptr) {
                return;
            }
            if (label_ptr->dominated) {
                this->label_pool_.release_with_ref_count(label_ptr);
                return;
            }

            const size_t node_pos = label_ptr->get_end_node()->pos();
            size_t& extended_count = extended_per_node->at(node_pos);
            if (extended_count >= this->effective_max_labels_per_node_) {
                // Known gap: an abandoned label never yields a boundary label, so the join sees
                // fewer forward halves. Only reachable under truncated labeling or memory pressure,
                // and such labels are never revisited.
                return;
            }
            ++extended_count;

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
                    if constexpr (Dir::backward) {
                        extract_backward_solution(*label_ptr);
                    } else {
                        this->extract_solution(*label_ptr);
                    }
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
        template <typename Dir, typename Container>
        void extend_into(Label<ResourceType>* label_ptr, std::vector<Container>& containers,
                         std::list<LabelIteratorPair<ResourceType>>* frontier) {
            pending_frontier_ = frontier;
            for (auto* arc_ptr : Dir::arcs(*this->graph_, label_ptr->get_end_node())) {
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

        /// @brief Records a complete path found by the backward search reaching a source.
        ///
        /// Its chain is already in forward order, so no reversal is needed.
        void extract_backward_solution(const Label<ResourceType>& label) {
            std::vector<size_t> arc_ids;
            const Label<ResourceType>* current = &label;
            const Label<ResourceType>* last = current;
            while (current != nullptr && current->get_out_arc() != nullptr) {
                arc_ids.push_back(current->get_out_arc()->id);
                last = current;
                current = current->prev_label;
            }
            if (arc_ids.empty()) {
                return;
            }
            // Normally the chain ends at the sink's seed; otherwise use the last arc's destination.
            const auto* end_node =
                current != nullptr ? current->get_end_node() : last->get_out_arc()->destination;
            this->extract_solution(label.get_cost(), std::move(arc_ids), end_node->id);
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
                                      clock + " states no ceiling at sink " +
                                          std::to_string(*sink) +
                                          ", so a backward label there would start below H");
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
        /// disables the bound. Use e.g. `TimeWindowExtensionFunction` or `BudgetExtensionFunction`.
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

        /// @brief A sink at which the clock's feasibility function gives no backward seed, if any.
        ///
        /// A backward label there starts at the type default, below `H`, and would stop at once.
        ///
        /// @param graph The graph.
        /// @return The first such sink's id, or @c std::nullopt.
        [[nodiscard]] std::optional<size_t> sink_without_a_ceiling(
            const Graph<ResourceType>& graph) const {
            for (const size_t sink_id : graph.get_sink_node_ids()) {
                const auto* sink = graph.get_node(sink_id);
                if (sink == nullptr || sink->resource == nullptr) {
                    continue;
                }
                if (!sink->resource
                         ->template get_component<CriticalRC>(this->params_.critical_resource_index)
                         .has_back_seed()) {
                    return sink_id;
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
                            "critical resource, so the completion bound is off. Bind the cost's "
                            "type, e.g. BidirectionalAlgoBound<IntResource, RealResource>.\n");
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

        /// @brief Refuses to start on a model that cannot express backward semantics.
        ///
        /// Pairs `backward_kind()` from one arc's extender with `merge_rule()` and the back seed
        /// from one node's resource, by component index.
        ///
        /// @throws std::runtime_error naming every offending component, before any label exists.
        void validate_backward_semantics(const Graph<ResourceType>& graph) const {
            // One arc suffices: all extenders share a factory, hence a layout (not enforced).
            std::vector<BackwardKind> kinds;
            const auto* first = graph.first_arc();
            if (first != nullptr && first->extender != nullptr) {
                first->extender->for_each_component(
                    [&](const auto& component) { kinds.push_back(component.backward_kind()); });
            }

            // No arcs (e.g. preprocessing removed them all): nothing to validate, and the solve
            // should return an empty result rather than throw.
            if (kinds.empty()) {
                return;
            }

            // Any node with a resource answers for all; finding none is a refusal.
            const Node<ResourceType>* node = nullptr;
            for (const size_t node_id : graph.get_node_ids()) {
                const auto* candidate = graph.get_node(node_id);
                if (candidate != nullptr && candidate->resource != nullptr) {
                    node = candidate;
                    break;
                }
            }
            if (node == nullptr) {
                throw std::runtime_error(
                    "BidirectionalDominanceAlgorithm cannot run on this model: no node carries a "
                    "resource, so its backward declarations cannot be checked");
            }

            std::vector<std::string> problems;
            size_t index = 0;
            node->resource->for_each_component([&](const auto& component) {
                const auto kind = index < kinds.size() ? kinds[index] : BackwardKind::Unspecified;
                describe_problem(component, kind, index, &problems);
                ++index;
            });
            check_floors_and_consumptions(graph, *first, kinds, &problems);

            // The composition dominance function must also have a backward form.
            try {
                static_cast<void>(node->resource->back_dominates(*node->resource));
            } catch (const NoBackwardDominance&) {
                problems.emplace_back(
                    "the model's composition dominance function has no backward form; override "
                    "check_back_dominance (CompositionDominanceFunction, the default, has one)");
            }

            // So must the composition extension and feasibility functions: probed once, before
            // any label exists, rather than failing in the backward search or the join. The merge
            // probe runs only once every component has a merge rule, or it would throw for that.
            const bool components_declared = problems.empty();
            try {
                Resource<ResourceType> probe(*node->resource);
                first->extender->extend_back(*node->resource, &probe);
            } catch (const NoBackwardExtension&) {
                problems.emplace_back(
                    "the model's composition extension function has no backward form; override "
                    "extend_back (CompositionExtensionFunction, the default, has one)");
            }
            try {
                static_cast<void>(node->resource->is_back_feasible());
            } catch (const NoBackwardFeasibility&) {
                problems.emplace_back(
                    "the model's composition feasibility function has no backward form; override "
                    "is_back_feasible (CompositionFeasibilityFunction, the default, has one)");
            }
            if (components_declared) {
                try {
                    static_cast<void>(node->resource->can_be_merged(*node->resource));
                } catch (const NoBackwardFeasibility&) {
                    problems.emplace_back(
                        "the model's composition feasibility function has no merge test; override "
                        "can_be_merged (CompositionFeasibilityFunction, the default, has one)");
                }
            }

            // The join, the backward terminal and the backward prune all add the two halves'
            // costs.
            if (!node->resource->cost_adds_across_join()) {
                problems.emplace_back(
                    "the model's cost function does not add across a join: the join adds the two "
                    "halves' costs, which is exact only when every component the cost reads has a "
                    "zero cost (TrivialCostFunction) or a value cost (ValueCostFunction) under an "
                    "accumulating extension; a threshold's backward value is a bound, not a cost");
            }

            if (!problems.empty()) {
                std::ostringstream message;
                message << "BidirectionalDominanceAlgorithm cannot run on this model:";
                for (const auto& problem : problems) {
                    message << "\n  - " << problem;
                }
                throw std::runtime_error(message.str());
            }
        }

        /// @brief Records what, if anything, is wrong with one component's backward declarations.
        ///
        /// Runs the setup-time backward coherence checks (3 to 8 in backward_kind.hpp), which
        /// every bidirectional solve reaches regardless of how the model was built.
        template <typename ComponentResource>
        static void describe_problem(const ComponentResource& component, BackwardKind kind,
                                     size_t index, std::vector<std::string>* problems) {
            using ComponentValue = std::decay_t<decltype(component.get_value())>;
            const auto rule = component.merge_rule();
            const std::string label = "component " + std::to_string(index);

            if (kind == BackwardKind::Unspecified) {
                problems->push_back(label + ": its extension function declares no backward_kind()");
            }
            if (rule == MergeRule::Unspecified) {
                problems->push_back(label + ": its feasibility function declares no merge_rule()");
            }
            if constexpr (!is_numerical_resource_v<ComponentValue>) {
                if (rule == MergeRule::DominanceOrder) {
                    problems->push_back(label +
                                        ": its feasibility function declares "
                                        "MergeRule::DominanceOrder, which compares two scalar "
                                        "values, on a resource that has none; declare "
                                        "MergeRule::Custom with a body of its own");
                }
            }
            // DominanceOrder treats the backward value as a bound; under an accumulation it is a
            // consumption, so the join would accept infeasible splices.
            if (kind == BackwardKind::Accumulate && rule == MergeRule::DominanceOrder) {
                problems->push_back(
                    label +
                    ": its extension accumulates but its feasibility function declares "
                    "MergeRule::DominanceOrder, which compares a prefix value against a suffix "
                    "value rather than against a bound; use a Threshold extension (e.g. "
                    "BudgetExtensionFunction) or declare MergeRule::Custom with a body that adds "
                    "the two halves");
            }
            if (kind == BackwardKind::Accumulate && seeds_itself_out_of_range(component)) {
                problems->push_back(
                    label +
                    ": its extension accumulates but its feasibility function seeds a backward "
                    "label at the far end of its range, so the first backward extension leaves "
                    "that range; use a Threshold extension (e.g. BudgetExtensionFunction) instead");
            }
            // A feasibility function that tests "is this node in my memory" needs an endpoint
            // mirror; with an ArcValue container every backward label would be rejected.
            if (component.requires_endpoint_mirror() && kind != BackwardKind::EndpointMirror) {
                problems->push_back(
                    label +
                    ": its feasibility function forbids each node at itself, which only reads "
                    "correctly backwards when the memory at a node excludes that node; its "
                    "extension function does not declare BackwardKind::EndpointMirror, so going "
                    "backward the memory arrives already holding the node it sits on and every "
                    "backward extension is rejected. Derive the extension function from "
                    "EndpointMirrorForm -- NgPathExtensionFunction is the one this library ships, "
                    "and "
                    "presets::add_elementary_resource wires it up for an elementary path");
            }
        }

        /// @brief Records the floors and consumptions a backward label cannot account for.
        ///
        /// Two checks, per component:
        ///  - a @c Threshold whose feasibility function tests a backward floor above the value
        ///    its extension clamps a forward arrival up to (or above 0 where it does not clamp)
        ///    rejects deadlines a forward path meets;
        ///  - a component whose backward reading assumes the value never decreases, which a
        ///    @c Threshold with no forward clamp does wherever it has a backward floor, cannot
        ///    have a negative consumption on any arc. A backward deadline carries only a ceiling,
        ///    so a dip below the floor inside the suffix would go unseen;
        ///  - a @c Threshold's backward clamp at a node must pass that node's @c is_back_feasible,
        ///    or every backward label there is rejected, and must equal the node's backward seed,
        ///    its upper bound, or the two searches solve different models (an extension and a
        ///    feasibility function built from different bounds).
        ///
        /// Costs one pass over the nodes and one over the arcs, with no clones.
        ///
        /// @param graph    The graph.
        /// @param first    Any arc carrying an extender, to read the forward floors from.
        /// @param kinds    Each component's backward kind.
        /// @param problems Receives one line per offending component.
        static void check_floors_and_consumptions(const Graph<ResourceType>& graph,
                                                  const Arc<ResourceType>& first,
                                                  const std::vector<BackwardKind>& kinds,
                                                  std::vector<std::string>* problems) {
            const size_t count = kinds.size();

            // A Threshold's forward floor and backward ceiling depend on the node only, so one
            // extender answers for every arc.
            std::vector<std::function<std::optional<double>(size_t)>> forward_floor;
            std::vector<std::function<std::optional<double>(size_t)>> backward_ceiling;
            first.extender->for_each_component([&](const auto& component) {
                using Value = std::decay_t<decltype(component.get_value())>;
                forward_floor.emplace_back([&component](size_t node_id) -> std::optional<double> {
                    if constexpr (is_numerical_resource_v<Value>) {
                        if (auto floor = component.floor_at(node_id)) {
                            return static_cast<double>(floor->get_value());
                        }
                    }
                    return std::nullopt;
                });
                backward_ceiling.emplace_back(
                    [&component](size_t node_id) -> std::optional<double> {
                        if constexpr (is_numerical_resource_v<Value>) {
                            if (auto ceiling = component.back_ceiling_at(node_id)) {
                                return static_cast<double>(ceiling->get_value());
                            }
                        }
                        return std::nullopt;
                    });
            });

            std::vector<bool> needs_scan(count, false);
            std::vector<bool> reported(count, false);
            std::vector<bool> ceiling_reported(count, false);
            for (const size_t node_id : graph.get_node_ids()) {
                const auto* node = graph.get_node(node_id);
                if (node == nullptr || node->resource == nullptr) {
                    continue;
                }
                size_t index = 0;
                node->resource->for_each_component([&](const auto& component) {
                    const size_t component_index = index++;
                    if (component_index >= count) {
                        return;
                    }
                    if (component.requires_nondecreasing()) {
                        needs_scan[component_index] = true;
                    }
                    using Value = std::decay_t<decltype(component.get_value())>;
                    if constexpr (is_numerical_resource_v<Value>) {
                        if (kinds[component_index] != BackwardKind::Threshold) {
                            return;
                        }
                        check_ceiling(component,
                                      backward_ceiling[component_index](node_id),
                                      component_index,
                                      node_id,
                                      &ceiling_reported,
                                      problems);
                        if (reported[component_index]) {
                            return;
                        }
                        const auto back = component.back_floor();
                        if (!back) {
                            return;
                        }
                        const auto back_floor = static_cast<double>(back->get_value());
                        const auto floor = forward_floor[component_index](node_id);
                        if (floor ? back_floor > *floor : back_floor > 0.0) {
                            reported[component_index] = true;
                            problems->push_back(
                                "component " + std::to_string(component_index) +
                                ": its feasibility function rejects a backward value below " +
                                std::to_string(back_floor) + " at node " + std::to_string(node_id) +
                                ", which its extension never raises a forward value to, so the "
                                "backward search rejects deadlines a forward path meets; build "
                                "the extension and the feasibility function from the same windows "
                                "(TimeWindowExtensionFunction with TimeWindowFeasibilityFunction), "
                                "or pair BudgetExtensionFunction with "
                                "MinMaxFeasibilityFunction(0, capacity)");
                        } else if (!floor) {
                            needs_scan[component_index] = true;
                        }
                    }
                });
            }

            if (std::ranges::find(needs_scan, true) == needs_scan.end()) {
                return;
            }
            graph.for_each_arc([&](const auto& arc) {
                if (arc.extender == nullptr) {
                    return;
                }
                size_t index = 0;
                arc.extender->for_each_component([&](const auto& component) {
                    const size_t component_index = index++;
                    if (component_index >= count || !needs_scan[component_index]) {
                        return;
                    }
                    using Value = std::decay_t<decltype(component.get_value())>;
                    if constexpr (is_numerical_resource_v<Value>) {
                        const auto consumption =
                            static_cast<double>(component.get_value().get_value());
                        if (consumption < 0.0) {
                            needs_scan[component_index] = false;  // report once
                            problems->push_back(
                                "component " + std::to_string(component_index) + ": arc " +
                                std::to_string(arc.origin->id) + " -> " +
                                std::to_string(arc.destination->id) + " consumes " +
                                std::to_string(consumption) +
                                ", but its backward reading assumes no arc lowers the value: a "
                                "backward label carries only a ceiling, so a path dipping below "
                                "the floor would be accepted; loads must be non-negative");
                        }
                    }
                });
            });
        }

        /// @brief Records a @c Threshold component whose backward clamp at @p node_id disagrees
        ///        with that node's feasibility function, once per component.
        ///
        /// Two disagreements:
        ///  - the node's backward test rejects the clamp, so every backward label there is lost;
        ///  - the clamp differs from the node's backward seed, which for a threshold pairing is the
        ///    node's upper bound (see @c FeasibilityFunction::back_seed_value). Above it, the
        ///    backward search admits deadlines the forward search rejects, which a test on the
        ///    floor alone (a time window's) cannot see; below it, the backward search rejects
        ///    deadlines a forward path meets.
        ///
        /// A clamp below the node's backward floor is left alone: that node cannot be reached in
        /// time whatever the ceiling, which is the model's business, not an incoherence.
        template <typename ComponentResource>
        static void check_ceiling(const ComponentResource& component, std::optional<double> ceiling,
                                  size_t component_index, size_t node_id,
                                  std::vector<bool>* ceiling_reported,
                                  std::vector<std::string>* problems) {
            if (!ceiling || (*ceiling_reported)[component_index]) {
                return;
            }
            using Value = std::decay_t<decltype(component.get_value())>;
            using Scalar = std::decay_t<decltype(std::declval<Value>().get_value())>;
            if (const auto floor = component.back_floor();
                floor && *ceiling < static_cast<double>(floor->get_value())) {
                return;
            }
            Value value;
            value.set_value(static_cast<Scalar>(*ceiling));
            std::string fault;
            if (!component.admits_back_value(value)) {
                fault =
                    ", which its feasibility function rejects there, so every backward label at "
                    "that node is lost";
            } else if (const auto seed = component.back_seed();
                       seed && static_cast<double>(seed->get_value()) != *ceiling) {
                const auto bound = static_cast<double>(seed->get_value());
                fault = ", but its feasibility function bounds the value there at " +
                        std::to_string(bound) + ", so the backward search " +
                        (*ceiling > bound ? "admits deadlines the forward search rejects"
                                          : "rejects deadlines a forward path meets");
            } else {
                return;
            }
            (*ceiling_reported)[component_index] = true;
            problems->push_back(
                "component " + std::to_string(component_index) + ": its backward labels at node " +
                std::to_string(node_id) + " are clamped to " + std::to_string(*ceiling) + fault +
                "; build the extension and the feasibility function from one NodeBounds "
                "(make_node_bounds, or the feasibility function's bounds())");
        }

        /// @brief Whether an accumulating component starts its backward labels somewhere hopeless.
        ///
        /// Catches a capacity seeded at its bound while the extension adds to it, so the first
        /// backward extension leaves the range. Under an accumulation, a seed strictly dominated by
        /// the unseeded state can never become feasible.
        ///
        /// @param component The node's resource component.
        /// @return `true` when the backward seed is strictly dominated by the unseeded state.
        template <typename ComponentResource>
        [[nodiscard]] static bool seeds_itself_out_of_range(const ComponentResource& component) {
            if (!component.has_back_seed()) {
                return false;
            }
            ComponentResource seeded(component);
            seeded.apply_back_seed();
            return component.back_dominates(seeded) && !seeded.back_dominates(component);
        }

        /// @brief Trims a frontier under memory pressure, keeping the cheapest labels.
        ///
        /// Non-dominated dropped entries stay in their containers but are never extended, so the
        /// result may be non-optimal; `memory_pressure_was_triggered()` reports this.
        void trim_frontier(std::list<LabelIteratorPair<ResourceType>>* frontier) {
            const size_t max_total = this->params_.memory_pressure_max_labels_per_node *
                                     this->graph_->get_number_of_nodes();
            if (frontier->size() <= max_total) {
                return;
            }
            frontier->sort([](const auto& lhs, const auto& rhs) {
                return lhs.first->get_cost() < rhs.first->get_cost();
            });
            while (frontier->size() > max_total) {
                auto& back = frontier->back();
                if (back.first->dominated) {
                    this->label_pool_.release_with_ref_count(back.first);
                }
                frontier->pop_back();
            }
        }

        std::vector<BackwardContainer> backward_labels_by_node_pos_;

        std::list<LabelIteratorPair<ResourceType>> forward_frontier_;
        std::list<LabelIteratorPair<ResourceType>> backward_frontier_;
        std::list<LabelIteratorPair<ResourceType>>* pending_frontier_ = nullptr;

        std::vector<size_t> forward_extended_per_node_;
        std::vector<size_t> backward_extended_per_node_;

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
};

/// @brief Binds the critical resource type, leaving a two-parameter template.
///
/// `ResourceGraph::solve` and the Python dispatch accept only `template <typename, typename>`.
///
/// @tparam CriticalRC The critical resource's type -- the clock.
/// @tparam CostRC     The cost resource's type, used by the completion bounds. Defaults to
///                    @c RealResource, not to the clock's type: an @c int clock beside a real
///                    cost is `BidirectionalAlgoBound<IntResource>`.
template <typename CriticalRC, typename CostRC = RealResource>
struct BidirectionalAlgoBound {
        template <typename RT, typename LC>
        class Algo : public BidirectionalDominanceAlgorithm<RT, LC, CriticalRC, CostRC> {
            public:
                using BidirectionalDominanceAlgorithm<RT, LC, CriticalRC,
                                                      CostRC>::BidirectionalDominanceAlgorithm;
        };
};

}  // namespace rcspp
