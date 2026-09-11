// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

#include "rcspp/algorithm/half_way_policy.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp {

/// @brief Pairs surviving forward and backward labels across each arc to form complete paths.
///
/// A forward label `f` sits at `u` and a backward label `b` at `v`, with an arc `(u, v)` between
/// them. Three things have to happen in a specific order:
///
///  1. **Extend before testing.** `f` and `b` sit at *different nodes*, so their numbers are not
///     comparable until `f` is moved across the arc to `v`. Extending applies the arc's consumption
///     to every resource, and the ordinary feasibility check at `v` then catches "`v` is already in
///     the forward half's visited set" with no special-casing.
///  2. **The crossing test**, whose two sides are both *forward* values -- see @ref join.
///  3. **The merge test**, per resource, via `can_be_merged`.
///
/// Cost is summed rather than compared: `merged = f'.get_cost() + b.get_cost()`. The join arc is
/// counted exactly once, by `f'`, because a backward label sitting *at* `v` has consumed no arc
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
        /// @brief Pairs every forward label with every backward label across each arc.
        ///
        /// The crossing rule is
        /// @code f.critical(u) <= H < f'.critical(v) @endcode
        /// where `f'` is `f` after extending across the arc. **Both sides are forward values.**
        /// Because the critical resource is monotone its value along a complete path only rises and
        /// straddles `H`, so it crosses `H` on exactly one arc -- which is what makes each complete
        /// path appear exactly once.
        ///
        /// A form of this rule that reads `b.critical(v)` on the right is wrong and filters
        /// nothing: the backward search already keeps only labels at or above `H`, and a surviving
        /// forward label is already at or below it, so that version holds for *every* surviving
        /// pair. The result is a duplicate-join explosion -- correct answers, exploding work,
        /// presenting as a dominance bug.
        ///
        /// When the half-way bound is disabled the crossing test is skipped and every pair is
        /// joined. That is still correct, just slower; `Solution`'s hash absorbs the duplicates.
        ///
        /// **On pruning.** Two different filters, deliberately separated. The caller's fixed
        /// `cost_upper_bound` is unconditional -- it drops exactly what the caller said to drop --
        /// while `best_cost_upper_bound`, "no better than something we already have", follows
        /// `prune_based_on_upper_bound_`, which is off by default. A pricing pool wants every
        /// negative-reduced-cost column rather than only the best one, and no other algorithm here
        /// drops a solution for being merely not-the-best.
        ///
        /// `stop_after_X_solutions` deliberately does NOT truncate the join: the join runs to
        /// completion and `Algorithm::solve` resizes the result afterwards, so a `COMPLETE` status
        /// still means the search was exhaustive.
        ///
        /// Either bound cuts pairs off, never halves. A half's own cost
        /// says nothing about what the completed path costs once reduced costs go negative, which
        /// is the ordinary case in pricing. The one admissible half-level test is a half plus the
        /// cheapest completion available across the arc, and that is what the code does.
        ///
        /// @tparam FwdContainer Per-node forward label container.
        /// @tparam BwdContainer Per-node backward label container.
        /// @tparam OnSolution   Callable `(double cost, std::vector<size_t> arc_ids,
        ///                      size_t end_node_id)`.
        /// @param graph                   The graph whose arcs are candidate join arcs.
        /// @param forward_by_pos          Forward labels, indexed by node position.
        /// @param backward_by_pos         Backward labels, indexed by node position.
        /// @param pool                    Pool the extended label `f'` is drawn from.
        /// @param half_way                The half-way policy; consulted only when enabled.
        /// @param critical_resource_index Index of the critical resource within its type slot.
        /// @param best_cost_upper_bound   Tightened in place as solutions are found. Consulted as a
        ///                                cutoff only when @p prune_against_incumbent is set.
        /// @param prune_against_incumbent Whether a pair that is no better than the incumbent may
        ///                                be dropped. This is `params_.prune_based_on_upper_bound_`,
        ///                                and it is off by default: a pricing pool wants every
        ///                                negative-reduced-cost column, not only the best one, and
        ///                                no other algorithm in this library drops a solution for
        ///                                being merely not-the-best.
        ///
        ///                                The *caller's* bound, @p cost_upper_bound, is a different
        ///                                filter and stays unconditional -- it drops exactly what
        ///                                the caller said to drop, and because both label lists are
        ///                                cost-sorted it still gives an early exit whenever that
        ///                                bound is finite.
        /// @param cost_upper_bound        The caller's fixed bound. Always applied.
        /// @param on_solution             Invoked once per accepted join.
        template <typename FwdContainer, typename BwdContainer, typename OnSolution>
        void join(const Graph<ResourceType>& graph, std::vector<FwdContainer>& forward_by_pos,
                  std::vector<BwdContainer>& backward_by_pos, LabelPool<ResourceType>& pool,
                  const HalfWayPolicy& half_way, size_t critical_resource_index,
                  double& best_cost_upper_bound, bool prune_against_incumbent,
                  double cost_upper_bound, OnSolution&& on_solution) {
            // No container provides a cost order: LabelList appends, and LabelBuckets orders by its
            // bucket resource across buckets and its sort resource within one -- which is not a
            // global cost order, and not cost at all unless the sort resource happens to be cost.
            // Without it the early exits below silently discard valid joins. Memoised per node
            // below, because the pairing walks arcs and a node is the origin of many of them.
            auto sorted_by_cost = [](const auto& container) {
                std::vector<Label<ResourceType>*> labels(container.get_labels().begin(),
                                                         container.get_labels().end());
                std::ranges::sort(labels, [](const auto* lhs, const auto* rhs) {
                    return lhs->get_cost() < rhs->get_cost();
                });
                return labels;
            };

            // Sorted ONCE PER NODE, not once per arc. A node with out-degree d was previously
            // copied into a fresh vector and sorted d times; on a dense graph that is tens of
            // thousands of redundant sorts and allocations per solve, and it is a fixed cost the
            // join pays whether or not it produces anything.
            //
            // Caching pointers is safe because nothing in this pass mutates a container: the pool
            // is pointer-stable, `release_label` only touches a free list, and no label's cost
            // changes.
            //
            // `std::vector<char>` rather than `std::vector<bool>`, and a separate "done" flag
            // rather than testing `.empty()`: a node can legitimately hold zero labels, which is
            // not the same as an uncomputed entry.
            std::vector<std::vector<Label<ResourceType>*>> forward_sorted_by_pos(
                forward_by_pos.size());
            std::vector<std::vector<Label<ResourceType>*>> backward_sorted_by_pos(
                backward_by_pos.size());
            std::vector<char> forward_sorted_done(forward_by_pos.size(), 0);
            std::vector<char> backward_sorted_done(backward_by_pos.size(), 0);

            graph.for_each_arc([&](const auto& arc) {
                // A forward label at a sink is already a complete path and a backward label at a
                // source is too; splicing either would put a terminal strictly inside the result.
                // Neither search traverses such an arc -- it records the label and stops -- so the
                // join must not do it either, or the two disagree about what a path is.
                if (arc.origin->sink || arc.destination->source) {
                    return;
                }
                const auto& forward_labels = forward_by_pos.at(arc.origin->pos());
                const auto& backward_labels = backward_by_pos.at(arc.destination->pos());
                if (forward_labels.get_labels().empty() || backward_labels.get_labels().empty()) {
                    return;
                }

                const size_t origin_pos = arc.origin->pos();
                const size_t destination_pos = arc.destination->pos();
                if (forward_sorted_done[origin_pos] == 0) {
                    forward_sorted_by_pos[origin_pos] = sorted_by_cost(forward_labels);
                    forward_sorted_done[origin_pos] = 1;
                }
                if (backward_sorted_done[destination_pos] == 0) {
                    backward_sorted_by_pos[destination_pos] = sorted_by_cost(backward_labels);
                    backward_sorted_done[destination_pos] = 1;
                }
                const auto& forward_sorted = forward_sorted_by_pos[origin_pos];
                const auto& backward_sorted = backward_sorted_by_pos[destination_pos];

                // The cheapest completion available through this arc. Sorted by cost, so it is
                // the first entry.
                const double cheapest_backward = backward_sorted.front()->get_cost();

                for (auto* forward : forward_sorted) {
                    const double critical_at_origin =
                        critical_value(forward->get_resource(), critical_resource_index);
                    if (half_way.enabled() && critical_at_origin > half_way.h()) {
                        continue;
                    }

                    // f' is pooled and never enters a non-dominated set, so it pins no predecessor
                    // and must be returned with release_label rather than release_with_ref_count.
                    auto& extended = pool.get_next_label(arc.destination);
                    forward->extend(arc, &extended);

                    // The crossing test first: a scalar comparison, where feasibility walks every
                    // component.
                    if (half_way.enabled()) {
                        const double critical_at_destination =
                            critical_value(extended.get_resource(), critical_resource_index);
                        if (!(critical_at_origin <= half_way.h() &&
                              half_way.h() < critical_at_destination)) {
                            pool.release_label(&extended);
                            continue;
                        }
                    }

                    if (!extended.is_feasible()) {
                        pool.release_label(&extended);
                        continue;
                    }

                    // A HALF is never compared against the incumbent on its own. This half plus
                    // the cheapest completion available through this arc is the best any pair
                    // starting here can do, so rejecting on that is admissible; rejecting on the
                    // half's own cost is not. Under reduced costs a half costing 30 completes to
                    // -20, and the version of this test that read `forward->get_cost()` alone lost
                    // the optimum of the repository's own VRPTW pricing instance -- returning
                    // -190.46 against the true -319.88, faster, and reporting COMPLETE.
                    //
                    // It is a `continue` rather than a `break` even though the list is cost-sorted:
                    // the quantity compared is the cost AFTER crossing the arc, and the arc's own
                    // contribution is not guaranteed to be the same constant for every label -- an
                    // extension function may clamp. Cheap to be wrong about, expensive to assume.
                    const double cheapest_completion = extended.get_cost() + cheapest_backward;
                    if (cheapest_completion >= cost_upper_bound ||
                        (prune_against_incumbent && cheapest_completion >= best_cost_upper_bound)) {
                        pool.release_label(&extended);
                        continue;
                    }

                    for (const auto* backward : backward_sorted) {
                        const double cost = extended.get_cost() + backward->get_cost();
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
                        if (!extended.get_resource().can_be_merged(backward->get_resource())) {
                            continue;
                        }

                        auto arc_ids = merged_path(graph, *forward, arc, *backward);
                        on_solution(cost, std::move(arc_ids), terminal_node_id(*backward));
                        best_cost_upper_bound = std::min(best_cost_upper_bound, cost);
                    }

                    pool.release_label(&extended);
                }
            });
        }

    private:
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

        /// @brief Splices the two chains and the join arc into one arc sequence, in path order.
        ///
        /// The forward chain walks `prev_label` / `get_in_arc()` towards the source, so it comes
        /// out reversed and has to be turned around. The backward chain is its mirror: each
        /// backward label's `prev_label` is the label nearer the *sink* and the arc it remembers is
        /// `get_out_arc()`, so walking it yields arcs **already in forward order**, with no
        /// reversal. That asymmetry is the single easiest thing here to get backwards, which is why
        /// it is asserted below rather than merely reasoned about.
        template <typename GraphType>
        [[nodiscard]] static std::vector<size_t> merged_path(const GraphType& graph,
                                                             const Label<ResourceType>& forward,
                                                             const Arc<ResourceType>& join_arc,
                                                             const Label<ResourceType>& backward) {
            std::vector<size_t> arc_ids;

            // Forward half, walked from the ORIGINAL label rather than from the extended one. f'
            // is a bare pooled label: extend() sets its in-arc but nothing sets its prev_label, so
            // its chain stops immediately and the whole forward half would be lost.
            for (const Label<ResourceType>* current = &forward;
                 current != nullptr && current->get_in_arc() != nullptr;
                 current = current->prev_label) {
                arc_ids.push_back(current->get_in_arc()->id);
            }
            std::ranges::reverse(arc_ids);

            arc_ids.push_back(join_arc.id);

            // Backward half, already in forward order.
            for (const Label<ResourceType>* current = &backward;
                 current != nullptr && current->get_out_arc() != nullptr;
                 current = current->prev_label) {
                arc_ids.push_back(current->get_out_arc()->id);
            }

            assert(is_contiguous(graph, arc_ids) && "merged path is not contiguous");
            return arc_ids;
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

    private:
};

}  // namespace rcspp
