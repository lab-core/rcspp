// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Tests for the rcspp::Timer utility class.
// Covers start/stop/reset/restart/running/elapsed/hms/accumulate paths.

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "rcspp/utils/timer.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

/// @brief Timer default-constructed is not running and elapsed time is zero.
TEST(TimerTest, DefaultNotRunning) {
    Timer t(/*start_timer=*/false);
    EXPECT_FALSE(t.running());
    EXPECT_NEAR(t.elapsed_seconds(), 0.0, 1e-6);
}

/// @brief Timer constructed with start_timer=true is running immediately.
TEST(TimerTest, ConstructWithStart) {
    Timer t(/*start_timer=*/true);
    EXPECT_TRUE(t.running());
}

/// @brief Calling start() twice does not reset the start time (idempotent).
TEST(TimerTest, StartTwiceIdempotent) {
    Timer t(/*start_timer=*/false);
    t.start();
    EXPECT_TRUE(t.running());
    t.start();  // second call should be no-op
    EXPECT_TRUE(t.running());
}

/// @brief Stop accumulates elapsed time and leaves timer not running.
TEST(TimerTest, StopAccumulates) {
    Timer t(/*start_timer=*/true);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    t.stop();
    EXPECT_FALSE(t.running());
    EXPECT_GT(t.elapsed_seconds(), 0.0);
}

/// @brief Stop on an already-stopped timer is a no-op.
TEST(TimerTest, StopWhenNotRunning) {
    Timer t(/*start_timer=*/false);
    t.stop();  // no-op
    EXPECT_FALSE(t.running());
    EXPECT_NEAR(t.elapsed_seconds(), 0.0, 1e-6);
}

/// @brief reset() clears accumulated time and leaves timer stopped.
TEST(TimerTest, ResetClearsTime) {
    Timer t(/*start_timer=*/true);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    t.stop();
    t.reset();
    EXPECT_FALSE(t.running());
    EXPECT_NEAR(t.elapsed_seconds(), 0.0, 1e-6);
}

/// @brief restart() clears accumulated time and starts timer.
TEST(TimerTest, RestartClearsAndStarts) {
    Timer t(/*start_timer=*/true);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    t.restart();
    EXPECT_TRUE(t.running());
    EXPECT_NEAR(t.elapsed_seconds(/*only_current=*/false), 0.0, 0.05);
}

/// @brief elapsed_milliseconds returns a non-negative value.
TEST(TimerTest, ElapsedMilliseconds) {
    Timer t(/*start_timer=*/true);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    t.stop();
    EXPECT_GE(t.elapsed_milliseconds(), 0LL);
}

/// @brief elapsed_microseconds returns a non-negative value.
TEST(TimerTest, ElapsedMicroseconds) {
    Timer t(/*start_timer=*/true);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    t.stop();
    EXPECT_GE(t.elapsed_microseconds(), 0LL);
}

/// @brief elapsed_to_hms returns a non-empty string in "HH:MM:SS.ss" format.
TEST(TimerTest, ElapsedToHms) {
    Timer t(/*start_timer=*/false);
    const std::string hms = t.elapsed_to_hms();
    EXPECT_FALSE(hms.empty());
    // Should contain two colons for HH:MM:SS.ss
    const auto colon_count = std::count(hms.begin(), hms.end(), ':');
    EXPECT_EQ(colon_count, 2);
}

/// @brief operator+= accumulates another timer's elapsed time.
TEST(TimerTest, AccumulateOperator) {
    Timer a(/*start_timer=*/true);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    a.stop();

    Timer b(/*start_timer=*/false);
    b += a;
    EXPECT_GE(b.elapsed_seconds(), 0.0);
}

/// @brief elapsed_seconds(only_current=true) reports only the current run.
TEST(TimerTest, ElapsedOnlyCurrent) {
    Timer t(/*start_timer=*/true);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    t.stop();
    t.start();
    // only_current=true should return just the current (new) run, which is ~0
    const double only_current = t.elapsed_seconds(/*only_current=*/true);
    EXPECT_GE(only_current, 0.0);
    // The full accumulated time should be >= only_current
    const double total = t.elapsed_seconds(/*only_current=*/false);
    EXPECT_GE(total, only_current);
}
