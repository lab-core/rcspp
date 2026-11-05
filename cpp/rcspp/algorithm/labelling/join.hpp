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
#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/labelling/half_way.hpp"
#include "rcspp/algorithm/solution.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp::detail {

/// @brief What one join pass may pair and keep.
///
/// @tparam ResourceType The resource type carried by labels.
template <typename ResourceType>
struct JoinOptions {
        /// @brief Which forward labels are boundary labels: those past the half-way point. Off:
        ///        every stored non-start forward label is paired.
        HalfWay<ResourceType> half_way;

        /// @brief Whether to drop pairs costing at least the incumbent (the cheapest complete
        ///        path found so far). The join prunes against the incumbent anyway when
        ///        @ref upper_bound is infinite.
        bool prune_by_incumbent = true;

        /// @brief The caller's fixed bound. Always applied.
        double upper_bound = std::numeric_limits<double>::infinity();

        /// @brief When below @c kNoLimit, keep only this many cheapest distinct joined paths and
        ///        hand them on at the end of the pass. A pair too expensive to be among them is
        ///        skipped before its path is built.
        size_t max_solutions = kNoLimit;
};

/// @brief Whether consecutive arcs connect end to end.
///
/// Asserted on every joined path in debug builds.
///
/// @param graph   The graph the arc ids belong to.
/// @param arc_ids The arc sequence, in path order.
/// @return @c true when each arc's destination is the next arc's origin.
template <typename GraphType>
[[nodiscard]] bool is_contiguous(const GraphType& graph, const std::vector<size_t>& arc_ids) {
    for (size_t i = 0; i + 1 < arc_ids.size(); ++i) {
        if (graph.get_arc(arc_ids[i])->destination != graph.get_arc(arc_ids[i + 1])->origin) {
            return false;
        }
    }
    return true;
}

namespace join_detail {

/// @brief The cheapest @c capacity distinct joined paths seen so far.
///
/// A max-heap on cost plus the set of paths it holds, so a path joined at several nodes takes one
/// slot.
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

/// @brief @p labels, cheapest first.
///
/// A strict order, as sorting requires: labels of equal cost keep no particular order, which the
/// join's early exits (`cost >= bound`) do not depend on.
template <typename ResourceType>
[[nodiscard]] std::vector<Label<ResourceType>*> cheapest_first(
    std::vector<Label<ResourceType>*> labels) {
    std::ranges::sort(labels, std::less{}, &Label<ResourceType>::get_cost);
    return labels;
}

/// @brief The forward labels at a node that crossed `H` on their own last arc, cheapest first: a
///        boundary label is a non-start label past the half-way point; with the point off, every
///        non-start label qualifies.
template <typename ResourceType, typename ForwardContainerType>
[[nodiscard]] std::vector<Label<ResourceType>*> boundary_labels_by_cost(
    const ForwardContainerType& container, const HalfWay<ResourceType>& half_way) {
    std::vector<Label<ResourceType>*> labels;
    for (auto* label : container.get_labels()) {
        // A start label is not a crossing.
        if (label->prev_label == nullptr || label->get_in_arc() == nullptr) {
            continue;
        }
        assert(!label->get_in_arc()->origin->sink &&
               "the forward search must never extend out of a sink");
        if (half_way.on() && !half_way.template past<ForwardDirection>(*label)) {
            continue;
        }
        labels.push_back(label);
    }
    return cheapest_first<ResourceType>(std::move(labels));
}

/// @brief A node's labels, cheapest first.
///
/// No label container guarantees a global cost order, which the join's early exits need.
template <typename ResourceType, typename ContainerType>
[[nodiscard]] std::vector<Label<ResourceType>*> sorted_by_cost(const ContainerType& container) {
    return cheapest_first<ResourceType>(
        {container.get_labels().begin(), container.get_labels().end()});
}

/// @brief Splices a boundary label's chain and a backward chain into one arc sequence.
///
/// The join arc is already @p forward's `in_arc`. The forward chain runs back to the source and is
/// reversed; the backward chain already runs towards the sink.
template <typename GraphType, typename ResourceType>
[[nodiscard]] std::vector<size_t> joined_path(const GraphType& graph,
                                              const Label<ResourceType>& forward,
                                              const Label<ResourceType>& backward) {
    std::vector<size_t> arc_ids = prefix_arc_ids(forward);
    append_suffix_arc_ids(backward, &arc_ids);
    assert(is_contiguous(graph, arc_ids) && "joined path is not contiguous");
    (void)graph;
    return arc_ids;
}

}  // namespace join_detail

/// @brief Pairs each node's boundary forward labels with the backward labels stored there, and
///        hands every accepted path to @p on_path.
///
/// Forward **boundary labels** -- labels whose own last arc crossed `H`, so they were stored but
/// never extended -- are paired with the backward labels at the same node. Each complete path
/// crosses `H` on exactly one arc, so it is produced once. Feasibility needs no test (infeasible
/// labels are never stored); each pair is checked with `can_be_joined`, and its cost is the sum of
/// the two halves (the join arc is inside the forward half). Forward labels a quota abandoned
/// contribute nothing. Cost bounds are tested on a pair's total cost, or on a forward label plus
/// the cheapest backward label at the node; never on one half's cost alone, which is not a lower
/// bound under reduced costs.
///
/// @tparam ResourceType      The resource type carried by labels.
/// @tparam ForwardContainer  Per-node forward label container.
/// @tparam BackwardContainer Per-node backward label container.
/// @tparam OnPath            Callable `(double cost, std::vector<size_t> arc_ids,
///                           size_t end_node_id)`.
/// @param graph     The model.
/// @param forward   Forward labels, by node position.
/// @param backward  Backward labels, by node position.
/// @param options   What the pass may pair and keep.
/// @param incumbent The cost of the cheapest complete path found so far; lowered in place as
///                  cheaper joins are found.
/// @param on_path   Invoked once per accepted join.
template <typename ResourceType, typename ForwardContainer, typename BackwardContainer,
          typename OnPath>
void join(  // NOLINT(readability-function-cognitive-complexity)
    const Graph<ResourceType>& graph, const std::vector<ForwardContainer>& forward,
    const std::vector<BackwardContainer>& backward, const JoinOptions<ResourceType>& options,
    double& incumbent, OnPath&& on_path) {
    const double upper_bound = options.upper_bound;
    // Prune against the incumbent when asked, or when there is no fixed bound: otherwise a default
    // solve would return every admissible pair. A finite bound with no request means "every
    // column below this bound".
    const bool prune_against_incumbent = options.prune_by_incumbent || !std::isfinite(upper_bound);
    const bool capped = options.max_solutions < kNoLimit;
    join_detail::Cheapest cheapest(options.max_solutions);
    // A budget of zero keeps nothing; stop before `out_of_range` reads an empty heap.
    if (capped && options.max_solutions == 0) {
        return;
    }
    // A pair at or above this cannot be among the cheapest kept, so is not worth splicing.
    const auto out_of_range = [&](double cost) {
        return cost >= upper_bound || (prune_against_incumbent && cost >= incumbent) ||
               (capped && cheapest.full() && cost >= cheapest.worst());
    };

    for (const size_t node_id : graph.get_node_ids()) {
        const auto* node = graph.get_node(node_id);
        // Paths ending at a sink were already recorded by the forward search.
        if (node == nullptr || node->sink) {
            continue;
        }
        const auto& backward_labels = backward.at(node->pos());
        if (backward_labels.get_labels().empty()) {
            continue;
        }
        const auto boundary =
            join_detail::boundary_labels_by_cost(forward.at(node->pos()), options.half_way);
        if (boundary.empty()) {
            continue;
        }
        // The early exits below require cost order.
        const auto backward_sorted = join_detail::sorted_by_cost<ResourceType>(backward_labels);
        const double cheapest_backward = backward_sorted.front()->get_cost();

        for (const auto* forward_label : boundary) {
            // Boundary labels are cost-sorted, so every later one is worse too.
            if (out_of_range(forward_label->get_cost() + cheapest_backward)) {
                break;
            }

            for (const auto* backward_label : backward_sorted) {
                const double cost = forward_label->get_cost() + backward_label->get_cost();
                // Forbidden arcs carry an infinite cost.
                if (std::isinf(cost)) {
                    continue;
                }
                // Cost-sorted, so once one pair is out of range every later one is too.
                if (out_of_range(cost)) {
                    break;
                }
                // can_be_joined is an exact test: a refused pair cannot form a feasible path.
                if (!forward_label->get_resource().can_be_joined(backward_label->get_resource())) {
                    continue;
                }

                auto arc_ids = join_detail::joined_path(graph, *forward_label, *backward_label);
                // The backward chain ends at the sink its start label sat on: the last arc's
                // destination (a boundary label has an in-arc, so there is one).
                const size_t end_node_id = graph.get_arc(arc_ids.back())->destination->id;
                if (capped) {
                    cheapest.offer(cost, std::move(arc_ids), end_node_id);
                } else {
                    on_path(cost, std::move(arc_ids), end_node_id);
                }
                incumbent = std::min(incumbent, cost);
            }
        }
    }

    for (auto& path : cheapest.take()) {
        on_path(path.cost, std::move(path.arc_ids), path.end_node_id);
    }
}

}  // namespace rcspp::detail
