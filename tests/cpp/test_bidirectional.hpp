// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// End-to-end tests for the bidirectional labeling algorithm.

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
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

/// @brief A line graph whose single additive component is the cost.
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

/// @brief Two routes from source to sink: upper 0 -> 1 -> 3 and lower 0 -> 2 -> 3.
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

/// @brief A line graph with a cost in slot 0 and a budget clock in slot 1.
///
/// The budget counts down backwards, so backward and forward labels share a scale and the
/// half-way bound applies.
///
/// @param arc_values Per-arc `{cost, clock}` consumption.
/// @param capacity   The clock's upper bound; its lower bound is zero.
inline std::unique_ptr<ResourceGraph<RealResource>> clock_line_graph(
    const std::vector<std::pair<double, double>>& arc_values, double capacity) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(std::make_unique<BudgetExtensionFunction<RealResource>>(),
                                      std::make_unique<MinMaxFeasibilityFunction<RealResource>>(
                                          0.0,
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
/// An accumulating cost is not a valid clock, so the bound disables itself even though
/// `half_way_point` is set.
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

/// @brief A capacity-bounded instance, with the budget as the clock, still finds the optimum.
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
    // Slot 1 is the budget, a valid clock.
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bt::params(3.0, /*critical_resource_index=*/1));
    const auto result = graph->solve(algorithm.get());

    EXPECT_TRUE(algorithm->bounded_by_half_way());
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions.front().cost, expected, bt::kTolerance);
}

/// @brief A time-window instance: the cheap route misses its deadline, so the dear one wins.
///
/// A backward time-window label carries the latest feasible departure, so it can serve as the
/// clock.
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
/// It has no arc crossing H, so the join cannot produce it; it must come from a forward label
/// reaching the sink.
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

    // Neither function declares its backward_kind / merge_rule; both must be named in one error.
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

    // Built through base-typed locals, as the Python bindings do. `add_resource` checks nothing
    // (undeclared is valid forward-only); the refusal comes at setup.
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

    // Refused before any label was created.
    EXPECT_EQ(algorithm->get_label_pool().get_nb_total_labels(), 0U);
}

/// @brief An accumulating extension paired with a back seed is refused, naming the fix.
///
/// Each function's declaration is legal on its own; only the pairing is wrong. MinMax no longer
/// makes this mistake (its seed follows the paired extension), so a function of the caller's own
/// stands in for it.
TEST(Bidirectional, AccumulateWithBackSeedIsRefused) {
    namespace bt = bidirectional_test;

    /// A cap of 5 that seeds backward labels at the cap whatever the extension does.
    class SeedsAtItsCap : public Clonable<SeedsAtItsCap, FeasibilityFunction<RealResource>> {
        public:
            auto is_feasible(const RealResource& resource) -> bool override {
                return resource.leq(5.0);
            }
            [[nodiscard]] MergeRule merge_rule() const override { return MergeRule::Custom; }
            auto can_be_merged(const RealResource& resource,
                               const RealResource& back_resource) -> bool override {
                return resource.get_value() + back_resource.get_value() <= 5.0;
            }
            [[nodiscard]] auto back_seed_value() const -> std::optional<RealResource> override {
                return RealResource(5.0);
            }
    };

    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<SeedsAtItsCap>(),
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

/// @brief A container whose feasibility test is a PREFIX question is refused, not solved.
///
/// Required-values feasibility asks what a label has collected so far, which a backward suffix
/// cannot answer, so the route would be silently lost. The forbidden-values sibling still solves,
/// showing the refusal is specific rather than "containers are refused".
TEST(Bidirectional, AContainerAskingAPrefixQuestionIsRefused) {
    namespace bt = bidirectional_test;

    // Built fresh per case. The arc carries {0}, so REQUIRED {0} is satisfied while FORBIDDEN {0}
    // would make node 1 infeasible and leave the preprocessor nothing to validate.
    // `node_mirror` selects an endpoint-mirror memory, which the solving sibling case needs.
    const auto build = [](bool forbidden, const std::set<int>& values, bool node_mirror = false) {
        auto graph = std::make_unique<ResourceGraph<RealResource, SetResource<int>>>();
        graph->add_resource<RealResource>(
            std::make_unique<AdditionExtensionFunction<RealResource>>(),
            std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
            std::make_unique<ValueCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());

        std::unique_ptr<ExtensionFunction<SetResource<int>>> memory;
        if (node_mirror) {
            memory = std::make_unique<NgPathExtensionFunction<SetResource<int>, int>>(
                std::map<size_t, std::set<int>>{{0, {0, 1}}, {1, {0, 1}}});
        } else {
            memory = std::make_unique<UnionExtensionFunction<SetResource<int>>>();
        }
        std::unique_ptr<FeasibilityFunction<SetResource<int>>> feasibility =
            std::make_unique<IntersectionFeasibilityFunction<SetResource<int>, int>>(
                std::map<size_t, std::set<int>>{{1, values}},
                forbidden);
        graph->add_resource<SetResource<int>>(
            std::move(memory),
            std::move(feasibility),
            std::make_unique<TrivialCostFunction<SetResource<int>>>(),
            std::make_unique<InclusionDominanceFunction<SetResource<int>>>());

        graph->add_node(0, /*source=*/true, /*sink=*/false);
        graph->add_node(1, /*source=*/false, /*sink=*/true);
        graph->add_arc<RealResource, SetResource<int>>(
            std::make_tuple(std::make_tuple(1.0), std::make_tuple(std::set<int>{0})),
            0,
            1,
            1.0);
        return graph;
    };

    using Composed = ResourceTypeComposition<RealResource, SetResource<int>>;
    AlgorithmParams<LabelList<Composed>> params;
    params.critical_resource_index = 0;
    params.half_way_point = 1.0;

    // Required values: a prefix question, so the model is refused and the component is named.
    auto required = build(/*forbidden=*/false, /*values=*/{0});
    auto refused = required->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    try {
        required->solve(refused.get());
        FAIL() << "a required-values component asks a prefix question and must be refused";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("component 1"), std::string::npos) << message;
        EXPECT_NE(message.find("merge_rule"), std::string::npos) << message;
    }

    // Forbidden values still solve when they are the ng-route condition (node 1 forbids itself)
    // carried by an endpoint mirror. Forbidding another node, or using a Union memory, would be
    // refused.
    auto forbidding = build(/*forbidden=*/true, /*values=*/{1}, /*node_mirror=*/true);
    auto solving = forbidding->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    EXPECT_NO_THROW({ forbidding->solve(solving.get()); });

    // The forward search is unaffected in both cases; the refusal is bidirectional-only.
    EXPECT_NO_THROW({
        auto forward_only = build(/*forbidden=*/false, /*values=*/{0});
        forward_only->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    });
}

/// @brief A forbidden set that is not the node's own is refused, on the two models that used to
///        get a wrong answer from it.
///
/// `DisjointMergeForm` needs `forbidden(v) = {v}`. Each case is a minimal model that gave a wrong
/// bidirectional answer before the precondition was checked; both are now refused at setup and
/// still solve forward.
TEST(Bidirectional, AContainerForbiddingAnotherNodeIsRefused) {
    struct Arc {
            size_t origin;
            size_t destination;
            double cost;
    };

    // cost (component 0), a clock (component 1), and the memory (component 2). The clock keeps
    // the cyclic case finite.
    const auto build = [](size_t num_nodes,
                          size_t sink,
                          double horizon,
                          const std::map<size_t, std::set<int>>& forbidden,
                          const std::vector<Arc>& arcs) {
        auto graph = std::make_unique<ResourceGraph<RealResource, SetResource<int>>>();
        std::map<size_t, std::pair<double, double>> windows;
        for (size_t node = 0; node < num_nodes; ++node) {
            windows[node] = {0.0, horizon};
        }

        presets::add_cost_resource<RealResource>(*graph);
        presets::add_window_resource<RealResource>(*graph, windows);
        // A plain visited set never forgets, so only the forbidden-set precondition is under test.
        graph->add_resource<SetResource<int>>(
            std::make_unique<UnionExtensionFunction<SetResource<int>>>(),
            std::make_unique<IntersectionFeasibilityFunction<SetResource<int>, int>>(
                forbidden,
                /*forbidden=*/true),
            std::make_unique<TrivialCostFunction<SetResource<int>>>(),
            std::make_unique<InclusionDominanceFunction<SetResource<int>>>());

        for (size_t node = 0; node < num_nodes; ++node) {
            graph->add_node(node, /*source=*/node == 0, /*sink=*/node == sink);
        }
        for (const auto& arc : arcs) {
            graph->add_arc<RealResource, RealResource, SetResource<int>>(
                std::make_tuple(std::make_tuple(arc.cost),
                                std::make_tuple(10.0),
                                std::make_tuple(std::set<int>{static_cast<int>(arc.origin)})),
                arc.origin,
                arc.destination,
                arc.cost);
        }
        return graph;
    };

    using Composed = ResourceTypeComposition<RealResource, SetResource<int>>;
    const auto expect_refused = [](auto* graph, double half_way) {
        AlgorithmParams<LabelList<Composed>> params;
        params.critical_resource_index = 1;
        params.half_way_point = half_way;
        auto algorithm =
            graph->template create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
        try {
            graph->solve(algorithm.get());
            FAIL() << "a forbidden set that is not the node's own has no backward reading and "
                      "must be refused";
        } catch (const std::runtime_error& error) {
            const std::string message = error.what();
            EXPECT_NE(message.find("component 2"), std::string::npos) << message;
            EXPECT_NE(message.find("merge_rule"), std::string::npos) << message;
        }
    };

    // ── Case 1: an infeasible splice ─────────────────────────────────────────────────────────────
    //
    //   0 ──▶ 1 ──▶ 2 ──▶ 3 ──▶ 4          forbidden(3) = {1}
    //    \          ▲
    //     \________/
    //
    // `0 1 2 3 4` is infeasible (arriving at 3 the memory holds 1); the optimum is `0 2 3 4` at
    // -2. The backward search never sees 1 before 3, so it would admit the infeasible path.
    {
        const std::vector<Arc> arcs{{0, 1, -10.0},
                                    {1, 2, -1.0},
                                    {0, 2, 0.0},
                                    {2, 3, -1.0},
                                    {3, 4, -1.0}};
        auto accepting = build(/*num_nodes=*/5, /*sink=*/4, /*horizon=*/100.0, {{3, {1}}}, arcs);
        expect_refused(accepting.get(), /*half_way=*/0.0);

        auto bounded = build(/*num_nodes=*/5, /*sink=*/4, /*horizon=*/100.0, {{3, {1}}}, arcs);
        expect_refused(bounded.get(), /*half_way=*/15.0);

        // Forward-only is unchanged, and returns what the model actually means.
        auto forward = build(/*num_nodes=*/5, /*sink=*/4, /*horizon=*/100.0, {{3, {1}}}, arcs);
        const auto result = forward->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
        ASSERT_FALSE(result.solutions.empty());
        EXPECT_NEAR(result.solutions.front().cost, -2.0, 1e-9);
        EXPECT_EQ(result.solutions.front().path_node_ids, (std::vector<size_t>{0, 2, 3, 4}));
    }

    // ── Case 2: over-strict splices ──────────────────────────────────────────────────────────────
    //
    //   0 ──▶ 1 ⇄ 2                        forbidden(2) = {9}, which is never collected
    //    \    │
    //     \   ▼
    //      ──▶ 3
    //
    // Nothing is actually forbidden, so the optimum loops 1 -> 2 -> 1 for -24 until the horizon
    // stops it; disjointness would refuse splices that share a node.
    {
        const std::vector<Arc> arcs{{0, 1, -1.0},
                                    {1, 2, -1.0},
                                    {2, 1, -10.0},
                                    {1, 3, -1.0},
                                    {0, 3, 0.0}};
        auto over_strict = build(/*num_nodes=*/4, /*sink=*/3, /*horizon=*/60.0, {{2, {9}}}, arcs);
        expect_refused(over_strict.get(), /*half_way=*/25.0);

        auto forward = build(/*num_nodes=*/4, /*sink=*/3, /*horizon=*/60.0, {{2, {9}}}, arcs);
        const auto result = forward->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
        ASSERT_FALSE(result.solutions.empty());
        EXPECT_NEAR(result.solutions.front().cost, -24.0, 1e-9);
    }

    // ── The control: the ng-route condition still solves ─────────────────────────────────────────
    //
    // Shows the refusals above are about the forbidden set, not about containers. The memory is
    // `NgPathExtensionFunction`, whose endpoint mirror gives `forbidden(v) = {v}` a backward
    // reading. Full neighborhoods make this elementary, with optimum `0 1 2 3 4`.
    {
        auto graph = std::make_unique<ResourceGraph<RealResource, SetResource<int>>>();
        std::map<size_t, std::pair<double, double>> windows;
        std::map<size_t, std::set<int>> neighborhoods;
        for (int node = 0; node < 5; ++node) {
            windows[static_cast<size_t>(node)] = {0.0, 100.0};
            neighborhoods[static_cast<size_t>(node)] = {0, 1, 2, 3, 4};
        }
        presets::add_cost_resource<RealResource>(*graph);
        presets::add_window_resource<RealResource>(*graph, windows);
        presets::add_ng_path_resource<SetResource<int>>(*graph, neighborhoods);

        for (size_t node = 0; node < 5; ++node) {
            graph->add_node(node, /*source=*/node == 0, /*sink=*/node == 4);
        }
        const std::vector<Arc> arcs{{0, 1, -10.0},
                                    {1, 2, -1.0},
                                    {0, 2, 0.0},
                                    {2, 3, -1.0},
                                    {3, 4, -1.0}};
        for (const auto& arc : arcs) {
            graph->add_arc<RealResource, RealResource, SetResource<int>>(
                std::make_tuple(std::make_tuple(arc.cost),
                                std::make_tuple(10.0),
                                std::make_tuple(std::set<int>{})),
                arc.origin,
                arc.destination,
                arc.cost);
        }

        AlgorithmParams<LabelList<Composed>> params;
        params.critical_resource_index = 1;
        params.half_way_point = 15.0;
        auto algorithm =
            graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
        SolveResult result;
        EXPECT_NO_THROW({ result = graph->solve(algorithm.get()); });
        ASSERT_FALSE(result.solutions.empty());
        EXPECT_NEAR(result.solutions.front().cost, -13.0, 1e-9);
    }
}

/// @brief The elementary-path model written as a visited set is refused, and the preset that
///        replaces it solves.
///
/// `UnionExtensionFunction` over arcs carrying `{origin}` with each node forbidding itself is
/// correct forward but not backward: the backward memory at `v` already contains `v`, so every
/// backward label is rejected on creation. With the half-way bound on, that silently returns an
/// empty result. Each half is legal alone; only a check that sees both catches it.
TEST(Bidirectional, AVisitedSetCannotCarryTheElementaryCondition) {
    struct Arc {
            size_t origin;
            size_t destination;
            double cost;
    };
    const std::vector<Arc> arcs{{0, 1, -10.0},
                                {1, 2, -1.0},
                                {0, 2, 0.0},
                                {2, 3, -1.0},
                                {3, 4, -1.0}};
    const std::vector<size_t> node_ids{0, 1, 2, 3, 4};

    enum class Memory { VisitedSet, Elementary };
    const auto build = [&](Memory memory) {
        auto graph = std::make_unique<ResourceGraph<RealResource, SetResource<int>>>();
        std::map<size_t, std::pair<double, double>> windows;
        std::map<size_t, std::set<int>> self_forbidden;
        for (int node = 0; node < 5; ++node) {
            windows[static_cast<size_t>(node)] = {0.0, 100.0};
            self_forbidden[static_cast<size_t>(node)] = {node};
        }
        presets::add_cost_resource<RealResource>(*graph);
        presets::add_window_resource<RealResource>(*graph, windows);

        if (memory == Memory::Elementary) {
            presets::add_elementary_resource<SetResource<int>>(*graph, node_ids);
        } else {
            graph->add_resource<SetResource<int>>(
                std::make_unique<UnionExtensionFunction<SetResource<int>>>(),
                std::make_unique<IntersectionFeasibilityFunction<SetResource<int>, int>>(
                    self_forbidden,
                    /*forbidden=*/true),
                std::make_unique<TrivialCostFunction<SetResource<int>>>(),
                std::make_unique<InclusionDominanceFunction<SetResource<int>>>());
        }

        for (size_t node : node_ids) {
            graph->add_node(node, /*source=*/node == 0, /*sink=*/node == 4);
        }
        for (const auto& arc : arcs) {
            graph->add_arc<RealResource, RealResource, SetResource<int>>(
                std::make_tuple(std::make_tuple(arc.cost),
                                std::make_tuple(10.0),
                                // What a visited-set model puts here; NgPathExtensionFunction
                                // ignores it.
                                std::make_tuple(std::set<int>{static_cast<int>(arc.origin)})),
                arc.origin,
                arc.destination,
                arc.cost);
        }
        return graph;
    };

    using Composed = ResourceTypeComposition<RealResource, SetResource<int>>;
    AlgorithmParams<LabelList<Composed>> params;
    params.critical_resource_index = 1;
    params.half_way_point = 15.0;
    params.release_after_solve = false;  // so the backward containers stay readable

    // The visited set is refused, and the message names the declaration to change.
    {
        auto graph = build(Memory::VisitedSet);
        auto algorithm =
            graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
        try {
            graph->solve(algorithm.get());
            FAIL() << "a visited set cannot carry the elementary condition backwards";
        } catch (const std::runtime_error& error) {
            const std::string message = error.what();
            EXPECT_NE(message.find("component 2"), std::string::npos) << message;
            EXPECT_NE(message.find("EndpointMirror"), std::string::npos) << message;
            // The remedy is named, not only the missing declaration.
            EXPECT_NE(message.find("EndpointMirrorForm"), std::string::npos) << message;
        }
    }

    // The preset solves the same model, and its backward search actually runs.
    {
        auto graph = build(Memory::Elementary);
        auto algorithm =
            graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
        SolveResult result;
        EXPECT_NO_THROW({ result = graph->solve(algorithm.get()); });
        ASSERT_FALSE(result.solutions.empty());
        EXPECT_NEAR(result.solutions.front().cost, -13.0, 1e-9);
        EXPECT_TRUE(result.bounded_by_half_way);

        size_t backward_labels = 0;
        for (const auto& container : algorithm->get_backward_labels_by_node_pos()) {
            backward_labels += container.get_labels().size();
        }
        // Only the seed would mean every backward extension was rejected.
        EXPECT_GT(backward_labels, 1U)
            << "the backward search kept only its seed, which is what a memory that includes the "
               "node it sits on does";
    }

    // Forward-only is unaffected: the visited set is a valid forward model.
    for (const Memory memory : {Memory::VisitedSet, Memory::Elementary}) {
        auto graph = build(memory);
        const auto result = graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
        ASSERT_FALSE(result.solutions.empty());
        EXPECT_NEAR(result.solutions.front().cost, -13.0, 1e-9);
    }
}

// ============================================================================
// The half-way bound's failure mode
// ============================================================================

/// @brief `half_way_point = 0` turns the bound OFF -- it does not derive one.
///
/// A control run with a real H shows the clock is valid, so the bound is off only because of the
/// zero. Other tests rely on 0 to request unbounded runs.
TEST(Bidirectional, AZeroHalfWayPointTurnsTheBoundOff) {
    namespace bt = bidirectional_test;

    const auto arcs = std::vector<std::pair<double, double>>{{1.0, 5.0}, {2.0, 2.0}, {3.0, 3.0}};

    // Control: the same clock with a real H engages the bound.
    auto bounded_graph = bt::clock_line_graph(arcs, /*capacity=*/20.0);
    auto bounded = bounded_graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bt::params(5.0, bt::kClockIndex));
    const auto bounded_result = bounded_graph->solve(bounded.get());
    ASSERT_TRUE(bounded->bounded_by_half_way())
        << "this clock must be usable, or the zero below proves nothing";

    // The same model and the same clock, with H = 0.
    auto graph = bt::clock_line_graph(arcs, /*capacity=*/20.0);
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bt::params(0.0, bt::kClockIndex));
    const auto result = graph->solve(algorithm.get());

    EXPECT_FALSE(algorithm->bounded_by_half_way())
        << "half_way_point = 0 must disable the bound, not derive one";
    EXPECT_FALSE(result.bounded_by_half_way) << "and the result the caller receives must say so";

    // An unbounded run is still exact.
    auto reference_graph = bt::clock_line_graph(arcs, /*capacity=*/20.0);
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions.front().cost,
                bt::forward_optimum(reference_graph.get()),
                bt::kTolerance);
    EXPECT_NEAR(result.solutions.front().cost,
                bounded_result.solutions.front().cost,
                bt::kTolerance);
}

/// @brief A non-monotone clock disables the bound, does not throw, and still finds the optimum.
TEST(Bidirectional, NonMonotoneClockDisablesTheBoundAndStillSolves) {
    namespace bt = bidirectional_test;

    // The first arc lifts the clock to 5 so the path stays feasible within [0, 20].
    auto monotone = bt::clock_line_graph({{1.0, 5.0}, {2.0, 2.0}, {3.0, 3.0}}, /*capacity=*/20.0);

    // The middle arc's -2 makes the clock non-monotone. A time window waits, so its clock may go
    // down on an arc; a budget may not, since a backward label cannot see it dip below 0.
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

    auto monotone_algorithm =
        monotone->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
            bt::params(3.0, bt::kClockIndex));
    const auto monotone_result = monotone->solve(monotone_algorithm.get());
    ASSERT_FALSE(monotone_result.solutions.empty());
    EXPECT_TRUE(monotone_algorithm->bounded_by_half_way());

    // Preprocessing off: it extends from default values, so the -2 would put node 2 below the
    // clock's floor of 0 and remove its arcs.
    auto algorithm = non_monotone->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bt::params(3.0, bt::kClockIndex));
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
/// Mostly a compile test: instantiating with an absent type must not be a hard error.
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
/// The upper route's first arc costs 30, above the incumbent of 10, yet completes to the optimum
/// of -20.
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
/// `arc.cost` keeps the original weight while labels carry reduced costs. A bound computed on
/// `arc.cost` would over-estimate here and prune the -1002 optimum against the -2 direct arc.
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
    p.heuristic_cost_index = 0;  // the slot the labels accumulate
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

/// @brief The backward search reaches the source and leaves a terminal label there.
TEST(Bidirectional, BackwardSearchReachesTheSource) {
    namespace bt = bidirectional_test;
    auto graph = bt::clock_line_graph({{1.0, 1.0}, {1.0, 1.0}}, /*capacity=*/100.0);

    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bt::params(50.0, bt::kClockIndex));
    graph->solve(algorithm.get());

    // The backward clock starts at 100 and stays above H = 50, so it runs back to the source.
    const auto* source = graph->get_node(0);
    const auto& at_source =
        algorithm->get_backward_labels_by_node_pos().at(source->pos()).get_labels();
    EXPECT_FALSE(at_source.empty())
        << "the backward search should reach the source and contribute a terminal label";
}

/// @brief Memory pressure trims both frontiers without corrupting the pool.
///
/// `memory_pressure_fraction = 0` with a huge limit reports pressure on every check without
/// stopping the solve. Trimming must leave reference counts intact (no leak, no double free).
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

    // A generous cap: pressure is reported but nothing is trimmed.
    auto roomy = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        under_pressure(/*max_labels_per_node=*/1000));
    SolveResult roomy_result;
    EXPECT_NO_THROW({ roomy_result = graph->solve(roomy.get()); });
    ASSERT_FALSE(roomy_result.solutions.empty())
        << "reporting pressure without exceeding the cap must not change the answer";
    EXPECT_TRUE(roomy->get_label_pool().check_ref_count_consistency());

    // A cap of zero drops every frontier entry on the first check.
    auto squeezed = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        under_pressure(/*max_labels_per_node=*/0));
    EXPECT_NO_THROW({ graph->solve(squeezed.get()); });
    EXPECT_TRUE(squeezed->get_label_pool().check_ref_count_consistency())
        << "trimming a frontier must not leak or double-release a label";
}

/// @brief Memory pressure tightens the per-node extension quota, not just the frontier.
///
/// Without the quota the frontiers would refill, so the check is that fewer labels are extended.
TEST(Bidirectional, MemoryPressureTightensThePerNodeQuota) {
    namespace bt = bidirectional_test;

    // Node 3 gets two non-dominated labels (cheap-slow via 1, dear-quick via 2), so a quota of 1
    // has something to refuse.
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
        // H = 20 exceeds the slow route's clock of 11, so both labels at node 3 survive the bound.
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

/// @brief Memory-pressure lossiness reaches `SolveResult`, not just the algorithm object.
///
/// A trimmed run may be non-optimal yet still reports COMPLETE, so the result flag is the only
/// signal a caller without the algorithm object gets.
TEST(Bidirectional, MemoryPressureReachesTheSolveResult) {
    namespace bt = bidirectional_test;

    const auto run = [](bool under_pressure) {
        auto graph = bt::clock_line_graph({{1.0, 1.0}, {2.0, 1.0}, {3.0, 1.0}}, /*capacity=*/20.0);
        auto p = bt::params(3.0, bt::kClockIndex);
        if (under_pressure) {
            constexpr double kHugeLimitGiB = 1e9;  // pressure on every check, never a hard stop
            p.max_memory_gb = kHugeLimitGiB;
            p.memory_pressure_fraction = 0.0;
            p.memory_check_interval = 1;
            p.memory_pressure_max_labels_per_node = 0;
        }
        auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(p);
        const SolveResult result = graph->solve(algorithm.get());
        return std::make_pair(result, algorithm->memory_pressure_was_triggered());
    };

    const auto [relaxed, relaxed_flag] = run(/*under_pressure=*/false);
    EXPECT_FALSE(relaxed_flag);
    EXPECT_FALSE(relaxed.memory_pressure_triggered)
        << "an ordinary solve must not claim it was trimmed";

    const auto [pressed, pressed_flag] = run(/*under_pressure=*/true);
    ASSERT_TRUE(pressed_flag) << "the pressure recipe did not fire";
    EXPECT_TRUE(pressed.memory_pressure_triggered)
        << "the algorithm knows the run was trimmed but the result the caller receives does not";
    EXPECT_EQ(pressed.status, AlgorithmStatus::COMPLETE)
        << "the status cannot express this, which is exactly why the flag has to";
}

/// @brief Every algorithm reports it, not only the bidirectional one.
///
/// The flag is set in the base `Algorithm::annotate`, so forward algorithms report it too.
TEST(Bidirectional, MemoryPressureIsReportedByForwardAlgorithmsToo) {
    namespace bt = bidirectional_test;
    auto graph = bt::clock_line_graph({{1.0, 1.0}, {2.0, 1.0}, {3.0, 1.0}}, /*capacity=*/20.0);

    constexpr double kHugeLimitGiB = 1e9;
    AlgorithmBaseParams pressed;
    pressed.max_memory_gb = kHugeLimitGiB;
    pressed.memory_pressure_fraction = 0.0;
    pressed.memory_check_interval = 1;
    pressed.memory_pressure_max_labels_per_node = 0;

    EXPECT_TRUE(graph->solve<SimpleDominanceAlgorithm>(pressed).memory_pressure_triggered);
    EXPECT_FALSE(
        graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}).memory_pressure_triggered);
}

/// @brief The default `release_after_solve` frees the backward containers too.
///
/// Other tests disable the release to inspect labels, so this covers the default path.
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
/// Setup validation reads declarations off the first arc; with no arcs it must not report every
/// component as undeclared.
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
/// A binding `upper_bound` can remove every arc (e.g. at column-generation convergence); like the
/// forward search, the result must simply be empty.
TEST(Bidirectional, PreprocessingAwayEveryArcIsNotAModellingError) {
    namespace bt = bidirectional_test;
    constexpr double kBindingUpperBound = 0.0;  // every arc cost below is positive

    // Reference: the forward search returns nothing.
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
/// A backward label must not pass through a sink; the forward search stops there, so paths must
/// match.
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

/// @brief A forward path does not re-enter a source either.
///
/// A walk that returns to the depot and leaves again is two routes, not one column.
TEST(Bidirectional, NoReturnedPathReEntersASource) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource>(std::make_tuple(1.0), 0, 1, 1.0);
    graph->add_arc<RealResource>(std::make_tuple(-10.0), 1, 0, -10.0);  // back into the source
    graph->add_arc<RealResource>(std::make_tuple(1.0), 1, 2, 1.0);

    // The rule applies to the forward search too.
    const auto forward =
        graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{},
                                               std::numeric_limits<double>::infinity(),
                                               /*preprocess=*/false);
    for (const auto& solution : forward.solutions) {
        const auto& nodes = solution.path_node_ids;
        for (size_t i = 1; i + 1 < nodes.size(); ++i) {
            EXPECT_FALSE(graph->get_node(nodes[i])->source)
                << "source " << nodes[i] << " appears inside a returned path";
        }
    }
}

// ============================================================================
// Parameters that are shared with the forward algorithms
// ============================================================================

/// @brief `return_dominated_solutions` records terminal paths as they are found, in both
///        directions.
///
/// Otherwise a terminal path dominated later would be gone before the end-of-solve sweep.
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

/// @brief Under a finite upper bound with pruning off, a join that ties the incumbent survives.
///
/// With no finite bound the incumbent cutoff stays on regardless of the flag. The two routes meet
/// at different nodes (so neither is dominated before the join) and neither search reaches the
/// other's terminal, so every solution comes from the join.
TEST(Bidirectional, TheJoinDoesNotPruneAgainstTheIncumbentUnlessAsked) {
    namespace bt = bidirectional_test;

    // 0 -> 1 -> 3 -> 5 and 0 -> 2 -> 4 -> 5, both costing 5, so the second exactly ties the
    // incumbent. Every arc consumes 6 of a clock capped at 18, with H = 8:
    //
    //   forward   0 (0)  1 (6)  2 (6)  extend;  3 (12)  4 (12) do not -- so 5 is never reached
    //   backward  5 (18) 3 (12) 4 (12) extend;  1 (6)   2 (6)  do not -- so 0 is never reached
    //
    // The halves meet at 3 and at 4, one route each.
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
                                              18.0,
                                              /*merge_by_increasing_value=*/true),
                                          std::make_unique<TrivialCostFunction<RealResource>>(),
                                          std::make_unique<ValueDominanceFunction<RealResource>>());
        graph->add_node(0, /*source=*/true, /*sink=*/false);
        graph->add_node(1);
        graph->add_node(2);
        graph->add_node(3);
        graph->add_node(4);
        graph->add_node(5, /*source=*/false, /*sink=*/true);
        graph->add_arc<RealResource, RealResource>({2.0, 6.0}, 0, 1, 2.0);
        graph->add_arc<RealResource, RealResource>({3.0, 6.0}, 0, 2, 3.0);
        graph->add_arc<RealResource, RealResource>({2.0, 6.0}, 1, 3, 2.0);
        graph->add_arc<RealResource, RealResource>({1.0, 6.0}, 2, 4, 1.0);
        graph->add_arc<RealResource, RealResource>({1.0, 6.0}, 3, 5, 1.0);
        graph->add_arc<RealResource, RealResource>({1.0, 6.0}, 4, 5, 1.0);
        return graph;
    };

    const auto run = [&build](bool prune, double upper_bound) {
        auto graph = build();
        auto p = bt::params(8.0, bt::kClockIndex);
        p.prune_based_on_upper_bound_ = prune;
        auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(p);
        const auto result = graph->solve(algorithm.get(), upper_bound);
        return std::make_pair(result.solutions.size(), algorithm->number_of_joined_paths());
    };

    constexpr double kFiniteBound = 10.0;  // above both routes, so it admits them and filters none
    constexpr double kNoBound = std::numeric_limits<double>::infinity();

    // The pricing case: a finite bound, pruning off. Both equal-cost routes come back.
    const auto [open_count, open_joins] = run(/*prune=*/false, kFiniteBound);
    EXPECT_GT(open_joins, 0U)
        << "the two routes must meet at the join, or this test does not exercise the cutoff";
    EXPECT_GE(open_count, 2U)
        << "both equal-cost routes should be returned under a finite bound with pruning off";

    // The same bound with pruning on: the incumbent cutoff bites.
    const auto [pruned_count, pruned_joins] = run(/*prune=*/true, kFiniteBound);
    EXPECT_LT(pruned_count, open_count);
    EXPECT_LE(pruned_joins, open_joins);

    // With no bound the cutoff applies whatever the flag says.
    const auto [unbounded_count, unbounded_joins] = run(/*prune=*/false, kNoBound);
    EXPECT_EQ(unbounded_count, pruned_count)
        << "an infinite upper bound must behave as though pruning had been requested";
    EXPECT_EQ(unbounded_joins, pruned_joins);
}

// ============================================================================
// Diagnostics on the result
// ============================================================================

/// @brief The result carries the diagnostics, not just the algorithm object.
///
/// Python callers never hold the algorithm, so diagnostics must be on `SolveResult` and match the
/// accessors.
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
/// Sources are a forward label at a sink, a backward label at a source, and a joined pair; this
/// case targets the backward label reaching the source.
TEST(Bidirectional, NoSolutionExceedsTheCallerUpperBound) {
    namespace bt = bidirectional_test;
    constexpr double kUpperBound = 0.0;  // "only strictly negative reduced costs, please"

    // Reference: every path costs +5, so the forward search returns nothing.
    auto forward_graph = bt::line_graph({2.0, 3.0});
    const auto forward =
        forward_graph->solve<SimpleDominanceAlgorithm>(kUpperBound,
                                                       AlgorithmParams<LabelList<bt::Composed>>{},
                                                       /*preprocess=*/false);
    ASSERT_TRUE(forward.solutions.empty());

    auto graph = bt::line_graph({2.0, 3.0});
    auto algorithm =
        graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(bt::params(1.0));
    // Preprocessing off, or it would remove every arc and the case would never arise.
    const auto result = graph->solve(algorithm.get(), kUpperBound, /*preprocess=*/false);

    EXPECT_TRUE(result.solutions.empty())
        << "a backward label reaching the source was recorded without the upper-bound filter";
    for (const auto& solution : result.solutions) {
        EXPECT_LT(solution.cost, kUpperBound);
    }
}

// ============================================================================
// Per-node caps, relaxed dominance, negative cycles, bound-off reason
// ============================================================================

namespace bidirectional_test {

/// @brief A cost slot plus a budget clock whose per-node caps live wherever the caller says.
///
/// @param extension_caps   Per-node caps handed to `BudgetExtensionFunction`.
/// @param feasibility_caps Per-node `{min, max}` windows handed to `MinMaxFeasibilityFunction`.
inline std::unique_ptr<ResourceGraph<RealResource>> budget_graph(
    std::map<size_t, double> extension_caps,
    std::map<size_t, std::pair<double, double>> feasibility_caps) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<BudgetExtensionFunction<RealResource>>(std::move(extension_caps)),
        std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0,
                                                                  100.0,
                                                                  std::move(feasibility_caps)),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    return graph;
}

/// @brief Solves @p graph bidirectionally with the budget in slot 1 as the clock.
inline SolveResult solve_on_the_budget_clock(ResourceGraph<RealResource>* graph,
                                             double half_way_point) {
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        params(half_way_point, kClockIndex));
    return graph->solve(algorithm.get());
}

}  // namespace bidirectional_test

/// @brief A per-node cap given to the feasibility function alone binds in both directions.
///
/// The backward ceiling at node 1 must be clamped to the cap, or the only path is lost.
TEST(Bidirectional, APerNodeCapOnTheFeasibilityFunctionAloneBindsBackward) {
    namespace bt = bidirectional_test;
    const auto build = [] {
        auto graph = bt::budget_graph({}, {{1, {0.0, 50.0}}});
        graph->add_node(0, /*source=*/true, /*sink=*/false);
        graph->add_node(1);
        graph->add_node(2, /*source=*/false, /*sink=*/true);
        graph->add_arc<RealResource, RealResource>({-1.0, 10.0}, 0, 1, -1.0);
        graph->add_arc<RealResource, RealResource>({-1.0, 10.0}, 1, 2, -1.0);
        return graph;
    };

    EXPECT_NEAR(bt::forward_optimum(build().get()), -2.0, bt::kTolerance);

    auto graph = build();
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bt::params(5.0, bt::kClockIndex));
    const auto result = graph->solve(algorithm.get());
    ASSERT_TRUE(algorithm->bounded_by_half_way());
    ASSERT_FALSE(result.solutions.empty())
        << "the backward ceiling at node 1 was not clamped to that node's cap";
    EXPECT_NEAR(result.solutions.front().cost, -2.0, bt::kTolerance);
}

/// @brief A cap on the extension alone cannot make the two searches solve different models.
///
/// The feasibility function alone defines the caps, so node 1 is uncapped and every H agrees with
/// the forward optimum.
TEST(Bidirectional, ACapOnTheExtensionAloneCannotMakeTheTwoSearchesDisagree) {
    namespace bt = bidirectional_test;
    const auto build = [] {
        auto graph = bt::budget_graph({{1, 5.0}}, {});
        graph->add_node(0, /*source=*/true, /*sink=*/false);
        graph->add_node(1);
        graph->add_node(3);
        graph->add_node(2, /*source=*/false, /*sink=*/true);
        graph->add_arc<RealResource, RealResource>({-5.0, 10.0}, 0, 1, -5.0);
        graph->add_arc<RealResource, RealResource>({-5.0, 10.0}, 1, 2, -5.0);
        graph->add_arc<RealResource, RealResource>({-0.5, 1.0}, 0, 3, -0.5);
        graph->add_arc<RealResource, RealResource>({-0.5, 1.0}, 3, 2, -0.5);
        return graph;
    };

    const double forward = bt::forward_optimum(build().get());
    EXPECT_NEAR(forward, -10.0, bt::kTolerance);
    for (const double half_way_point : {0.0, 5.0, 15.0}) {
        auto graph = build();
        const auto result = bt::solve_on_the_budget_clock(graph.get(), half_way_point);
        ASSERT_FALSE(result.solutions.empty()) << "H = " << half_way_point;
        EXPECT_NEAR(result.solutions.front().cost, forward, bt::kTolerance)
            << "H = " << half_way_point;
    }
}

/// @brief A relaxed dominance on the time window cannot let an arrival past the deadline join.
///
/// The join's feasibility check must not rely on the (trivial) dominance function; otherwise it
/// accepts 0 -> 1 -> 3 -> 4, arriving at 11 against a deadline of 6.
TEST(Bidirectional, ARelaxedDominanceCannotLetAnInfeasibleSpliceJoin) {
    namespace bt = bidirectional_test;
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}},
                                                        {1, {0.0, 100.0}},
                                                        {2, {0.0, 100.0}},
                                                        {3, {0.0, 100.0}},
                                                        {4, {0.0, 6.0}}};

    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<TrivialDominanceFunction<RealResource>>());
    // The clock is a budget in slot 2, so the relaxed window is not the one the bound reads.
    graph->add_resource<RealResource>(std::make_unique<BudgetExtensionFunction<RealResource>>(),
                                      std::make_unique<MinMaxFeasibilityFunction<RealResource>>(
                                          0.0,
                                          20.0,
                                          /*merge_by_increasing_value=*/true),
                                      std::make_unique<TrivialCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    for (size_t node_id = 0; node_id < 5; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id == 4);
    }
    // {cost, time, budget}. Arc 0 takes 10 time units, so any path through it is late.
    graph->add_arc<RealResource, RealResource, RealResource>({-4.0, 10.0, 1.0}, 0, 1, -4.0);
    graph->add_arc<RealResource, RealResource, RealResource>({0.0, 1.0, 1.0}, 0, 2, 0.0);
    graph->add_arc<RealResource, RealResource, RealResource>({-1.0, 0.0, 5.0}, 1, 3, -1.0);
    graph->add_arc<RealResource, RealResource, RealResource>({-1.0, 0.0, 5.0}, 2, 3, -1.0);
    graph->add_arc<RealResource, RealResource, RealResource>({-1.0, 1.0, 1.0}, 3, 4, -1.0);

    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bt::params(3.0, /*critical_resource_index=*/2));
    const auto result = graph->solve(algorithm.get());
    for (const auto& solution : result.solutions) {
        EXPECT_EQ(std::ranges::find(solution.path_arc_ids, size_t{0}), solution.path_arc_ids.end())
            << "a path through arc 0 reaches the sink at 11, past its deadline of 6";
        EXPECT_GE(solution.cost, -2.0 - bt::kTolerance);
    }
}

/// @brief A negative-cost cycle turns the completion prune off rather than zeroing the bound.
///
/// A zero bound would prune on a half's own cost, which is inadmissible. The 1 <-> 2 cycle is
/// worth -6 a lap and the optimum takes two laps.
TEST(Bidirectional, ANegativeCycleTurnsTheCompletionPruneOffRatherThanZeroingIt) {
    namespace bt = bidirectional_test;
    const auto build = [] {
        std::map<size_t, std::pair<double, double>> windows;
        for (size_t node_id = 0; node_id < 4; ++node_id) {
            windows[node_id] = {0.0, 7.0};
        }
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
        for (size_t node_id = 0; node_id < 4; ++node_id) {
            graph->add_node(node_id, node_id == 0, node_id == 3);
        }
        graph->add_arc<RealResource, RealResource>({5.0, 1.0}, 0, 1, 5.0);
        graph->add_arc<RealResource, RealResource>({-3.0, 1.0}, 1, 2, -3.0);
        graph->add_arc<RealResource, RealResource>({-3.0, 1.0}, 2, 1, -3.0);
        graph->add_arc<RealResource, RealResource>({0.0, 1.0}, 1, 3, 0.0);
        graph->add_arc<RealResource, RealResource>({2.0, 1.0}, 0, 3, 2.0);
        return graph;
    };
    constexpr double kOptimum = -7.0;  // 0 1 2 1 2 1 3: 5 - 4 * 3 + 0, arriving at 6 <= 7

    // Preprocessing off throughout: this is about the search's own prune.
    const auto forward =
        build()->solve<SimpleDominanceAlgorithm>(std::numeric_limits<double>::infinity(),
                                                 AlgorithmParams<LabelList<bt::Composed>>{},
                                                 /*preprocess=*/false);
    ASSERT_FALSE(forward.solutions.empty());
    EXPECT_NEAR(forward.solutions.front().cost, kOptimum, bt::kTolerance);

    auto graph = build();
    auto params = bt::params(3.0, /*critical_resource_index=*/1);
    params.prune_based_on_upper_bound_ = true;
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    const auto result = graph->solve(algorithm.get(),
                                     std::numeric_limits<double>::infinity(),
                                     /*preprocess=*/false);
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions.front().cost, kOptimum, bt::kTolerance)
        << "the completion prune cut the optimum on a half's own cost";
}

/// @brief A solve that runs with the bound off says why.
///
/// Checked through `half_way_off_reason()`; the one-time WARN log is not observable reliably.
TEST(Bidirectional, TheReasonTheBoundIsOffIsReported) {
    namespace bt = bidirectional_test;

    auto unbounded_graph = bt::clock_line_graph({{1.0, 1.0}, {2.0, 1.0}}, /*capacity=*/20.0);
    auto unbounded = unbounded_graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bt::params(0.0, bt::kClockIndex));
    static_cast<void>(unbounded_graph->solve(unbounded.get()));
    EXPECT_FALSE(unbounded->bounded_by_half_way());
    EXPECT_NE(unbounded->half_way_off_reason().find("no half_way_point"), std::string::npos)
        << unbounded->half_way_off_reason();

    auto bounded_graph = bt::clock_line_graph({{1.0, 1.0}, {2.0, 1.0}}, /*capacity=*/20.0);
    auto bounded = bounded_graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bt::params(1.5, bt::kClockIndex));
    static_cast<void>(bounded_graph->solve(bounded.get()));
    EXPECT_TRUE(bounded->bounded_by_half_way());
    EXPECT_TRUE(bounded->half_way_off_reason().empty()) << bounded->half_way_off_reason();

    // The cost is an accumulation, not a clock: an explicit H does not rescue it.
    auto cost_graph = bt::line_graph({1.0, 2.0});
    auto cost_clock =
        cost_graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(bt::params(1.5));
    static_cast<void>(cost_graph->solve(cost_clock.get()));
    EXPECT_FALSE(cost_clock->bounded_by_half_way());
    EXPECT_NE(cost_clock->half_way_off_reason().find("threshold"), std::string::npos)
        << cost_clock->half_way_off_reason();
}

// ============================================================================
// An int clock beside a real cost
// ============================================================================

static_assert(std::is_same_v<BidirectionalAlgoBound<IntResource>,
                             BidirectionalAlgoBound<IntResource, RealResource>>,
              "the cost type defaults to RealResource, not to the clock's type");

/// @brief The one-argument spelling for an int clock keeps the optimum with the completion-bound
///        prune on.
///
/// The cost type used to default to the clock's, so the completion bound relaxed on the int clock
/// slot: "cost + time-to-sink >= incumbent" pruned the optimum 0-1-2-3 (cost 3) and returned the
/// cost-10 arc, or nothing under an upper bound of 4.
TEST(Bidirectional, AnIntClockWithARealCostKeepsTheOptimum) {
    namespace bt = bidirectional_test;
    using Graph = ResourceGraph<RealResource, IntResource>;
    using Composition = ResourceTypeComposition<RealResource, IntResource>;
    const auto build = [] {
        std::map<size_t, std::pair<int, int>> windows;
        for (size_t node_id = 0; node_id < 4; ++node_id) {
            windows[node_id] = {0, 100};
        }
        auto graph = std::make_unique<Graph>();
        graph->add_resource<RealResource>(
            std::make_unique<AdditionExtensionFunction<RealResource>>(),
            std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
            std::make_unique<ValueCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
        graph->add_resource<IntResource>(
            std::make_unique<TimeWindowExtensionFunction<IntResource>>(windows),
            std::make_unique<TimeWindowFeasibilityFunction<IntResource>>(windows),
            std::make_unique<TrivialCostFunction<IntResource>>(),
            std::make_unique<ValueDominanceFunction<IntResource>>());
        for (size_t node_id = 0; node_id < 4; ++node_id) {
            graph->add_node(node_id, node_id == 0, node_id == 3);
        }
        const auto arc = [&](size_t origin, size_t destination, double cost, int time) {
            graph->add_arc<RealResource, IntResource>(
                std::make_tuple(std::make_tuple(cost), std::make_tuple(time)),
                origin,
                destination,
                cost);
        };
        arc(0, 3, 10.0, 1);
        arc(0, 1, 1.0, 10);
        arc(1, 2, 1.0, 10);
        arc(2, 3, 1.0, 10);
        return graph;
    };

    const auto forward = build()->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    ASSERT_FALSE(forward.solutions.empty());
    ASSERT_NEAR(forward.solutions.front().cost, 3.0, bt::kTolerance);

    for (const double upper_bound : {std::numeric_limits<double>::infinity(), 4.0}) {
        for (const double half_way_point : {0.0, 15.0}) {
            auto graph = build();
            AlgorithmParams<LabelList<Composition>> params;
            params.critical_resource_index = 0;
            params.half_way_point = half_way_point;
            params.prune_based_on_upper_bound_ = true;
            auto algorithm =
                graph->create_algorithm<BidirectionalAlgoBound<IntResource>::Algo>(params);
            const auto result = graph->solve(algorithm.get(), upper_bound);
            ASSERT_FALSE(result.solutions.empty())
                << "ub = " << upper_bound << ", H = " << half_way_point;
            EXPECT_NEAR(result.solutions.front().cost, 3.0, bt::kTolerance)
                << "ub = " << upper_bound << ", H = " << half_way_point;
        }
    }
}

// ============================================================================
// Memory pressure never loosens the caller's quota
// ============================================================================

namespace memory_pressure_quota_test {

/// @brief Six hops of two parallel arcs, one cheap and slow and one dear and quick, so every node
///        holds many non-dominated labels and a quota of 2 has plenty to refuse.
inline std::unique_ptr<ResourceGraph<RealResource>> fan() {
    constexpr size_t kHops = 6;
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<BudgetExtensionFunction<RealResource>>(),
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

/// @brief Labels extended under a per-node quota of 2, with or without memory pressure.
template <template <typename, typename> class Algorithm>
size_t extended_labels(bool under_pressure) {
    auto graph = fan();
    AlgorithmParams<LabelList<ResourceTypeComposition<RealResource>>> params;
    params.num_labels_to_extend_by_node = 2;
    params.critical_resource_index = 1;
    params.half_way_point = 6.0;
    if (under_pressure) {
        constexpr double kHugeLimitGiB = 1e9;  // pressure on every check, never a stop
        params.max_memory_gb = kHugeLimitGiB;
        params.memory_pressure_fraction = 0.0;
        params.memory_check_interval = 1;
        params.memory_pressure_max_labels_per_node = 200;
    }
    auto algorithm = graph->template create_algorithm<Algorithm>(params);
    const auto result = graph->solve(algorithm.get());
    EXPECT_EQ(result.memory_pressure_triggered, under_pressure);
    return algorithm->get_number_of_extended_labels();
}

}  // namespace memory_pressure_quota_test

/// @brief Memory pressure lowers the per-node quota to its own limit, never raises it.
///
/// It used to assign the pressure limit (200) unconditionally, so a caller's quota of 5 became 200
/// and the solve did up to 16 times more work under pressure than without it.
TEST(MemoryPressure, NeverLoosensTheBidirectionalQuota) {
    namespace mpq = memory_pressure_quota_test;
    // Simple and Pushing are covered in test_dominance_algorithms.hpp.
    EXPECT_EQ(mpq::extended_labels<BidirectionalAlgoBound<RealResource>::Algo>(true),
              mpq::extended_labels<BidirectionalAlgoBound<RealResource>::Algo>(false));
}

// ============================================================================
// Per-solve diagnostics
// ============================================================================

/// @brief The extended-label count describes the last solve, not every solve so far.
///
/// It used to accumulate: 4 then 8 for Simple, 7 then 14 for bidirectional, on the same object.
TEST(BidirectionalDiagnostics, ExtendedLabelCountIsPerSolve) {
    namespace bt = bidirectional_test;
    auto graph = bt::clock_line_graph({{1.0, 1.0}, {2.0, 1.0}, {3.0, 1.0}}, /*capacity=*/20.0);

    auto simple = graph->create_algorithm<SimpleDominanceAlgorithm>(
        AlgorithmParams<LabelList<ResourceTypeComposition<RealResource>>>{});
    static_cast<void>(graph->solve(simple.get()));
    const size_t first_simple = simple->get_number_of_extended_labels();
    static_cast<void>(graph->solve(simple.get()));
    EXPECT_GT(first_simple, 0U);
    EXPECT_EQ(simple->get_number_of_extended_labels(), first_simple);

    auto bidirectional = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bt::params(1.5, bt::kClockIndex));
    static_cast<void>(graph->solve(bidirectional.get()));
    const size_t first_bidirectional = bidirectional->get_number_of_extended_labels();
    static_cast<void>(graph->solve(bidirectional.get()));
    EXPECT_GT(first_bidirectional, 0U);
    EXPECT_EQ(bidirectional->get_number_of_extended_labels(), first_bidirectional);
}

/// @brief `number_of_joined_paths` counts the distinct paths only the join produced.
///
/// With the bound off every stored label is a boundary label, so the single path of a line was
/// joined at each interior node and counted 3 times, although the forward search had already
/// recorded it at the sink.
TEST(BidirectionalDiagnostics, JoinedPathsCountsDistinctPaths) {
    namespace bt = bidirectional_test;
    auto graph = bt::clock_line_graph({{1.0, 1.0}, {2.0, 1.0}, {3.0, 1.0}, {4.0, 1.0}},
                                      /*capacity=*/20.0);
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bt::params(0.0, bt::kClockIndex));
    // A finite bound, so the join does not prune against the incumbent and runs every pair.
    const auto result = graph->solve(algorithm.get(), 1e9);
    ASSERT_EQ(result.solutions.size(), 1U);
    EXPECT_EQ(result.number_of_joined_paths, 0U)
        << "the forward search recorded the only path at the sink before the join ran";
    EXPECT_LE(result.number_of_joined_paths, result.solutions.size());
}
