// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The graph pricer on a two-path graph: reduced costs from the duals, the stages it runs, and
// when its answer is a proof.

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "rcspp/column_generation/graph_pricer.hpp"
#include "test_column_generation_graphs.hpp"

using namespace rcspp;                              // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation;           // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation::testing;  // NOLINT(google-build-using-namespace)

namespace {

using TwoPathPricer = GraphPricer<RealResource>;

// Duals of rows 0 and 1: the path through node 1 then costs 5 − 6 = −1, the direct one 7 − 0.
const std::vector<double> through_improves{6.0, 0.0};
constexpr double through_reduced_cost = -1.0;

// A path found by a scripted stage.
SolveResult one_path(double reduced_cost) {
    SolveResult found;
    found.solutions.emplace_back(reduced_cost, std::vector<size_t>{}, std::vector<size_t>{});
    return found;
}

}  // namespace

TEST(ColumnGenerationGraphPricer, ReturnsThePathsBelowTheThresholdAsColumns) {
    auto graph = TwoPathGraph::make();
    TwoPathPricer pricer(*graph);

    const PricingResult result = pricer.price(through_improves, PricingRequest{});

    ASSERT_EQ(result.columns.size(), 1U);
    EXPECT_DOUBLE_EQ(result.columns[0].cost, through_reduced_cost);
    EXPECT_DOUBLE_EQ(result.columns[0].column.cost, TwoPathGraph::through_cost);  // not reduced
    ASSERT_EQ(result.columns[0].column.rows.size(), 1U);
    EXPECT_EQ(result.columns[0].column.rows[0].index, 0U);
    EXPECT_EQ(result.status, PricingStatus::Optimal);
    EXPECT_TRUE(result.exact_stage);
    EXPECT_DOUBLE_EQ(result.min_reduced_cost_bound, through_reduced_cost);
}

TEST(ColumnGenerationGraphPricer, NothingBelowTheThresholdIsAProof) {
    auto graph = TwoPathGraph::make();
    TwoPathPricer pricer(*graph);

    // Zero duals: both paths cost more than the threshold.
    const std::vector<double> zero{0.0, 0.0};
    const PricingResult result = pricer.price(zero, PricingRequest{.reduced_cost_threshold = -1.0});

    EXPECT_TRUE(result.columns.empty());
    EXPECT_EQ(result.status, PricingStatus::Optimal);
    EXPECT_TRUE(result.exact_stage);
    EXPECT_DOUBLE_EQ(result.min_reduced_cost_bound, -1.0);  // every path is above the threshold
}

TEST(ColumnGenerationGraphPricer, AHeuristicStageThatFindsColumnsStopsTheCall) {
    auto graph = TwoPathGraph::make();
    std::vector<StageRequest> heuristic_requests;
    std::vector<StageRequest> exact_requests;
    Strategy strategy({scripted_stage(&heuristic_requests, false, one_path(through_reduced_cost)),
                       scripted_stage(&exact_requests, true)});
    TwoPathPricer pricer(*graph, std::move(strategy));

    const PricingResult result = pricer.price(through_improves, PricingRequest{});

    EXPECT_EQ(result.columns.size(), 1U);
    EXPECT_TRUE(exact_requests.empty());
    EXPECT_FALSE(result.exact_stage);
    EXPECT_EQ(result.status, PricingStatus::SolverLimit);  // a heuristic proves nothing
    EXPECT_EQ(result.min_reduced_cost_bound, -std::numeric_limits<double>::infinity());
}

TEST(ColumnGenerationGraphPricer, AStageThatFindsNothingHandsOverToTheNext) {
    auto graph = TwoPathGraph::make();
    std::vector<StageRequest> heuristic_requests;
    Strategy strategy({scripted_stage(&heuristic_requests, false), Stage::make()});
    TwoPathPricer pricer(*graph, std::move(strategy));

    const PricingResult first = pricer.price(through_improves, PricingRequest{});
    EXPECT_EQ(heuristic_requests.size(), 1U);
    EXPECT_EQ(first.columns.size(), 1U);
    EXPECT_TRUE(first.exact_stage);
    EXPECT_EQ(first.status, PricingStatus::Optimal);

    // The next call starts again from the heuristic.
    static_cast<void>(pricer.price(through_improves, PricingRequest{}));
    EXPECT_EQ(heuristic_requests.size(), 2U);
}

TEST(ColumnGenerationGraphPricer, AnExactSearchCutShortProvesNothing) {
    auto graph = TwoPathGraph::make();
    std::vector<StageRequest> requests;

    SolveResult timed_out;
    timed_out.status = AlgorithmStatus::TIMEOUT;
    TwoPathPricer stopped(*graph, Strategy({scripted_stage(&requests, true, timed_out)}));
    const PricingResult stopped_result = stopped.price(through_improves, PricingRequest{});
    EXPECT_EQ(stopped_result.status, PricingStatus::SolverLimit);
    EXPECT_EQ(stopped_result.min_reduced_cost_bound, -std::numeric_limits<double>::infinity());

    // A memory-pressure trim can leave the status complete.
    SolveResult trimmed;
    trimmed.memory_pressure_triggered = true;
    TwoPathPricer trimmed_pricer(*graph, Strategy({scripted_stage(&requests, true, trimmed)}));
    EXPECT_EQ(trimmed_pricer.price(through_improves, PricingRequest{}).status,
              PricingStatus::SolverLimit);
}

TEST(ColumnGenerationGraphPricer, NoTimeLeftStopsBeforeTheNextStage) {
    auto graph = TwoPathGraph::make();
    std::vector<StageRequest> heuristic_requests;
    std::vector<StageRequest> exact_requests;
    TwoPathPricer pricer(*graph,
                         Strategy({scripted_stage(&heuristic_requests, false),
                                   scripted_stage(&exact_requests, true)}));

    const PricingResult result = pricer.price(through_improves, PricingRequest{.timeout_s = 0.0});

    EXPECT_EQ(heuristic_requests.size(), 1U);
    EXPECT_TRUE(exact_requests.empty());
    EXPECT_EQ(result.status, PricingStatus::SolverLimit);
}

TEST(ColumnGenerationGraphPricer, PassesItsOptionsToTheStage) {
    auto graph = TwoPathGraph::make();
    std::vector<StageRequest> requests;
    constexpr double threshold = -0.5;

    TwoPathPricer by_default(*graph, Strategy({scripted_stage(&requests, true)}));
    static_cast<void>(
        by_default.price(through_improves, PricingRequest{.reduced_cost_threshold = threshold}));
    TwoPathPricer without_preprocessing(*graph,
                                        Strategy({scripted_stage(&requests, true)}),
                                        0,
                                        false);
    static_cast<void>(without_preprocessing.price(through_improves, PricingRequest{}));

    ASSERT_EQ(requests.size(), 2U);
    EXPECT_EQ(requests[0].upper_bound, threshold);
    EXPECT_TRUE(requests[0].preprocess);
    EXPECT_EQ(requests[0].cost_index, 0U);
    EXPECT_FALSE(requests[1].preprocess);
}

TEST(ColumnGenerationGraphPricer, CostScaleIsTheLargestArcCost) {
    auto graph = TwoPathGraph::make();
    const TwoPathPricer pricer(*graph);
    EXPECT_DOUBLE_EQ(pricer.cost_scale(), TwoPathGraph::direct_cost);
}

TEST(ColumnGenerationGraphPricer, RefusesACostIndexTheGraphDoesNotHave) {
    // One cost component: index 1 would leave the costs as they are and search them.
    auto graph = TwoPathGraph::make();
    TwoPathPricer pricer(*graph, TwoPathPricer::Strategy(), /*cost_index=*/1);

    EXPECT_THROW(static_cast<void>(pricer.price(through_improves, PricingRequest{})),
                 std::out_of_range);
}

TEST(ColumnGenerationGraphPricer, RefusesAGraphWithoutSink) {
    ResourceGraph<RealResource> graph;
    graph.add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                     std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                     std::make_unique<ValueCostFunction<RealResource>>(),
                                     std::make_unique<ValueDominanceFunction<RealResource>>());
    graph.add_node(0, /*source=*/true, /*sink=*/false);
    TwoPathPricer pricer(graph);

    EXPECT_THROW(static_cast<void>(pricer.price({}, PricingRequest{})), std::logic_error);
}
