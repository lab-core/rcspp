// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The ng utilities a column-generation driver grows neighbourhoods with: finding the cycles in a
// path, testing a path against an ng memory, and swapping a built graph's neighbourhoods.

#include <gtest/gtest.h>

#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

/// @brief Each pair of consecutive visits to one node is one cycle, with the nodes between it.
TEST(NgNeighborhoods, FindCyclesReportsEachConsecutiveRevisit) {
    const std::vector<size_t> elementary{0, 1, 2, 3};
    EXPECT_TRUE(find_cycles(elementary).empty());

    const std::vector<size_t> once{0, 1, 2, 1, 3};
    const auto one = find_cycles(once);
    ASSERT_EQ(one.size(), 1U);
    EXPECT_EQ(one[0].node, 1U);
    EXPECT_EQ(one[0].first, 1U);
    EXPECT_EQ(one[0].last, 3U);
    EXPECT_EQ(one[0].interior, (std::vector<size_t>{2}));
    EXPECT_EQ(one[0].length(), 1U);

    const std::vector<size_t> thrice{0, 1, 2, 1, 4, 5, 1, 6};
    const auto two = find_cycles(thrice);
    ASSERT_EQ(two.size(), 2U) << "one cycle per consecutive pair of visits";
    EXPECT_EQ(two[0].interior, (std::vector<size_t>{2}));
    EXPECT_EQ(two[1].interior, (std::vector<size_t>{4, 5}));

    const std::vector<size_t> overlapping{0, 1, 2, 3, 1, 2, 4};
    const auto both = find_cycles(overlapping);
    ASSERT_EQ(both.size(), 2U);
    EXPECT_EQ(both[0].node, 1U);
    EXPECT_EQ(both[0].interior, (std::vector<size_t>{2, 3}));
    EXPECT_EQ(both[1].node, 2U);
    EXPECT_EQ(both[1].interior, (std::vector<size_t>{3, 1}));
}

/// @brief A revisit is forbidden exactly when every node since the first visit remembers it.
TEST(NgNeighborhoods, IsNgFeasibleForgetsWhatANeighbourhoodDoesNotHold) {
    const std::vector<size_t> revisit{0, 1, 2, 1, 3};

    // 2 forgets 1 on arrival, so returning to 1 is allowed.
    const std::map<size_t, std::set<size_t>> forgets{{0, {}}, {1, {}}, {2, {}}, {3, {}}};
    EXPECT_TRUE(is_ng_feasible(std::span<const size_t>(revisit), forgets));

    // 2 remembers 1, so the return is forbidden.
    const std::map<size_t, std::set<size_t>> remembers{{0, {}}, {1, {}}, {2, {1}}, {3, {}}};
    EXPECT_FALSE(is_ng_feasible(std::span<const size_t>(revisit), remembers));

    // Returning straight away is always forbidden: a node always keeps itself on arrival.
    const std::vector<size_t> immediate{0, 1, 1, 3};
    EXPECT_FALSE(is_ng_feasible(std::span<const size_t>(immediate), forgets));

    // A node the table does not name has no forbidden set, as under the presets.
    const std::map<size_t, std::set<size_t>> unnamed{{0, {}}, {2, {1}}, {3, {}}};
    EXPECT_TRUE(is_ng_feasible(std::span<const size_t>(revisit), unnamed));
}

// ============================================================================
// Swapping a built graph's neighbourhoods
// ============================================================================

namespace ng_swap_test {

using Graph = ResourceGraph<RealResource, SizeTBitsetResource>;
using Composed = ResourceTypeComposition<RealResource, SizeTBitsetResource>;
using Table = std::map<size_t, std::set<size_t>>;

/// @brief Every node named, and 2 forgetting 1: the negative cycle 1 -> 2 -> 1 may repeat.
inline Table forgets() {
    return {{0, {}}, {1, {}}, {2, {}}, {3, {}}};
}

/// @brief The same keys, and 2 remembering 1: returning to 1 through 2 is forbidden.
inline Table remembers() {
    return {{0, {}}, {1, {}}, {2, {1}}, {3, {}}};
}

/// @brief Source 0, sink 3, a -10 cycle 1 -> 2 -> 1, and a window [0, 10] with one unit per arc.
///
/// Under @ref forgets the cycle repeats until the window runs out: 0 1 (2 1) x4 3, ten arcs,
/// 1 - 40 + 1 = -38. Under @ref remembers the only path is 0 1 3, cost 2.
inline std::unique_ptr<Graph> make_graph(const Table& neighborhoods) {
    std::map<size_t, std::pair<double, double>> windows;
    for (size_t node = 0; node < 4; ++node) {
        windows[node] = {0.0, 10.0};
    }
    auto graph = std::make_unique<Graph>();
    presets::add_cost_resource<RealResource>(*graph);
    presets::add_window_resource<RealResource>(*graph, windows);
    presets::add_ng_path_resource<SizeTBitsetResource>(*graph, neighborhoods);
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2);
    graph->add_node(3, /*source=*/false, /*sink=*/true);
    // {cost, time, ng payload}; the payload is ignored, the memory reads the arc's endpoints.
    const std::set<size_t> none;
    graph->add_arc<RealResource, RealResource, SizeTBitsetResource>({1.0, 1.0, none}, 0, 1, 1.0);
    graph->add_arc<RealResource, RealResource, SizeTBitsetResource>({-5.0, 1.0, none}, 1, 2, -5.0);
    graph->add_arc<RealResource, RealResource, SizeTBitsetResource>({-5.0, 1.0, none}, 2, 1, -5.0);
    graph->add_arc<RealResource, RealResource, SizeTBitsetResource>({1.0, 1.0, none}, 1, 3, 1.0);
    return graph;
}

/// @brief The best path a forward solve returns.
inline Solution forward_best(Graph* graph) {
    const auto result = graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    EXPECT_FALSE(result.solutions.empty());
    return result.solutions.empty() ? Solution{} : result.solutions.front();
}

/// @brief The best path a bidirectional solve returns, with the window as its clock.
inline Solution bidirectional_best(Graph* graph) {
    AlgorithmParams<LabelList<Composed>> params;
    params.critical_resource_index = 1;
    params.half_way_point = 5.0;
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    const auto result = graph->solve(algorithm.get());
    EXPECT_TRUE(result.bounded_by_half_way) << "the rest of the test is about a bound in force";
    EXPECT_FALSE(result.solutions.empty());
    return result.solutions.empty() ? Solution{} : result.solutions.front();
}

constexpr double kCycling = -38.0;
constexpr double kDirect = 2.0;

}  // namespace ng_swap_test

/// @brief After a swap, a graph solves exactly as one built with the new table.
///
/// The test the swap rests on. Arcs cache the neighbourhoods they read when they are built, so a
/// swap that replaced only the prototype would leave every arc on the old table, and this solve
/// would still return the cycling path.
TEST(NgNeighborhoods, ASwappedTableIsTheTableABuiltGraphWouldHave) {
    namespace ns = ng_swap_test;

    auto swapped = ns::make_graph(ns::forgets());
    ASSERT_DOUBLE_EQ(ns::forward_best(swapped.get()).cost, ns::kCycling)
        << "the control: under forgets() the cycle repeats";

    set_ng_neighborhoods<SizeTBitsetResource>(*swapped, 0, ns::remembers());
    auto built = ns::make_graph(ns::remembers());

    const Solution after = ns::forward_best(swapped.get());
    const Solution reference = ns::forward_best(built.get());
    EXPECT_DOUBLE_EQ(after.cost, ns::kDirect);
    EXPECT_DOUBLE_EQ(after.cost, reference.cost);
    EXPECT_EQ(after.path_arc_ids, reference.path_arc_ids);
    EXPECT_EQ(ng_neighborhoods<SizeTBitsetResource>(*swapped, 0), ns::remembers());
}

/// @brief The rebuild re-runs `preprocess`, which caches the backward side as well.
TEST(NgNeighborhoods, TheSwapRebuildsBothDirections) {
    namespace ns = ng_swap_test;

    auto graph = ns::make_graph(ns::forgets());
    EXPECT_DOUBLE_EQ(ns::bidirectional_best(graph.get()).cost, ns::kCycling);

    set_ng_neighborhoods<SizeTBitsetResource>(*graph, 0, ns::remembers());
    EXPECT_DOUBLE_EQ(ns::bidirectional_best(graph.get()).cost, ns::kDirect)
        << "a stale backward side would let the backward search, or the join, keep the cycle";
    EXPECT_DOUBLE_EQ(ns::bidirectional_best(graph.get()).cost, ns::forward_best(graph.get()).cost);
}

/// @brief A swap rebuilds the ng component only: the costs and the window still apply.
///
/// Swapping back to the table the graph was built with must give back the original optimum, which
/// only the cost slot (the -10 cycle) and the window (the loop stops at ten arcs) produce.
TEST(NgNeighborhoods, TheSwapKeepsEveryOtherComponentAndTheArcValues) {
    namespace ns = ng_swap_test;

    auto graph = ns::make_graph(ns::forgets());
    const Solution original = ns::forward_best(graph.get());

    set_ng_neighborhoods<SizeTBitsetResource>(*graph, 0, ns::remembers());
    set_ng_neighborhoods<SizeTBitsetResource>(*graph, 0, ns::forgets());

    const Solution restored = ns::forward_best(graph.get());
    EXPECT_DOUBLE_EQ(restored.cost, ns::kCycling);
    EXPECT_EQ(restored.path_arc_ids, original.path_arc_ids);
    EXPECT_EQ(restored.path_arc_ids.size(), 10U) << "the window must still stop the loop";
}

/// @brief A table that would change which nodes are forbidden, or no ng component, is refused.
TEST(NgNeighborhoods, ANewKeyOrAForeignMemberIsRefused) {
    namespace ns = ng_swap_test;

    auto graph = ns::make_graph(ns::forgets());

    auto new_key = ns::forgets();
    new_key[7] = {};
    EXPECT_THROW(set_ng_neighborhoods<SizeTBitsetResource>(*graph, 0, new_key),
                 std::invalid_argument);

    auto foreign_member = ns::forgets();
    foreign_member[2] = {9};
    EXPECT_THROW(set_ng_neighborhoods<SizeTBitsetResource>(*graph, 0, foreign_member),
                 std::invalid_argument);

    auto dropped = ns::forgets();
    dropped.erase(3);
    EXPECT_THROW(set_ng_neighborhoods<SizeTBitsetResource>(*graph, 0, dropped),
                 std::invalid_argument);

    EXPECT_THROW(set_ng_neighborhoods<SizeTBitsetResource>(*graph, 1, ns::remembers()),
                 std::out_of_range)
        << "the graph has one SizeTBitsetResource component";

    // A container component that is not ng-path.
    ResourceGraph<SizeTBitsetResource> visited_set;
    visited_set.add_resource<SizeTBitsetResource>(
        std::make_unique<UnionExtensionFunction<SizeTBitsetResource>>(),
        std::make_unique<TrivialFeasibilityFunction<SizeTBitsetResource>>(),
        std::make_unique<TrivialCostFunction<SizeTBitsetResource>>(),
        std::make_unique<InclusionDominanceFunction<SizeTBitsetResource>>());
    EXPECT_THROW(set_ng_neighborhoods<SizeTBitsetResource>(visited_set, 0, ns::remembers()),
                 std::invalid_argument);

    // A refused swap changes nothing.
    EXPECT_DOUBLE_EQ(ns::forward_best(graph.get()).cost, ns::kCycling);
}

/// @brief A clone keeps the table it was taken with.
TEST(NgNeighborhoods, ClonesKeepTheirOwnTable) {
    namespace ns = ng_swap_test;

    auto graph = ns::make_graph(ns::forgets());
    auto before = graph->clone();
    set_ng_neighborhoods<SizeTBitsetResource>(*graph, 0, ns::remembers());
    auto after = graph->clone();

    EXPECT_DOUBLE_EQ(ns::forward_best(before.get()).cost, ns::kCycling)
        << "a clone taken earlier is untouched";
    EXPECT_DOUBLE_EQ(ns::forward_best(after.get()).cost, ns::kDirect)
        << "a clone taken later carries the new table";
}

/// @brief `is_ng_feasible` says of a path exactly what the solver does.
TEST(NgNeighborhoods, TheSolverAndIsNgFeasibleAgree) {
    namespace ns = ng_swap_test;

    auto graph = ns::make_graph(ns::forgets());
    const Solution cycling = ns::forward_best(graph.get());
    ASSERT_DOUBLE_EQ(cycling.cost, ns::kCycling);
    const std::span<const size_t> nodes(cycling.path_node_ids);

    EXPECT_TRUE(is_ng_feasible(nodes, ns::forgets())) << "the solver returned it under forgets()";
    EXPECT_FALSE(is_ng_feasible(nodes, ns::remembers()))
        << "and under remembers() the solver no longer returns it";

    set_ng_neighborhoods<SizeTBitsetResource>(*graph, 0, ns::remembers());
    const Solution direct = ns::forward_best(graph.get());
    EXPECT_TRUE(is_ng_feasible(std::span<const size_t>(direct.path_node_ids), ns::remembers()));
}
