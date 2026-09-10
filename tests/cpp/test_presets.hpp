// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// rcspp::presets -- one call per resource kind.
//
// The claim a preset makes is "this is what you would have written by hand". These tests assert
// it, rather than trusting that two lists of four make_uniques stay in step. Drift is the real
// hazard here: a preset and the hand-built form diverging after someone edits one of them.

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace presets_test {

inline std::map<size_t, std::pair<double, double>> windows() {
    return {{0, {0.0, 100.0}}, {1, {0.0, 100.0}}};
}

/// @brief Adds two nodes and one arc, so both models are solvable and comparable.
template <typename Graph, typename R>
inline void finish(Graph* graph, double arc_value) {
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1, /*source=*/false, /*sink=*/true);
    graph->template add_arc<R>(std::make_tuple(arc_value), 0, 1, 10.0);
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

// The one worth writing carefully: hand-build it with the CORRECT pairing --
// BudgetExtensionFunction + MinMaxFeasibilityFunction(0, cap, true) -- not the
// AdditionExtensionFunction one the VRP model used to have. The preset exists so the wrong
// pairing is not expressible through it.
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

TEST(Presets, NgPathResourceMatchesTheHandBuiltModel) {
    const std::map<size_t, std::set<size_t>> neighborhoods{{0, {0, 1}}, {1, {0, 1}}};
    const std::map<size_t, std::set<size_t>> forbidden{{0, {0}}, {1, {1}}};

    auto by_preset = std::make_unique<ResourceGraph<SizeTBitsetResource>>();
    presets::add_ng_path_resource<SizeTBitsetResource>(*by_preset, neighborhoods, forbidden);
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

// The structural half: the derived reversal flag is what a mismatched quadruple gets wrong
// *before* any label exists, so checking it fails earliest and cheapest.
//
// Reading the flag needs a borrowed pointer, which only the hand-built side can offer -- a preset
// owns its objects, so there is nothing to borrow. The preset half is therefore checked by the
// flag's observable consequence: a bidirectional solve that finds something. If the reversal were
// wrong the backward search would prune its better labels and return nothing.
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

    auto by_preset = std::make_unique<ResourceGraph<RealResource>>();
    presets::add_window_resource<RealResource>(*by_preset, presets_test::windows());
    presets_test::finish<ResourceGraph<RealResource>, RealResource>(by_preset.get(), 10.0);

    AlgorithmParams<LabelList<ResourceTypeComposition<RealResource>>> params;
    params.critical_resource_index = 0;
    params.half_way_point = 50.0;
    auto algorithm =
        by_preset->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    const auto result = by_preset->solve(algorithm.get());
    EXPECT_FALSE(result.solutions.empty())
        << "a preset-built model must be solvable bidirectionally; if the derived reversal were "
           "wrong the backward search would prune its better labels and find nothing";
}

// The point of add_budget_resource: it produces a Threshold extension, not an Accumulate one, so
// the pairing that silently found nothing is not expressible through it.
TEST(Presets, BudgetPresetDeclaresThreshold) {
    static_assert(backward_kind_of_v<BudgetExtensionFunction<IntResource>> ==
                  BackwardKind::Threshold);

    // And the model it builds is solvable bidirectionally, which is the property the wrong
    // pairing destroyed: with an accumulating extension the backward label leaves its range on
    // the first arc and the search finds nothing.
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
