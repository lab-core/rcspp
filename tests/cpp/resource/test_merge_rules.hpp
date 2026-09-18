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

/// @brief A MinMaxFeasibilityFunction already paired with an extension function's backward kind.
///
/// `ResourceGraph::add_resource` is what supplies the kind in a real model; these tests build
/// functions by hand, so they have to supply it themselves or the function correctly declares
/// MergeRule::Unspecified and every merge through it throws.
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
///
/// The map is the ng-route condition -- every node forbids itself -- because that is the only
/// shape whose merge rule is `Custom`; anything else is refused at setup rather than merged. See
/// `ForbiddingSomeOtherNodeIsRefusedRatherThanMerged`.
TEST(MergeRules, DisjointRejectsSharedElements) {
    std::map<size_t, std::set<int>> forbidden;
    for (int node = 0; node <= 8; ++node) {
        forbidden[static_cast<size_t>(node)] = {node};
    }

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

/// @brief A forbidding intersection function that forbids nothing declares AlwaysTrue.
///
/// Disjointness is a restriction the extension does not impose -- it rejects two halves that share
/// a node even when revisiting is legal -- so a function with empty forbidden sets must not carry
/// it into the join. An "inert" ng component that still narrowed the join made a bounded solve
/// return -51.95 where the unbounded one returned -80.02 (review finding D6).
TEST(MergeRules, AnIntersectionFunctionThatForbidsNothingIsAlwaysTrue) {
    using R = SetResource<int>;

    // Nothing forbidden anywhere: the map is empty.
    IntersectionFeasibilityFunction<R> inert({}, /*forbidden=*/true);
    EXPECT_EQ(inert.merge_rule(), MergeRule::AlwaysTrue);

    // A map whose every entry is an empty set is the same thing said differently.
    IntersectionFeasibilityFunction<R> all_empty({{0, {}}, {1, {}}}, /*forbidden=*/true);
    EXPECT_EQ(all_empty.merge_rule(), MergeRule::AlwaysTrue);

    // One non-empty entry is enough to make the rule bite -- when it forbids its own node.
    IntersectionFeasibilityFunction<R> forbidding({{0, {}}, {1, {1}}}, /*forbidden=*/true);
    EXPECT_EQ(forbidding.merge_rule(), MergeRule::Custom);

    // And non-empty is NOT enough on its own: "any set non-empty" was the old test and it is a
    // proxy. A set that forbids some other node, or that binds nothing at all, is refused.
    IntersectionFeasibilityFunction<R> forbids_another({{1, {2}}}, /*forbidden=*/true);
    EXPECT_EQ(forbids_another.merge_rule(), MergeRule::Unspecified);

    IntersectionFeasibilityFunction<R> forbids_itself_and_more({{1, {1, 2}}}, /*forbidden=*/true);
    EXPECT_EQ(forbids_itself_and_more.merge_rule(), MergeRule::Unspecified);

    // One bad entry among good ones is enough: the rule is a property of the whole model.
    IntersectionFeasibilityFunction<R> mostly_good({{0, {0}}, {1, {1}}, {2, {9}}},
                                                   /*forbidden=*/true);
    EXPECT_EQ(mostly_good.merge_rule(), MergeRule::Unspecified);

    // Required values that require something refuse instead -- see
    // RequiredIntersectionIsRefusedRatherThanMerged. Inertness is checked in BOTH directions, so
    // a required-values function that requires nothing anywhere is still AlwaysTrue.
    IntersectionFeasibilityFunction<R> required({{1, {1}}}, /*forbidden=*/false);
    EXPECT_EQ(required.merge_rule(), MergeRule::Unspecified);

    IntersectionFeasibilityFunction<R> inert_required({{0, {}}}, /*forbidden=*/false);
    EXPECT_EQ(inert_required.merge_rule(), MergeRule::AlwaysTrue);
}

/// @brief Required (not forbidden) values refuse the join rather than merging freely.
///
/// This test used to be `RequiredIntersectionIsAlwaysTrue` and asserted the opposite: that two
/// overlapping halves merge, "the asymmetry with the forbidden case". The merge half of that is
/// still true -- nothing two halves do by being combined can break a requirement -- but it was
/// the wrong question. `is_feasible` here asks whether the set collected SO FAR contains a
/// required value, which is a predicate on a prefix, and `is_back_feasible` applies it unchanged
/// to a backward label's suffix. The backward half is discarded before the join, so the pair the
/// old test built never reaches the merge rule in a real solve.
///
/// So the rule declares Unspecified and `Resource::can_be_merged` throws, which is what a
/// bidirectional solve turns into a setup refusal naming the component. See
/// `IntersectionFeasibilityFunction::merge_rule`.
TEST(MergeRules, RequiredIntersectionIsRefusedRatherThanMerged) {
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

    // The rule is undeclared, so the dispatch throws rather than guessing.
    EXPECT_EQ(forward->merge_rule(), MergeRule::Unspecified);
    EXPECT_THROW(
        { [[maybe_unused]] const bool merged = forward->can_be_merged(*backward); },
        std::runtime_error);
}

/// @brief Forbidding some OTHER node is a prefix question too, and is refused.
///
/// `forbidden(v) = {v}` -- the ng-route condition -- is the only forbidden shape with a backward
/// reading. Forward it says "the prefix already visited `v`"; backward the inherited
/// `is_back_feasible` reads it as "the suffix visits `v` again", which is the right mirror; and
/// the cross case is exactly what `DisjointMergeForm` tests. `forbidden(u) = {w}` has no such
/// coincidence, and it fails by ACCEPTING: the backward half never sees `w`, so it is admitted,
/// and disjointness never sees `w` either because `w` is not on the suffix. The end-to-end
/// consequence is pinned by `Bidirectional.AContainerForbiddingAnotherNodeIsRefused`; this is the
/// declaration it rests on.
///
/// The sibling case in the same test is a set that binds nothing. It fails the other way -- an
/// over-strict join -- and is the reason "is any set non-empty" is not the precondition.
TEST(MergeRules, ForbiddingSomeOtherNodeIsRefusedRatherThanMerged) {
    // Node 0 forbids node 3. A path that collects 3 and later reaches 0 is infeasible, and no
    // backward label can tell.
    std::map<size_t, std::set<int>> forbids_another{{0, {3}}};

    auto forward = merge_rules_test::make_resource<SetResource<int>>(
        merge_rules_test::make_set({3}),
        std::make_unique<IntersectionFeasibilityFunction<SetResource<int>>>(forbids_another,
                                                                            /*forbidden=*/true),
        std::make_unique<InclusionDominanceFunction<SetResource<int>>>());
    auto backward = merge_rules_test::make_resource<SetResource<int>>(
        merge_rules_test::make_set({7}),
        std::make_unique<IntersectionFeasibilityFunction<SetResource<int>>>(forbids_another,
                                                                            /*forbidden=*/true),
        std::make_unique<InclusionDominanceFunction<SetResource<int>>>());

    // Disjointness would have said `true` here -- {3} and {7} share nothing -- which is exactly
    // the acceptance this refusal replaces.
    EXPECT_EQ(forward->merge_rule(), MergeRule::Unspecified);
    EXPECT_THROW(
        { [[maybe_unused]] const bool merged = forward->can_be_merged(*backward); },
        std::runtime_error);

    // The same refusal for a set that constrains nothing reachable. Disjointness would have said
    // `false` here -- {1,2} and {2,5} share 2 -- on a model that permits the revisit outright.
    std::map<size_t, std::set<int>> binds_nothing{{0, {99}}};
    auto loose_forward = merge_rules_test::make_resource<SetResource<int>>(
        merge_rules_test::make_set({1, 2}),
        std::make_unique<IntersectionFeasibilityFunction<SetResource<int>>>(binds_nothing,
                                                                            /*forbidden=*/true),
        std::make_unique<InclusionDominanceFunction<SetResource<int>>>());
    EXPECT_EQ(loose_forward->merge_rule(), MergeRule::Unspecified);
}

/// @brief Custom: SizeFeasibilityFunction counts |f u b| against the cap, not |f| + |b|.
///
/// The union is the whole point. Two halves meeting at a node have both collected whatever they
/// share, and the merged path holds it once; summing counts it twice and refuses splices the model
/// permits. This test used to assert the sum, under the name `CustomSizeRuleSumsAgainstTheCap` --
/// the last two cases below are the ones that changed answer.
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

    // Disjoint: the union is the sum, so these are the cases the two readings agree on.
    EXPECT_TRUE(two->can_be_merged(*also_two));  // |{1,2,3,4}| = 4 <= 4
    EXPECT_FALSE(two->can_be_merged(*three));    // |{1,2,5,6,7}| = 5 > 4

    // Overlapping: where they part. The sum said 6 and refused; the union is 3.
    EXPECT_TRUE(three->can_be_merged(*three));  // |{5,6,7}| = 3 <= 4

    // Overlapping and still over the cap: sharing an element is not a free pass, so the bound is
    // doing work rather than being vacuous once the double-counting is gone.
    EXPECT_FALSE(three->can_be_merged(*overlapping_three));  // |{5,6,7,8,9}| = 5 > 4
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

// DisjointOnScalarResourceThrows was deleted in step 6, and what replaces it is not another
// test but an *impossibility*: a scalar resource can no longer declare disjointness, because the
// body lives on DisjointMergeForm and `DisjointMergeForm<RealResource, ...>` does not compile --
// RealResource has no intersects(). There is no throw left to exercise.
//
// Asserting that non-compilation from inside the suite is not worth it: naming the template in a
// `requires` clause does not instantiate its members, so the missing intersects() would not be
// detected there, and a try_compile fixture is new build machinery whose own failure modes are
// harder to reason about than the thing it checks. This comment is the honest record.
//
// `DisjointRejectsSharedElements` below still covers the disjointness body, now reached through
// `Custom`, and `RequiredIntersectionIsRefusedRatherThanMerged` covers the required-values arm --
// which no longer short-circuits to true, for the reason that test now gives.

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
    // MinMaxFeasibilityFunction is the one function whose rule depends on what it is paired with,
    // because a backward label's value is a ceiling under a threshold extension and the suffix's
    // own consumption under an accumulating one. Unpaired it answers Unspecified on purpose --
    // a refusal rather than an omission -- so the pairing is supplied here and the three arms are
    // pinned separately by MinMaxRuleFollowsTheBackwardKind.
    MinMaxFeasibilityFunction<RealResource> min_max{0.0, 100.0, true};
    min_max.set_backward_kind(BackwardKind::Threshold);
    EXPECT_EQ(min_max.merge_rule(), MergeRule::DominanceOrder);
    // Custom, not Disjoint: the body is inherited from DisjointMergeForm, so the rule only has
    // to say "call my own body" rather than naming a test a third party must interpret. The map
    // has to be the ng-route condition -- every node forbidding itself -- because that is the
    // precondition the inherited body needs; see ForbiddingSomeOtherNodeIsRefusedRatherThanMerged.
    const std::map<size_t, std::set<int>> self_forbidden{{0, {0}}, {1, {1}}};
    EXPECT_EQ(
        (IntersectionFeasibilityFunction<SetResource<int>>{self_forbidden, true}.merge_rule()),
        MergeRule::Custom);
    EXPECT_EQ((SizeFeasibilityFunction<SetResource<int>>{0U, 4U}.merge_rule()), MergeRule::Custom);

    // Four configurations deliberately declare Unspecified, which is a refusal rather than an
    // omission -- the test name's "other than Unspecified" does not extend to them, and each says
    // why in its own merge_rule() doc. All four are about is_feasible / is_reachable asking a
    // PREFIX question that a backward label's suffix cannot answer, not about the merge.
    EXPECT_EQ((IntersectionFeasibilityFunction<SetResource<int>>{values, false}.merge_rule()),
              MergeRule::Unspecified);
    EXPECT_EQ((ReachableFeasibilityFunction<SetResource<int>>{merge_rules_test::make_set({1, 2})}
                   .merge_rule()),
              MergeRule::Unspecified);

    // A non-zero size floor is the third: the cap is suffix-safe, the floor is not.
    EXPECT_EQ((SizeFeasibilityFunction<SetResource<int>>{2U, 4U}.merge_rule()),
              MergeRule::Unspecified);

    // And the fourth: forbidden values that are not the node's own. `values` above is
    // {0: {1, 2}}, i.e. node 0 forbidding nodes 1 and 2, which has no backward reading in either
    // direction of the constraint.
    EXPECT_EQ((IntersectionFeasibilityFunction<SetResource<int>>{values, true}.merge_rule()),
              MergeRule::Unspecified);
}

// ============================================================================
// MinMaxFeasibilityFunction: the rule follows the pairing
// ============================================================================
//
// The defect these cover: MinMaxFeasibilityFunction used to declare DominanceOrder for every
// pairing. DominanceOrder means `check_dominance(forward, backward)`, which reads the backward
// value as a BOUND -- the largest forward value still admissible here. That is exactly right
// under a threshold extension, where a backward label counts a capacity down from the cap, and
// meaningless under an accumulating one, where a backward label counts the suffix's own load up
// from zero. Comparing a prefix load against a suffix load tests nothing the model contains, and
// it fails in the direction a merge rule is never allowed to fail: it ACCEPTS. A prefix of 3 and
// a suffix of 3 pass `3 <= 3` under a capacity of 4 while the merged path carries 6, and the
// joiner replays refusals but never acceptances, so the infeasible path is returned as the
// optimum with a COMPLETE status.
//
// `BidirectionalValidation.AccumulatingCapacityJoinsOnlyWithinTheCap` is the end-to-end half.

/// @brief The rule is DominanceOrder under a threshold pairing, Custom under an accumulating one,
///        and Unspecified when the function has not been paired at all.
TEST(MergeRules, MinMaxRuleFollowsTheBackwardKind) {
    // Unpaired: a directly constructed function has no kind yet. Guessing one here is what the
    // defect was, so it declares Unspecified and a bidirectional solve refuses at setup.
    EXPECT_EQ((MinMaxFeasibilityFunction<RealResource>{0.0, 100.0, true}.merge_rule()),
              MergeRule::Unspecified);

    EXPECT_EQ(merge_rules_test::paired_min_max(BackwardKind::Threshold)->merge_rule(),
              MergeRule::DominanceOrder);
    EXPECT_EQ(merge_rules_test::paired_min_max(BackwardKind::Accumulate)->merge_rule(),
              MergeRule::Custom);

    // Mirror is a container's shape; a scalar window paired with one is incoherent, so it is a
    // refusal rather than a third rule.
    EXPECT_EQ(merge_rules_test::paired_min_max(BackwardKind::Mirror)->merge_rule(),
              MergeRule::Unspecified);
}

/// @brief Under an accumulating pairing the two halves ADD, and the sum is tested against the cap.
///
/// The grid straddles the cap so both outcomes are exercised, and the diagonal `f == b` is
/// included on purpose: that is the case the old DominanceOrder rule accepted for every value,
/// including the ones that overflow the cap.
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

    // The specific pair the defect accepted: equal halves that together overflow the cap.
    auto three_forward = merge_rules_test::make_real_resource(
        3.0,
        merge_rules_test::paired_min_max(BackwardKind::Accumulate, 0.0, kCapacity));
    auto three_backward = merge_rules_test::make_real_resource(
        3.0,
        merge_rules_test::paired_min_max(BackwardKind::Accumulate, 0.0, kCapacity));
    EXPECT_FALSE(three_forward->can_be_merged(*three_backward));
}

/// @brief The accumulating body is compared against the TIGHTEST cap in the model, not this
///        node's.
///
/// The merged path has to fit under the cap at every node it passes through after the join, and
/// `can_be_merged` sees two values rather than the suffix's nodes -- the same reason
/// `SizeFeasibilityFunction` caches one. Sound but conservative, which is what the replay flag
/// below is for.
TEST(MergeRules, MinMaxAccumulateUsesTheTightestCapAndSaysItIsConservative) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}}, {1, {0.0, 4.0}}};
    auto function = std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 100.0, windows);
    function->set_backward_kind(BackwardKind::Accumulate);
    EXPECT_TRUE(function->merge_refusal_may_be_conservative());

    auto forward = merge_rules_test::make_real_resource(3.0, std::move(function));

    auto other = std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 100.0, windows);
    other->set_backward_kind(BackwardKind::Accumulate);
    auto backward = merge_rules_test::make_real_resource(3.0, std::move(other));

    // Node 0's own window would allow 6; node 1's cap of 4 is the one that governs.
    EXPECT_FALSE(forward->can_be_merged(*backward));

    // A uniform window whose minimum is zero makes the sum exact, so a refusal can be trusted.
    EXPECT_FALSE(merge_rules_test::paired_min_max(BackwardKind::Accumulate, 0.0, 4.0)
                     ->merge_refusal_may_be_conservative());

    // A non-zero minimum seeds the backward label above zero, so the sum over-counts that offset.
    EXPECT_TRUE(merge_rules_test::paired_min_max(BackwardKind::Accumulate, 1.0, 4.0)
                    ->merge_refusal_may_be_conservative());

    // A threshold pairing never routes through the sum at all.
    EXPECT_FALSE(merge_rules_test::paired_min_max(BackwardKind::Threshold, 1.0, 4.0)
                     ->merge_refusal_may_be_conservative());
}

/// @brief The accumulating body refuses to answer for a pairing it is not the body of.
///
/// A threshold pairing merges through MergeRule::DominanceOrder, so `Resource::can_be_merged`
/// asks the dominance function and never calls this. Writing a second, hand-rolled `f <= b` here
/// for that case would be the second source of truth the rule dispatch removed, so the direct
/// call is a loud error instead.
TEST(MergeRules, MinMaxCanBeMergedIsTheAccumulatingBodyOnly) {
    auto threshold = merge_rules_test::paired_min_max(BackwardKind::Threshold);
    const RealResource low(1.0);
    const RealResource high(2.0);
    // The result is [[nodiscard]], and EXPECT_THROW would discard it; binding it keeps the
    // expression an ordinary use rather than a suppressed warning.
    EXPECT_THROW(
        { [[maybe_unused]] const bool merged = threshold->can_be_merged(low, high); },
        std::logic_error);

    auto accumulate = merge_rules_test::paired_min_max(BackwardKind::Accumulate, 0.0, 4.0);
    EXPECT_TRUE(accumulate->can_be_merged(low, high));
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
            //
            // The pairing is supplied explicitly: `f <= b` is the THRESHOLD reading, which is the
            // one the deleted body implemented and the only one it was ever right for. Under an
            // accumulating pairing the same two values add instead -- see
            // MinMaxAccumulateAddsTheTwoHalves.
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

// DisjointMergeForm used STANDALONE -- no merge_rule() override.
//
// IntersectionFeasibilityFunction is the form's only production user and it narrows merge_rule()
// to short-circuit the required-values case, so the form's own `return Custom` is never reached
// through it. A future container resource with no such distinction is the case this covers, and
// without it the form's declaration is uncovered code that reads as dead.
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

    // The form supplies the declaration as well as the body: Custom, because the body is here.
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
