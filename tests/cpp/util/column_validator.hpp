// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// An independent check of the columns an algorithm returns.
//
// Heuristics may miss columns but must never invent one. Every heuristic under study builds its
// paths in its own way -- a dive's `path_`, a join's splice, a diversification step on a graph
// clone -- so each returned column is replayed here, forward from a fresh source resource, through
// the model's own extension and feasibility functions. Nothing here shares code with the
// algorithms beyond those functions.
//
// Run it on the graph the solve used, after the solve returned: the preprocessor has restored its
// arcs by then, and the arc ids in a `Solution` are the graph's own.

#include <algorithm>
#include <cmath>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "rcspp/rcspp.hpp"

namespace test_util {

/// @brief What the validator found wrong with one column.
struct ColumnIssue {
        /// @brief Index of the column in the vector passed in.
        size_t column = 0;
        /// @brief What is wrong, in one sentence.
        std::string what;
};

/// @brief Checks one column against the model.
///
/// The checks, in order; the first that fails is reported:
///  - the path is non-empty and every arc id names an active arc;
///  - consecutive arcs connect end to end;
///  - it starts at a source and ends at a sink, with neither strictly inside it;
///  - `path_node_ids`, when filled, lists the nodes the arcs visit;
///  - replayed forward, every extension passes the reachability look-ahead and is feasible;
///  - the replayed labeling cost equals `Solution::cost`, within @p tolerance (relative to the
///    cost's magnitude, and absolute below 1);
///  - `column.cost` equals the sum of the arcs' original weights.
///
/// @param graph     The graph the solve ran on.
/// @param solution  The column to check.
/// @param tolerance Allowed cost discrepancy.
/// @return An empty string when the column is valid, else what is wrong with it.
template <typename ResourceType>
std::string check_column(const rcspp::Graph<ResourceType>& graph, const rcspp::Solution& solution,
                         double tolerance = 1e-6) {
    using ResourcePtr = std::unique_ptr<rcspp::Resource<ResourceType>>;
    const auto& arc_ids = solution.path_arc_ids;
    if (arc_ids.empty()) {
        return "the path has no arc";
    }

    std::vector<const rcspp::Arc<ResourceType>*> arcs;
    arcs.reserve(arc_ids.size());
    for (const size_t arc_id : arc_ids) {
        const auto* arc = graph.get_arc(arc_id);
        if (arc == nullptr) {
            return "arc " + std::to_string(arc_id) + " is not an active arc of the graph";
        }
        arcs.push_back(arc);
    }

    for (size_t i = 0; i + 1 < arcs.size(); ++i) {
        if (arcs[i]->destination != arcs[i + 1]->origin) {
            return "arcs " + std::to_string(i) + " and " + std::to_string(i + 1) +
                   " do not connect";
        }
    }

    if (!arcs.front()->origin->source) {
        return "the path starts at node " + std::to_string(arcs.front()->origin->id) +
               ", which is not a source";
    }
    if (!arcs.back()->destination->sink) {
        return "the path ends at node " + std::to_string(arcs.back()->destination->id) +
               ", which is not a sink";
    }
    for (size_t i = 0; i + 1 < arcs.size(); ++i) {
        const auto* inner = arcs[i]->destination;
        if (inner->source || inner->sink) {
            return "node " + std::to_string(inner->id) +
                   " is a source or a sink strictly inside the path";
        }
    }

    if (!solution.path_node_ids.empty()) {
        if (solution.path_node_ids.size() != arcs.size() + 1) {
            return "path_node_ids has " + std::to_string(solution.path_node_ids.size()) +
                   " nodes for " + std::to_string(arcs.size()) + " arcs";
        }
        for (size_t i = 0; i < arcs.size(); ++i) {
            if (solution.path_node_ids[i] != arcs[i]->origin->id) {
                return "path_node_ids[" + std::to_string(i) + "] disagrees with the arcs";
            }
        }
        if (solution.path_node_ids.back() != arcs.back()->destination->id) {
            return "path_node_ids ends at a node the arcs do not reach";
        }
    }

    // Replay: a fresh source resource, then one extension per arc into a resource bound to the
    // arc's destination, as a pooled label is.
    ResourcePtr current =
        std::make_unique<rcspp::Resource<ResourceType>>(*arcs.front()->origin->resource);
    for (size_t i = 0; i < arcs.size(); ++i) {
        const auto* arc = arcs[i];
        if (!current->is_reachable(arc->destination->id)) {
            return "the look-ahead rejects arc " + std::to_string(i) + " (into node " +
                   std::to_string(arc->destination->id) + ")";
        }
        ResourcePtr next =
            std::make_unique<rcspp::Resource<ResourceType>>(*arc->destination->resource);
        arc->extender->extend(*current, next.get());
        if (!next->is_feasible()) {
            return "the path is infeasible on arriving at node " +
                   std::to_string(arc->destination->id) + " (arc " + std::to_string(i) + ")";
        }
        current = std::move(next);
    }

    const double replayed = current->get_cost();
    const double scale = std::max(1.0, std::abs(replayed));
    if (!(std::abs(replayed - solution.cost) <= tolerance * scale)) {
        return "the column claims cost " + std::to_string(solution.cost) + " but replays at " +
               std::to_string(replayed);
    }

    long double weight = 0.0;
    for (const auto* arc : arcs) {
        weight += arc->cost;
    }
    const double column_scale = std::max(1.0L, std::abs(weight));
    if (!(std::abs(static_cast<double>(weight) - solution.column.cost) <=
          tolerance * column_scale)) {
        return "column.cost is " + std::to_string(solution.column.cost) + " but the arcs weigh " +
               std::to_string(static_cast<double>(weight));
    }
    return {};
}

/// @brief Checks every column, plus that no path is returned twice.
///
/// @param graph     The graph the solve ran on.
/// @param solutions The columns to check.
/// @param tolerance Allowed cost discrepancy.
/// @return One issue per bad column; empty when all are valid.
template <typename ResourceType>
std::vector<ColumnIssue> validate_columns(const rcspp::Graph<ResourceType>& graph,
                                          const std::vector<rcspp::Solution>& solutions,
                                          double tolerance = 1e-6) {
    std::vector<ColumnIssue> issues;
    std::set<std::vector<size_t>> seen;
    for (size_t i = 0; i < solutions.size(); ++i) {
        if (!seen.insert(solutions[i].path_arc_ids).second) {
            issues.push_back({i, "the same path is returned twice"});
            continue;
        }
        if (auto what = check_column(graph, solutions[i], tolerance); !what.empty()) {
            issues.push_back({i, std::move(what)});
        }
    }
    return issues;
}

/// @brief Renders issues for a test failure message, at most @p limit of them.
inline std::string describe(const std::vector<ColumnIssue>& issues, size_t limit = 5) {
    std::string text;
    for (size_t i = 0; i < issues.size() && i < limit; ++i) {
        text += "\n  column " + std::to_string(issues[i].column) + ": " + issues[i].what;
    }
    if (issues.size() > limit) {
        text += "\n  ... and " + std::to_string(issues.size() - limit) + " more";
    }
    return text;
}

}  // namespace test_util
