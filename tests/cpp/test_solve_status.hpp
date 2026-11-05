// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The status a solve reports, and is_optimal() after it, must not depend on whether the labels are
// released when it ends (release_after_solve, true by default). Releasing empties the label
// queues, so anything read from them afterwards reads "nothing left": a timed-out solve looked
// optimal, and a truncated run that used up its phases reported Complete.

#include <gtest/gtest.h>

#include <memory>
#include <utility>

#include "rcspp/rcspp.hpp"

namespace solve_status_test {

using namespace rcspp;

using Resources = ResourceTypeComposition<RealResource, IntResource>;

/// @brief Source 0, sink 3. Node 1 gets two labels neither of which dominates the other: 0 -> 1
///        (cost 1, time 10) and 0 -> 2 -> 1 (cost 2, time 2), so a quota of one extension per node
///        and phase holds labels back.
inline std::unique_ptr<ResourceGraph<RealResource, IntResource>> two_routes() {
    auto graph = std::make_unique<ResourceGraph<RealResource, IntResource>>();
    presets::add_cost_resource<RealResource>(*graph);
    graph->add_resource<IntResource>(
        std::make_unique<AdditionExtensionFunction<IntResource>>(),
        std::make_unique<MinMaxFeasibilityFunction<IntResource>>(0, 100),
        std::make_unique<TrivialCostFunction<IntResource>>(),
        std::make_unique<ValueDominanceFunction<IntResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2);
    graph->add_node(3, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource, IntResource>({1.0, 10}, 0, 1, 1.0);
    graph->add_arc<RealResource, IntResource>({1.0, 1}, 0, 2, 1.0);
    graph->add_arc<RealResource, IntResource>({1.0, 1}, 2, 1, 1.0);
    graph->add_arc<RealResource, IntResource>({1.0, 1}, 1, 3, 1.0);
    return graph;
}

struct Outcome {
        AlgorithmStatus status;
        bool optimal;
};

/// @brief Solves @ref two_routes with @p AlgorithmType and @p params, and reads is_optimal()
///        after the solve.
template <template <typename, typename> class AlgorithmType>
inline Outcome solve(const AlgorithmBaseParams& params) {
    auto graph = two_routes();
    auto algorithm =
        graph->create_algorithm<AlgorithmType>(params.with_container<LabelList<Resources>>());
    const SolveResult result = graph->solve(algorithm.get());
    return {.status = result.status, .optimal = algorithm->is_optimal()};
}

/// @brief One extension per node and phase, and a single phase: labels are held back.
inline AlgorithmBaseParams truncated(bool release_after_solve) {
    AlgorithmBaseParams params;
    params.max_extended_labels_per_node = 1;
    params.max_phases = 1;
    params.release_after_solve = release_after_solve;
    return params;
}

/// @brief The status and is_optimal() of a truncated run, with and without releasing the labels.
template <template <typename, typename> class AlgorithmType>
inline void expect_release_changes_nothing() {
    const Outcome released = solve<AlgorithmType>(truncated(true));
    const Outcome kept = solve<AlgorithmType>(truncated(false));
    EXPECT_EQ(released.status, kept.status);
    EXPECT_EQ(released.optimal, kept.optimal);
    EXPECT_EQ(released.optimal, released.status == AlgorithmStatus::Complete);
}

}  // namespace solve_status_test

TEST(SolveStatus, AFullSolveIsCompleteAndOptimal) {
    namespace sst = solve_status_test;
    const sst::Outcome outcome = sst::solve<rcspp::SimpleDominanceAlgorithm>({});
    EXPECT_EQ(outcome.status, rcspp::AlgorithmStatus::Complete);
    EXPECT_TRUE(outcome.optimal);
}

TEST(SolveStatus, IsOptimalIsFalseBeforeAnySolve) {
    namespace sst = solve_status_test;
    auto graph = sst::two_routes();
    auto algorithm = graph->create_algorithm<rcspp::SimpleDominanceAlgorithm>(
        rcspp::AlgorithmBaseParams{}.with_container<rcspp::LabelList<sst::Resources>>());
    EXPECT_FALSE(algorithm->is_optimal());
}

// Releasing the labels used to make is_optimal() read an empty queue after the timeout.
TEST(SolveStatus, ATimedOutSolveIsNotOptimal) {
    namespace sst = solve_status_test;
    rcspp::AlgorithmBaseParams params;
    params.timeout_s = 0.0;  // stops before the first extension
    const sst::Outcome outcome = sst::solve<rcspp::SimpleDominanceAlgorithm>(params);
    EXPECT_EQ(outcome.status, rcspp::AlgorithmStatus::Timeout);
    EXPECT_FALSE(outcome.optimal);
}

// Releasing the labels used to hide the ones a phase held back: Complete instead of MaxPhases.
TEST(SolveStatus, ATruncatedRunThatUsesUpItsPhasesIsNotComplete) {
    namespace sst = solve_status_test;
    const sst::Outcome outcome = sst::solve<rcspp::SimpleDominanceAlgorithm>(sst::truncated(true));
    EXPECT_EQ(outcome.status, rcspp::AlgorithmStatus::MaxPhases);
    EXPECT_FALSE(outcome.optimal);
}

TEST(SolveStatus, ReleasingTheLabelsChangesNeitherStatusNorOptimality) {
    namespace sst = solve_status_test;
    sst::expect_release_changes_nothing<rcspp::SimpleDominanceAlgorithm>();
    sst::expect_release_changes_nothing<rcspp::PushingDominanceAlgorithm>();
    sst::expect_release_changes_nothing<rcspp::PullingDominanceAlgorithm>();
    sst::expect_release_changes_nothing<rcspp::AStarAlgoBound<rcspp::RealResource>::Algo>();
}
