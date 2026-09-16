// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The merge-rule contract, asserted generically.
//
//   Given a forward-feasible prefix ending at u and a backward-feasible suffix starting at v, the
//   join's verdict on that pair across arc (u, v) must equal whether the concatenation replays
//   feasibly from end to end.
//
// Both directions of the biconditional, and the second one is the point:
//
//   accept  => feasible    SOUNDNESS.    Already well covered -- a bad merge produces a path
//                                        neither search would generate, and `path_problem`
//                                        replays every returned solution.
//   feasible => accept     COMPLETENESS. Covered by nothing. An over-strict merge rule loses no
//                                        elementary route, so it cannot corrupt a
//                                        column-generation bound; it shows up only as
//                                        `bidirectional` and `simple` disagreeing, and only on an
//                                        instance where the difference happens to move the
//                                        optimum. That is how review finding F1 survived a suite
//                                        with two oracles and a seeded sweep.
//
// This checks the rule directly rather than waiting for it to move an optimum, which is the same
// relationship `check_backward_contract` has to `BackwardKind`: the declaration selects behaviour,
// the contract test proves the behaviour is right.
//
// It shares no code with either algorithm. The halves are enumerated here, through the arcs' own
// `Extender`s, under the same two rules the searches obey -- a walk stops at the first terminal it
// reaches and never re-enters one of its own seeds -- so the pairs offered to the merge rule are
// pairs the joiner could really be handed.

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
/// The counts are not decoration: a contract test that checks zero pairs, or that never sees the
/// rule reject anything, passes without testing it. Assert on them.
struct MergeContractReport {
        std::string violation;  ///< empty when the contract holds
        size_t pairs = 0;       ///< candidate pairs examined
        size_t accepted = 0;    ///< pairs the join accepted
        size_t rejected = 0;    ///< pairs the join rejected
        size_t verified = 0;    ///< refusals the joiner's replay overturned
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
    // A forward half may end anywhere that is not a sink: a search records and stops at a sink,
    // so the joiner never uses one as a join arc's origin.
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
/// The arc list is built by prepending, so it comes out in forward order -- the same asymmetry
/// `Joiner::merged_path` relies on.
template <typename Composed>
inline void collect_backward(const rcspp::Graph<Composed>& graph, const rcspp::Node<Composed>* node,
                             const rcspp::Resource<Composed>& here, size_t remaining_depth,
                             size_t max_per_node, std::vector<size_t>* arcs,
                             std::map<size_t, size_t>* per_node,
                             std::vector<MergeHalf<Composed>>* out) {
    // Mirror of the forward rule: a backward half may start anywhere that is not a source.
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
    graph.build_csr();  // get_out_arcs reads the CSR, which solve() would otherwise build
    std::vector<MergeHalf<Composed>> halves;
    std::map<size_t, size_t> per_node;
    for (const size_t source_id : graph.get_source_node_ids()) {
        const auto* source = graph.get_node(source_id);
        std::vector<size_t> arcs;
        // A forward label starts at the type default -- nothing consumed -- which is what a copy
        // of the node's resource carries.
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
/// The join's verdict is `extended.is_feasible() && extended.can_be_merged(backward)`, plus the
/// replay `Joiner::join` runs when a rule declares `merge_refusal_may_be_conservative()` -- the
/// tests it applies once cost and the crossing rule have had their say. Cost and the crossing rule
/// are deliberately excluded: they decide *which* pairs are offered, not whether a pair is
/// admissible, and mixing them in would let a cost filter mask a merge-rule defect.
///
/// @ref MergeContractReport::verified counts the refusals the replay overturned. For a rule that
/// declares itself exact that must stay zero; for one that does not, a test should assert it is
/// non-zero, or the verification path is dormant and untested.
///
/// @tparam Composed The graph's resource composition.
/// @param graph        The model to check. Taken by reference because the CSR is built here:
///                     this checker never calls solve(), which is what would otherwise build it.
/// @param max_depth    Longest half to enumerate. Enumeration is exponential; keep it small.
/// @param max_per_node How many halves to keep per node, so a dense graph stays finite.
/// @return The first violation found, plus how much was actually checked.
template <typename Composed>
[[nodiscard]] inline MergeContractReport merge_contract_violation(rcspp::Graph<Composed>& graph,
                                                                  size_t max_depth = 4,
                                                                  size_t max_per_node = 20) {
    MergeContractReport report;
    // The arc spans below come from the CSR, which is built by solve(); this checker never solves.
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

                const bool conservative = extended.merge_refusal_may_be_conservative();

                for (const auto* suffix : suffixes->second) {
                    std::vector<size_t> merged = prefix->arc_ids;
                    merged.push_back(arc->id);
                    merged.insert(merged.end(), suffix->arc_ids.begin(), suffix->arc_ids.end());

                    const bool replays = replay_is_feasible(graph, merged);

                    bool accepted = feasible_at_join && extended.can_be_merged(*suffix->resource);
                    if (feasible_at_join && !accepted && conservative) {
                        // What Joiner::join does with a refusal from a rule that admits it may be
                        // conservative: replay before dropping.
                        accepted = replays;
                        if (accepted) {
                            ++report.verified;
                        }
                    }

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
