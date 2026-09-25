// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// `dynamic_half_way`: a persistent bidirectional algorithm object moving `H` between solves.
//
// The controller's arithmetic is pinned in test_half_way_controller.hpp with hand-written
// observations. This file is about the wiring: that the algorithm seeds the controller from the
// params, feeds it every solve's result, starts the next solve from the value it holds, and leaves
// it alone when the solve cannot be trusted. That moving `H` never changes an answer is asserted
// across the whole random sweep in test_equivalence.hpp.
//
// The workhorse model is a long clock line with `H0` set deliberately low. On a line each node
// holds one label per direction, so the forward search keeps the few nodes below `H0` and the
// backward search keeps everything above it -- a backward-heavy split by construction, which the
// controller has to answer by raising `H`. Every test here runs with the DEFAULT
// `release_after_solve = true`, unlike the rest of the bidirectional suite: the controller reads
// the returned SolveResult, not the containers, and these tests are what say so.

#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "test_bidirectional.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace dynamic_half_way_test {

/// @brief Arcs on the line; the clock rises by 1 per arc and the path costs 1 per arc.
constexpr size_t kArcs = 10;

/// @brief The clock's real range on that line.
constexpr double kCapacity = 10.0;

/// @brief A deliberately poor `H0`: the forward search stops after two arcs, the backward search
///        covers the other eight. `R = 2 * H0 = 3`, so the controller may raise `H` to at most
///        `0.95 * 3 = 2.85`.
constexpr double kLowH = 1.5;

/// @brief How many solves each persistent object is put through.
constexpr int kSolves = 6;

/// @brief The model: one cost slot and one threshold clock slot, the pairing the bound needs.
inline std::unique_ptr<ResourceGraph<RealResource>> long_clock_line() {
    std::vector<std::pair<double, double>> arcs(kArcs, {1.0, 1.0});
    return bidirectional_test::clock_line_graph(arcs, kCapacity);
}

/// @brief Params with the clock bound, `H0`, and the flag -- and the default release.
inline AlgorithmParams<LabelList<bidirectional_test::Composed>> dynamic_params(
    double half_way_point, bool dynamic = true) {
    AlgorithmParams<LabelList<bidirectional_test::Composed>> params;
    params.half_way_point = half_way_point;
    params.critical_resource_index = bidirectional_test::kClockIndex;
    params.dynamic_half_way = dynamic;
    return params;
}

/// @brief Solves @p solves times with one object, returning each solve's result in order.
template <typename Algorithm>
std::vector<SolveResult> solve_repeatedly(ResourceGraph<RealResource>* graph, Algorithm* algorithm,
                                          int solves = kSolves) {
    std::vector<SolveResult> results;
    results.reserve(static_cast<size_t>(solves));
    for (int i = 0; i < solves; ++i) {
        results.push_back(graph->solve(algorithm));
    }
    return results;
}

/// @brief The half-way point each result reports having used.
inline std::vector<double> hs_used(const std::vector<SolveResult>& results) {
    std::vector<double> hs;
    hs.reserve(results.size());
    for (const auto& result : results) {
        hs.push_back(result.half_way_point_used);
    }
    return hs;
}

}  // namespace dynamic_half_way_test

// ============================================================================
// The loop
// ============================================================================

/// @brief A backward-heavy split raises `H` from one solve to the next, and never the answer.
///
/// The central test. Solve 0 must use `H0` exactly -- the controller is seeded, not yet moved --
/// and every later solve must use a larger `H` until the clamp stops it. Each solve still returns
/// the optimum, because the half-way bound is correct for any `H`.
TEST(DynamicHalfWay, ABackwardHeavySplitRaisesHBetweenSolves) {
    namespace dh = dynamic_half_way_test;
    namespace bt = bidirectional_test;

    auto graph = dh::long_clock_line();
    const double optimum = bt::forward_optimum(graph.get());
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        dh::dynamic_params(dh::kLowH));

    const auto results = dh::solve_repeatedly(graph.get(), algorithm.get());
    const auto hs = dh::hs_used(results);

    ASSERT_TRUE(results.front().bounded_by_half_way) << "the rest is about a bound in force";
    ASSERT_GT(results.front().backward_labels, results.front().forward_labels)
        << "the model is supposed to be backward-heavy at H0";

    EXPECT_DOUBLE_EQ(hs.front(), dh::kLowH) << "the first solve starts from half_way_point";
    EXPECT_GT(hs[1], hs[0]) << "a backward-heavy split must raise H";
    for (size_t i = 1; i < hs.size(); ++i) {
        EXPECT_GE(hs[i], hs[i - 1]) << "H only rises on a consistently backward-heavy line";
        EXPECT_LE(hs[i], 0.95 * 2.0 * dh::kLowH + 1e-12) << "the clamp holds";
    }
    for (const auto& result : results) {
        ASSERT_FALSE(result.solutions.empty());
        EXPECT_NEAR(result.solutions.front().cost, optimum, bt::kTolerance)
            << "moving H must never change the answer";
    }
    EXPECT_GT(algorithm->half_way_controller().moves(), 0U);
    EXPECT_EQ(algorithm->half_way_controller().observations(), static_cast<size_t>(dh::kSolves));
}

/// @brief Each solve starts from exactly the `H` the controller held when it began.
///
/// So what a caller reads from `half_way_controller().h()` between two solves is a promise about
/// the next one, not an approximation of it.
TEST(DynamicHalfWay, TheNextSolveUsesTheControllersH) {
    namespace dh = dynamic_half_way_test;

    auto graph = dh::long_clock_line();
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        dh::dynamic_params(dh::kLowH));

    for (int i = 0; i < dh::kSolves; ++i) {
        const bool seeded = algorithm->half_way_controller().seeded();
        const double promised = seeded ? algorithm->half_way_controller().h() : dh::kLowH;
        const auto result = graph->solve(algorithm.get());
        EXPECT_DOUBLE_EQ(result.half_way_point_used, promised) << "solve " << i;
    }
}

/// @brief Without the flag, the same object keeps the same `H`, and the controller stays unseeded.
TEST(DynamicHalfWay, WithoutTheFlagHStaysPut) {
    namespace dh = dynamic_half_way_test;

    auto graph = dh::long_clock_line();
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        dh::dynamic_params(dh::kLowH, /*dynamic=*/false));

    for (const double h : dh::hs_used(dh::solve_repeatedly(graph.get(), algorithm.get()))) {
        EXPECT_DOUBLE_EQ(h, dh::kLowH);
    }
    EXPECT_FALSE(algorithm->half_way_controller().seeded())
        << "a caller who never sets the flag never gets a controller";
}

// ============================================================================
// What the loop refuses to learn from
// ============================================================================

/// @brief A per-node extension quota truncates the search, so its counts teach nothing.
///
/// The status cannot say so -- a bidirectional solve reports COMPLETE either way -- which is why
/// the algorithm reads `num_labels_to_extend_by_node` itself before feeding the controller.
TEST(DynamicHalfWay, ATruncatedSearchTeachesNothing) {
    namespace dh = dynamic_half_way_test;

    auto graph = dh::long_clock_line();
    auto params = dh::dynamic_params(dh::kLowH);
    params.num_labels_to_extend_by_node = 1000;  // never binds here, but the promise is gone
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);

    const auto results = dh::solve_repeatedly(graph.get(), algorithm.get());
    ASSERT_EQ(results.front().status, AlgorithmStatus::COMPLETE)
        << "the premise: the status looks exact";
    for (const double h : dh::hs_used(results)) {
        EXPECT_DOUBLE_EQ(h, dh::kLowH);
    }
    EXPECT_EQ(algorithm->half_way_controller().last_move(), HalfWayMove::Skipped);
    EXPECT_EQ(algorithm->half_way_controller().moves(), 0U);
}

/// @brief When the bound switches itself off, there is no `H` to learn about.
///
/// A cost-only line: the one slot accumulates backwards, so it is not a clock and the bound
/// disables itself every solve. The controller is seeded -- `half_way_point` was positive -- but
/// skips every observation.
TEST(DynamicHalfWay, ASolveWithTheBoundOffTeachesNothing) {
    namespace bt = bidirectional_test;

    auto graph = bt::line_graph({1.0, 2.0, 3.0, 4.0});
    AlgorithmParams<LabelList<bt::Composed>> params;
    params.half_way_point = 5.0;
    params.dynamic_half_way = true;
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);

    for (int i = 0; i < 3; ++i) {
        const auto result = graph->solve(algorithm.get());
        EXPECT_FALSE(result.bounded_by_half_way);
        ASSERT_FALSE(result.solutions.empty());
        EXPECT_NEAR(result.solutions.front().cost, 10.0, bt::kTolerance);
    }
    ASSERT_TRUE(algorithm->half_way_controller().seeded());
    EXPECT_DOUBLE_EQ(algorithm->half_way_controller().h(), 5.0);
    EXPECT_EQ(algorithm->half_way_controller().last_move(), HalfWayMove::Skipped);
}

/// @brief With `half_way_point = 0` the bound is off, so the flag has nothing to seed from.
///
/// The solve must still be correct -- two unbounded searches and a join -- and must not throw from
/// the controller's refusal to seed at zero.
TEST(DynamicHalfWay, NoHalfWayPointLeavesTheFlagInert) {
    namespace dh = dynamic_half_way_test;
    namespace bt = bidirectional_test;

    auto graph = dh::long_clock_line();
    const double optimum = bt::forward_optimum(graph.get());
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        dh::dynamic_params(/*half_way_point=*/0.0));

    for (const auto& result : dh::solve_repeatedly(graph.get(), algorithm.get(), 2)) {
        EXPECT_FALSE(result.bounded_by_half_way);
        EXPECT_DOUBLE_EQ(result.half_way_point_used, 0.0);
        ASSERT_FALSE(result.solutions.empty());
        EXPECT_NEAR(result.solutions.front().cost, optimum, bt::kTolerance);
    }
    EXPECT_FALSE(algorithm->half_way_controller().seeded());
}

// ============================================================================
// Freezing and resetting through the algorithm
// ============================================================================

/// @brief Freezing through the algorithm holds `H` for every later solve.
///
/// RouteOpt's pattern: adapt during the root node's column generation, then freeze for the tree.
TEST(DynamicHalfWay, FreezingHoldsHForLaterSolves) {
    namespace dh = dynamic_half_way_test;

    auto graph = dh::long_clock_line();
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        dh::dynamic_params(dh::kLowH));

    graph->solve(algorithm.get());  // seeds, then moves H once
    const double learned = algorithm->half_way_controller().h();
    ASSERT_GT(learned, dh::kLowH);

    algorithm->half_way_controller().set_frozen(true);
    for (const double h : dh::hs_used(dh::solve_repeatedly(graph.get(), algorithm.get(), 3))) {
        EXPECT_DOUBLE_EQ(h, learned);
    }
    EXPECT_EQ(algorithm->half_way_controller().last_move(), HalfWayMove::Frozen);
}

/// @brief Resetting through the algorithm sends the next solve back to `H0`.
TEST(DynamicHalfWay, ResetSendsTheNextSolveBackToTheInitialH) {
    namespace dh = dynamic_half_way_test;

    auto graph = dh::long_clock_line();
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        dh::dynamic_params(dh::kLowH));

    dh::solve_repeatedly(graph.get(), algorithm.get(), 3);
    ASSERT_GT(algorithm->half_way_controller().h(), dh::kLowH);

    algorithm->half_way_controller().reset();
    EXPECT_DOUBLE_EQ(graph->solve(algorithm.get()).half_way_point_used, dh::kLowH);
}

/// @brief A controller assigned before the first solve is kept, knobs and all.
///
/// The documented way to tune it from C++: the first solve seeds a default controller only when
/// none is seeded yet.
TEST(DynamicHalfWay, AnAssignedControllerIsUsedWithItsOwnKnobs) {
    namespace dh = dynamic_half_way_test;

    auto graph = dh::long_clock_line();
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        dh::dynamic_params(dh::kLowH));

    HalfWayControllerParams knobs;
    knobs.initial_step = 0.1;
    knobs.min_step = 0.01;
    algorithm->half_way_controller() = HalfWayController(dh::kLowH, knobs);

    graph->solve(algorithm.get());
    EXPECT_DOUBLE_EQ(algorithm->half_way_controller().h(), dh::kLowH * 1.1)
        << "one backward-heavy solve, one 10 % step up -- not the default 20 %";
}

// ============================================================================
// The observation the algorithm builds
// ============================================================================

/// @brief `half_way_observation` copies the counts and derives `exact` from three things.
TEST(DynamicHalfWay, TheObservationReadsTheResult) {
    SolveResult result;
    result.forward_labels = 7;
    result.backward_labels = 11;
    result.number_of_joined_paths = 3;
    result.solutions.resize(2);
    result.bounded_by_half_way = true;
    result.status = AlgorithmStatus::COMPLETE;

    auto observation = half_way_observation(result);
    EXPECT_EQ(observation.forward_labels, 7U);
    EXPECT_EQ(observation.backward_labels, 11U);
    EXPECT_EQ(observation.joined_paths, 3U);
    EXPECT_EQ(observation.solutions, 2U);
    EXPECT_TRUE(observation.bounded);
    EXPECT_TRUE(observation.exact);

    EXPECT_FALSE(half_way_observation(result, /*truncated=*/true).exact);

    result.memory_pressure_triggered = true;
    EXPECT_FALSE(half_way_observation(result).exact) << "a trimmed pass is not exact";

    result.memory_pressure_triggered = false;
    result.status = AlgorithmStatus::TIMEOUT;
    EXPECT_FALSE(half_way_observation(result).exact) << "a timed-out pass is not exact";
}

/// @brief A solve whose join was truncated is not something the controller learns from.
///
/// The label counts are unaffected by a pair budget, but `joined_paths` is, and the zero-join guard
/// reads it: a capped join that joined nothing would pull `H` toward the centre for no reason.
TEST(DynamicHalfWay, ATruncatedJoinIsNotAnExactObservation) {
    SolveResult result;
    result.status = AlgorithmStatus::COMPLETE;
    result.bounded_by_half_way = true;
    result.forward_labels = 10;
    result.backward_labels = 1;
    ASSERT_TRUE(half_way_observation(result).exact) << "the control must be exact";

    result.join_truncated = true;
    EXPECT_FALSE(half_way_observation(result).exact);
}
