// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Phase 8: the first real backward search, checked against an independent oracle.
//
// Phases 2-7 each added a piece of backward machinery that nothing exercised end to end. This is
// where a wrong sign, a wrong seed or an un-routed container stops being theoretical -- and every
// one of those failures is silent rather than loud:
//
//   backward extension adds where it should subtract  -> wrong answers, or none; reports success
//   backward seed left at zero                        -> backward search finds nothing
//   forward dominance in the backward container       -> optimal solutions pruned, plausible answer
//
// So the additive instances are checked three ways (forward == backward == reversed-graph oracle),
// and the time-window instance is checked against numbers derived by hand, since the oracle is not
// valid there.
//
// Note these tests read label *values* rather than solve() solutions. Backward path reconstruction
// does not exist yet: a backward label carries an out-arc and no in-arc, so get_path_arc_ids walks
// an empty chain and extract_solution drops it. That is phase 10's job, and nothing here depends
// on it.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/backward_only_algorithm.hpp"
#include "util/reverse_graph_oracle.hpp"
#include "util/test_arc.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace backward_search_test {

using Composed = ResourceTypeComposition<RealResource>;
using BackwardList = LabelList<Composed, BackwardDirection>;
using BackwardAlgorithm = test_util::BackwardOnlyAlgorithm<Composed, BackwardList>;

constexpr double kTolerance = 1e-9;

/// @brief The cheapest forward solution, via the ordinary algorithm.
inline double forward_optimum(ResourceGraph<RealResource>* graph) {
    const auto result = graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    if (result.solutions.empty()) {
        return std::numeric_limits<double>::infinity();
    }
    return result.solutions.front().cost;
}

/// @brief Params that keep the label containers alive after solve().
///
/// solve() calls release_label_memory() at the end when release_after_solve is set, which clears
/// the per-node containers. These tests read label *values* out of them afterwards, so they have
/// to survive -- reading them after a release is an out-of-range access on an emptied vector,
/// which is a very different failure from an empty result.
inline AlgorithmParams<BackwardList> inspectable_params() {
    AlgorithmParams<BackwardList> params;
    params.release_after_solve = false;
    return params;
}

/// @brief The cheapest backward path, read from the labels at the backward terminals.
inline double backward_optimum(ResourceGraph<RealResource>* graph) {
    auto algorithm = graph->create_algorithm<test_util::BackwardOnlyAlgorithm, BackwardList>(
        inspectable_params());
    graph->solve(algorithm.get());
    return algorithm->best_terminal_cost();
}

/// @brief A three-node line: 0 -> 1 -> 2, costs -2 and -3.
inline test_util::AdditiveInstance line_instance() {
    return {.num_nodes = 3,
            .sources = {0},
            .sinks = {2},
            .arcs = {{.origin = 0, .destination = 1, .cost = -2.0, .load = 0.0},
                     {.origin = 1, .destination = 2, .cost = -3.0, .load = 0.0}},
            .capacity = 0.0};
}

/// @brief Two routes to the sink: 0->1->3 costs -6, 0->2->3 costs -4.
inline test_util::AdditiveInstance two_path_instance() {
    return {.num_nodes = 4,
            .sources = {0},
            .sinks = {3},
            .arcs = {{.origin = 0, .destination = 1, .cost = -2.0, .load = 0.0},
                     {.origin = 1, .destination = 3, .cost = -4.0, .load = 0.0},
                     {.origin = 0, .destination = 2, .cost = -1.0, .load = 0.0},
                     {.origin = 2, .destination = 3, .cost = -3.0, .load = 0.0}},
            .capacity = 0.0};
}

/// @brief The cheap route is too heavy: capacity 5 rules out the -6 path, leaving -4.
inline test_util::AdditiveInstance capacity_instance() {
    return {.num_nodes = 4,
            .sources = {0},
            .sinks = {3},
            .arcs = {{.origin = 0, .destination = 1, .cost = -2.0, .load = 4.0},
                     {.origin = 1, .destination = 3, .cost = -4.0, .load = 4.0},
                     {.origin = 0, .destination = 2, .cost = -1.0, .load = 1.0},
                     {.origin = 2, .destination = 3, .cost = -3.0, .load = 1.0}},
            .capacity = 5.0};
}

// ── The time-window instance, from the design notes ─────────────────────────
//
//   arcs (travel time)            time windows [open, close]
//     s(0) -> a(1)   15             s: [ 0,   0]
//     s(0) -> b(2)   25             a: [10,  40]
//     a(1) -> b(2)   10             b: [20,  60]
//     a(1) -> t(3)   50             t: [ 0, 100]
//     b(2) -> t(3)   30
//
//   forward (earliest arrival)     backward (latest feasible departure)
//     f(s) = 0                       b(t) = 100              <- the seed
//     f(a) = max(10, 0+15)  = 15     b(b) = min(60, 100-30)  = 60
//     f(b) = max(20, 0+25)  = 25     b(a) = min(40, 100-50)  = 40
//     f(t) = 25 + 30        = 55     b(s) = min( 0,  40-15)  =  0

constexpr size_t kNodeS = 0;
constexpr size_t kNodeA = 1;
constexpr size_t kNodeB = 2;
constexpr size_t kNodeT = 3;

inline std::map<size_t, std::pair<double, double>> time_windows() {
    return {{kNodeS, {0.0, 0.0}},
            {kNodeA, {10.0, 40.0}},
            {kNodeB, {20.0, 60.0}},
            {kNodeT, {0.0, 100.0}}};
}

/// @brief Builds the time-window instance above as a single-component time resource.
inline std::unique_ptr<ResourceGraph<RealResource>> time_window_graph() {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    const auto windows = time_windows();

    graph->add_resource<RealResource>(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        std::make_unique<ValueCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());

    graph->add_node(kNodeS, /*source=*/true, /*sink=*/false);
    graph->add_node(kNodeA);
    graph->add_node(kNodeB);
    graph->add_node(kNodeT, /*source=*/false, /*sink=*/true);

    graph->add_arc<RealResource>(std::make_tuple(15.0), kNodeS, kNodeA);
    graph->add_arc<RealResource>(std::make_tuple(25.0), kNodeS, kNodeB);
    graph->add_arc<RealResource>(std::make_tuple(10.0), kNodeA, kNodeB);
    graph->add_arc<RealResource>(std::make_tuple(50.0), kNodeA, kNodeT);
    graph->add_arc<RealResource>(std::make_tuple(30.0), kNodeB, kNodeT);
    return graph;
}

/// @brief The scalar value of a composition label's first component.
///
/// A composition Resource's own get_value() returns the composition *tag*, which carries no value;
/// the numbers live in the components.
inline double component_value(const Label<Composed>& label) {
    const auto& components = std::get<0>(label.get_resource().get_components());
    return components.front()->get_value().get_value();
}

/// @brief The single backward label value held at @p node_id, or nullopt when there is none.
inline std::optional<double> backward_value_at(const BackwardAlgorithm& algorithm,
                                               const ResourceGraph<RealResource>& graph,
                                               size_t node_id) {
    const auto* node = graph.get_node(node_id);
    const auto& labels = algorithm.get_labels_by_node_pos().at(node->pos()).get_labels();
    if (labels.empty()) {
        return std::nullopt;
    }
    // Every instance here leaves exactly one non-dominated label per node.
    return component_value(*labels.front());
}

}  // namespace backward_search_test

// ============================================================================
// The oracle's own precondition
// ============================================================================

/// @brief The oracle only ever builds additive resources.
///
/// It is valid only where forward and backward extension coincide, because a reversed arc still
/// applies the forward extension function. This pins that precondition so an edit that slips a
/// threshold resource into the builder fails here rather than producing a confident wrong answer.
TEST(BackwardSearch, OracleBuildsOnlyAdditiveResources) {
    EXPECT_EQ(AdditionExtensionFunction<RealResource>{}.backward_kind(), BackwardKind::Accumulate);

    // And the two resources it would reverse are genuinely not thresholds.
    EXPECT_NE(TimeWindowExtensionFunction<RealResource>{backward_search_test::time_windows()}
                  .backward_kind(),
              BackwardKind::Accumulate);
}

// ============================================================================
// Additive instances: forward == backward == oracle
// ============================================================================

/// @brief Cost only, a three-node line.
TEST(BackwardSearch, LineAgreesThreeWays) {
    namespace bst = backward_search_test;
    const auto instance = bst::line_instance();

    auto forward_graph = test_util::build_additive_graph(instance, /*reversed=*/false);
    auto backward_graph = test_util::build_additive_graph(instance, /*reversed=*/false);
    auto oracle_graph = test_util::build_additive_graph(instance, /*reversed=*/true);

    const double forward = bst::forward_optimum(forward_graph.get());
    const double backward = bst::backward_optimum(backward_graph.get());
    const double oracle = bst::forward_optimum(oracle_graph.get());

    EXPECT_NEAR(forward, -5.0, bst::kTolerance);
    EXPECT_NEAR(backward, forward, bst::kTolerance);
    EXPECT_NEAR(oracle, forward, bst::kTolerance);
}

/// @brief Cost only, two competing routes.
TEST(BackwardSearch, TwoPathsAgreeThreeWays) {
    namespace bst = backward_search_test;
    const auto instance = bst::two_path_instance();

    auto forward_graph = test_util::build_additive_graph(instance, /*reversed=*/false);
    auto backward_graph = test_util::build_additive_graph(instance, /*reversed=*/false);
    auto oracle_graph = test_util::build_additive_graph(instance, /*reversed=*/true);

    const double forward = bst::forward_optimum(forward_graph.get());
    const double backward = bst::backward_optimum(backward_graph.get());
    const double oracle = bst::forward_optimum(oracle_graph.get());

    EXPECT_NEAR(forward, -6.0, bst::kTolerance);
    EXPECT_NEAR(backward, forward, bst::kTolerance);
    EXPECT_NEAR(oracle, forward, bst::kTolerance);
}

/// @brief Cost plus a bounded load: the capacity rules the cheap route out.
TEST(BackwardSearch, CapacityAgreesThreeWays) {
    namespace bst = backward_search_test;
    const auto instance = bst::capacity_instance();

    auto forward_graph = test_util::build_additive_graph(instance, /*reversed=*/false);
    auto backward_graph = test_util::build_additive_graph(instance, /*reversed=*/false);
    auto oracle_graph = test_util::build_additive_graph(instance, /*reversed=*/true);

    const double forward = bst::forward_optimum(forward_graph.get());
    const double backward = bst::backward_optimum(backward_graph.get());
    const double oracle = bst::forward_optimum(oracle_graph.get());

    // The -6 route carries load 8 > 5, so the -4 route wins.
    EXPECT_NEAR(forward, -4.0, bst::kTolerance);
    EXPECT_NEAR(backward, forward, bst::kTolerance);
    EXPECT_NEAR(oracle, forward, bst::kTolerance);
}

/// @brief A node the source cannot reach does not let the backward search invent a path.
TEST(BackwardSearch, UnreachableNodeInventsNothing) {
    namespace bst = backward_search_test;
    // Node 1 has an arc INTO the sink but nothing reaching it from the source.
    test_util::AdditiveInstance instance{
        .num_nodes = 3,
        .sources = {0},
        .sinks = {2},
        .arcs = {{.origin = 1, .destination = 2, .cost = -9.0, .load = 0.0}},
        .capacity = 0.0};

    auto forward_graph = test_util::build_additive_graph(instance, /*reversed=*/false);
    auto backward_graph = test_util::build_additive_graph(instance, /*reversed=*/false);

    EXPECT_TRUE(std::isinf(bst::forward_optimum(forward_graph.get())));
    // The backward search reaches node 1, but node 1 is not a source, so no terminal label exists.
    EXPECT_TRUE(std::isinf(bst::backward_optimum(backward_graph.get())));
}

// ============================================================================
// Time windows: hand-computed, no oracle
// ============================================================================

/// @brief Every backward label equals the hand-computed latest feasible departure.
///
/// This is the test that catches a wrong sign in TimeWindowExtensionFunction::extend_back and a
/// wrong seed in back_seed_value() -- both silently, and both fatally. It asserts on label values
/// rather than on a solve result precisely so the two are distinguishable.
TEST(BackwardSearch, TimeWindowBackwardLabelsMatchHandComputation) {
    namespace bst = backward_search_test;
    auto graph = bst::time_window_graph();

    auto algorithm = graph->create_algorithm<test_util::BackwardOnlyAlgorithm, bst::BackwardList>(
        bst::inspectable_params());
    graph->solve(algorithm.get(), std::numeric_limits<double>::infinity(), /*preprocess=*/false);

    // b(t) = 100: the seed, straight from the sink's closing time.
    const auto at_t = bst::backward_value_at(*algorithm, *graph, bst::kNodeT);
    ASSERT_TRUE(at_t.has_value());
    EXPECT_NEAR(*at_t, 100.0, bst::kTolerance);

    // b(b) = min(60, 100 - 30) = 60.
    const auto at_b = bst::backward_value_at(*algorithm, *graph, bst::kNodeB);
    ASSERT_TRUE(at_b.has_value());
    EXPECT_NEAR(*at_b, 60.0, bst::kTolerance);

    // b(a) = min(40, 100 - 50) = 40.
    const auto at_a = bst::backward_value_at(*algorithm, *graph, bst::kNodeA);
    ASSERT_TRUE(at_a.has_value());
    EXPECT_NEAR(*at_a, 40.0, bst::kTolerance);

    // b(s) = min(0, 40 - 15) = 0.
    const auto at_s = bst::backward_value_at(*algorithm, *graph, bst::kNodeS);
    ASSERT_TRUE(at_s.has_value());
    EXPECT_NEAR(*at_s, 0.0, bst::kTolerance);
}

/// @brief b(b) is 60, not 70: the node's own closing time caps it.
///
/// That cap is the min() in extend_back, and it is what makes a limit in the middle of the path
/// propagate backwards. Without it the value would be 100 - 30 = 70.
TEST(BackwardSearch, NodeClosingTimeCapsTheBackwardValue) {
    namespace bst = backward_search_test;
    auto graph = bst::time_window_graph();

    auto algorithm = graph->create_algorithm<test_util::BackwardOnlyAlgorithm, bst::BackwardList>(
        bst::inspectable_params());
    graph->solve(algorithm.get(), std::numeric_limits<double>::infinity(), /*preprocess=*/false);

    const auto at_b = bst::backward_value_at(*algorithm, *graph, bst::kNodeB);
    ASSERT_TRUE(at_b.has_value());
    EXPECT_NEAR(*at_b, 60.0, bst::kTolerance);
    EXPECT_LT(*at_b, 70.0);  // the raw subtraction, had the clamp not applied
}

/// @brief A backward value below its node's opening time is rejected, not clamped up to it.
///
/// extend_back deliberately does not clamp from below: clamping would turn an infeasible value
/// into a feasible-looking one and rob is_back_feasible of the case it exists to catch.
TEST(BackwardSearch, BelowOpeningTimeIsRejectedNotClamped) {
    namespace bst = backward_search_test;

    // Arc a -> t takes 50, and t closes at 60, so b(a) = min(40, 60 - 50) = 10 -- below a's
    // opening time of 30. The label must be rejected rather than lifted to 30.
    std::map<size_t, std::pair<double, double>> windows{{0, {30.0, 40.0}}, {1, {0.0, 60.0}}};

    TimeWindowExtensionFunction<RealResource> extension(windows);
    test_util::TestArc<RealResource> fixture(0, 1);
    auto extend = extension.create(fixture.arc);

    RealResource extended;
    extend->extend_back(RealResource(60.0), RealResource(50.0), &extended);
    EXPECT_NEAR(extended.get_value(), 10.0, bst::kTolerance);  // not lifted to 30

    TimeWindowFeasibilityFunction<RealResource> feasibility(windows);
    auto at_node = feasibility.create(0);
    EXPECT_FALSE(at_node->is_back_feasible(extended));
}

// ============================================================================
// Dominance direction, through the real algorithm
// ============================================================================

/// @brief Two backward routes to one node, equal cost and different deadlines: the LATER survives.
///
/// The integration counterpart of phase 7's container unit test. Node 1 is reachable backward from
/// the sink both directly (deadline 100 - 10 = 90) and via node 2 (deadline 100 - 30 - 20 = 50),
/// at equal cost. Backward, a later deadline is more permissive, so 90 must survive.
TEST(BackwardSearch, LaterDeadlineSurvivesBackwardDominance) {
    namespace bst = backward_search_test;

    // Windows are slack everywhere so only the dominance decision is under test.
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 1000.0}},
                                                        {1, {0.0, 1000.0}},
                                                        {2, {0.0, 1000.0}},
                                                        {3, {0.0, 100.0}}};

    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());

    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2);
    graph->add_node(3, /*source=*/false, /*sink=*/true);

    graph->add_arc<RealResource>(std::make_tuple(5.0), 0, 1);
    graph->add_arc<RealResource>(std::make_tuple(10.0), 1, 3);  // direct: b(1) = 90
    graph->add_arc<RealResource>(std::make_tuple(20.0), 1, 2);  // via 2:  b(1) = 50
    graph->add_arc<RealResource>(std::make_tuple(30.0), 2, 3);

    auto algorithm = graph->create_algorithm<test_util::BackwardOnlyAlgorithm, bst::BackwardList>(
        bst::inspectable_params());
    graph->solve(algorithm.get(), std::numeric_limits<double>::infinity(), /*preprocess=*/false);

    const auto* node = graph->get_node(1);
    const auto& labels = algorithm->get_labels_by_node_pos().at(node->pos()).get_labels();
    ASSERT_EQ(labels.size(), 1U) << "the two labels should have resolved to one by dominance";
    EXPECT_NEAR(bst::component_value(*labels.front()), 90.0, bst::kTolerance);
}
