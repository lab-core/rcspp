// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Phase 9: the half-way policy, and the validation that refuses to trust a resource that is not
// actually a clock.
//
// A wrong bound is worse than no bound. With a non-monotone critical resource the clock can cross H
// more than once, so a valid path may be discarded by the forward search AND by the backward search
// and never found at all -- while the solver reports COMPLETE. So the response to a failed check is
// to disable the bound (correct but slow), never to throw: a pricing loop calls solve() thousands
// of times and an exception on iteration 900 is unrecoverable.
//
// ClampedExtensionIsAccepted is the test that justifies the design. Reading an arc's stored
// consumption would reject a monotone-but-clamped extension; probing the property accepts it.

#include <gtest/gtest.h>

#include <limits>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace half_way_test {

constexpr double kTolerance = 1e-12;
constexpr double kUpperBound = 100.0;

/// @brief The probe set the phase recommends: enough to catch a sign error, cheap on a dense graph.
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
/// The boundary is asserted explicitly: a label sitting exactly on H must survive in both
/// directions, or a path crossing there is lost by both searches.
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

    // But an explicit H rescues it: the caller has supplied the middle directly.
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
/// This is what a reduced cost looks like during pricing: update_reduced_costs rewrites the cost
/// slot to `arc.cost - sum(coefficient * dual)`, which is negative on most arcs. Pointing the
/// critical resource at that slot is the mistake this check exists to catch.
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
/// This is the case that justifies probing the property instead of reading `arc.extender`'s value.
/// The arc carries -5, which read naively says "not monotone"; but the extension clamps the result
/// up to the destination's opening time of 60, so from probes of 0 and 50 the value only ever
/// rises. Reading the stored consumption would reject a perfectly good clock.
TEST(HalfWayPolicy, ClampedExtensionIsAccepted) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 1000.0}}, {1, {60.0, 1000.0}}};

    auto graph = half_way_test::two_node_graph(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        /*arc_value=*/-5.0);

    // Sanity: the raw consumption really is negative, so this is not accidentally the easy case.
    const auto probes = half_way_test::probes();
    EXPECT_TRUE(critical_resource_is_monotone<RealResource>(*graph, 0, probes));
}

/// @brief Once one arc fails, the remaining arcs are skipped rather than re-probed.
///
/// The check answers a yes/no question about the whole graph, so there is nothing to learn from
/// probing further -- and on a dense graph the saving is the difference between one arc's work and
/// every arc's. Two arcs, the first non-monotone: the second must take the short-circuit.
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
///
/// The same guard AStarDominanceAlgorithm uses when its cost type is not in the composition.
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
// The failure response
// ============================================================================

/// @brief A failed check disables the bound and does not throw.
///
/// Throwing would be unrecoverable in a pricing loop that calls solve() thousands of times. This
/// mirrors AStarDominanceAlgorithm, which catches Bellman-Ford's negative-cycle error and disables
/// its heuristic rather than failing.
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
    // And with the bound off the search keeps every label: correct but slow.
    EXPECT_FALSE(policy.should_stop_forward(1e9));
    EXPECT_FALSE(policy.should_stop_backward(-1e9));
}
