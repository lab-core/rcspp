// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// rcspp::presets -- one call per resource kind.
//
// Each preset must match the model you would have written by hand; these tests guard against the
// two drifting apart.

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace presets_test {

inline std::map<size_t, std::pair<double, double>> windows() {
    return {{0, {0.0, 100.0}}, {1, {0.0, 100.0}}};
}

/// @brief Adds two nodes and one arc, so both models are solvable and comparable.
///
/// @p arc_value takes the resource's own value type, so an integer resource is not handed a
/// double.
template <typename Graph, typename R,
          typename V = std::decay_t<decltype(std::declval<R>().get_value())>>
inline void finish(Graph* graph, V arc_value) {
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1, /*source=*/false, /*sink=*/true);
    graph->template add_arc<R>(std::make_tuple(arc_value), 0, 1, 10.0);
}

/// @brief Node 1's cap is 5 and node 3's is 7; every other node has the uniform capacity.
inline std::map<size_t, double> per_node_caps() {
    return {{1, 5.0}, {3, 7.0}};
}

/// @brief The per-node budget model: components are (cost, load).
///
/// 0 -> 1 -> 2 costs -10 but breaks node 1's cap. 0 -> 3 -> 2 costs -1 and respects node 3's cap;
/// at H = 5 the join needs a backward label at node 3, which exists only if the extension clamps
/// its ceiling to that cap. -1 via arcs {2, 3} is the only right answer.
inline void add_two_routes(ResourceGraph<RealResource>* graph) {
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(3);
    graph->add_node(2, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource, RealResource>({-5.0, 10.0}, 0, 1, -5.0);
    graph->add_arc<RealResource, RealResource>({-5.0, 10.0}, 1, 2, -5.0);
    graph->add_arc<RealResource, RealResource>({-0.5, 6.0}, 0, 3, -0.5);
    graph->add_arc<RealResource, RealResource>({-0.5, 1.0}, 3, 2, -0.5);
}

/// @brief Compares two solution sets by cost and arc sequence.
inline void expect_same_solutions(const SolveResult& lhs, const SolveResult& rhs) {
    ASSERT_EQ(lhs.solutions.size(), rhs.solutions.size());
    for (size_t i = 0; i < lhs.solutions.size(); ++i) {
        EXPECT_DOUBLE_EQ(lhs.solutions[i].cost, rhs.solutions[i].cost);
        EXPECT_EQ(lhs.solutions[i].path_arc_ids, rhs.solutions[i].path_arc_ids);
    }
}

}  // namespace presets_test

TEST(Presets, CostResourceMatchesTheHandBuiltModel) {
    auto by_preset = std::make_unique<ResourceGraph<RealResource>>();
    presets::add_cost_resource<RealResource>(*by_preset);
    presets_test::finish<ResourceGraph<RealResource>, RealResource>(by_preset.get(), 10.0);

    auto by_hand = std::make_unique<ResourceGraph<RealResource>>();
    by_hand->add_resource<RealResource>(
        std::make_unique<AdditionExtensionFunction<RealResource>>(),
        std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<ValueCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    presets_test::finish<ResourceGraph<RealResource>, RealResource>(by_hand.get(), 10.0);

    presets_test::expect_same_solutions(
        by_preset->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}),
        by_hand->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}));
}

TEST(Presets, WindowResourceMatchesTheHandBuiltModel) {
    auto by_preset = std::make_unique<ResourceGraph<RealResource>>();
    presets::add_window_resource<RealResource>(*by_preset, presets_test::windows());
    presets_test::finish<ResourceGraph<RealResource>, RealResource>(by_preset.get(), 10.0);

    auto by_hand = std::make_unique<ResourceGraph<RealResource>>();
    by_hand->add_resource<RealResource>(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(presets_test::windows()),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(presets_test::windows()),
        std::make_unique<ValueCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    presets_test::finish<ResourceGraph<RealResource>, RealResource>(by_hand.get(), 10.0);

    presets_test::expect_same_solutions(
        by_preset->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}),
        by_hand->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}));
}

// Hand-built with the correct pairing: BudgetExtensionFunction + MinMaxFeasibilityFunction(0, cap,
// true), not AdditionExtensionFunction.
TEST(Presets, BudgetResourceMatchesTheHandBuiltModel) {
    constexpr int kCapacity = 50;

    auto by_preset = std::make_unique<ResourceGraph<IntResource>>();
    presets::add_budget_resource<IntResource>(*by_preset, kCapacity);
    presets_test::finish<ResourceGraph<IntResource>, IntResource>(by_preset.get(), 10);

    auto by_hand = std::make_unique<ResourceGraph<IntResource>>();
    by_hand->add_resource<IntResource>(
        std::make_unique<BudgetExtensionFunction<IntResource>>(std::map<size_t, int>{}, kCapacity),
        std::make_unique<MinMaxFeasibilityFunction<IntResource>>(0,
                                                                 kCapacity,
                                                                 /*merge_by_increasing_value=*/
                                                                 true),
        std::make_unique<TrivialCostFunction<IntResource>>(),
        std::make_unique<ValueDominanceFunction<IntResource>>());
    presets_test::finish<ResourceGraph<IntResource>, IntResource>(by_hand.get(), 10);

    presets_test::expect_same_solutions(
        by_preset->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}),
        by_hand->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}));
}

// Per-node caps go on the feasibility function; the extension takes its backward clamp from there.
TEST(Presets, PerNodeBudgetMatchesTheHandBuiltModel) {
    auto by_preset = std::make_unique<ResourceGraph<RealResource>>();
    presets::add_cost_resource<RealResource>(*by_preset);
    presets::add_budget_resource<RealResource>(*by_preset,
                                               /*capacity=*/100.0,
                                               presets_test::per_node_caps());
    presets_test::add_two_routes(by_preset.get());

    std::map<size_t, std::pair<double, double>> windows;
    for (const auto& [node_id, cap] : presets_test::per_node_caps()) {
        windows.emplace(node_id, std::pair<double, double>{0.0, cap});
    }
    auto by_hand = std::make_unique<ResourceGraph<RealResource>>();
    presets::add_cost_resource<RealResource>(*by_hand);
    by_hand->add_resource<RealResource>(
        std::make_unique<BudgetExtensionFunction<RealResource>>(std::map<size_t, double>{}, 100.0),
        std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 100.0, std::move(windows)),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    presets_test::add_two_routes(by_hand.get());

    presets_test::expect_same_solutions(
        by_preset->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}),
        by_hand->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}));
}

// Per-node caps bind forward and at every H; at H = 5 the answer needs the backward clamp at
// node 3.
TEST(Presets, PerNodeBudgetCapsBindInBothDirections) {
    const auto build = [] {
        auto graph = std::make_unique<ResourceGraph<RealResource>>();
        presets::add_cost_resource<RealResource>(*graph);
        presets::add_budget_resource<RealResource>(*graph,
                                                   /*capacity=*/100.0,
                                                   presets_test::per_node_caps());
        presets_test::add_two_routes(graph.get());
        return graph;
    };
    const std::vector<size_t> dear_route{2, 3};

    const auto forward = build()->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    ASSERT_FALSE(forward.solutions.empty());
    EXPECT_DOUBLE_EQ(forward.solutions.front().cost, -1.0);
    EXPECT_EQ(forward.solutions.front().path_arc_ids, dear_route);

    // 0 turns the bound off; 5 and 15 sit either side of node 1's load of 10.
    for (const double half_way_point : {0.0, 5.0, 15.0}) {
        auto graph = build();
        AlgorithmParams<LabelList<ResourceTypeComposition<RealResource>>> params;
        params.critical_resource_index = 1;
        params.half_way_point = half_way_point;
        auto algorithm =
            graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
        const auto result = graph->solve(algorithm.get());
        ASSERT_FALSE(result.solutions.empty()) << "H = " << half_way_point;
        EXPECT_DOUBLE_EQ(result.solutions.front().cost, -1.0) << "H = " << half_way_point;
        EXPECT_EQ(result.solutions.front().path_arc_ids, dear_route) << "H = " << half_way_point;
    }
}

TEST(Presets, NgPathResourceMatchesTheHandBuiltModel) {
    const std::map<size_t, std::set<size_t>> neighborhoods{{0, {0, 1}}, {1, {0, 1}}};
    const std::map<size_t, std::set<size_t>> forbidden{{0, {0}}, {1, {1}}};

    auto by_preset = std::make_unique<ResourceGraph<SizeTBitsetResource>>();
    presets::add_ng_path_resource<SizeTBitsetResource>(*by_preset, neighborhoods);
    by_preset->add_node(0, /*source=*/true, /*sink=*/false);
    by_preset->add_node(1, /*source=*/false, /*sink=*/true);
    by_preset->add_arc<SizeTBitsetResource>(std::make_tuple(std::set<size_t>{}), 0, 1, 10.0);

    auto by_hand = std::make_unique<ResourceGraph<SizeTBitsetResource>>();
    by_hand->add_resource<SizeTBitsetResource>(
        std::make_unique<NgPathExtensionFunction<SizeTBitsetResource, size_t>>(neighborhoods),
        std::make_unique<IntersectionFeasibilityFunction<SizeTBitsetResource, size_t>>(
            forbidden,
            /*forbidden=*/true),
        std::make_unique<TrivialCostFunction<SizeTBitsetResource>>(),
        std::make_unique<InclusionDominanceFunction<SizeTBitsetResource>>());
    by_hand->add_node(0, /*source=*/true, /*sink=*/false);
    by_hand->add_node(1, /*source=*/false, /*sink=*/true);
    by_hand->add_arc<SizeTBitsetResource>(std::make_tuple(std::set<size_t>{}), 0, 1, 10.0);

    presets_test::expect_same_solutions(
        by_preset->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}),
        by_hand->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}));
}

/// @brief The elementary preset is the ng preset with nothing ever forgotten.
///
/// The preset must build neighborhoods spanning every node and `forbidden[v] = {v}`; a missed node
/// would silently permit a revisit through it.
TEST(Presets, ElementaryResourceIsNgPathWithUniversalNeighborhoods) {
    const std::vector<size_t> node_ids{0, 1, 2};
    const std::map<size_t, std::set<size_t>> universal{{0, {0, 1, 2}},
                                                       {1, {0, 1, 2}},
                                                       {2, {0, 1, 2}}};
    const std::map<size_t, std::set<size_t>> forbidden{{0, {0}}, {1, {1}}, {2, {2}}};

    const auto build_arcs = [](auto* graph) {
        graph->add_node(0, /*source=*/true, /*sink=*/false);
        graph->add_node(1);
        graph->add_node(2, /*source=*/false, /*sink=*/true);
        graph->template add_arc<SizeTBitsetResource>(std::make_tuple(std::set<size_t>{}),
                                                     0,
                                                     1,
                                                     10.0);
        graph->template add_arc<SizeTBitsetResource>(std::make_tuple(std::set<size_t>{}),
                                                     1,
                                                     2,
                                                     10.0);
        graph->template add_arc<SizeTBitsetResource>(std::make_tuple(std::set<size_t>{}),
                                                     0,
                                                     2,
                                                     30.0);
    };

    auto by_preset = std::make_unique<ResourceGraph<SizeTBitsetResource>>();
    presets::add_elementary_resource<SizeTBitsetResource>(*by_preset, node_ids);
    build_arcs(by_preset.get());

    auto by_hand = std::make_unique<ResourceGraph<SizeTBitsetResource>>();
    by_hand->add_resource<SizeTBitsetResource>(
        std::make_unique<NgPathExtensionFunction<SizeTBitsetResource, size_t>>(universal),
        std::make_unique<IntersectionFeasibilityFunction<SizeTBitsetResource, size_t>>(
            forbidden,
            /*forbidden=*/true),
        std::make_unique<TrivialCostFunction<SizeTBitsetResource>>(),
        std::make_unique<InclusionDominanceFunction<SizeTBitsetResource>>());
    build_arcs(by_hand.get());

    presets_test::expect_same_solutions(
        by_preset->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}),
        by_hand->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}));
}

// The ng preset forbids `{v}` at every node its neighborhoods mention, not just the keys. Node 1
// has no neighborhood but node 2 remembers it, so the negative cycle 1 -> 2 -> 1 is forbidden.
TEST(Presets, NgPathPresetForbidsEveryNodeItsNeighborhoodsMention) {
    std::map<size_t, std::pair<double, double>> windows;
    for (size_t node = 0; node < 4; ++node) {
        windows[node] = {0.0, 10.0};
    }
    auto graph = std::make_unique<ResourceGraph<RealResource, SizeTBitsetResource>>();
    presets::add_cost_resource<RealResource>(*graph);
    presets::add_window_resource<RealResource>(*graph, windows);
    presets::add_ng_path_resource<SizeTBitsetResource>(*graph, {{2, {1, 2}}});
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

    const auto result = graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_DOUBLE_EQ(result.solutions.front().cost, 2.0);
    EXPECT_EQ(result.solutions.front().path_arc_ids, (std::vector<size_t>{0, 3}));
}

// The derived dominance reversal. A preset owns its objects, so its half is checked indirectly:
// a wrong reversal would make the bidirectional solve find nothing.
TEST(Presets, DerivedReversalAgreesWithTheHandBuiltModel) {
    auto by_hand = std::make_unique<ResourceGraph<RealResource>>();
    auto dominance = std::make_unique<ValueDominanceFunction<RealResource>>();
    auto* borrowed = dominance.get();
    std::unique_ptr<ExtensionFunction<RealResource>> extension =
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(presets_test::windows());
    by_hand->add_resource<RealResource>(
        std::move(extension),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(presets_test::windows()),
        std::make_unique<ValueCostFunction<RealResource>>(),
        std::move(dominance));

    // A window resource is a Threshold, so its dominance must be reversed.
    EXPECT_TRUE(borrowed->is_backward_reversed());

    // The window is the clock beside a cost slot, as in any real model. Priced on the window
    // itself the model is refused: the join would add a backward deadline as a cost.
    auto by_preset = std::make_unique<ResourceGraph<RealResource>>();
    presets::add_cost_resource<RealResource>(*by_preset);
    presets::add_window_resource<RealResource>(*by_preset, presets_test::windows());
    by_preset->add_node(0, /*source=*/true, /*sink=*/false);
    by_preset->add_node(1, /*source=*/false, /*sink=*/true);
    by_preset->add_arc<RealResource, RealResource>({10.0, 10.0}, 0, 1, 10.0);

    AlgorithmParams<LabelList<ResourceTypeComposition<RealResource>>> params;
    params.critical_resource_index = 1;
    params.half_way_point = 50.0;
    auto algorithm =
        by_preset->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    const auto result = by_preset->solve(algorithm.get());
    EXPECT_FALSE(result.solutions.empty())
        << "a preset-built model must be solvable bidirectionally; if the derived reversal were "
           "wrong the backward search would prune its better labels and find nothing";
}

// add_budget_resource produces a Threshold extension, not an Accumulate one.
TEST(Presets, BudgetPresetDeclaresThreshold) {
    static_assert(backward_kind_of_v<BudgetExtensionFunction<IntResource>> ==
                  BackwardKind::Threshold);

    // The model is solvable bidirectionally; an accumulating extension would find nothing.
    auto graph = std::make_unique<ResourceGraph<RealResource, IntResource>>();
    presets::add_cost_resource<RealResource>(*graph);
    presets::add_budget_resource<IntResource>(*graph, /*capacity=*/50);
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource, IntResource>(
        std::make_tuple(std::make_tuple(10.0), std::make_tuple(10)),
        0,
        1,
        10.0);

    AlgorithmParams<LabelList<ResourceTypeComposition<RealResource, IntResource>>> params;
    params.critical_resource_index = 0;
    params.half_way_point = 0.0;
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    const auto result = graph->solve(algorithm.get());
    EXPECT_FALSE(result.solutions.empty())
        << "a budget preset must be coherent backwards; finding nothing is the signature of the "
           "accumulate-plus-ceiling-seed defect this preset exists to prevent";
}

// ============================================================================
// Integer arguments on a real resource convert, never truncate
// ============================================================================

namespace presets_test {

/// @brief Two routes: 0-1-2 costs 2 and consumes 2.9 + 2.9 = 5.8; 0-2 costs 10 and consumes 0.
inline void add_fractional_routes(ResourceGraph<RealResource, RealResource>* graph) {
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource, RealResource>({1.0, 2.9}, 0, 1, 1.0);
    graph->add_arc<RealResource, RealResource>({1.0, 2.9}, 1, 2, 1.0);
    graph->add_arc<RealResource, RealResource>({10.0, 0.0}, 0, 2, 10.0);
}

}  // namespace presets_test

/// @brief An `int` capacity on a real resource is the capacity, not the type every consumption is
///        read as.
///
/// `V` used to be deduced from the argument, so `5` built an `int` budget over a real resource,
/// read each 2.9 as 2, and accepted the 5.8 route at cost 2.
TEST(Presets, BudgetOnARealResourceKeepsFractionalConsumption) {
    auto graph = std::make_unique<ResourceGraph<RealResource, RealResource>>();
    presets::add_cost_resource<RealResource>(*graph);
    presets::add_budget_resource<RealResource>(*graph, 5);
    presets_test::add_fractional_routes(graph.get());

    const auto result = graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    ASSERT_EQ(result.solutions.size(), 1U) << "the 5.8 route breaks a capacity of 5";
    EXPECT_DOUBLE_EQ(result.solutions.front().cost, 10.0);
}

/// @brief Integer windows on a real resource convert, so a node closing at 5 rejects an arrival at
///        5.8.
TEST(Presets, IntegerWindowsOnARealResourceKeepFractionalTimes) {
    auto graph = std::make_unique<ResourceGraph<RealResource, RealResource>>();
    presets::add_cost_resource<RealResource>(*graph);
    const std::map<size_t, std::pair<int, int>> windows{{0, {0, 100}}, {1, {0, 100}}, {2, {0, 5}}};
    presets::add_window_resource<RealResource>(*graph, windows);
    presets_test::add_fractional_routes(graph.get());

    const auto result = graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    ASSERT_EQ(result.solutions.size(), 1U) << "0-1-2 arrives at 5.8, after node 2 closes at 5";
    EXPECT_DOUBLE_EQ(result.solutions.front().cost, 10.0);
}
