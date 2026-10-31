// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Tests for rcspp::MemoryLimitHelper's forced pressure, the seam tests use to trim the label
// queues on chosen iterations.

#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

#include "rcspp/utils/memory.hpp"

/// @brief Forced pressure fires on the listed checks only, at the check cadence, never exceeds the
///        limit, and is gone after the next resolve.
TEST(MemoryPressure, ForcedPressureFiresOnTheChecksAsked) {
    using rcspp::MemoryState;
    rcspp::MemoryLimitHelper limit;
    limit.resolve(0.0, false, false, 0.9, 0.8, /*check_interval_in=*/2);
    EXPECT_EQ(limit.effective_limit, 0U);
    EXPECT_EQ(limit.check(2), MemoryState::Ok);  // no limit: nothing is read

    limit.force_pressure_on_checks({2, 4});
    EXPECT_GT(limit.effective_limit, 0U);
    EXPECT_FALSE(limit.is_exceeded());
    std::vector<MemoryState> seen;
    for (size_t iteration = 0; iteration <= 10; ++iteration) {
        seen.push_back(limit.check(iteration));  // the checks fall on 2, 4, 6, 8 and 10
    }
    const auto ok = MemoryState::Ok;
    const auto pressure = MemoryState::UnderPressure;
    // The second and fourth checks are iterations 4 and 8.
    EXPECT_EQ(seen,
              (std::vector<MemoryState>{ok, ok, ok, ok, pressure, ok, ok, ok, pressure, ok, ok}));
    EXPECT_FALSE(limit.is_exceeded());

    limit.resolve(0.0, false, false, 0.9, 0.8, 2);  // the next solve: the forcing is gone
    EXPECT_EQ(limit.effective_limit, 0U);
    EXPECT_EQ(limit.check(4), MemoryState::Ok);
}
