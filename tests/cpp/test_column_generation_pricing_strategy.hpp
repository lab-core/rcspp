// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Pricing stages and the strategy that moves between them, on a three-node graph.

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <tuple>
#include <vector>

#include "rcspp/column_generation/pricing_strategy.hpp"
#include "rcspp/rcspp.hpp"

using namespace rcspp;                     // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation;  // NOLINT(google-build-using-namespace)

namespace {

using StageGraph = ResourceGraph<RealResource>;
using Stage = PricingStage<RealResource>;
using Strategy = PricingStrategy<RealResource>;

// Source 0 to sink 2, through 1 (cost 2 + 3 = 5) or directly (cost 7).
constexpr double through_first_arc = 2.0;
constexpr double through_second_arc = 3.0;
constexpr double through_cost = through_first_arc + through_second_arc;
constexpr double direct_cost = 7.0;

std::unique_ptr<StageGraph> two_path_graph() {
    auto graph = std::make_unique<StageGraph>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1, /*source=*/false, /*sink=*/false);
    graph->add_node(2, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource>(std::make_tuple(through_first_arc), 0, 1, through_first_arc);
    graph->add_arc<RealResource>(std::make_tuple(through_second_arc), 1, 2, through_second_arc);
    graph->add_arc<RealResource>(std::make_tuple(direct_cost), 0, 2, direct_cost);
    return graph;
}

// A stage that runs no search: it records the request and returns nothing.
Stage recording_stage(std::vector<StageRequest>* requests, bool exact) {
    return {[requests](StageGraph& /*graph*/, const StageRequest& request) {
                requests->push_back(request);
                return SolveResult{};
            },
            exact};
}

}  // namespace

TEST(ColumnGenerationPricingStrategy, DefaultIsOneExactStage) {
    Strategy strategy;

    EXPECT_EQ(strategy.stages().size(), 1U);
    EXPECT_TRUE(strategy.is_exact());
    EXPECT_FALSE(strategy.next_stage());
    EXPECT_EQ(strategy.current_index(), 0U);
}

TEST(ColumnGenerationPricingStrategy, MovesToTheNextStageThenComesBack) {
    std::vector<StageRequest> requests;
    Strategy strategy({recording_stage(&requests, false), recording_stage(&requests, true)});
    EXPECT_FALSE(strategy.is_exact());

    EXPECT_TRUE(strategy.next_stage());
    EXPECT_EQ(strategy.current_index(), 1U);
    EXPECT_TRUE(strategy.is_exact());

    EXPECT_FALSE(strategy.next_stage());  // already the last one
    EXPECT_EQ(strategy.current_index(), 1U);

    strategy.reset();
    EXPECT_EQ(strategy.current_index(), 0U);
    EXPECT_FALSE(strategy.is_exact());
}

TEST(ColumnGenerationPricingStrategy, RejectsNoStageAndAnEmptySearch) {
    EXPECT_THROW(Strategy(std::vector<Stage>{}), std::invalid_argument);
    EXPECT_THROW(Stage(Stage::Search{}, true), std::invalid_argument);
}

TEST(ColumnGenerationPricingStrategy, AStageGetsTheWholeRequest) {
    std::vector<StageRequest> requests;
    const Stage stage = recording_stage(&requests, true);
    auto graph = two_path_graph();

    constexpr double upper_bound = -1.0;
    constexpr double timeout_s = 4.0;
    static_cast<void>(stage.search(*graph,
                                   StageRequest{.upper_bound = upper_bound,
                                                .timeout_s = timeout_s,
                                                .preprocess = false,
                                                .cost_index = 0}));

    ASSERT_EQ(requests.size(), 1U);
    EXPECT_EQ(requests[0].upper_bound, upper_bound);
    EXPECT_EQ(requests[0].timeout_s, timeout_s);
    EXPECT_FALSE(requests[0].preprocess);
}

TEST(ColumnGenerationPricingStrategy, AnAlgorithmStageKeepsThePathsBelowTheBound) {
    const Stage stage = Stage::make();
    auto graph = two_path_graph();

    // Between the two path costs: only the path through node 1.
    const SolveResult below = stage.search(*graph, StageRequest{.upper_bound = direct_cost});
    EXPECT_EQ(below.status, AlgorithmStatus::COMPLETE);
    ASSERT_EQ(below.solutions.size(), 1U);
    EXPECT_DOUBLE_EQ(below.solutions[0].cost, through_cost);

    // Below both: nothing, and the search still finished.
    const SolveResult none = stage.search(*graph, StageRequest{.upper_bound = through_cost});
    EXPECT_EQ(none.status, AlgorithmStatus::COMPLETE);
    EXPECT_TRUE(none.solutions.empty());
}

TEST(ColumnGenerationPricingStrategy, TheRequestCapsTheAlgorithmTimeLimit) {
    const Stage stage = Stage::make();  // no time limit of its own
    auto graph = two_path_graph();

    const SolveResult result =
        stage.search(*graph, StageRequest{.upper_bound = direct_cost, .timeout_s = 0.0});

    EXPECT_EQ(result.status, AlgorithmStatus::TIMEOUT);
}
