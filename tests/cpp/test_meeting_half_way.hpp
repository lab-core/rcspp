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
#include "test_bidirectional.hpp"
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

}  // namespace meeting_half_way_test

/// @brief Appendix A.1: on a line, the searches meet at 20 and the join builds the path.
TEST(MeetingHalfWay, OnALineTheSearchesMeetAndTheJoinBuildsThePath) {
    namespace bv = bidirectional_validation_test;
    namespace mh = meeting_half_way_test;
    auto graph = bv::join_only_graph();
    auto algorithm =
        graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(mh::meeting_params());
    const SolveResult result = graph->solve(algorithm.get());

    ASSERT_EQ(result.status, AlgorithmStatus::COMPLETE);
    ASSERT_TRUE(result.bounded_by_half_way) << algorithm->half_way_off_reason();
    EXPECT_EQ(result.half_way_forward_bound, 20.0);
    EXPECT_EQ(result.half_way_backward_bound, 20.0);
    EXPECT_EQ(result.half_way_point_used, 20.0);
    ASSERT_EQ(result.solutions.size(), 1U);
    EXPECT_NEAR(result.solutions.front().cost, 4.0, bv::kTolerance);
    EXPECT_EQ(result.solutions.front().path_arc_ids, (std::vector<size_t>{0, 1, 2, 3}));
    EXPECT_EQ(result.number_of_joined_paths, 1U);
    EXPECT_FALSE(algorithm->one_search_was_complete()) << "both searches stopped a label here";
    EXPECT_TRUE(algorithm->get_label_pool().check_ref_count_consistency());
}

/// @brief Appendix A.2: with slack the searches pass each other; the bounds still end equal.
TEST(MeetingHalfWay, WhenTheSearchesNeverMeetTheBoundsSnapTogether) {
    namespace bv = bidirectional_validation_test;
    namespace mh = meeting_half_way_test;
    // s(window [0, 0]) -> t(window [0, 10]), travel time 2, cost 1.
    auto graph = bv::clocked_graph({{0, {0.0, 0.0}}, {1, {0.0, 10.0}}}, {{1.0, 2.0, 0, 1}});
    auto algorithm =
        graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(mh::meeting_params());
    const SolveResult result = graph->solve(algorithm.get());

    ASSERT_EQ(result.status, AlgorithmStatus::COMPLETE);
    ASSERT_TRUE(result.bounded_by_half_way) << algorithm->half_way_off_reason();
    EXPECT_EQ(result.half_way_forward_bound, 0.0);
    EXPECT_EQ(result.half_way_backward_bound, 0.0);
    ASSERT_EQ(result.solutions.size(), 1U);
    EXPECT_NEAR(result.solutions.front().cost, 1.0, bv::kTolerance);
    EXPECT_EQ(result.number_of_joined_paths, 0U) << "a search reached a terminal on its own";
    EXPECT_FALSE(algorithm->one_search_was_complete())
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
    auto algorithm =
        graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(mh::meeting_params());
    const SolveResult result = graph->solve(algorithm.get());

    ASSERT_EQ(result.status, AlgorithmStatus::COMPLETE);
    ASSERT_TRUE(result.bounded_by_half_way) << algorithm->half_way_off_reason();
    EXPECT_TRUE(algorithm->one_search_was_complete());
    EXPECT_EQ(result.half_way_forward_bound, 99.0);
    EXPECT_EQ(result.half_way_backward_bound, 99.0);
    EXPECT_EQ(result.join_pairs_tested, 0U) << "the join must be skipped";
    EXPECT_EQ(result.number_of_joined_paths, 0U);
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
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    const SolveResult result = graph->solve(algorithm.get());
    EXPECT_TRUE(result.bounded_by_half_way) << algorithm->half_way_off_reason();

    // The forward search's optimum is 2 (route 0-1-3-4-6), as the fixed mode finds.
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions.front().cost, 2.0, bv::kTolerance);
}

/// @brief Both direction rules find the same optimum as the forward search.
TEST(MeetingHalfWay, BothDirectionRulesMatchTheForwardSearch) {
    namespace bv = bidirectional_validation_test;
    namespace bt = bidirectional_test;
    namespace mh = meeting_half_way_test;
    for (const auto rule : {HalfWayDirectionRule::FewestWaiting, HalfWayDirectionRule::LeastWork}) {
        auto graph = bv::two_join_graph();
        const double expected = bt::forward_optimum(graph.get());
        auto params = mh::meeting_params();
        params.half_way_direction_rule = rule;
        auto algorithm =
            graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
        const SolveResult result = graph->solve(algorithm.get());
        ASSERT_FALSE(result.solutions.empty());
        EXPECT_NEAR(result.solutions.front().cost, expected, bv::kTolerance);
        EXPECT_EQ(result.half_way_forward_bound, result.half_way_backward_bound);
    }
}

/// @brief A cost-only model has no clock, so the meeting bound turns itself off, as the fixed one
///        does, and the solve stays correct.
TEST(MeetingHalfWay, ACostOnlyModelTurnsTheBoundOff) {
    namespace bt = bidirectional_test;
    auto graph = bt::line_graph({1.0, 2.0, 3.0});
    auto params = bt::params(/*half_way_point=*/0.0);
    params.half_way_mode = HalfWayMode::Meeting;
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    const SolveResult result = graph->solve(algorithm.get());
    EXPECT_FALSE(result.bounded_by_half_way);
    EXPECT_FALSE(algorithm->half_way_off_reason().empty());
    EXPECT_EQ(result.half_way_point_used, 0.0);
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions.front().cost, bt::forward_optimum(graph.get()), bt::kTolerance);
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
            graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
        const SolveResult result = graph->solve(algorithm.get());
        EXPECT_TRUE(algorithm->get_label_pool().check_ref_count_consistency());
        return result;
    };

    const SolveResult joined = run(/*join_after_early_stop=*/true);
    ASSERT_EQ(joined.status, AlgorithmStatus::MEMORY_LIMIT)
        << "the limit did not stop the search, so this test proves nothing";
    EXPECT_EQ(joined.half_way_backward_bound, 10.0);
    EXPECT_EQ(joined.half_way_forward_bound, 30.0);
    EXPECT_EQ(joined.half_way_point_used, 10.0);
    ASSERT_EQ(joined.number_of_joined_paths, 1U);
    ASSERT_FALSE(joined.solutions.empty());
    EXPECT_NEAR(joined.solutions.front().cost, 4.0, bv::kTolerance);
    EXPECT_EQ(joined.solutions.front().path_arc_ids, (std::vector<size_t>{0, 1, 2, 3}));

    const SolveResult skipped = run(/*join_after_early_stop=*/false);
    ASSERT_EQ(skipped.status, AlgorithmStatus::MEMORY_LIMIT);
    EXPECT_EQ(skipped.number_of_joined_paths, 0U);
}

/// @brief An interrupt skips the join, as in the fixed mode.
TEST(MeetingHalfWay, AnInterruptSkipsTheJoin) {
    namespace bv = bidirectional_validation_test;
    namespace mh = meeting_half_way_test;
    auto graph = bv::join_only_graph();
    size_t calls = 0;
    auto params = mh::meeting_params();
    params.should_stop = [&calls]() { return ++calls > 2; };
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    const SolveResult result = graph->solve(algorithm.get());
    ASSERT_EQ(result.status, AlgorithmStatus::INTERRUPTED);
    EXPECT_EQ(result.number_of_joined_paths, 0U);
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
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    const SolveResult result = graph->solve(algorithm.get());
    ASSERT_TRUE(result.bounded_by_half_way);
    EXPECT_EQ(result.half_way_forward_bound, 20.0) << "the fixed H, not a meeting point";
    EXPECT_EQ(result.half_way_backward_bound, 20.0);

    params.half_way_point = 0.0;
    auto without_h = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    EXPECT_FALSE(graph->solve(without_h.get()).bounded_by_half_way)
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
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    for (int solve = 0; solve < 3; ++solve) {
        static_cast<void>(graph->solve(algorithm.get()));
    }
    EXPECT_FALSE(algorithm->half_way_controller().seeded());
}

/// @brief Memory pressure sheds a clock-ordered frontier without losing track of labels.
TEST(MeetingHalfWay, MemoryPressureKeepsTheCountsConsistent) {
    namespace bt = bidirectional_test;
    auto graph = bt::clock_line_graph({{1.0, 1.0}, {2.0, 1.0}, {3.0, 1.0}}, /*capacity=*/20.0);
    auto under_pressure = [](size_t max_labels_per_node) {
        auto p = bt::params(0.0, bt::kClockIndex);
        p.half_way_mode = HalfWayMode::Meeting;
        constexpr double kHugeLimitGiB = 1e9;
        p.max_memory_gb = kHugeLimitGiB;
        p.memory_pressure_fraction = 0.0;
        p.memory_check_interval = 1;
        p.memory_pressure_max_labels_per_node = max_labels_per_node;
        return p;
    };

    auto roomy =
        graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(under_pressure(1000));
    SolveResult roomy_result;
    EXPECT_NO_THROW({ roomy_result = graph->solve(roomy.get()); });
    EXPECT_FALSE(roomy_result.solutions.empty());
    EXPECT_TRUE(roomy->get_label_pool().check_ref_count_consistency());

    auto squeezed =
        graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(under_pressure(0));
    EXPECT_NO_THROW({ graph->solve(squeezed.get()); });
    EXPECT_TRUE(squeezed->get_label_pool().check_ref_count_consistency());
}

/// @brief A non-monotone clock disables the bound, does not throw, and still finds the optimum.
TEST(MeetingHalfWay, NonMonotoneClockDisablesTheBoundAndStillSolves) {
    namespace bt = bidirectional_test;

    // The first arc lifts the clock to 5 so the path stays feasible within [0, 20].
    auto monotone = bt::clock_line_graph({{1.0, 5.0}, {2.0, 2.0}, {3.0, 3.0}}, /*capacity=*/20.0);

    // The middle arc's -2 makes the clock non-monotone. A time window waits, so its clock may go
    // down on an arc; a capacity may not, since a backward label cannot see it dip below 0.
    const std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 20.0}},
                                                              {1, {0.0, 20.0}},
                                                              {2, {0.0, 20.0}},
                                                              {3, {0.0, 20.0}}};
    auto non_monotone = std::make_unique<ResourceGraph<RealResource>>();
    non_monotone->add_resource<RealResource>(
        std::make_unique<AdditionExtensionFunction<RealResource>>(),
        std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<ValueCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    non_monotone->add_resource<RealResource>(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    for (size_t node_id = 0; node_id < 4; ++node_id) {
        non_monotone->add_node(node_id, node_id == 0, node_id == 3);
    }
    non_monotone->add_arc<RealResource, RealResource>({1.0, 5.0}, 0, 1, 1.0);
    non_monotone->add_arc<RealResource, RealResource>({2.0, -2.0}, 1, 2, 2.0);
    non_monotone->add_arc<RealResource, RealResource>({3.0, 3.0}, 2, 3, 3.0);

    auto meeting = [] {
        auto params = bt::params(3.0, bt::kClockIndex);
        params.half_way_mode = HalfWayMode::Meeting;
        return params;
    };

    auto monotone_algorithm =
        monotone->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(meeting());
    const auto monotone_result = monotone->solve(monotone_algorithm.get());
    ASSERT_FALSE(monotone_result.solutions.empty());
    EXPECT_TRUE(monotone_algorithm->bounded_by_half_way());

    // Preprocessing off: it extends from default values, so the -2 would put node 2 below the
    // clock's floor of 0 and remove its arcs.
    auto algorithm =
        non_monotone->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(meeting());
    SolveResult result;
    EXPECT_NO_THROW({
        result = non_monotone->solve(algorithm.get(),
                                     std::numeric_limits<double>::infinity(),
                                     /*preprocess=*/false);
    });

    EXPECT_FALSE(algorithm->bounded_by_half_way())
        << "a non-monotone clock must disable the bound rather than be trusted";
    ASSERT_FALSE(result.solutions.empty()) << "disabling the bound must not lose the optimum";
    EXPECT_NEAR(result.solutions.front().cost, 6.0, bt::kTolerance);  // 1 + 2 + 3
}

/// @brief A clock that does not take part in dominance disables the bound, and the answer survives.
///
/// The bound needs the clock in the dominance order; otherwise a dominator above H could evict a
/// label below it and silently lose a path.
TEST(MeetingHalfWay, ATrivialDominanceOnTheClockDisablesTheBound) {
    namespace bt = bidirectional_test;

    // Same shape as clock_line_graph, but the clock slot carries a TrivialDominanceFunction.
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<CapacityExtensionFunction<RealResource>>(20.0),
        std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 20.0),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<TrivialDominanceFunction<RealResource>>());
    for (size_t node_id = 0; node_id < 4; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id == 3);
    }
    graph->add_arc<RealResource, RealResource>({1.0, 5.0}, 0, 1, 1.0);
    graph->add_arc<RealResource, RealResource>({2.0, 2.0}, 1, 2, 2.0);
    graph->add_arc<RealResource, RealResource>({3.0, 3.0}, 2, 3, 3.0);

    auto params = bt::params(3.0, bt::kClockIndex);
    params.half_way_mode = HalfWayMode::Meeting;
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    SolveResult result;
    EXPECT_NO_THROW({ result = graph->solve(algorithm.get()); });

    EXPECT_FALSE(algorithm->bounded_by_half_way())
        << "a clock outside the dominance order must not be trusted";
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions.front().cost, 6.0, bt::kTolerance);  // 1 + 2 + 3
}

/// @brief A clock index that names no component turns the bound off, and the frontiers then key
///        every label at 0 rather than reading that component.
TEST(MeetingHalfWay, AClockIndexOutOfRangeTurnsTheBoundOff) {
    namespace bv = bidirectional_validation_test;
    namespace bt = bidirectional_test;
    namespace mh = meeting_half_way_test;
    auto graph = bv::join_only_graph();
    auto params = mh::meeting_params();
    params.critical_resource_index = 7;
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    SolveResult result;
    EXPECT_NO_THROW({ result = graph->solve(algorithm.get()); });
    EXPECT_FALSE(result.bounded_by_half_way);
    EXPECT_EQ(result.half_way_forward_bound, 0.0);
    EXPECT_EQ(result.half_way_backward_bound, 0.0);
    ASSERT_FALSE(result.solutions.empty());
    auto reference = bv::join_only_graph();
    EXPECT_NEAR(result.solutions.front().cost,
                bt::forward_optimum(reference.get()),
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
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    ASSERT_FALSE(algorithm->meeting_point().has_value());

    const SolveResult first = graph->solve(algorithm.get());
    ASSERT_EQ(first.status, AlgorithmStatus::COMPLETE);
    EXPECT_EQ(first.half_way_point_used, 20.0) << "the first solve starts from the full range";
    ASSERT_TRUE(algorithm->meeting_point().has_value());
    EXPECT_EQ(*algorithm->meeting_point(), 20.0);

    // Bounds [16, 24]. H_F never rises above 24, so after the forward label at 20 is processed
    // (H_B = 20) the forward front, 30, is past H_F: forward retires and H_B snaps to 24, and the
    // backward front, 20, is then at or below it. The join at 24 pairs the same halves at node 3.
    const SolveResult second = graph->solve(algorithm.get());
    ASSERT_EQ(second.status, AlgorithmStatus::COMPLETE);
    EXPECT_EQ(second.half_way_forward_bound, 24.0);
    EXPECT_EQ(second.half_way_backward_bound, 24.0);
    ASSERT_EQ(second.solutions.size(), 1U);
    EXPECT_NEAR(second.solutions.front().cost, 4.0, bv::kTolerance);
    EXPECT_EQ(second.number_of_joined_paths, 1U);
    EXPECT_EQ(*algorithm->meeting_point(), 24.0);
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
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    algorithm->set_meeting_point(5.0);

    const SolveResult result = graph->solve(algorithm.get());
    ASSERT_EQ(result.status, AlgorithmStatus::COMPLETE);
    ASSERT_TRUE(result.bounded_by_half_way) << algorithm->half_way_off_reason();
    EXPECT_EQ(result.half_way_forward_bound, 7.0);
    EXPECT_EQ(result.half_way_backward_bound, 7.0);
    ASSERT_EQ(result.solutions.size(), 1U);
    EXPECT_NEAR(result.solutions.front().cost, 4.0, bv::kTolerance);
    EXPECT_EQ(result.solutions.front().path_arc_ids, (std::vector<size_t>{0, 1, 2, 3}));
    ASSERT_TRUE(algorithm->meeting_point().has_value());
    EXPECT_EQ(*algorithm->meeting_point(), 7.0);
    EXPECT_TRUE(algorithm->get_label_pool().check_ref_count_consistency());

    // Resetting the point starts the next solve from the full range again.
    algorithm->set_meeting_point(std::nullopt);
    EXPECT_EQ(graph->solve(algorithm.get()).half_way_point_used, 20.0);
}

/// @brief Without a window the point is still remembered, but the bounds start at the full range.
TEST(MeetingHalfWay, NoWindowIgnoresTheMeetingPoint) {
    namespace bv = bidirectional_validation_test;
    namespace mh = meeting_half_way_test;
    auto graph = bv::join_only_graph();
    auto algorithm =
        graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(mh::meeting_params());
    algorithm->set_meeting_point(5.0);
    const SolveResult result = graph->solve(algorithm.get());
    EXPECT_EQ(result.half_way_point_used, 20.0);
}
