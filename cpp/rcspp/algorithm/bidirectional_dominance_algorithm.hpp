// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <limits>
#include <list>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/directional_dominance_algorithm.hpp"
#include "rcspp/algorithm/half_way_policy.hpp"
#include "rcspp/algorithm/label_join.hpp"
#include "rcspp/preprocessor/bellman_ford_algorithm.hpp"

namespace rcspp {

/// @brief Bidirectional labeling: a forward search, a backward search, and a join.
///
/// The class **is** the forward search -- it derives from
/// @ref DirectionalDominanceAlgorithm bound to @ref ForwardDirection -- and adds a second container
/// set and a second frontier. It deliberately does *not* compose two algorithm objects: each
/// `Algorithm` brings its own label pool, solution set, upper bound, memory limit and timers, and
/// the join needs to tighten one bound that both searches prune against.
///
/// Everything except the frontier bookkeeping is shared, through the direction-templated helpers on
/// the base: extension, feasibility, dominance update, label removal and release are written once
/// and instantiated twice. The `release_label` / `release_with_ref_count` cascade in particular is
/// never mirrored.
///
/// @tparam ResourceType       The resource type carried by labels.
/// @tparam LabelContainerType The forward per-node container.
/// @tparam CriticalRC         The critical resource's *type*: reading a component needs a type, not
///                            just an index. Wrapped by @ref BidirectionalAlgoBound so the class
///                            still fits the two-parameter shape the entry points require.
/// @tparam CostRC             The cost resource's *type*, which the completion bounds relax on.
///                            Defaults to @p CriticalRC because the Python dispatch binds the two
///                            together, but they are different questions: a model may have a
///                            real-valued cost and an integer clock. The index within the slot is
///                            @c params_.heuristic_cost_index.
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>,
          typename CriticalRC = RealResource, typename CostRC = CriticalRC>
    requires ResourceTypeConcept<ResourceType>
class BidirectionalDominanceAlgorithm
    : public DirectionalDominanceAlgorithm<ResourceType, LabelContainerType, ForwardDirection> {
        using Base =
            DirectionalDominanceAlgorithm<ResourceType, LabelContainerType, ForwardDirection>;

        /// @brief The backward container is fixed internally rather than parameterised.
        ///
        /// `LabelBuckets` is forward-only in v1, so a backward search uses a list anyway -- and
        /// keeping it out of the template list is what lets this class stay within the
        /// two-parameter shape `ResourceGraph::solve` and the Python dispatch table accept.
        using BackwardContainer = LabelList<ResourceType, BackwardDirection>;

    public:
        BidirectionalDominanceAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                                        AlgorithmParams<LabelContainerType> params)
            : Base(resource_factory, std::move(params)) {}

        ~BidirectionalDominanceAlgorithm() override = default;

        /// @brief Whether the half-way bound was in force for the last solve.
        ///
        /// Surfaced so a disabled bound is visible rather than only logged: the difference between
        /// "this pricing iteration was mysteriously slow" and a one-line explanation. A disabled
        /// bound costs speed, not optimality, so it deliberately does *not* set
        /// `could_be_non_optimal()`.
        [[nodiscard]] bool bounded_by_half_way() const { return half_way_.enabled(); }

        /// @brief How many complete paths the join pass produced during the last solve.
        ///
        /// A diagnostic, and the one number that tells the two payoff failures apart. Zero on an
        /// instance that still returns the optimum means the answer came from a search reaching a
        /// terminal, not from the join -- which is exactly what happens to a route whose clock
        /// never reaches `H` and has therefore no crossing arc. A number that dwarfs the label
        /// count means the opposite: the crossing test is not filtering, and every surviving pair
        /// is being joined.
        ///
        /// @return The number of joins accepted, reset at the start of each solve.
        [[nodiscard]] size_t number_of_joined_paths() const { return joined_paths_; }

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
            forward_frontier_.clear();
            backward_frontier_.clear();

            backward_labels_by_node_pos_.clear();
            backward_labels_by_node_pos_.reserve(num_nodes);
            for (size_t i = 0; i < num_nodes; ++i) {
                backward_labels_by_node_pos_.emplace_back(BackwardContainer{});
            }

            // Refuse to run on a model whose author has not declared the backward semantics.
            // Naming the component here costs one pass at setup; discovering it from inside the
            // join costs a solve. Throwing is right, unlike the half-way bound below: a model that
            // cannot express backward semantics cannot produce a correct answer at all, whereas a
            // missing bound is only slow.
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
        /// Balancing by frontier size keeps the two halves meeting near the middle without needing
        /// a schedule; `Node::pos()` comes from a cost-driven connectivity sort, which is not a
        /// topological order on a cyclic graph, so nothing stronger is safe to assume.
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
        /// Three sources, not one. A complete path whose critical value never exceeds `H` has no
        /// arc on which the clock crosses `H`, so the joiner never produces it -- it exists only as
        /// a forward label that ran all the way to a sink. Short routes are exactly that case and
        /// are common in pricing. Duplicates across the three are absorbed by `Solution`'s hash.
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

        /// @brief Trims both frontiers and tightens the per-node quota, as every other algorithm
        ///        does.
        ///
        /// Trimming alone is a one-off dip: the frontiers refill from continued extension on the
        /// next few iterations, because `step<Dir>`'s per-node quota is still whatever the caller
        /// set (`MAX_INT` by default). Lowering `effective_max_labels_per_node_` is what turns a
        /// pressure event into a change of regime, and it is why every other algorithm's
        /// `on_memory_pressure` does both.
        ///
        /// There is deliberately no "second pressure event releases what the first set aside" arm
        /// here, which `Simple`, `Pushing`, `Pulling` and `AStar` all have: `trim_frontier` sets
        /// nothing aside. A frontier entry it drops stays in its node's container -- so nothing
        /// dangles -- and is simply never extended again. `memory_pressure_triggered_` is
        /// therefore informational in this algorithm, and is set so that
        /// `memory_pressure_was_triggered()` can report it.
        /// @brief Puts the two bidirectional diagnostics on the result the caller receives.
        ///
        /// The accessors above stay and read the same members, so the two cannot disagree; this is
        /// the copy that crosses the Python boundary, where the algorithm object does not.
        void annotate(SolveResult* result) const override {
            result->bounded_by_half_way = half_way_.enabled();
            result->number_of_joined_paths = joined_paths_;
        }

        void on_memory_pressure() override {
            Base::on_memory_pressure();
            this->effective_max_labels_per_node_ =
                this->params_.memory_pressure_max_labels_per_node;
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
            // Unused: main_loop() drives both frontiers directly rather than through the single
            // frontier hook the one-directional algorithms use.
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
        /// Returns nothing: every reason to abandon a label -- dominated, over its node's
        /// extension quota, beyond the completion bound, past `H` -- abandons that label alone and
        /// leaves the loop running. The reasons to stop the whole solve (timeout, interrupt,
        /// memory) are the base class's and are checked in `main_loop`'s own condition, so a
        /// second "stop everything" channel out of here would only be a second place for them to
        /// disagree.
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
                // KNOWN GAP, deliberately left open. A label abandoned here is still in its node's
                // container but is never grown, so it never produces the past-`H` boundary label
                // the join reads. The join therefore sees fewer forward halves than it would at an
                // unbounded quota -- fewer columns, and possibly a worse incumbent.
                //
                // Only reachable in two already-inexact modes: `num_labels_to_extend_by_node` set
                // by the caller (truncated labeling), and `on_memory_pressure` tightening the quota
                // to `memory_pressure_max_labels_per_node`. At the default of `MAX_INT` this never
                // fires. `could_be_non_optimal()` already reports the first; the second is a
                // deliberate trade of answers for memory, and a smaller join is consistent with it.
                //
                // It also sits on top of a larger pre-existing gap rather than beside one: this
                // algorithm's `number_of_labels()` is the two frontier sizes, and `main_loop` runs
                // until both are empty, so the phase loop in `Algorithm::solve` always exits after
                // one pass. Truncated labeling's second phase never runs here, and the labels
                // skipped above are never revisited by any mechanism.
                //
                // The fix, if a measured case ever wants it: extend the label anyway when its clock
                // is at or below `H`, keep only the results past `H`, and queue none of them. That
                // restores exactly the join's input at the cost of one extension per out-arc, and
                // it cannot regrow the frontier because nothing past `H` is ever extended. It is
                // NOT applied today because nothing in the suite exercises truncated bidirectional,
                // and because under memory pressure it spends the memory the quota exists to save.
                return;
            }
            ++extended_count;

            // Completion bound, NOT the raw cost test. A forward half costing 30 completes to
            // something else -- and with reduced costs, possibly to -20 -- so comparing a half's
            // own cost against the incumbent discards exactly the labels that lead to the best
            // columns, while reporting COMPLETE.
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
                // Record it now, not only in the end-of-solve sweep, when the caller asked for
                // dominated solutions too: a label that reaches a terminal and is later dominated
                // is gone from its container by then. This is what the one-directional loop does,
                // and it is also what makes `stop_after_X_solutions` able to stop the search --
                // `should_stop()` reads `solutions_.size()`, which otherwise stays at zero until
                // after `main_loop` has already run to completion.
                //
                // A backward chain walks `get_out_arc()`, so it cannot go through the `Label`
                // overload of `extract_solution`, which walks `get_in_arc()`.
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

            // The half-way bound: forward labels stop above H, backward labels below it.
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

            extend_into<Dir>(label_ptr, containers, frontier);
            return;
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

        /// @brief Runs the join pass, feeding every accepted pair to `extract_solution`.
        ///
        /// The halves are paired at the node they meet on; see @c Joiner::join for what that
        /// pairing does and does not consider.
        void run_join_pass() {
            auto record = [this](double cost, std::vector<size_t> arc_ids, size_t end_node_id) {
                ++joined_paths_;
                this->extract_solution(cost, std::move(arc_ids), end_node_id);
            };
            joiner_.join(*this->graph_,
                         this->non_dominated_labels_by_node_pos_,
                         backward_labels_by_node_pos_,
                         half_way_,
                         this->params_.critical_resource_index,
                         this->best_cost_upper_bound_,
                         this->params_.prune_based_on_upper_bound_,
                         this->cost_upper_bound_,
                         record);
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
            const auto* end_node = (current != nullptr ? current : last)->get_end_node();
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
        void configure_half_way(const Graph<ResourceType>& graph) {
            if (this->params_.dynamic_half_way) {
                LOG_WARN(
                    "BidirectionalDominanceAlgorithm: dynamic_half_way is reserved and not yet "
                    "implemented; the half-way point is static for this solve.\n");
            }

            half_way_ = HalfWayPolicy(this->params_.half_way_point, resource_upper_bound(graph));

            if constexpr (!is_cost_in_composition_v<CriticalRC, ResourceType>) {
                LOG_DEBUG(
                    "BidirectionalDominanceAlgorithm: the critical resource type is not in "
                    "the model; disabling the half-way bound for this solve.\n");
                half_way_.disable();
            } else {
                if (!half_way_.enabled()) {
                    return;
                }
                if (critical_backward_kind(graph) != BackwardKind::Threshold) {
                    LOG_DEBUG(
                        "BidirectionalDominanceAlgorithm: critical resource ",
                        this->params_.critical_resource_index,
                        " does not extend backwards as a threshold, so its backward values are "
                        "not on the forward scale; disabling the half-way bound for this solve.\n");
                    half_way_.disable();
                    return;
                }
                const std::vector<double> probes{0.0, half_way_.h()};
                if (!critical_is_monotone(graph, probes)) {
                    LOG_DEBUG("BidirectionalDominanceAlgorithm: critical resource ",
                              this->params_.critical_resource_index,
                              " is not monotone; disabling the half-way bound for this solve.\n");
                    half_way_.disable();
                    return;
                }
                if (!critical_dominance_is_increasing(graph)) {
                    LOG_DEBUG(
                        "BidirectionalDominanceAlgorithm: critical resource ",
                        this->params_.critical_resource_index,
                        " does not take part in the dominance order as an increasing one, so a "
                        "dominator may sit above H while the label it evicted sat below it; "
                        "disabling the half-way bound for this solve.\n");
                    half_way_.disable();
                }
            }
        }

        /// @brief The backward kind of the critical component, read off any arc's extender.
        ///
        /// The half-way rule compares a backward label's critical value against `H`, a value on
        /// the *forward* scale. Only a `Threshold` backward extension puts it there: it carries a
        /// ceiling -- the largest forward value still admissible at this node -- counting down
        /// from `R` exactly as the forward value counts up from zero. An `Accumulate` (or
        /// `Mirror`) backward value instead measures consumption from the sink, so it *starts* at
        /// zero and grows, and `should_stop_backward` discards the seed itself: the backward
        /// search finds nothing, the join finds nothing, and the solver reports COMPLETE.
        ///
        /// Rescaling it to `R - consumed` would be sound but only as a relaxation, and it would
        /// silently make `R` -- which is derived, not declared -- load-bearing for correctness.
        /// So this disables the bound instead: correct but slow, the same response a non-monotone
        /// clock gets. A model that wants the bound must give the clock a threshold extension
        /// (`TimeWindowExtensionFunction`, `BudgetExtensionFunction`).
        ///
        /// @param graph The graph whose arcs carry the extenders.
        /// @return The critical component's declared backward kind, or `Unspecified` when the
        ///         graph has no arc carrying an extender.
        [[nodiscard]] BackwardKind critical_backward_kind(const Graph<ResourceType>& graph) const {
            // Read off ONE arc: every extender in a graph comes from the same
            // ResourceCompositionFactory, so they all carry the same component layout and the same
            // declared shapes. Stated here because nothing enforces it.
            const auto* arc = graph.first_arc();
            if (arc == nullptr || arc->extender == nullptr) {
                return BackwardKind::Unspecified;
            }
            return arc->extender
                ->template get_component<CriticalRC>(this->params_.critical_resource_index)
                .backward_kind();
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

        /// @brief `R`, the critical resource's finite maximum -- which this algorithm always
        ///        derives from `H` rather than from the model.
        ///
        /// There is no general accessor for a feasibility function's upper bound, so the number
        /// here is `2H` when the caller supplied `H` and infinity otherwise. Two consequences worth
        /// knowing:
        ///
        ///  - `HalfWayPolicy`'s "derive H as R/2" branch is unreachable from this algorithm. It is
        ///    reachable, and tested, from a direct construction; the constructor keeps both
        ///    arguments because that is where the distinction is real.
        ///  - `half_way_point = 0` therefore means **bound off**, and that sentinel is relied on:
        ///    the equivalence suite gets its unbounded runs that way.
        ///
        /// Deriving a real `R` is possible -- a threshold clock's `back_seed_value()` at a sink IS
        /// its upper bound -- but it would change what `half_way_point = 0` means, so it needs its
        /// own way to say "bound off" and its own change.
        [[nodiscard]] double resource_upper_bound(const Graph<ResourceType>& /*graph*/) const {
            return this->params_.half_way_point > 0.0 ? this->params_.half_way_point * 2.0
                                                      : std::numeric_limits<double>::infinity();
        }

        /// @brief Computes both completion bounds, inheriting A*'s negative-cycle discipline.
        ///
        /// Guarded on the COST type, not the critical one. The two coincide whenever the Python
        /// dispatch built the algorithm, because `BidirectionalAlgoEntry` binds them together --
        /// but they answer different questions, and a C++ caller can separate them.
        ///
        /// With no cost component in the pack the bounds stay at zero, i.e. prune nothing. A*
        /// falls back to `arc.cost` in the same situation; this deliberately does not. A* uses its
        /// `h` for frontier ordering as well as pruning, so a rough one still earns its keep;
        /// here `h` does nothing but prune, and an over-estimating prune loses the optimum while
        /// reporting COMPLETE.
        void compute_completion_bounds(const Graph<ResourceType>& graph) {
            const size_t num_nodes = graph.get_number_of_nodes();
            h_to_sink_.assign(num_nodes, 0.0);
            h_from_source_.assign(num_nodes, 0.0);

            // The only consumer is step<Dir>'s completion-bound prune, which is gated on the same
            // flag. Two O(V*E) relaxations per solve is a real cost in a pricing loop that calls
            // solve() thousands of times, and zeros are exactly what "prune nothing" needs anyway.
            // The `assign` calls above stay before this return: `completion_bound<Dir>` indexes
            // with `.at()` and the vectors must remain correctly sized.
            if (!this->params_.prune_based_on_upper_bound_) {
                return;
            }

            if constexpr (is_numerical_resource_v<CostRC> &&
                          is_cost_in_composition_v<CostRC, ResourceType>) {
                fill_bound(graph, graph.get_sink_node_ids(), /*forward=*/false, &h_to_sink_);
                fill_bound(graph, graph.get_source_node_ids(), /*forward=*/true, &h_from_source_);
            }
        }

        /// @brief Runs one Bellman-Ford pass over the LABELING cost slot.
        ///
        /// Not the three-argument `solve(graph, targets, forward)` overload, which relaxes on
        /// `arc.cost`. `arc.cost` is the ORIGINAL arc weight -- `update_reduced_costs` rewrites the
        /// cost *component* and leaves `arc.cost` alone, because `extract_solution` sums it to
        /// build the column's original cost. So under reduced costs the two are different
        /// quantities, a sum of `arc.cost` over-estimates the remaining reduced cost, and an
        /// over-estimating bound is not admissible: it discards labels on the true optimal path
        /// and then reports COMPLETE. This is the same call, on the same slot, that
        /// `AStarDominanceAlgorithm::initialize()` makes.
        void fill_bound(const Graph<ResourceType>& graph, const std::vector<size_t>& targets,
                        bool forward, std::vector<double>* out) {
            try {
                auto distance = BellmanFordAlgorithm::solve<CostRC>(
                    graph, targets, this->params_.heuristic_cost_index, forward);
                for (size_t node_id : graph.get_node_ids()) {
                    const auto* node = graph.get_node(node_id);
                    auto it = distance.find(node_id);
                    out->at(node->pos()) = (it != distance.end())
                                               ? it->second
                                               : std::numeric_limits<double>::infinity();
                }
            } catch (const std::runtime_error&) {
                // A negative-cost cycle in the reduced-cost relaxation: the shortest cost-to-go is
                // -inf, so there is no finite lower bound. Prune nothing rather than falling back
                // to arc.cost -- see fill_bound's own note on why that substitution is
                // inadmissible. Same reasoning as AStarDominanceAlgorithm's catch block.
                std::ranges::fill(*out, 0.0);
            }
        }

        /// @brief Refuses to start on a model that cannot express backward semantics.
        ///
        /// The declarations live in two different places: `backward_kind()` on an *arc's* extension
        /// function and `merge_rule()` / `back_seed_value()` on a *node's* feasibility function. So
        /// this walks one arc's extender and one node's resource and pairs them by component index
        /// -- both compositions share a layout, so the orders agree.
        ///
        /// @throws std::runtime_error naming every offending component, before any label exists.
        void validate_backward_semantics(const Graph<ResourceType>& graph) const {
            // One arc, not a full sweep: every extender in a graph comes from the same
            // ResourceCompositionFactory, so they all declare the same component shapes. Stated
            // here because nothing enforces it.
            std::vector<BackwardKind> kinds;
            const auto* first = graph.first_arc();
            if (first != nullptr && first->extender != nullptr) {
                first->extender->for_each_component(
                    [&](const auto& component) { kinds.push_back(component.backward_kind()); });
            }

            // No arc carries an extender, so there is nothing to extend in either direction and
            // nothing to validate. This is NOT a degenerate case worth complaining about: it is
            // what `solve(preprocess = true)` produces whenever `upper_bound` binds hard enough,
            // and at column-generation convergence -- no improving column left -- the preprocessor
            // removes every arc. The forward algorithms return an empty result there; throwing
            // "component 0 declares no backward_kind()" would name a modelling problem that does
            // not exist and would break a CG loop on its terminating iteration.
            //
            // Reading the kinds from the resource factory's prototypes instead of from an arc
            // would remove the dependency on the arc set altogether. That is a larger change; this
            // early return is the fix for the symptom.
            if (kinds.empty()) {
                return;
            }

            const auto& node_ids = graph.get_node_ids();
            if (node_ids.empty()) {
                return;
            }
            const auto* node = graph.get_node(node_ids.front());
            if (node == nullptr || node->resource == nullptr) {
                return;
            }

            std::vector<std::string> problems;
            size_t index = 0;
            node->resource->for_each_component([&](const auto& component) {
                const auto kind = index < kinds.size() ? kinds[index] : BackwardKind::Unspecified;
                describe_problem(component, kind, index, &problems);
                ++index;
            });

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
            // The complaint that used to sit here -- "declares MergeRule::Disjoint but its
            // resource has no intersects()" -- retired in step 6. It is the one runtime check in
            // the shape-genericity work that was *replaced* rather than supplemented, and that is
            // legitimate here and only here: a scalar resource can no longer declare disjointness
            // at all, because the body lives on DisjointMergeForm and that does not compile
            // without intersects(). The mistake became unrepresentable rather than merely
            // detected, so nothing is lost for Python callers either -- they cannot construct the
            // bad pairing.
            if (kind == BackwardKind::Accumulate && seeds_itself_out_of_range(component)) {
                problems->push_back(
                    label +
                    ": its extension accumulates but its feasibility function seeds a backward "
                    "label at the far end of its range, so the first backward extension leaves "
                    "that range; use a Threshold extension (e.g. BudgetExtensionFunction) instead");
            }
        }

        /// @brief Whether an accumulating component starts its backward labels somewhere hopeless.
        ///
        /// The incoherent pairing this catches is a *capacity written as an addition*:
        /// `MinMaxFeasibilityFunction` seeds a backward label at the bound -- "start at the
        /// capacity" -- while the extension adds to it, so the label leaves the range on its first
        /// arc and the backward search silently finds nothing while the solver reports COMPLETE.
        /// Both halves declare legal values individually, so only a check that sees both catches
        /// it. Verified against this repository's own VRP subproblem, which used exactly that
        /// pairing.
        ///
        /// **Merely having a back seed is not the defect**, which an earlier form of this check got
        /// wrong: `MinMaxFeasibilityFunction` built with `merge_by_increasing_value = false` seeds
        /// at the *minimum*, which is precisely where an accumulating backward label should start,
        /// and the reverse-graph oracle's load resource is built that way on purpose.
        ///
        /// So the question is not "is there a seed" but "does the seed start out worse than where
        /// a forward label starts". Under an accumulation there is nowhere to go but further from
        /// the origin, so a seed the default state strictly dominates has no route back into
        /// feasibility. Asked through the component's own dominance function, which is the only
        /// thing that knows which direction "worse" runs in for this resource.
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
        /// A dropped entry that is not already `dominated` is abandoned rather than released: it
        /// stays in its node's container, so nothing dangles, but it is never extended and the
        /// result may no longer be optimal. `could_be_non_optimal()` reads params only and cannot
        /// see this, which is what `memory_pressure_was_triggered()` is for.
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

        size_t joined_paths_ = 0;
        Joiner<ResourceType, CriticalRC> joiner_;
};

/// @brief Binds the critical resource type, leaving a two-parameter template.
///
/// The same wrapper trick `AStarAlgoBound` uses, and for the same reason: `ResourceGraph::solve`
/// and the Python dispatch table accept only `template <typename, typename> class`.
///
/// @tparam CriticalRC The critical resource's type -- the clock.
/// @tparam CostRC     The cost resource's type, which the completion bounds relax on. Defaults to
///                    @p CriticalRC, which is what the Python dispatch wants; a C++ caller whose
///                    clock and cost live in different type slots names both.
template <typename CriticalRC, typename CostRC = CriticalRC>
struct BidirectionalAlgoBound {
        template <typename RT, typename LC>
        class Algo : public BidirectionalDominanceAlgorithm<RT, LC, CriticalRC, CostRC> {
            public:
                using BidirectionalDominanceAlgorithm<RT, LC, CriticalRC,
                                                      CostRC>::BidirectionalDominanceAlgorithm;
        };
};

}  // namespace rcspp
