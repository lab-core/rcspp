// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/half_way_policy.hpp"
#include "rcspp/algorithm/solution.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp {

/// @brief What one join pass may pair, keep and spend.
///
/// A @ref max_solutions at or above @c MAX_INT (the params' "no limit") means no cap.
struct JoinOptions {
        /// @brief The half-way policy; consulted only when enabled. Off by default: every stored
        ///        non-start forward label is then paired.
        HalfWayPolicy half_way{0.0, std::numeric_limits<double>::infinity()};

        /// @brief Index of the critical resource within its type slot.
        size_t critical_resource_index = 0;

        /// @brief Whether to drop pairs costing at least the incumbent (the cheapest complete
        ///        path found so far). The join prunes against the incumbent anyway when
        ///        @ref cost_upper_bound is infinite.
        bool prune_requested = true;

        /// @brief The caller's fixed bound. Always applied.
        double cost_upper_bound = std::numeric_limits<double>::infinity();

        /// @brief When below @c MAX_INT, keep only this many cheapest distinct joined paths and
        ///        hand them on at the end of the pass. A pair too expensive to be among them is
        ///        skipped before its path is built.
        size_t max_solutions = MAX_INT;

        /// @brief When set, the pass stops after this many join-rule questions and reports
        ///        `truncated`; what it had kept is still handed on.
        size_t max_pairs = MAX_INT;
};

/// @brief What one join pass did.
struct JoinStats {
        /// @brief Pairs the join rule was asked about: one per `can_be_joined` call.
        size_t pairs_tested = 0;
        /// @brief Whether the pass stopped at its pair budget with an in-range pair still
        ///        untested.
        bool truncated = false;
};

/// @brief Pairs surviving forward and backward labels at the node they meet on to form complete
///        paths.
///
/// Forward **boundary labels** -- labels whose own last arc crossed `H`, so they were stored but
/// never extended -- are paired with the backward labels at the same node. Each complete path
/// crosses `H` on exactly one arc, so it is produced once. Feasibility needs no test (infeasible
/// labels are never stored); each pair is checked with `can_be_joined`, and its cost is the sum of
/// the two halves (the join arc is inside the forward half). Runs after both searches finish.
///
/// @tparam ResourceType The resource type carried by labels.
/// @tparam CriticalRC   The critical resource's type -- the clock the half-way rule reads.
template <typename ResourceType, typename CriticalRC>
class Joiner {
    public:
        /// @brief Pairs each node's boundary forward labels with the backward labels sitting
        ///        there.
        ///
        /// With the bound disabled, every stored non-start forward label is paired. Forward labels
        /// abandoned by the per-node extension quota contribute nothing. Cost bounds are tested on
        /// a pair's total cost, or on a forward label plus the cheapest backward label at the
        /// node; never on one half's cost alone, which is not a lower bound under reduced costs.
        ///
        /// @tparam FwdContainer Per-node forward label container.
        /// @tparam BwdContainer Per-node backward label container.
        /// @tparam OnSolution   Callable `(double cost, std::vector<size_t> arc_ids,
        ///                      size_t end_node_id)`.
        /// @param graph                 The model.
        /// @param forward_by_pos        Forward labels, indexed by node position.
        /// @param backward_by_pos       Backward labels, indexed by node position.
        /// @param options               What the pass may pair, keep and spend.
        /// @param best_cost_upper_bound The cost of the cheapest complete path found so far;
        ///                              lowered in place as cheaper joins are found.
        /// @param on_solution           Invoked once per accepted join.
        /// @return How many pairs were tested, and whether the pair budget cut the pass short.
        template <typename FwdContainer, typename BwdContainer, typename OnSolution>
        JoinStats join(const Graph<ResourceType>& graph, std::vector<FwdContainer>& forward_by_pos,
                       std::vector<BwdContainer>& backward_by_pos, const JoinOptions& options,
                       double& best_cost_upper_bound, OnSolution&& on_solution) {
            JoinStats stats;
            const double cost_upper_bound = options.cost_upper_bound;
            // Prune against the incumbent when asked, or when there is no fixed bound: otherwise a
            // default solve would return every admissible pair. A finite bound with no request
            // means "every column below this bound".
            const bool prune_against_incumbent =
                options.prune_requested || !std::isfinite(cost_upper_bound);
            const bool capped = options.max_solutions < MAX_INT;
            const size_t max_pairs = options.max_pairs < MAX_INT
                                         ? options.max_pairs
                                         : std::numeric_limits<size_t>::max();
            Cheapest cheapest(options.max_solutions);
            // A budget of zero keeps nothing; stop before `out_of_range` reads an empty heap.
            if (capped && options.max_solutions == 0) {
                return stats;
            }
            // A pair at or above this cannot be among the cheapest kept, so is not worth splicing.
            const auto out_of_range = [&](double cost) {
                return cost >= cost_upper_bound ||
                       (prune_against_incumbent && cost >= best_cost_upper_bound) ||
                       (capped && cheapest.full() && cost >= cheapest.worst());
            };

            for (const size_t node_id : graph.get_node_ids()) {
                if (stats.truncated) {
                    break;
                }
                const auto* node = graph.get_node(node_id);
                // Paths ending at a sink were already recorded by the forward search.
                if (node == nullptr || node->sink) {
                    continue;
                }
                const auto& backward_labels = backward_by_pos.at(node->pos());
                if (backward_labels.get_labels().empty()) {
                    continue;
                }
                const auto boundary = boundary_labels_by_cost(forward_by_pos.at(node->pos()),
                                                              options.half_way,
                                                              options.critical_resource_index);
                if (boundary.empty()) {
                    continue;
                }
                // The early exits below require cost order.
                const auto backward_sorted = sorted_by_cost(backward_labels);
                const double cheapest_backward = backward_sorted.front()->get_cost();

                for (const auto* forward : boundary) {
                    if (stats.truncated) {
                        break;
                    }
                    // Boundary labels are cost-sorted, so every later one is worse too.
                    if (out_of_range(forward->get_cost() + cheapest_backward)) {
                        break;
                    }

                    for (const auto* backward : backward_sorted) {
                        const double cost = forward->get_cost() + backward->get_cost();
                        // Forbidden arcs carry an infinite cost.
                        if (std::isinf(cost)) {
                            continue;
                        }
                        // Cost-sorted, so once one pair is out of range every later one is too.
                        if (out_of_range(cost)) {
                            break;
                        }
                        // The budget binds only on a pair that would really be tested, so
                        // `truncated` means "an in-range pair was left", never "the budget was
                        // exactly used up".
                        if (stats.pairs_tested >= max_pairs) {
                            stats.truncated = true;
                            break;
                        }
                        ++stats.pairs_tested;
                        // can_be_joined is an exact test: a refused pair cannot form a feasible
                        // path.
                        if (!forward->get_resource().can_be_joined(backward->get_resource())) {
                            continue;
                        }

                        auto arc_ids = joined_path(graph, *forward, *backward);
                        if (capped) {
                            cheapest.offer(cost, std::move(arc_ids), terminal_node_id(*backward));
                        } else {
                            on_solution(cost, std::move(arc_ids), terminal_node_id(*backward));
                        }
                        best_cost_upper_bound = std::min(best_cost_upper_bound, cost);
                    }
                }
            }

            for (auto& path : cheapest.take()) {
                on_solution(path.cost, std::move(path.arc_ids), path.end_node_id);
            }
            return stats;
        }

    private:
        /// @brief The cheapest @c capacity distinct joined paths seen so far.
        ///
        /// A max-heap on cost plus the set of paths it holds, so a path joined at several nodes
        /// takes one slot.
        class Cheapest {
            public:
                /// @brief One joined path.
                struct Path {
                        double cost;
                        std::vector<size_t> arc_ids;
                        size_t end_node_id;
                };

                explicit Cheapest(size_t capacity) : capacity_(capacity) {}

                /// @brief Whether the heap holds @c capacity paths.
                [[nodiscard]] bool full() const { return heap_.size() >= capacity_; }

                /// @brief The most expensive path kept; only meaningful when @ref full.
                [[nodiscard]] double worst() const { return heap_.front().cost; }

                /// @brief Keeps a path if it is new and among the cheapest.
                void offer(double cost, std::vector<size_t> arc_ids, size_t end_node_id) {
                    // `join` never offers to a zero capacity: it returns before the pass.
                    if (kept_.contains(arc_ids)) {
                        return;
                    }
                    if (full()) {
                        if (cost >= worst()) {
                            return;
                        }
                        std::ranges::pop_heap(heap_, std::less{}, &Path::cost);
                        kept_.erase(heap_.back().arc_ids);
                        heap_.pop_back();
                    }
                    kept_.insert(arc_ids);
                    heap_.push_back({cost, std::move(arc_ids), end_node_id});
                    std::ranges::push_heap(heap_, std::less{}, &Path::cost);
                }

                /// @brief The kept paths, cheapest first; empties the heap.
                [[nodiscard]] std::vector<Path> take() {
                    std::ranges::sort_heap(heap_, std::less{}, &Path::cost);
                    kept_.clear();
                    return std::move(heap_);
                }

            private:
                /// @brief FNV-1a over the arc ids, as @c Solution hashes them.
                struct PathHash {
                        size_t operator()(const std::vector<size_t>& arc_ids) const noexcept {
                            std::uint64_t hash = FNV_OFFSET_BASIS;
                            for (const size_t arc_id : arc_ids) {
                                hash = fnv1a_mix_uint64(static_cast<std::uint64_t>(arc_id), hash);
                            }
                            return static_cast<size_t>(hash);
                        }
                };

                size_t capacity_;
                std::vector<Path> heap_;
                std::unordered_set<std::vector<size_t>, PathHash> kept_;
        };

        /// @brief The forward labels at a node that crossed `H` on their own last arc, cheapest
        ///        first.
        ///
        /// A boundary label is a non-start label with `clock > H`; with the bound disabled, every
        /// non-start label qualifies.
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
                // A start label is not a crossing.
                if (label->prev_label == nullptr || label->get_in_arc() == nullptr) {
                    continue;
                }
                assert(!label->get_in_arc()->origin->sink &&
                       "the forward search must never extend out of a sink");
                if (half_way.enabled() &&
                    critical_value<CriticalRC>(label->get_resource(), critical_resource_index) <=
                        half_way.h()) {
                    continue;
                }
                labels.push_back(label);
            }
            return cheapest_first(std::move(labels));
        }

        /// @brief A node's labels, cheapest first.
        ///
        /// No label container guarantees a global cost order, which the join's early exits need.
        ///
        /// @tparam Container The per-node label container.
        /// @param container The node's labels.
        /// @return Pointers to them, sorted by cost.
        template <typename Container>
        [[nodiscard]] static std::vector<Label<ResourceType>*> sorted_by_cost(
            const Container& container) {
            return cheapest_first({container.get_labels().begin(), container.get_labels().end()});
        }

        /// @brief @p labels, cheapest first.
        ///
        /// A strict order, as sorting requires: labels of equal cost keep no particular order,
        /// which the join's early exits (`cost >= bound`) do not depend on.
        ///
        /// @param labels The labels to sort.
        /// @return @p labels, sorted by cost.
        [[nodiscard]] static std::vector<Label<ResourceType>*> cheapest_first(
            std::vector<Label<ResourceType>*> labels) {
            std::ranges::sort(labels, std::less{}, &Label<ResourceType>::get_cost);
            return labels;
        }

        /// @brief Splices a boundary label's chain and a backward chain into one arc sequence.
        ///
        /// The join arc is already @p forward's `in_arc`. The forward chain runs back to the
        /// source and is reversed; the backward chain already runs towards the sink.
        ///
        /// @tparam GraphType The graph type.
        /// @param graph    The graph the arc ids belong to.
        /// @param forward  The boundary label; its chain ends with the join arc.
        /// @param backward The backward half sitting at the same node.
        /// @return The joined path, in traversal order.
        template <typename GraphType>
        [[nodiscard]] static std::vector<size_t> joined_path(const GraphType& graph,
                                                             const Label<ResourceType>& forward,
                                                             const Label<ResourceType>& backward) {
            std::vector<size_t> arc_ids;
            for (const Label<ResourceType>* current = &forward;
                 current != nullptr && current->get_in_arc() != nullptr;
                 current = current->prev_label) {
                arc_ids.push_back(current->get_in_arc()->id);
            }
            std::ranges::reverse(arc_ids);

            // Already in forward order.
            for (const Label<ResourceType>* current = &backward;
                 current != nullptr && current->get_out_arc() != nullptr;
                 current = current->prev_label) {
                arc_ids.push_back(current->get_out_arc()->id);
            }

            assert(is_contiguous(graph, arc_ids) && "joined path is not contiguous");
            return arc_ids;
        }

        /// @brief The node a joined path ends at: where the backward chain runs out, i.e. a sink.
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
        /// Asserted on every reconstruction in debug builds; public so it can be tested directly.
        ///
        /// @param graph   The graph the arc ids belong to.
        /// @param arc_ids The joined arc sequence, in path order.
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
