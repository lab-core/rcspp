// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The controller that moves the half-way point between solves, driven by hand-written observations.
//
// No graph and no solve anywhere in this file: every rule of the controller is a statement about
// numbers in and a number out, so it is asserted that way, with exact values. The solve-level
// behaviour -- that a persistent algorithm object actually feeds the controller, and that moving
// `H` never changes an answer -- is in test_dynamic_half_way.hpp and test_equivalence.hpp.
//
// Every observation below that is meant to be acted on is `bounded` and `exact`; the ones that are
// not are in the guard tests, which exist to show those two flags are read.

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace half_way_controller_test {

/// @brief Loose enough for a product of a few binary fractions (0.8 * 1.1 is not exactly 0.88).
constexpr double kTolerance = 1e-9;

/// @brief `H0`, so `R = 200` and the clamp is `[10, 190]`.
constexpr double kInitialH = 100.0;

/// @brief A trustworthy observation with the given label counts.
///
/// `joined_paths = 1` keeps the zero-join guard out of the way; the tests about that guard set it
/// to 0 themselves.
inline HalfWayObservation observed(size_t forward, size_t backward, size_t joined = 1,
                                   size_t solutions = 1) {
    return HalfWayObservation{.forward_labels = forward,
                              .backward_labels = backward,
                              .joined_paths = joined,
                              .solutions = solutions,
                              .bounded = true,
                              .exact = true};
}

/// @brief Forward did three times the backward work -- far outside the dead zone.
inline HalfWayObservation forward_heavy() {
    return observed(300, 100);
}

/// @brief The mirror image.
inline HalfWayObservation backward_heavy() {
    return observed(100, 300);
}

}  // namespace half_way_controller_test

// ============================================================================
// Seeding and validation
// ============================================================================

/// @brief A default-constructed controller is unseeded and ignores every observation.
TEST(HalfWayController, UnseededControllerDoesNothing) {
    namespace hc = half_way_controller_test;

    HalfWayController controller;
    EXPECT_FALSE(controller.seeded());
    EXPECT_EQ(controller.update(hc::forward_heavy()), HalfWayMove::Skipped);
    EXPECT_EQ(controller.observations(), 0U) << "an unseeded controller records nothing";
}

/// @brief Seeding fixes `H`, the step and the range `R = 2 * H0`.
TEST(HalfWayController, SeedsFromTheInitialH) {
    namespace hc = half_way_controller_test;

    const HalfWayController controller(hc::kInitialH);
    EXPECT_TRUE(controller.seeded());
    EXPECT_NEAR(controller.h(), hc::kInitialH, hc::kTolerance);
    EXPECT_NEAR(controller.initial_h(), hc::kInitialH, hc::kTolerance);
    EXPECT_NEAR(controller.range(), 2.0 * hc::kInitialH, hc::kTolerance);
    EXPECT_NEAR(controller.step(), HalfWayControllerParams{}.initial_step, hc::kTolerance);
    EXPECT_FALSE(controller.frozen());
}

/// @brief Zero is the "bound off" sentinel, so it cannot seed a controller; nor can garbage.
TEST(HalfWayController, RefusesANonPositiveOrNonFiniteSeed) {
    EXPECT_THROW(HalfWayController(0.0), std::invalid_argument);
    EXPECT_THROW(HalfWayController(-5.0), std::invalid_argument);
    EXPECT_THROW(HalfWayController{std::numeric_limits<double>::infinity()}, std::invalid_argument);
    EXPECT_THROW(HalfWayController{std::numeric_limits<double>::quiet_NaN()},
                 std::invalid_argument);
}

/// @brief Each out-of-range knob is refused at construction, not discovered mid-loop.
TEST(HalfWayController, RefusesOutOfRangeParams) {
    auto with = [](auto edit) {
        HalfWayControllerParams params;
        edit(params);
        return params;
    };
    EXPECT_THROW(HalfWayController(1.0, with([](auto& p) { p.dead_zone = -0.1; })),
                 std::invalid_argument);
    EXPECT_THROW(HalfWayController(1.0, with([](auto& p) { p.initial_step = 1.0; })),
                 std::invalid_argument);
    EXPECT_THROW(HalfWayController(1.0, with([](auto& p) { p.min_step = 0.0; })),
                 std::invalid_argument);
    EXPECT_THROW(HalfWayController(1.0, with([](auto& p) { p.min_step = 0.5; })),
                 std::invalid_argument)
        << "a floor above the starting step";
    EXPECT_THROW(HalfWayController(1.0, with([](auto& p) { p.step_decay = 0.5; })),
                 std::invalid_argument)
        << "a decay below 1 would GROW the step on every reversal";
    EXPECT_THROW(HalfWayController(1.0, with([](auto& p) { p.min_fraction = 0.0; })),
                 std::invalid_argument);
    EXPECT_THROW(HalfWayController(1.0, with([](auto& p) { p.max_fraction = 1.0; })),
                 std::invalid_argument);
    EXPECT_THROW(HalfWayController(1.0, with([](auto& p) {
                                       p.min_fraction = 0.6;
                                       p.max_fraction = 0.4;
                                   })),
                 std::invalid_argument);
    EXPECT_NO_THROW(HalfWayController(1.0, HalfWayControllerParams{}));
}

// ============================================================================
// The moves
// ============================================================================

/// @brief Inside the dead zone nothing moves -- including the step.
///
/// 110 against 100 is a 10 % imbalance, under the default 20 %. Label counts are noisy from one
/// pricing iteration to the next, and a controller that chased that noise would move `H` on every
/// solve.
TEST(HalfWayController, TheDeadZoneHoldsH) {
    namespace hc = half_way_controller_test;

    HalfWayController controller(hc::kInitialH);
    EXPECT_EQ(controller.update(hc::observed(110, 100)), HalfWayMove::Unchanged);
    EXPECT_EQ(controller.update(hc::observed(100, 110)), HalfWayMove::Unchanged);
    // Exactly on the edge is still inside: the rule is "exceeds", not "reaches".
    EXPECT_EQ(controller.update(hc::observed(120, 100)), HalfWayMove::Unchanged);
    EXPECT_NEAR(controller.h(), hc::kInitialH, hc::kTolerance);
    EXPECT_EQ(controller.observations(), 3U);
    EXPECT_EQ(controller.moves(), 0U);
}

/// @brief A heavier forward search lowers `H` by exactly one step, so forward stops earlier.
TEST(HalfWayController, AHeavierForwardSearchLowersH) {
    namespace hc = half_way_controller_test;

    HalfWayController controller(hc::kInitialH);
    EXPECT_EQ(controller.update(hc::forward_heavy()), HalfWayMove::Down);
    EXPECT_NEAR(controller.h(), 80.0, hc::kTolerance) << "100 * (1 - 0.2)";
    EXPECT_EQ(controller.moves(), 1U);
}

/// @brief A heavier backward search raises `H` by exactly one step.
TEST(HalfWayController, AHeavierBackwardSearchRaisesH) {
    namespace hc = half_way_controller_test;

    HalfWayController controller(hc::kInitialH);
    EXPECT_EQ(controller.update(hc::backward_heavy()), HalfWayMove::Up);
    EXPECT_NEAR(controller.h(), 120.0, hc::kTolerance) << "100 * (1 + 0.2)";
}

/// @brief Moving the same way repeatedly keeps the full step: it is a reversal that damps.
TEST(HalfWayController, AConsistentDirectionKeepsTheStep) {
    namespace hc = half_way_controller_test;

    HalfWayController controller(hc::kInitialH);
    controller.update(hc::forward_heavy());
    controller.update(hc::forward_heavy());
    EXPECT_NEAR(controller.step(), 0.2, hc::kTolerance);
    EXPECT_NEAR(controller.h(), 64.0, hc::kTolerance) << "100 * 0.8 * 0.8";
}

/// @brief A reversal halves the step before it is taken, so `H` settles instead of oscillating.
TEST(HalfWayController, AReversalHalvesTheStep) {
    namespace hc = half_way_controller_test;

    HalfWayController controller(hc::kInitialH);
    controller.update(hc::forward_heavy());   // down 20 %: 80
    controller.update(hc::backward_heavy());  // reversal: step 0.1, up 10 %: 88
    EXPECT_NEAR(controller.step(), 0.1, hc::kTolerance);
    EXPECT_NEAR(controller.h(), 88.0, hc::kTolerance);

    controller.update(hc::forward_heavy());  // reversal again: step 0.05, down 5 %: 83.6
    EXPECT_NEAR(controller.step(), 0.05, hc::kTolerance);
    EXPECT_NEAR(controller.h(), 83.6, hc::kTolerance);
}

/// @brief However many reversals, the step never drops below its floor.
///
/// RouteOpt's step only ever halves; it gets away with that because it freezes the meet point
/// after the root node. Without the floor, a controller left running would stop being able to
/// move at all after a handful of early reversals.
TEST(HalfWayController, TheStepHasAFloor) {
    namespace hc = half_way_controller_test;

    HalfWayController controller(hc::kInitialH);
    for (int i = 0; i < 20; ++i) {
        controller.update(i % 2 == 0 ? hc::forward_heavy() : hc::backward_heavy());
    }
    EXPECT_NEAR(controller.step(), HalfWayControllerParams{}.min_step, hc::kTolerance);

    // And the floor is a step that still moves H.
    const double before = controller.h();
    controller.update(hc::backward_heavy());  // the same way as the last update (i = 19)
    EXPECT_NEAR(controller.h(), before * 1.025, 1e-9);
}

/// @brief `H` is clamped into `[0.05 R, 0.95 R]` however long one side stays heavier.
///
/// Any `H` is correct; the clamp only stops a runaway from turning the solve into a
/// one-directional search with extra work.
TEST(HalfWayController, HStaysInsideTheRange) {
    namespace hc = half_way_controller_test;

    HalfWayController up(hc::kInitialH);
    for (int i = 0; i < 50; ++i) {
        up.update(hc::backward_heavy());
    }
    EXPECT_NEAR(up.h(), 0.95 * up.range(), hc::kTolerance);

    HalfWayController down(hc::kInitialH);
    for (int i = 0; i < 50; ++i) {
        down.update(hc::forward_heavy());
    }
    EXPECT_NEAR(down.h(), 0.05 * down.range(), hc::kTolerance);
}

/// @brief The knobs are honoured, not just validated.
TEST(HalfWayController, CustomParamsAreUsed) {
    namespace hc = half_way_controller_test;

    HalfWayControllerParams params;
    params.dead_zone = 0.5;
    params.initial_step = 0.1;
    HalfWayController controller(hc::kInitialH, params);

    EXPECT_EQ(controller.update(hc::observed(140, 100)), HalfWayMove::Unchanged)
        << "40 % is inside a 50 % dead zone";
    EXPECT_EQ(controller.update(hc::forward_heavy()), HalfWayMove::Down);
    EXPECT_NEAR(controller.h(), 90.0, hc::kTolerance) << "100 * (1 - 0.1)";
}

// ============================================================================
// The zero-join guard
// ============================================================================

/// @brief No join yet solutions found: `H` moves toward the centre, even with balanced counts.
///
/// Every answer came from a search that ran all the way to a terminal, so nothing crossed `H` and
/// the split bought nothing. The label counts cannot see that -- here they are identical -- which
/// is why the guard is its own trigger.
TEST(HalfWayController, NoJoinMovesHTowardTheCentre) {
    namespace hc = half_way_controller_test;

    HalfWayController controller(hc::kInitialH);
    controller.update(hc::forward_heavy());
    controller.update(hc::forward_heavy());  // 64: well below the centre of 100, and 36 % off it
    ASSERT_NEAR(controller.h(), 64.0, hc::kTolerance);

    EXPECT_EQ(controller.update(hc::observed(100, 100, /*joined=*/0, /*solutions=*/3)),
              HalfWayMove::TowardCentre);
    // A reversal (down, then up): the step halves to 0.1 first. 64 * 1.1 = 70.4.
    EXPECT_NEAR(controller.h(), 70.4, 1e-9);
}

/// @brief The pull toward the centre stops at the centre, and does nothing near it.
TEST(HalfWayController, NoJoinNeverOvershootsTheCentre) {
    namespace hc = half_way_controller_test;

    HalfWayControllerParams params;
    params.dead_zone = 0.01;  // so the first pull is not absorbed by the dead zone around 100
    HalfWayController controller(hc::kInitialH, params);
    controller.update(hc::observed(105, 100));  // 5 % > 1 %: down 20 %, to 80
    ASSERT_NEAR(controller.h(), 80.0, hc::kTolerance);
    controller.update(hc::observed(100, 104));  // reversal, step 0.1: up to 88
    controller.update(hc::observed(100, 104));  // same way: 96.8
    ASSERT_NEAR(controller.h(), 96.8, 1e-9);

    // 96.8 * 1.1 would be 106.48 -- past the centre. It stops at 100 instead.
    EXPECT_EQ(controller.update(hc::observed(100, 100, /*joined=*/0, /*solutions=*/1)),
              HalfWayMove::TowardCentre);
    EXPECT_NEAR(controller.h(), 100.0, hc::kTolerance);

    // At the centre, the guard has nothing left to do.
    EXPECT_EQ(controller.update(hc::observed(100, 100, /*joined=*/0, /*solutions=*/1)),
              HalfWayMove::Unchanged);
}

/// @brief An imbalance outranks the zero-join guard -- the regression test for the order.
///
/// The controller starts at `H0 = R/2`, which is the centre. With the guard checked first, a
/// solve with no join returned "already central, unchanged" and the imbalance rule never ran: the
/// equivalence sweep found instances with four times the forward labels and `H` pinned. Here the
/// counts are 3:1 and nothing joined; the counts must win.
TEST(HalfWayController, AnImbalanceOutranksTheZeroJoinGuard) {
    namespace hc = half_way_controller_test;

    HalfWayController controller(hc::kInitialH);
    EXPECT_EQ(controller.update(hc::observed(300, 100, /*joined=*/0, /*solutions=*/2)),
              HalfWayMove::Down);
    EXPECT_NEAR(controller.h(), 80.0, hc::kTolerance);
}

/// @brief No join and no solutions is not the guard's case: there was simply nothing to find.
///
/// Column generation's last pricing call returns nothing by design. Treating that as "nothing
/// crossed H" would drag `H` toward the centre on every converged iteration.
TEST(HalfWayController, AnEmptyResultIsNotAZeroJoin) {
    namespace hc = half_way_controller_test;

    HalfWayController controller(hc::kInitialH);
    controller.update(hc::forward_heavy());  // 80
    EXPECT_EQ(controller.update(hc::observed(100, 100, /*joined=*/0, /*solutions=*/0)),
              HalfWayMove::Unchanged);
    EXPECT_NEAR(controller.h(), 80.0, hc::kTolerance);
}

// ============================================================================
// The guard: what the controller refuses to learn from
// ============================================================================

/// @brief An unbounded, inexact or empty observation teaches nothing -- not even the step.
TEST(HalfWayController, UntrustworthyObservationsAreSkipped) {
    namespace hc = half_way_controller_test;

    HalfWayController controller(hc::kInitialH);

    auto unbounded = hc::forward_heavy();
    unbounded.bounded = false;
    EXPECT_EQ(controller.update(unbounded), HalfWayMove::Skipped)
        << "an unbounded solve did not use H at all";

    auto truncated = hc::forward_heavy();
    truncated.exact = false;
    EXPECT_EQ(controller.update(truncated), HalfWayMove::Skipped)
        << "a truncated pass measures the cap, not the split";

    EXPECT_EQ(controller.update(hc::observed(0, 100)), HalfWayMove::Skipped);
    EXPECT_EQ(controller.update(hc::observed(100, 0)), HalfWayMove::Skipped);

    EXPECT_NEAR(controller.h(), hc::kInitialH, hc::kTolerance);
    EXPECT_NEAR(controller.step(), 0.2, hc::kTolerance);
    EXPECT_EQ(controller.moves(), 0U);
    EXPECT_EQ(controller.observations(), 4U) << "skipped, but still counted as seen";
}

// ============================================================================
// Freezing and resetting
// ============================================================================

/// @brief A frozen controller holds `H`, and resumes from where it was when thawed.
TEST(HalfWayController, FreezingHoldsH) {
    namespace hc = half_way_controller_test;

    HalfWayController controller(hc::kInitialH);
    controller.update(hc::forward_heavy());  // 80
    controller.set_frozen(true);
    EXPECT_EQ(controller.update(hc::forward_heavy()), HalfWayMove::Frozen);
    EXPECT_NEAR(controller.h(), 80.0, hc::kTolerance);

    controller.set_frozen(false);
    EXPECT_EQ(controller.update(hc::forward_heavy()), HalfWayMove::Down);
    EXPECT_NEAR(controller.h(), 64.0, hc::kTolerance);
}

/// @brief `reset` returns `H` and the step to where they started, and forgets the direction.
TEST(HalfWayController, ResetForgetsWhatWasLearned) {
    namespace hc = half_way_controller_test;

    HalfWayController controller(hc::kInitialH);
    controller.update(hc::forward_heavy());
    controller.update(hc::backward_heavy());  // step now 0.1
    controller.set_frozen(true);

    controller.reset();
    EXPECT_NEAR(controller.h(), hc::kInitialH, hc::kTolerance);
    EXPECT_NEAR(controller.step(), 0.2, hc::kTolerance);
    EXPECT_EQ(controller.observations(), 0U);
    EXPECT_EQ(controller.moves(), 0U);
    EXPECT_TRUE(controller.frozen()) << "reset is about what was learned, not about the switch";

    // The direction is forgotten too: a first move after reset is not a "reversal".
    controller.set_frozen(false);
    controller.update(hc::backward_heavy());
    EXPECT_NEAR(controller.step(), 0.2, hc::kTolerance);
    EXPECT_NEAR(controller.h(), 120.0, hc::kTolerance);
}

/// @brief Every move has a name, for logs and for the Python enum.
TEST(HalfWayController, EveryMoveHasAName) {
    EXPECT_EQ(to_string(HalfWayMove::Unchanged), "unchanged");
    EXPECT_EQ(to_string(HalfWayMove::Skipped), "skipped");
    EXPECT_EQ(to_string(HalfWayMove::Frozen), "frozen");
    EXPECT_EQ(to_string(HalfWayMove::Down), "down");
    EXPECT_EQ(to_string(HalfWayMove::Up), "up");
    EXPECT_EQ(to_string(HalfWayMove::TowardCentre), "toward_centre");
}
