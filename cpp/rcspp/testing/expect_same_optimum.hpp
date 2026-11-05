// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/make_algorithm.hpp"
#include "rcspp/algorithm/search_direction.hpp"
#include "rcspp/algorithm/solution.hpp"
#include "rcspp/algorithm/strategies.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/resource/concrete/numerical_resource.hpp"

/// @brief Helpers for testing a strategy of one's own.
namespace rcspp::testing {

/// @brief What is wrong with @p solution as a path of @p graph, or "".
///
/// Extends a fresh source resource arc by arc through each arc's extender, and requires the path
/// to start at a source and end at a sink, every arc to follow the last, every node on the way to
/// be feasible, and the replayed cost to be the reported one.
template <typename ComposedType>
[[nodiscard]] std::string path_problem(
    const Graph<ComposedType>& graph, const Solution& solution,
    double tolerance = 1e-6) {  // NOLINT(readability-magic-numbers)
    if (solution.path_arc_ids.empty()) {
        return "empty path";
    }
    const auto* first = graph.get_arc(solution.path_arc_ids.front());
    if (first == nullptr) {
        return "unknown arc id";
    }
    if (!first->origin->source) {
        return "path does not start at a source";
    }
    auto current = std::make_unique<Resource<ComposedType>>(*first->origin->resource);
    const Arc<ComposedType>* previous = nullptr;
    for (const size_t arc_id : solution.path_arc_ids) {
        const auto* arc = graph.get_arc(arc_id);
        if (arc == nullptr) {
            return "unknown arc id";
        }
        if (previous != nullptr && previous->destination != arc->origin) {
            return "path is not contiguous at arc " + std::to_string(arc_id);
        }
        if (arc->extender == nullptr) {
            return "arc has no extender";
        }
        auto extended = std::make_unique<Resource<ComposedType>>(*arc->destination->resource);
        arc->extender->extend(*current, extended.get());
        if (!extended->is_feasible()) {
            return "path is infeasible at node " + std::to_string(arc->destination->id);
        }
        current = std::move(extended);
        previous = arc;
    }
    if (!previous->destination->sink) {
        return "path does not end at a sink";
    }
    if (std::abs(current->get_cost() - solution.cost) > tolerance) {
        return "path costs " + std::to_string(current->get_cost()) + " but is reported at " +
               std::to_string(solution.cost);
    }
    return "";
}

/// @brief Whether @p Strategy, in every direction it searches, finds forward Simple's optimum on
///        @p graph, or, like Simple, nothing, along paths that replay: "" when it does, otherwise
///        what went wrong first.
///
/// Every exact strategy must pass on every model: it changes the order of the work, never its
/// answer. A strategy whose admission rule is not exact may fail, and that is not a defect.
///
/// @code
/// EXPECT_EQ(rcspp::testing::expect_same_optimum<MyStrategy>(graph, params), "");
/// @endcode
///
/// @tparam Strategy             The strategy to test.
/// @tparam CostResourceType     The cost resource type.
/// @tparam CriticalResourceType The bidirectional search's clock type.
/// @param graph  The model; solved without preprocessing, and left unchanged.
/// @param params The params of every solve, but their direction; a bidirectional solve reads
///               their `critical_resource_index` and `half_way_point`.
/// @param tolerance How far two optima may be apart.
/// @return "", or a description of the first disagreement.
template <typename Strategy, typename CostResourceType = RealResource,
          typename CriticalResourceType = CostResourceType, typename GraphType,
          typename LabelContainerType>
[[nodiscard]] std::string expect_same_optimum(
    GraphType& graph, const AlgorithmParams<LabelContainerType>& params,
    double tolerance = 1e-6) {  // NOLINT(readability-magic-numbers)
    // `tag` names the strategy: std::type_identity<S>.
    const auto solve = [&](auto tag, SearchDirection direction) {
        using S = typename decltype(tag)::type;
        auto directed = params;
        directed.direction = direction;
        return graph.template solve<S, LabelContainerType, CostResourceType, CriticalResourceType>(
            std::numeric_limits<double>::infinity(),
            std::move(directed),
            /*preprocess=*/false);
    };
    const auto best = [](const SolveResult& result) {
        double cost = std::numeric_limits<double>::infinity();
        for (const auto& solution : result.solutions) {
            cost = std::min(cost, solution.cost);
        }
        return cost;
    };

    const SolveResult reference =
        solve(std::type_identity<strategy::Simple>{}, SearchDirection::Forward);
    for (const auto direction :
         {SearchDirection::Forward, SearchDirection::Backward, SearchDirection::Bidirectional}) {
        if (!detail::searches<Strategy>(direction)) {
            continue;
        }
        const std::string where = std::string(to_string(direction)) + ": ";
        const SolveResult result = solve(std::type_identity<Strategy>{}, direction);
        if (result.solutions.empty() != reference.solutions.empty()) {
            return where + (result.solutions.empty() ? "no solution, but forward Simple found one"
                                                     : "a solution, but forward Simple found none");
        }
        if (!result.solutions.empty() && std::abs(best(result) - best(reference)) > tolerance) {
            return where + "the optimum costs " + std::to_string(best(result)) +
                   ", forward Simple's " + std::to_string(best(reference));
        }
        for (const auto& solution : result.solutions) {
            if (auto problem = path_problem(graph, solution, tolerance); !problem.empty()) {
                return where + problem;
            }
        }
    }
    return "";
}

}  // namespace rcspp::testing
