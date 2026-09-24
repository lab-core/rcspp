// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The merge-rule contract: for a forward-feasible prefix ending at u and a backward-feasible
// suffix starting at v, the join's verdict across arc (u, v) must equal whether the concatenation
// replays feasibly. Both directions matter: accept => feasible (soundness) and feasible => accept
// (completeness); an over-strict rule is otherwise nearly invisible.
//
// Halves are enumerated independently of the algorithms, through the arcs' own extenders, under
// the searches' rules: a walk stops at the first terminal and never re-enters one of its seeds.

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

namespace test_util {

/// @brief One enumerated half-path: where it meets the other half, its arcs, and its state there.
///
/// @tparam Composed The graph's resource composition.
template <typename Composed>
struct MergeHalf {
        size_t node = 0;                                      ///< where the half ends (forward)
                                                              ///< or starts (backward)
        std::vector<size_t> arc_ids;                          ///< in path order, both directions
        std::unique_ptr<rcspp::Resource<Composed>> resource;  ///< the state at @ref node
};

/// @brief What @ref merge_contract_violation found.
///
/// Assert on the counts too: a check over zero pairs, or with no rejections, tests nothing.
struct MergeContractReport {
        std::string violation;  ///< empty when the contract holds
        size_t pairs = 0;       ///< candidate pairs examined
        size_t accepted = 0;    ///< pairs the join accepted
        size_t rejected = 0;    ///< pairs the join rejected
};

/// @brief Replays an arc sequence through the real extenders and reports whether it is feasible.
///
/// @tparam Composed The graph's resource composition.
/// @param graph   The graph the arc ids belong to.
/// @param arc_ids The path, in traversal order.
/// @return `true` when every intermediate node is feasible.
template <typename Composed>
[[nodiscard]] inline bool replay_is_feasible(const rcspp::Graph<Composed>& graph,
                                             const std::vector<size_t>& arc_ids) {
    if (arc_ids.empty()) {
        return false;
    }
    const auto* first = graph.get_arc(arc_ids.front());
    auto current = std::make_unique<rcspp::Resource<Composed>>(*first->origin->resource);
    for (const size_t arc_id : arc_ids) {
        const auto* arc = graph.get_arc(arc_id);
        auto extended = std::make_unique<rcspp::Resource<Composed>>(*arc->destination->resource);
        arc->extender->extend(*current, extended.get());
        if (!extended->is_feasible()) {
            return false;
        }
        current = std::move(extended);
    }
    return true;
}

namespace detail {

/// @brief Depth-first enumeration of forward-feasible prefixes.
template <typename Composed>
inline void collect_forward(const rcspp::Graph<Composed>& graph, const rcspp::Node<Composed>* node,
                            const rcspp::Resource<Composed>& here, size_t remaining_depth,
                            size_t max_per_node, std::vector<size_t>* arcs,
                            std::map<size_t, size_t>* per_node,
                            std::vector<MergeHalf<Composed>>* out) {
    // A forward half may end anywhere but a sink: the joiner never uses a sink as a join origin.
    if (!node->sink) {
        size_t& taken = (*per_node)[node->id];
        if (taken < max_per_node) {
            ++taken;
            out->push_back({node->id, *arcs, std::make_unique<rcspp::Resource<Composed>>(here)});
        }
    }
    if (remaining_depth == 0 || node->sink) {
        return;  // a path ends at the first sink it reaches
    }

    for (auto* arc : graph.get_out_arcs(node)) {
        if (arc->destination->source) {
            continue;  // no path re-enters a source
        }
        rcspp::Resource<Composed> next(*arc->destination->resource);
        arc->extender->extend(here, &next);
        if (!next.is_feasible()) {
            continue;
        }
        arcs->push_back(arc->id);
        collect_forward(graph,
                        arc->destination,
                        next,
                        remaining_depth - 1,
                        max_per_node,
                        arcs,
                        per_node,
                        out);
        arcs->pop_back();
    }
}

/// @brief Depth-first enumeration of backward-feasible suffixes.
///
/// The arc list is built by prepending, so it comes out in forward order.
template <typename Composed>
inline void collect_backward(const rcspp::Graph<Composed>& graph, const rcspp::Node<Composed>* node,
                             const rcspp::Resource<Composed>& here, size_t remaining_depth,
                             size_t max_per_node, std::vector<size_t>* arcs,
                             std::map<size_t, size_t>* per_node,
                             std::vector<MergeHalf<Composed>>* out) {
    // A backward half may start anywhere but a source.
    if (!node->source) {
        size_t& taken = (*per_node)[node->id];
        if (taken < max_per_node) {
            ++taken;
            out->push_back({node->id, *arcs, std::make_unique<rcspp::Resource<Composed>>(here)});
        }
    }
    if (remaining_depth == 0 || node->source) {
        return;
    }

    for (auto* arc : graph.get_in_arcs(node)) {
        if (arc->origin->sink) {
            continue;  // backward never re-enters a sink
        }
        rcspp::Resource<Composed> next(*arc->origin->resource);
        arc->extender->extend_back(here, &next);
        if (!next.is_back_feasible()) {
            continue;
        }
        arcs->insert(arcs->begin(), arc->id);
        collect_backward(graph,
                         arc->origin,
                         next,
                         remaining_depth - 1,
                         max_per_node,
                         arcs,
                         per_node,
                         out);
        arcs->erase(arcs->begin());
    }
}

inline std::string to_string(const std::vector<size_t>& arc_ids) {
    std::string text = "[";
    for (size_t i = 0; i < arc_ids.size(); ++i) {
        text += (i == 0 ? "" : " ") + std::to_string(arc_ids[i]);
    }
    return text + "]";
}

}  // namespace detail

/// @brief Every forward-feasible prefix, up to @p max_depth arcs and @p max_per_node per node.
template <typename Composed>
[[nodiscard]] inline std::vector<MergeHalf<Composed>> enumerate_forward_halves(
    rcspp::Graph<Composed>& graph, size_t max_depth, size_t max_per_node) {
    graph.build_csr();  // get_out_arcs reads the CSR; solve() is never called here
    std::vector<MergeHalf<Composed>> halves;
    std::map<size_t, size_t> per_node;
    for (const size_t source_id : graph.get_source_node_ids()) {
        const auto* source = graph.get_node(source_id);
        std::vector<size_t> arcs;
        // A forward label starts at the type default, which a copy of the node's resource carries.
        const rcspp::Resource<Composed> seed(*source->resource);
        detail::collect_forward(graph,
                                source,
                                seed,
                                max_depth,
                                max_per_node,
                                &arcs,
                                &per_node,
                                &halves);
    }
    return halves;
}

/// @brief Every backward-feasible suffix, up to @p max_depth arcs and @p max_per_node per node.
template <typename Composed>
[[nodiscard]] inline std::vector<MergeHalf<Composed>> enumerate_backward_halves(
    rcspp::Graph<Composed>& graph, size_t max_depth, size_t max_per_node) {
    graph.build_csr();
    std::vector<MergeHalf<Composed>> halves;
    std::map<size_t, size_t> per_node;
    for (const size_t sink_id : graph.get_sink_node_ids()) {
        const auto* sink = graph.get_node(sink_id);
        std::vector<size_t> arcs;
        // A backward label at a sink starts at that sink's own upper bound, not at zero.
        rcspp::Resource<Composed> seed(*sink->resource);
        seed.apply_back_seed();
        detail::collect_backward(graph,
                                 sink,
                                 seed,
                                 max_depth,
                                 max_per_node,
                                 &arcs,
                                 &per_node,
                                 &halves);
    }
    return halves;
}

/// @brief Checks the merge contract over every pair the joiner could be handed.
///
/// The verdict is `extended.is_feasible() && extended.can_be_merged(backward)`, as in
/// `Joiner::join`. Cost and the crossing rule are excluded so they cannot mask a merge-rule defect.
///
/// @tparam Composed The graph's resource composition.
/// @param graph        The model to check (non-const: the CSR is built here).
/// @param max_depth    Longest half to enumerate. Enumeration is exponential; keep it small.
/// @param max_per_node How many halves to keep per node, so a dense graph stays finite.
/// @return The first violation found, plus how much was actually checked.
template <typename Composed>
[[nodiscard]] inline MergeContractReport merge_contract_violation(rcspp::Graph<Composed>& graph,
                                                                  size_t max_depth = 4,
                                                                  size_t max_per_node = 20) {
    MergeContractReport report;
    // The arc spans below come from the CSR; this checker never calls solve().
    graph.build_csr();

    const auto forward = enumerate_forward_halves(graph, max_depth, max_per_node);
    const auto backward = enumerate_backward_halves(graph, max_depth, max_per_node);

    std::map<size_t, std::vector<const MergeHalf<Composed>*>> forward_by_node;
    for (const auto& half : forward) {
        forward_by_node[half.node].push_back(&half);
    }
    std::map<size_t, std::vector<const MergeHalf<Composed>*>> backward_by_node;
    for (const auto& half : backward) {
        backward_by_node[half.node].push_back(&half);
    }

    for (const size_t node_id : graph.get_node_ids()) {
        const auto* origin = graph.get_node(node_id);
        if (origin->sink) {
            continue;  // the joiner skips an arc leaving a sink
        }
        const auto prefixes = forward_by_node.find(node_id);
        if (prefixes == forward_by_node.end()) {
            continue;
        }

        for (auto* arc : graph.get_out_arcs(origin)) {
            if (arc->destination->source) {
                continue;  // ... and an arc entering a source
            }
            const auto suffixes = backward_by_node.find(arc->destination->id);
            if (suffixes == backward_by_node.end()) {
                continue;
            }

            for (const auto* prefix : prefixes->second) {
                rcspp::Resource<Composed> extended(*arc->destination->resource);
                arc->extender->extend(*prefix->resource, &extended);
                const bool feasible_at_join = extended.is_feasible();

                for (const auto* suffix : suffixes->second) {
                    std::vector<size_t> merged = prefix->arc_ids;
                    merged.push_back(arc->id);
                    merged.insert(merged.end(), suffix->arc_ids.begin(), suffix->arc_ids.end());

                    const bool replays = replay_is_feasible(graph, merged);

                    const bool accepted =
                        feasible_at_join && extended.can_be_merged(*suffix->resource);

                    ++report.pairs;
                    (accepted ? report.accepted : report.rejected) += 1;

                    if (accepted != replays) {
                        report.violation =
                            std::string(accepted ? "the join ACCEPTED a splice that replays as "
                                                   "infeasible (unsound)"
                                                 : "the join REFUSED a splice that replays as "
                                                   "feasible (incomplete)") +
                            ": join arc " + std::to_string(arc->id) + " (" +
                            std::to_string(origin->id) + " -> " +
                            std::to_string(arc->destination->id) + "), prefix " +
                            detail::to_string(prefix->arc_ids) + ", suffix " +
                            detail::to_string(suffix->arc_ids) + ", merged " +
                            detail::to_string(merged);
                        return report;
                    }
                }
            }
        }
    }

    return report;
}

}  // namespace test_util
