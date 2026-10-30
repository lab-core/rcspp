// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Tests for GreedyAlgorithm, strategy::Tabu, and BacktrackingDiveAlgorithm.
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
    const auto result = g->solve<strategy::Greedy>(AlgorithmBaseParams{});
    ASSERT_FALSE(result.solutions.empty()) << "Greedy must find the single path";
    EXPECT_NEAR(result.solutions[0].cost, kLinearPathCost, 1e-9);
}

/// @brief GreedyAlgorithm on single-path graph reports Complete status.
TEST(GreedyAlgorithmTest, SinglePathGraphStatusComplete) {
    auto g = make_linear_graph();
    const auto result = g->solve<strategy::Greedy>(AlgorithmBaseParams{});
    EXPECT_EQ(result.status, AlgorithmStatus::Complete);
}

/// @brief GreedyAlgorithm on single-path graph visits nodes in order.
TEST(GreedyAlgorithmTest, SinglePathGraphNodeSequence) {
    auto g = make_linear_graph();
    const auto result = g->solve<strategy::Greedy>(AlgorithmBaseParams{});
    ASSERT_FALSE(result.solutions.empty());
    ASSERT_EQ(result.solutions[0].path_node_ids.size(), 3U);
    EXPECT_EQ(result.solutions[0].path_node_ids[0], 0U);
    EXPECT_EQ(result.solutions[0].path_node_ids[1], 1U);
    EXPECT_EQ(result.solutions[0].path_node_ids[2], 2U);
}

/// @brief GreedyAlgorithm on a two-path graph returns at least one solution.
TEST(GreedyAlgorithmTest, TwoPathGraphNonEmpty) {
    auto g = make_two_path_graph();
    const auto result = g->solve<strategy::Greedy>(AlgorithmBaseParams{});
    EXPECT_FALSE(result.solutions.empty());
}

/// @brief GreedyAlgorithm picks the lower-cost first arc and finds the best path.
///
/// On the two-path graph, greedy sorts children by label cost and picks
/// the arc with cost -2 (to node 1) before the arc with cost -1 (to node 2),
/// so the best solution 0->1->3 (total -6) is found first.
TEST(GreedyAlgorithmTest, TwoPathGraphPicksBestFirst) {
    auto g = make_two_path_graph();
    const auto result = g->solve<strategy::Greedy>(AlgorithmBaseParams{});
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions[0].cost, kBestPathCost, 1e-9);
}

/// @brief max_solutions = 1 stops at the first solution.
TEST(GreedyAlgorithmTest, StopAfterOneSolution) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_solutions = 1;
    const auto result = g->solve<strategy::Greedy>(params);
    EXPECT_EQ(result.solutions.size(), 1U);
    EXPECT_EQ(result.status, AlgorithmStatus::MaxSolutions);
}

/// @brief max_solutions=1 returns at most one solution.
TEST(GreedyAlgorithmTest, StopAfterOneSolutionBound) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_solutions = 1;
    const auto result = g->solve<strategy::Greedy>(params);
    EXPECT_LE(result.solutions.size(), 1U);
}

/// @brief A 0-second timeout returns Timeout status immediately.
TEST(GreedyAlgorithmTest, ZeroTimeoutReportsTimeout) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.timeout_s = 0.0;
    const auto result = g->solve<strategy::Greedy>(params);
    EXPECT_EQ(result.status, AlgorithmStatus::Timeout);
}

/// @brief GreedyAlgorithm with prune_by_incumbent covers that branch.
TEST(GreedyAlgorithmTest, PruneBasedOnUpperBound) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.prune_by_incumbent = true;
    const auto result = g->solve<strategy::Greedy>(params, /*upper_bound=*/-1.0);
    // upper_bound=-1 prunes all solutions (all paths have cost <= -4 < -1 is false)
    // Actually solutions cost -6 and -4, so upper_bound=-1: -6 < -1 yes, so the
    // solution is accepted. Both paths have cost < -1, so at least one solution found.
    EXPECT_FALSE(result.solutions.empty());
}

/// @brief GreedyAlgorithm finds both solutions when max solutions is large.
TEST(GreedyAlgorithmTest, TwoPathGraphFindsBothSolutions) {
    auto g = make_two_path_graph();
    const auto result = g->solve<strategy::Greedy>(AlgorithmBaseParams{});
    // Both paths are distinct, so both solutions should be returned.
    EXPECT_EQ(result.solutions.size(), 2U);
}

// ============================================================================
// TabuSearchAlgorithm tests
// ============================================================================

/// @brief TabuSearchAlgorithm with default params (max_iterations=kNoLimit)
///        returns immediately with empty result and logs an error.
TEST(TabuSearchAlgorithmTest, DefaultParamsReturnsEmpty) {
    auto g = make_linear_graph();
    // Default AlgorithmBaseParams has max_iterations = kNoLimit, which triggers
    // the early-return guard in TabuSearchAlgorithm::main_loop.
    const auto result = g->solve<strategy::Tabu>(AlgorithmBaseParams{});
    EXPECT_TRUE(result.solutions.empty())
        << "TabuSearch with kNoLimit iterations must return empty";
}

/// @brief TabuSearchAlgorithm with finite max_iterations finds the path.
TEST(TabuSearchAlgorithmTest, FiniteIterationsFindsSolution) {
    auto g = make_linear_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    const auto result = g->solve<strategy::Tabu>(params);
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
    const auto result = g->solve<strategy::Tabu>(params);
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
    const auto result = g->solve<strategy::Tabu>(params);
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
    const auto result = g->solve<strategy::Tabu>(params);
    // Should still find a solution (same path via aspiration).
    EXPECT_FALSE(result.solutions.empty());
}

/// @brief TabuSearch: max_solutions=1 stops after the first solution.
TEST(TabuSearchAlgorithmTest, StopAfterOneSolution) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    params.max_solutions = 1;
    const auto result = g->solve<strategy::Tabu>(params);
    EXPECT_EQ(result.solutions.size(), 1U);
    EXPECT_EQ(result.status, AlgorithmStatus::MaxSolutions);
}

/// @brief TabuSearch: a 0-second timeout returns Timeout status.
TEST(TabuSearchAlgorithmTest, ZeroTimeoutReportsTimeout) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    params.timeout_s = 0.0;
    const auto result = g->solve<strategy::Tabu>(params);
    EXPECT_EQ(result.status, AlgorithmStatus::Timeout);
}

/// @brief TabuSearch: return_dominated_solutions = true includes all solutions.
TEST(TabuSearchAlgorithmTest, ReturnDominatedSolutions) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    params.return_dominated_solutions = true;
    params.max_solutions = 2;
    const auto result = g->solve<strategy::Tabu>(params);
    EXPECT_GE(result.solutions.size(), 1U);
}

/// @brief TabuSearch: tabu_exempt_node_ids excludes source/sink arcs from tabu list.
TEST(TabuSearchAlgorithmTest, ForbiddenTabuExcludesArcs) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    params.tabu_exempt_node_ids = {0U, 3U};  // source and sink node ids
    const auto result = g->solve<strategy::Tabu>(params);
    EXPECT_FALSE(result.solutions.empty());
}

// ============================================================================
// ImprovingTabuSearch tests
// ============================================================================

/// @brief ImprovingTabuSearch with kNoLimit max_iterations returns empty.
TEST(ImprovingTabuSearchTest, DefaultMaxIterationsReturnsEmpty) {
    auto g = make_linear_graph();
    const auto result = g->solve<strategy::ImprovingTabu>(AlgorithmBaseParams{});
    EXPECT_TRUE(result.solutions.empty());
}

/// @brief ImprovingTabuSearch finds the single path on a linear graph.
TEST(ImprovingTabuSearchTest, SinglePathFindsOptimal) {
    auto g = make_linear_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    const auto result = g->solve<strategy::ImprovingTabu>(params);
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions[0].cost, kLinearPathCost, 1e-9);
}

/// @brief ImprovingTabuSearch finds solutions on a two-path graph.
TEST(ImprovingTabuSearchTest, TwoPathFindsSolutions) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    const auto result = g->solve<strategy::ImprovingTabu>(params);
    EXPECT_FALSE(result.solutions.empty());
}

/// @brief ImprovingTabuSearch: zero timeout → Timeout status.
TEST(ImprovingTabuSearchTest, ZeroTimeoutReportsTimeout) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    params.timeout_s = 0.0;
    const auto result = g->solve<strategy::ImprovingTabu>(params);
    EXPECT_EQ(result.status, AlgorithmStatus::Timeout);
}

/// @brief ImprovingTabuSearch: max_solutions=1 stops early.
TEST(ImprovingTabuSearchTest, StopAfterOneSolution) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    params.max_solutions = 1;
    const auto result = g->solve<strategy::ImprovingTabu>(params);
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
    const auto result = g->solve<strategy::ImprovingTabu>(params);
    EXPECT_TRUE(result.solutions.empty());
}

/// @brief ImprovingTabuSearch: return_dominated_solutions includes all solutions.
TEST(ImprovingTabuSearchTest, ReturnDominatedSolutions) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    params.return_dominated_solutions = true;
    params.max_solutions = 3;
    const auto result = g->solve<strategy::ImprovingTabu>(params);
    EXPECT_GE(result.solutions.size(), 1U);
}

/// @brief ImprovingTabuSearch: best path (-6) found within iterations.
TEST(ImprovingTabuSearchTest, TwoPathFindsBestPath) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters * 2U;
    const auto result = g->solve<strategy::ImprovingTabu>(params);
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions[0].cost, kBestPathCost, 1e-9);
}

// ============================================================================
// DiversificationSearch tests
// ============================================================================

/// @brief DiversificationSearch with kNoLimit max_iterations returns empty.
TEST(DiversificationSearchTest, DefaultMaxIterationsReturnsEmpty) {
    auto g = make_linear_graph();
    const auto result = g->solve<strategy::Diversification<>>(AlgorithmBaseParams{});
    EXPECT_TRUE(result.solutions.empty());
}

/// @brief DiversificationSearch finds the single path on a linear graph.
TEST(DiversificationSearchTest, SinglePathFindsSolution) {
    auto g = make_linear_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kDivItersSmall;
    const auto result = g->solve<strategy::Diversification<>>(params);
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions[0].cost, kLinearPathCost, 1e-9);
}

/// @brief DiversificationSearch finds solutions on a two-path graph.
TEST(DiversificationSearchTest, TwoPathFindsSolutions) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kDivIters;
    const auto result = g->solve<strategy::Diversification<>>(params);
    EXPECT_FALSE(result.solutions.empty());
}

/// @brief DiversificationSearch: zero timeout → Timeout status.
TEST(DiversificationSearchTest, ZeroTimeoutReportsTimeout) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kDivIters;
    params.timeout_s = 0.0;
    const auto result = g->solve<strategy::Diversification<>>(params);
    EXPECT_EQ(result.status, AlgorithmStatus::Timeout);
}

/// @brief DiversificationSearch: max_solutions=1 stops after first solution.
TEST(DiversificationSearchTest, StopAfterOneSolution) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kDivIters;
    params.max_solutions = 1;
    const auto result = g->solve<strategy::Diversification<>>(params);
    EXPECT_LE(result.solutions.size(), 1U);
}

/// @brief DiversificationSearch: tabu_exempt_node_ids preserves arcs involving those nodes.
TEST(DiversificationSearchTest, ForbiddenTabuExcludesArcs) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kDivIters;
    params.tabu_exempt_node_ids = {0U, 3U};  // source and sink
    const auto result = g->solve<strategy::Diversification<>>(params);
    EXPECT_FALSE(result.solutions.empty());
}

// ============================================================================
// The status of the tabu searches
// ============================================================================

namespace {

/// @brief A graph whose paths all dead-end: @p layers layers of two nodes, each node joined to
///        both nodes of the next layer, and a sink no arc enters.
///
/// A dive tries all 2^layers paths before it runs out, so with 12 layers it passes the dives'
/// check of the stop conditions, made every 4096 steps.
///
/// @param layers The number of layers.
/// @return Owning pointer to the configured ResourceGraph.
inline std::unique_ptr<ResourceGraph<RealResource>> make_dead_end_graph(size_t layers) {
    auto g = std::make_unique<ResourceGraph<RealResource>>();
    g->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                  std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                  std::make_unique<ValueCostFunction<RealResource>>(),
                                  std::make_unique<ValueDominanceFunction<RealResource>>());
    g->add_node(0, /*source=*/true, /*sink=*/false);
    for (size_t node_id = 1; node_id <= 2 * layers; ++node_id) {
        g->add_node(node_id);
    }
    g->add_node(2 * layers + 1, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(1.0), 0, 1);
    g->add_arc<RealResource>(std::make_tuple(1.0), 0, 2);
    for (size_t layer = 1; layer < layers; ++layer) {
        for (const size_t from : {2 * layer - 1, 2 * layer}) {
            g->add_arc<RealResource>(std::make_tuple(1.0), from, 2 * layer + 1);
            g->add_arc<RealResource>(std::make_tuple(1.0), from, 2 * layer + 2);
        }
    }
    return g;
}

constexpr size_t kDeadEndLayers = 12U;  // 4096 paths: more steps than one check interval

/// @brief Params that interrupt a solve once it has looked at should_stop @p looks_before times,
///        so that the next look, inside the first dive, stops it.
///
/// Before its first dive, TabuSearchAlgorithm looks twice (the solve's loop and its own), and
/// ImprovingTabuSearch once (the solve's loop: its construction dive comes before its loop).
///
/// @param looks_before The looks to let through.
/// @param calls        Counts the looks.
inline AlgorithmBaseParams interrupted_inside_the_first_dive(size_t looks_before, size_t* calls) {
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    params.should_stop = [looks_before, calls]() { return ++*calls > looks_before; };
    return params;
}

}  // namespace

/// @brief A tabu search that runs out of paths, with no arc tabu, has proved there is none.
TEST(TabuStatusTest, TabuThatRunsOutOfPathsIsComplete) {
    auto g = make_dead_end_graph(3);
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    auto algorithm = g->create_algorithm<strategy::Tabu>(
        params.with_container<LabelList<ResourceTypeComposition<RealResource>>>());
    const auto result = g->solve(algorithm.get());
    EXPECT_TRUE(result.solutions.empty());
    EXPECT_EQ(result.status, AlgorithmStatus::Complete);
    EXPECT_TRUE(algorithm->is_optimal());
}

/// @brief An interrupt inside the first dive is an interrupt, not "no feasible path".
///
/// The dive stops with paths left to try. The tabu list is still empty, which used to read as
/// a proof that no path exists: the status was Complete and is_optimal() true.
TEST(TabuStatusTest, TabuInterruptedInItsFirstDiveIsInterrupted) {
    auto g = make_dead_end_graph(kDeadEndLayers);
    size_t calls = 0;
    auto algorithm = g->create_algorithm<strategy::Tabu>(
        interrupted_inside_the_first_dive(2, &calls)
            .with_container<LabelList<ResourceTypeComposition<RealResource>>>());
    const auto result = g->solve(algorithm.get());
    EXPECT_GT(calls, 2U) << "the interrupt did not fire";
    EXPECT_EQ(result.status, AlgorithmStatus::Interrupted);
    EXPECT_FALSE(algorithm->is_optimal());
}

/// @brief The same for ImprovingTabuSearch, whose first dive is its construction phase.
TEST(TabuStatusTest, ImprovingTabuInterruptedInItsFirstDiveIsInterrupted) {
    auto g = make_dead_end_graph(kDeadEndLayers);
    size_t calls = 0;
    const auto result =
        g->solve<strategy::ImprovingTabu>(interrupted_inside_the_first_dive(1, &calls));
    EXPECT_GT(calls, 1U) << "the interrupt did not fire";
    EXPECT_EQ(result.status, AlgorithmStatus::Interrupted);
}

/// @brief ImprovingTabuSearch that spends its iteration budget is not Complete.
///
/// Each improvement dive cleared its path, so a search ended by its budget used to report
/// Complete. It now reports Truncated, as TabuSearchAlgorithm does.
TEST(TabuStatusTest, ImprovingTabuThatSpendsItsIterationsIsTruncated) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    const auto result = g->solve<strategy::ImprovingTabu>(params);
    EXPECT_FALSE(result.solutions.empty());
    EXPECT_EQ(result.status, AlgorithmStatus::Truncated);
}

/// @brief ImprovingTabuSearch that reaches max_solutions reports MaxSolutions, not Complete.
TEST(TabuStatusTest, ImprovingTabuThatReachesMaxSolutionsIsMaxSolutions) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    params.max_solutions = 1;
    const auto result = g->solve<strategy::ImprovingTabu>(params);
    EXPECT_EQ(result.solutions.size(), 1U);
    EXPECT_EQ(result.status, AlgorithmStatus::MaxSolutions);
}

/// @brief ImprovingTabuSearch that runs out of paths in its first dive is Complete.
TEST(TabuStatusTest, ImprovingTabuThatRunsOutOfPathsIsComplete) {
    auto g = make_dead_end_graph(3);
    AlgorithmBaseParams params;
    params.max_iterations = kTabuIters;
    const auto result = g->solve<strategy::ImprovingTabu>(params);
    EXPECT_TRUE(result.solutions.empty());
    EXPECT_EQ(result.status, AlgorithmStatus::Complete);
}
