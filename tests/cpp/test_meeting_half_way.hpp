// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// HalfWayMode::Meeting: a half-way point placed where the two searches meet (Tilk et al. 2017).
// The two hand traces are in analysis/dynamic_half_way/meeting-half-way-implementation-plan.md,
// Appendix A.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "test_bidirectional_validation.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace meeting_half_way_test {

/// @brief Params for a meeting solve on a `clocked_graph` (clock in slot 1).
inline AlgorithmParams<LabelList<bidirectional_validation_test::Composed>> meeting_params() {
    auto params = bidirectional_validation_test::clocked_params(/*half_way_point=*/0.0);
    params.half_way_mode = HalfWayMode::Meeting;
    params.release_after_solve = false;
    return params;
}

/// @brief The forward search's optimum, for comparison.
inline double forward_optimum(ResourceGraph<RealResource>* graph) {
    const auto result = graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    return result.solutions.empty() ? std::numeric_limits<double>::infinity()
                                    : result.solutions.front().cost;
}

}  // namespace meeting_half_way_test

/// @brief Appendix A.1: on a line, the searches meet at 20 and the join builds the path.
TEST(MeetingHalfWay, OnALineTheSearchesMeetAndTheJoinBuildsThePath) {
    namespace bv = bidirectional_validation_test;
    namespace mh = meeting_half_way_test;
    auto graph = bv::join_only_graph();
    auto algorithm = graph->create_algorithm<SimpleDominanceAlgorithm>(
        test_util::bidirectional(mh::meeting_params()));
    const SolveResult result = graph->solve(algorithm.get());

    ASSERT_EQ(result.status, AlgorithmStatus::COMPLETE);
    ASSERT_TRUE(result.bidirectional->bounded_by_half_way)
        << result.bidirectional->half_way_off_reason;
    EXPECT_EQ(result.bidirectional->half_way_forward_bound, 20.0);
    EXPECT_EQ(result.bidirectional->half_way_backward_bound, 20.0);
    EXPECT_EQ(result.bidirectional->half_way_point_used, 20.0);
    ASSERT_EQ(result.solutions.size(), 1U);
    EXPECT_NEAR(result.solutions.front().cost, 4.0, bv::kTolerance);
    EXPECT_EQ(result.solutions.front().path_arc_ids, (std::vector<size_t>{0, 1, 2, 3}));
    EXPECT_EQ(result.bidirectional->number_of_joined_paths, 1U);
    EXPECT_FALSE(result.bidirectional->one_search_complete) << "both searches stopped a label here";
    EXPECT_TRUE(algorithm->get_label_pool().check_ref_count_consistency());
}

/// @brief Appendix A.2: with slack the searches pass each other; the bounds still end equal.
TEST(MeetingHalfWay, WhenTheSearchesNeverMeetTheBoundsSnapTogether) {
    namespace bv = bidirectional_validation_test;
    namespace mh = meeting_half_way_test;
    // s(window [0, 0]) -> t(window [0, 10]), travel time 2, cost 1.
    auto graph = bv::clocked_graph({{0, {0.0, 0.0}}, {1, {0.0, 10.0}}}, {{1.0, 2.0, 0, 1}});
    auto algorithm = graph->create_algorithm<SimpleDominanceAlgorithm>(
        test_util::bidirectional(mh::meeting_params()));
    const SolveResult result = graph->solve(algorithm.get());

    ASSERT_EQ(result.status, AlgorithmStatus::COMPLETE);
    ASSERT_TRUE(result.bidirectional->bounded_by_half_way)
        << result.bidirectional->half_way_off_reason;
    EXPECT_EQ(result.bidirectional->half_way_forward_bound, 0.0);
    EXPECT_EQ(result.bidirectional->half_way_backward_bound, 0.0);
    ASSERT_EQ(result.solutions.size(), 1U);
    EXPECT_NEAR(result.solutions.front().cost, 1.0, bv::kTolerance);
    EXPECT_EQ(result.bidirectional->number_of_joined_paths, 0U)
        << "a search reached a terminal on its own";
    EXPECT_FALSE(result.bidirectional->one_search_complete)
        << "each direction retired with a label past its bound still waiting, not empty";
}

/// @brief Appendix A.4: a search that runs out while the bounds are apart was complete, so the
///        other one is stopped and the join skipped.
TEST(MeetingHalfWay, AFrontierThatEmptiesBeforeAnyStopEndsTheSolve) {
    namespace bv = bidirectional_validation_test;
    namespace mh = meeting_half_way_test;
    // 0 -> 1 -> 2, travel 1 and cost 1 each, every window [0, 100]: so much slack that the
    // forward search reaches the sink long before it gets near the backward search.
    auto graph = bv::clocked_graph({{0, {0.0, 100.0}}, {1, {0.0, 100.0}}, {2, {0.0, 100.0}}},
                                   {{1.0, 1.0, 0, 1}, {1.0, 1.0, 1, 2}});
    auto algorithm = graph->create_algorithm<SimpleDominanceAlgorithm>(
        test_util::bidirectional(mh::meeting_params()));
    const SolveResult result = graph->solve(algorithm.get());

    ASSERT_EQ(result.status, AlgorithmStatus::COMPLETE);
    ASSERT_TRUE(result.bidirectional->bounded_by_half_way)
        << result.bidirectional->half_way_off_reason;
    EXPECT_TRUE(result.bidirectional->one_search_complete);
    EXPECT_EQ(result.bidirectional->half_way_forward_bound, 99.0);
    EXPECT_EQ(result.bidirectional->half_way_backward_bound, 99.0);
    EXPECT_EQ(result.bidirectional->join_pairs_tested, 0U) << "the join must be skipped";
    EXPECT_EQ(result.bidirectional->number_of_joined_paths, 0U);
    ASSERT_EQ(result.solutions.size(), 1U);
    EXPECT_NEAR(result.solutions.front().cost, 2.0, bv::kTolerance);
    EXPECT_TRUE(algorithm->get_label_pool().check_ref_count_consistency());
}

/// @brief The meeting mode needs no half_way_point: 0 does not turn it off.
TEST(MeetingHalfWay, NeedsNoHalfWayPoint) {
    namespace bv = bidirectional_validation_test;
    namespace mh = meeting_half_way_test;
    auto graph = bv::two_join_graph();
    auto params = mh::meeting_params();
    ASSERT_EQ(params.half_way_point, 0.0);
    auto algorithm =
        graph->create_algorithm<SimpleDominanceAlgorithm>(test_util::bidirectional(params));
    const SolveResult result = graph->solve(algorithm.get());
    EXPECT_TRUE(result.bidirectional->bounded_by_half_way)
        << result.bidirectional->half_way_off_reason;

    // The forward search's optimum is 2 (route 0-1-3-4-6), as the fixed mode finds.
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions.front().cost, 2.0, bv::kTolerance);
}

/// @brief Both direction rules find the same optimum as the forward search.
TEST(MeetingHalfWay, BothDirectionRulesMatchTheForwardSearch) {
    namespace bv = bidirectional_validation_test;
    namespace mh = meeting_half_way_test;
    for (const auto rule : {HalfWayDirectionRule::FewestWaiting, HalfWayDirectionRule::LeastWork}) {
        auto graph = bv::two_join_graph();
        const double expected = mh::forward_optimum(graph.get());
        auto params = mh::meeting_params();
        params.half_way_direction_rule = rule;
        auto algorithm =
            graph->create_algorithm<SimpleDominanceAlgorithm>(test_util::bidirectional(params));
        const SolveResult result = graph->solve(algorithm.get());
        ASSERT_FALSE(result.solutions.empty());
        EXPECT_NEAR(result.solutions.front().cost, expected, bv::kTolerance);
        EXPECT_EQ(result.bidirectional->half_way_forward_bound,
                  result.bidirectional->half_way_backward_bound);
    }
}

/// @brief Appendix A.3: a memory-limit stop still joins, at H_B, the halves both searches hold.
TEST(MeetingHalfWay, AMemoryLimitStopStillJoinsAtTheBackwardBound) {
    namespace bv = bidirectional_validation_test;
    namespace mh = meeting_half_way_test;
    const auto run = [](bool join_after_early_stop) {
        auto graph = bv::join_only_graph();
        auto params = mh::meeting_params();
        params.max_memory_gb = 1e-9;  // always exceeded
        params.memory_check_interval = 4;
        params.join_after_early_stop = join_after_early_stop;
        auto algorithm =
            graph->create_algorithm<SimpleDominanceAlgorithm>(test_util::bidirectional(params));
        const SolveResult result = graph->solve(algorithm.get());
        EXPECT_TRUE(algorithm->get_label_pool().check_ref_count_consistency());
        return result;
    };

    const SolveResult joined = run(/*join_after_early_stop=*/true);
    ASSERT_EQ(joined.status, AlgorithmStatus::MEMORY_LIMIT)
        << "the limit did not stop the search, so this test proves nothing";
    EXPECT_EQ(joined.bidirectional->half_way_backward_bound, 10.0);
    EXPECT_EQ(joined.bidirectional->half_way_forward_bound, 30.0);
    EXPECT_EQ(joined.bidirectional->half_way_point_used, 10.0);
    ASSERT_EQ(joined.bidirectional->number_of_joined_paths, 1U);
    ASSERT_FALSE(joined.solutions.empty());
    EXPECT_NEAR(joined.solutions.front().cost, 4.0, bv::kTolerance);
    EXPECT_EQ(joined.solutions.front().path_arc_ids, (std::vector<size_t>{0, 1, 2, 3}));

    const SolveResult skipped = run(/*join_after_early_stop=*/false);
    ASSERT_EQ(skipped.status, AlgorithmStatus::MEMORY_LIMIT);
    EXPECT_EQ(skipped.bidirectional->number_of_joined_paths, 0U);
}

/// @brief An interrupt skips the join, as in the fixed mode.
TEST(MeetingHalfWay, AnInterruptSkipsTheJoin) {
    namespace bv = bidirectional_validation_test;
    namespace mh = meeting_half_way_test;
    auto graph = bv::join_only_graph();
    size_t calls = 0;
    auto params = mh::meeting_params();
    params.should_stop = [&calls]() { return ++calls > 2; };
    auto algorithm =
        graph->create_algorithm<SimpleDominanceAlgorithm>(test_util::bidirectional(params));
    const SolveResult result = graph->solve(algorithm.get());
    ASSERT_EQ(result.status, AlgorithmStatus::INTERRUPTED);
    EXPECT_EQ(result.bidirectional->number_of_joined_paths, 0U);
    EXPECT_TRUE(algorithm->get_label_pool().check_ref_count_consistency());
}

/// @brief A per-node quota falls back to the fixed mode, which reads half_way_point.
TEST(MeetingHalfWay, AQuotaFallsBackToTheFixedMode) {
    namespace bv = bidirectional_validation_test;
    namespace mh = meeting_half_way_test;
    auto graph = bv::join_only_graph();
    auto params = mh::meeting_params();
    params.num_labels_to_extend_by_node = 5;
    params.half_way_point = 20.0;
    auto algorithm =
        graph->create_algorithm<SimpleDominanceAlgorithm>(test_util::bidirectional(params));
    const SolveResult result = graph->solve(algorithm.get());
    ASSERT_TRUE(result.bidirectional->bounded_by_half_way);
    EXPECT_EQ(result.bidirectional->half_way_forward_bound, 20.0)
        << "the fixed H, not a meeting point";
    EXPECT_EQ(result.bidirectional->half_way_backward_bound, 20.0);

    params.half_way_point = 0.0;
    auto without_h =
        graph->create_algorithm<SimpleDominanceAlgorithm>(test_util::bidirectional(params));
    EXPECT_FALSE(graph->solve(without_h.get()).bidirectional->bounded_by_half_way)
        << "the fallback is the fixed mode, which needs half_way_point";
}

/// @brief dynamic_half_way is ignored: the controller is never seeded.
TEST(MeetingHalfWay, TheBetweenSolvesControllerStaysUnseeded) {
    namespace bv = bidirectional_validation_test;
    namespace mh = meeting_half_way_test;
    auto graph = bv::join_only_graph();
    auto params = mh::meeting_params();
    params.dynamic_half_way = true;
    params.half_way_point = 20.0;
    auto algorithm =
        graph->create_algorithm<SimpleDominanceAlgorithm>(test_util::bidirectional(params));
    for (int solve = 0; solve < 3; ++solve) {
        static_cast<void>(graph->solve(algorithm.get()));
    }
    EXPECT_FALSE(as_bidirectional(algorithm.get())->half_way_controller().has_initial_h());
}

/// @brief A clock index that names no component turns the bound off, and the frontiers then key
///        every label at 0 rather than reading that component.
TEST(MeetingHalfWay, AClockIndexOutOfRangeTurnsTheBoundOff) {
    namespace bv = bidirectional_validation_test;
    namespace mh = meeting_half_way_test;
    auto graph = bv::join_only_graph();
    auto params = mh::meeting_params();
    params.critical_resource_index = 7;
    auto algorithm =
        graph->create_algorithm<SimpleDominanceAlgorithm>(test_util::bidirectional(params));
    SolveResult result;
    EXPECT_NO_THROW({ result = graph->solve(algorithm.get()); });
    EXPECT_FALSE(result.bidirectional->bounded_by_half_way);
    EXPECT_EQ(result.bidirectional->half_way_forward_bound, 0.0);
    EXPECT_EQ(result.bidirectional->half_way_backward_bound, 0.0);
    ASSERT_FALSE(result.solutions.empty());
    auto reference = bv::join_only_graph();
    EXPECT_NEAR(result.solutions.front().cost,
                mh::forward_optimum(reference.get()),
                bv::kTolerance);
}

/// @brief A warm start centres the next solve's bounds on where the last one met, and the answer
///        is the same.
TEST(MeetingHalfWay, AWarmStartReusesTheLastMeetingPoint) {
    namespace bv = bidirectional_validation_test;
    namespace mh = meeting_half_way_test;
    auto graph = bv::join_only_graph();
    auto params = mh::meeting_params();
    params.half_way_meeting_window = 4.0;  // [c - 4, c + 4]
    auto algorithm =
        graph->create_algorithm<SimpleDominanceAlgorithm>(test_util::bidirectional(params));
    ASSERT_FALSE(as_bidirectional(algorithm.get())->meeting_point().has_value());

    const SolveResult first = graph->solve(algorithm.get());
    ASSERT_EQ(first.status, AlgorithmStatus::COMPLETE);
    EXPECT_EQ(first.bidirectional->half_way_point_used, 20.0)
        << "the first solve starts from the full range";
    ASSERT_TRUE(as_bidirectional(algorithm.get())->meeting_point().has_value());
    EXPECT_EQ(*as_bidirectional(algorithm.get())->meeting_point(), 20.0);

    // Bounds [16, 24]. H_F never rises above 24, so after the forward label at 20 is processed
    // (H_B = 20) the forward front, 30, is past H_F: forward retires and H_B snaps to 24, and the
    // backward front, 20, is then at or below it. The join at 24 pairs the same halves at node 3.
    const SolveResult second = graph->solve(algorithm.get());
    ASSERT_EQ(second.status, AlgorithmStatus::COMPLETE);
    EXPECT_EQ(second.bidirectional->half_way_forward_bound, 24.0);
    EXPECT_EQ(second.bidirectional->half_way_backward_bound, 24.0);
    ASSERT_EQ(second.solutions.size(), 1U);
    EXPECT_NEAR(second.solutions.front().cost, 4.0, bv::kTolerance);
    EXPECT_EQ(second.bidirectional->number_of_joined_paths, 1U);
    EXPECT_EQ(*as_bidirectional(algorithm.get())->meeting_point(), 24.0);
    EXPECT_TRUE(algorithm->get_label_pool().check_ref_count_consistency());
}

/// @brief A window far from where the searches would meet makes one search do most of the work,
///        and the solve is still exact.
///
/// Window [3, 7] on `join_only_graph()`: the forward search retires once its front, 10, is past
/// `H_F = 7`, which snaps `H_B` to 7; the backward search then runs down to the source. The join at
/// 7 pairs the forward label at node 1 (clock 10, predecessor 0) with the backward label there.
TEST(MeetingHalfWay, AWarmStartFarFromTheMeetingPointStaysExact) {
    namespace bv = bidirectional_validation_test;
    namespace mh = meeting_half_way_test;
    auto graph = bv::join_only_graph();
    auto params = mh::meeting_params();
    params.half_way_meeting_window = 2.0;
    auto algorithm =
        graph->create_algorithm<SimpleDominanceAlgorithm>(test_util::bidirectional(params));
    as_bidirectional(algorithm.get())->set_meeting_point(5.0);

    const SolveResult result = graph->solve(algorithm.get());
    ASSERT_EQ(result.status, AlgorithmStatus::COMPLETE);
    ASSERT_TRUE(result.bidirectional->bounded_by_half_way)
        << result.bidirectional->half_way_off_reason;
    EXPECT_EQ(result.bidirectional->half_way_forward_bound, 7.0);
    EXPECT_EQ(result.bidirectional->half_way_backward_bound, 7.0);
    ASSERT_EQ(result.solutions.size(), 1U);
    EXPECT_NEAR(result.solutions.front().cost, 4.0, bv::kTolerance);
    EXPECT_EQ(result.solutions.front().path_arc_ids, (std::vector<size_t>{0, 1, 2, 3}));
    ASSERT_TRUE(as_bidirectional(algorithm.get())->meeting_point().has_value());
    EXPECT_EQ(*as_bidirectional(algorithm.get())->meeting_point(), 7.0);
    EXPECT_TRUE(algorithm->get_label_pool().check_ref_count_consistency());

    // Resetting the point starts the next solve from the full range again.
    as_bidirectional(algorithm.get())->set_meeting_point(std::nullopt);
    EXPECT_EQ(graph->solve(algorithm.get()).bidirectional->half_way_point_used, 20.0);
}

/// @brief Without a window the point is still remembered, but the bounds start at the full range.
TEST(MeetingHalfWay, NoWindowIgnoresTheMeetingPoint) {
    namespace bv = bidirectional_validation_test;
    namespace mh = meeting_half_way_test;
    auto graph = bv::join_only_graph();
    auto algorithm = graph->create_algorithm<SimpleDominanceAlgorithm>(
        test_util::bidirectional(mh::meeting_params()));
    as_bidirectional(algorithm.get())->set_meeting_point(5.0);
    const SolveResult result = graph->solve(algorithm.get());
    EXPECT_EQ(result.bidirectional->half_way_point_used, 20.0);
}
