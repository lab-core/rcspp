// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The half-way policy, and the checks that the critical resource is a valid clock.
//
// A non-monotone clock can cross H more than once, so both searches may discard a valid path.
// A failed check therefore disables the bound (correct but slower) instead of throwing.

#include <gtest/gtest.h>

#include <limits>
#include <map>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace half_way_test {

constexpr double kTolerance = 1e-12;
constexpr double kUpperBound = 100.0;

/// @brief A small probe set, enough to catch a sign error.
inline std::vector<double> probes(double upper_bound = kUpperBound) {
    return {0.0, upper_bound / 2.0};
}

/// @brief A two-node graph whose single component uses @p extension.
template <typename ExtensionFn, typename FeasibilityFn>
std::unique_ptr<ResourceGraph<RealResource>> two_node_graph(std::unique_ptr<ExtensionFn> extension,
                                                            std::unique_ptr<FeasibilityFn> feas,
                                                            double arc_value) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::move(extension),
                                      std::move(feas),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource>(std::make_tuple(arc_value), 0, 1);
    return graph;
}

}  // namespace half_way_test

// ============================================================================
// H derivation and the two predicates
// ============================================================================

/// @brief H is R/2 when unset, and the explicit value otherwise.
TEST(HalfWayPolicy, DerivesOrHonoursH) {
    const HalfWayPolicy derived(/*half_way_point=*/0.0, /*resource_upper_bound=*/100.0);
    EXPECT_TRUE(derived.enabled());
    EXPECT_NEAR(derived.h(), 50.0, half_way_test::kTolerance);

    const HalfWayPolicy explicit_h(/*half_way_point=*/30.0, /*resource_upper_bound=*/100.0);
    EXPECT_TRUE(explicit_h.enabled());
    EXPECT_NEAR(explicit_h.h(), 30.0, half_way_test::kTolerance);  // explicit wins over R/2
}

/// @brief Forward stops above H, backward stops below it, and neither stops exactly at H.
///
/// A label exactly on H must survive in both directions, or a path crossing there is lost.
TEST(HalfWayPolicy, StopPredicatesAndTheBoundary) {
    const HalfWayPolicy policy(/*half_way_point=*/0.0, /*resource_upper_bound=*/100.0);
    ASSERT_NEAR(policy.h(), 50.0, half_way_test::kTolerance);

    EXPECT_FALSE(policy.should_stop_forward(49.0));
    EXPECT_TRUE(policy.should_stop_forward(51.0));

    EXPECT_FALSE(policy.should_stop_backward(51.0));
    EXPECT_TRUE(policy.should_stop_backward(49.0));

    // Exactly at H: neither direction discards.
    EXPECT_FALSE(policy.should_stop_forward(50.0));
    EXPECT_FALSE(policy.should_stop_backward(50.0));
}

/// @brief A disabled policy stops nothing, in either direction, for any input.
TEST(HalfWayPolicy, DisabledStopsNothing) {
    HalfWayPolicy policy(/*half_way_point=*/0.0, /*resource_upper_bound=*/100.0);
    ASSERT_TRUE(policy.enabled());

    policy.disable();
    EXPECT_FALSE(policy.enabled());

    for (const double value : {-1000.0, 0.0, 49.0, 50.0, 51.0, 1000.0}) {
        EXPECT_FALSE(policy.should_stop_forward(value));
        EXPECT_FALSE(policy.should_stop_backward(value));
    }
}

/// @brief With no explicit H and no finite R there is no middle to aim at, so the bound is off.
TEST(HalfWayPolicy, UnboundedResourceWithoutExplicitHIsDisabled) {
    const HalfWayPolicy infinite(0.0, std::numeric_limits<double>::infinity());
    EXPECT_FALSE(infinite.enabled());
    EXPECT_FALSE(infinite.should_stop_forward(1e9));
    EXPECT_FALSE(infinite.should_stop_backward(-1e9));

    const HalfWayPolicy zero_bound(0.0, 0.0);
    EXPECT_FALSE(zero_bound.enabled());

    // An explicit H still enables it.
    const HalfWayPolicy explicit_h(40.0, std::numeric_limits<double>::infinity());
    EXPECT_TRUE(explicit_h.enabled());
    EXPECT_NEAR(explicit_h.h(), 40.0, half_way_test::kTolerance);
}

// ============================================================================
// Monotonicity validation
// ============================================================================

/// @brief A plain additive time resource is monotone.
TEST(HalfWayPolicy, MonotoneResourceAccepted) {
    auto graph =
        half_way_test::two_node_graph(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      /*arc_value=*/15.0);

    const auto probes = half_way_test::probes();
    EXPECT_TRUE(critical_resource_is_monotone<RealResource>(*graph, 0, probes));
}

/// @brief A negative consumption on the critical slot is not monotone.
///
/// E.g. a reduced-cost slot during pricing, which must not be used as the clock.
TEST(HalfWayPolicy, NegativeConsumptionRejected) {
    auto graph =
        half_way_test::two_node_graph(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      /*arc_value=*/-5.0);

    const auto probes = half_way_test::probes();
    EXPECT_FALSE(critical_resource_is_monotone<RealResource>(*graph, 0, probes));
}

/// @brief A monotone-but-clamped extension is ACCEPTED, though its stored arc value is negative.
///
/// The extension clamps up to the opening time of 60, so the result still rises from every probe.
/// This is why monotonicity is probed rather than read from the stored arc value.
TEST(HalfWayPolicy, ClampedExtensionIsAccepted) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 1000.0}}, {1, {60.0, 1000.0}}};

    auto graph = half_way_test::two_node_graph(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        /*arc_value=*/-5.0);

    const auto probes = half_way_test::probes();
    EXPECT_TRUE(critical_resource_is_monotone<RealResource>(*graph, 0, probes));
}

/// @brief Once one arc fails, the remaining arcs are skipped rather than probed.
TEST(HalfWayPolicy, StopsProbingAfterTheFirstFailure) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource>(std::make_tuple(-5.0), 0, 1);  // fails first
    graph->add_arc<RealResource>(std::make_tuple(5.0), 1, 2);   // never probed

    const auto probes = half_way_test::probes();
    EXPECT_FALSE(critical_resource_is_monotone<RealResource>(*graph, 0, probes));
}

/// @brief A resource type absent from the pack disables the bound rather than failing.
TEST(HalfWayPolicy, AbsentCriticalTypeIsNotMonotone) {
    auto graph =
        half_way_test::two_node_graph(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      /*arc_value=*/15.0);

    const auto probes = half_way_test::probes();
    // IntResource is not part of this graph's pack.
    EXPECT_FALSE((critical_resource_is_monotone<IntResource>(*graph, 0, probes)));
}

// ============================================================================
// Dominance-order validation
// ============================================================================

/// @brief A clock compared by value is accepted.
///
/// A dominator's clock is then never above the evicted label's, so it stays a join candidate.
TEST(HalfWayPolicy, ValueDominanceOnTheClockIsAccepted) {
    auto graph =
        half_way_test::two_node_graph(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      /*arc_value=*/15.0);

    EXPECT_TRUE((critical_resource_dominance_is_increasing<RealResource, RealResource>(
        *graph,
        /*critical_resource_index=*/0)));
}

/// @brief A clock with a trivial dominance is rejected.
///
/// A dominator above H could evict a label below H, leaving neither available to the join.
TEST(HalfWayPolicy, TrivialDominanceOnTheClockIsRejected) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<TrivialDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource>(std::make_tuple(15.0), 0, 1);

    EXPECT_FALSE((critical_resource_dominance_is_increasing<RealResource, RealResource>(
        *graph,
        /*critical_resource_index=*/0)));
}

/// @brief An absent critical type has no dominance order, like the monotonicity probe.
TEST(HalfWayPolicy, AbsentCriticalTypeHasNoDominanceOrder) {
    auto graph =
        half_way_test::two_node_graph(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      /*arc_value=*/15.0);

    // IntResource is not part of this graph's pack.
    EXPECT_FALSE((critical_resource_dominance_is_increasing<IntResource, RealResource>(
        *graph,
        /*critical_resource_index=*/0)));
}

// ============================================================================
// The failure response
// ============================================================================

/// @brief A failed check disables the bound and does not throw.
TEST(HalfWayPolicy, RejectionDisablesRatherThanThrows) {
    auto graph =
        half_way_test::two_node_graph(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      /*arc_value=*/-5.0);

    HalfWayPolicy policy(0.0, half_way_test::kUpperBound);
    ASSERT_TRUE(policy.enabled());

    const auto probes = half_way_test::probes();
    EXPECT_NO_THROW({
        if (!critical_resource_is_monotone<RealResource>(*graph, 0, probes)) {
            policy.disable();
        }
    });

    EXPECT_FALSE(policy.enabled());
    // With the bound off, every label is kept.
    EXPECT_FALSE(policy.should_stop_forward(1e9));
    EXPECT_FALSE(policy.should_stop_backward(-1e9));
}

TEST(HalfWayPolicy, FixedAtIsInForceWhateverTheValue) {
    const auto at_zero = HalfWayPolicy::fixed_at(0.0);
    EXPECT_TRUE(at_zero.enabled());
    EXPECT_EQ(at_zero.h(), 0.0);
    EXPECT_TRUE(at_zero.should_stop_forward(0.5));
    EXPECT_FALSE(at_zero.should_stop_forward(0.0));

    const auto negative = HalfWayPolicy::fixed_at(-3.0);
    EXPECT_TRUE(negative.enabled());
    EXPECT_EQ(negative.h(), -3.0);
}

TEST(HalfWayBounds, StartOpen) {
    const HalfWayBounds bounds;
    EXPECT_EQ(bounds.forward_bound(), std::numeric_limits<double>::infinity());
    EXPECT_EQ(bounds.backward_bound(), -std::numeric_limits<double>::infinity());
    EXPECT_FALSE(bounds.should_stop_forward(1e300));
    EXPECT_FALSE(bounds.should_stop_backward(-1e300));
}

TEST(HalfWayBounds, ForwardProgressRaisesTheBackwardBoundOnly) {
    HalfWayBounds bounds;
    bounds.after_forward(3.0);
    EXPECT_EQ(bounds.backward_bound(), 3.0);
    bounds.after_forward(1.0);  // never lowered
    EXPECT_EQ(bounds.backward_bound(), 3.0);
    EXPECT_EQ(bounds.forward_bound(), std::numeric_limits<double>::infinity());
}

TEST(HalfWayBounds, BackwardProgressLowersTheForwardBoundOnly) {
    HalfWayBounds bounds;
    bounds.after_backward(40.0);
    EXPECT_EQ(bounds.forward_bound(), 40.0);
    bounds.after_backward(50.0);  // never raised
    EXPECT_EQ(bounds.forward_bound(), 40.0);
    EXPECT_EQ(bounds.backward_bound(), -std::numeric_limits<double>::infinity());
}

TEST(HalfWayBounds, ABoundNeverCrossesTheOther) {
    HalfWayBounds bounds;
    bounds.after_backward(10.0);  // H_F = 10
    bounds.after_forward(25.0);   // H_B = min(25, 10) = 10
    EXPECT_EQ(bounds.backward_bound(), 10.0);
    bounds.after_backward(5.0);  // H_F = max(5, 10) = 10
    EXPECT_EQ(bounds.forward_bound(), 10.0);
}

TEST(HalfWayBounds, RandomUpdatesKeepTheOrderAndTheDirections) {
    std::mt19937 generator(42);  // NOLINT(cert-msc51-cpp): deterministic on purpose
    std::uniform_real_distribution<double> clock(0.0, 100.0);
    std::bernoulli_distribution forward(0.5);
    HalfWayBounds bounds;
    for (int i = 0; i < 1000; ++i) {
        const double before_forward = bounds.forward_bound();
        const double before_backward = bounds.backward_bound();
        if (forward(generator)) {
            bounds.after_forward(clock(generator));
        } else {
            bounds.after_backward(clock(generator));
        }
        ASSERT_LE(bounds.backward_bound(), bounds.forward_bound()) << "step " << i;
        ASSERT_LE(bounds.forward_bound(), before_forward) << "H_F rose at step " << i;
        ASSERT_GE(bounds.backward_bound(), before_backward) << "H_B fell at step " << i;
    }
}

TEST(HalfWayBounds, FinishingSnapsTheOtherBound) {
    HalfWayBounds bounds;
    bounds.after_forward(5.0);    // H_B = 5
    bounds.after_backward(30.0);  // H_F = 30
    bounds.forward_finished();
    EXPECT_EQ(bounds.backward_bound(), 30.0);
    EXPECT_EQ(bounds.forward_bound(), 30.0);
    bounds.after_backward(12.0);  // max(12, 30) = 30: the snap holds
    EXPECT_EQ(bounds.forward_bound(), 30.0);

    HalfWayBounds other;
    other.after_forward(5.0);
    other.after_backward(30.0);
    other.backward_finished();
    EXPECT_EQ(other.forward_bound(), 5.0);
    EXPECT_EQ(other.backward_bound(), 5.0);
}

TEST(HalfWayBounds, StopPredicatesAndTheJoinPoint) {
    HalfWayBounds bounds;
    bounds.after_forward(4.0);    // H_B = 4
    bounds.after_backward(10.0);  // H_F = 10
    EXPECT_FALSE(bounds.should_stop_forward(10.0));
    EXPECT_TRUE(bounds.should_stop_forward(10.5));
    EXPECT_TRUE(bounds.should_stop_backward(4.0));
    EXPECT_FALSE(bounds.should_stop_backward(4.5));
    EXPECT_EQ(bounds.join_point(), 4.0);
}

TEST(HalfWayBounds, AWindowStartsTheBoundsInsideIt) {
    HalfWayBounds bounds(16.0, 24.0);
    EXPECT_EQ(bounds.backward_bound(), 16.0);
    EXPECT_EQ(bounds.forward_bound(), 24.0);
    bounds.after_forward(10.0);  // below the window: H_B stays
    EXPECT_EQ(bounds.backward_bound(), 16.0);
    bounds.after_backward(40.0);  // above the window: H_F stays
    EXPECT_EQ(bounds.forward_bound(), 24.0);
    bounds.after_forward(20.0);
    EXPECT_EQ(bounds.backward_bound(), 20.0);
    EXPECT_TRUE(bounds.should_stop_forward(25.0));
    EXPECT_TRUE(bounds.should_stop_backward(20.0));
    EXPECT_FALSE(bounds.should_stop_backward(20.5));
}
