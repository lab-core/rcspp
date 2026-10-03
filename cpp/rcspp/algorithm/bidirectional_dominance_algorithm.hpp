// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
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
                 !truncated,
    };
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
                Dir::seed(*this->graph_, seed_node, label);
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

        /// @brief Whether the search was cut short by a *run-level* stop, so the join is not owed.
        ///
        /// Run-level stops are timeout, interrupt and the hard memory limit, not
        /// `stop_after_X_solutions`. The frontier is tested first so an exhausted search keeps its
        /// join and `is_time_out()` (which sets `timed_out_`) is not called.
        ///
        /// @return @c true when labels remain AND a run-level stop reason is in force.
        [[nodiscard]] bool search_stopped_early() {
            if (number_of_labels() == 0) {
                return false;
            }
            return this->is_time_out() || this->is_interrupted() ||
                   (this->memory_limit_.effective_limit > 0 && this->memory_limit_.is_exceeded());
        }

        /// @brief Runs the join pass, feeding every accepted pair to `extract_solution`.
        ///
        /// Skipped when @ref search_stopped_early.
        void run_join_pass() {
            if (search_stopped_early()) {
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
            joiner_.join(*this->graph_,
                         this->non_dominated_labels_by_node_pos_,
                         backward_labels_by_node_pos_,
                         half_way_,
                         this->params_.critical_resource_index,
                         this->best_cost_upper_bound_,
                         this->params_.prune_based_on_upper_bound_,
                         this->cost_upper_bound_,
                         record,
                         this->params_.stop_after_X_solutions < MAX_INT
                             ? this->params_.stop_after_X_solutions
                             : std::numeric_limits<size_t>::max());
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
                if (!observe_start((*entering)->extender->template get_component<CriticalRC>(index),
                                   &scratch)) {
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
        /// Pairs `backward_kind()` from one arc's extender with `join_rule()` and the other
        /// declarations of one node's resource, by component index.
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
            const auto needs_scan = check_threshold_clamps(graph, kinds, &problems);
            scan_negative_loads(graph, needs_scan, &problems);
            check_accumulations(graph, *node->resource, kinds, &problems);

            // The composition dominance function must also have a backward form.
            try {
                static_cast<void>(node->resource->back_dominates(*node->resource));
            } catch (const NoBackwardDominance&) {
                problems.emplace_back(
                    "the model's composition dominance function has no backward form; override "
                    "check_back_dominance (CompositionDominanceFunction, the default, has one)");
            }

            // So must the composition extension and feasibility functions: probed once, before
            // any label exists, rather than failing in the backward search or the join. The join
            // probe runs only once every component has a join rule, or it would throw for that.
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
                Resource<ResourceType> probe(*node->resource);
                first->extender->start_back(&probe);
            } catch (const NoBackwardExtension&) {
                problems.emplace_back(
                    "the model's composition extension function has no backward start; override "
                    "start_back (CompositionExtensionFunction, the default, has one)");
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
                    static_cast<void>(node->resource->can_be_joined(*node->resource));
                } catch (const NoBackwardFeasibility&) {
                    problems.emplace_back(
                        "the model's composition feasibility function has no join test; override "
                        "can_be_joined (CompositionFeasibilityFunction, the default, has one)");
                }
            }

            // The join, the backward terminal and the backward prune all add the two halves'
            // costs.
            if (!node->resource->is_cost_additive()) {
                problems.emplace_back(
                    "the model's cost function is not additive: the join adds the two halves' "
                    "costs, which is exact only when every component the cost reads has a "
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
            const auto rule = component.join_rule();
            const std::string label = "component " + std::to_string(index);

            if (kind == BackwardKind::Unspecified) {
                problems->push_back(label + ": its extension function declares no backward_kind()");
            }
            if (rule == JoinRule::Unspecified) {
                problems->push_back(label + ": its feasibility function declares no join_rule()");
            }
            if constexpr (!is_numerical_resource_v<ComponentValue>) {
                if (rule == JoinRule::ValueOrder) {
                    problems->push_back(label +
                                        ": its feasibility function declares "
                                        "JoinRule::ValueOrder, which compares two scalar "
                                        "values, on a resource that has none; declare "
                                        "JoinRule::Custom with a body of its own");
                }
            }
            // ValueOrder reads the backward value as a bound; under an accumulation it is a
            // consumption, so the join would accept infeasible splices.
            if (kind == BackwardKind::Accumulate && rule == JoinRule::ValueOrder) {
                problems->push_back(
                    label +
                    ": its extension accumulates but its feasibility function declares "
                    "JoinRule::ValueOrder, which compares a prefix value against a suffix "
                    "value rather than against a bound; use a Threshold extension (e.g. "
                    "CapacityExtensionFunction) or declare JoinRule::Custom with a body that adds "
                    "the two halves");
            }
            // A feasibility function that tests "is this node in my memory" needs a memory read
            // from the arc's endpoints; with an ArcValue one every backward label would be
            // rejected.
            if (component.requires_arc_endpoints() && kind != BackwardKind::ArcEndpoints) {
                problems->push_back(
                    label +
                    ": its feasibility function forbids each node at itself, which only reads "
                    "correctly backwards when the memory at a node excludes that node; its "
                    "extension function does not declare BackwardKind::ArcEndpoints, so going "
                    "backward the memory arrives already holding the node it sits on and every "
                    "backward extension is rejected. Derive the extension function from "
                    "ArcEndpointsForm -- NgPathExtensionFunction is the one this library ships, "
                    "and "
                    "presets::add_elementary_resource wires it up for an elementary path");
            }
        }

        /// @brief Records each @c Threshold component whose clamps disagree with its feasibility
        ///        function, and says which components the negative-load scan must check.
        ///
        /// Per node, it observes the clamps (@ref observe_node_clamps) and checks each component
        /// against them (@ref check_component_clamps):
        ///  - the backward clamp, and at a sink the backward start, against the node's backward
        ///    and forward tests (@ref check_ceiling);
        ///  - the lowest value a forward label can hold there against the node's backward test
        ///    (@ref check_floor).
        ///
        /// The clamps and starts are observed, not declared, so the checks see what the search
        /// applies, however the extension is written. The checks share one probe per node and
        /// direction.
        ///
        /// A component needs the negative-load scan when its feasibility function declares
        /// @c requires_nondecreasing(), or when it is a @c Threshold whose backward test has a
        /// floor and whose extension no forward clamp (see @ref check_floor).
        ///
        /// Costs one pass over the nodes, with two probes each and a third at a sink.
        ///
        /// @param graph    The graph.
        /// @param kinds    Each component's backward kind.
        /// @param problems Receives one line per offending component.
        /// @return For each component, whether @ref scan_negative_loads must check its arcs.
        [[nodiscard]] static std::vector<bool> check_threshold_clamps(
            const Graph<ResourceType>& graph, const std::vector<BackwardKind>& kinds,
            std::vector<std::string>* problems) {
            const size_t count = kinds.size();
            const auto removed = index_removed_arcs(graph);
            std::unique_ptr<ClampProbe> probe;
            NodeClamps clamps(count);
            ClampCheckState state(count);
            for (const size_t node_id : graph.get_node_ids()) {
                const auto* node = graph.get_node(node_id);
                if (node == nullptr || node->resource == nullptr) {
                    continue;
                }
                if (probe == nullptr) {
                    probe = std::make_unique<ClampProbe>(*node->resource);
                }
                observe_node_clamps(*node, removed, kinds, probe.get(), &clamps);

                size_t index = 0;
                node->resource->for_each_component([&](const auto& component) {
                    const size_t component_index = index++;
                    if (component_index < count) {
                        check_component_clamps(component,
                                               component_index,
                                               node_id,
                                               kinds[component_index],
                                               clamps,
                                               &state,
                                               problems);
                    }
                });
            }
            return state.needs_scan;
        }

        /// @brief Records a negative consumption on any arc, removed ones included, of a component
        ///        whose backward reading assumes the value never decreases.
        ///
        /// A backward label carries only a ceiling, so a path whose value dips below the floor
        /// inside the suffix would be accepted. Reported once per component.
        ///
        /// Costs one pass over the arcs when some component needs it, reading values only.
        ///
        /// @param graph      The graph.
        /// @param needs_scan For each component, whether to check it; see
        ///                   @ref check_threshold_clamps.
        /// @param problems   Receives one line per offending component.
        static void scan_negative_loads(const Graph<ResourceType>& graph,
                                        std::vector<bool> needs_scan,
                                        std::vector<std::string>* problems) {
            if (std::ranges::find(needs_scan, true) == needs_scan.end()) {
                return;
            }
            for_each_model_arc(graph, [&](const Arc<ResourceType>& arc) {
                if (arc.extender == nullptr) {
                    return;
                }
                size_t index = 0;
                arc.extender->for_each_component([&](const auto& component) {
                    const size_t component_index = index++;
                    if (component_index >= needs_scan.size() || !needs_scan[component_index]) {
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

        /// @brief Calls @p fn on every arc preprocessing removed for this solve.
        ///
        /// Such an arc still belongs to the model, so the setup checks read it like a live one:
        /// a refusal does not depend on what one solve's bounds happened to remove.
        template <typename Fn>
        static void for_each_removed_arc(const Graph<ResourceType>& graph, Fn&& fn) {
            for (const size_t arc_id : graph.get_removed_arc_ids()) {
                if (const auto* arc = graph.get_removed_arc(arc_id); arc != nullptr) {
                    fn(*arc);
                }
            }
        }

        /// @brief Calls @p fn on every arc of the model: the live ones, then the removed ones.
        template <typename Fn>
        static void for_each_model_arc(const Graph<ResourceType>& graph, Fn&& fn) {
            graph.for_each_arc([&](const Arc<ResourceType>& arc) { fn(arc); });
            for_each_removed_arc(graph, fn);
        }

        /// @brief One removed arc leaving and one entering each node that has any, for the nodes
        ///        whose live arcs preprocessing took away.
        struct RemovedArcs {
                std::unordered_map<size_t, const Arc<ResourceType>*> leaving;
                std::unordered_map<size_t, const Arc<ResourceType>*> entering;
        };

        /// @brief Indexes the removed arcs by the node they leave and the node they enter.
        ///
        /// @param graph The graph.
        /// @return The index.
        [[nodiscard]] static RemovedArcs index_removed_arcs(const Graph<ResourceType>& graph) {
            RemovedArcs removed;
            for_each_removed_arc(graph, [&](const Arc<ResourceType>& arc) {
                if (arc.extender != nullptr) {
                    removed.leaving.try_emplace(arc.origin->id, &arc);
                    removed.entering.try_emplace(arc.destination->id, &arc);
                }
            });
            return removed;
        }

        /// @brief Records each accumulating component whose two halves the join adds although its
        ///        extension is not a sum, once per component.
        ///
        /// A component's halves are added where its cost is its value (see @c is_cost_additive), or
        /// where its feasibility function reads the backward value as a running total
        /// (@c requires_nondecreasing, as @c MinMaxFeasibilityFunction does under an accumulation:
        /// its join tests `forward + backward`). That is exact only if a path's value is the sum
        /// of one amount per arc, which @c BackwardKind::Accumulate does not promise: a
        /// bottleneck, the largest load seen, extends backward the same way, but does not add.
        ///
        /// Checked by running the extension, as the clamps are (@ref check_sums), and by checking
        /// that such a component's backward labels start where the forward ones do
        /// (@ref check_accumulation_starts).
        ///
        /// @param graph    The graph.
        /// @param any      Any node's resource, to read the declarations from and copy probes of.
        /// @param kinds    Each component's backward kind.
        /// @param problems Receives one line per offending component.
        static void check_accumulations(const Graph<ResourceType>& graph,
                                        const Resource<ResourceType>& any,
                                        const std::vector<BackwardKind>& kinds,
                                        std::vector<std::string>* problems) {
            std::vector<bool> halves_added(kinds.size(), false);
            size_t index = 0;
            any.for_each_component([&](const auto& component) {
                const size_t component_index = index++;
                using Value = std::decay_t<decltype(component.get_value())>;
                if constexpr (is_numerical_resource_v<Value>) {
                    if (component_index < kinds.size() &&
                        kinds[component_index] == BackwardKind::Accumulate) {
                        halves_added[component_index] = component.cost_form() == CostForm::Value ||
                                                        component.requires_nondecreasing();
                    }
                }
            });
            if (std::ranges::find(halves_added, true) == halves_added.end()) {
                return;
            }
            const auto removed = index_removed_arcs(graph);
            check_sums(graph, any, removed, &halves_added, problems);
            check_accumulation_starts(graph, any, removed, halves_added, problems);
        }

        /// @brief The sum check of @ref check_accumulations: from the type default, two steps
        ///        must give the sum of the two single steps.
        ///
        /// For each arc `a` of the model, and an arc `b` leaving its destination (or `a` again),
        /// `extend(extend(0, a), b)` must be `extend(0, a) + extend(0, b)`, exactly for an
        /// integral value and to a relative 1e-9 for floating point. Each step starts from a
        /// cleared output, as a label's does, since an extension may write nothing. The model's
        /// own arcs are a sample, not a proof: a step that is a sum on these values only passes.
        ///
        /// Costs one pass over the arcs, with three steps each.
        ///
        /// @param graph    The graph.
        /// @param any      Any node's resource.
        /// @param removed  The removed arcs, for a node whose live arcs are all gone.
        /// @param checked  Which components to check; one is cleared once reported.
        /// @param problems Receives one line per offending component.
        static void check_sums(const Graph<ResourceType>& graph, const Resource<ResourceType>& any,
                               const RemovedArcs& removed, std::vector<bool>* checked,
                               std::vector<std::string>* problems) {
            Resource<ResourceType> start(any);
            start.reset(any);
            Resource<ResourceType> first(any);
            Resource<ResourceType> second(any);
            Resource<ResourceType> direct(any);
            const auto step = [&](Resource<ResourceType>* output,
                                  const Resource<ResourceType>& input,
                                  const Arc<ResourceType>& arc) {
                output->reset(any);
                size_t index = 0;
                output->for_each_component(input,
                                           *arc.extender,
                                           [&](auto& out, const auto& in, const auto& extender) {
                                               if ((*checked)[index++]) {
                                                   extender.extend(in, &out);
                                               }
                                           });
            };
            for_each_model_arc(graph, [&](const Arc<ResourceType>& arc) {
                if (arc.extender == nullptr ||
                    std::ranges::find(*checked, true) == checked->end()) {
                    return;
                }
                const auto* next =
                    arc_to_probe(arc.destination->out_arcs, removed.leaving, arc.destination->id);
                const Arc<ResourceType>& then = next != nullptr ? *next : arc;
                step(&first, start, arc);
                step(&second, first, then);
                step(&direct, start, then);
                size_t index = 0;
                second.for_each_component(
                    first,
                    direct,
                    [&](const auto& both, const auto& one, const auto& other) {
                        const size_t component_index = index++;
                        using Value = std::decay_t<decltype(both.get_value())>;
                        if constexpr (is_numerical_resource_v<Value>) {
                            if (!(*checked)[component_index]) {
                                return;
                            }
                            const auto two_steps = both.get_value().get_value();
                            const auto first_step = one.get_value().get_value();
                            const auto second_step = other.get_value().get_value();
                            if (is_sum(two_steps, first_step, second_step)) {
                                return;
                            }
                            (*checked)[component_index] = false;  // report once
                            problems->push_back(
                                "component " + std::to_string(component_index) +
                                ": its extension declares BackwardKind::Accumulate, but along " +
                                std::to_string(arc.origin->id) + " -> " +
                                std::to_string(arc.destination->id) + " -> " +
                                std::to_string(then.destination->id) +
                                " it is not a sum: the two steps give " +
                                std::to_string(two_steps) + ", the single steps " +
                                std::to_string(first_step) + " and " + std::to_string(second_step) +
                                "; its cost, or its feasibility function's join test, adds the two "
                                "halves, which is exact only for a sum: write the step as an "
                                "addition, or give the component a zero cost (TrivialCostFunction) "
                                "and a join test that does not add");
                        }
                    });
            });
        }

        /// @brief Whether @p both is @p one plus @p other: exactly for an integral type, to a
        ///        relative 1e-9 for floating point, where an infinite sum (an arc that costs
        ///        infinity, say) must match exactly.
        ///
        /// @param both  The value two steps give.
        /// @param one   The value the first step gives alone.
        /// @param other The value the second step gives alone.
        /// @return Whether the two steps add up.
        template <typename Scalar>
        [[nodiscard]] static bool is_sum(Scalar both, Scalar one, Scalar other) {
            const Scalar sum = one + other;
            if (both == sum) {
                return true;
            }
            if constexpr (std::is_floating_point_v<Scalar>) {
                constexpr Scalar kRelative = 1e-9;
                return std::abs(both - sum) <=
                       kRelative * std::max({Scalar{1}, std::abs(both), std::abs(sum)});
            } else {
                return false;
            }
        }

        /// @brief The start check of @ref check_accumulations: at each sink, a checked component
        ///        must leave the backward start at the type default, where the forward search
        ///        starts, or the join counts the start on top of the path's own sum.
        ///
        /// Read through an arc entering the sink, as @c BackwardDirection::seed reads it
        /// (@ref observe_start).
        ///
        /// @param graph    The graph.
        /// @param any      Any node's resource.
        /// @param removed  The removed arcs, for a node whose live arcs are all gone.
        /// @param checked  Which components to check.
        /// @param problems Receives one line per offending component.
        static void check_accumulation_starts(const Graph<ResourceType>& graph,
                                              const Resource<ResourceType>& any,
                                              const RemovedArcs& removed,
                                              const std::vector<bool>& checked,
                                              std::vector<std::string>* problems) {
            std::vector<bool> reported(checked.size(), false);
            Resource<ResourceType> scratch(any);
            for (const size_t sink_id : graph.get_sink_node_ids()) {
                const auto* sink = graph.get_node(sink_id);
                if (sink == nullptr) {
                    continue;
                }
                const auto* entering = arc_to_probe(sink->in_arcs, removed.entering, sink_id);
                if (entering == nullptr) {
                    continue;
                }
                size_t index = 0;
                scratch.for_each_component(
                    *entering->extender,
                    [&](auto& component, const auto& extender) {
                        const size_t component_index = index++;
                        if (component_index >= checked.size() || !checked[component_index] ||
                            reported[component_index]) {
                            return;
                        }
                        if (const auto start = observe_start(*extender, &component)) {
                            reported[component_index] = true;
                            problems->push_back(
                                "component " + std::to_string(component_index) +
                                ": its extension declares BackwardKind::Accumulate, but starts "
                                "its backward labels at sink " +
                                std::to_string(sink_id) + " at " + std::to_string(*start) +
                                ", not at the type default the forward search starts from, so "
                                "the join counts the start on top of the path's own sum; leave "
                                "start_back to its default");
                        }
                    });
            }
        }

        /// @brief The clamps observed at one node, per component; reused from node to node.
        struct NodeClamps {
                explicit NodeClamps(size_t count) : backward(count), forward(count), start(count) {}

                /// Whether an arc leaves the node, so that a backward step can arrive there.
                bool has_leaving = false;
                /// Whether an arc enters the node, so that a forward step can arrive there.
                bool has_entering = false;
                /// Whether the node is a sink an arc enters, so that a backward label starts there.
                bool has_start = false;
                /// The backward clamp, or @c std::nullopt where there is none.
                std::vector<std::optional<double>> backward;
                /// The forward clamp, or @c std::nullopt where there is none.
                std::vector<std::optional<double>> forward;
                /// The backward start, or @c std::nullopt where it is the type default.
                std::vector<std::optional<double>> start;
        };

        /// @brief Scratch resources for @ref observe_clamps: copied once per solve from any node's
        ///        resource, then reused for every probe.
        struct ClampProbe {
                explicit ClampProbe(const Resource<ResourceType>& any)
                    : first_input(any), second_input(any), first_output(any), second_output(any) {}

                Resource<ResourceType> first_input;
                Resource<ResourceType> second_input;
                Resource<ResourceType> first_output;
                Resource<ResourceType> second_output;
        };

        /// @brief Observes the clamps a backward step and a forward step arriving at @p node get,
        ///        and at a sink the start a backward label gets.
        ///
        /// A backward step arrives over an arc leaving the node, a forward one over an arc
        /// entering it. A node no arc leaves is never the arrival of a backward step, so it gets
        /// no backward probe; likewise for the forward probe at a node no arc enters. A backward
        /// label starts at a sink through an arc entering it, as @c BackwardDirection::seed reads
        /// it. A clamp or start not probed reads as none.
        ///
        /// @param node    The node.
        /// @param removed The removed arcs, for a node whose live arcs are all gone.
        /// @param kinds   Each component's backward kind.
        /// @param probe   Reused scratch resources.
        /// @param clamps  Receives the clamps.
        static void observe_node_clamps(const Node<ResourceType>& node, const RemovedArcs& removed,
                                        const std::vector<BackwardKind>& kinds, ClampProbe* probe,
                                        NodeClamps* clamps) {
            const auto* leaving = arc_to_probe(node.out_arcs, removed.leaving, node.id);
            const auto* entering = arc_to_probe(node.in_arcs, removed.entering, node.id);
            clamps->has_leaving = leaving != nullptr;
            clamps->has_entering = entering != nullptr;
            clamps->has_start = node.sink && entering != nullptr;
            if (leaving != nullptr) {
                observe_clamps(*leaving, kinds, /*backward=*/true, probe, &clamps->backward);
            } else {
                std::ranges::fill(clamps->backward, std::nullopt);
            }
            if (entering != nullptr) {
                observe_clamps(*entering, kinds, /*backward=*/false, probe, &clamps->forward);
            } else {
                std::ranges::fill(clamps->forward, std::nullopt);
            }
            if (clamps->has_start) {
                observe_starts(*entering, kinds, probe, &clamps->start);
            } else {
                std::ranges::fill(clamps->start, std::nullopt);
            }
        }

        /// @brief What the clamp checks have found so far, per component.
        struct ClampCheckState {
                explicit ClampCheckState(size_t count)
                    : needs_scan(count, false),
                      ceiling_reported(count, false),
                      floor_reported(count, false) {}

                /// Whether @ref scan_negative_loads must check the component's arcs.
                std::vector<bool> needs_scan;
                /// Whether @ref check_ceiling reported the component already.
                std::vector<bool> ceiling_reported;
                /// Whether @ref check_floor reported the component already.
                std::vector<bool> floor_reported;
        };

        /// @brief Checks one component of one node against the clamps observed there.
        ///
        /// Marks the component for the negative-load scan when its feasibility function requires
        /// non-decreasing values; for a @c Threshold, runs @ref check_ceiling where a backward
        /// step can arrive and where a backward label starts, and @ref check_floor where a
        /// forward step can arrive.
        ///
        /// @param component       The node's resource component.
        /// @param component_index The component's index.
        /// @param node_id         The node.
        /// @param kind            The component's backward kind.
        /// @param clamps          The clamps observed at the node.
        /// @param state           What the checks have found so far.
        /// @param problems        Receives one line per offending component.
        template <typename ComponentResource>
        static void check_component_clamps(const ComponentResource& component,
                                           size_t component_index, size_t node_id,
                                           BackwardKind kind, const NodeClamps& clamps,
                                           ClampCheckState* state,
                                           std::vector<std::string>* problems) {
            if (component.requires_nondecreasing()) {
                state->needs_scan[component_index] = true;
            }
            using Value = std::decay_t<decltype(component.get_value())>;
            if constexpr (is_numerical_resource_v<Value>) {
                if (kind != BackwardKind::Threshold) {
                    return;
                }
                if (clamps.has_leaving) {
                    check_ceiling(component,
                                  clamps.backward[component_index],
                                  clamps.forward[component_index],
                                  component_index,
                                  node_id,
                                  /*start=*/false,
                                  &state->ceiling_reported,
                                  problems);
                }
                if (clamps.has_start) {
                    check_ceiling(component,
                                  clamps.start[component_index],
                                  clamps.forward[component_index],
                                  component_index,
                                  node_id,
                                  /*start=*/true,
                                  &state->ceiling_reported,
                                  problems);
                }
                if (clamps.has_entering && check_floor(component,
                                                       clamps.forward[component_index],
                                                       clamps.backward[component_index],
                                                       component_index,
                                                       node_id,
                                                       &state->floor_reported,
                                                       problems)) {
                    state->needs_scan[component_index] = true;
                }
            }
        }

        /// @brief An arc to observe a node's clamps through: a live one, or else one that
        ///        preprocessing removed.
        ///
        /// @param live    The node's live arcs in the direction asked for.
        /// @param removed The removed arcs, by the node they leave or enter.
        /// @param node_id The node.
        /// @return The arc, or @c nullptr when the model has none.
        [[nodiscard]] static const Arc<ResourceType>* arc_to_probe(
            const std::vector<Arc<ResourceType>*>& live,
            const std::unordered_map<size_t, const Arc<ResourceType>*>& removed, size_t node_id) {
            for (const auto* arc : live) {
                if (arc->extender != nullptr) {
                    return arc;
                }
            }
            const auto it = removed.find(node_id);
            return it != removed.end() ? it->second : nullptr;
        }

        /// @brief What @p arc's extension does to values beyond every bound, per component: the
        ///        value a @c Threshold component clamps them to, or @c std::nullopt where its
        ///        output follows its input (and for every other component).
        ///
        /// Two such values go through the arc, backward to observe the clamp at its origin, or
        /// forward to observe the one at its destination. A clamp sends both to one value; a
        /// function that does not clamp keeps them apart. Only @c extend and @c extend_back are
        /// called, so a clamp is seen however the extension is written.
        ///
        /// @param arc      The arc.
        /// @param kinds    Each component's backward kind.
        /// @param backward Whether to push the values backward.
        /// @param probe    Reused inputs and outputs.
        /// @param clamps   Receives one entry per component.
        static void observe_clamps(const Arc<ResourceType>& arc,
                                   const std::vector<BackwardKind>& kinds, bool backward,
                                   ClampProbe* probe, std::vector<std::optional<double>>* clamps) {
            const auto place = [&](Resource<ResourceType>* input, bool second) {
                size_t index = 0;
                input->for_each_component(
                    *arc.extender,
                    [&](auto& component, const auto& extender) {
                        const size_t component_index = index++;
                        using Value = std::decay_t<decltype(component.get_value())>;
                        if constexpr (is_numerical_resource_v<Value>) {
                            if (component_index < kinds.size() &&
                                kinds[component_index] == BackwardKind::Threshold) {
                                component.set_value(beyond_bounds(extender->get_value().get_value(),
                                                                  backward,
                                                                  second));
                            }
                        }
                    });
            };
            place(&probe->first_input, /*second=*/false);
            place(&probe->second_input, /*second=*/true);

            const auto push = [backward](auto& output, const auto& input, const auto& extender) {
                if (backward) {
                    extender.extend_back(input, &output);
                } else {
                    extender.extend(input, &output);
                }
            };
            probe->first_output.for_each_component(probe->first_input, *arc.extender, push);
            probe->second_output.for_each_component(probe->second_input, *arc.extender, push);

            size_t index = 0;
            probe->first_output.for_each_component(
                probe->second_output,
                [&](const auto& first, const auto& second) {
                    const size_t component_index = index++;
                    if (component_index >= clamps->size()) {
                        return;
                    }
                    std::optional<double> clamp;
                    using Value = std::decay_t<decltype(first.get_value())>;
                    if constexpr (is_numerical_resource_v<Value>) {
                        const auto value = first.get_value().get_value();
                        if (kinds[component_index] == BackwardKind::Threshold &&
                            value == second->get_value().get_value()) {
                            clamp = static_cast<double>(value);
                        }
                    }
                    (*clamps)[component_index] = clamp;
                });
        }

        /// @brief The start @p arc's extension gives a backward label at its destination, per
        ///        component: what a @c Threshold component sets (@ref observe_start), or
        ///        @c std::nullopt where it leaves the type default (and for every other component).
        ///
        /// @param arc    An arc entering a sink.
        /// @param kinds  Each component's backward kind.
        /// @param probe  Reused scratch resources.
        /// @param starts Receives one entry per component.
        static void observe_starts(const Arc<ResourceType>& arc,
                                   const std::vector<BackwardKind>& kinds, ClampProbe* probe,
                                   std::vector<std::optional<double>>* starts) {
            size_t index = 0;
            probe->first_input.for_each_component(
                *arc.extender,
                [&](auto& component, const auto& extender) {
                    const size_t component_index = index++;
                    if (component_index >= starts->size()) {
                        return;
                    }
                    (*starts)[component_index] = kinds[component_index] == BackwardKind::Threshold
                                                     ? observe_start(*extender, &component)
                                                     : std::nullopt;
                });
        }

        /// @brief The value @p extender starts a backward label with at its arc's destination,
        ///        or @c std::nullopt where it leaves the type default.
        ///
        /// Two values beyond every bound are started in turn. A start sends both to one value; a
        /// function that sets none keeps them apart, as @ref observe_clamps tells a clamp. Only
        /// @c start_back is called, so a start is seen however the extension is written.
        ///
        /// @param extender The component's extender, on an arc entering the sink.
        /// @param scratch  A resource of the component's type, overwritten.
        /// @return The start, or @c std::nullopt; always @c std::nullopt without a scalar value.
        template <typename ComponentExtender, typename ComponentResource>
        [[nodiscard]] static std::optional<double> observe_start(const ComponentExtender& extender,
                                                                 ComponentResource* scratch) {
            using Value = std::decay_t<decltype(scratch->get_value())>;
            if constexpr (is_numerical_resource_v<Value>) {
                using Scalar = std::decay_t<decltype(std::declval<Value>().get_value())>;
                scratch->set_value(beyond_bounds(Scalar{0}, /*backward=*/true, /*second=*/false));
                extender.start_back(scratch);
                const auto first = scratch->get_value().get_value();
                scratch->set_value(beyond_bounds(Scalar{0}, /*backward=*/true, /*second=*/true));
                extender.start_back(scratch);
                if (first == scratch->get_value().get_value()) {
                    return static_cast<double>(first);
                }
            }
            return std::nullopt;
        }

        /// @brief A value beyond every bound a model can state, which the arc's own step keeps
        ///        in range: above every bound going backward, below every bound going forward.
        ///
        /// Floating point uses the infinities, then the finite extremes. An integral type uses
        /// its extreme, moved by a negative consumption so that a translation cannot overflow,
        /// then the value one step inside it.
        ///
        /// @param consumption The arc's consumption.
        /// @param backward    Whether the value goes backward.
        /// @param second      Whether this is the second of the two values.
        /// @return The value.
        template <typename Scalar>
        [[nodiscard]] static Scalar beyond_bounds(Scalar consumption, bool backward, bool second) {
            using Limits = std::numeric_limits<Scalar>;
            if constexpr (std::is_floating_point_v<Scalar>) {
                if (backward) {
                    return second ? Limits::max() : Limits::infinity();
                }
                return second ? Limits::lowest() : -Limits::infinity();
            } else {
                const bool negative = consumption < Scalar{0};
                if (backward) {
                    const Scalar top = negative ? Limits::max() + consumption : Limits::max();
                    return second ? top - 1 : top;
                }
                const Scalar bottom = negative ? Limits::lowest() - consumption : Limits::lowest();
                return second ? bottom + 1 : bottom;
            }
        }

        /// @brief A value just above @p value, beyond the tolerance the library's own comparisons
        ///        allow (@c value_leq adds the type's epsilon), so that a test of a ceiling at
        ///        @p value rejects it.
        ///
        /// @param value The value.
        /// @return A few units in the last place above @p value for floating point, the next
        ///         value for an integral type, and @p value itself at an integral type's maximum.
        template <typename Scalar>
        [[nodiscard]] static Scalar just_above(Scalar value) {
            using Limits = std::numeric_limits<Scalar>;
            if constexpr (std::is_floating_point_v<Scalar>) {
                constexpr Scalar kSteps = 4;
                return value + (kSteps * Limits::epsilon() * std::max(Scalar{1}, std::abs(value)));
            } else {
                return value == Limits::max() ? value : static_cast<Scalar>(value + 1);
            }
        }

        /// @brief Records a @c Threshold component whose backward clamp at @p node_id, or whose
        ///        backward start at sink @p node_id, disagrees with that node's feasibility
        ///        function, once per component.
        ///
        /// A backward value is a deadline: the latest value at which the rest of the path is still
        /// feasible. At a node it can be no later than the largest value the node's forward test
        /// admits, its ceiling, and it is that ceiling when nothing later lowers it. So the clamp
        /// of a backward step arriving at the node, and the start of a backward label at a sink,
        /// must both be the ceiling. Four disagreements:
        ///  - none, where the forward test has a ceiling (it rejects a value beyond every bound):
        ///    without a clamp, the backward search admits deadlines the forward search rejects;
        ///    without a start, the label keeps the type default and rejects deadlines a forward
        ///    path meets;
        ///  - one the node's backward test rejects, so every backward label there is lost;
        ///  - one the forward test rejects, above the ceiling: the backward search admits
        ///    deadlines the forward search rejects, which a test on the floor alone (a time
        ///    window's) cannot see;
        ///  - one with a value just above it (@ref just_above) the forward test admits, below the
        ///    ceiling: the backward search rejects deadlines a forward path meets.
        ///
        /// The forward test is asked, not read, so it must be exact at its ceiling: one admitting
        /// values a little above it, beyond the library's own tolerance, is refused. Where the
        /// forward test has no ceiling, only the backward test is asked.
        ///
        /// A clamp below the lowest value a forward label can hold at the node
        /// (@ref lowest_forward_value) is left alone: the node's window is empty, so it cannot be
        /// reached in time whatever the ceiling, which is the model's business, not an
        /// incoherence.
        ///
        /// @param component        The node's resource component.
        /// @param ceiling          The clamp a backward step arriving at the node was observed to
        ///                         apply, or the start a backward label at the sink was observed
        ///                         to take; @c std::nullopt when there is none.
        /// @param forward_clamp    The clamp a forward step arriving at the node was observed to
        ///                         apply, if any.
        /// @param component_index  The component's index, for the message.
        /// @param node_id          The node, for the message.
        /// @param start            Whether @p ceiling is a sink's backward start, not a clamp.
        /// @param ceiling_reported Which components were reported already.
        /// @param problems         Receives the line, if any.
        template <typename ComponentResource>
        static void check_ceiling(const ComponentResource& component, std::optional<double> ceiling,
                                  const std::optional<double>& forward_clamp,
                                  size_t component_index, size_t node_id, bool start,
                                  std::vector<bool>* ceiling_reported,
                                  std::vector<std::string>* problems) {
            if ((*ceiling_reported)[component_index]) {
                return;
            }
            using Value = std::decay_t<decltype(component.get_value())>;
            using Scalar = std::decay_t<decltype(std::declval<Value>().get_value())>;
            const auto admits = [&component](Scalar scalar) {
                Value value;
                value.set_value(scalar);
                return component.admits_value(value);
            };
            const bool has_ceiling =
                !admits(beyond_bounds(Scalar{0}, /*backward=*/true, /*second=*/false));
            const std::string subject = "component " + std::to_string(component_index) +
                                        ": its backward labels at " + (start ? "sink " : "node ") +
                                        std::to_string(node_id);
            if (!ceiling) {
                if (has_ceiling) {
                    (*ceiling_reported)[component_index] = true;
                    problems->push_back(
                        subject +
                        (start ? " start at the type default" : " are not clamped at all") +
                        ", but its feasibility function rejects values above some bound there, so "
                        "the backward search " +
                        (start ? "rejects deadlines a forward path meets; start each backward "
                                 "label at its sink's upper bound, as ThresholdForm::start_back "
                                 "does"
                               : "admits deadlines the forward search rejects; clamp each "
                                 "backward label to its node's upper bound, as ThresholdForm "
                                 "does") +
                        ", built from the feasibility function's NodeBounds");
                }
                return;
            }
            if (*ceiling < lowest_forward_value(forward_clamp)) {
                return;
            }
            const auto scalar = static_cast<Scalar>(*ceiling);
            Value value;
            value.set_value(scalar);
            std::string fault;
            if (!component.admits_back_value(value)) {
                fault =
                    ", which its feasibility function rejects there, so every backward label at "
                    "that node is lost";
            } else if (!has_ceiling) {
                return;
            } else if (!admits(scalar)) {
                fault =
                    ", above the largest value its feasibility function admits there, so the "
                    "backward search admits deadlines the forward search rejects";
            } else if (const Scalar above = just_above(scalar); above != scalar && admits(above)) {
                fault =
                    ", below the largest value its feasibility function admits there, so the "
                    "backward search rejects deadlines a forward path meets";
            } else {
                return;
            }
            (*ceiling_reported)[component_index] = true;
            problems->push_back(subject + (start ? " start at " : " are clamped to ") +
                                std::to_string(*ceiling) + fault +
                                "; build the extension and the feasibility function from one "
                                "NodeBounds (make_node_bounds, or the feasibility function's "
                                "bounds())");
        }

        /// @brief The lowest value a forward label can hold at a node: the forward clamp observed
        ///        there, or else 0, since values start at 0 and, once the negative-load scan has
        ///        ruled out a decrease, never fall.
        ///
        /// @param forward_clamp The forward clamp observed at the node, if any.
        /// @return The value.
        [[nodiscard]] static double lowest_forward_value(
            const std::optional<double>& forward_clamp) {
            return forward_clamp.value_or(0.0);
        }

        /// @brief Records a @c Threshold component whose backward test at @p node_id rejects a
        ///        value a forward label can hold there, once per component.
        ///
        /// The backward search would then reject deadlines a forward path meets. The test is
        /// asked directly, at @ref lowest_forward_value: a floor at or below that value is met.
        /// A node whose backward clamp is below it has an empty window, which is left alone, as
        /// @ref check_ceiling leaves it.
        ///
        /// @param component       The node's resource component.
        /// @param forward_clamp   The forward clamp observed at the node, or @c std::nullopt when
        ///                        the extension applies none.
        /// @param backward_clamp  The backward clamp observed at the node, if any.
        /// @param component_index The component's index, for the message.
        /// @param node_id         The node, for the message.
        /// @param floor_reported  Which components were reported already.
        /// @param problems        Receives the line, if any.
        /// @return Whether the component relies on the negative-load scan: its backward test has
        ///         a floor, rejecting a value below every bound, and its extension no forward
        ///         clamp.
        template <typename ComponentResource>
        static bool check_floor(const ComponentResource& component,
                                const std::optional<double>& forward_clamp,
                                const std::optional<double>& backward_clamp, size_t component_index,
                                size_t node_id, std::vector<bool>* floor_reported,
                                std::vector<std::string>* problems) {
            if ((*floor_reported)[component_index]) {
                return false;
            }
            using Value = std::decay_t<decltype(component.get_value())>;
            using Scalar = std::decay_t<decltype(std::declval<Value>().get_value())>;
            const auto admits = [&component](Scalar scalar) {
                Value value;
                value.set_value(scalar);
                return component.admits_back_value(value);
            };
            if (admits(beyond_bounds(Scalar{0}, /*backward=*/false, /*second=*/false))) {
                return false;  // no floor at all
            }
            const double lowest = lowest_forward_value(forward_clamp);
            const bool empty_window = backward_clamp && *backward_clamp < lowest;
            if (!empty_window && !admits(static_cast<Scalar>(lowest))) {
                (*floor_reported)[component_index] = true;
                problems->push_back(
                    "component " + std::to_string(component_index) +
                    ": its feasibility function rejects a backward value of " +
                    std::to_string(lowest) + " at node " + std::to_string(node_id) +
                    ", the lowest value a forward label can hold there, so the backward search "
                    "rejects deadlines a forward path meets; build the extension and the "
                    "feasibility function from the same windows (TimeWindowExtensionFunction with "
                    "TimeWindowFeasibilityFunction), or pair CapacityExtensionFunction with "
                    "MinMaxFeasibilityFunction(0, capacity)");
                return false;
            }
            return !forward_clamp;
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
