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

#include "rcspp/algorithm/algorithm_params.hpp"
#include "rcspp/algorithm/make_algorithm.hpp"
#include "rcspp/algorithm/path_evaluator.hpp"
#include "rcspp/algorithm/search_direction.hpp"
#include "rcspp/algorithm/solution.hpp"
#include "rcspp/algorithm/solve_result.hpp"
#include "rcspp/algorithm/strategies.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/resource/concrete/numerical_resource.hpp"

/// @brief Helpers for testing a strategy of one's own.
namespace rcspp::test_support {

/// @brief The name of @p fault, for a message.
[[nodiscard]] inline std::string path_fault_name(PathFault fault) {
    switch (fault) {
        case PathFault::None:
            return "none";
        case PathFault::Empty:
            return "empty path";
        case PathFault::UnknownArc:
            return "unknown arc";
        case PathFault::Disconnected:
            return "disconnected";
        case PathFault::NotFromSource:
            return "does not start at a source";
        case PathFault::NotToSink:
            return "does not end at a sink";
        case PathFault::Unreachable:
            return "unreachable";
        case PathFault::Infeasible:
            return "infeasible";
        case PathFault::NodesDisagree:
            return "node ids disagree with the arcs";
        case PathFault::CostDisagrees:
            return "cost disagrees with the replay";
    }
    return "unknown fault";
}

/// @brief What is wrong with @p solution as a path of @p graph, or "": the first fault
///        @ref PathEvaluator finds, and where.
///
/// @param graph     The graph the solve ran on.
/// @param solution  The solution to check.
/// @param tolerance How far the reported cost may be from the replayed one (relative to the
///                  cost's magnitude, absolute below 1).
template <typename ResourceType>
[[nodiscard]] std::string path_problem(const Graph<ResourceType>& graph, const Solution& solution,
                                       double tolerance = kDefaultPathCostTolerance) {
    const PathEvaluation evaluation =
        PathEvaluator<ResourceType>(graph).evaluate(solution, tolerance);
    if (evaluation.is_valid()) {
        return "";
    }
    return "path fault: " + path_fault_name(evaluation.fault) + ", at position " +
           std::to_string(evaluation.position) + " (replayed cost " +
           std::to_string(evaluation.cost) + ", reported " + std::to_string(solution.cost) + ")";
}

/// @brief Solves @p graph with @p Strategy in each direction it does not refuse, and compares
///        each result with forward Simple's: returns "" when every solve finds Simple's optimum
///        cost (or, like Simple, no path) along paths that replay through the model
///        (@ref path_problem); otherwise the first disagreement, prefixed by its direction.
///
/// An exact strategy (one that changes the order of the work, never its answer) must pass on
/// every model. A strategy that drops labels may fail, and that is not a defect: one whose
/// admission rule is not exact, or one stopped by a budget.
///
/// Every solve, the reference included, uses @p params with only the direction changed, and
/// without preprocessing. So @p params must not limit the search (no @c max_iterations,
/// @c timeout_s, @c max_solutions, per-node quota, or admission rule other than @c Exact), or
/// the reference is no optimum either. The solves' status is not read.
///
/// @code
/// EXPECT_EQ(rcspp::test_support::optimum_problem<MyStrategy>(graph, params), "");
/// @endcode
///
/// @tparam Strategy             The strategy to test.
/// @tparam CostResourceType     The cost resource type.
/// @tparam CriticalResourceType The type of the critical resource (@c critical_resource_index),
///                              whose value decides where a bidirectional search's two halves
///                              stop (its half-way point).
/// @param graph  The model; solved without preprocessing, and left unchanged.
/// @param params The params of every solve, except their direction, which this function sets;
///               a bidirectional solve reads their `critical_resource_index` and
///               `half_way_point`.
/// @param tolerance How far two optima may be apart.
/// @return "", or a description of the first disagreement.
/// @throws ModelRefused when the model checks refuse a backward or bidirectional solve.
/// @throws std::invalid_argument when @p Strategy searches backward or bidirectionally and
///         @p LabelContainerType cannot compare backward (no `with_direction` of its own).
template <typename Strategy, typename CostResourceType = RealResource,
          typename CriticalResourceType = CostResourceType, typename GraphType,
          typename LabelContainerType>
[[nodiscard]] std::string optimum_problem(
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

}  // namespace rcspp::test_support
