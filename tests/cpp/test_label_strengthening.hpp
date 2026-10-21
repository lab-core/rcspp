// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The label strengthening hook: which labels it sees, in which direction, and what it reports.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

namespace label_strengthening_test {

using Graph = rcspp::ResourceGraph<rcspp::RealResource>;
using Composed = rcspp::ResourceTypeComposition<rcspp::RealResource>;

/// @brief Records the node of every label it is shown, and reports one change per label.
class RecordingStrengthening : public rcspp::LabelStrengthening<Composed> {
    public:
        size_t strengthen_forward(rcspp::Label<Composed>* label) override {
            seen.push_back(label->get_end_node()->id);
            return 1;
        }

        std::vector<size_t> seen;
};

/// @brief 0 (source), 1, 2, 3 (sink). Node 2 closes at 3, so 0-1-2, which reaches it at 6, is
///        infeasible, while 0-2 reaches it at 1.
inline std::unique_ptr<Graph> diamond() {
    using rcspp::RealResource;
    const std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}},
                                                              {1, {0.0, 100.0}},
                                                              {2, {0.0, 3.0}},
                                                              {3, {0.0, 100.0}}};
    auto graph = std::make_unique<Graph>();
    graph->add_resource<RealResource>(
        std::make_unique<rcspp::AdditionExtensionFunction<RealResource>>(),
        std::make_unique<rcspp::TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<rcspp::ValueCostFunction<RealResource>>(),
        std::make_unique<rcspp::ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<rcspp::TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<rcspp::TimeWindowFeasibilityFunction<RealResource>>(windows),
        std::make_unique<rcspp::TrivialCostFunction<RealResource>>(),
        std::make_unique<rcspp::ValueDominanceFunction<RealResource>>());
    for (size_t node_id = 0; node_id < 4; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id == 3);
    }
    // {origin, destination, cost, time}
    const std::vector<std::tuple<size_t, size_t, double, double>> arcs{{0, 1, 1.0, 1.0},
                                                                       {0, 2, 4.0, 1.0},
                                                                       {1, 2, 1.0, 5.0},
                                                                       {1, 3, 5.0, 1.0},
                                                                       {2, 3, 1.0, 1.0}};
    for (const auto& [origin, destination, cost, time] : arcs) {
        graph->add_arc<RealResource, RealResource>({cost, time}, origin, destination, cost);
    }
    return graph;
}

}  // namespace label_strengthening_test

/// @brief The hook sees every feasible forward label once, and never the infeasible one.
TEST(LabelStrengthening, SeesEveryFeasibleForwardLabelOnce) {
    namespace lst = label_strengthening_test;
    auto graph = lst::diamond();
    auto algorithm = graph->create_algorithm<rcspp::SimpleDominanceAlgorithm>(
        rcspp::AlgorithmParams<rcspp::LabelList<lst::Composed>>{});
    lst::RecordingStrengthening recorder;
    algorithm->set_label_strengthening(&recorder);
    // Without preprocessing, which would remove the arc 1-2 that no path can use in time.
    const auto result = graph->solve(algorithm.get(),
                                     std::numeric_limits<double>::infinity(),
                                     /*preprocess=*/false);

    // Extensions: 0-1, 0-2, 1-2 (infeasible: 2 closes at 3), 1-3, 2-3. The four feasible ones
    // land on 1, 2, 3 and 3.
    auto seen = recorder.seen;
    std::ranges::sort(seen);
    EXPECT_EQ(seen, (std::vector<size_t>{1, 2, 3, 3}));
    EXPECT_EQ(algorithm->get_number_of_extended_labels(), 5U);
    EXPECT_EQ(result.unreachable_folds, recorder.seen.size());
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_DOUBLE_EQ(result.solutions.front().cost, 5.0);
}

/// @brief A backward search is not shown to the hook.
TEST(LabelStrengthening, IsNotAppliedBackward) {
    namespace lst = label_strengthening_test;
    auto graph = lst::diamond();
    rcspp::AlgorithmParams<rcspp::LabelList<lst::Composed>> params;
    params.direction = rcspp::SearchDirection::Backward;
    auto algorithm = graph->create_algorithm<rcspp::SimpleDominanceAlgorithm>(params);
    lst::RecordingStrengthening recorder;
    algorithm->set_label_strengthening(&recorder);
    const auto result = graph->solve(algorithm.get());

    EXPECT_TRUE(recorder.seen.empty());
    EXPECT_EQ(result.unreachable_folds, 0U);
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_DOUBLE_EQ(result.solutions.front().cost, 5.0);
}

/// @brief Without a hook, nothing is counted, and removing one restores the plain solve.
TEST(LabelStrengthening, NoneMeansNoChange) {
    namespace lst = label_strengthening_test;
    auto graph = lst::diamond();
    auto algorithm = graph->create_algorithm<rcspp::SimpleDominanceAlgorithm>(
        rcspp::AlgorithmParams<rcspp::LabelList<lst::Composed>>{});
    const auto plain = graph->solve(algorithm.get());
    const size_t plain_extended = algorithm->get_number_of_extended_labels();
    EXPECT_EQ(plain.unreachable_folds, 0U);

    lst::RecordingStrengthening recorder;
    algorithm->set_label_strengthening(&recorder);
    (void)graph->solve(algorithm.get());
    algorithm->set_label_strengthening(nullptr);
    const auto again = graph->solve(algorithm.get());

    EXPECT_EQ(again.unreachable_folds, 0U);
    EXPECT_EQ(algorithm->get_number_of_extended_labels(), plain_extended);
    ASSERT_EQ(again.solutions.size(), plain.solutions.size());
    EXPECT_DOUBLE_EQ(again.solutions.front().cost, plain.solutions.front().cost);
}
