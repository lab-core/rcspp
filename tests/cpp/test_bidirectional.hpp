// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Phase 11: the assembled bidirectional algorithm.
//
// ShortPathIsFoundWithBoundEnabled is the test a joiner-only design silently fails. A complete path
// whose critical value never exceeds H has no arc on which the clock crosses H, so the joiner never
// produces it -- it exists only as a forward label that ran all the way to a sink. Short routes are
// exactly that case and are common in pricing.
//
// HalfPathPruningKeepsNegativeReducedCosts is the regression test for the pruning trap: a forward
// half costing 30 can complete to -20 once reduced costs are negative, so comparing a half's own
// cost against the incumbent discards precisely the labels that lead to the best columns -- while
// reporting COMPLETE.

#include <gtest/gtest.h>

#include <limits>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace bidirectional_test {

using Composed = ResourceTypeComposition<RealResource>;
using Bidirectional = BidirectionalAlgoBound<RealResource>::Algo<Composed, LabelList<Composed>>;

constexpr double kTolerance = 1e-9;

/// @brief Solves with the ordinary forward algorithm, for comparison.
inline double forward_optimum(ResourceGraph<RealResource>* graph) {
    const auto result = graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    return result.solutions.empty() ? std::numeric_limits<double>::infinity()
                                    : result.solutions.front().cost;
}

/// @brief Params that keep the label containers alive for inspection after the solve.
inline AlgorithmParams<LabelList<Composed>> params(double half_way_point,
                                                   size_t critical_resource_index = 0) {
    AlgorithmParams<LabelList<Composed>> p;
    p.release_after_solve = false;
    p.half_way_point = half_way_point;
    p.critical_resource_index = critical_resource_index;
    return p;
}

/// @brief A line graph whose one component is both the clock and the cost.
///
/// A plain additive resource, so it is monotone and the half-way bound applies.
inline std::unique_ptr<ResourceGraph<RealResource>> line_graph(
    const std::vector<double>& arc_values) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    const size_t num_nodes = arc_values.size() + 1;
    for (size_t node_id = 0; node_id < num_nodes; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id + 1 == num_nodes);
    }
    for (size_t i = 0; i < arc_values.size(); ++i) {
        graph->add_arc<RealResource>(std::make_tuple(arc_values[i]), i, i + 1, arc_values[i]);
    }
    return graph;
}

/// @brief Two routes from source to sink, so the algorithm has a real choice to make.
///
/// 0 -> 1 -> 3 costs @p upper_cost; 0 -> 2 -> 3 costs @p lower_cost.
inline std::unique_ptr<ResourceGraph<RealResource>> diamond_graph(double upper_first,
                                                                  double upper_second,
                                                                  double lower_first,
                                                                  double lower_second) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2);
    graph->add_node(3, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource>(std::make_tuple(upper_first), 0, 1, upper_first);
    graph->add_arc<RealResource>(std::make_tuple(upper_second), 1, 3, upper_second);
    graph->add_arc<RealResource>(std::make_tuple(lower_first), 0, 2, lower_first);
    graph->add_arc<RealResource>(std::make_tuple(lower_second), 2, 3, lower_second);
    return graph;
}

/// @brief A line graph with a cost slot and a *threshold* clock slot, so the bound applies.
///
/// Component 0 is the cost (`AdditionExtensionFunction`, `Accumulate`). Component 1 is the clock:
/// `BudgetExtensionFunction`, whose backward form counts down from the capacity and therefore
/// puts a backward label's value on the same scale as a forward one. That is what the half-way
/// rule compares against `H`, and it is why a plain additive resource cannot serve as the clock.
///
/// @param arc_values  Per-arc `{cost, clock}` consumption, one pair per arc.
/// @param capacity    The clock's upper bound `R`.
/// @param clock_floor The clock's lower bound. Negative only where an arc has to be allowed to
///                    turn the clock back -- `FeasibilityPreprocessor` builds a node's initial
///                    resource by extending its predecessor's *default* value, so with a floor of
///                    0 a negative consumption leaves that node with no feasible initial resource
///                    and every arc out of it is removed before the solve begins.
inline std::unique_ptr<ResourceGraph<RealResource>> clock_line_graph(
    const std::vector<std::pair<double, double>>& arc_values, double capacity,
    double clock_floor = 0.0) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(std::make_unique<BudgetExtensionFunction<RealResource>>(),
                                      std::make_unique<MinMaxFeasibilityFunction<RealResource>>(
                                          clock_floor,
                                          capacity,
                                          /*merge_by_increasing_value=*/true),
                                      std::make_unique<TrivialCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    const size_t num_nodes = arc_values.size() + 1;
    for (size_t node_id = 0; node_id < num_nodes; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id + 1 == num_nodes);
    }
    for (size_t i = 0; i < arc_values.size(); ++i) {
        graph->add_arc<RealResource, RealResource>({arc_values[i].first, arc_values[i].second},
                                                   i,
                                                   i + 1,
                                                   arc_values[i].first);
    }
    return graph;
}

/// @brief The clock lives in slot 1 of `clock_line_graph`.
constexpr size_t kClockIndex = 1;

}  // namespace bidirectional_test

// ============================================================================
// End to end
// ============================================================================

/// @brief A cost-only line: the bidirectional optimum matches the hand-computed one.
///
/// The model's only resource is the cost, which extends backwards by *accumulating* -- a backward
/// label's value counts consumption from the sink, not a ceiling on the forward scale. So the
/// bound turns itself off here even though `half_way_point` was supplied, and the answer comes
/// from the forward search reaching the sink. Correct but slow is the right failure mode; the
/// alternative is a backward search that discards its own seed and reports COMPLETE.
TEST(Bidirectional, CostOnlyLineMatchesHandComputedOptimum) {
    namespace bt = bidirectional_test;
    auto graph = bt::line_graph({1.0, 2.0, 3.0, 4.0});

    auto algorithm =
        graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(bt::params(5.0));
    const auto result = graph->solve(algorithm.get());

    EXPECT_FALSE(algorithm->bounded_by_half_way())
        << "an accumulating critical resource is not a clock and must disable the bound";
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions.front().cost, 10.0, bt::kTolerance);  // 1 + 2 + 3 + 4
    EXPECT_EQ(result.solutions.front().path_arc_ids, (std::vector<size_t>{0, 1, 2, 3}));
}

/// @brief With two routes available, the cheaper one wins -- and matches the forward search.
TEST(Bidirectional, DiamondMatchesForwardOptimum) {
    namespace bt = bidirectional_test;
    auto forward_graph = bt::diamond_graph(4.0, 4.0, 1.0, 2.0);
    auto graph = bt::diamond_graph(4.0, 4.0, 1.0, 2.0);

    const double expected = bt::forward_optimum(forward_graph.get());
    ASSERT_NEAR(expected, 3.0, bt::kTolerance);  // 1 + 2 beats 4 + 4

    auto algorithm =
        graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(bt::params(2.0));
    const auto result = graph->solve(algorithm.get());

    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions.front().cost, expected, bt::kTolerance);
}

/// @brief A capacity-bounded instance still finds the optimum.
///
/// The capacity uses BudgetExtensionFunction, the coherent pairing for a bounded resource: it is a
/// Threshold, so a backward label counts *down* from the bound rather than adding to it.
TEST(Bidirectional, CapacityInstanceMatchesForwardOptimum) {
    namespace bt = bidirectional_test;

    auto build = []() {
        auto graph = std::make_unique<ResourceGraph<RealResource>>();
        graph->add_resource<RealResource>(
            std::make_unique<AdditionExtensionFunction<RealResource>>(),
            std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
            std::make_unique<ValueCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
        graph->add_resource<RealResource>(std::make_unique<BudgetExtensionFunction<RealResource>>(),
                                          std::make_unique<MinMaxFeasibilityFunction<RealResource>>(
                                              0.0,
                                              5.0,
                                              /*merge_by_increasing_value=*/true),
                                          std::make_unique<TrivialCostFunction<RealResource>>(),
                                          std::make_unique<ValueDominanceFunction<RealResource>>());
        graph->add_node(0, /*source=*/true, /*sink=*/false);
        graph->add_node(1);
        graph->add_node(2);
        graph->add_node(3, /*source=*/false, /*sink=*/true);
        // The cheap route is too heavy: 4 + 4 = 8 > 5.
        graph->add_arc<RealResource, RealResource>({1.0, 4.0}, 0, 1, 1.0);
        graph->add_arc<RealResource, RealResource>({1.0, 4.0}, 1, 3, 1.0);
        graph->add_arc<RealResource, RealResource>({3.0, 1.0}, 0, 2, 3.0);
        graph->add_arc<RealResource, RealResource>({3.0, 1.0}, 2, 3, 3.0);
        return graph;
    };

    auto forward_graph = build();
    const double expected = bt::forward_optimum(forward_graph.get());
    ASSERT_NEAR(expected, 6.0, bt::kTolerance);  // the heavy 2-cost route is infeasible

    auto graph = build();
    // Slot 1 is the budget: a Threshold, so it can serve as the clock and the bound stays on.
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bt::params(3.0, /*critical_resource_index=*/1));
    const auto result = graph->solve(algorithm.get());

    EXPECT_TRUE(algorithm->bounded_by_half_way());
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions.front().cost, expected, bt::kTolerance);
}

/// @brief A time-window instance: the cheap route misses its deadline, so the dear one wins.
///
/// `TimeWindowExtensionFunction` is the other `Threshold`, and the one bidirectional labeling was
/// invented for: a backward label carries the *latest* departure that still meets every downstream
/// window -- a deadline on the same scale as a forward arrival, which is exactly what the half-way
/// rule needs.
TEST(Bidirectional, TimeWindowInstanceMatchesForwardOptimum) {
    namespace bt = bidirectional_test;

    // Node 3 closes at 5. The cheap route 0 -> 2 -> 3 arrives at 6 and is out.
    const std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}},
                                                              {1, {0.0, 100.0}},
                                                              {2, {0.0, 100.0}},
                                                              {3, {0.0, 5.0}}};

    auto build = [&windows]() {
        auto graph = std::make_unique<ResourceGraph<RealResource>>();
        graph->add_resource<RealResource>(
            std::make_unique<AdditionExtensionFunction<RealResource>>(),
            std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
            std::make_unique<ValueCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
        graph->add_resource<RealResource>(
            std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
            std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
            std::make_unique<TrivialCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
        graph->add_node(0, /*source=*/true, /*sink=*/false);
        graph->add_node(1);
        graph->add_node(2);
        graph->add_node(3, /*source=*/false, /*sink=*/true);
        graph->add_arc<RealResource, RealResource>({10.0, 1.0}, 0, 1, 10.0);
        graph->add_arc<RealResource, RealResource>({10.0, 1.0}, 1, 3, 10.0);
        graph->add_arc<RealResource, RealResource>({1.0, 3.0}, 0, 2, 1.0);
        graph->add_arc<RealResource, RealResource>({1.0, 3.0}, 2, 3, 1.0);
        return graph;
    };

    auto forward_graph = build();
    const double expected = bt::forward_optimum(forward_graph.get());
    ASSERT_NEAR(expected, 20.0, bt::kTolerance);  // the cheap route arrives at 6 > 5

    auto graph = build();
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bt::params(3.0, bt::kClockIndex));
    const auto result = graph->solve(algorithm.get());

    EXPECT_TRUE(algorithm->bounded_by_half_way())
        << "a time window is a threshold, so the bound applies";
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions.front().cost, expected, bt::kTolerance);
}

// ============================================================================
// THE short-path test
// ============================================================================

/// @brief A path whose clock never reaches H is still found, with the bound ENABLED.
///
/// It has no crossing arc, so the joiner cannot produce it. It survives only because a forward
/// label reaches a sink and terminal collection sweeps that direction too. A joiner-only design
/// loses this silently.
TEST(Bidirectional, ShortPathIsFoundWithBoundEnabled) {
    namespace bt = bidirectional_test;
    // Total clock 2, but H = 50: the path never comes close to crossing.
    auto graph = bt::clock_line_graph({{1.0, 1.0}, {1.0, 1.0}}, /*capacity=*/100.0);

    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bt::params(50.0, bt::kClockIndex));
    const auto result = graph->solve(algorithm.get());

    ASSERT_TRUE(algorithm->bounded_by_half_way())
        << "the point of this test is that the bound is ON while the path never crosses H";
    ASSERT_FALSE(result.solutions.empty())
        << "a path that never crosses H must still be found, via the forward search";
    EXPECT_NEAR(result.solutions.front().cost, 2.0, bt::kTolerance);
    EXPECT_EQ(result.solutions.front().path_arc_ids, (std::vector<size_t>{0, 1}));
}

// ============================================================================
// Setup validation
// ============================================================================

/// @brief An undeclared component is named at setup, before any label exists.
TEST(Bidirectional, UndeclaredComponentIsNamedAtSetup) {
    namespace bt = bidirectional_test;

    // Neither half declares anything: the extension inherits the base class's Unspecified
    // backward_kind, the feasibility function its Unspecified merge_rule. Both must be named,
    // in one error -- a model author fixing one at a time is a model author running the solve
    // twice to find out about the other.
    class UndeclaredExtensionFunction
        : public Clonable<UndeclaredExtensionFunction, ExtensionFunction<RealResource>> {
        public:
            void extend(const RealResource& resource, const RealResource& extender_value,
                        RealResource* extended_resource) override {
                extended_resource->set_value(resource.get_value() + extender_value.get_value());
            }
    };

    class UndeclaredFeasibilityFunction
        : public Clonable<UndeclaredFeasibilityFunction, FeasibilityFunction<RealResource>> {
        public:
            [[nodiscard]] auto is_feasible(const RealResource& /*resource*/) -> bool override {
                return true;
            }
    };

    auto graph = std::make_unique<ResourceGraph<RealResource>>();

    // Routed through the ERASED overload on purpose. This test is about the setup refusal, which
    // is the only path the Python bindings have; the typed overload rejects an undeclared
    // component at *compile* time, which test_typed_add_resource.hpp documents instead. Two
    // base-typed locals are more than the overload resolution needs -- one is enough to
    // disqualify the typed overload -- but both declarations are the subject of this test, so
    // both are spelled out.
    std::unique_ptr<ExtensionFunction<RealResource>> extension =
        std::make_unique<UndeclaredExtensionFunction>();
    std::unique_ptr<FeasibilityFunction<RealResource>> feasibility =
        std::make_unique<UndeclaredFeasibilityFunction>();
    graph->add_resource<RealResource>(std::move(extension),
                                      std::move(feasibility),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource>(std::make_tuple(1.0), 0, 1, 1.0);

    auto algorithm =
        graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(bt::params(1.0));

    try {
        graph->solve(algorithm.get());
        FAIL() << "an undeclared component must be refused at setup";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("component 0"), std::string::npos) << message;
        EXPECT_NE(message.find("backward_kind"), std::string::npos) << message;
        EXPECT_NE(message.find("merge_rule"), std::string::npos) << message;
    }

    // Refused before any label was created: naming the component costs one pass at setup, where
    // discovering it from inside the join would cost a whole solve.
    EXPECT_EQ(algorithm->get_label_pool().get_nb_total_labels(), 0U);
}

/// @brief An accumulating extension paired with a back seed is refused, naming the fix.
///
/// Both halves declare legal values individually, so only a check that sees both catches it. This
/// is the pairing the repository's own VRP demand resource uses -- fine forward-only, fatal
/// backward.
TEST(Bidirectional, AccumulateWithBackSeedIsRefused) {
    namespace bt = bidirectional_test;

    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    // Addition accumulates; MinMax with the default merge direction supplies a back seed.
    //
    // The typed add_resource overload would NOT reject this pairing either --
    // MinMaxFeasibilityFunction chooses its seed end from a constructor argument, so its
    // BackSeedEnd is Unknown and the static_assert stays silent. The runtime probe
    // seeds_itself_out_of_range is the only thing that catches it, which is why it stays and why
    // this test is about the runtime refusal. Routed through the erased overload so the test
    // keeps testing that refusal even if the trait's answer for MinMax ever changes.
    std::unique_ptr<ExtensionFunction<RealResource>> accumulating =
        std::make_unique<AdditionExtensionFunction<RealResource>>();
    graph->add_resource<RealResource>(std::move(accumulating),
                                      std::make_unique<MinMaxFeasibilityFunction<RealResource>>(
                                          0.0,
                                          5.0,
                                          /*merge_by_increasing_value=*/true),
                                      std::make_unique<TrivialCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource, RealResource>({1.0, 1.0}, 0, 1, 1.0);

    auto algorithm =
        graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(bt::params(1.0));

    try {
        graph->solve(algorithm.get());
        FAIL() << "an accumulating extension with a backward seed must be refused";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("component 1"), std::string::npos) << message;
        EXPECT_NE(message.find("BudgetExtensionFunction"), std::string::npos)
            << "the error should name the coherent alternative: " << message;
    }
}

// DisjointOnAResourceWithoutIntersectsIsRefused was deleted in step 6.
//
// Its subject -- a scalar resource declaring disjointness -- became unrepresentable: the merge
// body moved onto DisjointMergeForm, which does not compile for a resource without intersects(),
// so there is nothing left for the validator to refuse. That is why `describe_problem` lost its
// third complaint at the same time, and why deleting a test that asserted a safety property is
// the right move here rather than a regression: the property is now enforced by the type system
// instead of by the validator.
//
// UndeclaredComponentIsRefused above asserts on complaints #1 and #2 and is unaffected.

// ============================================================================
// The half-way bound's failure mode
// ============================================================================

/// @brief A non-monotone clock disables the bound, does not throw, and still finds the optimum.
TEST(Bidirectional, NonMonotoneClockDisablesTheBoundAndStillSolves) {
    namespace bt = bidirectional_test;

    // A negative arc value on the critical slot: exactly what a reduced cost looks like. Both
    // graphs cost 1 + 2 + 3; they differ only in the middle arc's clock consumption. The first
    // arc lifts the clock to 5 so that subtracting 2 still leaves it inside [0, 20] -- the path
    // has to stay *feasible*, or preprocessing removes the arcs and there is nothing left to
    // probe, which is a different failure wearing this one's clothes.
    auto monotone = bt::clock_line_graph({{1.0, 5.0}, {2.0, 2.0}, {3.0, 3.0}}, /*capacity=*/20.0);
    auto non_monotone = bt::clock_line_graph({{1.0, 5.0}, {2.0, -2.0}, {3.0, 3.0}},
                                             /*capacity=*/20.0,
                                             /*clock_floor=*/-20.0);

    auto monotone_algorithm =
        monotone->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
            bt::params(3.0, bt::kClockIndex));
    const auto monotone_result = monotone->solve(monotone_algorithm.get());
    ASSERT_FALSE(monotone_result.solutions.empty());
    EXPECT_TRUE(monotone_algorithm->bounded_by_half_way());

    auto algorithm = non_monotone->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bt::params(3.0, bt::kClockIndex));
    SolveResult result;
    EXPECT_NO_THROW({ result = non_monotone->solve(algorithm.get()); });

    EXPECT_FALSE(algorithm->bounded_by_half_way())
        << "a non-monotone clock must disable the bound rather than be trusted";
    ASSERT_FALSE(result.solutions.empty()) << "disabling the bound must not lose the optimum";
    EXPECT_NEAR(result.solutions.front().cost, 6.0, bt::kTolerance);  // 1 + 2 + 3
}

/// @brief A clock that does not take part in dominance disables the bound, and the answer survives.
///
/// The half-way bound assumes a dominated label's dominator is a valid substitute at the join too,
/// which needs the clock inside the dominance order. With a trivial dominance it is not: a
/// dominator above H evicts a label below it, and the path is lost while the solve reports
/// COMPLETE. Disabling is the right response -- correct but slow, the same treatment a non-monotone
/// clock gets.
TEST(Bidirectional, ATrivialDominanceOnTheClockDisablesTheBound) {
    namespace bt = bidirectional_test;

    // Same shape as clock_line_graph, but the clock slot carries a TrivialDominanceFunction.
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(std::make_unique<BudgetExtensionFunction<RealResource>>(),
                                      std::make_unique<MinMaxFeasibilityFunction<RealResource>>(
                                          0.0,
                                          20.0,
                                          /*merge_by_increasing_value=*/true),
                                      std::make_unique<TrivialCostFunction<RealResource>>(),
                                      std::make_unique<TrivialDominanceFunction<RealResource>>());
    for (size_t node_id = 0; node_id < 4; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id == 3);
    }
    graph->add_arc<RealResource, RealResource>({1.0, 5.0}, 0, 1, 1.0);
    graph->add_arc<RealResource, RealResource>({2.0, 2.0}, 1, 2, 2.0);
    graph->add_arc<RealResource, RealResource>({3.0, 3.0}, 2, 3, 3.0);

    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bt::params(3.0, bt::kClockIndex));
    SolveResult result;
    EXPECT_NO_THROW({ result = graph->solve(algorithm.get()); });

    EXPECT_FALSE(algorithm->bounded_by_half_way())
        << "a clock outside the dominance order must not be trusted";
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions.front().cost, 6.0, bt::kTolerance);  // 1 + 2 + 3
}

/// @brief A critical resource type outside the model's pack disables the bound instead of failing
///        to compile.
///
/// The algorithm has three `if constexpr` branches for this case. They were unreachable: the joiner
/// read the critical component unguarded, and `get_component<T>` resolves a constrained trait that
/// does not exist for an absent `T`, so the instantiation was a hard error. This test exists mostly
/// to be compiled.
TEST(Bidirectional, AnAbsentCriticalResourceTypeDisablesTheBound) {
    namespace bt = bidirectional_test;
    auto graph = bt::line_graph({1.0, 2.0, 3.0});  // the pack is <RealResource> only

    // IntResource is NOT in this graph's pack.
    auto algorithm =
        graph->create_algorithm<BidirectionalAlgoBound<IntResource>::Algo>(bt::params(2.0));
    SolveResult result;
    EXPECT_NO_THROW({ result = graph->solve(algorithm.get()); });

    EXPECT_FALSE(algorithm->bounded_by_half_way());
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions.front().cost, 6.0, bt::kTolerance);  // 1 + 2 + 3
}

// ============================================================================
// The pruning trap
// ============================================================================

/// @brief A negative-cost completion is not pruned away by a half's own cost.
///
/// The upper route's first arc costs 30 on its own, which the naive test would compare against an
/// incumbent of 10 and discard -- yet it completes to -20, the true optimum. Only a completion
/// bound keeps it.
TEST(Bidirectional, HalfPathPruningKeepsNegativeReducedCosts) {
    namespace bt = bidirectional_test;
    // Upper: 30 then -50 => -20. Lower: 5 then 5 => 10.
    auto forward_graph = bt::diamond_graph(30.0, -50.0, 5.0, 5.0);
    const double expected = bt::forward_optimum(forward_graph.get());
    ASSERT_NEAR(expected, -20.0, bt::kTolerance);

    auto graph = bt::diamond_graph(30.0, -50.0, 5.0, 5.0);
    auto algorithm =
        graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(bt::params(10.0));
    const auto result = graph->solve(algorithm.get());

    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions.front().cost, expected, bt::kTolerance)
        << "a half costing 30 completes to -20; pruning on the half's own cost loses it";
}

/// @brief The completion bound is admissible under reduced costs.
///
/// `arc.cost` is the ORIGINAL arc weight; `update_reduced_costs` rewrites the cost *component* and
/// leaves `arc.cost` alone. A completion bound relaxed on `arc.cost` therefore over-estimates the
/// remaining reduced cost, and an over-estimating bound discards labels on the true optimal path
/// while reporting COMPLETE.
///
/// The instance: a chain whose original weights are 1000 / 1 / 1000 and whose reduced costs are
/// -1 / -1000 / -1, plus a direct arc (weight 1, reduced cost -2) that sets a cheap incumbent.
/// The optimum is -1002. Relaxed on `arc.cost`, `h_to_sink(2)` reads 1000 instead of -1, so the
/// label at node 2 -- cost -1001, one arc from a -1002 completion -- is pruned against an
/// incumbent of -2, and the solve returns -2.
TEST(Bidirectional, CompletionBoundIsAdmissibleUnderReducedCosts) {
    namespace bt = bidirectional_test;

    // {reduced cost carried by the labels, original arc.cost}
    const auto build = [] {
        auto graph = std::make_unique<ResourceGraph<RealResource>>();
        graph->add_resource<RealResource>(
            std::make_unique<AdditionExtensionFunction<RealResource>>(),
            std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
            std::make_unique<ValueCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
        graph->add_node(0, /*source=*/true, /*sink=*/false);
        graph->add_node(1);
        graph->add_node(2);
        graph->add_node(3, /*source=*/false, /*sink=*/true);
        graph->add_arc<RealResource>(std::make_tuple(-1.0), 0, 1, /*arc.cost=*/1000.0);
        graph->add_arc<RealResource>(std::make_tuple(-1000.0), 1, 2, /*arc.cost=*/1.0);
        graph->add_arc<RealResource>(std::make_tuple(-1.0), 2, 3, /*arc.cost=*/1000.0);
        graph->add_arc<RealResource>(std::make_tuple(-2.0), 0, 3, /*arc.cost=*/1.0);
        return graph;
    };

    auto reference_graph = build();
    const double expected = bt::forward_optimum(reference_graph.get());
    ASSERT_NEAR(expected, -1002.0, bt::kTolerance);

    auto graph = build();
    auto p = bt::params(/*half_way_point=*/0.0);
    p.prune_based_on_upper_bound_ = true;
    p.heuristic_cost_index = 0;  // the slot the labels accumulate; the bound must use the same one
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(p);

    // Preprocessing off: it would remove arcs on original weights and change what is measured.
    const auto result = graph->solve(algorithm.get(),
                                     std::numeric_limits<double>::infinity(),
                                     /*preprocess=*/false);

    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions.front().cost, expected, bt::kTolerance)
        << "the completion bound over-estimated the remaining reduced cost and pruned the optimum";
}

// ============================================================================
// Pool hygiene
// ============================================================================

/// @brief Reference counts stay consistent after a full bidirectional solve.
TEST(Bidirectional, RefCountsStayConsistentAfterSolve) {
    namespace bt = bidirectional_test;
    auto graph = bt::diamond_graph(4.0, 4.0, 1.0, 2.0);

    auto algorithm =
        graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(bt::params(2.0));
    graph->solve(algorithm.get());

    EXPECT_TRUE(algorithm->get_label_pool().check_ref_count_consistency())
        << "two searches and a join pass must leave the reference counts intact";
}

/// @brief Both directions' terminal labels are collected.
///
/// The backward search reaching a source is a third source of solutions alongside the forward
/// search reaching a sink and the join.
TEST(Bidirectional, BackwardSearchReachesTheSource) {
    namespace bt = bidirectional_test;
    auto graph = bt::clock_line_graph({{1.0, 1.0}, {1.0, 1.0}}, /*capacity=*/100.0);

    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bt::params(50.0, bt::kClockIndex));
    graph->solve(algorithm.get());

    // The backward label seeds at the capacity, 100, and each arc takes 1 off it, so it stays far
    // above H = 50 the whole way: the backward search runs back to the source with the bound on.
    const auto* source = graph->get_node(0);
    const auto& at_source =
        algorithm->get_backward_labels_by_node_pos().at(source->pos()).get_labels();
    EXPECT_FALSE(at_source.empty())
        << "the backward search should reach the source and contribute a terminal label";
}

/// @brief Memory pressure trims both frontiers without corrupting the pool.
///
/// `memory_pressure_fraction = 0` makes every check report pressure while the enormous limit means
/// the solve is never stopped outright, so `on_memory_pressure()` runs on each iteration -- the
/// same recipe `test_memory_pressure_pruning` uses. A bidirectional search has *two* frontiers, so
/// the thing worth asserting is that trimming reaches both and leaves the reference counts intact:
/// a frontier entry that is dropped rather than released is a leak, and one released while its
/// container still holds it is a double free.
TEST(Bidirectional, MemoryPressureTrimsBothFrontiers) {
    namespace bt = bidirectional_test;
    auto graph = bt::clock_line_graph({{1.0, 1.0}, {2.0, 1.0}, {3.0, 1.0}}, /*capacity=*/20.0);

    auto under_pressure = [](size_t max_labels_per_node) {
        auto p = bt::params(3.0, bt::kClockIndex);
        constexpr double kHugeLimitGiB = 1e9;  // effectively unlimited: pressure, never a stop
        p.max_memory_gb = kHugeLimitGiB;
        p.memory_pressure_fraction = 0.0;
        p.memory_check_interval = 1;
        p.memory_pressure_max_labels_per_node = max_labels_per_node;
        return p;
    };

    // A generous cap: pressure is reported on every check, but no frontier is over it, so the
    // trim returns without touching anything.
    auto roomy = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        under_pressure(/*max_labels_per_node=*/1000));
    SolveResult roomy_result;
    EXPECT_NO_THROW({ roomy_result = graph->solve(roomy.get()); });
    ASSERT_FALSE(roomy_result.solutions.empty())
        << "reporting pressure without exceeding the cap must not change the answer";
    EXPECT_TRUE(roomy->get_label_pool().check_ref_count_consistency());

    // And a cap of zero, where every frontier entry is dropped on the first check.
    auto squeezed = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        under_pressure(/*max_labels_per_node=*/0));
    EXPECT_NO_THROW({ graph->solve(squeezed.get()); });
    EXPECT_TRUE(squeezed->get_label_pool().check_ref_count_consistency())
        << "trimming a frontier must not leak or double-release a label";
}

/// @brief Memory pressure tightens the per-node extension quota, not just the frontier.
///
/// Trimming alone is a one-off dip -- the frontiers refill from continued extension -- so the
/// observable consequence of the quota is that far fewer labels are extended in total. Measured
/// through `get_number_of_extended_labels()`, which is the number the whole benchmark is built on.
TEST(Bidirectional, MemoryPressureTightensThePerNodeQuota) {
    namespace bt = bidirectional_test;

    // A line graph will not do: with one label per node a quota of 1 never bites. This one gives
    // node 3 two non-dominated labels -- cheap-but-slow through 1, dear-but-quick through 2 -- so
    // the quota has something to refuse.
    const auto build = [] {
        auto graph = std::make_unique<ResourceGraph<RealResource>>();
        graph->add_resource<RealResource>(
            std::make_unique<AdditionExtensionFunction<RealResource>>(),
            std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
            std::make_unique<ValueCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
        graph->add_resource<RealResource>(std::make_unique<BudgetExtensionFunction<RealResource>>(),
                                          std::make_unique<MinMaxFeasibilityFunction<RealResource>>(
                                              0.0,
                                              40.0,
                                              /*merge_by_increasing_value=*/true),
                                          std::make_unique<TrivialCostFunction<RealResource>>(),
                                          std::make_unique<ValueDominanceFunction<RealResource>>());
        for (size_t node_id = 0; node_id < 5; ++node_id) {
            graph->add_node(node_id, node_id == 0, node_id == 4);
        }
        graph->add_arc<RealResource, RealResource>({1.0, 10.0}, 0, 1, 1.0);   // cheap, slow
        graph->add_arc<RealResource, RealResource>({10.0, 1.0}, 0, 2, 10.0);  // dear, quick
        graph->add_arc<RealResource, RealResource>({1.0, 1.0}, 1, 3, 1.0);
        graph->add_arc<RealResource, RealResource>({1.0, 1.0}, 2, 3, 1.0);
        graph->add_arc<RealResource, RealResource>({1.0, 1.0}, 3, 4, 1.0);
        return graph;
    };

    const auto run = [&build](bool under_pressure) {
        auto graph = build();
        // H = 20, above the slow route's clock of 11: both labels at node 3 have to survive the
        // half-way bound, or there is only one of them and the quota has nothing to refuse.
        auto p = bt::params(20.0, bt::kClockIndex);
        if (under_pressure) {
            constexpr double kHugeLimitGiB = 1e9;  // pressure on every check, never a hard stop
            p.max_memory_gb = kHugeLimitGiB;
            p.memory_pressure_fraction = 0.0;
            p.memory_check_interval = 1;
            p.memory_pressure_max_labels_per_node = 1;
        }
        auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(p);
        graph->solve(algorithm.get());
        return std::make_pair(algorithm->get_number_of_extended_labels(),
                              algorithm->memory_pressure_was_triggered());
    };

    const auto [relaxed_labels, relaxed_flag] = run(/*under_pressure=*/false);
    const auto [pressed_labels, pressed_flag] = run(/*under_pressure=*/true);

    EXPECT_FALSE(relaxed_flag);
    EXPECT_TRUE(pressed_flag) << "the pressure recipe did not fire";
    EXPECT_LT(pressed_labels, relaxed_labels)
        << "memory pressure trimmed the frontiers but left the per-node quota at its original "
           "value, so they refilled immediately";
}

/// @brief The default `release_after_solve` frees the backward containers too.
///
/// Every other test here turns that off so it can look at the label sets afterwards, which means
/// the release path -- the one every real caller takes -- would otherwise never run. A
/// bidirectional solve owns a second set of containers and two frontiers, none of which the base
/// class knows about.
TEST(Bidirectional, ReleasingAfterSolveClearsTheBackwardContainers) {
    namespace bt = bidirectional_test;
    auto graph = bt::clock_line_graph({{1.0, 1.0}, {2.0, 1.0}}, /*capacity=*/20.0);

    AlgorithmParams<LabelList<bt::Composed>> p;  // release_after_solve left at its default
    p.half_way_point = 3.0;
    p.critical_resource_index = bt::kClockIndex;

    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(p);
    const auto result = graph->solve(algorithm.get());

    ASSERT_FALSE(result.solutions.empty()) << "releasing the labels must not lose the answer";
    EXPECT_NEAR(result.solutions.front().cost, 3.0, bt::kTolerance);
    EXPECT_TRUE(algorithm->get_backward_labels_by_node_pos().empty());
    EXPECT_TRUE(algorithm->get_label_pool().check_ref_count_consistency());
}

// ============================================================================
// Setup validation
// ============================================================================

/// @brief A graph with no arcs returns an empty result rather than raising.
///
/// The setup validation reads each component's declared backward kind off the first arc it finds.
/// With no arcs it finds nothing, and an earlier form of the check reported every component as
/// undeclared -- naming a modelling problem on a model that declares everything correctly.
TEST(Bidirectional, AGraphWithNoArcsReturnsAnEmptyResult) {
    namespace bt = bidirectional_test;

    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1, /*source=*/false, /*sink=*/true);

    auto algorithm =
        graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(bt::params(1.0));

    SolveResult result;
    EXPECT_NO_THROW({ result = graph->solve(algorithm.get()); });
    EXPECT_TRUE(result.solutions.empty());
}

/// @brief Preprocessing away every arc is not a modelling error.
///
/// `solve(preprocess = true)` with a binding `upper_bound` removes every arc that cannot lie on a
/// cheap enough path; at column-generation convergence that is all of them, and the solve that
/// terminates the loop is exactly this one. The forward search returns an empty result, and so
/// must this one.
TEST(Bidirectional, PreprocessingAwayEveryArcIsNotAModellingError) {
    namespace bt = bidirectional_test;
    constexpr double kBindingUpperBound = 0.0;  // every arc cost below is positive

    // The forward search is the reference: it returns nothing, without complaint.
    auto forward_graph = bt::line_graph({2.0, 3.0});
    const auto forward =
        forward_graph->solve<SimpleDominanceAlgorithm>(kBindingUpperBound,
                                                       AlgorithmParams<LabelList<bt::Composed>>{},
                                                       /*preprocess=*/true);
    ASSERT_TRUE(forward.solutions.empty());

    auto graph = bt::line_graph({2.0, 3.0});
    auto algorithm =
        graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(bt::params(1.0));

    SolveResult result;
    EXPECT_NO_THROW(
        { result = graph->solve(algorithm.get(), kBindingUpperBound, /*preprocess=*/true); });
    EXPECT_TRUE(result.solutions.empty());
}

// ============================================================================
// What counts as a path
// ============================================================================

/// @brief No returned path has a terminal node strictly inside it.
///
/// A backward label used to be allowed to land on a sink and keep going, so the unbounded
/// bidirectional search returned paths visiting the sink two and three times while the forward
/// search, which stops at a sink, could not produce them at all.
TEST(Bidirectional, NoReturnedPathHasATerminalInItsInterior) {
    namespace bt = bidirectional_test;

    // A three-node line plus a back arc out of the sink, so a walk COULD pass through it.
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource>(std::make_tuple(1.0), 0, 1, 1.0);
    graph->add_arc<RealResource>(std::make_tuple(1.0), 1, 2, 1.0);
    graph->add_arc<RealResource>(std::make_tuple(-10.0), 2, 1, -10.0);  // out of the sink

    auto algorithm =
        graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(bt::params(0.0));
    const auto result = graph->solve(algorithm.get(),
                                     std::numeric_limits<double>::infinity(),
                                     /*preprocess=*/false);

    for (const auto& solution : result.solutions) {
        const auto& nodes = solution.path_node_ids;
        ASSERT_GE(nodes.size(), 2U);
        for (size_t i = 1; i + 1 < nodes.size(); ++i) {
            EXPECT_FALSE(graph->get_node(nodes[i])->sink)
                << "sink " << nodes[i] << " appears inside a returned path";
        }
    }
}


// ============================================================================
// Parameters that are shared with the forward algorithms
// ============================================================================

/// @brief `return_dominated_solutions` records terminal paths as they are found, in both
///        directions.
///
/// Without it a path that reaches a terminal and is later dominated is gone from its container by
/// the time the end-of-solve sweep runs, so the caller never sees it -- while the forward
/// algorithms, which record at the terminal, do return it.
TEST(Bidirectional, ReturnDominatedSolutionsIsHonoured) {
    namespace bt = bidirectional_test;
    auto graph = bt::diamond_graph(4.0, 4.0, 1.0, 2.0);

    auto p = bt::params(2.0);
    p.return_dominated_solutions = true;
    p.stop_after_X_solutions = 100;  // check() warns when this is left at MAX with the flag on
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(p);
    const auto with_dominated = graph->solve(algorithm.get());

    auto plain_graph = bt::diamond_graph(4.0, 4.0, 1.0, 2.0);
    auto plain =
        plain_graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(bt::params(2.0));
    const auto without = plain_graph->solve(plain.get());

    EXPECT_GE(with_dominated.solutions.size(), without.solutions.size());
    ASSERT_FALSE(with_dominated.solutions.empty());
    ASSERT_FALSE(without.solutions.empty());
    EXPECT_NEAR(with_dominated.solutions.front().cost,
                without.solutions.front().cost,
                bt::kTolerance)
        << "recording more solutions must not change which one is best";
}

/// @brief With pruning off, a join that merely ties the incumbent is still returned.
///
/// The joiner used to consult and tighten `best_cost_upper_bound` whatever the caller asked for, so
/// a pricing pool that wanted every negative-reduced-cost column got only the improving ones. The
/// caller's own `upper_bound` is a different filter and stays unconditional.
///
/// The two routes join on *different* arcs on purpose: routed through a shared node they would meet
/// as two labels with identical resources, one would dominate the other, and the second solution
/// would be gone before the join ever saw it -- for a reason that has nothing to do with the cutoff
/// under test.
TEST(Bidirectional, TheJoinDoesNotPruneAgainstTheIncumbentUnlessAsked) {
    namespace bt = bidirectional_test;

    // 0 -> 1 -> 3 and 0 -> 2 -> 3, both costing 5, so the second exactly ties the incumbent.
    // The clock crosses H = 8 on the arcs into the sink, so both complete paths reach the join.
    const auto build = [] {
        auto graph = std::make_unique<ResourceGraph<RealResource>>();
        graph->add_resource<RealResource>(
            std::make_unique<AdditionExtensionFunction<RealResource>>(),
            std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
            std::make_unique<ValueCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
        graph->add_resource<RealResource>(std::make_unique<BudgetExtensionFunction<RealResource>>(),
                                          std::make_unique<MinMaxFeasibilityFunction<RealResource>>(
                                              0.0,
                                              40.0,
                                              /*merge_by_increasing_value=*/true),
                                          std::make_unique<TrivialCostFunction<RealResource>>(),
                                          std::make_unique<ValueDominanceFunction<RealResource>>());
        graph->add_node(0, /*source=*/true, /*sink=*/false);
        graph->add_node(1);
        graph->add_node(2);
        graph->add_node(3, /*source=*/false, /*sink=*/true);
        graph->add_arc<RealResource, RealResource>({2.0, 6.0}, 0, 1, 2.0);
        graph->add_arc<RealResource, RealResource>({3.0, 6.0}, 0, 2, 3.0);
        graph->add_arc<RealResource, RealResource>({3.0, 6.0}, 1, 3, 3.0);
        graph->add_arc<RealResource, RealResource>({2.0, 6.0}, 2, 3, 2.0);
        return graph;
    };

    const auto run = [&build](bool prune) {
        auto graph = build();
        auto p = bt::params(8.0, bt::kClockIndex);
        p.prune_based_on_upper_bound_ = prune;
        auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(p);
        const auto result = graph->solve(algorithm.get());
        return std::make_pair(result.solutions.size(), algorithm->number_of_joined_paths());
    };

    const auto [open_count, open_joins] = run(/*prune=*/false);  // the default, stated
    EXPECT_GT(open_joins, 0U)
        << "the two routes must meet at the join, or this test does not exercise the cutoff";
    EXPECT_GE(open_count, 2U)
        << "both equal-cost routes should be returned when pruning is off";

    // And with pruning on the incumbent cutoff bites, which is what the flag is for.
    const auto [pruned_count, pruned_joins] = run(/*prune=*/true);
    EXPECT_LE(pruned_count, open_count);
    EXPECT_LE(pruned_joins, open_joins);
}

// ============================================================================
// Diagnostics on the result
// ============================================================================

/// @brief The result carries the diagnostics, not just the algorithm object.
///
/// A Python caller never holds the algorithm, so anything the documentation tells them to check has
/// to be on `SolveResult`. The accessors stay, and read the same members, so the two cannot drift.
TEST(Bidirectional, SolveResultCarriesTheDiagnostics) {
    namespace bt = bidirectional_test;

    // A clock the bound can use: the budget slot of clock_line_graph.
    auto clocked = bt::clock_line_graph({{1.0, 5.0}, {2.0, 2.0}, {3.0, 3.0}}, /*capacity=*/20.0);
    auto with_clock = clocked->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bt::params(3.0, bt::kClockIndex));
    const auto bounded = clocked->solve(with_clock.get());
    EXPECT_TRUE(bounded.bounded_by_half_way);
    EXPECT_EQ(bounded.bounded_by_half_way, with_clock->bounded_by_half_way());
    EXPECT_EQ(bounded.number_of_joined_paths, with_clock->number_of_joined_paths());

    // A cost-only model has no clock, so the bound refuses to engage.
    auto cost_only = bt::line_graph({1.0, 2.0, 3.0});
    auto without_clock =
        cost_only->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(bt::params(3.0));
    const auto unbounded = cost_only->solve(without_clock.get());
    EXPECT_FALSE(unbounded.bounded_by_half_way);

    // Every other algorithm leaves the fields at their defaults.
    auto forward_graph = bt::line_graph({1.0, 2.0, 3.0});
    const auto forward = forward_graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    EXPECT_FALSE(forward.bounded_by_half_way);
    EXPECT_EQ(forward.number_of_joined_paths, 0U);
}

// ============================================================================
// The caller's upper bound
// ============================================================================

/// @brief No solution above the caller's upper bound, from any of the three sources.
///
/// A bidirectional solve records complete paths from three places: a forward label at a sink, a
/// backward label at a source, and a joined pair. Only the first went through the `Label` overload
/// of `extract_solution`, which is where the `cost >= cost_upper_bound_` filter lives, so a
/// backward half that ran all the way to the source was returned whatever it cost. In column
/// generation that is a non-improving column presented as a priced one.
TEST(Bidirectional, NoSolutionExceedsTheCallerUpperBound) {
    namespace bt = bidirectional_test;
    constexpr double kUpperBound = 0.0;  // "only strictly negative reduced costs, please"

    // The forward search is the reference: every path here costs +5, so it returns nothing.
    auto forward_graph = bt::line_graph({2.0, 3.0});
    const auto forward =
        forward_graph->solve<SimpleDominanceAlgorithm>(kUpperBound,
                                                       AlgorithmParams<LabelList<bt::Composed>>{},
                                                       /*preprocess=*/false);
    ASSERT_TRUE(forward.solutions.empty());

    auto graph = bt::line_graph({2.0, 3.0});
    auto algorithm =
        graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(bt::params(1.0));
    // Preprocessing off on purpose: with it on the preprocessor removes every arc first and the
    // backward search never reaches the source, so the case under test never arises.
    const auto result = graph->solve(algorithm.get(), kUpperBound, /*preprocess=*/false);

    EXPECT_TRUE(result.solutions.empty())
        << "a backward label reaching the source was recorded without the upper-bound filter";
    for (const auto& solution : result.solutions) {
        EXPECT_LT(solution.cost, kUpperBound);
    }
}
