// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Phase 4: merge rules.
//
// At a join you have a forward label moved to v and a backward label at v. Each resource says
// whether the two are compatible; the join is allowed only if all agree. Rather than write a merge
// body per resource, each feasibility function *declares* a MergeRule and Resource::can_be_merged
// dispatches on it -- so only SizeFeasibilityFunction writes any merge code at all.
//
// The default stays a throw on purpose. `true` would mean "these halves are always compatible",
// which for a visited-set resource glues two halves that both visit customer 7 and reports a path
// with a cycle as valid. A throw is loud; `true` is silently wrong.

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace merge_rules_test {

using RealComposition = ResourceTypeComposition<RealResource>;

/// @brief Builds a Resource with the given feasibility and dominance functions.
///
/// The dominance function is explicit rather than defaulted: defaulting it to
/// ValueDominanceFunction would instantiate that template for container resources too, and it
/// calls leq(), which SetResource does not have.
template <typename ResourceType, typename FeasibilityFn>
std::unique_ptr<Resource<ResourceType>> make_resource(
    const ResourceType& value, std::unique_ptr<FeasibilityFn> feasibility,
    std::unique_ptr<DominanceFunction<ResourceType>> dominance) {
    return std::make_unique<Resource<ResourceType>>(
        value,
        std::move(dominance),
        std::move(feasibility),
        std::make_unique<TrivialCostFunction<ResourceType>>());
}

/// @brief make_resource with the usual scalar dominance function.
template <typename FeasibilityFn>
std::unique_ptr<Resource<RealResource>> make_real_resource(
    double value, std::unique_ptr<FeasibilityFn> feasibility) {
    return make_resource<RealResource>(RealResource(value),
                                       std::move(feasibility),
                                       std::make_unique<ValueDominanceFunction<RealResource>>());
}

inline SetResource<int> make_set(const std::set<int>& values) {
    SetResource<int> resource;
    resource.set_value(values);
    return resource;
}

/// @brief A dominance function where LARGER is better -- the shape of a resource whose forward
///        order is already flipped.
class HigherIsBetterDominanceFunction
    : public Clonable<HigherIsBetterDominanceFunction, DominanceFunction<RealResource>> {
    public:
        [[nodiscard]] auto check_dominance(const RealResource& lhs_resource,
                                           const RealResource& rhs_resource) -> bool override {
            return lhs_resource.geq(rhs_resource.get_value());
        }

        auto fast_check_dominance(const RealResource& lhs_resource,
                                  const RealResource& rhs_resource, double delta) -> bool override {
            return lhs_resource.geq(rhs_resource.get_value() - delta);
        }
};

/// @brief Declares a rule without implementing anything -- used to reach the error arms.
template <MergeRule Rule>
class DeclaredRuleFeasibilityFunction
    : public Clonable<DeclaredRuleFeasibilityFunction<Rule>, FeasibilityFunction<RealResource>> {
    public:
        [[nodiscard]] auto is_feasible(const RealResource& /*resource*/) -> bool override {
            return true;
        }

        [[nodiscard]] MergeRule merge_rule() const override { return Rule; }
};

/// @brief Declares nothing at all: merge_rule() stays Unspecified.
class UndeclaredFeasibilityFunction
    : public Clonable<UndeclaredFeasibilityFunction, FeasibilityFunction<RealResource>> {
    public:
        [[nodiscard]] auto is_feasible(const RealResource& /*resource*/) -> bool override {
            return true;
        }
};

}  // namespace merge_rules_test

// ============================================================================
// One test per rule -- every switch arm must be hit
// ============================================================================

/// @brief AlwaysTrue: a cost resource merges with anything.
TEST(MergeRules, AlwaysTrueAcceptsEverything) {
    auto forward = merge_rules_test::make_real_resource(
        (1000.0),
        std::make_unique<TrivialFeasibilityFunction<RealResource>>());
    auto backward = merge_rules_test::make_real_resource(
        (-1000.0),
        std::make_unique<TrivialFeasibilityFunction<RealResource>>());

    EXPECT_TRUE(forward->can_be_merged(*backward));
    EXPECT_TRUE(backward->can_be_merged(*forward));
}

/// @brief DominanceOrder: for a time window, can_be_merged(f, b) == (f <= b).
TEST(MergeRules, DominanceOrderIsForwardDominance) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}}};

    auto early = merge_rules_test::make_real_resource(
        (30.0),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows));
    auto late = merge_rules_test::make_real_resource(
        (70.0),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows));

    // Arriving at 30 against a deadline of 70: fits.
    EXPECT_TRUE(early->can_be_merged(*late));
    // Arriving at 70 against a deadline of 30: does not.
    EXPECT_FALSE(late->can_be_merged(*early));
}

/// @brief The merge test flips with the dominance function, with no code in the feasibility
///        function.
///
/// This is what "the rule follows the dominance order" buys: a higher-is-better resource needs no
/// direction flag of its own.
TEST(MergeRules, DominanceOrderFollowsAFlippedDominanceFunction) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}}};

    auto small_value = merge_rules_test::make_resource<RealResource>(
        RealResource(30.0),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        std::make_unique<merge_rules_test::HigherIsBetterDominanceFunction>());
    auto large_value = merge_rules_test::make_resource<RealResource>(
        RealResource(70.0),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        std::make_unique<merge_rules_test::HigherIsBetterDominanceFunction>());

    // Exactly the opposite of the previous test, purely because the dominance function flipped.
    EXPECT_FALSE(small_value->can_be_merged(*large_value));
    EXPECT_TRUE(large_value->can_be_merged(*small_value));
}

/// @brief Disjoint: containers sharing an element reject; disjoint ones accept.
TEST(MergeRules, DisjointRejectsSharedElements) {
    std::map<size_t, std::set<int>> forbidden{{0, {1, 2, 3}}};

    auto forward = merge_rules_test::make_resource<SetResource<int>>(
        merge_rules_test::make_set({1, 2}),
        std::make_unique<IntersectionFeasibilityFunction<SetResource<int>>>(forbidden,
                                                                            /*forbidden=*/true),
        std::make_unique<InclusionDominanceFunction<SetResource<int>>>());
    auto overlapping = merge_rules_test::make_resource<SetResource<int>>(
        merge_rules_test::make_set({2, 5}),
        std::make_unique<IntersectionFeasibilityFunction<SetResource<int>>>(forbidden,
                                                                            /*forbidden=*/true),
        std::make_unique<InclusionDominanceFunction<SetResource<int>>>());
    auto disjoint = merge_rules_test::make_resource<SetResource<int>>(
        merge_rules_test::make_set({7, 8}),
        std::make_unique<IntersectionFeasibilityFunction<SetResource<int>>>(forbidden,
                                                                            /*forbidden=*/true),
        std::make_unique<InclusionDominanceFunction<SetResource<int>>>());

    EXPECT_FALSE(forward->can_be_merged(*overlapping));  // share element 2
    EXPECT_TRUE(forward->can_be_merged(*disjoint));      // no shared element
}

/// @brief Required (not forbidden) values merge freely: it is a whole-path property.
TEST(MergeRules, RequiredIntersectionIsAlwaysTrue) {
    std::map<size_t, std::set<int>> required{{0, {1, 2, 3}}};

    auto forward = merge_rules_test::make_resource<SetResource<int>>(
        merge_rules_test::make_set({1, 2}),
        std::make_unique<IntersectionFeasibilityFunction<SetResource<int>>>(required,
                                                                            /*forbidden=*/false),
        std::make_unique<InclusionDominanceFunction<SetResource<int>>>());
    auto backward = merge_rules_test::make_resource<SetResource<int>>(
        merge_rules_test::make_set({2, 5}),
        std::make_unique<IntersectionFeasibilityFunction<SetResource<int>>>(required,
                                                                            /*forbidden=*/false),
        std::make_unique<InclusionDominanceFunction<SetResource<int>>>());

    // Overlapping, and accepted anyway -- the asymmetry with the forbidden case.
    EXPECT_TRUE(forward->can_be_merged(*backward));
}

/// @brief Custom: SizeFeasibilityFunction accepts |f| + |b| <= max and rejects above it.
TEST(MergeRules, CustomSizeRuleSumsAgainstTheCap) {
    constexpr size_t kMaxSize = 4;

    auto make_sized = [kMaxSize](const std::set<int>& values) {
        return merge_rules_test::make_resource<SetResource<int>>(
            merge_rules_test::make_set(values),
            std::make_unique<SizeFeasibilityFunction<SetResource<int>>>(0U, kMaxSize),
            std::make_unique<InclusionDominanceFunction<SetResource<int>>>());
    };

    auto two = make_sized({1, 2});
    auto also_two = make_sized({3, 4});
    auto three = make_sized({5, 6, 7});

    EXPECT_TRUE(two->can_be_merged(*also_two));  // 2 + 2 = 4 <= 4
    EXPECT_FALSE(two->can_be_merged(*three));    // 2 + 3 = 5 >  4
    EXPECT_FALSE(three->can_be_merged(*three));  // 3 + 3 = 6 >  4
}

// ============================================================================
// The two error arms
// ============================================================================

/// @brief An undeclared rule throws rather than guessing.
///
/// `true` would silently accept a merge that produces a cyclic path; a throw is loud. Phase 11
/// adds setup validation that names the offending component before any label is created, so this
/// is the backstop rather than the front door.
TEST(MergeRules, UnspecifiedThrows) {
    auto forward = merge_rules_test::make_real_resource(
        (1.0),
        std::make_unique<merge_rules_test::UndeclaredFeasibilityFunction>());
    auto backward = merge_rules_test::make_real_resource(
        (2.0),
        std::make_unique<merge_rules_test::UndeclaredFeasibilityFunction>());

    EXPECT_THROW((void)forward->can_be_merged(*backward), std::runtime_error);
}

/// @brief Declaring Disjoint on a scalar resource throws: it has no intersects().
///
/// This is the `if constexpr` else branch. It is unreachable from any resource the library ships,
/// so it has to be constructed deliberately.
TEST(MergeRules, DisjointOnScalarResourceThrows) {
    auto forward = merge_rules_test::make_real_resource(
        (1.0),
        std::make_unique<merge_rules_test::DeclaredRuleFeasibilityFunction<MergeRule::Disjoint>>());
    auto backward = merge_rules_test::make_real_resource(
        (2.0),
        std::make_unique<merge_rules_test::DeclaredRuleFeasibilityFunction<MergeRule::Disjoint>>());

    EXPECT_THROW((void)forward->can_be_merged(*backward), std::logic_error);
}

// ============================================================================
// Composition
// ============================================================================

/// @brief One component rejecting rejects the whole merge.
TEST(MergeRules, CompositionRejectsIfAnyComponentRejects) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}}};

    auto build = [&windows](double cost_value, double time_value) {
        std::tuple<std::vector<std::unique_ptr<Resource<RealResource>>>> components;
        // Component 0: cost, AlwaysTrue.
        std::get<0>(components)
            .push_back(merge_rules_test::make_real_resource(
                cost_value,
                std::make_unique<TrivialFeasibilityFunction<RealResource>>()));
        // Component 1: time window, DominanceOrder.
        std::get<0>(components)
            .push_back(merge_rules_test::make_real_resource(
                time_value,
                std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows)));

        return std::make_unique<Resource<merge_rules_test::RealComposition>>(
            std::move(components),
            std::make_unique<CompositionDominanceFunction<RealResource>>(),
            std::make_unique<CompositionFeasibilityFunction<RealResource>>(),
            std::make_unique<CompositionCostFunction<RealResource>>(),
            0);
    };

    auto forward = build(/*cost=*/999.0, /*time=*/30.0);
    auto accepting = build(/*cost=*/-999.0, /*time=*/70.0);
    auto rejecting = build(/*cost=*/-999.0, /*time=*/10.0);

    // Cost never blocks; the time window decides.
    EXPECT_TRUE(forward->can_be_merged(*accepting));
    EXPECT_FALSE(forward->can_be_merged(*rejecting));
}

// ============================================================================
// The table in the design, and the two deletions
// ============================================================================

/// @brief Every concrete feasibility function declares a rule other than Unspecified.
///
/// Because the composition ANDs across all components, one resource without a rule makes every
/// join in the model throw. This single test guards the whole table.
TEST(MergeRules, EveryConcreteFunctionDeclaresARule) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}}};
    std::map<size_t, std::set<int>> values{{0, {1, 2}}};

    EXPECT_EQ(TrivialFeasibilityFunction<RealResource>{}.merge_rule(), MergeRule::AlwaysTrue);
    EXPECT_EQ(TimeWindowFeasibilityFunction<RealResource>{windows}.merge_rule(),
              MergeRule::DominanceOrder);
    EXPECT_EQ((MinMaxFeasibilityFunction<RealResource>{0.0, 100.0, true}.merge_rule()),
              MergeRule::DominanceOrder);
    EXPECT_EQ((IntersectionFeasibilityFunction<SetResource<int>>{values, true}.merge_rule()),
              MergeRule::Disjoint);
    EXPECT_EQ((IntersectionFeasibilityFunction<SetResource<int>>{values, false}.merge_rule()),
              MergeRule::AlwaysTrue);
    EXPECT_EQ((ReachableFeasibilityFunction<SetResource<int>>{merge_rules_test::make_set({1, 2})}
                   .merge_rule()),
              MergeRule::AlwaysTrue);
    EXPECT_EQ((SizeFeasibilityFunction<SetResource<int>>{0U, 4U}.merge_rule()), MergeRule::Custom);
}

/// @brief ReachableFeasibilityFunction actually works, now that it can be instantiated.
///
/// Its is_reachable() used to call resource.contains(), which `Resource` does not have, so the
/// class could not be instantiated at all -- the error had never surfaced because nothing in the
/// repository used it. Declaring a merge rule on it required instantiating it, which exposed
/// that; this test covers the fixed body, both operands of the short-circuit and both outcomes.
TEST(MergeRules, ReachableFeasibilityFunctionIsUsable) {
    ReachableFeasibilityFunction<SetResource<int>> function{merge_rules_test::make_set({1, 2})};

    auto visited_one = merge_rules_test::make_resource<SetResource<int>>(
        merge_rules_test::make_set({1}),
        std::make_unique<TrivialFeasibilityFunction<SetResource<int>>>(),
        std::make_unique<InclusionDominanceFunction<SetResource<int>>>());
    auto visited_nothing = merge_rules_test::make_resource<SetResource<int>>(
        merge_rules_test::make_set({}),
        std::make_unique<TrivialFeasibilityFunction<SetResource<int>>>(),
        std::make_unique<InclusionDominanceFunction<SetResource<int>>>());

    // Locally always feasible; all the work is in is_reachable().
    EXPECT_TRUE(function.is_feasible(visited_one->get_value()));

    // Node 3 is not checked, so it is reachable whatever the label has visited.
    EXPECT_TRUE(function.is_reachable(*visited_nothing, 3));

    // Node 1 is checked: reachable only if the label has already visited it.
    EXPECT_TRUE(function.is_reachable(*visited_one, 1));
    EXPECT_FALSE(function.is_reachable(*visited_nothing, 1));
}

/// @brief The deleted hand-written bodies are provably behaviour-preserving.
///
/// TimeWindowFeasibilityFunction used to implement `f <= b`, and MinMaxFeasibilityFunction the
/// same with a direction flag. Both are now MergeRule::DominanceOrder. This drives a grid of
/// (f, b) through the dispatch and checks it against what the old bodies returned, so the removal
/// of two working implementations comes with evidence.
TEST(MergeRules, DominanceOrderAgreesWithTheDeletedBodies) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}}};
    const std::vector<double> grid{0.0, 10.0, 30.0, 30.0, 70.0, 100.0};

    for (const double forward_value : grid) {
        for (const double backward_value : grid) {
            SCOPED_TRACE("f=" + std::to_string(forward_value) +
                         " b=" + std::to_string(backward_value));

            // What TimeWindowFeasibilityFunction::can_be_merged used to compute.
            const bool old_time_window_answer = forward_value <= backward_value;
            auto tw_forward = merge_rules_test::make_real_resource(
                forward_value,
                std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows));
            auto tw_backward = merge_rules_test::make_real_resource(
                backward_value,
                std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows));
            EXPECT_EQ(tw_forward->can_be_merged(*tw_backward), old_time_window_answer);

            // What MinMaxFeasibilityFunction::can_be_merged used to compute with
            // merge_by_increasing_value_ = true. The flag is kept on the constructor but no
            // longer drives the test; the dominance function carries the direction instead.
            auto mm_forward = merge_rules_test::make_real_resource(
                forward_value,
                std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 100.0, true));
            auto mm_backward = merge_rules_test::make_real_resource(
                backward_value,
                std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 100.0, true));
            EXPECT_EQ(mm_forward->can_be_merged(*mm_backward), old_time_window_answer);
        }
    }
}

/// @brief The cached rule is rebound when a recycled resource adopts another's functions.
///
/// reset(const ResourceClass&) rebinds the three function pointers; if the cached rule were not
/// rebound alongside, a recycled label would keep the previous model's rule.
TEST(MergeRules, ResetRebindsTheCachedRule) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}}};

    // A resource whose rule is AlwaysTrue.
    auto always_true = merge_rules_test::make_real_resource(
        (0.0),
        std::make_unique<TrivialFeasibilityFunction<RealResource>>());

    // A resource whose rule is DominanceOrder.
    auto dominance_order = merge_rules_test::make_real_resource(
        (0.0),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows));

    auto late = merge_rules_test::make_real_resource(
        (70.0),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows));

    // Before the reset it accepts everything, being AlwaysTrue.
    always_true->set_value(RealResource(99.0));
    EXPECT_TRUE(always_true->can_be_merged(*late));

    // After adopting the DominanceOrder function it must apply that rule instead.
    always_true->reset(*dominance_order);
    always_true->set_value(RealResource(99.0));
    EXPECT_FALSE(always_true->can_be_merged(*late));  // 99 <= 70 is false
}
