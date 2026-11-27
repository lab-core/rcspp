// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// What the loop tells about each iteration: the statistics switches and the fields they fill.

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "rcspp/column_generation/column_generation.hpp"
#include "rcspp/column_generation/graph_pricer.hpp"
#include "test_column_generation_fakes.hpp"
#include "test_column_generation_graphs.hpp"

using namespace rcspp;                              // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation;           // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation::testing;  // NOLINT(google-build-using-namespace)

namespace rcspp::column_generation::testing {

// One row "cover >= 1" with an artificial column, and a pricer with a convexity row (at most 1
// path) that offers a path of cost 10 while its reduced cost is negative. First LP: the artificial
// column, value 100; second: the path, value 10. The convexity dual is 2 in both.
struct StatisticsToy {
        static constexpr double path_cost = 10.0;
        static constexpr double artificial_cost = 100.0;
        static constexpr double convexity_dual = 2.0;

        StatisticsToy() {
            master.add_rows(1, RowSense::GreaterEqual, 1.0);
            master.add_pricer(pricer, ConvexityRow{.max_paths = 1.0});
            const std::vector<size_t> rows{0};
            const std::vector<double> coefficients{1.0};
            master.add_artificial_columns(rows, coefficients, artificial_cost);
            solver.on_solve = [](ScriptedMasterSolver& lp) {
                const bool has_path = lp.columns.size() > 1;
                lp.objective_value = has_path ? path_cost : artificial_cost;
                // With the path, its reduced cost is 0: 10 - (10 - 2) - 2.
                lp.dual_values = {has_path ? path_cost - convexity_dual : artificial_cost,
                                  convexity_dual};
                lp.values = has_path ? std::unordered_map<ColumnId, double>{{1, 1.0}}
                                     : std::unordered_map<ColumnId, double>{{0, 1.0}};
                return SolverStatus::Optimal;
            };
            pricer.respond = [this](std::span<const double> duals, const PricingRequest& request) {
                // The model forgets its solution once it changes: the loop must have read it.
                solver.values.clear();
                PricingResult result = ScriptedPricer::proven_none();
                const double reduced_cost = path_cost - duals[0];
                if (reduced_cost < request.reduced_cost_threshold) {
                    result.columns.push_back(
                        make_path(path_cost, {{.index = 0, .coefficient = 1.0}}, reduced_cost));
                }
                return result;
            };
            params.initialization = std::make_shared<NoInitialization>();
            params.on_iteration = [this](const IterationInfo& info) { seen.push_back(info); };
        }

        ColumnGenerationResult run() {
            ColumnGeneration loop(master, params);
            return loop.run();
        }

        ScriptedMasterSolver solver;
        MasterProblem master{solver};
        ScriptedPricer pricer;
        ColumnGenerationParams params;
        std::vector<IterationInfo> seen;
};

}  // namespace rcspp::column_generation::testing

TEST(ColumnGenerationStatistics, ByDefaultOnlyTheTimesAreAdded) {
    StatisticsToy toy;
    const ColumnGenerationResult result = toy.run();

    ASSERT_EQ(result.iterations, 2U);
    double relaxation = 0.0;
    double pricing = 0.0;
    for (const IterationInfo& info : toy.seen) {
        ASSERT_TRUE(info.relaxation_seconds.has_value());
        ASSERT_TRUE(info.pricing_seconds.has_value());
        relaxation += *info.relaxation_seconds;
        pricing += *info.pricing_seconds;
        EXPECT_FALSE(info.pricers.has_value());
        EXPECT_FALSE(info.duals.has_value());
        EXPECT_FALSE(info.master_values.has_value());
        EXPECT_FALSE(info.added_columns.has_value());
    }
    // The iterations' times add up to the run's.
    EXPECT_NEAR(relaxation, result.statistics.relaxation_seconds, 1e-9);
    EXPECT_NEAR(pricing, result.statistics.pricing_seconds, 1e-9);
    for (const PricingRequest& request : toy.pricer.requests) {
        EXPECT_FALSE(request.search_details);
    }
}

TEST(ColumnGenerationStatistics, WithoutTimersThereAreNoTimes) {
    StatisticsToy toy;
    toy.params.statistics.timers = false;
    (void)toy.run();

    for (const IterationInfo& info : toy.seen) {
        EXPECT_FALSE(info.relaxation_seconds.has_value());
        EXPECT_FALSE(info.pricing_seconds.has_value());
    }
}

TEST(ColumnGenerationStatistics, EachPricerSaysWhatItDid) {
    StatisticsToy toy;
    toy.params.statistics.pricers = true;
    (void)toy.run();

    ASSERT_EQ(toy.seen.size(), 2U);
    ASSERT_TRUE(toy.seen[0].pricers.has_value());
    ASSERT_EQ(toy.seen[0].pricers->size(), 1U);
    const PricerIterationInfo& first = toy.seen[0].pricers->front();
    EXPECT_EQ(first.status, PricingStatus::Optimal);
    EXPECT_EQ(first.new_columns, 1U);
    // The path's reduced cost 10 - 100, minus the convexity dual.
    ASSERT_TRUE(first.min_reduced_cost.has_value());
    EXPECT_DOUBLE_EQ(
        *first.min_reduced_cost,
        StatisticsToy::path_cost - StatisticsToy::artificial_cost - StatisticsToy::convexity_dual);
    EXPECT_GE(first.seconds, 0.0);
    EXPECT_TRUE(first.search_details.empty());  // a pricer without stages

    ASSERT_TRUE(toy.seen[1].pricers.has_value());
    EXPECT_EQ(toy.seen[1].pricers->front().new_columns, 0U);
    EXPECT_FALSE(toy.seen[1].pricers->front().min_reduced_cost.has_value());
    for (const PricingRequest& request : toy.pricer.requests) {
        EXPECT_TRUE(request.search_details);
    }
}

TEST(ColumnGenerationStatistics, TheSearchCountsNeedThePricers) {
    StatisticsToy toy;
    toy.params.statistics.search = true;
    EXPECT_THROW({ const ColumnGeneration loop(toy.master, toy.params); }, std::invalid_argument);

    toy.params.statistics.pricers = true;
    EXPECT_NO_THROW({ const ColumnGeneration loop(toy.master, toy.params); });
}

TEST(ColumnGenerationStatistics, TheDualsAreTheOnesPriced) {
    StatisticsToy toy;
    toy.params.statistics.duals = true;
    (void)toy.run();

    ASSERT_TRUE(toy.seen[0].duals.has_value());
    EXPECT_EQ(*toy.seen[0].duals,
              (std::vector<double>{StatisticsToy::artificial_cost, StatisticsToy::convexity_dual}));
    EXPECT_EQ(*toy.seen[0].duals, toy.pricer.seen_duals[0]);
}

TEST(ColumnGenerationStatistics, TheMasterValuesAreReadBeforeTheColumnsGoIn) {
    StatisticsToy toy;
    toy.params.statistics.master_values = true;
    (void)toy.run();

    ASSERT_EQ(toy.seen.size(), 2U);
    ASSERT_TRUE(toy.seen[0].master_values.has_value());
    EXPECT_EQ(toy.seen[0].master_values->first, (std::vector<ColumnId>{0}));
    EXPECT_EQ(toy.seen[0].master_values->second, (std::vector<double>{1.0}));
    ASSERT_TRUE(toy.seen[1].master_values.has_value());
    EXPECT_EQ(toy.seen[1].master_values->first, (std::vector<ColumnId>{1}));
    EXPECT_EQ(toy.seen[1].master_values->second, (std::vector<double>{1.0}));
}

TEST(ColumnGenerationStatistics, TheAddedColumnsAreTheMasters) {
    StatisticsToy toy;
    toy.params.statistics.columns = true;
    (void)toy.run();

    ASSERT_TRUE(toy.seen[0].added_columns.has_value());
    ASSERT_EQ(toy.seen[0].added_columns->size(), 1U);
    const MasterColumn& added = toy.seen[0].added_columns->front();
    EXPECT_EQ(added.id, 1U);
    EXPECT_EQ(added.pricer, 0U);
    EXPECT_EQ(added.tag, ColumnTag::Real);
    EXPECT_DOUBLE_EQ(added.solution.column.cost, StatisticsToy::path_cost);
    ASSERT_TRUE(toy.seen[1].added_columns.has_value());
    EXPECT_TRUE(toy.seen[1].added_columns->empty());
}

TEST(ColumnGenerationStatistics, TheSwitchesChangeNothingElse) {
    StatisticsToy plain;
    plain.params.statistics.timers = false;
    const ColumnGenerationResult plain_result = plain.run();

    StatisticsToy full;
    full.params.statistics = StatisticsParams{.timers = true,
                                              .pricers = true,
                                              .search = true,
                                              .duals = true,
                                              .master_values = true,
                                              .columns = true};
    const ColumnGenerationResult full_result = full.run();

    EXPECT_EQ(plain_result.status, full_result.status);
    EXPECT_EQ(plain_result.objective, full_result.objective);
    EXPECT_EQ(plain_result.iterations, full_result.iterations);
    ASSERT_EQ(plain.seen.size(), full.seen.size());
    for (size_t index = 0; index < plain.seen.size(); ++index) {
        EXPECT_EQ(plain.seen[index].lower_bound, full.seen[index].lower_bound);
    }
}

TEST(ColumnGenerationStatistics, AGraphPricerListsTheStagesItRanWhenAsked) {
    auto graph = TwoPathGraph::make();
    GraphPricer<RealResource> pricer(*graph);
    // The path through node 1 costs 5 - 6 = -1.
    const std::vector<double> duals{6.0, 0.0};

    EXPECT_TRUE(pricer.price(duals, PricingRequest{}).search_details.empty());

    const PricingResult result = pricer.price(duals, PricingRequest{.search_details = true});
    ASSERT_EQ(result.search_details.size(), 1U);
    const PricingStageInfo& stage = result.search_details.front();
    EXPECT_EQ(stage.stage, 0U);
    EXPECT_EQ(stage.status, AlgorithmStatus::Complete);
    EXPECT_EQ(stage.new_columns, result.columns.size());
    EXPECT_GE(stage.seconds, 0.0);
    ASSERT_TRUE(stage.extended_labels.has_value());
    EXPECT_GT(*stage.extended_labels, 0U);
    EXPECT_FALSE(stage.bidirectional.has_value());
}
