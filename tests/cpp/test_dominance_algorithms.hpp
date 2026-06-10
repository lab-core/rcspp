// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Tests for SimpleDominanceAlgorithm, AStarDominanceAlgorithm, TrivialCostFunction,
// and related dominance infrastructure using simple ResourceGraph<RealResource> graphs.

#include <gtest/gtest.h>

#include <memory>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace {

// Named constants shared across dominance-algorithm tests.
constexpr double kCostA01 = -2.0;
constexpr double kCostA13 = -4.0;
constexpr double kCostA02 = -1.0;
constexpr double kCostA23 = -3.0;
constexpr double kBestCost = -6.0;
constexpr double kAltCost = -4.0;
constexpr double kSingleCost = -5.0;
constexpr double kSingleArc1 = -2.0;
constexpr double kSingleArc2 = -3.0;
constexpr double kDomVal42 = 42.0;

/// @brief Single-path graph: source(0)->mid(1)->sink(2).
///
/// Resource values equal arc costs so ValueCostFunction gives total -5.
///
/// @return Owning pointer to the configured graph.
inline std::unique_ptr<ResourceGraph<RealResource>> make_dom_linear_graph() {
    auto g = std::make_unique<ResourceGraph<RealResource>>();
    g->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                  std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                  std::make_unique<ValueCostFunction<RealResource>>(),
                                  std::make_unique<ValueDominanceFunction<RealResource>>());
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/false);
    g->add_node(2, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(kSingleArc1), 0, 1);
    g->add_arc<RealResource>(std::make_tuple(kSingleArc2), 1, 2);
    return g;
}

/// @brief Two-path graph: 0->1->3 (cost -6, best) and 0->2->3 (cost -4).
///
/// @return Owning pointer to the configured graph.
inline std::unique_ptr<ResourceGraph<RealResource>> make_dom_two_path_graph() {
    auto g = std::make_unique<ResourceGraph<RealResource>>();
    g->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                  std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                  std::make_unique<ValueCostFunction<RealResource>>(),
                                  std::make_unique<ValueDominanceFunction<RealResource>>());
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/false);
    g->add_node(2, /*source=*/false, /*sink=*/false);
    g->add_node(3, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(kCostA01), 0, 1);
    g->add_arc<RealResource>(std::make_tuple(kCostA13), 1, 3);
    g->add_arc<RealResource>(std::make_tuple(kCostA02), 0, 2);
    g->add_arc<RealResource>(std::make_tuple(kCostA23), 2, 3);
    return g;
}

/// @brief Graph with source only (no arcs, no sink) → no feasible path.
inline std::unique_ptr<ResourceGraph<RealResource>> make_dom_no_path_graph() {
    auto g = std::make_unique<ResourceGraph<RealResource>>();
    g->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                  std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                  std::make_unique<ValueCostFunction<RealResource>>(),
                                  std::make_unique<ValueDominanceFunction<RealResource>>());
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/true);
    return g;
}

/// @brief Graph with a trivial cost function instead of value-based cost.
///
/// Uses TrivialCostFunction (always returns 0.0) so solution cost is 0.
/// This exercises the TrivialCostFunction::get_cost code path.
///
/// @return Owning pointer to the configured graph.
inline std::unique_ptr<ResourceGraph<RealResource>> make_trivial_cost_graph() {
    auto g = std::make_unique<ResourceGraph<RealResource>>();
    g->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                  std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                  std::make_unique<TrivialCostFunction<RealResource>>(),
                                  std::make_unique<ValueDominanceFunction<RealResource>>());
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(1.0), 0, 1);
    return g;
}

}  // namespace

// ============================================================================
// TrivialCostFunction tests
// ============================================================================

/// @brief TrivialCostFunction::get_cost returns 0 for any resource value.
TEST(TrivialCostFunction, GetCostReturnsZero) {
    TrivialCostFunction<RealResource> fn;
    RealResource r;
    r.set_value(kDomVal42);
    EXPECT_NEAR(fn.get_cost(r), 0.0, 1e-12);
}

/// @brief Solve using TrivialCostFunction — solution cost is always 0.
TEST(TrivialCostFunction, SolveWithTrivialCost) {
    auto g = make_trivial_cost_graph();
    const auto result = g->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions[0].cost, 0.0, 1e-12);
}

// ============================================================================
// SimpleDominanceAlgorithm tests
// ============================================================================

/// @brief SimpleDominanceAlgorithm finds the optimal path on a single-path graph.
TEST(SimpleDominanceAlgorithmTest, SinglePathFindsOptimal) {
    auto g = make_dom_linear_graph();
    const auto result = g->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions[0].cost, kSingleCost, 1e-9);
}

/// @brief SimpleDominanceAlgorithm on a single-path graph reports COMPLETE status.
TEST(SimpleDominanceAlgorithmTest, SinglePathStatusComplete) {
    auto g = make_dom_linear_graph();
    const auto result = g->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    EXPECT_EQ(result.status, AlgorithmStatus::COMPLETE);
}

/// @brief SimpleDominanceAlgorithm finds the best path on a two-path graph.
TEST(SimpleDominanceAlgorithmTest, TwoPathFindsOptimal) {
    auto g = make_dom_two_path_graph();
    const auto result = g->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions[0].cost, kBestCost, 1e-9);
}

/// @brief SimpleDominanceAlgorithm: the dominated solution is pruned.
///
/// With ValueDominanceFunction, the label at sink with cost -6 dominates the
/// label with cost -4, so only the better solution should appear.
TEST(SimpleDominanceAlgorithmTest, DominatedSolutionPruned) {
    auto g = make_dom_two_path_graph();
    const auto result = g->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    // The non-dominated path cost is -6; the -4 path is dominated.
    EXPECT_EQ(result.solutions.size(), 1U);
}

/// @brief SimpleDominanceAlgorithm returns empty when no path exists.
TEST(SimpleDominanceAlgorithmTest, NoPathReturnsEmpty) {
    auto g = make_dom_no_path_graph();
    const auto result = g->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    EXPECT_TRUE(result.solutions.empty());
}

/// @brief stop_after_X_solutions=1 on two-path graph gives MAX_SOLUTIONS status.
TEST(SimpleDominanceAlgorithmTest, StopAfterOneSolution) {
    auto g = make_dom_two_path_graph();
    AlgorithmBaseParams params;
    params.stop_after_X_solutions = 1;
    const auto result = g->solve<SimpleDominanceAlgorithm>(params);
    EXPECT_LE(result.solutions.size(), 1U);
}

/// @brief Timeout=0 returns TIMEOUT status.
TEST(SimpleDominanceAlgorithmTest, ZeroTimeoutReportsTimeout) {
    auto g = make_dom_two_path_graph();
    AlgorithmBaseParams params;
    params.timeout_s = 0.0;
    const auto result = g->solve<SimpleDominanceAlgorithm>(params);
    EXPECT_EQ(result.status, AlgorithmStatus::TIMEOUT);
}

/// @brief return_dominated_solutions=true includes non-optimal solutions.
TEST(SimpleDominanceAlgorithmTest, ReturnDominatedSolutions) {
    auto g = make_dom_two_path_graph();
    AlgorithmBaseParams params;
    params.return_dominated_solutions = true;
    params.stop_after_X_solutions = 2;
    const auto result = g->solve<SimpleDominanceAlgorithm>(params);
    EXPECT_GE(result.solutions.size(), 1U);
}

/// @brief upper_bound prunes solutions whose cost exceeds the bound.
TEST(SimpleDominanceAlgorithmTest, UpperBoundPrunePoorSolution) {
    auto g = make_dom_two_path_graph();
    // upper_bound=-5 means: only accept cost <= -5. Path 0->2->3 (cost -4) is pruned.
    // Path 0->1->3 (cost -6) is accepted.
    AlgorithmBaseParams params;
    const auto result = g->solve<SimpleDominanceAlgorithm>(params, /*upper_bound=*/-5.0);
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions[0].cost, kBestCost, 1e-9);
}

// ============================================================================
// AStarDominanceAlgorithm tests
// ============================================================================

/// @brief AStarDominanceAlgorithm finds the optimal path on a single-path graph.
TEST(AStarDominanceAlgorithmTest, SinglePathFindsOptimal) {
    auto g = make_dom_linear_graph();
    const auto result = g->solve<AStarAlgoBound<RealResource>::Algo>(AlgorithmBaseParams{});
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions[0].cost, kSingleCost, 1e-9);
}

/// @brief AStarDominanceAlgorithm on two-path graph finds the best path.
TEST(AStarDominanceAlgorithmTest, TwoPathFindsOptimal) {
    auto g = make_dom_two_path_graph();
    const auto result = g->solve<AStarAlgoBound<RealResource>::Algo>(AlgorithmBaseParams{});
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions[0].cost, kBestCost, 1e-9);
}

/// @brief AStarDominanceAlgorithm: COMPLETE status on single-path graph.
TEST(AStarDominanceAlgorithmTest, StatusComplete) {
    auto g = make_dom_linear_graph();
    const auto result = g->solve<AStarAlgoBound<RealResource>::Algo>(AlgorithmBaseParams{});
    EXPECT_EQ(result.status, AlgorithmStatus::COMPLETE);
}

/// @brief AStarDominanceAlgorithm: zero timeout gives TIMEOUT status.
TEST(AStarDominanceAlgorithmTest, ZeroTimeoutReportsTimeout) {
    auto g = make_dom_two_path_graph();
    AlgorithmBaseParams params;
    params.timeout_s = 0.0;
    const auto result = g->solve<AStarAlgoBound<RealResource>::Algo>(params);
    EXPECT_EQ(result.status, AlgorithmStatus::TIMEOUT);
}

/// @brief AStarDominanceAlgorithm: no path → empty result.
TEST(AStarDominanceAlgorithmTest, NoPathReturnsEmpty) {
    auto g = make_dom_no_path_graph();
    const auto result = g->solve<AStarAlgoBound<RealResource>::Algo>(AlgorithmBaseParams{});
    EXPECT_TRUE(result.solutions.empty());
}
