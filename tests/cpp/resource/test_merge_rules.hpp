// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Merge rules: each feasibility function declares a MergeRule and Resource::can_be_merged
// dispatches on it; a join is allowed only if every component agrees. An undeclared rule throws
// rather than defaulting to `true`, which would silently accept cyclic splices.

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace merge_rules_test {

using RealComposition = ResourceTypeComposition<RealResource>;

/// @brief Builds a Resource with the given feasibility and dominance functions.
///
/// The dominance function is explicit because ValueDominanceFunction calls leq(), which
/// SetResource lacks.
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

/// @brief A MinMaxFeasibilityFunction already paired with an extension function's backward kind.
///
/// In a real model `ResourceGraph::add_resource` supplies the kind; unpaired, the function
/// declares MergeRule::Unspecified.
///
/// @param kind The kind the paired extension function would declare.
/// @param min  Window lower bound; also the value a backward label is seeded at.
/// @param max  Window upper bound.
inline std::unique_ptr<MinMaxFeasibilityFunction<RealResource>> paired_min_max(BackwardKind kind,
                                                                               double min = 0.0,
                                                                               double max = 100.0) {
    auto function = std::make_unique<MinMaxFeasibilityFunction<RealResource>>(
        min,
        max,
        /*merge_by_increasing_value=*/kind == BackwardKind::Threshold);
    function->set_backward_kind(kind);
    return function;
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

/// @brief The merge test compares the values; the dominance function has no say in it.
///
/// Mergeability is a fact about the model, while dominance may be relaxed (e.g. by a heuristic);
/// a flipped or trivial dominance function must not change the verdict.
TEST(MergeRules, DominanceOrderIgnoresTheDominanceFunction) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}}};

    const auto arrival_and_deadline =
        [&](double value, std::unique_ptr<DominanceFunction<RealResource>> dom) {
            return merge_rules_test::make_resource<RealResource>(
                RealResource(value),
                std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
                std::move(dom));
        };

    auto early_flipped =
        arrival_and_deadline(30.0,
                             std::make_unique<merge_rules_test::HigherIsBetterDominanceFunction>());
    auto late_flipped =
        arrival_and_deadline(70.0,
                             std::make_unique<merge_rules_test::HigherIsBetterDominanceFunction>());
    // Same answers as with the ordinary dominance function.
    EXPECT_TRUE(early_flipped->can_be_merged(*late_flipped));
    EXPECT_FALSE(late_flipped->can_be_merged(*early_flipped));

    auto early_trivial =
        arrival_and_deadline(30.0, std::make_unique<TrivialDominanceFunction<RealResource>>());
    auto late_trivial =
        arrival_and_deadline(70.0, std::make_unique<TrivialDominanceFunction<RealResource>>());
    EXPECT_TRUE(early_trivial->can_be_merged(*late_trivial));
    EXPECT_FALSE(late_trivial->can_be_merged(*early_trivial))
        << "a relaxed dominance must not let an arrival past the deadline join";
}

/// @brief Disjoint: the body IntersectionFeasibilityFunction inherits rejects shared elements.
///
/// Called directly: a constraining function declares MergeRule::Unspecified, so the dispatch never
/// reaches this body.
TEST(MergeRules, DisjointRejectsSharedElements) {
    std::map<size_t, std::set<int>> forbidden{{0, {1, 2, 3}}};
    IntersectionFeasibilityFunction<SetResource<int>> function(forbidden, /*forbidden=*/true);

    // share element 2
    EXPECT_FALSE(function.can_be_merged(merge_rules_test::make_set({1, 2}),
                                        merge_rules_test::make_set({2, 5})));
    // no shared element
    EXPECT_TRUE(function.can_be_merged(merge_rules_test::make_set({1, 2}),
                                       merge_rules_test::make_set({7, 8})));
}

/// @brief A forbidding intersection function that forbids nothing declares AlwaysTrue.
///
/// Disjointness would reject halves sharing a node even when revisiting is legal, so a function
/// with empty forbidden sets must not impose it on the join.
TEST(MergeRules, AnIntersectionFunctionThatForbidsNothingIsAlwaysTrue) {
    using R = SetResource<int>;

    // Nothing forbidden anywhere: the map is empty.
    IntersectionFeasibilityFunction<R> inert({}, /*forbidden=*/true);
    EXPECT_EQ(inert.merge_rule(), MergeRule::AlwaysTrue);

    // A map whose every entry is an empty set is the same thing said differently.
    IntersectionFeasibilityFunction<R> all_empty({{0, {}}, {1, {}}}, /*forbidden=*/true);
    EXPECT_EQ(all_empty.merge_rule(), MergeRule::AlwaysTrue);

    // One non-empty entry constrains something, which has no backward reading: a refusal.
    IntersectionFeasibilityFunction<R> forbidding({{0, {}}, {1, {1}}}, /*forbidden=*/true);
    EXPECT_EQ(forbidding.merge_rule(), MergeRule::Unspecified);

    // The same for required values; an inert required-values function stays AlwaysTrue.
    IntersectionFeasibilityFunction<R> required({{1, {1}}}, /*forbidden=*/false);
    EXPECT_EQ(required.merge_rule(), MergeRule::Unspecified);
    IntersectionFeasibilityFunction<R> inert_required({{1, {}}}, /*forbidden=*/false);
    EXPECT_EQ(inert_required.merge_rule(), MergeRule::AlwaysTrue);
}

/// @brief Any constraining intersection function is refused rather than merged.
///
/// Every constraining configuration asks a prefix question a backward label cannot answer.
/// Model-level behaviour is covered by
/// `MergeContract.ContainerConstraintsWithoutABackwardReadingAreRefused`.
TEST(MergeRules, AConstrainingIntersectionIsRefusedRatherThanMerged) {
    const std::map<size_t, std::set<int>> values{{0, {1, 2, 3}}};

    for (const bool forbidden : {true, false}) {
        SCOPED_TRACE(forbidden ? "forbidden values" : "required values");
        auto forward = merge_rules_test::make_resource<SetResource<int>>(
            merge_rules_test::make_set({1, 2}),
            std::make_unique<IntersectionFeasibilityFunction<SetResource<int>>>(values, forbidden),
            std::make_unique<InclusionDominanceFunction<SetResource<int>>>());
        auto backward = merge_rules_test::make_resource<SetResource<int>>(
            merge_rules_test::make_set({7, 8}),
            std::make_unique<IntersectionFeasibilityFunction<SetResource<int>>>(values, forbidden),
            std::make_unique<InclusionDominanceFunction<SetResource<int>>>());

        EXPECT_EQ(forward->merge_rule(), MergeRule::Unspecified);
        // Backstop: validate_backward_semantics refuses such a model before any solve gets here.
        EXPECT_THROW((void)forward->can_be_merged(*backward), std::runtime_error);
    }
}

/// @brief Custom: SizeFeasibilityFunction counts |f u b| against the cap, not |f| + |b|.
///
/// Elements shared by both halves appear once in the merged path; summing would double-count them.
TEST(MergeRules, CustomSizeRuleUnionsAgainstTheCap) {
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
    auto overlapping_three = make_sized({5, 8, 9});

    // Disjoint: the union equals the sum.
    EXPECT_TRUE(two->can_be_merged(*also_two));  // |{1,2,3,4}| = 4 <= 4
    EXPECT_FALSE(two->can_be_merged(*three));    // |{1,2,5,6,7}| = 5 > 4

    // Overlapping: the sum would be 6, the union is 3.
    EXPECT_TRUE(three->can_be_merged(*three));  // |{5,6,7}| = 3 <= 4

    // Overlapping but still over the cap.
    EXPECT_FALSE(three->can_be_merged(*overlapping_three));  // |{5,6,7,8,9}| = 5 > 4
}

// ============================================================================
// The two error arms
// ============================================================================

/// @brief An undeclared rule throws rather than guessing.
///
/// A backstop: setup validation normally names the offending component before any label exists.
TEST(MergeRules, UnspecifiedThrows) {
    auto forward = merge_rules_test::make_real_resource(
        (1.0),
        std::make_unique<merge_rules_test::UndeclaredFeasibilityFunction>());
    auto backward = merge_rules_test::make_real_resource(
        (2.0),
        std::make_unique<merge_rules_test::UndeclaredFeasibilityFunction>());

    EXPECT_THROW((void)forward->can_be_merged(*backward), std::runtime_error);
}

// A scalar resource cannot declare disjointness: `DisjointMergeForm<RealResource, ...>` does not
// compile (RealResource has no intersects()), so there is no runtime error arm to test.

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
// Declared rules for every concrete function
// ============================================================================

/// @brief Every concrete feasibility function declares a rule, and every Unspecified in the table
///        is a deliberate refusal.
///
/// One component without a rule makes every join in the model throw, so the whole table is pinned
/// here, including the configurations that deliberately answer Unspecified.
TEST(MergeRules, EveryConcreteFunctionDeclaresARule) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}}};
    std::map<size_t, std::set<int>> values{{0, {1, 2}}};

    EXPECT_EQ(TrivialFeasibilityFunction<RealResource>{}.merge_rule(), MergeRule::AlwaysTrue);
    EXPECT_EQ(TimeWindowFeasibilityFunction<RealResource>{windows}.merge_rule(),
              MergeRule::DominanceOrder);
    // MinMax's rule depends on its pairing; see MinMaxRuleFollowsTheBackwardKind.
    MinMaxFeasibilityFunction<RealResource> min_max{0.0, 100.0, true};
    min_max.set_backward_kind(BackwardKind::Threshold);
    EXPECT_EQ(min_max.merge_rule(), MergeRule::DominanceOrder);
    // Refusals: a constraining intersection function in either direction, a reachability
    // look-ahead, and a size floor all ask a prefix question a backward label cannot answer.
    EXPECT_EQ((IntersectionFeasibilityFunction<SetResource<int>>{values, true}.merge_rule()),
              MergeRule::Unspecified);
    EXPECT_EQ((IntersectionFeasibilityFunction<SetResource<int>>{values, false}.merge_rule()),
              MergeRule::Unspecified);
    EXPECT_EQ((IntersectionFeasibilityFunction<SetResource<int>>{{}, true}.merge_rule()),
              MergeRule::AlwaysTrue);
    EXPECT_EQ((ReachableFeasibilityFunction<SetResource<int>>{merge_rules_test::make_set({1, 2})}
                   .merge_rule()),
              MergeRule::Unspecified);
    EXPECT_EQ((SizeFeasibilityFunction<SetResource<int>>{0U, 4U}.merge_rule()), MergeRule::Custom);
    EXPECT_EQ((SizeFeasibilityFunction<SetResource<int>>{1U, 4U}.merge_rule()),
              MergeRule::Unspecified);
    EXPECT_EQ((SizeFeasibilityFunction<SetResource<int>>{0U, 4U, {{2, {1, 4}}}}.merge_rule()),
              MergeRule::Unspecified);
    // A per-node cap bounds what the path has collected up to that node, which the backward suffix
    // cannot see; an override equal to the default is still a uniform cap.
    EXPECT_EQ((SizeFeasibilityFunction<SetResource<int>>{0U, 4U, {{2, {0, 1}}}}.merge_rule()),
              MergeRule::Unspecified);
    EXPECT_EQ((SizeFeasibilityFunction<SetResource<int>>{0U, 4U, {{2, {0, 4}}}}.merge_rule()),
              MergeRule::Custom);
}

// ============================================================================
// MinMaxFeasibilityFunction: the rule follows the pairing
// ============================================================================
//
// Under a threshold extension a backward value is a ceiling, so `forward <= backward` is right.
// Under an accumulating one it is the suffix's own load, so the halves must be summed; comparing
// them would wrongly accept (e.g. 3 + 3 under a cap of 4), and acceptances are never replayed.

/// @brief The rule is DominanceOrder under a threshold pairing, Custom under an accumulating one,
///        and Unspecified when the function has not been paired at all.
TEST(MergeRules, MinMaxRuleFollowsTheBackwardKind) {
    // Unpaired: no kind yet, so Unspecified and a bidirectional solve refuses at setup.
    EXPECT_EQ((MinMaxFeasibilityFunction<RealResource>{0.0, 100.0, true}.merge_rule()),
              MergeRule::Unspecified);

    EXPECT_EQ(merge_rules_test::paired_min_max(BackwardKind::Threshold)->merge_rule(),
              MergeRule::DominanceOrder);
    EXPECT_EQ(merge_rules_test::paired_min_max(BackwardKind::Accumulate)->merge_rule(),
              MergeRule::Custom);

    // Mirror is a container's shape; pairing it with a scalar window is incoherent.
    EXPECT_EQ(merge_rules_test::paired_min_max(BackwardKind::Mirror)->merge_rule(),
              MergeRule::Unspecified);
}

/// @brief Under a threshold pairing, a floor anywhere is a refusal.
///
/// A backward label carries only a ceiling, so a minimum cannot be checked at the join. Any
/// non-zero minimum counts, default or per node. The accumulating arm is unaffected.
TEST(MergeRules, MinMaxFloorUnderAThresholdIsRefused) {
    using Windows = std::map<size_t, std::pair<double, double>>;
    const auto threshold_rule = [](MinMaxFeasibilityFunction<RealResource> function) {
        function.set_backward_kind(BackwardKind::Threshold);
        return function.merge_rule();
    };

    EXPECT_EQ(threshold_rule({0.0, 100.0, true}), MergeRule::DominanceOrder);
    EXPECT_EQ(threshold_rule({5.0, 100.0, true}), MergeRule::Unspecified);
    EXPECT_EQ(threshold_rule({-20.0, 100.0, true}), MergeRule::Unspecified);
    // Per node: a zero floor on an override is still no floor; a positive one is.
    EXPECT_EQ(threshold_rule({0.0, 100.0, Windows{{1, {0.0, 50.0}}}}), MergeRule::DominanceOrder);
    EXPECT_EQ(threshold_rule({0.0, 100.0, Windows{{1, {3.0, 50.0}}}}), MergeRule::Unspecified);

    // The accumulating arm refuses a floor too; see MinMaxAccumulateIsDeclaredOnlyWhenExact.
    EXPECT_EQ(merge_rules_test::paired_min_max(BackwardKind::Accumulate, 5.0, 100.0)->merge_rule(),
              MergeRule::Unspecified);
}

/// @brief Under an accumulating pairing the two halves ADD, and the sum is tested against the cap.
///
/// The grid straddles the cap, and includes the diagonal `f == b` that `f <= b` would accept.
TEST(MergeRules, MinMaxAccumulateAddsTheTwoHalves) {
    constexpr double kCapacity = 4.0;
    const std::vector<double> grid{0.0, 1.0, 2.0, 3.0, 4.0, 5.0};

    for (const double forward_value : grid) {
        for (const double backward_value : grid) {
            SCOPED_TRACE("f=" + std::to_string(forward_value) +
                         " b=" + std::to_string(backward_value));

            auto forward = merge_rules_test::make_real_resource(
                forward_value,
                merge_rules_test::paired_min_max(BackwardKind::Accumulate, 0.0, kCapacity));
            auto backward = merge_rules_test::make_real_resource(
                backward_value,
                merge_rules_test::paired_min_max(BackwardKind::Accumulate, 0.0, kCapacity));

            EXPECT_EQ(forward->can_be_merged(*backward),
                      forward_value + backward_value <= kCapacity);
        }
    }

    // Equal halves that together overflow the cap.
    auto three_forward = merge_rules_test::make_real_resource(
        3.0,
        merge_rules_test::paired_min_max(BackwardKind::Accumulate, 0.0, kCapacity));
    auto three_backward = merge_rules_test::make_real_resource(
        3.0,
        merge_rules_test::paired_min_max(BackwardKind::Accumulate, 0.0, kCapacity));
    EXPECT_FALSE(three_forward->can_be_merged(*three_backward));
}

/// @brief The accumulating sum is declared only where it is exact: a uniform window with a zero
///        minimum.
///
/// A minimum offsets the backward seed, and a per-node window bounds what the path has consumed
/// up to a node, which the backward suffix cannot see. Either way a bidirectional solve refuses.
TEST(MergeRules, MinMaxAccumulateIsDeclaredOnlyWhenExact) {
    EXPECT_EQ(merge_rules_test::paired_min_max(BackwardKind::Accumulate, 0.0, 4.0)->merge_rule(),
              MergeRule::Custom);
    EXPECT_EQ(merge_rules_test::paired_min_max(BackwardKind::Accumulate, 1.0, 4.0)->merge_rule(),
              MergeRule::Unspecified);
    EXPECT_EQ(merge_rules_test::paired_min_max(BackwardKind::Accumulate, -1.0, 4.0)->merge_rule(),
              MergeRule::Unspecified);

    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}}, {1, {0.0, 4.0}}};
    MinMaxFeasibilityFunction<RealResource> per_node{0.0, 100.0, windows};
    per_node.set_backward_kind(BackwardKind::Accumulate);
    EXPECT_EQ(per_node.merge_rule(), MergeRule::Unspecified);
}

/// @brief The accumulating body refuses to answer for a pairing it is not the body of.
///
/// A threshold pairing merges via MergeRule::DominanceOrder in `Resource::can_be_merged` and never
/// routes here, so a direct call throws.
TEST(MergeRules, MinMaxCanBeMergedIsTheAccumulatingBodyOnly) {
    auto threshold = merge_rules_test::paired_min_max(BackwardKind::Threshold);
    const RealResource low(1.0);
    const RealResource high(2.0);
    // Bind the [[nodiscard]] result so EXPECT_THROW does not discard it.
    EXPECT_THROW(
        { [[maybe_unused]] const bool merged = threshold->can_be_merged(low, high); },
        std::logic_error);

    auto accumulate = merge_rules_test::paired_min_max(BackwardKind::Accumulate, 0.0, 4.0);
    EXPECT_TRUE(accumulate->can_be_merged(low, high));
}

/// @brief ReachableFeasibilityFunction can be instantiated and is_reachable() works.
///
/// Covers both operands of the short-circuit and both outcomes.
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

/// @brief DominanceOrder matches `f <= b` for time-window and threshold-paired min-max functions.
///
/// Drives a grid of (f, b) through the dispatch.
TEST(MergeRules, DominanceOrderAgreesWithTheDeletedBodies) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}}};
    const std::vector<double> grid{0.0, 10.0, 30.0, 30.0, 70.0, 100.0};

    for (const double forward_value : grid) {
        for (const double backward_value : grid) {
            SCOPED_TRACE("f=" + std::to_string(forward_value) +
                         " b=" + std::to_string(backward_value));

            // The expected `f <= b` answer.
            const bool old_time_window_answer = forward_value <= backward_value;
            auto tw_forward = merge_rules_test::make_real_resource(
                forward_value,
                std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows));
            auto tw_backward = merge_rules_test::make_real_resource(
                backward_value,
                std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows));
            EXPECT_EQ(tw_forward->can_be_merged(*tw_backward), old_time_window_answer);

            // `f <= b` is the threshold reading; an accumulating pairing adds the values instead
            // (see MinMaxAccumulateAddsTheTwoHalves).
            auto mm_forward = merge_rules_test::make_real_resource(
                forward_value,
                merge_rules_test::paired_min_max(BackwardKind::Threshold));
            auto mm_backward = merge_rules_test::make_real_resource(
                backward_value,
                merge_rules_test::paired_min_max(BackwardKind::Threshold));
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

// DisjointMergeForm used standalone, with no merge_rule() override: its own `Custom` declaration
// is otherwise unreachable, since IntersectionFeasibilityFunction overrides it.
TEST(MergeRules, DisjointFormStandaloneDeclaresCustomAndRejectsOverlap) {
    class PlainDisjoint
        : public Clonable<
              PlainDisjoint,
              DisjointMergeForm<SetResource<int>, FeasibilityFunction<SetResource<int>>>,
              FeasibilityFunction<SetResource<int>>> {
        public:
            [[nodiscard]] auto is_feasible(const SetResource<int>& /*resource*/) -> bool override {
                return true;
            }
    };

    // The form supplies the declaration as well as the body.
    EXPECT_EQ(PlainDisjoint{}.merge_rule(), MergeRule::Custom);

    auto forward = merge_rules_test::make_resource<SetResource<int>>(
        merge_rules_test::make_set({1, 2}),
        std::make_unique<PlainDisjoint>(),
        std::make_unique<InclusionDominanceFunction<SetResource<int>>>());
    auto overlapping = merge_rules_test::make_resource<SetResource<int>>(
        merge_rules_test::make_set({2, 5}),
        std::make_unique<PlainDisjoint>(),
        std::make_unique<InclusionDominanceFunction<SetResource<int>>>());
    auto disjoint = merge_rules_test::make_resource<SetResource<int>>(
        merge_rules_test::make_set({7, 8}),
        std::make_unique<PlainDisjoint>(),
        std::make_unique<InclusionDominanceFunction<SetResource<int>>>());

    EXPECT_FALSE(forward->can_be_merged(*overlapping));  // share element 2
    EXPECT_TRUE(forward->can_be_merged(*disjoint));
}
