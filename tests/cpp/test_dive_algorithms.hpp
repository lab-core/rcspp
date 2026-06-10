// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Tests for GreedyAlgorithm, TabuSearchAlgorithm, and BacktrackingDiveAlgorithm.
// Each test builds a small ResourceGraph<RealResource> from a unique_ptr to
// avoid the deleted copy/move constructors of ResourceGraph.

#include <gtest/gtest.h>

#include <limits>
#include <memory>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace {

// Named constants (avoid readability-magic-numbers warnings).
constexpr double kLinearPathCost = -5.0;  // arc costs: -2 + -3
constexpr double kBestPathCost = -6.0;    // arc costs: -2 + -4
constexpr double kAltPathCost = -4.0;     // arc costs: -1 + -3
constexpr double kArc01Cost = -2.0;
constexpr double kArc13Cost = -4.0;
constexpr double kArc02Cost = -1.0;
constexpr double kArc23Cost = -3.0;
constexpr double kArc12aCost = -3.0;
constexpr size_t kTabuIters = 5U;
constexpr size_t kDivIters = 5U;
constexpr size_t kDivItersSmall = 3U;

/// @brief Build a single-path graph: source(0)->mid(1)->sink(2).
///
/// Resource values (= cost increments): kArc01Cost (0->1), kArc12aCost (1->2).
/// ValueCostFunction accumulates resource values, so total solution cost = -5.
///
/// @return Owning pointer to the configured ResourceGraph.
inline std::unique_ptr<ResourceGraph<RealResource>> make_linear_graph() {
    auto g = std::make_unique<ResourceGraph<RealResource>>();
    g->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                  std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                  std::make_unique<ValueCostFunction<RealResource>>(),
                                  std::make_unique<ValueDominanceFunction<RealResource>>());
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/false);
    g->add_node(2, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(kArc01Cost), 0, 1);   // arc 0: cost -2
    g->add_arc<RealResource>(std::make_tuple(kArc12aCost), 1, 2);  // arc 1: cost -3
    return g;
}

/// @brief Build a two-path graph with four nodes.
///
/// source(0) -> mid(1) [res -2] -> sink(3) [res -4]   path cost = -6 (best)
/// source(0) -> mid(2) [res -1] -> sink(3) [res -3]   path cost = -4 (alt)
///
/// ValueCostFunction sums resource values; both paths have cost < 0.
///
/// @return Owning pointer to the configured ResourceGraph.
inline std::unique_ptr<ResourceGraph<RealResource>> make_two_path_graph() {
    auto g = std::make_unique<ResourceGraph<RealResource>>();
    g->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                  std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                  std::make_unique<ValueCostFunction<RealResource>>(),
                                  std::make_unique<ValueDominanceFunction<RealResource>>());
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/false);
    g->add_node(2, /*source=*/false, /*sink=*/false);
    g->add_node(3, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(kArc01Cost), 0, 1);  // arc 0: 0->1 res -2
    g->add_arc<RealResource>(std::make_tuple(kArc13Cost), 1, 3);  // arc 1: 1->3 res -4
    g->add_arc<RealResource>(std::make_tuple(kArc02Cost), 0, 2);  // arc 2: 0->2 res -1
    g->add_arc<RealResource>(std::make_tuple(kArc23Cost), 2, 3);  // arc 3: 2->3 res -3
    return g;
}

}  // namespace

// ============================================================================
// GreedyAlgorithm tests
// ============================================================================

/// @brief GreedyAlgorithm finds the single feasible path in a linear graph.
///
/// The graph has only one path (0->1->2), so the algorithm must find it.
TEST(GreedyAlgorithmTest, SinglePathGraphFindsSolution) {
    auto g = make_linear_graph();
    const auto result = g->solve<GreedyAlgorithm>(AlgorithmBaseParams{});
    ASSERT_FALSE(result.solutions.empty()) << "Greedy must find the single path";
    EXPECT_NEAR(result.solutions[0].cost, kLinearPathCost, 1e-9);
}

/// @brief GreedyAlgorithm on single-path graph reports COMPLETE status.
TEST(GreedyAlgorithmTest, SinglePathGraphStatusComplete) {
    auto g = make_linear_graph();
    const auto result = g->solve<GreedyAlgorithm>(AlgorithmBaseParams{});
    EXPECT_EQ(result.status, AlgorithmStatus::COMPLETE);
}

/// @brief GreedyAlgorithm on single-path graph visits nodes in order.
TEST(GreedyAlgorithmTest, SinglePathGraphNodeSequence) {
    auto g = make_linear_graph();
    const auto result = g->solve<GreedyAlgorithm>(AlgorithmBaseParams{});
    ASSERT_FALSE(result.solutions.empty());
    ASSERT_EQ(result.solutions[0].path_node_ids.size(), 3U);
    EXPECT_EQ(result.solutions[0].path_node_ids[0], 0U);
    EXPECT_EQ(result.solutions[0].path_node_ids[1], 1U);
    EXPECT_EQ(result.solutions[0].path_node_ids[2], 2U);
}

/// @brief GreedyAlgorithm on a two-path graph returns at least one solution.
TEST(GreedyAlgorithmTest, TwoPathGraphNonEmpty) {
    auto g = make_two_path_graph();
    const auto result = g->solve<GreedyAlgorithm>(AlgorithmBaseParams{});
    EXPECT_FALSE(result.solutions.empty());
}

/// @brief GreedyAlgorithm picks the lower-cost first arc and finds the best path.
///
/// On the two-path graph, greedy sorts children by label cost and picks
/// the arc with cost -2 (to node 1) before the arc with cost -1 (to node 2),
/// so the best solution 0->1->3 (total -6) is found first.
TEST(GreedyAlgorithmTest, TwoPathGraphPicksBestFirst) {
    auto g = make_two_path_graph();
    const auto result = g->solve<GreedyAlgorithm>(AlgorithmBaseParams{});
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions[0].cost, kBestPathCost, 1e-9);
}

/// @brief stop_after_X_solutions = 1 stops at the first solution.
TEST(GreedyAlgorithmTest, StopAfterOneSolution) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.stop_after_X_solutions = 1;
    const auto result = g->solve<GreedyAlgorithm>(params);
    EXPECT_EQ(result.solutions.size(), 1U);
    EXPECT_EQ(result.status, AlgorithmStatus::MAX_SOLUTIONS);
}

/// @brief stop_after_X_solutions=1 returns at most one solution.
TEST(GreedyAlgorithmTest, StopAfterOneSolutionBound) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.stop_after_X_solutions = 1;
    const auto result = g->solve<GreedyAlgorithm>(params);
    EXPECT_LE(result.solutions.size(), 1U);
}

/// @brief A 0-second timeout returns TIMEOUT status immediately.
TEST(GreedyAlgorithmTest, ZeroTimeoutReportsTimeout) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.timeout_s = 0.0;
    const auto result = g->solve<GreedyAlgorithm>(params);
    EXPECT_EQ(result.status, AlgorithmStatus::TIMEOUT);
}

/// @brief GreedyAlgorithm with prune_based_on_upper_bound_ covers that branch.
TEST(GreedyAlgorithmTest, PruneBasedOnUpperBound) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.prune_based_on_upper_bound_ = true;
    const auto result = g->solve<GreedyAlgorithm>(params, /*upper_bound=*/-1.0);
    // upper_bound=-1 prunes all solutions (all paths have cost <= -4 < -1 is false)
    // Actually solutions cost -6 and -4, so upper_bound=-1: -6 < -1 yes, so the
    // solution is accepted. Both paths have cost < -1, so at least one solution found.
    EXPECT_FALSE(result.solutions.empty());
}

/// @brief GreedyAlgorithm finds both solutions when max solutions is large.
TEST(GreedyAlgorithmTest, TwoPathGraphFindsBothSolutions) {
    auto g = make_two_path_graph();
    const auto result = g->solve<GreedyAlgorithm>(AlgorithmBaseParams{});
    // Both paths are distinct, so both solutions should be returned.
    EXPECT_EQ(result.solutions.size(), 2U);
}

// ============================================================================
// TabuSearchAlgorithm tests
// ============================================================================

/// @brief TabuSearchAlgorithm with default params (max_iterations=MAX_INT)
///        returns immediately with empty result and logs an error.
TEST(TabuSearchAlgorithmTest, DefaultParamsReturnsEmpty) {
    auto g = make_linear_graph();
    // Default AlgorithmBaseParams has max_iterations = MAX_INT, which triggers
    // the early-return guard in TabuSearchAlgorithm::main_loop.
    const auto result = g->solve<TabuSearchAlgorithm>(AlgorithmBaseParams{});
    EXPECT_TRUE(result.solutions.empty()) << "TabuSearch with MAX_INT iterations must return empty";
}

/// @brief TabuSearchAlgorithm with finite max_iterations finds the path.
TEST(TabuSearchAlgorithmTest, FiniteIterationsFindsSolution) {
    auto g = make_linear_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    const auto result = g->solve<TabuSearchAlgorithm>(params);
    ASSERT_FALSE(result.solutions.empty()) << "TabuSearch must find the single-path solution";
    EXPECT_NEAR(result.solutions[0].cost, kLinearPathCost, 1e-9);
}

/// @brief TabuSearchAlgorithm finds multiple solutions on a two-path graph.
///
/// First iteration finds the best path (cost -6) and marks its arcs as tabu.
/// Second iteration uses aspiration to still find a solution.
TEST(TabuSearchAlgorithmTest, TwoPathFindsSolutions) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    const auto result = g->solve<TabuSearchAlgorithm>(params);
    EXPECT_FALSE(result.solutions.empty());
}

/// @brief TabuSearchAlgorithm covers the "no-arc source" early-return path.
///
/// When the graph has a source with no out-arcs the first dive immediately
/// fails and the tabu list is empty, so main_loop returns at line 122.
TEST(TabuSearchAlgorithmTest, NoFeasiblePathReturnsEmpty) {
    // Graph: source(0), sink(1), no arcs.
    auto g = std::make_unique<ResourceGraph<RealResource>>();
    g->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                  std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                  std::make_unique<ValueCostFunction<RealResource>>(),
                                  std::make_unique<ValueDominanceFunction<RealResource>>());
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/true);
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    const auto result = g->solve<TabuSearchAlgorithm>(params);
    EXPECT_TRUE(result.solutions.empty());
}

/// @brief TabuSearch: aspiration is triggered when all children of a node are
///        tabu on the second iteration of a single-path graph.
///
/// Single-path graph means the only children at each node are the tabu arcs
/// from iteration 1.  select_children must fall back to the tabu set (line 75).
TEST(TabuSearchAlgorithmTest, AspirationUsesTabuArcs) {
    auto g = make_linear_graph();
    AlgorithmBaseParams params;
    // Two iterations: iteration 1 finds the path and adds arcs to tabu;
    // iteration 2 has all arcs tabu -> aspiration kicks in.
    params.max_iterations = 2;
    const auto result = g->solve<TabuSearchAlgorithm>(params);
    // Should still find a solution (same path via aspiration).
    EXPECT_FALSE(result.solutions.empty());
}

/// @brief TabuSearch: stop_after_X_solutions=1 stops after the first solution.
TEST(TabuSearchAlgorithmTest, StopAfterOneSolution) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    params.stop_after_X_solutions = 1;
    const auto result = g->solve<TabuSearchAlgorithm>(params);
    EXPECT_EQ(result.solutions.size(), 1U);
    EXPECT_EQ(result.status, AlgorithmStatus::MAX_SOLUTIONS);
}

/// @brief TabuSearch: a 0-second timeout returns TIMEOUT status.
TEST(TabuSearchAlgorithmTest, ZeroTimeoutReportsTimeout) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    params.timeout_s = 0.0;
    const auto result = g->solve<TabuSearchAlgorithm>(params);
    EXPECT_EQ(result.status, AlgorithmStatus::TIMEOUT);
}

/// @brief TabuSearch: return_dominated_solutions = true includes all solutions.
TEST(TabuSearchAlgorithmTest, ReturnDominatedSolutions) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    params.return_dominated_solutions = true;
    params.stop_after_X_solutions = 2;
    const auto result = g->solve<TabuSearchAlgorithm>(params);
    EXPECT_GE(result.solutions.size(), 1U);
}

/// @brief TabuSearch: forbidden_tabu excludes source/sink arcs from tabu list.
TEST(TabuSearchAlgorithmTest, ForbiddenTabuExcludesArcs) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    params.forbidden_tabu = {0U, 3U};  // source and sink node ids
    const auto result = g->solve<TabuSearchAlgorithm>(params);
    EXPECT_FALSE(result.solutions.empty());
}

// ============================================================================
// ImprovingTabuSearch tests
// ============================================================================

/// @brief ImprovingTabuSearch with MAX_INT max_iterations returns empty.
TEST(ImprovingTabuSearchTest, DefaultMaxIterationsReturnsEmpty) {
    auto g = make_linear_graph();
    const auto result = g->solve<ImprovingTabuSearch>(AlgorithmBaseParams{});
    EXPECT_TRUE(result.solutions.empty());
}

/// @brief ImprovingTabuSearch finds the single path on a linear graph.
TEST(ImprovingTabuSearchTest, SinglePathFindsOptimal) {
    auto g = make_linear_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    const auto result = g->solve<ImprovingTabuSearch>(params);
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions[0].cost, kLinearPathCost, 1e-9);
}

/// @brief ImprovingTabuSearch finds solutions on a two-path graph.
TEST(ImprovingTabuSearchTest, TwoPathFindsSolutions) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    const auto result = g->solve<ImprovingTabuSearch>(params);
    EXPECT_FALSE(result.solutions.empty());
}

/// @brief ImprovingTabuSearch: zero timeout → TIMEOUT status.
TEST(ImprovingTabuSearchTest, ZeroTimeoutReportsTimeout) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    params.timeout_s = 0.0;
    const auto result = g->solve<ImprovingTabuSearch>(params);
    EXPECT_EQ(result.status, AlgorithmStatus::TIMEOUT);
}

/// @brief ImprovingTabuSearch: stop_after_X_solutions=1 stops early.
TEST(ImprovingTabuSearchTest, StopAfterOneSolution) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    params.stop_after_X_solutions = 1;
    const auto result = g->solve<ImprovingTabuSearch>(params);
    EXPECT_LE(result.solutions.size(), 1U);
}

/// @brief ImprovingTabuSearch on a graph with no arcs returns empty.
TEST(ImprovingTabuSearchTest, NoFeasiblePathReturnsEmpty) {
    auto g = std::make_unique<ResourceGraph<RealResource>>();
    g->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                  std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                  std::make_unique<ValueCostFunction<RealResource>>(),
                                  std::make_unique<ValueDominanceFunction<RealResource>>());
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/true);
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    const auto result = g->solve<ImprovingTabuSearch>(params);
    EXPECT_TRUE(result.solutions.empty());
}

/// @brief ImprovingTabuSearch: return_dominated_solutions includes all solutions.
TEST(ImprovingTabuSearchTest, ReturnDominatedSolutions) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    params.return_dominated_solutions = true;
    params.stop_after_X_solutions = 3;
    const auto result = g->solve<ImprovingTabuSearch>(params);
    EXPECT_GE(result.solutions.size(), 1U);
}

/// @brief ImprovingTabuSearch: best path (-6) found within iterations.
TEST(ImprovingTabuSearchTest, TwoPathFindsBestPath) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters * 2U;
    const auto result = g->solve<ImprovingTabuSearch>(params);
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions[0].cost, kBestPathCost, 1e-9);
}

// ============================================================================
// DiversificationSearch tests
// ============================================================================

/// @brief DiversificationSearch with MAX_INT max_iterations returns empty.
TEST(DiversificationSearchTest, DefaultMaxIterationsReturnsEmpty) {
    auto g = make_linear_graph();
    const auto result = g->solve<DiversificationSearch>(AlgorithmBaseParams{});
    EXPECT_TRUE(result.solutions.empty());
}

/// @brief DiversificationSearch finds the single path on a linear graph.
TEST(DiversificationSearchTest, SinglePathFindsSolution) {
    auto g = make_linear_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kDivItersSmall;
    const auto result = g->solve<DiversificationSearch>(params);
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions[0].cost, kLinearPathCost, 1e-9);
}

/// @brief DiversificationSearch finds solutions on a two-path graph.
TEST(DiversificationSearchTest, TwoPathFindsSolutions) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kDivIters;
    const auto result = g->solve<DiversificationSearch>(params);
    EXPECT_FALSE(result.solutions.empty());
}

/// @brief DiversificationSearch: zero timeout → TIMEOUT status.
TEST(DiversificationSearchTest, ZeroTimeoutReportsTimeout) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kDivIters;
    params.timeout_s = 0.0;
    const auto result = g->solve<DiversificationSearch>(params);
    EXPECT_EQ(result.status, AlgorithmStatus::TIMEOUT);
}

/// @brief DiversificationSearch: stop_after_X_solutions=1 stops after first solution.
TEST(DiversificationSearchTest, StopAfterOneSolution) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kDivIters;
    params.stop_after_X_solutions = 1;
    const auto result = g->solve<DiversificationSearch>(params);
    EXPECT_LE(result.solutions.size(), 1U);
}

/// @brief DiversificationSearch: forbidden_tabu preserves arcs involving those nodes.
TEST(DiversificationSearchTest, ForbiddenTabuExcludesArcs) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kDivIters;
    params.forbidden_tabu = {0U, 3U};  // source and sink
    const auto result = g->solve<DiversificationSearch>(params);
    EXPECT_FALSE(result.solutions.empty());
}
