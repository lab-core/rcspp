// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Direction-aware label containers.
//
// Dominance lives in the containers, so they must know the direction: backward, a later deadline
// is better, and a forward-comparing container would discard the better labels. Labels use a
// composition resource, the only instantiation the library uses.

#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace directional_containers_test {

using RealComposition = ResourceTypeComposition<RealResource>;
using SetComposition = ResourceTypeComposition<SetResource<int>>;

constexpr double kEarly = 30.0;
constexpr double kLate = 70.0;

/// @brief Builds a one-component composition label whose scalar value is @p value.
///
/// @param label_id  Identifier for the label.
/// @param value     The component value.
/// @param reversed  Whether the component's dominance reverses backward (a Threshold resource).
inline std::unique_ptr<Label<RealComposition>> make_real_label(size_t label_id, double value,
                                                               bool reversed) {
    auto dominance = std::make_unique<ValueDominanceFunction<RealResource>>();
    dominance->set_backward_reversed(reversed);
    auto component = std::make_unique<Resource<RealResource>>(
        RealResource(value),
        std::move(dominance),
        std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<TrivialCostFunction<RealResource>>());

    std::tuple<std::vector<std::unique_ptr<Resource<RealResource>>>> components;
    std::get<0>(components).push_back(std::move(component));

    auto resource = std::make_unique<Resource<RealComposition>>(
        std::move(components),
        std::make_unique<CompositionDominanceFunction<RealResource>>(),
        std::make_unique<CompositionFeasibilityFunction<RealResource>>(),
        std::make_unique<CompositionCostFunction<RealResource>>(),
        0);
    return std::make_unique<Label<RealComposition>>(label_id, std::move(resource));
}

/// @brief Builds a one-component composition label over a container resource.
///
/// A set resource is a container shape: the smaller remembered set wins in both directions.
inline std::unique_ptr<Label<SetComposition>> make_set_label(size_t label_id,
                                                             const std::set<int>& values) {
    SetResource<int> value;
    value.set_value(values);
    auto component = std::make_unique<Resource<SetResource<int>>>(
        value,
        std::make_unique<InclusionDominanceFunction<SetResource<int>>>(),
        std::make_unique<TrivialFeasibilityFunction<SetResource<int>>>(),
        std::make_unique<TrivialCostFunction<SetResource<int>>>());

    std::tuple<std::vector<std::unique_ptr<Resource<SetResource<int>>>>> components;
    std::get<0>(components).push_back(std::move(component));

    auto resource = std::make_unique<Resource<SetComposition>>(
        std::move(components),
        std::make_unique<CompositionDominanceFunction<SetResource<int>>>(),
        std::make_unique<CompositionFeasibilityFunction<SetResource<int>>>(),
        std::make_unique<CompositionCostFunction<SetResource<int>>>(),
        0);
    return std::make_unique<Label<SetComposition>>(label_id, std::move(resource));
}

}  // namespace directional_containers_test

// ============================================================================
// Forward is unchanged
// ============================================================================

/// @brief A forward LabelList keeps the smaller value and drops the larger.
TEST(DirectionalContainers, ForwardListKeepsTheEarlierLabel) {
    namespace dct = directional_containers_test;

    LabelList<dct::RealComposition> list;
    auto early = dct::make_real_label(0, dct::kEarly, /*reversed=*/false);
    auto late = dct::make_real_label(1, dct::kLate, /*reversed=*/false);

    list.add_label(early.get());

    // The later label is dominated by the stored earlier one.
    EXPECT_TRUE(list.is_dominated(*late));
    // ... and the earlier one is not dominated by anything.
    EXPECT_FALSE(list.is_dominated(*early));

    // Inserting the earlier label into a list holding the later one evicts it.
    LabelList<dct::RealComposition> other;
    other.add_label(late.get());
    EXPECT_EQ(other.remove_dominated_labels(*early), 1U);
    EXPECT_TRUE(other.get_labels().empty());
}

// ============================================================================
// Backward prunes the other way
// ============================================================================

/// @brief A backward LabelList over a Threshold resource keeps the LATER label.
TEST(DirectionalContainers, BackwardKeepsTheLaterLabel) {
    namespace dct = directional_containers_test;

    // Premise: this resource really does reverse backward.
    ValueDominanceFunction<RealResource> probe;
    probe.set_backward_reversed(true);
    ASSERT_TRUE(probe.is_backward_reversed());
    ASSERT_NE(probe.check_dominance(RealResource(dct::kEarly), RealResource(dct::kLate)),
              probe.check_back_dominance(RealResource(dct::kEarly), RealResource(dct::kLate)));

    LabelList<dct::RealComposition, BackwardDirection> list;
    auto early = dct::make_real_label(0, dct::kEarly, /*reversed=*/true);
    auto late = dct::make_real_label(1, dct::kLate, /*reversed=*/true);

    list.add_label(late.get());

    // Backward, a later deadline is more permissive, so the later label dominates.
    EXPECT_TRUE(list.is_dominated(*early));
    EXPECT_FALSE(list.is_dominated(*late));

    // Inserting the later label into a list holding the earlier one evicts it.
    LabelList<dct::RealComposition, BackwardDirection> other;
    other.add_label(early.get());
    EXPECT_EQ(other.remove_dominated_labels(*late), 1U);
    EXPECT_TRUE(other.get_labels().empty());
}

/// @brief Side by side: the same two labels, opposite survivors.
TEST(DirectionalContainers, SameLabelsOppositeSurvivors) {
    namespace dct = directional_containers_test;

    auto early = dct::make_real_label(0, dct::kEarly, /*reversed=*/true);
    auto late = dct::make_real_label(1, dct::kLate, /*reversed=*/true);

    LabelList<dct::RealComposition> forward;
    forward.add_label(early.get());
    forward.add_label(late.get());
    forward.remove_dominated_labels(*early);
    ASSERT_EQ(forward.get_labels().size(), 1U);
    EXPECT_EQ(forward.get_labels().front()->id, 0U);  // the earlier one survives

    LabelList<dct::RealComposition, BackwardDirection> backward;
    backward.add_label(early.get());
    backward.add_label(late.get());
    backward.remove_dominated_labels(*late);
    ASSERT_EQ(backward.get_labels().size(), 1U);
    EXPECT_EQ(backward.get_labels().front()->id, 1U);  // the later one survives
}

// ============================================================================
// Accumulate and the container kinds behave the same in both directions
// ============================================================================

/// @brief A backward LabelList over an Accumulate resource behaves like the forward one.
///
/// Cost does not reverse: the cheaper label wins either way.
TEST(DirectionalContainers, BackwardMatchesForwardOnAccumulate) {
    namespace dct = directional_containers_test;

    auto cheap = dct::make_real_label(0, 1.0, /*reversed=*/false);
    auto costly = dct::make_real_label(1, 5.0, /*reversed=*/false);

    LabelList<dct::RealComposition> forward;
    LabelList<dct::RealComposition, BackwardDirection> backward;
    forward.add_label(cheap.get());
    backward.add_label(cheap.get());

    EXPECT_EQ(forward.is_dominated(*costly), backward.is_dominated(*costly));
    EXPECT_TRUE(backward.is_dominated(*costly));
    EXPECT_FALSE(backward.is_dominated(*cheap));
}

/// @brief A backward LabelList over a container resource behaves like the forward one.
///
/// The library stores the visited set, not its complement, so the smaller set wins both ways.
TEST(DirectionalContainers, BackwardMatchesForwardOnMirror) {
    namespace dct = directional_containers_test;

    auto small_set = dct::make_set_label(0, {1});
    auto big_set = dct::make_set_label(1, {1, 2});

    LabelList<dct::SetComposition> forward;
    LabelList<dct::SetComposition, BackwardDirection> backward;
    forward.add_label(small_set.get());
    backward.add_label(small_set.get());

    EXPECT_EQ(forward.is_dominated(*big_set), backward.is_dominated(*big_set));
    EXPECT_TRUE(backward.is_dominated(*big_set));
    EXPECT_FALSE(backward.is_dominated(*small_set));
}

// ============================================================================
// copy() preserves the direction
// ============================================================================

/// @brief copy() returns the same instantiation, direction included.
///
/// Per-node containers are built with copy(), so losing the direction would break backward pruning.
TEST(DirectionalContainers, CopyPreservesDirection) {
    namespace dct = directional_containers_test;

    LabelList<dct::RealComposition, BackwardDirection> backward;
    auto copied = backward.copy();
    static_assert(
        std::is_same_v<decltype(copied), LabelList<dct::RealComposition, BackwardDirection>>,
        "copy() must preserve the direction");

    // The copy also prunes the backward way.
    auto early = dct::make_real_label(0, dct::kEarly, /*reversed=*/true);
    auto late = dct::make_real_label(1, dct::kLate, /*reversed=*/true);
    copied.add_label(late.get());
    EXPECT_TRUE(copied.is_dominated(*early));

    // The forward container's copy stays forward.
    LabelList<dct::RealComposition> forward;
    auto forward_copy = forward.copy();
    static_assert(std::is_same_v<decltype(forward_copy), LabelList<dct::RealComposition>>,
                  "the default direction must survive copy() too");
}

// ============================================================================
// is_back_lower follows the flag
// ============================================================================

/// @brief is_back_lower equals is_lower unreversed, and the swapped call reversed.
TEST(DirectionalContainers, IsBackLowerFollowsTheFlag) {
    constexpr double kDelta = 1.0;

    auto make = [](double value, bool reversed) {
        auto dominance = std::make_unique<ValueDominanceFunction<RealResource>>();
        dominance->set_backward_reversed(reversed);
        return std::make_unique<Resource<RealResource>>(
            RealResource(value),
            std::move(dominance),
            std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
            std::make_unique<TrivialCostFunction<RealResource>>());
    };

    // Unreversed: identical to is_lower.
    auto lhs_fwd = make(5.0, /*reversed=*/false);
    auto rhs_fwd = make(7.0, /*reversed=*/false);
    EXPECT_EQ(lhs_fwd->is_back_lower(*rhs_fwd, kDelta), lhs_fwd->is_lower(*rhs_fwd, kDelta));
    EXPECT_TRUE(lhs_fwd->is_back_lower(*rhs_fwd, kDelta));  // 5 <= 7 + 1

    // Reversed: the swapped call, so it accepts exactly when lhs >= rhs - delta.
    auto lhs_bwd = make(5.0, /*reversed=*/true);
    auto rhs_bwd = make(7.0, /*reversed=*/true);
    EXPECT_FALSE(lhs_bwd->is_back_lower(*rhs_bwd, kDelta));  // 5 >= 7 - 1 is false
    EXPECT_TRUE(rhs_bwd->is_back_lower(*lhs_bwd, kDelta));   // 7 >= 5 - 1

    // The two flag states must disagree.
    EXPECT_NE(lhs_fwd->is_back_lower(*rhs_fwd, kDelta), lhs_bwd->is_back_lower(*rhs_bwd, kDelta));
}

// ============================================================================
// LabelBuckets defaults to forward
// ============================================================================

/// @brief LabelBuckets defaults to the forward direction.
TEST(DirectionalContainers, ForwardBucketsUnchanged) {
    namespace dct = directional_containers_test;
    using Buckets = LabelBuckets<RealResource, RealResource, dct::RealComposition>;

    static_assert(std::is_base_of_v<LabelList<dct::RealComposition, ForwardDirection>, Buckets>,
                  "LabelBuckets must default to the forward direction");
    static_assert(
        std::is_same_v<
            Buckets,
            LabelBuckets<RealResource, RealResource, dct::RealComposition, ForwardDirection>>,
        "the Dir parameter must be defaulted, not required");

    // copy() keeps the direction.
    Buckets buckets(/*range_buckets=*/10, /*bucket_resource_index=*/0, /*sort_resource_index=*/0);
    auto copied = buckets.copy();
    static_assert(std::is_same_v<decltype(copied), Buckets>,
                  "LabelBuckets::copy() must preserve the direction");
}
