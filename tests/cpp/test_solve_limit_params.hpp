// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// SolveLimitParams: one solve's time limit and number of solutions, in place of the params of an
// algorithm kept across solves. They hold for that solve only.

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <tuple>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "test_label_recycling.hpp"

namespace solve_limit_params_test {

using rcspp::RealResource;
using Params =
    rcspp::AlgorithmParams<rcspp::LabelList<rcspp::ResourceTypeComposition<RealResource>>>;

/// @brief Params of an algorithm kept across solves.
inline Params kept_params() {
    Params params;
    params.release_after_solve = false;
    return params;
}

/// @brief Three paths from a source (0) to a sink (4), through node 1, 2 or 3, none dominating
///        another: the cheaper, the slower. Component 0 is the cost, component 1 a time limited
///        to 10.
inline std::unique_ptr<label_recycling_test::Graph> trade_off_graph() {
    auto graph = std::make_unique<label_recycling_test::Graph>();
    graph->add_resource<RealResource>(
        std::make_unique<rcspp::AdditionExtensionFunction<RealResource>>(),
        std::make_unique<rcspp::TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<rcspp::ValueCostFunction<RealResource>>(),
        std::make_unique<rcspp::ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<rcspp::AdditionExtensionFunction<RealResource>>(),
        std::make_unique<rcspp::MinMaxFeasibilityFunction<RealResource>>(0.0, 10.0),
        std::make_unique<rcspp::TrivialCostFunction<RealResource>>(),
        std::make_unique<rcspp::ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    for (size_t node = 1; node <= 3; ++node) {
        graph->add_node(node);
    }
    graph->add_node(4, /*source=*/false, /*sink=*/true);
    for (size_t node = 1; node <= 3; ++node) {
        const auto cost = static_cast<double>(node);
        const double time = 4.0 - cost;
        std::tuple<std::vector<std::tuple<double>>> values;
        std::get<0>(values) = {std::tuple<double>(cost), std::tuple<double>(time)};
        graph->add_arc(values, 0, node, cost);
        graph->add_arc(values, node, 4, cost);
    }
    return graph;
}

/// @brief A solve with @c max_solutions 1 returns one solution; the solves before and after it,
///        without a limit of their own, return all the solutions the params allow.
template <typename Strategy>
void check_max_solutions_limit(Params params) {
    namespace lrt = label_recycling_test;
    auto graph = trade_off_graph();
    auto kept = graph->create_algorithm<Strategy>(params);

    const auto all = graph->solve(kept.get(), lrt::kInfinity, false);
    ASSERT_GT(all.solutions.size(), 1U);

    const auto one = graph->solve(kept.get(),
                                  lrt::kInfinity,
                                  false,
                                  0,
                                  rcspp::SolveLimitParams{.max_solutions = 1});
    EXPECT_EQ(one.solutions.size(), 1U);

    const auto again = graph->solve(kept.get(), lrt::kInfinity, false);
    lrt::expect_same_solutions(again, all);
}

/// @brief A solve with a time limit of 0 stops before its first extension (@c Timeout); the solve
///        after it, without a limit of its own, runs in full.
template <typename Strategy>
void check_timeout_limit(Params params) {
    namespace lrt = label_recycling_test;
    auto graph = lrt::layered_graph(/*time_limit=*/6.0);
    auto kept = graph->create_algorithm<Strategy>(params);

    const auto stopped = graph->solve(kept.get(),
                                      lrt::kInfinity,
                                      false,
                                      0,
                                      rcspp::SolveLimitParams{.timeout_s = 0.0});
    EXPECT_EQ(stopped.status, rcspp::AlgorithmStatus::Timeout);
    EXPECT_TRUE(stopped.solutions.empty());

    const auto full = graph->solve(kept.get(), lrt::kInfinity, false);
    EXPECT_NE(full.status, rcspp::AlgorithmStatus::Timeout);
    EXPECT_FALSE(full.solutions.empty());
}

/// @brief Without a limit, @p Strategy finds several paths of trade_off_graph; with a solve's
///        max_solutions 1, it stops at the first (@c MaxSolutions).
template <typename Strategy>
void check_search_stops_at_the_limit(Params params) {
    namespace lrt = label_recycling_test;
    auto graph = trade_off_graph();
    auto kept = graph->create_algorithm<Strategy>(params);

    const auto all = graph->solve(kept.get(), lrt::kInfinity, false);
    ASSERT_GT(all.solutions.size(), 1U);
    EXPECT_NE(all.status, rcspp::AlgorithmStatus::MaxSolutions);

    const auto one = graph->solve(kept.get(),
                                  lrt::kInfinity,
                                  false,
                                  0,
                                  rcspp::SolveLimitParams{.max_solutions = 1});
    ASSERT_EQ(one.solutions.size(), 1U);
    EXPECT_EQ(one.status, rcspp::AlgorithmStatus::MaxSolutions);
}

}  // namespace solve_limit_params_test

TEST(SolveLimitParams, MaxSolutionsHoldsForOneSolve) {
    namespace slp = solve_limit_params_test;
    slp::check_max_solutions_limit<rcspp::strategy::Simple>(slp::kept_params());
    slp::check_max_solutions_limit<rcspp::strategy::Pulling>(slp::kept_params());
    slp::check_max_solutions_limit<rcspp::strategy::Greedy>(slp::kept_params());
    auto bidirectional = slp::kept_params();
    bidirectional.direction = rcspp::SearchDirection::Bidirectional;
    bidirectional.critical_resource_index = 1;
    bidirectional.half_way_point = 3.0;  // NOLINT(readability-magic-numbers)
    slp::check_max_solutions_limit<rcspp::strategy::Simple>(bidirectional);
}

/// @brief The search itself stops at a solve's max_solutions, not only the final cut of the
///        results: with the limit, Greedy and Tabu stop at their first path (MaxSolutions) where
///        the solve before, without it, found several. The labelling searches record a sink's
///        labels when they visit the sink, after the other labels: a small graph cannot show it.
TEST(SolveLimitParams, ASearchStopsAtTheSolvesMaxSolutions) {
    namespace slp = solve_limit_params_test;
    slp::check_search_stops_at_the_limit<rcspp::strategy::Greedy>(slp::kept_params());
    auto bounded = slp::kept_params();
    bounded.max_iterations = 20;  // NOLINT(readability-magic-numbers)
    slp::check_search_stops_at_the_limit<rcspp::strategy::Tabu>(bounded);
}

TEST(SolveLimitParams, TimeoutHoldsForOneSolve) {
    namespace slp = solve_limit_params_test;
    slp::check_timeout_limit<rcspp::strategy::Simple>(slp::kept_params());
    slp::check_timeout_limit<rcspp::strategy::Pulling>(slp::kept_params());
    slp::check_timeout_limit<rcspp::strategy::Greedy>(slp::kept_params());
    auto bounded = slp::kept_params();
    bounded.max_iterations = 20;  // NOLINT(readability-magic-numbers)
    slp::check_timeout_limit<rcspp::strategy::Tabu>(bounded);
    slp::check_timeout_limit<rcspp::strategy::Diversification<rcspp::strategy::Greedy>>(bounded);
}

/// @brief A solve's limit replaces the params' whichever is larger: params that allow no time at
///        all still solve in full with a time limit of +infinity for that solve.
TEST(SolveLimitParams, ASolveLimitCanLiftTheParamsLimit) {
    namespace lrt = label_recycling_test;
    namespace slp = solve_limit_params_test;
    auto graph = lrt::layered_graph(/*time_limit=*/6.0);
    auto params = slp::kept_params();
    params.timeout_s = 0.0;
    auto kept = graph->create_algorithm<rcspp::strategy::Simple>(params);

    EXPECT_EQ(graph->solve(kept.get(), lrt::kInfinity, false).status,
              rcspp::AlgorithmStatus::Timeout);
    const auto lifted = graph->solve(kept.get(),
                                     lrt::kInfinity,
                                     false,
                                     0,
                                     rcspp::SolveLimitParams{.timeout_s = lrt::kInfinity});
    EXPECT_EQ(lifted.status, rcspp::AlgorithmStatus::Complete);
    ASSERT_FALSE(lifted.solutions.empty());
    EXPECT_EQ(lifted.solutions.front().cost, lrt::fresh_optimum(graph.get()));
}

/// @brief Algorithm::solve takes them too, called directly on a graph a solve prepared.
TEST(SolveLimitParams, AlgorithmSolveTakesThem) {
    namespace lrt = label_recycling_test;
    namespace slp = solve_limit_params_test;
    auto graph = slp::trade_off_graph();
    auto kept = graph->create_algorithm<rcspp::strategy::Simple>(slp::kept_params());
    const auto all = graph->solve(kept.get(), lrt::kInfinity, false);  // sorts the graph
    ASSERT_GT(all.solutions.size(), 1U);

    const auto one =
        kept->solve(graph.get(), lrt::kInfinity, rcspp::SolveLimitParams{.max_solutions = 1});
    ASSERT_EQ(one.solutions.size(), 1U);
    // A labelling search finds them all, and keeps the cheapest.
    EXPECT_EQ(one.solutions.front().cost, all.solutions.front().cost);
}
