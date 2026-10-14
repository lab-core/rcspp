// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Backward-extension semantics for TimeWindowExtensionFunction.
//
// A backward label stores a deadline (the latest arrival that can still finish), so
// b(u) = min(latest_u, b(v) - t_uv).

#include <gtest/gtest.h>

#include <limits>
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

// extend_back must not clamp up to the origin's earliest time, or is_back_feasible could not
// reject the infeasible value. A deadline below it is unmeetable, so it becomes lowest().
TEST(TimeWindowExtensionFunction, BackwardDoesNotClampUpToEarliest) {
    // origin 0 opens at 50; a deadline of 60 minus a 30-minute arc leaves 30 < 50.
    std::map<size_t, std::pair<double, double>> windows{{0, {50.0, 90.0}}, {1, {0.0, 1000.0}}};
    TimeWindowExtensionFunction<RealResource> proto(windows);
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource extended;
    fn->extend_back(RealResource(60.0), RealResource(tw_ext_test::kArcTime), &extended);

    // Not 50: no arrival at the origin can meet a deadline of 30, so it is marked unmeetable.
    EXPECT_EQ(extended.get_value(), std::numeric_limits<double>::lowest());

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

// An unsigned value type cannot represent an unmeetable deadline, so it reports Unspecified and
// a bidirectional solve refuses it. Forward use is unaffected.
TEST(TimeWindowExtensionFunction, UnsignedValueTypeRefusesBackwardUse) {
    std::map<size_t, std::pair<unsigned int, unsigned int>> windows{{0, {0U, 100U}},
                                                                    {1, {0U, 100U}}};
    TimeWindowExtensionFunction<UIntResource> proto(windows);
    EXPECT_EQ(proto.backward_kind(), BackwardKind::Unspecified);

    // Forward extension still works.
    test_util::TestArc<UIntResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);
    UIntResource extended;
    fn->extend(UIntResource(10U), UIntResource(30U), &extended);
    EXPECT_EQ(extended.get_value(), 40U);

    // A direct backward call saturates at zero rather than wrapping.
    UIntResource back_extended;
    fn->extend_back(UIntResource(10U), UIntResource(30U), &back_extended);
    EXPECT_EQ(back_extended.get_value(), 0U);

    // Without underflow it is the ordinary subtraction.
    UIntResource back_ok;
    fn->extend_back(UIntResource(90U), UIntResource(30U), &back_ok);
    EXPECT_EQ(back_ok.get_value(), 60U);
}

// Only the origin is in the map, so the destination's lower bound stays 0 and the forward clamp
// is max(0, .). Guards against an absent node silently dropping the clamp.
TEST(TimeWindowExtensionFunction, ForwardClampsToZeroWhenDestinationHasNoWindow) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 1000.0}}};
    TimeWindowExtensionFunction<RealResource> proto(windows);
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource extended;
    fn->extend(RealResource(-50.0), RealResource(tw_ext_test::kArcTime), &extended);
    EXPECT_NEAR(extended.get_value(), 0.0, tw_ext_test::kTolerance);  // max(0, -20): it binds

    fn->extend(RealResource(10.0), RealResource(tw_ext_test::kArcTime), &extended);
    EXPECT_NEAR(extended.get_value(), 40.0, tw_ext_test::kTolerance);  // max(0, 40): it does not
}

// Only the destination is in the map, so the backward clamp uses the constructor's default.
TEST(TimeWindowExtensionFunction, BackwardClampsToTheDefaultWhenOriginHasNoWindow) {
    std::map<size_t, std::pair<double, double>> windows{{1, {0.0, 1000.0}}};
    TimeWindowExtensionFunction<RealResource> proto(windows,
                                                    /*default_max_time_window=*/500.0);
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource back;
    fn->extend_back(RealResource(600.0), RealResource(tw_ext_test::kArcTime), &back);
    EXPECT_NEAR(back.get_value(), 500.0, tw_ext_test::kTolerance);  // min(500, 570): the default

    fn->extend_back(RealResource(400.0), RealResource(tw_ext_test::kArcTime), &back);
    EXPECT_NEAR(back.get_value(),
                370.0,
                tw_ext_test::kTolerance);  // min(500, 370): the subtraction
}
