// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Every labelling strategy that searches bidirectionally, on what each side's schedule does with
// a cut: in a bidirectional search it abandons the label, which is never extended, since the
// search runs one phase.
//
// Its own translation unit: each strategy's bidirectional instantiation.

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>

#include "rcspp/rcspp.hpp"
#include "util/equivalence_helpers.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace bidirectional_strategies_test {

using Composed = ResourceTypeComposition<RealResource>;
using List = LabelList<Composed>;

/// @brief Six hops of two parallel arcs, one cheap and slow and one dear and quick, so every node
///        holds many non-dominated labels and a quota of 2 has plenty to refuse; the second
///        resource is the clock.
inline std::unique_ptr<ResourceGraph<RealResource>> fan() {
    constexpr size_t kHops = 6;
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<CapacityExtensionFunction<RealResource>>(1000.0),
        std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 1000.0),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    for (size_t node_id = 0; node_id <= kHops; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id == kHops);
    }
    for (size_t node_id = 0; node_id < kHops; ++node_id) {
        graph->add_arc<RealResource, RealResource>({1.0, 4.0}, node_id, node_id + 1, 1.0);
        graph->add_arc<RealResource, RealResource>({3.0, 1.0}, node_id, node_id + 1, 3.0);
    }
    return graph;
}

/// @brief Bidirectional params on the fan's clock, with a per-node quota of @p quota.
inline AlgorithmParams<List> params(size_t quota) {
    AlgorithmParams<List> result;
    result.direction = SearchDirection::Bidirectional;
    result.critical_resource_index = 1;
    result.half_way_point = 6.0;
    result.max_extended_labels_per_node = quota;
    return result;
}

/// @brief Expects a bidirectional @p Strategy under a quota of 2 to abandon labels, so the solve
///        is not Complete, and still to return only paths that replay through the model.
template <typename Strategy>
void expect_a_quota_abandons_and_the_result_stays_a_path(const char* name) {
    auto graph = fan();
    auto algorithm = graph->template create_algorithm<Strategy>(params(2));
    const auto result = graph->solve(algorithm.get());
    EXPECT_EQ(result.status, AlgorithmStatus::Truncated) << name << ": a quota of 2 abandons";
    EXPECT_FALSE(algorithm->is_optimal()) << name;
    ASSERT_FALSE(result.solutions.empty()) << name;
    for (const auto& solution : result.solutions) {
        EXPECT_EQ(equivalence_test::path_problem(*graph, solution), "") << name;
    }
}

/// @brief Expects a bidirectional @p Strategy that keeps its labels between solves
///        (`release_after_solve = false`) to find forward Simple's optimum twice in a row.
template <typename Strategy>
void expect_a_second_solve_finds_the_same_optimum(const char* name) {
    auto graph = fan();
    const auto forward = graph->template solve<strategy::Simple>(AlgorithmBaseParams{});
    ASSERT_FALSE(forward.solutions.empty());

    auto kept = params(kNoLimit);
    kept.release_after_solve = false;
    auto algorithm = graph->template create_algorithm<Strategy>(kept);
    for (const char* solve : {"first", "second"}) {
        const auto result = graph->solve(algorithm.get());
        EXPECT_EQ(result.status, AlgorithmStatus::Complete) << name << ", " << solve;
        ASSERT_FALSE(result.solutions.empty()) << name << ", " << solve;
        EXPECT_DOUBLE_EQ(result.solutions.front().cost, forward.solutions.front().cost)
            << name << ", " << solve;
        EXPECT_EQ(result.solutions.front().path_arc_ids, forward.solutions.front().path_arc_ids)
            << name << ", " << solve;
    }
}

}  // namespace bidirectional_strategies_test

/// @brief In every bidirectional search, a quota abandons labels and the paths stay paths.
TEST(BidirectionalStrategies, AQuotaAbandonsAndTheResultStaysAPath) {
    namespace bst = bidirectional_strategies_test;
    bst::expect_a_quota_abandons_and_the_result_stays_a_path<strategy::Simple>("Simple");
    bst::expect_a_quota_abandons_and_the_result_stays_a_path<strategy::AStar>("A*");
}

/// @brief Every bidirectional search starts each solve afresh: kept labels change no optimum.
TEST(BidirectionalStrategies, ASecondSolveWithoutReleaseFindsTheSameOptimum) {
    namespace bst = bidirectional_strategies_test;
    bst::expect_a_second_solve_finds_the_same_optimum<strategy::Simple>("Simple");
    bst::expect_a_second_solve_finds_the_same_optimum<strategy::AStar>("A*");
}
