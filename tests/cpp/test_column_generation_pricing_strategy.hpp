// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Pricing stages and the strategy that moves between them.

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "rcspp/column_generation/pricing_strategy.hpp"
#include "test_column_generation_graphs.hpp"

using namespace rcspp;                              // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation;           // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation::testing;  // NOLINT(google-build-using-namespace)

namespace {

constexpr double through_cost = TwoPathGraph::through_cost;
constexpr double direct_cost = TwoPathGraph::direct_cost;

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
    Strategy strategy({scripted_stage(&requests, false), scripted_stage(&requests, true)});
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
    const Stage stage = scripted_stage(&requests, true);
    auto graph = TwoPathGraph::make();

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
    auto graph = TwoPathGraph::make();

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
    auto graph = TwoPathGraph::make();

    const SolveResult result =
        stage.search(*graph, StageRequest{.upper_bound = direct_cost, .timeout_s = 0.0});

    EXPECT_EQ(result.status, AlgorithmStatus::TIMEOUT);
}
