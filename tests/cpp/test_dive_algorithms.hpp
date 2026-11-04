// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Tests for strategy::Greedy, strategy::Tabu, and BacktrackingDiveAlgorithm.
// Each test builds a small ResourceGraph<RealResource> from a unique_ptr to
// avoid the deleted copy/move constructors of ResourceGraph.

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <vector>

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
// strategy::Greedy tests
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

/// @brief With prune_by_incumbent, Greedy drops a label that cannot lead to a path cheaper than
///        the incumbent (its cost plus its completion bound reaches it), as every labelling search
///        does; without it, it returns both paths under the upper bound.
///
/// Under the upper bound -1 both paths (-6 and -4) qualify. The source label costs 0, but its
/// bound is -6, so it is kept. Greedy finds -6 first; the label at 2 (cost -1, bound -3) then
/// reaches the incumbent and is dropped, so only the optimum is returned.
TEST(GreedyAlgorithmTest, PruneByIncumbentDropsALabelThatCannotImprove) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    EXPECT_EQ(g->solve<strategy::Greedy>(params, -1.0).solutions.size(), 2U);
    params.prune_by_incumbent = true;
    const auto pruned = g->solve<strategy::Greedy>(params, -1.0);
    ASSERT_EQ(pruned.solutions.size(), 1U);
    EXPECT_NEAR(pruned.solutions[0].cost, kBestPathCost, 1e-9);
    EXPECT_EQ(pruned.solutions.size(), g->solve<strategy::Simple>(params, -1.0).solutions.size());
}

/// @brief Greedy counts its iterations in labels taken: with max_iterations 2 it takes the source
///        and the cheaper child, and stops before it reaches the sink.
TEST(GreedyAlgorithmTest, MaxIterationsCountsTheLabelsTaken) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = 2;
    const auto two = g->solve<strategy::Greedy>(params);
    EXPECT_TRUE(two.solutions.empty());
    EXPECT_EQ(two.status, AlgorithmStatus::Truncated);
    params.max_iterations = 3;
    const auto three = g->solve<strategy::Greedy>(params);
    ASSERT_EQ(three.solutions.size(), 1U);
    EXPECT_NEAR(three.solutions[0].cost, kBestPathCost, 1e-9);
}

/// @brief Greedy applies the per-node quota, as every labelling search does: with one label taken
///        per node, the second path's label at the sink is stored aside for a next phase that
///        max_phases 1 does not run.
TEST(GreedyAlgorithmTest, AppliesThePerNodeQuota) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_extended_labels_per_node = 1;
    const auto result = g->solve<strategy::Greedy>(params);
    ASSERT_EQ(result.solutions.size(), 1U);
    EXPECT_NEAR(result.solutions[0].cost, kBestPathCost, 1e-9);
    EXPECT_EQ(result.status, AlgorithmStatus::Truncated);
}

/// @brief GreedyAlgorithm finds both solutions when max solutions is large.
TEST(GreedyAlgorithmTest, TwoPathGraphFindsBothSolutions) {
    auto g = make_two_path_graph();
    const auto result = g->solve<strategy::Greedy>(AlgorithmBaseParams{});
    // Both paths are distinct, so both solutions should be returned.
    EXPECT_EQ(result.solutions.size(), 2U);
}

/// @brief A sink Greedy reaches by backtracking to a sibling is recorded.
///
/// From the source, the cheaper child (node 1) is a dead end; its sibling is the sink. Greedy
/// dives to node 1, backtracks to the sink, and must record the path 0 -> 2 before it moves on.
TEST(GreedyAlgorithmTest, RecordsASinkReachedByBacktracking) {
    auto g = std::make_unique<ResourceGraph<RealResource>>();
    g->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                  std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                  std::make_unique<ValueCostFunction<RealResource>>(),
                                  std::make_unique<ValueDominanceFunction<RealResource>>());
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/false);
    g->add_node(2, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(kArc01Cost), 0, 1);  // arc 0: a dead end
    g->add_arc<RealResource>(std::make_tuple(kArc02Cost), 0, 2);  // arc 1: to the sink
    const auto result = g->solve<strategy::Greedy>(AlgorithmBaseParams{});
    ASSERT_EQ(result.solutions.size(), 1U);
    EXPECT_EQ(result.solutions[0].path_arc_ids, std::vector<size_t>{1});
    EXPECT_EQ(result.status, AlgorithmStatus::Complete);
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

// ============================================================================
// Drivers
// ============================================================================

namespace {

/// @brief How each run of the last solve driven by a test driver ended.
inline std::vector<parts::RunEnd>& driven_runs() {
    static std::vector<parts::RunEnd> runs;
    return runs;
}

/// @brief Ends each run at its first path, makes that path's arcs hard, and runs again until a
///        run finds none.
struct MaskEachPath {
        explicit MaskEachPath(const AlgorithmBaseParams& /*params*/) {}

        template <typename SearchType>
        bool begin(SearchType& search) {
            mask.reset(search.graph().get_arc_id_bound());
            search.use_arc_mask(&mask);
            search.end_runs_at_end_node();
            driven_runs().clear();
            return true;
        }

        template <typename SearchType>
        bool after_run(SearchType& search, size_t number_of_runs) {
            driven_runs().push_back(search.last_run());
            EXPECT_EQ(number_of_runs, driven_runs().size());
            if (!search.last_run().recorded) {
                return false;
            }
            search.lose_proof(ProofLoss::HeuristicDriver);
            for (const size_t arc_id : search.last_run().arc_ids) {
                mask.set(arc_id, ArcMask::State::Hard);
            }
            search.reseed();
            return true;
        }

        ArcMask mask;
};

/// @brief Refuses to run.
struct Refuses {
        explicit Refuses(const AlgorithmBaseParams& /*params*/) {}
        template <typename SearchType>
        bool begin(SearchType& /*search*/) {
            return false;
        }
        template <typename SearchType>
        bool after_run(SearchType& /*search*/, size_t /*number_of_runs*/) {
            ADD_FAILURE() << "a driver that refused to run is asked after a run";
            return false;
        }
};

/// @brief Ends the first run at its first path, and stops.
struct OneRun {
        explicit OneRun(const AlgorithmBaseParams& /*params*/) {}
        template <typename SearchType>
        bool begin(SearchType& search) {
            search.end_runs_at_end_node();
            return true;
        }
        template <typename SearchType>
        bool after_run(SearchType& /*search*/, size_t /*number_of_runs*/) {
            return false;
        }
};

/// @brief Greedy's parts, run by @p DriverType.
template <typename DriverType>
struct DrivenDive {
        template <typename ResourceType, typename Direction>
        using schedule = parts::DepthFirstSchedule<ResourceType>;
        template <typename ResourceType, typename Direction>
        using admission = parts::AdmitAll;
        template <typename ResourceType, typename Direction>
        using retention = parts::Retain<parts::KeepWhileWaiting>;
        using driver = DriverType;
        static constexpr bool backward = false;
};

}  // namespace

/// @brief A driver runs the search again and again: each run ends at its first path, whose arcs
///        it then masks, until a run finds none. The runs take no notice of max_iterations.
TEST(DrivenSearch, RunsTheSearchUntilTheDriverStops) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.max_iterations = 1;  // a driver's runs do not count the labels they take
    const auto result = g->solve<DrivenDive<MaskEachPath>>(params);
    ASSERT_EQ(result.solutions.size(), 2U);
    EXPECT_NEAR(result.solutions[0].cost, kBestPathCost, 1e-9);
    EXPECT_NEAR(result.solutions[1].cost, kAltPathCost, 1e-9);
    EXPECT_EQ(result.status, AlgorithmStatus::Complete);
    EXPECT_FALSE(result.proven);
    EXPECT_EQ(result.proof_lost_by, "heuristic_driver");

    const auto& runs = driven_runs();
    ASSERT_EQ(runs.size(), 3U);
    EXPECT_TRUE(runs[0].recorded);
    EXPECT_TRUE(runs[0].new_solution);
    EXPECT_NEAR(runs[0].cost, kBestPathCost, 1e-9);
    EXPECT_EQ(runs[0].arc_ids, (std::vector<size_t>{0, 1}));
    EXPECT_EQ(runs[1].arc_ids, (std::vector<size_t>{2, 3}));
    EXPECT_FALSE(runs[2].recorded);
    EXPECT_TRUE(runs[2].exhausted);
}

/// @brief A driver that refuses to run leaves the start labels waiting: nothing is extended, and
///        the solve is not complete.
TEST(DrivenSearch, ADriverThatRefusesRunsNothing) {
    auto g = make_two_path_graph();
    auto algorithm = g->create_algorithm<DrivenDive<Refuses>>(
        AlgorithmBaseParams{}.with_container<LabelList<ResourceTypeComposition<RealResource>>>());
    const auto result = g->solve(algorithm.get());
    EXPECT_TRUE(result.solutions.empty());
    EXPECT_EQ(algorithm->get_number_of_extended_labels(), 0U);
    EXPECT_EQ(result.status, AlgorithmStatus::Truncated);
}

/// @brief A run that ends at its first path leaves the labels still waiting: a driver that stops
///        there leaves a solve that is not complete.
TEST(DrivenSearch, ARunEndsAtItsFirstPath) {
    auto g = make_two_path_graph();
    const auto result = g->solve<DrivenDive<OneRun>>(AlgorithmBaseParams{});
    ASSERT_EQ(result.solutions.size(), 1U);
    EXPECT_NEAR(result.solutions[0].cost, kBestPathCost, 1e-9);
    EXPECT_EQ(result.status, AlgorithmStatus::Truncated);
    EXPECT_EQ(result.proof_lost_by, "truncated");
}

/// @brief A driven strategy does not search bidirectionally.
TEST(DrivenSearch, ADrivenStrategyDoesNotSearchBidirectionally) {
    auto g = make_two_path_graph();
    AlgorithmBaseParams params;
    params.direction = SearchDirection::Bidirectional;
    EXPECT_THROW((void)g->create_algorithm<DrivenDive<OneRun>>(
                     params.with_container<LabelList<ResourceTypeComposition<RealResource>>>()),
                 std::invalid_argument);
}
