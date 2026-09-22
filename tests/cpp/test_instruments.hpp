// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The four SolveResult diagnostics: the two direction label counts, the dominance-comparison
// total, and the half-way point actually used.
//
// StatisticsSurviveTheDefaultRelease is the test the group exists for. `annotate` reads the label
// containers and `release_label_memory()` clears them, and `release_after_solve` defaults to TRUE
// -- so with the two in the wrong order every ordinary solve reports zero and only a caller who
// happened to turn the release off sees the truth. No other test in the suite can see that,
// because `bidirectional_test::params` sets `release_after_solve = false` so the helpers can
// inspect the containers afterwards.
//
// The bounded model is the one `Bidirectional.SolveResultCarriesTheDiagnostics` already uses and is
// known to engage the bound. If an assertion about `bounded_by_half_way` ever fails here, copy that
// test's construction again rather than inventing a new graph.

#include <gtest/gtest.h>

#include <memory>

#include "rcspp/rcspp.hpp"
#include "test_bidirectional.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace instruments_test {

/// @brief The half-way point the bounded model below is solved with.
constexpr double kHalfWay = 3.0;

/// @brief A model whose clock passes validation, so the half-way bound really engages.
///
/// Component 0 is the cost, component 1 the threshold clock; `bidirectional_test::kClockIndex`
/// names the second. Copied from `Bidirectional.SolveResultCarriesTheDiagnostics`.
inline std::unique_ptr<ResourceGraph<RealResource>> bounded_graph() {
    return bidirectional_test::clock_line_graph({{1.0, 5.0}, {2.0, 2.0}, {3.0, 3.0}},
                                                /*capacity=*/20.0);
}

/// @brief A *diamond* with a cost slot and a threshold clock slot.
///
/// `clock_line_graph` cannot serve every assertion here, and the reason is worth stating: on a
/// line each node receives exactly one label per direction, and `update_non_dominated_labels`
/// consults the container **before** inserting -- so the first label at a node is compared against
/// an empty set. A line therefore performs **zero** dominance comparisons, correctly. Asserting
/// `dominance_checks > 0` there fails, and it should.
///
/// Two routes 0->{1,2}->3 give node 3 two forward labels and node 0 two backward ones, so both
/// directions actually compare something. Costs differ, clocks do not, so the cheaper branch
/// dominates and the comparison has an outcome as well as a count.
///
/// Slot 0 is the cost (`AdditionExtensionFunction`, `Accumulate`); slot 1 is the clock
/// (`BudgetExtensionFunction`, `Threshold`) -- the same pairing `clock_line_graph` uses, and the
/// only one the half-way rule can compare against `H`.
inline std::unique_ptr<ResourceGraph<RealResource>> clock_diamond_graph(double capacity = 20.0) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<BudgetExtensionFunction<RealResource>>(capacity),
        std::make_unique<MinMaxFeasibilityFunction<RealResource>>(
            /*min=*/0.0,
            capacity,
            /*merge_by_increasing_value=*/true),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2);
    graph->add_node(3, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource, RealResource>({1.0, 2.0}, 0, 1, 1.0);
    graph->add_arc<RealResource, RealResource>({2.0, 2.0}, 0, 2, 2.0);
    graph->add_arc<RealResource, RealResource>({1.0, 2.0}, 1, 3, 1.0);
    graph->add_arc<RealResource, RealResource>({1.0, 2.0}, 2, 3, 1.0);
    return graph;
}

/// @brief Params for that model, with the release under the caller's control.
inline AlgorithmParams<LabelList<bidirectional_test::Composed>> bounded_params(
    bool release_after_solve) {
    AlgorithmParams<LabelList<bidirectional_test::Composed>> params;
    params.release_after_solve = release_after_solve;
    params.half_way_point = kHalfWay;
    params.critical_resource_index = bidirectional_test::kClockIndex;
    return params;
}

}  // namespace instruments_test

/// @brief A forward solve reports forward labels and no backward ones.
TEST(Instruments, ForwardSolveReportsOnlyTheForwardSide) {
    namespace bt = bidirectional_test;

    auto graph = bt::diamond_graph(1.0, 1.0, 2.0, 2.0);
    const auto result = graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});

    EXPECT_GT(result.forward_labels, 0U) << "the forward containers held survivors";
    EXPECT_EQ(result.backward_labels, 0U) << "a forward algorithm has no backward containers";
    EXPECT_GT(result.dominance_checks, 0U);
    EXPECT_DOUBLE_EQ(result.half_way_point_used, 0.0) << "no half-way bound outside bidirectional";
}

/// @brief A bounded bidirectional solve reports both sides and the H it used.
TEST(Instruments, BidirectionalSolveReportsBothSidesAndItsH) {
    namespace it = instruments_test;

    auto graph = it::bounded_graph();
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        it::bounded_params(/*release_after_solve=*/false));
    const auto result = graph->solve(algorithm.get());

    ASSERT_TRUE(result.bounded_by_half_way) << "the rest of this test is about a bound in force";
    EXPECT_GT(result.forward_labels, 0U);
    EXPECT_GT(result.backward_labels, 0U) << "the backward search must have kept something";
    EXPECT_DOUBLE_EQ(result.half_way_point_used, it::kHalfWay);
}

/// @brief A line performs no dominance comparisons; a diamond performs some.
///
/// Both halves of this are the point. `update_non_dominated_labels` consults a node's container
/// *before* inserting into it, so the first label to reach a node is compared against nothing --
/// and on a line there is never a second. Zero is therefore the correct answer there, not a broken
/// counter, and asserting `> 0` on a line is the mistake this test exists to document.
TEST(Instruments, DominanceChecksNeedLabelsThatActuallyCompete) {
    namespace it = instruments_test;

    auto line = it::bounded_graph();
    auto line_algo = line->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        it::bounded_params(/*release_after_solve=*/false));
    const auto on_a_line = line->solve(line_algo.get());
    EXPECT_GT(on_a_line.forward_labels, 0U) << "labels were stored";
    EXPECT_EQ(on_a_line.dominance_checks, 0U)
        << "one label per node per direction, and the container is consulted before the insert";

    auto diamond = it::clock_diamond_graph();
    auto diamond_algo = diamond->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        it::bounded_params(/*release_after_solve=*/false));
    const auto on_a_diamond = diamond->solve(diamond_algo.get());
    ASSERT_TRUE(on_a_diamond.bounded_by_half_way);
    EXPECT_GT(on_a_diamond.forward_labels, 0U);
    EXPECT_GT(on_a_diamond.backward_labels, 0U);
    EXPECT_GT(on_a_diamond.dominance_checks, 0U)
        << "two routes meet at node 3 forwards and at node 0 backwards";
}

/// @brief With the bound off, `half_way_point_used` is 0 and says nothing was applied.
///
/// A cost-only model has no threshold clock, so the bound disables itself. The pairing of
/// `bounded_by_half_way == false` with `half_way_point_used == 0.0` is what tells a caller that the
/// 0 means "no bound" rather than "H was 0".
TEST(Instruments, HalfWayPointUsedIsZeroWhenTheBoundIsOff) {
    namespace bt = bidirectional_test;

    auto graph = bt::line_graph({1.0, 2.0, 3.0, 4.0});
    auto algorithm =
        graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(bt::params(5.0));
    const auto result = graph->solve(algorithm.get());

    EXPECT_FALSE(result.bounded_by_half_way);
    EXPECT_DOUBLE_EQ(result.half_way_point_used, 0.0);
}

/// @brief The statistics are the same whether or not label memory is released after the solve.
///
/// The regression test for the ordering in `Algorithm::solve`. `release_after_solve` defaults to
/// TRUE, so the released run is the ordinary one; if `annotate` ran after the release it would
/// report zeros here and the truth in the retained run, and the two would differ.
TEST(Instruments, StatisticsSurviveTheDefaultRelease) {
    namespace it = instruments_test;

    auto retained_graph = it::bounded_graph();
    auto retained_algo =
        retained_graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
            it::bounded_params(/*release_after_solve=*/false));
    const auto retained = retained_graph->solve(retained_algo.get());

    auto released_graph = it::bounded_graph();
    auto released_algo =
        released_graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
            it::bounded_params(/*release_after_solve=*/true));
    const auto released = released_graph->solve(released_algo.get());

    ASSERT_GT(retained.forward_labels, 0U) << "the retained run is the control";
    EXPECT_EQ(released.forward_labels, retained.forward_labels);
    EXPECT_EQ(released.backward_labels, retained.backward_labels);
    EXPECT_EQ(released.dominance_checks, retained.dominance_checks);
    EXPECT_DOUBLE_EQ(released.half_way_point_used, retained.half_way_point_used);
}
