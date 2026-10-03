// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// CapacityExtensionFunction: additive forward, threshold backward, b(u) = min(max_at_node, b(v) -
// q). Unlike AdditionExtensionFunction, which adds in both directions (as cost needs).

#include <gtest/gtest.h>

#include <map>
#include <stdexcept>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/backward_contract.hpp"
#include "util/test_arc.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace capacity_ext_test {

constexpr double kArcLoad = 10.0;
constexpr double kTolerance = 1e-12;

// A capacity no test value reaches, so the clamp stays out of the way.
constexpr double kHuge = 1e9;

/// @brief The value a backward step leaving @p node_id clamps a deadline to, read by extending
///        one no capacity reaches.
inline double clamp_leaving(CapacityExtensionFunction<RealResource>& proto, size_t node_id) {
    test_util::TestArc<RealResource> fixture(node_id, node_id + 1);
    auto fn = proto.create(fixture.arc);
    RealResource deadline;
    fn->extend_back(RealResource(kHuge), RealResource(0.0), &deadline);
    return deadline.get_value();
}

}  // namespace capacity_ext_test

// The defining property, checked with no per-node clamp in the way.
TEST(CapacityExtensionFunction, BackwardContractHolds) {
    CapacityExtensionFunction<RealResource> proto(capacity_ext_test::kHuge);
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
    CapacityExtensionFunction<RealResource> proto(
        make_node_bounds(0.0, capacity_ext_test::kHuge, {{0, {0.0, 50.0}}}));
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource extended;
    fn->extend_back(RealResource(90.0), RealResource(capacity_ext_test::kArcLoad), &extended);

    EXPECT_NEAR(extended.get_value(), 50.0, capacity_ext_test::kTolerance);
}

// The bound of the arc's ORIGIN clamps, because that is the node a backward extension lands on.
// A limit on the destination must not be applied here.
TEST(CapacityExtensionFunction, ClampUsesOriginNotDestination) {
    // A limit on the destination only.
    CapacityExtensionFunction<RealResource> proto(
        make_node_bounds(0.0, capacity_ext_test::kHuge, {{1, {0.0, 50.0}}}));
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource extended;
    fn->extend_back(RealResource(90.0), RealResource(capacity_ext_test::kArcLoad), &extended);

    // Origin 0 has no override, so the default (huge) bound applies and the subtraction stands.
    EXPECT_NEAR(extended.get_value(), 80.0, capacity_ext_test::kTolerance);
}

// A node absent from the overrides takes the default capacity.
TEST(CapacityExtensionFunction, AbsentNodeUsesTheDefaultCapacity) {
    CapacityExtensionFunction<RealResource> proto(make_node_bounds(0.0, 75.0, {{7, {0.0, 50.0}}}));
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource extended;
    fn->extend_back(RealResource(200.0), RealResource(capacity_ext_test::kArcLoad), &extended);

    EXPECT_NEAR(extended.get_value(), 75.0, capacity_ext_test::kTolerance);
}

// A uniform capacity never binds on a value that started at or below it, since
// b(v) <= Q implies b(v) - q <= Q.
TEST(CapacityExtensionFunction, UniformCapacityNeverBindsBelowIt) {
    CapacityExtensionFunction<RealResource> proto(100.0);
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource extended;
    fn->extend_back(RealResource(90.0), RealResource(capacity_ext_test::kArcLoad), &extended);
    EXPECT_NEAR(extended.get_value(), 80.0, capacity_ext_test::kTolerance);

    // Above the capacity it clamps.
    fn->extend_back(RealResource(200.0), RealResource(capacity_ext_test::kArcLoad), &extended);
    EXPECT_NEAR(extended.get_value(), 100.0, capacity_ext_test::kTolerance);
}

// The two constructors agree, and the shared bounds are the object passed in.
TEST(CapacityExtensionFunction, SharesTheBoundsItIsGiven) {
    const auto caps = make_node_bounds(0.0, 40.0, {{3, {0.0, 20.0}}});
    CapacityExtensionFunction<RealResource> shared(caps);
    EXPECT_EQ(shared.bounds(), caps);
    EXPECT_NEAR(capacity_ext_test::clamp_leaving(shared, 3), 20.0, capacity_ext_test::kTolerance);
    EXPECT_NEAR(capacity_ext_test::clamp_leaving(shared, 4), 40.0, capacity_ext_test::kTolerance);

    CapacityExtensionFunction<RealResource> uniform(40.0);
    EXPECT_TRUE(uniform.bounds()->is_uniform());
    EXPECT_NEAR(capacity_ext_test::clamp_leaving(uniform, 3), 40.0, capacity_ext_test::kTolerance);

    EXPECT_THROW(CapacityExtensionFunction<RealResource>(SharedNodeBounds<double>{}),
                 std::invalid_argument);
}

// Only the upper end is read: a capacity does not wait, so a lower end of 5 is no forward floor.
TEST(CapacityExtensionFunction, ReadsOnlyTheUpperEnd) {
    CapacityExtensionFunction<RealResource> proto(make_node_bounds(5.0, 40.0));

    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);
    RealResource extended;
    fn->extend(RealResource(0.0), RealResource(1.0), &extended);
    EXPECT_NEAR(extended.get_value(), 1.0, capacity_ext_test::kTolerance);
}

// Forward is a plain accumulation, matching AdditionExtensionFunction.
TEST(CapacityExtensionFunction, ForwardAccumulates) {
    CapacityExtensionFunction<RealResource> proto(
        make_node_bounds(0.0, capacity_ext_test::kHuge, {{0, {0.0, 50.0}}}));
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
    CapacityExtensionFunction<RealResource> proto(capacity_ext_test::kHuge);
    EXPECT_EQ(proto.backward_kind(), BackwardKind::Threshold);

    AdditionExtensionFunction<RealResource> addition;
    EXPECT_EQ(addition.backward_kind(), BackwardKind::Accumulate);
}
