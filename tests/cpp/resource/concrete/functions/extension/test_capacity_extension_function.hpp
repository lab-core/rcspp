// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// CapacityExtensionFunction: additive forward, threshold backward, b(u) = min(max_at_node, b(v) -
// q). Unlike AdditionExtensionFunction, which adds in both directions (as cost needs).

#include <gtest/gtest.h>

#include <map>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/backward_contract.hpp"
#include "util/test_arc.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace capacity_ext_test {

constexpr double kArcLoad = 10.0;
constexpr double kTolerance = 1e-12;

}  // namespace capacity_ext_test

// The defining property, checked with no per-node clamp in the way.
TEST(CapacityExtensionFunction, BackwardContractHolds) {
    CapacityExtensionFunction<RealResource> proto;
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    const RealResource arc_value(capacity_ext_test::kArcLoad);
    test_util::check_backward_contract<ExtensionFunction<RealResource>, RealResource, double>(
        *fn,
        arc_value,
        /*samples=*/{0.0, 5.0, 20.0, 40.0, 90.0},
        /*thetas=*/{10.0, 30.0, 50.0, 90.0});
}

// The clamp binds when the origin carries a per-node limit:
// Q_u = 50, b(v) = 90, q = 10  ->  min(50, 80) = 50, not 80.
TEST(CapacityExtensionFunction, PerNodeClampBinds) {
    std::map<size_t, double> max_by_node{{0, 50.0}};
    CapacityExtensionFunction<RealResource> proto(max_by_node);
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource extended;
    fn->extend_back(RealResource(90.0), RealResource(capacity_ext_test::kArcLoad), &extended);

    EXPECT_NEAR(extended.get_value(), 50.0, capacity_ext_test::kTolerance);
}

// The bound of the arc's ORIGIN clamps, because that is the node a backward extension lands on.
// A limit on the destination must not be applied here.
TEST(CapacityExtensionFunction, ClampUsesOriginNotDestination) {
    std::map<size_t, double> max_by_node{{1, 50.0}};  // limit on the destination only
    CapacityExtensionFunction<RealResource> proto(max_by_node);
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource extended;
    fn->extend_back(RealResource(90.0), RealResource(capacity_ext_test::kArcLoad), &extended);

    // Origin 0 has no entry, so the default (huge) bound applies and the subtraction stands.
    EXPECT_NEAR(extended.get_value(), 80.0, capacity_ext_test::kTolerance);
}

// A node absent from a non-empty map falls back to default_max.
TEST(CapacityExtensionFunction, AbsentNodeUsesDefaultMax) {
    std::map<size_t, double> max_by_node{{7, 50.0}};
    CapacityExtensionFunction<RealResource> proto(max_by_node, /*default_max=*/75.0);
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource extended;
    fn->extend_back(RealResource(200.0), RealResource(capacity_ext_test::kArcLoad), &extended);

    EXPECT_NEAR(extended.get_value(), 75.0, capacity_ext_test::kTolerance);
}

// The empty-map constructor path: with a uniform capacity the clamp never binds, since
// b(v) <= Q implies b(v) - q <= Q.
TEST(CapacityExtensionFunction, EmptyMapClampNeverBinds) {
    CapacityExtensionFunction<RealResource> proto;  // no per-node bounds at all
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource extended;
    fn->extend_back(RealResource(90.0), RealResource(capacity_ext_test::kArcLoad), &extended);

    EXPECT_NEAR(extended.get_value(), 80.0, capacity_ext_test::kTolerance);
}

// Forward is a plain accumulation, matching AdditionExtensionFunction.
TEST(CapacityExtensionFunction, ForwardAccumulates) {
    std::map<size_t, double> max_by_node{{0, 50.0}};
    CapacityExtensionFunction<RealResource> proto(max_by_node);
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource capacity_extended;
    fn->extend(RealResource(30.0), RealResource(capacity_ext_test::kArcLoad), &capacity_extended);
    EXPECT_NEAR(capacity_extended.get_value(), 40.0, capacity_ext_test::kTolerance);

    // The per-node bound does not clamp the forward direction.
    AdditionExtensionFunction<RealResource> addition;
    RealResource addition_extended;
    addition.extend(RealResource(30.0),
                    RealResource(capacity_ext_test::kArcLoad),
                    &addition_extended);
    EXPECT_NEAR(capacity_extended.get_value(),
                addition_extended.get_value(),
                capacity_ext_test::kTolerance);
}

// A capacity is a ceiling-style bound; cost, which uses AdditionExtensionFunction, is not.
TEST(CapacityExtensionFunction, DeclaresThresholdBackwardKind) {
    CapacityExtensionFunction<RealResource> proto;
    EXPECT_EQ(proto.backward_kind(), BackwardKind::Threshold);

    AdditionExtensionFunction<RealResource> addition;
    EXPECT_EQ(addition.backward_kind(), BackwardKind::Accumulate);
}
