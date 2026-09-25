// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "rcspp/algorithm/half_way_policy.hpp"
#include "rcspp/algorithm/solution.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp {

/// @brief What one join pass did.
struct JoinStats {
        /// @brief Pairs the merge rule was asked about: one per `can_be_merged` call.
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
/// labels are never stored); each pair is checked with `can_be_merged`, and its cost is the sum of
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
        /// With the bound disabled, every stored non-seed forward label is paired. Forward labels
        /// abandoned by the per-node extension quota contribute nothing. Cost bounds cut pairs,
        /// or a half plus the cheapest completion at the node, never a half's own cost.
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
        /// @param best_cost_upper_bound   The incumbent, tightened in place as solutions are found.
        /// @param prune_requested         Whether to drop pairs no better than the incumbent. The
        ///                                incumbent cutoff also applies when @p cost_upper_bound
        ///                                is infinite.
        /// @param cost_upper_bound        The caller's fixed bound. Always applied.
        /// @param on_solution             Invoked once per accepted join.
        /// @param max_solutions           When finite, only the cheapest this many distinct
        ///                                joined paths reach @p on_solution, after the pass;
        ///                                pairs that cannot be among them are never spliced.
        /// @param max_pairs               When finite, the pass stops after this many merge-rule
        ///                                questions and reports `truncated`; what it had kept is
        ///                                still handed to @p on_solution.
        /// @return How many pairs were tested, and whether @p max_pairs cut the pass short.
        template <typename FwdContainer, typename BwdContainer, typename OnSolution>
        JoinStats join(const Graph<ResourceType>& graph, std::vector<FwdContainer>& forward_by_pos,
                       std::vector<BwdContainer>& backward_by_pos, const HalfWayPolicy& half_way,
                       size_t critical_resource_index, double& best_cost_upper_bound,
                       bool prune_requested, double cost_upper_bound, OnSolution&& on_solution,
                       size_t max_solutions = std::numeric_limits<size_t>::max(),
                       size_t max_pairs = std::numeric_limits<size_t>::max()) {
            JoinStats stats;
            // Prune against the incumbent when asked, or when there is no fixed bound: otherwise a
            // default solve would return every admissible pair. A finite bound with no request
            // means "every column below this bound".
            const bool prune_against_incumbent =
                prune_requested || !std::isfinite(cost_upper_bound);
            const bool capped = max_solutions != std::numeric_limits<size_t>::max();
            Cheapest cheapest(max_solutions);
            // A budget of zero keeps nothing; stop before `out_of_range` reads an empty heap.
            if (capped && max_solutions == 0) {
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
                                                              half_way,
                                                              critical_resource_index);
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
                        // Merge rules are exact, so a refusal is final.
                        if (!forward->get_resource().can_be_merged(backward->get_resource())) {
                            continue;
                        }

                        auto arc_ids = merged_path(graph, *forward, *backward);
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
                    if (capacity_ == 0 || kept_.contains(arc_ids)) {
                        return;
                    }
                    if (full()) {
                        if (cost >= worst()) {
                            return;
                        }
                        std::ranges::pop_heap(heap_, by_cost);
                        kept_.erase(heap_.back().arc_ids);
                        heap_.pop_back();
                    }
                    kept_.insert(arc_ids);
                    heap_.push_back({cost, std::move(arc_ids), end_node_id});
                    std::ranges::push_heap(heap_, by_cost);
                }

                /// @brief The kept paths, cheapest first; empties the heap.
                [[nodiscard]] std::vector<Path> take() {
                    std::ranges::sort_heap(heap_, by_cost);
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

                static constexpr auto by_cost = [](const Path& lhs, const Path& rhs) {
                    return lhs.cost < rhs.cost;
                };

                size_t capacity_;
                std::vector<Path> heap_;
                std::unordered_set<std::vector<size_t>, PathHash> kept_;
        };

        /// @brief The forward labels at a node that crossed `H` on their own last arc, cheapest
        ///        first.
        ///
        /// A boundary label is a non-seed label with `clock > H`; with the bound disabled, every
        /// non-seed label qualifies.
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
                // A seed is not a crossing.
                if (label->prev_label == nullptr || label->get_in_arc() == nullptr) {
                    continue;
                }
                assert(!label->get_in_arc()->origin->sink &&
                       "the forward search must never extend out of a sink");
                if (half_way.enabled() && critical_value(label->get_resource(),
                                                         critical_resource_index) <= half_way.h()) {
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
        /// No label container guarantees a global cost order, which the join's early exits need.
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
        /// The join arc is already @p forward's `in_arc`. The forward chain runs back to the
        /// source and is reversed; the backward chain already runs towards the sink.
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

            // Already in forward order.
            for (const Label<ResourceType>* current = &backward;
                 current != nullptr && current->get_out_arc() != nullptr;
                 current = current->prev_label) {
                arc_ids.push_back(current->get_out_arc()->id);
            }

            assert(is_contiguous(graph, arc_ids) && "merged path is not contiguous");
            return arc_ids;
        }

        /// @brief Reads the critical resource's scalar value out of a composed resource.
        ///
        /// Returns 0.0 when the type is absent from the pack (the bound is then disabled); the
        /// guard keeps that case compiling.
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
        /// Asserted on every reconstruction in debug builds; public so it can be tested directly.
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
