// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Tests for ShortestPathPreprocessor, FeasibilityPreprocessor, and
// BellmanFordAlgorithm.  Each test builds a small ResourceGraph<RealResource>
// directly and inspects which arcs survive after preprocessing.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace {

// Composition type alias.
using Comp = ResourceTypeComposition<RealResource>;

// Named constants for values used in multiple tests.
constexpr double kDefaultMaxBound = 1e9;
constexpr double kExpensiveCost = 9.0;
constexpr double kTightBound = 5.0;
constexpr double kLooseBound = 100.0;
constexpr double kNegCycleCost = -2.0;
constexpr double kThreeCost = 3.0;
constexpr double kTwoCost = 2.0;
constexpr double kSevenCost = 7.0;
constexpr double kFourCost = 4.0;
constexpr double kSixCost = 6.0;
constexpr double kFiftyVal = 50.0;
constexpr double kTwoHundredVal = 200.0;
constexpr double kTenBound = 10.0;

/// @brief Allocate a ResourceGraph<RealResource> with one additive cost resource.
///
/// The resource accumulates via addition, is feasible in [0, max_val],
/// and is dominated by value (lower is better).
///
/// @param max_val Upper bound of the feasibility window.
/// @return Owning pointer to the configured ResourceGraph.
inline std::unique_ptr<ResourceGraph<RealResource>> make_graph(double max_val = kDefaultMaxBound) {
    auto g = std::make_unique<ResourceGraph<RealResource>>();
    g->add_resource<RealResource>(
        std::make_unique<AdditionExtensionFunction<RealResource>>(),
        std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, max_val),
        std::make_unique<ValueCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    return g;
}

/// @brief Return the number of active arcs in the graph.
///
/// @param g The graph to inspect.
/// @return Count of arcs that have not been removed.
template <typename ResourceType>
inline size_t count_arcs(const Graph<ResourceType>& g) {
    size_t n = 0;
    g.for_each_arc([&](const auto& /*arc*/) { ++n; });
    return n;
}

}  // namespace

// ============================================================================
// ShortestPathPreprocessor tests
// ============================================================================

// ---------------------------------------------------------------------------
// Topology used by most ShortestPathPreprocessor tests:
//
//   source(0) --[cost 1]--> node(1) --[cost 1]--> sink(2)
//   source(0) --[cost 9]--> sink(2)
//
// Shortest path 0->2 = 2.0 (via node 1).
// Arc 0->2 direct has extender value 9, so full path cost = 0 + 9 + 0 = 9.
// ---------------------------------------------------------------------------

/// @brief ShortestPathPreprocessor removes arc 0->2 when upper_bound is tight.
///
/// With upper_bound = 5.0 the direct arc (extender value 9) cannot be part of
/// any path cheaper than 5, so it must be removed.  The two arcs of the
/// shorter path (1 + 1 = 2) survive.
TEST(ShortestPathPreprocessor, TightBoundRemovesExpensiveArc) {
    auto g = make_graph();
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/false);
    g->add_node(2, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(1.0), 0, 1, /*cost=*/1.0);
    g->add_arc<RealResource>(std::make_tuple(1.0), 1, 2, /*cost=*/1.0);
    g->add_arc<RealResource>(std::make_tuple(kExpensiveCost), 0, 2, /*cost=*/kExpensiveCost);

    ASSERT_EQ(count_arcs(*g), 3U);

    ShortestPathPreprocessor<RealResource, RealResource> spp(g.get(),
                                                             /*upper_bound=*/kTightBound,
                                                             /*cost_index=*/0);
    const bool removed_any = spp.preprocess();

    EXPECT_TRUE(removed_any);
    EXPECT_EQ(count_arcs(*g), 2U);
    EXPECT_NE(g->get_arc(0), nullptr);
    EXPECT_NE(g->get_arc(1), nullptr);
    EXPECT_EQ(g->get_arc(2), nullptr);
}

/// @brief ShortestPathPreprocessor removes no arc when upper_bound is loose.
///
/// With upper_bound = 100.0 every arc can participate in a sub-100-cost path,
/// so the preprocessor must leave all arcs intact.
TEST(ShortestPathPreprocessor, LooseBoundRemovesNoArc) {
    auto g = make_graph();
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/false);
    g->add_node(2, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(1.0), 0, 1, /*cost=*/1.0);
    g->add_arc<RealResource>(std::make_tuple(1.0), 1, 2, /*cost=*/1.0);
    g->add_arc<RealResource>(std::make_tuple(kExpensiveCost), 0, 2, /*cost=*/kExpensiveCost);

    ShortestPathPreprocessor<RealResource, RealResource> spp(g.get(),
                                                             /*upper_bound=*/kLooseBound,
                                                             /*cost_index=*/0);
    const bool removed_any = spp.preprocess();

    EXPECT_FALSE(removed_any);
    EXPECT_EQ(count_arcs(*g), 3U);
}

/// @brief ShortestPathPreprocessor disables itself when upper_bound is +inf.
///
/// An infinite upper bound means no bound is known yet; the preprocessor
/// disables itself entirely and all arcs must survive.
TEST(ShortestPathPreprocessor, InfiniteUpperBoundDisablesPreprocessing) {
    auto g = make_graph();
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/false);
    g->add_node(2, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(1.0), 0, 1, /*cost=*/1.0);
    g->add_arc<RealResource>(std::make_tuple(1.0), 1, 2, /*cost=*/1.0);
    g->add_arc<RealResource>(std::make_tuple(kExpensiveCost), 0, 2, /*cost=*/kExpensiveCost);

    ShortestPathPreprocessor<RealResource, RealResource> spp(
        g.get(),
        /*upper_bound=*/std::numeric_limits<double>::infinity(),
        /*cost_index=*/0);
    const bool removed_any = spp.preprocess();

    EXPECT_FALSE(removed_any);
    EXPECT_EQ(count_arcs(*g), 3U);
}

/// @brief ShortestPathPreprocessor restores all arcs after restore() is called.
///
/// After preprocessing removes arcs, calling restore() must put them back so
/// the graph is in its original state.
TEST(ShortestPathPreprocessor, RestoreReturnsAllArcs) {
    auto g = make_graph();
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/false);
    g->add_node(2, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(1.0), 0, 1, /*cost=*/1.0);
    g->add_arc<RealResource>(std::make_tuple(1.0), 1, 2, /*cost=*/1.0);
    g->add_arc<RealResource>(std::make_tuple(kExpensiveCost), 0, 2, /*cost=*/kExpensiveCost);

    ShortestPathPreprocessor<RealResource, RealResource> spp(g.get(),
                                                             /*upper_bound=*/kTightBound,
                                                             /*cost_index=*/0);
    spp.preprocess();
    ASSERT_EQ(count_arcs(*g), 2U);

    spp.restore();
    EXPECT_EQ(count_arcs(*g), 3U);
    EXPECT_NE(g->get_arc(2), nullptr);
}

/// @brief ShortestPathPreprocessor disables itself when a negative-weight cycle
///        is detected by BellmanFordAlgorithm.
///
/// A negative cycle makes shortest-path distances unbounded (−∞), so the
/// preprocessor cannot safely prune anything.  It must disable itself, leaving
/// all arcs in place.
TEST(ShortestPathPreprocessor, NegativeCycleDisablesPreprocessing) {
    // Build a graph with a negative cycle: 1 -> 2 -> 1 with total weight -1.
    //   source(0) --[1]---> node(1) --[-2]--> node(2) --[1]--> sink(3)
    //   node(2)   --[1]---> node(1)    <- closes a cycle of weight -2+1 = -1
    auto g = make_graph();
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/false);
    g->add_node(2, /*source=*/false, /*sink=*/false);
    g->add_node(3, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(1.0), 0, 1, /*cost=*/1.0);
    g->add_arc<RealResource>(std::make_tuple(kNegCycleCost), 1, 2, /*cost=*/kNegCycleCost);
    g->add_arc<RealResource>(std::make_tuple(1.0), 2, 3, /*cost=*/1.0);
    g->add_arc<RealResource>(std::make_tuple(1.0), 2, 1, /*cost=*/1.0);  // closes cycle

    const size_t arc_count_before = count_arcs(*g);

    // The constructor must catch the std::runtime_error from BellmanFordAlgorithm
    // and set disable_preprocessing_ = true; preprocess() then returns false.
    ShortestPathPreprocessor<RealResource, RealResource> spp(g.get(),
                                                             /*upper_bound=*/kTenBound,
                                                             /*cost_index=*/0);
    const bool removed_any = spp.preprocess();

    EXPECT_FALSE(removed_any);
    EXPECT_EQ(count_arcs(*g), arc_count_before);
}

/// @brief ShortestPathPreprocessor removes all arcs when upper_bound equals 0.
///
/// With upper_bound = 0.0 and all arc weights strictly positive, no arc can
/// appear on a valid path.  Every arc must be pruned.
TEST(ShortestPathPreprocessor, ZeroUpperBoundRemovesAllArcs) {
    auto g = make_graph();
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/false);
    g->add_node(2, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(1.0), 0, 1, /*cost=*/1.0);
    g->add_arc<RealResource>(std::make_tuple(1.0), 1, 2, /*cost=*/1.0);

    ShortestPathPreprocessor<RealResource, RealResource> spp(g.get(),
                                                             /*upper_bound=*/0.0,
                                                             /*cost_index=*/0);
    spp.preprocess();

    EXPECT_EQ(count_arcs(*g), 0U);
}

// ============================================================================
// BellmanFordAlgorithm direct tests
// ============================================================================

/// @brief BellmanFordAlgorithm::solve (arc.cost overload) computes correct
///        forward distances on an acyclic graph.
///
/// Graph: source(0) -1-> node(1) -1-> sink(2); source(0) -3-> sink(2).
/// Forward distances from source: d[0]=0, d[1]=1, d[2]=2.
TEST(BellmanFordAlgorithm, ForwardDistancesUsesArcCost) {
    auto g = make_graph();
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/false);
    g->add_node(2, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(1.0), 0, 1, /*cost=*/1.0);
    g->add_arc<RealResource>(std::make_tuple(1.0), 1, 2, /*cost=*/1.0);
    g->add_arc<RealResource>(std::make_tuple(kThreeCost), 0, 2, /*cost=*/kThreeCost);

    const Distance dist =
        BellmanFordAlgorithm::solve<RealResource>(*g, g->get_source_node_ids(), /*forward=*/true);

    EXPECT_DOUBLE_EQ(dist.at(0), 0.0);
    EXPECT_DOUBLE_EQ(dist.at(1), 1.0);
    EXPECT_DOUBLE_EQ(dist.at(2), kTwoCost);
}

/// @brief BellmanFordAlgorithm::solve (arc.cost overload) computes correct
///        backward distances from the sink.
///
/// Same graph; backward distances (distances to sink): d[2]=0, d[1]=1, d[0]=2.
TEST(BellmanFordAlgorithm, BackwardDistancesUsesArcCost) {
    auto g = make_graph();
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/false);
    g->add_node(2, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(1.0), 0, 1, /*cost=*/1.0);
    g->add_arc<RealResource>(std::make_tuple(1.0), 1, 2, /*cost=*/1.0);
    g->add_arc<RealResource>(std::make_tuple(kThreeCost), 0, 2, /*cost=*/kThreeCost);

    const Distance dist =
        BellmanFordAlgorithm::solve<RealResource>(*g, g->get_sink_node_ids(), /*forward=*/false);

    EXPECT_DOUBLE_EQ(dist.at(2), 0.0);
    EXPECT_DOUBLE_EQ(dist.at(1), 1.0);
    EXPECT_DOUBLE_EQ(dist.at(0), kTwoCost);
}

/// @brief BellmanFordAlgorithm::solve (cost_index overload) uses resource
///        extension to compute arc weights.
///
/// When cost_index is provided, arc weights are the difference in the
/// CostResourceType component after extension.  For additive resources this
/// matches the extender value.
TEST(BellmanFordAlgorithm, ForwardDistancesWithCostIndex) {
    auto g = make_graph();
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/false);
    g->add_node(2, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(kTwoCost), 0, 1, /*cost=*/kTwoCost);
    g->add_arc<RealResource>(std::make_tuple(kThreeCost), 1, 2, /*cost=*/kThreeCost);
    g->add_arc<RealResource>(std::make_tuple(kSevenCost), 0, 2, /*cost=*/kSevenCost);

    const Distance dist =
        BellmanFordAlgorithm::solve<RealResource, RealResource>(*g,
                                                                g->get_source_node_ids(),
                                                                /*cost_index=*/size_t{0},
                                                                /*forward=*/true);

    EXPECT_DOUBLE_EQ(dist.at(0), 0.0);
    EXPECT_DOUBLE_EQ(dist.at(1), kTwoCost);
    EXPECT_DOUBLE_EQ(dist.at(2), kTightBound);
}

/// @brief BellmanFordAlgorithm::solve (cost_index overload) initialises
///        unreachable nodes to +infinity.
///
/// An isolated node that is neither a source nor reachable from a source
/// should retain its initial infinity distance.
TEST(BellmanFordAlgorithm, UnreachableNodeHasInfiniteDistance) {
    auto g = make_graph();
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/false);  // isolated
    g->add_node(2, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(1.0), 0, 2, /*cost=*/1.0);

    const Distance dist =
        BellmanFordAlgorithm::solve<RealResource, RealResource>(*g,
                                                                g->get_source_node_ids(),
                                                                /*cost_index=*/size_t{0},
                                                                /*forward=*/true);

    EXPECT_DOUBLE_EQ(dist.at(0), 0.0);
    EXPECT_DOUBLE_EQ(dist.at(2), 1.0);
    EXPECT_TRUE(std::isinf(dist.at(1)));
}

/// @brief BellmanFordAlgorithm::solve (cost_index, nullopt) falls back to the
///        arc.cost-based overload.
TEST(BellmanFordAlgorithm, NulloptCostIndexFallsBackToArcCost) {
    auto g = make_graph();
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/false);
    g->add_node(2, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(kFourCost), 0, 1, /*cost=*/kFourCost);
    g->add_arc<RealResource>(std::make_tuple(kSixCost), 1, 2, /*cost=*/kSixCost);

    const Distance dist_nullopt =
        BellmanFordAlgorithm::solve<RealResource, RealResource>(*g,
                                                                g->get_source_node_ids(),
                                                                /*cost_index=*/std::nullopt,
                                                                /*forward=*/true);

    const Distance dist_arc =
        BellmanFordAlgorithm::solve<RealResource>(*g, g->get_source_node_ids(), /*forward=*/true);

    EXPECT_DOUBLE_EQ(dist_nullopt.at(0), dist_arc.at(0));
    EXPECT_DOUBLE_EQ(dist_nullopt.at(1), dist_arc.at(1));
    EXPECT_DOUBLE_EQ(dist_nullopt.at(2), dist_arc.at(2));
}

/// @brief BellmanFordAlgorithm::solve throws on a negative-weight cycle.
TEST(BellmanFordAlgorithm, ThrowsOnNegativeCycle) {
    auto g = make_graph();
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/false);
    g->add_node(2, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(kNegCycleCost), 0, 1, /*cost=*/kNegCycleCost);
    g->add_arc<RealResource>(std::make_tuple(1.0), 1, 0, /*cost=*/1.0);
    g->add_arc<RealResource>(std::make_tuple(1.0), 1, 2, /*cost=*/1.0);

    EXPECT_THROW(
        (BellmanFordAlgorithm::solve<RealResource>(*g, g->get_source_node_ids(), /*forward=*/true)),
        std::runtime_error);
}

// ============================================================================
// FeasibilityPreprocessor tests
// ============================================================================

/// @brief FeasibilityPreprocessor removes an arc that always produces an
///        infeasible resource.
///
/// Feasibility window: [0, 100].  Adding 200 exceeds it, so the arc must be
/// removed.
TEST(FeasibilityPreprocessor, RemovesInfeasibleArc) {
    auto g = make_graph(/*max_val=*/kLooseBound);
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(kTwoHundredVal), 0, 1, /*cost=*/kTwoHundredVal);

    ASSERT_EQ(count_arcs(*g), 1U);

    FeasibilityPreprocessor<Comp> fp(&g->get_resource_factory(), g.get());
    const bool removed_any = fp.preprocess();

    EXPECT_TRUE(removed_any);
    EXPECT_EQ(count_arcs(*g), 0U);
}

/// @brief FeasibilityPreprocessor keeps an arc that stays within the feasibility
///        window.
TEST(FeasibilityPreprocessor, KeepsFeasibleArc) {
    auto g = make_graph(/*max_val=*/kLooseBound);
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(kFiftyVal), 0, 1, /*cost=*/kFiftyVal);

    FeasibilityPreprocessor<Comp> fp(&g->get_resource_factory(), g.get());
    const bool removed_any = fp.preprocess();

    EXPECT_FALSE(removed_any);
    EXPECT_EQ(count_arcs(*g), 1U);
}

/// @brief FeasibilityPreprocessor removes only the infeasible arc.
TEST(FeasibilityPreprocessor, PartialRemovalOfInfeasibleArcs) {
    auto g = make_graph(/*max_val=*/kLooseBound);
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(2, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(kFiftyVal), 0, 2, /*cost=*/kFiftyVal);
    g->add_arc<RealResource>(std::make_tuple(kTwoHundredVal), 0, 2, /*cost=*/kTwoHundredVal);

    ASSERT_EQ(count_arcs(*g), 2U);

    FeasibilityPreprocessor<Comp> fp(&g->get_resource_factory(), g.get());
    fp.preprocess();

    EXPECT_EQ(count_arcs(*g), 1U);
    EXPECT_NE(g->get_arc(0), nullptr);
    EXPECT_EQ(g->get_arc(1), nullptr);
}

/// @brief FeasibilityPreprocessor removes an arc if ALL paths from the source
///        lead to infeasibility at the arc's origin.
///
/// Graph (window [0, 5]):
///   source(0) --[4]--> node(1) --[4]--> sink(2)
/// Extending by 4 from source gives value 4 (feasible for arc 0->1).
/// Then extending by another 4 gives 8 > 5 (infeasible for arc 1->2).
TEST(FeasibilityPreprocessor, RemovesArcWhenAllPathsLeadToInfeasibility) {
    auto g = make_graph(/*max_val=*/kTightBound);
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/false);
    g->add_node(2, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(kFourCost), 0, 1, /*cost=*/kFourCost);
    g->add_arc<RealResource>(std::make_tuple(kFourCost), 1, 2, /*cost=*/kFourCost);

    FeasibilityPreprocessor<Comp> fp(&g->get_resource_factory(), g.get());
    fp.preprocess();

    EXPECT_NE(g->get_arc(0), nullptr);
    EXPECT_EQ(g->get_arc(1), nullptr);
}

/// @brief FeasibilityPreprocessor leaves the graph unchanged when all arcs are
///        feasible.
TEST(FeasibilityPreprocessor, NoRemovalWhenAllArcsFeasible) {
    auto g = make_graph(/*max_val=*/kLooseBound);
    g->add_node(0, /*source=*/true, /*sink=*/false);
    g->add_node(1, /*source=*/false, /*sink=*/false);
    g->add_node(2, /*source=*/false, /*sink=*/true);
    g->add_arc<RealResource>(std::make_tuple(kTenBound), 0, 1, /*cost=*/kTenBound);
    g->add_arc<RealResource>(std::make_tuple(kTenBound), 1, 2, /*cost=*/kTenBound);

    FeasibilityPreprocessor<Comp> fp(&g->get_resource_factory(), g.get());
    const bool removed_any = fp.preprocess();

    EXPECT_FALSE(removed_any);
    EXPECT_EQ(count_arcs(*g), 2U);
}
