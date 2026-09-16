// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Phase 3: backward dominance, and the derivation that keeps it honest.
//
// Backward, a label stores a *deadline* rather than a consumption, so "a later deadline is
// better" and the comparison flips -- but only for threshold resources. The flip is never a new
// comparison, just an argument swap: check_back_dominance(a, b) == check_dominance(b, a). Which
// resources flip is *derived* from the extension function's backward_kind() in
// ResourceGraph::add_resource, so a resource that inverts its extension can never forget to
// reverse its dominance.

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace backward_dominance_test {

using RealComposition = ResourceTypeComposition<RealResource>;

/// @brief Adds a RealResource component and returns a borrowed pointer to its dominance function.
///
/// add_resource takes ownership, but the graph's factory keeps the prototype alive for the
/// graph's lifetime, so the returned pointer stays valid. This is how the tests inspect the
/// derived flag without adding a public accessor nobody outside add_resource should need.
inline ValueDominanceFunction<RealResource>* add_real_resource(
    ResourceGraph<RealResource>* graph,
    std::unique_ptr<ExtensionFunction<RealResource>> extension) {
    auto dominance = std::make_unique<ValueDominanceFunction<RealResource>>();
    auto* borrowed = dominance.get();
    graph->add_resource<RealResource>(std::move(extension),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<TrivialCostFunction<RealResource>>(),
                                      std::move(dominance));
    return borrowed;
}

inline std::map<size_t, std::pair<double, double>> windows() {
    return {{0, {0.0, 100.0}}, {1, {0.0, 100.0}}};
}

/// @brief Builds a one-component RealResource composition holding @p value.
inline std::unique_ptr<Resource<RealResource>> make_component(
    double value, ValueDominanceFunction<RealResource>** borrowed_out = nullptr) {
    auto dominance = std::make_unique<ValueDominanceFunction<RealResource>>();
    if (borrowed_out != nullptr) {
        *borrowed_out = dominance.get();
    }
    return std::make_unique<Resource<RealResource>>(
        RealResource(value),
        std::move(dominance),
        std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<TrivialCostFunction<RealResource>>());
}

/// @brief Builds a two-component composition; component 1's dominance is reversed.
///
/// Mirrors a real model where cost accumulates (unreversed) while a time window is a deadline
/// (reversed) -- both inside the same composition.
inline std::unique_ptr<Resource<RealComposition>> make_mixed_composition(double unreversed_value,
                                                                         double reversed_value) {
    ValueDominanceFunction<RealResource>* reversed_dominance = nullptr;
    auto unreversed_component = make_component(unreversed_value);
    auto reversed_component = make_component(reversed_value, &reversed_dominance);
    reversed_dominance->set_backward_reversed(true);

    std::tuple<std::vector<std::unique_ptr<Resource<RealResource>>>> components;
    std::get<0>(components).push_back(std::move(unreversed_component));
    std::get<0>(components).push_back(std::move(reversed_component));

    return std::make_unique<Resource<RealComposition>>(
        std::move(components),
        std::make_unique<CompositionDominanceFunction<RealResource>>(),
        std::make_unique<CompositionFeasibilityFunction<RealResource>>(),
        std::make_unique<CompositionCostFunction<RealResource>>(),
        0);
}

}  // namespace backward_dominance_test

// ============================================================================
// 1. Symmetric (Accumulate) resources are bit-identical
// ============================================================================

/// @brief An Accumulate resource keeps the forward comparison, exactly, including the tolerance.
TEST(BackwardDominance, AccumulateIsBitIdenticalToForward) {
    ResourceGraph<RealResource> graph;
    auto* dominance = backward_dominance_test::add_real_resource(
        &graph,
        std::make_unique<AdditionExtensionFunction<RealResource>>());

    ASSERT_FALSE(dominance->is_backward_reversed());

    const std::vector<double> grid{-5.0, 0.0, 1.0, 2.5, 10.0};
    const std::vector<double> deltas{0.0, 0.5, 3.0};
    for (const double lhs : grid) {
        for (const double rhs : grid) {
            const RealResource left(lhs);
            const RealResource right(rhs);
            SCOPED_TRACE("lhs=" + std::to_string(lhs) + " rhs=" + std::to_string(rhs));

            EXPECT_EQ(dominance->check_back_dominance(left, right),
                      dominance->check_dominance(left, right));

            for (const double delta : deltas) {
                EXPECT_EQ(dominance->fast_check_back_dominance(left, right, delta),
                          dominance->fast_check_dominance(left, right, delta));
            }
        }
    }
}

// ============================================================================
// 2. Threshold resources reverse
// ============================================================================

/// @brief A time window is a deadline, so its dominance reverses: later is more permissive.
TEST(BackwardDominance, ThresholdReverses) {
    ResourceGraph<RealResource> graph;
    auto* dominance = backward_dominance_test::add_real_resource(
        &graph,
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(
            backward_dominance_test::windows()));

    EXPECT_TRUE(dominance->is_backward_reversed());

    const RealResource early(10.0);
    const RealResource late(50.0);

    // Forward: the smaller value dominates. Backward: the larger one does.
    EXPECT_TRUE(dominance->check_dominance(early, late));
    EXPECT_FALSE(dominance->check_dominance(late, early));
    EXPECT_FALSE(dominance->check_back_dominance(early, late));
    EXPECT_TRUE(dominance->check_back_dominance(late, early));

    // The reversal is exactly an argument swap.
    EXPECT_EQ(dominance->check_back_dominance(early, late),
              dominance->check_dominance(late, early));
    EXPECT_EQ(dominance->check_back_dominance(late, early),
              dominance->check_dominance(early, late));
}

// ============================================================================
// 3. Mirror resources do NOT reverse
// ============================================================================

/// @brief An ng-set stores the set rather than its complement; complementation would flip the
///        order once and the reversal would flip it again, so the two cancel.
TEST(BackwardDominance, MirrorDoesNotReverse) {
    ResourceGraph<SetResource<int>> graph;
    std::map<size_t, std::set<int>> ng_map{{0, {1, 2}}, {1, {0, 2}}};

    auto dominance = std::make_unique<InclusionDominanceFunction<SetResource<int>>>();
    auto* borrowed = dominance.get();
    graph.add_resource<SetResource<int>>(
        std::make_unique<NgPathExtensionFunction<SetResource<int>>>(ng_map),
        std::make_unique<TrivialFeasibilityFunction<SetResource<int>>>(),
        std::make_unique<TrivialCostFunction<SetResource<int>>>(),
        std::move(dominance));

    EXPECT_FALSE(borrowed->is_backward_reversed());

    // NOTE: not named `small` -- <rpcndr.h>, pulled in via <windows.h>, defines that as a macro
    // for `char`, so `SetResource<int> small;` does not compile on Windows.
    SetResource<int> small_set;
    small_set.set_value(std::set<int>{1});
    SetResource<int> big_set;
    big_set.set_value(std::set<int>{1, 2});

    // A smaller remembered set is more permissive, in both directions.
    EXPECT_EQ(borrowed->check_back_dominance(small_set, big_set),
              borrowed->check_dominance(small_set, big_set));
    EXPECT_EQ(borrowed->check_back_dominance(big_set, small_set),
              borrowed->check_dominance(big_set, small_set));
    EXPECT_TRUE(borrowed->check_back_dominance(small_set, big_set));
    EXPECT_FALSE(borrowed->check_back_dominance(big_set, small_set));
}

// ============================================================================
// 4. The tolerance survives the swap
// ============================================================================

/// @brief fast_check_back_dominance(l, r, d) accepts exactly when l >= r - d.
///
/// This is the part that looks like it should break, and does not:
///   forward   fast_check_dominance(l, r, d) = l <= r + d
///   swapped   fast_check_dominance(r, l, d) = r <= l + d  <=>  l >= r - d
TEST(BackwardDominance, ToleranceSurvivesTheSwap) {
    ResourceGraph<RealResource> graph;
    auto* dominance = backward_dominance_test::add_real_resource(
        &graph,
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(
            backward_dominance_test::windows()));
    ASSERT_TRUE(dominance->is_backward_reversed());

    const std::vector<double> grid{0.0, 4.5, 5.0, 5.5, 7.0};
    const std::vector<double> deltas{0.0, 1.0, 2.5};
    for (const double lhs : grid) {
        for (const double rhs : grid) {
            for (const double delta : deltas) {
                SCOPED_TRACE("lhs=" + std::to_string(lhs) + " rhs=" + std::to_string(rhs) +
                             " delta=" + std::to_string(delta));
                const RealResource left(lhs);
                const RealResource right(rhs);
                // Accepts exactly when l >= r - delta.
                EXPECT_EQ(dominance->fast_check_back_dominance(left, right, delta),
                          left.geq(rhs - delta));
            }
        }
    }

    // At least one input where the reversed answer differs from the unreversed one -- otherwise
    // the reversed branch would be indistinguishable from the forward call and the test would
    // pass even if the swap were dropped.
    const RealResource left(5.0);
    const RealResource right(7.0);
    constexpr double kDelta = 1.0;
    EXPECT_TRUE(dominance->fast_check_dominance(left, right, kDelta));        // 5 <= 7 + 1
    EXPECT_FALSE(dominance->fast_check_back_dominance(left, right, kDelta));  // 5 >= 7 - 1 fails
    EXPECT_TRUE(dominance->fast_check_back_dominance(right, left, kDelta));   // 7 >= 5 - 1
}

// ============================================================================
// 5. Composition fan-out
// ============================================================================

/// @brief check_back_dominance ANDs the per-component *backward* answers.
///
/// Component 0 is unreversed, component 1 is reversed -- the shape of a real model where cost
/// accumulates while a time window is a deadline.
TEST(BackwardDominance, CompositionFansOutPerComponentDirection) {
    // lhs = (1, 5), rhs = (2, 3).
    //   forward : (1 <= 2) && (5 <= 3) = false
    //   backward: (1 <= 2) && (3 <= 5) = true    <- component 1 reversed
    auto lhs = backward_dominance_test::make_mixed_composition(1.0, 5.0);
    auto rhs = backward_dominance_test::make_mixed_composition(2.0, 3.0);

    EXPECT_FALSE(*lhs <= *rhs);
    EXPECT_TRUE(lhs->back_dominates(*rhs));

    // The unreversed component still gates the AND: make it fail and the whole thing fails,
    // however permissive the reversed component is.
    auto blocked = backward_dominance_test::make_mixed_composition(9.0, 5.0);
    EXPECT_FALSE(blocked->back_dominates(*rhs));
    EXPECT_FALSE(*blocked <= *rhs);

    // Both components satisfied backward.
    auto both = backward_dominance_test::make_mixed_composition(1.0, 9.0);
    EXPECT_TRUE(both->back_dominates(*rhs));
}

// ============================================================================
// 6. The derivation is applied
// ============================================================================

/// @brief Every BackwardKind maps to the flag the table in the design prescribes.
///
/// This is the test that protects the design: it is what fails if someone adds a resource and the
/// wiring in add_resource is skipped.
TEST(BackwardDominance, DerivationMatchesBackwardKind) {
    ResourceGraph<RealResource> graph;

    // Accumulate -> not reversed.
    auto* accumulate = backward_dominance_test::add_real_resource(
        &graph,
        std::make_unique<AdditionExtensionFunction<RealResource>>());
    EXPECT_FALSE(accumulate->is_backward_reversed());

    // Threshold -> reversed.
    auto* threshold = backward_dominance_test::add_real_resource(
        &graph,
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(
            backward_dominance_test::windows()));
    EXPECT_TRUE(threshold->is_backward_reversed());

    // Threshold, the other one.
    auto* budget = backward_dominance_test::add_real_resource(
        &graph,
        std::make_unique<BudgetExtensionFunction<RealResource>>());
    EXPECT_TRUE(budget->is_backward_reversed());

    // Mirror -> not reversed.
    ResourceGraph<SetResource<int>> set_graph;
    std::map<size_t, std::set<int>> ng_map{{0, {1, 2}}};
    auto mirror = std::make_unique<InclusionDominanceFunction<SetResource<int>>>();
    auto* mirror_borrowed = mirror.get();
    set_graph.add_resource<SetResource<int>>(
        std::make_unique<NgPathExtensionFunction<SetResource<int>>>(ng_map),
        std::make_unique<TrivialFeasibilityFunction<SetResource<int>>>(),
        std::make_unique<TrivialCostFunction<SetResource<int>>>(),
        std::move(mirror));
    EXPECT_FALSE(mirror_borrowed->is_backward_reversed());
}

/// @brief The overload taking a default_resource_initializer derives the flag too.
///
/// Missing this overload would produce a resource whose dominance silently does not reverse --
/// the exact failure the derivation exists to prevent.
TEST(BackwardDominance, DerivationAppliesToBothAddResourceOverloads) {
    ResourceGraph<RealResource> graph;

    auto dominance = std::make_unique<ValueDominanceFunction<RealResource>>();
    auto* borrowed = dominance.get();
    graph.add_resource<RealResource>(std::make_unique<TimeWindowExtensionFunction<RealResource>>(
                                         backward_dominance_test::windows()),
                                     std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                     std::make_unique<TrivialCostFunction<RealResource>>(),
                                     std::move(dominance),
                                     std::make_tuple(0.0));

    EXPECT_TRUE(borrowed->is_backward_reversed());
}

/// @brief A component reporting Unspecified derives reversed == false.
///
/// TimeWindowExtensionFunction<UIntResource> reports Unspecified because an unsigned value type
/// cannot represent "this deadline cannot be met" (see phase 2). It is a threshold resource
/// sitting in the unreversed bucket, which would prune the *better* backward labels -- inert only
/// because a bidirectional solve refuses to start on an Unspecified component. Do not "fix" this
/// by mapping Unspecified to reversed; this test exists so that if that refusal is ever relaxed,
/// the failure is loud rather than a silent loss of optimal solutions.
TEST(BackwardDominance, UnspecifiedDerivesUnreversed) {
    ResourceGraph<UIntResource> graph;
    std::map<size_t, std::pair<unsigned int, unsigned int>> uint_windows{{0, {0U, 100U}}};

    auto dominance = std::make_unique<ValueDominanceFunction<UIntResource>>();
    auto* borrowed = dominance.get();
    auto extension = std::make_unique<TimeWindowExtensionFunction<UIntResource>>(uint_windows);
    ASSERT_EQ(extension->backward_kind(), BackwardKind::Unspecified);

    graph.add_resource<UIntResource>(std::move(extension),
                                     std::make_unique<TrivialFeasibilityFunction<UIntResource>>(),
                                     std::make_unique<TrivialCostFunction<UIntResource>>(),
                                     std::move(dominance));

    EXPECT_FALSE(borrowed->is_backward_reversed());
}
