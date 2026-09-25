// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The column validator catches each fault it claims to catch, and accepts what a solve returns.

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/column_validator.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace column_validator_test {

/// @brief Source 0, sinks 3 and 4; node 2's window closes at 8.
///
/// Arcs (cost, time): 0->1 (1, 5), 1->2 (1, 5), 0->2 (4, 1), 2->3 (1, 1), 1->3 (5, 1), 3->4 (0, 0).
/// So 0 1 2 3 arrives at node 2 at time 10, past its window.
inline std::unique_ptr<ResourceGraph<RealResource>> graph() {
    auto built = std::make_unique<ResourceGraph<RealResource>>();
    const std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}},
                                                              {1, {0.0, 100.0}},
                                                              {2, {0.0, 8.0}},
                                                              {3, {0.0, 100.0}},
                                                              {4, {0.0, 100.0}}};
    presets::add_cost_resource<RealResource>(*built);
    presets::add_window_resource<RealResource>(*built, windows);
    built->add_node(0, true, false);
    built->add_node(1);
    built->add_node(2);
    built->add_node(3, false, true);
    built->add_node(4, false, true);
    built->add_arc<RealResource, RealResource>({1.0, 5.0}, 0, 1, 1.0);  // arc 0
    built->add_arc<RealResource, RealResource>({1.0, 5.0}, 1, 2, 1.0);  // arc 1
    built->add_arc<RealResource, RealResource>({4.0, 1.0}, 0, 2, 4.0);  // arc 2
    built->add_arc<RealResource, RealResource>({1.0, 1.0}, 2, 3, 1.0);  // arc 3
    built->add_arc<RealResource, RealResource>({5.0, 1.0}, 1, 3, 5.0);  // arc 4
    built->add_arc<RealResource, RealResource>({0.0, 0.0}, 3, 4, 0.0);  // arc 5
    return built;
}

/// @brief A column over @p arcs claiming @p cost, with a consistent `column.cost`.
inline Solution column(const ResourceGraph<RealResource>& graph, std::vector<size_t> arcs,
                       double cost) {
    Solution solution(cost, {}, std::move(arcs));
    for (const size_t arc_id : solution.path_arc_ids) {
        solution.column.cost += graph.get_arc(arc_id)->cost;
    }
    return solution;
}

}  // namespace column_validator_test

TEST(ColumnValidator, AcceptsWhatAnExactSolveReturns) {
    namespace cv = column_validator_test;
    auto graph = cv::graph();
    const auto result = graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    ASSERT_FALSE(result.solutions.empty());
    const auto issues = test_util::validate_columns(*graph, result.solutions);
    EXPECT_TRUE(issues.empty()) << test_util::describe(issues);
}

TEST(ColumnValidator, AcceptsAValidHandBuiltColumn) {
    namespace cv = column_validator_test;
    auto graph = cv::graph();
    EXPECT_EQ(test_util::check_column(*graph, cv::column(*graph, {2, 3}, 5.0)), "");
    EXPECT_EQ(test_util::check_column(*graph, cv::column(*graph, {0, 4}, 6.0)), "");
}

TEST(ColumnValidator, RejectsEachFaultItClaimsToCatch) {
    namespace cv = column_validator_test;
    auto graph = cv::graph();

    // Wrong cost.
    EXPECT_NE(test_util::check_column(*graph, cv::column(*graph, {2, 3}, 4.0)), "");
    // Empty.
    EXPECT_NE(test_util::check_column(*graph, cv::column(*graph, {}, 0.0)), "");
    // Not contiguous: 0->1 then 2->3.
    EXPECT_NE(test_util::check_column(*graph, cv::column(*graph, {0, 3}, 2.0)), "");
    // Does not start at a source: 1->2, 2->3.
    EXPECT_NE(test_util::check_column(*graph, cv::column(*graph, {1, 3}, 2.0)), "");
    // Does not end at a sink: 0->1, 1->2.
    EXPECT_NE(test_util::check_column(*graph, cv::column(*graph, {0, 1}, 2.0)), "");
    // Passes through sink 3 on its way to sink 4.
    EXPECT_NE(test_util::check_column(*graph, cv::column(*graph, {2, 3, 5}, 5.0)), "");
    // Infeasible: reaches node 2 at time 10, past its window.
    EXPECT_NE(test_util::check_column(*graph, cv::column(*graph, {0, 1, 3}, 3.0)), "");
    // An arc id the graph does not have.
    EXPECT_NE(test_util::check_column(*graph, Solution(0.0, {}, {99})), "");

    // column.cost disagrees with the arcs.
    auto wrong_column = cv::column(*graph, {2, 3}, 5.0);
    wrong_column.column.cost += 1.0;
    EXPECT_NE(test_util::check_column(*graph, wrong_column), "");

    // Returned twice.
    const auto issues = test_util::validate_columns(
        *graph,
        std::vector<Solution>{cv::column(*graph, {2, 3}, 5.0), cv::column(*graph, {2, 3}, 5.0)});
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues.front().column, 1U);
}
