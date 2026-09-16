// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Phase 1 (foundations) of bidirectional labelling: the declarations, accessors and defaults
// added by the backward API. Nothing in the library calls these yet, so the tests here are the
// only exercise of the new branches.

#include <gtest/gtest.h>

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace {

// Named constants for the backward-API tests.
constexpr double kBackA = 1.0;
constexpr double kBackB = 2.0;
constexpr double kBackDelta = 5.0;
constexpr double kBackArcValue = 3.0;
constexpr double kBackLabelValue = 7.0;
constexpr double kBwdArc01 = -2.0;
constexpr double kBwdArc12 = -3.0;

/// @brief Builds a bare scalar Resource<RealResource> holding @p value.
inline std::unique_ptr<Resource<RealResource>> make_real_resource(double value) {
    return std::make_unique<Resource<RealResource>>(
        RealResource(value),
        std::make_unique<ValueDominanceFunction<RealResource>>(),
        std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<TrivialCostFunction<RealResource>>());
}

/// @brief Linear graph source(0) -> 1 -> sink(2), arc values doubling as costs.
inline std::unique_ptr<ResourceGraph<RealResource>> make_backward_api_graph() {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1, /*source=*/false, /*sink=*/false);
    graph->add_node(2, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource>(std::make_tuple(kBwdArc01), 0, 1);
    graph->add_arc<RealResource>(std::make_tuple(kBwdArc12), 1, 2);
    return graph;
}

/// @brief SimpleDominanceAlgorithm exposing the protected extract_solution overloads.
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>>
class SolutionProbeAlgorithm : public SimpleDominanceAlgorithm<ResourceType, LabelContainerType> {
        using Base = SimpleDominanceAlgorithm<ResourceType, LabelContainerType>;

    public:
        using Base::Base;

        /// @brief Number of distinct solutions currently recorded.
        [[nodiscard]] size_t solution_count() const { return this->solutions_.size(); }

        /// @brief Calls the path-taking extract_solution overload.
        void extract_path(double cost, std::vector<size_t> path_arc_ids, size_t end_node_id) {
            this->extract_solution(cost, std::move(path_arc_ids), end_node_id);
        }
};

}  // namespace

// ============================================================================
// Enum defaults
// ============================================================================

/// @brief backward_kind() and merge_rule() both default to Unspecified.
///
/// Asserted on a function that declares neither, not on a concrete one: the concrete extension
/// functions all declare a real kind (see BackwardKindDeclared), so this pins the *base-class
/// default* -- the thing phase 1 added, and what phase 11 keys off to refuse an undeclared
/// component.
TEST(BackwardApi, EnumsDefaultToUnspecified) {
    class UndeclaredExtensionFunction
        : public Clonable<UndeclaredExtensionFunction, ExtensionFunction<RealResource>> {
        public:
            void extend(const RealResource& /*resource*/, const RealResource& /*extender_value*/,
                        RealResource* /*extended_resource*/) override {}
    };
    EXPECT_EQ(UndeclaredExtensionFunction{}.backward_kind(), BackwardKind::Unspecified);

    class UndeclaredFeasibilityFunction
        : public Clonable<UndeclaredFeasibilityFunction, FeasibilityFunction<RealResource>> {
        public:
            [[nodiscard]] auto is_feasible(const RealResource& /*resource*/) -> bool override {
                return true;
            }
    };
    EXPECT_EQ(UndeclaredFeasibilityFunction{}.merge_rule(), MergeRule::Unspecified);

    CompositionFeasibilityFunction<RealResource> composition_feasibility;
    EXPECT_EQ(composition_feasibility.merge_rule(), MergeRule::Unspecified);
}

/// @brief back_seed_value() defaults to nullopt on the scalar specialisation.
TEST(BackwardApi, BackSeedDefaultsToNullopt) {
    TrivialFeasibilityFunction<RealResource> feasibility;
    EXPECT_FALSE(feasibility.back_seed_value().has_value());
}

// ============================================================================
// Backward dominance
// ============================================================================

/// @brief check_back_dominance / fast_check_back_dominance follow the reversal flag.
TEST(BackwardApi, BackDominanceFollowsTheFlag) {
    ValueDominanceFunction<RealResource> dominance;
    const RealResource lhs(kBackA);
    const RealResource rhs(kBackB);

    EXPECT_FALSE(dominance.is_backward_reversed());
    EXPECT_EQ(dominance.check_back_dominance(lhs, rhs), dominance.check_dominance(lhs, rhs));
    EXPECT_EQ(dominance.check_back_dominance(rhs, lhs), dominance.check_dominance(rhs, lhs));
    EXPECT_EQ(dominance.fast_check_back_dominance(lhs, rhs, kBackDelta),
              dominance.fast_check_dominance(lhs, rhs, kBackDelta));

    dominance.set_backward_reversed(true);
    EXPECT_TRUE(dominance.is_backward_reversed());
    EXPECT_EQ(dominance.check_back_dominance(lhs, rhs), dominance.check_dominance(rhs, lhs));
    EXPECT_EQ(dominance.check_back_dominance(rhs, lhs), dominance.check_dominance(lhs, rhs));
    EXPECT_EQ(dominance.fast_check_back_dominance(lhs, rhs, kBackDelta),
              dominance.fast_check_dominance(rhs, lhs, kBackDelta));

    // The two flag states must actually disagree, or the test proves nothing.
    dominance.set_backward_reversed(false);
    const bool forward_order = dominance.check_back_dominance(lhs, rhs);
    dominance.set_backward_reversed(true);
    EXPECT_NE(forward_order, dominance.check_back_dominance(lhs, rhs));
}

/// @brief Resource::back_dominates routes through the dominance function's backward check.
TEST(BackwardApi, ResourceBackDominatesRoutesToDominanceFunction) {
    auto lhs = make_real_resource(kBackA);
    auto rhs = make_real_resource(kBackB);
    EXPECT_TRUE(lhs->back_dominates(*rhs));
    EXPECT_FALSE(rhs->back_dominates(*lhs));
}

/// @brief The composition back-dominance fan-out matches the forward one in phase 1.
TEST(BackwardApi, CompositionBackDominanceMatchesForward) {
    auto graph = make_backward_api_graph();
    const auto* node = graph->get_node(0);
    ASSERT_NE(node, nullptr);
    ASSERT_NE(node->resource, nullptr);

    auto lhs = node->resource->copy();
    auto rhs = node->resource->copy();
    EXPECT_EQ(lhs->back_dominates(*rhs), *lhs <= *rhs);
    EXPECT_TRUE(lhs->back_dominates(*rhs));
}

// ============================================================================
// Label backward accessors
// ============================================================================

/// @brief Label::extend_back mirrors extend(): it lands on the arc's origin and sets out_arc_.
TEST(BackwardApi, LabelExtendBackSetsMirrorFields) {
    Node<RealResource> origin(0, /*source=*/false, /*sink=*/false);
    Node<RealResource> destination(1, /*source=*/false, /*sink=*/false);
    Arc<RealResource> arc(0,
                          &origin,
                          &destination,
                          std::make_unique<Extender<RealResource>>(
                              RealResource(kBackArcValue),
                              std::make_unique<AdditionExtensionFunction<RealResource>>(),
                              0),
                          0.0);

    Label<RealResource> label(0, make_real_resource(kBackLabelValue));
    Label<RealResource> extended(1, make_real_resource(0.0));

    label.extend_back(arc, &extended);

    EXPECT_EQ(extended.get_end_node(), arc.origin);
    EXPECT_EQ(extended.get_out_arc(), &arc);
    EXPECT_EQ(extended.get_in_arc(), nullptr);
    // extend_back defaults to extend, so the value is the plain sum.
    EXPECT_NEAR(extended.get_resource().get_value().get_value(),
                kBackLabelValue + kBackArcValue,
                1e-12);

    // The forward extension is the mirror image: it lands on the destination.
    Label<RealResource> forward_extended(2, make_real_resource(0.0));
    label.extend(arc, &forward_extended);
    EXPECT_EQ(forward_extended.get_end_node(), arc.destination);
    EXPECT_EQ(forward_extended.get_in_arc(), &arc);
    EXPECT_EQ(forward_extended.get_out_arc(), nullptr);
}

/// @brief Label::is_back_feasible and Label::back_dominates delegate to the resource.
TEST(BackwardApi, LabelBackFeasibleAndBackDominates) {
    Label<RealResource> cheap(0, make_real_resource(kBackA));
    Label<RealResource> costly(1, make_real_resource(kBackB));

    EXPECT_TRUE(cheap.is_back_feasible());
    EXPECT_TRUE(cheap.back_dominates(costly));
    EXPECT_FALSE(costly.back_dominates(cheap));
}

// ============================================================================
// extract_solution overloads
// ============================================================================

/// @brief The path-taking overload reproduces exactly the Solution the label form recorded.
TEST(BackwardApi, ExtractSolutionOverloadsAgree) {
    auto graph = make_backward_api_graph();
    auto algorithm = graph->create_algorithm<SolutionProbeAlgorithm>(
        AlgorithmParams<LabelList<ResourceTypeComposition<RealResource>>>{});

    const auto result = graph->solve(algorithm.get());
    ASSERT_EQ(result.solutions.size(), 1U);
    const size_t solutions_before = algorithm->solution_count();
    ASSERT_EQ(solutions_before, 1U);

    const auto& solution = result.solutions.front();
    algorithm->extract_path(solution.cost, solution.path_arc_ids, solution.path_node_ids.back());
    EXPECT_EQ(algorithm->solution_count(), solutions_before);
}

/// @brief The path-taking overload drops an empty path instead of recording a degenerate one.
TEST(BackwardApi, ExtractSolutionIgnoresEmptyPath) {
    auto graph = make_backward_api_graph();
    auto algorithm = graph->create_algorithm<SolutionProbeAlgorithm>(
        AlgorithmParams<LabelList<ResourceTypeComposition<RealResource>>>{});

    const auto result = graph->solve(algorithm.get());
    ASSERT_FALSE(result.solutions.empty());
    const size_t solutions_before = algorithm->solution_count();

    algorithm->extract_path(0.0, {}, 2);
    EXPECT_EQ(algorithm->solution_count(), solutions_before);
}
