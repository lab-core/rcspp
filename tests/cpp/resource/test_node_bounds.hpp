// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// NodeBounds: the per-node bounds a threshold extension and its feasibility function share, and
// the constructors that take them.

#include <gtest/gtest.h>

#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

#include "rcspp/rcspp.hpp"
#include "util/test_arc.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace node_bounds_test {

constexpr double kTolerance = 1e-12;

/// @brief Windows of `[0, 100]` except node 2, `[10, 30]`.
inline SharedNodeBounds<double> windows() {
    return make_node_bounds(0.0, 100.0, {{2, {10.0, 30.0}}});
}

}  // namespace node_bounds_test

TEST(NodeBounds, AnOverrideWinsAndTheDefaultsFillTheRest) {
    const auto bounds = node_bounds_test::windows();
    EXPECT_EQ(bounds->at(2), (std::pair<double, double>{10.0, 30.0}));
    EXPECT_EQ(bounds->at(7), (std::pair<double, double>{0.0, 100.0}));
    EXPECT_DOUBLE_EQ(bounds->lower(2), 10.0);
    EXPECT_DOUBLE_EQ(bounds->upper(2), 30.0);
    EXPECT_DOUBLE_EQ(bounds->lower(7), 0.0);
    EXPECT_DOUBLE_EQ(bounds->upper(7), 100.0);
    EXPECT_DOUBLE_EQ(bounds->default_lower(), 0.0);
    EXPECT_DOUBLE_EQ(bounds->default_upper(), 100.0);
    EXPECT_EQ(bounds->by_node().size(), 1U);
    EXPECT_FALSE(bounds->is_uniform());

    const NodeBounds<int> uniform(0, 5);
    EXPECT_TRUE(uniform.is_uniform());
    EXPECT_EQ(uniform.at(3), (std::pair<int, int>{0, 5}));
}

TEST(NodeBounds, MinMaxFeasibilityReadsTheSharedBoundsAtEachNode) {
    const auto bounds = node_bounds_test::windows();
    MinMaxFeasibilityFunction<RealResource> prototype(bounds);
    EXPECT_EQ(prototype.bounds(), bounds);

    auto at_two = prototype.create(2);
    EXPECT_FALSE(at_two->is_feasible(RealResource(5.0)));  // below node 2's minimum
    EXPECT_TRUE(at_two->is_feasible(RealResource(20.0)));
    EXPECT_FALSE(at_two->is_feasible(RealResource(40.0)));      // above node 2's maximum
    EXPECT_FALSE(at_two->is_back_feasible(RealResource(9.0)));  // the backward test, too
    EXPECT_TRUE(at_two->is_back_feasible(RealResource(10.0)));

    auto elsewhere = prototype.create(7);
    EXPECT_TRUE(elsewhere->is_feasible(RealResource(40.0)));

    // The map constructor builds bounds of its own, with the same contents.
    MinMaxFeasibilityFunction<RealResource> from_map(0.0, 100.0, {{2, {10.0, 30.0}}});
    EXPECT_EQ(from_map.bounds()->at(2), bounds->at(2));
    EXPECT_EQ(from_map.bounds()->at(7), bounds->at(7));

    EXPECT_THROW(MinMaxFeasibilityFunction<RealResource>(SharedNodeBounds<double>{}),
                 std::invalid_argument);
}

TEST(NodeBounds, TimeWindowFeasibilityReadsTheSharedBoundsAtEachNode) {
    const auto bounds = node_bounds_test::windows();
    TimeWindowFeasibilityFunction<RealResource> prototype(bounds);
    EXPECT_EQ(prototype.bounds(), bounds);

    auto at_two = prototype.create(2);
    EXPECT_TRUE(at_two->is_feasible(RealResource(30.0)));
    EXPECT_FALSE(at_two->is_feasible(RealResource(31.0)));
    EXPECT_FALSE(at_two->is_back_feasible(RealResource(9.0)));
    EXPECT_TRUE(at_two->is_back_feasible(RealResource(10.0)));
    EXPECT_TRUE(prototype.create(7)->is_back_feasible(RealResource(0.0)));

    // The map constructor opens every node at 0 and closes absent ones at its default.
    TimeWindowFeasibilityFunction<RealResource> from_map({{2, {10.0, 30.0}}}, 100.0);
    EXPECT_EQ(from_map.bounds()->at(2), bounds->at(2));
    EXPECT_EQ(from_map.bounds()->at(7), bounds->at(7));

    EXPECT_THROW(TimeWindowFeasibilityFunction<RealResource>(SharedNodeBounds<double>{}),
                 std::invalid_argument);
}

TEST(NodeBounds, TimeWindowExtensionReadsBothEnds) {
    const auto bounds = node_bounds_test::windows();
    TimeWindowExtensionFunction<RealResource> extension(bounds);
    EXPECT_EQ(extension.bounds(), bounds);

    // Arriving at node 2 at 5 waits until its opening time, 10.
    test_util::TestArc<RealResource> into_two(1, 2);
    auto forward = extension.create(into_two.arc);
    RealResource arrival;
    forward->extend(RealResource(0.0), RealResource(5.0), &arrival);
    EXPECT_NEAR(arrival.get_value(), 10.0, node_bounds_test::kTolerance);

    // Leaving node 2 backward with a deadline of 90 is clamped to its closing time, 30.
    test_util::TestArc<RealResource> out_of_two(2, 3);
    auto backward = extension.create(out_of_two.arc);
    RealResource deadline;
    backward->extend_back(RealResource(95.0), RealResource(5.0), &deadline);
    EXPECT_NEAR(deadline.get_value(), 30.0, node_bounds_test::kTolerance);

    // A backward label starting at node 2 starts at its closing time, 30.
    RealResource start;
    forward->start_back(&start);
    EXPECT_NEAR(start.get_value(), 30.0, node_bounds_test::kTolerance);

    // A node without a window of its own closes at the default, 100.
    test_util::TestArc<RealResource> out_of_seven(7, 8);
    auto backward_from_seven = extension.create(out_of_seven.arc);
    backward_from_seven->extend_back(RealResource(500.0), RealResource(5.0), &deadline);
    EXPECT_NEAR(deadline.get_value(), 100.0, node_bounds_test::kTolerance);

    EXPECT_THROW(TimeWindowExtensionFunction<RealResource>(SharedNodeBounds<double>{}),
                 std::invalid_argument);
}

// One object, three readers: nothing is copied, so nothing can drift apart.
TEST(NodeBounds, OneObjectServesTheExtensionAndTheFeasibilityFunction) {
    const auto caps = make_node_bounds(0, 50, {{1, {0, 20}}});
    const CapacityExtensionFunction<IntResource> capacity(caps);
    const MinMaxFeasibilityFunction<IntResource> feasibility(caps);
    CapacityExtensionFunction<IntResource> from_feasibility(feasibility.bounds());
    EXPECT_EQ(capacity.bounds(), caps);
    EXPECT_EQ(feasibility.bounds(), caps);
    EXPECT_EQ(from_feasibility.bounds(), caps);

    // A backward step leaving node 1 clamps to the cap the feasibility function holds there.
    test_util::TestArc<IntResource> out_of_one(1, 2);
    auto backward = from_feasibility.create(out_of_one.arc);
    IntResource deadline;
    backward->extend_back(IntResource(50), IntResource(0), &deadline);
    EXPECT_EQ(deadline.get_value(), 20);
}
