// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <vector>

#include "rcspp/algorithm/half_way_policy.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp {

/// @brief Pairs surviving forward and backward labels at the node they meet on to form complete
///        paths.
///
/// The forward search extends a label only while its clock is at or below `H`. What comes out of
/// that extension lands *past* `H`, is stored at the far node, and is then popped, found to be past
/// `H`, and never extended again. Those **boundary labels** are what this pairs with the backward
/// labels sitting at the same node. Both halves are at one node, so their numbers are directly
/// comparable, and the join arc is already inside the forward half.
///
/// Three things have to hold for a pair to be a path, and only the last one costs anything:
///
///  1. **The crossing.** The critical resource is monotone, so its value along a complete path only
///     rises and straddles `H` on exactly one arc -- which is what makes each complete path appear
///     exactly once. Here that arc is the boundary label's own `in_arc`: `clock(g, v) > H` is one
///     comparison, and `clock(pred, u) <= H` needs no test at all, because `g` exists only because
///     `pred` was extended and the forward search extends nothing past `H`.
///  2. **Feasibility** is not tested, because an infeasible label is never stored. Nor are the
///     terminal rules -- a path may not have a sink or a source strictly inside it -- because the
///     search never extends out of a sink and `extend_label` refuses to extend into a seed.
///     Asserted below rather than assumed.
///  3. **The merge test**, per resource, via `can_be_merged`.
///
/// Cost is summed rather than compared: `merged = g.get_cost() + b.get_cost()`. The join arc is
/// counted exactly once, by `g`, because a backward label sitting *at* `v` has consumed no arc
/// into `v`.
///
/// Runs as a separate pass after both searches finish. Interleaving it would let the upper bound
/// tighten earlier, but would make "did we consider every valid pair?" depend on frontier
/// scheduling.
///
/// @tparam ResourceType The resource type carried by labels.
/// @tparam CriticalRC   The critical resource's type -- the clock the half-way rule reads.
template <typename ResourceType, typename CriticalRC>
class Joiner {
    public:
        /// @brief Pairs each node's boundary forward labels with the backward labels sitting
        ///        there.
        ///
        /// **What the dominance filter changes.** A boundary label had to survive dominance at the
        /// meeting node. A join that instead rebuilt the crossing label from scratch, once per
        /// candidate arc, would not consult that filter and so would pair more -- but the optimum
        /// is the same either way, because a dominated prefix completes no better than the prefix
        /// that dominated it. What differs is the number of *columns* returned, and the ones lost
        /// are ones the forward search would not have returned either. Measured against a
        /// forward-only solve on the generated suite and on every Solomon instance the benchmark
        /// uses: no column returned by both the forward search and an arc-based join was ever
        /// missing from this one.
        ///
        /// **With the bound disabled** the predicate degenerates to "has a predecessor", so every
        /// stored non-seed label is paired. Completeness then rests on the forward search reaching
        /// the sinks on its own, which it does, rather than on the join.
        ///
        /// **One place this sees less than pairing across the arc would, and it is not the
        /// dominance filter.** A boundary label exists only because its predecessor was extended,
        /// so a forward label the per-node extension quota abandoned contributes nothing here. That
        /// needs the quota to bind, which happens only under truncated labeling or memory pressure,
        /// never at the defaults. See the note at the quota check in
        /// @c BidirectionalDominanceAlgorithm::step for why it is left open and what the fix would
        /// be.
        ///
        /// **On pruning.** Two different filters, deliberately separated. The caller's fixed
        /// @p cost_upper_bound is unconditional -- it drops exactly what the caller said to drop.
        /// The incumbent cutoff, "no better than something we already have", follows
        /// @p prune_requested *when a finite @p cost_upper_bound says the caller is really
        /// filtering*, and otherwise stays on. See the body for what that second clause is worth.
        ///
        /// `stop_after_X_solutions` deliberately does NOT truncate the join: the join runs to
        /// completion and `Algorithm::solve` resizes the result afterwards, so a `COMPLETE` status
        /// still means the search was exhaustive.
        ///
        /// Either bound cuts pairs off, never halves. A half's own cost says nothing about what the
        /// completed path costs once reduced costs go negative, which is the ordinary case in
        /// pricing. The one admissible half-level test is a half plus the cheapest completion
        /// available at the node, and that is what the code does.
        ///
        /// @tparam FwdContainer Per-node forward label container.
        /// @tparam BwdContainer Per-node backward label container.
        /// @tparam OnSolution   Callable `(double cost, std::vector<size_t> arc_ids,
        ///                      size_t end_node_id)`.
        /// @param graph                   The model.
        /// @param forward_by_pos          Forward labels, indexed by node position.
        /// @param backward_by_pos         Backward labels, indexed by node position.
        /// @param half_way                The half-way policy; consulted only when enabled.
        /// @param critical_resource_index Index of the critical resource within its type slot.
        /// @param best_cost_upper_bound   Tightened in place as solutions are found. Consulted as a
        ///                                cutoff only when the incumbent cutoff is in force -- see
        ///                                @p prune_requested and the note in the body.
        /// @param prune_requested         `params_.prune_based_on_upper_bound_`, the caller's
        ///                                explicit request to drop a pair that is no better than
        ///                                the incumbent. It is off by default: a pricing pool wants
        ///                                every negative-reduced-cost column, not only the best
        ///                                one, and no other algorithm in this library drops a
        ///                                solution for being merely not-the-best.
        /// @param cost_upper_bound        The caller's fixed bound. Always applied.
        /// @param on_solution             Invoked once per accepted join.
        template <typename FwdContainer, typename BwdContainer, typename OnSolution>
        void join(const Graph<ResourceType>& graph, std::vector<FwdContainer>& forward_by_pos,
                  std::vector<BwdContainer>& backward_by_pos, const HalfWayPolicy& half_way,
                  size_t critical_resource_index, double& best_cost_upper_bound,
                  bool prune_requested, double cost_upper_bound, OnSolution&& on_solution) {
            // The incumbent cutoff applies when the caller asked for it, and ALSO whenever they
            // supplied no fixed bound at all.
            //
            // The second clause is not a convenience. Gating this cutoff on the request alone --
            // which is what B2 first did -- makes a default solve return every admissible pair
            // rather than the improving ones, and on a large instance that is not a few extra
            // columns. Measured on the full C201 with the arc-based join this replaced: 3 064 862
            // joins and 3 065 288 solutions, 12.23 s against the forward search's 0.68 s, while the
            // bidirectional SEARCH extended 1.46x FEWER labels than forward. On RC201 the same run
            // reached 9.3 GB resident and had to be killed. None of that cost is search; it is
            // recording and de-duplicating the set.
            //
            // An infinite `cost_upper_bound` means the caller expressed no filter -- a benchmark,
            // an equivalence check, an exploratory solve -- and there "every path" is not the
            // question anybody asked. A FINITE bound is the case B2 exists for: a pricing pool
            // saying "every column below this", which wants all of them and not only the best. So
            // the flag is honoured exactly where it means something.
            const bool prune_against_incumbent =
                prune_requested || !std::isfinite(cost_upper_bound);
            // Whether any component's merge rule says a refusal is worth verifying. Read once from
            // one node: the flag is a property of the function objects, which every node clones
            // from the same prototype, not of the node.
            const bool verify_refusals = any_merge_refusal_may_be_conservative(graph);

            for (const size_t node_id : graph.get_node_ids()) {
                const auto* node = graph.get_node(node_id);
                if (node == nullptr) {
                    continue;
                }
                const auto& backward_labels = backward_by_pos.at(node->pos());
                if (backward_labels.get_labels().empty()) {
                    continue;
                }
                const auto boundary = boundary_labels_by_cost(forward_by_pos.at(node->pos()),
                                                              half_way,
                                                              critical_resource_index);
                if (boundary.empty()) {
                    continue;
                }
                // No container provides a cost order, and without one the early exits below
                // silently discard valid joins. Nothing to memoise: a node is visited once, not
                // once per incident arc.
                const auto backward_sorted = sorted_by_cost(backward_labels);
                const double cheapest_backward = backward_sorted.front()->get_cost();

                for (const auto* forward : boundary) {
                    // `break`, not `continue`: the join arc is already inside `forward`'s own
                    // cost, so the list really is ordered by the quantity being tested and
                    // everything after this point is worse. (Pairing across an arc instead would
                    // have to compare the cost AFTER crossing it, which an extension function may
                    // clamp, and there the list order says nothing.)
                    const double cheapest_completion = forward->get_cost() + cheapest_backward;
                    if (cheapest_completion >= cost_upper_bound ||
                        (prune_against_incumbent && cheapest_completion >= best_cost_upper_bound)) {
                        break;
                    }

                    for (const auto* backward : backward_sorted) {
                        const double cost = forward->get_cost() + backward->get_cost();
                        // Forbidden arcs carry an infinite cost; without this, inf + finite would
                        // become a "solution". Mirrors main_loop's own isinf guard.
                        if (std::isinf(cost)) {
                            continue;
                        }
                        // Cost-sorted, so once one pair is out of range every later one is too.
                        if (cost >= cost_upper_bound ||
                            (prune_against_incumbent && cost >= best_cost_upper_bound)) {
                            break;
                        }
                        if (!forward->get_resource().can_be_merged(backward->get_resource())) {
                            // A rule that cannot decide exactly from two values may refuse a
                            // feasible pair, and only a rule that says so gets its refusals
                            // replayed.
                            if (!verify_refusals ||
                                !replays_feasibly(graph, merged_path(graph, *forward, *backward))) {
                                continue;
                            }
                        }

                        auto arc_ids = merged_path(graph, *forward, *backward);
                        on_solution(cost, std::move(arc_ids), terminal_node_id(*backward));
                        best_cost_upper_bound = std::min(best_cost_upper_bound, cost);
                    }
                }
            }
        }

    private:
        /// @brief The forward labels at a node that crossed `H` on their own last arc, cheapest
        ///        first.
        ///
        /// A boundary label is one the forward search created and then declined to extend. The
        /// predicate is `clock > H` plus "is not a seed"; see @ref join for why no second
        /// comparison against the predecessor is needed.
        ///
        /// With the bound disabled there is no `H` to be past, so every non-seed label qualifies.
        ///
        /// @tparam FwdContainer Per-node forward label container.
        /// @param container               The node's forward labels.
        /// @param half_way                The half-way policy.
        /// @param critical_resource_index Index of the critical resource within its type slot.
        /// @return The boundary labels, sorted by cost.
        template <typename FwdContainer>
        [[nodiscard]] static std::vector<Label<ResourceType>*> boundary_labels_by_cost(
            const FwdContainer& container, const HalfWayPolicy& half_way,
            size_t critical_resource_index) {
            std::vector<Label<ResourceType>*> labels;
            for (auto* label : container.get_labels()) {
                // A seed is not a crossing: nothing was extended to produce it. This is also what
                // keeps a source out of the forward half's interior, since the forward search
                // never extends INTO a seed -- so a forward label at a source is always one.
                if (label->prev_label == nullptr || label->get_in_arc() == nullptr) {
                    continue;
                }
                assert(!label->get_in_arc()->origin->sink &&
                       "the forward search must never extend out of a sink");
                if (half_way.enabled() &&
                    critical_value(label->get_resource(), critical_resource_index) <=
                        half_way.h()) {
                    continue;
                }
                labels.push_back(label);
            }
            std::ranges::sort(labels, [](const auto* lhs, const auto* rhs) {
                return lhs->get_cost() < rhs->get_cost();
            });
            return labels;
        }

        /// @brief A node's labels, cheapest first.
        ///
        /// No container provides a cost order: LabelList appends, and LabelBuckets orders by its
        /// bucket resource across buckets and its sort resource within one -- which is not a global
        /// cost order, and not cost at all unless the sort resource happens to be cost. Without it
        /// the early exits above silently discard valid joins.
        ///
        /// @tparam Container The per-node label container.
        /// @param container The node's labels.
        /// @return Pointers to them, sorted by cost.
        template <typename Container>
        [[nodiscard]] static std::vector<Label<ResourceType>*> sorted_by_cost(
            const Container& container) {
            std::vector<Label<ResourceType>*> labels(container.get_labels().begin(),
                                                      container.get_labels().end());
            std::ranges::sort(labels, [](const auto* lhs, const auto* rhs) {
                return lhs->get_cost() < rhs->get_cost();
            });
            return labels;
        }

        /// @brief Splices a boundary label's chain and a backward chain into one arc sequence.
        ///
        /// **The join arc is already inside @p forward's chain** -- it is that label's own
        /// `in_arc` -- which is the easiest thing here to get wrong: pushing the arc again would
        /// duplicate it, and walking from a freshly extended label instead would lose the prefix.
        ///
        /// The two chains are walked differently. A forward chain's `prev_label` runs back towards
        /// the source, so its arcs are collected and then reversed; a backward chain's runs
        /// *forwards* towards the sink, so its arcs come out already in order.
        ///
        /// @tparam GraphType The graph type.
        /// @param graph    The graph the arc ids belong to.
        /// @param forward  The boundary label; its chain ends with the join arc.
        /// @param backward The backward half sitting at the same node.
        /// @return The merged path, in traversal order.
        template <typename GraphType>
        [[nodiscard]] static std::vector<size_t> merged_path(const GraphType& graph,
                                                             const Label<ResourceType>& forward,
                                                             const Label<ResourceType>& backward) {
            std::vector<size_t> arc_ids;
            for (const Label<ResourceType>* current = &forward;
                 current != nullptr && current->get_in_arc() != nullptr;
                 current = current->prev_label) {
                arc_ids.push_back(current->get_in_arc()->id);
            }
            std::ranges::reverse(arc_ids);

            // Already in forward order; see above for why the two chains are walked
            // differently.
            for (const Label<ResourceType>* current = &backward;
                 current != nullptr && current->get_out_arc() != nullptr;
                 current = current->prev_label) {
                arc_ids.push_back(current->get_out_arc()->id);
            }

            assert(is_contiguous(graph, arc_ids) && "merged path is not contiguous");
            return arc_ids;
        }

        /// @brief Whether any component asked for its merge refusals to be verified.
        ///
        /// @param graph The model.
        /// @return @c true when at least one component declares a conservative refusal.
        template <typename GraphType>
        [[nodiscard]] static bool any_merge_refusal_may_be_conservative(const GraphType& graph) {
            const auto node_ids = graph.get_node_ids();
            if (node_ids.empty()) {
                return false;
            }
            const auto* node = graph.get_node(node_ids.front());
            return node != nullptr && node->resource != nullptr &&
                   node->resource->merge_refusal_may_be_conservative();
        }

        /// @brief Whether an arc sequence replays feasibly through the arcs' own extenders.
        ///
        /// The ground truth a conservative merge rule is checked against: extend a fresh source
        /// resource arc by arc exactly as the forward search would, and require every node to be
        /// feasible. Costs one resource copy and one extension per arc, which is why only a rule
        /// that asks for it gets it.
        ///
        /// @param graph   The graph the arc ids belong to.
        /// @param arc_ids The merged path, in traversal order.
        /// @return @c true when every node of the path is feasible.
        template <typename GraphType>
        [[nodiscard]] static bool replays_feasibly(const GraphType& graph,
                                                   const std::vector<size_t>& arc_ids) {
            if (arc_ids.empty()) {
                return false;
            }
            const auto* first = graph.get_arc(arc_ids.front());
            auto current = std::make_unique<Resource<ResourceType>>(*first->origin->resource);
            for (const size_t arc_id : arc_ids) {
                const auto* arc = graph.get_arc(arc_id);
                auto extended =
                    std::make_unique<Resource<ResourceType>>(*arc->destination->resource);
                arc->extender->extend(*current, extended.get());
                if (!extended->is_feasible()) {
                    return false;
                }
                current = std::move(extended);
            }
            return true;
        }

        /// @brief Reads the critical resource's scalar value out of a composed resource.
        ///
        /// Guarded, because `get_component<T>` resolves a *constrained* index trait that does not
        /// exist when `T` is absent from the pack -- and this function is instantiated
        /// unconditionally, so without the guard a critical type outside the pack is a compile
        /// error and the algorithm's three "type not in the model, disabling the bound" branches
        /// can never run. With it they can, and 0.0 is the right answer on that path anyway: the
        /// bound is disabled, so no crossing test consults this.
        [[nodiscard]] static double critical_value(const Resource<ResourceType>& resource,
                                                   size_t critical_resource_index) {
            if constexpr (is_cost_in_composition_v<CriticalRC, ResourceType>) {
                const auto& component =
                    resource.template get_component<CriticalRC>(critical_resource_index);
                return static_cast<double>(component.get_value().get_value());
            } else {
                return 0.0;
            }
        }

        /// @brief The node a merged path ends at: where the backward chain runs out, i.e. a sink.
        [[nodiscard]] static size_t terminal_node_id(const Label<ResourceType>& backward) {
            const Label<ResourceType>* current = &backward;
            while (current->prev_label != nullptr) {
                current = current->prev_label;
            }
            return current->get_end_node()->id;
        }

    public:
        /// @brief Whether consecutive arcs actually connect end to end.
        ///
        /// A predicate rather than a bare assertion so the "fires when violated" half can be tested
        /// directly, without a death test: an aborting assert is awkward to exercise portably, and
        /// a check nobody has seen reject anything is not much of a check.
        ///
        /// Asserted on every reconstruction in debug builds. O(path length), and it turns a
        /// reconstruction mistake from a silently dropped path -- `extract_solution` returns early
        /// on an empty arc list -- into a loud failure.
        ///
        /// @param graph   The graph the arc ids belong to.
        /// @param arc_ids The merged arc sequence, in path order.
        /// @return `true` when each arc's destination is the next arc's origin.
        template <typename GraphType>
        [[nodiscard]] static bool is_contiguous(const GraphType& graph,
                                                const std::vector<size_t>& arc_ids) {
            for (size_t i = 0; i + 1 < arc_ids.size(); ++i) {
                if (graph.get_arc(arc_ids[i])->destination !=
                    graph.get_arc(arc_ids[i + 1])->origin) {
                    return false;
                }
            }
            return true;
        }
};

}  // namespace rcspp
