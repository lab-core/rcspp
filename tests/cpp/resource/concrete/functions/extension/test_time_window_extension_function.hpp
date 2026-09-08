// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Backward-extension semantics for TimeWindowExtensionFunction.
//
// A forward label stores the *earliest* you can be at a node; a backward label stores the
// *latest* you can be there and still finish -- a deadline. So the backward extension is the
// inverse of the forward one: b(u) = min(latest_u, b(v) - t_uv).

#include <gtest/gtest.h>

#include <map>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/backward_contract.hpp"
#include "util/test_arc.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace tw_ext_test {

constexpr double kArcTime = 30.0;
constexpr double kSlackBound = 1000.0;
constexpr double kTolerance = 1e-12;

// Windows wide enough that neither clamp binds -- the precondition for the biconditional.
inline std::map<size_t, std::pair<double, double>> slack_windows() {
    return {{0, {0.0, kSlackBound}}, {1, {0.0, kSlackBound}}};
}

}  // namespace tw_ext_test

// The defining property of a threshold backward extension:
//     extend(x, arc) <= theta   <==>   x <= extend_back(theta, arc)
// A sign error fails this immediately, with no graph and no search.
TEST(TimeWindowExtensionFunction, BackwardContractHolds) {
    TimeWindowExtensionFunction<RealResource> proto(tw_ext_test::slack_windows());
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    const RealResource arc_value(tw_ext_test::kArcTime);
    test_util::check_backward_contract<ExtensionFunction<RealResource>, RealResource, double>(
        *fn,
        arc_value,
        /*samples=*/{0.0, 10.0, 40.0, 50.0, 100.0, 300.0},
        /*thetas=*/{40.0, 90.0, 200.0, 500.0});
}

// Worked case: b(v)=100, t=30, latest[u]=60  ->  min(60, 70) = 60. The origin's own closing
// time binds, which is how a mid-path limit propagates backwards.
TEST(TimeWindowExtensionFunction, BackwardClampBindsAtOriginLatest) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 60.0}}, {1, {0.0, 1000.0}}};
    TimeWindowExtensionFunction<RealResource> proto(windows);
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource extended;
    fn->extend_back(RealResource(100.0), RealResource(tw_ext_test::kArcTime), &extended);

    EXPECT_NEAR(extended.get_value(), 60.0, tw_ext_test::kTolerance);
}

// Worked case: b(v)=100, t=30, latest[u]=90  ->  min(90, 70) = 70. The subtraction binds.
TEST(TimeWindowExtensionFunction, BackwardSubtractionBindsWhenWindowIsSlack) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 90.0}}, {1, {0.0, 1000.0}}};
    TimeWindowExtensionFunction<RealResource> proto(windows);
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource extended;
    fn->extend_back(RealResource(100.0), RealResource(tw_ext_test::kArcTime), &extended);

    EXPECT_NEAR(extended.get_value(), 70.0, tw_ext_test::kTolerance);
}

// extend_back must NOT clamp up to the origin's earliest time. Clamping from below would turn
// an infeasible backward value into a feasible-looking one and rob is_back_feasible of the very
// case it exists to catch.
TEST(TimeWindowExtensionFunction, BackwardDoesNotClampUpToEarliest) {
    // origin 0 opens at 50; a deadline of 60 minus a 30-minute arc leaves 30 < 50.
    std::map<size_t, std::pair<double, double>> windows{{0, {50.0, 90.0}}, {1, {0.0, 1000.0}}};
    TimeWindowExtensionFunction<RealResource> proto(windows);
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource extended;
    fn->extend_back(RealResource(60.0), RealResource(tw_ext_test::kArcTime), &extended);

    // 30, not 50: the raw subtraction is preserved so feasibility can reject it.
    EXPECT_NEAR(extended.get_value(), 30.0, tw_ext_test::kTolerance);

    // And the feasibility function does reject it, at the node the backward label landed on.
    TimeWindowFeasibilityFunction<RealResource> feas_proto(windows);
    auto feas = feas_proto.create(0);
    EXPECT_FALSE(feas->is_back_feasible(extended));
}

// Forward extension is untouched: it still waits for the destination's opening time.
TEST(TimeWindowExtensionFunction, ForwardStillClampsUpToEarliest) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 1000.0}}, {1, {80.0, 1000.0}}};
    TimeWindowExtensionFunction<RealResource> proto(windows);
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource extended;
    fn->extend(RealResource(10.0), RealResource(tw_ext_test::kArcTime), &extended);

    // 10 + 30 = 40, but node 1 does not open until 80, so the vehicle waits.
    EXPECT_NEAR(extended.get_value(), 80.0, tw_ext_test::kTolerance);
}

// A time window is a deadline-style bound.
TEST(TimeWindowExtensionFunction, DeclaresThresholdBackwardKind) {
    TimeWindowExtensionFunction<RealResource> proto(tw_ext_test::slack_windows());
    EXPECT_EQ(proto.backward_kind(), BackwardKind::Threshold);

    // Signed integers work too.
    std::map<size_t, std::pair<int, int>> int_windows{{0, {0, 100}}, {1, {0, 100}}};
    TimeWindowExtensionFunction<IntResource> int_proto(int_windows);
    EXPECT_EQ(int_proto.backward_kind(), BackwardKind::Threshold);
}

// An UNSIGNED value type cannot represent "this deadline cannot be met": the subtraction would
// wrap to a huge positive value, and saturating at zero is no better, because
// is_back_feasible accepts 0 wherever the node's earliest time is 0.
//
// TimeWindowExtensionFunction<UIntResource> is a live instantiation -- Python exports it as
// TimeWindowExtensionFunction_uint -- so instead of rejecting it at compile time it reports
// Unspecified, and phase 11 refuses to start a bidirectional solve on it. Forward-only use is
// unaffected, which the second half of this test pins.
TEST(TimeWindowExtensionFunction, UnsignedValueTypeRefusesBackwardUse) {
    std::map<size_t, std::pair<unsigned int, unsigned int>> windows{{0, {0U, 100U}},
                                                                    {1, {0U, 100U}}};
    TimeWindowExtensionFunction<UIntResource> proto(windows);
    EXPECT_EQ(proto.backward_kind(), BackwardKind::Unspecified);

    // Forward extension on an unsigned time window still behaves exactly as before.
    test_util::TestArc<UIntResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);
    UIntResource extended;
    fn->extend(UIntResource(10U), UIntResource(30U), &extended);
    EXPECT_EQ(extended.get_value(), 40U);

    // And the backward path saturates rather than wrapping, so a direct call cannot
    // manufacture a huge "very loose deadline".
    UIntResource back_extended;
    fn->extend_back(UIntResource(10U), UIntResource(30U), &back_extended);
    EXPECT_EQ(back_extended.get_value(), 0U);

    // Where the subtraction does not underflow, it is the ordinary one -- saturation is
    // reached only by the branch above.
    UIntResource back_ok;
    fn->extend_back(UIntResource(90U), UIntResource(30U), &back_ok);
    EXPECT_EQ(back_ok.get_value(), 60U);
}
