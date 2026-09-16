// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// BudgetExtensionFunction: additive forward, threshold backward.
//
// Forward it accumulates like AdditionExtensionFunction; backward it inverts,
// b(u) = min(max_at_node, b(v) - q), because a capacity is a ceiling rather than a running
// total. That divergence is the whole reason this is a separate class from
// AdditionExtensionFunction, which must keep adding in both directions for the cost slot.

#include <gtest/gtest.h>

#include <map>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/backward_contract.hpp"
#include "util/test_arc.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace budget_ext_test {

constexpr double kArcLoad = 10.0;
constexpr double kTolerance = 1e-12;

}  // namespace budget_ext_test

// The defining property, checked with no per-node clamp in the way.
TEST(BudgetExtensionFunction, BackwardContractHolds) {
    BudgetExtensionFunction<RealResource> proto;
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    const RealResource arc_value(budget_ext_test::kArcLoad);
    test_util::check_backward_contract<ExtensionFunction<RealResource>, RealResource, double>(
        *fn,
        arc_value,
        /*samples=*/{0.0, 5.0, 20.0, 40.0, 90.0},
        /*thetas=*/{10.0, 30.0, 50.0, 90.0});
}

// The clamp binds when the origin carries a per-node limit:
// Q_u = 50, b(v) = 90, q = 10  ->  min(50, 80) = 50, not 80.
TEST(BudgetExtensionFunction, PerNodeClampBinds) {
    std::map<size_t, double> max_by_node{{0, 50.0}};
    BudgetExtensionFunction<RealResource> proto(max_by_node);
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource extended;
    fn->extend_back(RealResource(90.0), RealResource(budget_ext_test::kArcLoad), &extended);

    EXPECT_NEAR(extended.get_value(), 50.0, budget_ext_test::kTolerance);
}

// The bound of the arc's ORIGIN clamps, because that is the node a backward extension lands on.
// A limit on the destination must not be applied here.
TEST(BudgetExtensionFunction, ClampUsesOriginNotDestination) {
    std::map<size_t, double> max_by_node{{1, 50.0}};  // limit on the destination only
    BudgetExtensionFunction<RealResource> proto(max_by_node);
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource extended;
    fn->extend_back(RealResource(90.0), RealResource(budget_ext_test::kArcLoad), &extended);

    // Origin 0 has no entry, so the default (huge) bound applies and the subtraction stands.
    EXPECT_NEAR(extended.get_value(), 80.0, budget_ext_test::kTolerance);
}

// A node absent from a non-empty map falls back to default_max.
TEST(BudgetExtensionFunction, AbsentNodeUsesDefaultMax) {
    std::map<size_t, double> max_by_node{{7, 50.0}};
    BudgetExtensionFunction<RealResource> proto(max_by_node, /*default_max=*/75.0);
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource extended;
    fn->extend_back(RealResource(200.0), RealResource(budget_ext_test::kArcLoad), &extended);

    EXPECT_NEAR(extended.get_value(), 75.0, budget_ext_test::kTolerance);
}

// The empty-map constructor path: with a uniform capacity the clamp never binds, since
// b(v) <= Q implies b(v) - q <= Q.
TEST(BudgetExtensionFunction, EmptyMapClampNeverBinds) {
    BudgetExtensionFunction<RealResource> proto;  // no per-node bounds at all
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource extended;
    fn->extend_back(RealResource(90.0), RealResource(budget_ext_test::kArcLoad), &extended);

    EXPECT_NEAR(extended.get_value(), 80.0, budget_ext_test::kTolerance);
}

// Forward is a plain accumulation, matching AdditionExtensionFunction.
TEST(BudgetExtensionFunction, ForwardAccumulates) {
    std::map<size_t, double> max_by_node{{0, 50.0}};
    BudgetExtensionFunction<RealResource> proto(max_by_node);
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource budget_extended;
    fn->extend(RealResource(30.0), RealResource(budget_ext_test::kArcLoad), &budget_extended);
    EXPECT_NEAR(budget_extended.get_value(), 40.0, budget_ext_test::kTolerance);

    // The per-node bound does not clamp the forward direction.
    AdditionExtensionFunction<RealResource> addition;
    RealResource addition_extended;
    addition.extend(RealResource(30.0),
                    RealResource(budget_ext_test::kArcLoad),
                    &addition_extended);
    EXPECT_NEAR(budget_extended.get_value(),
                addition_extended.get_value(),
                budget_ext_test::kTolerance);
}

// A budget is a ceiling-style bound; cost, which uses AdditionExtensionFunction, is not.
TEST(BudgetExtensionFunction, DeclaresThresholdBackwardKind) {
    BudgetExtensionFunction<RealResource> proto;
    EXPECT_EQ(proto.backward_kind(), BackwardKind::Threshold);

    AdditionExtensionFunction<RealResource> addition;
    EXPECT_EQ(addition.backward_kind(), BackwardKind::Accumulate);
}
