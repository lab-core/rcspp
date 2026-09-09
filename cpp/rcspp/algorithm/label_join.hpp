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
        /// @param best_cost_upper_bound   Tightened in place as solutions are found, which is what
        ///                                makes the early exits bite.
        /// @param on_solution             Invoked once per accepted join.
        template <typename FwdContainer, typename BwdContainer, typename OnSolution>
        void join(const Graph<ResourceType>& graph, std::vector<FwdContainer>& forward_by_pos,
                  std::vector<BwdContainer>& backward_by_pos, LabelPool<ResourceType>& pool,
                  const HalfWayPolicy& half_way, size_t critical_resource_index,
                  double& best_cost_upper_bound, OnSolution&& on_solution) {
            // No container provides a cost order: LabelList appends, and LabelBuckets orders by its
            // bucket resource across buckets and its sort resource within one -- which is not a
            // global cost order, and not cost at all unless the sort resource happens to be cost.
            // Sorting once per node is O(n log n) against an O(n^2) pairing, and without it the
            // early exits below silently discard valid joins.
            auto sorted_by_cost = [](const auto& container) {
                std::vector<Label<ResourceType>*> labels(container.get_labels().begin(),
                                                         container.get_labels().end());
                std::ranges::sort(labels, [](const auto* lhs, const auto* rhs) {
                    return lhs->get_cost() < rhs->get_cost();
                });
                return labels;
            };

            graph.for_each_arc([&](const auto& arc) {
                const auto& forward_labels = forward_by_pos.at(arc.origin->pos());
                const auto& backward_labels = backward_by_pos.at(arc.destination->pos());
                if (forward_labels.get_labels().empty() || backward_labels.get_labels().empty()) {
                    return;
                }

                const auto forward_sorted = sorted_by_cost(forward_labels);
                const auto backward_sorted = sorted_by_cost(backward_labels);

                for (auto* forward : forward_sorted) {
                    // Sorted by cost, so once one label is too expensive every later one is too.
                    if (forward->get_cost() >= best_cost_upper_bound) {
                        break;
                    }
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

                    for (const auto* backward : backward_sorted) {
                        const double cost = extended.get_cost() + backward->get_cost();
                        // Forbidden arcs carry an infinite cost; without this, inf + finite would
                        // become a "solution". Mirrors main_loop's own isinf guard.
                        if (std::isinf(cost)) {
                            continue;
                        }
                        if (cost >= best_cost_upper_bound) {
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
        [[nodiscard]] static double critical_value(const Resource<ResourceType>& resource,
                                                   size_t critical_resource_index) {
            const auto& component =
                resource.template get_component<CriticalRC>(critical_resource_index);
            return static_cast<double>(component.get_value().get_value());
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
