// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <vector>

#include "rcspp/algorithm/tabu_list.hpp"

using namespace rcspp;

namespace {
constexpr int kTabuSeed = 12345;
}  // namespace

// add() marks an arc tabu; unrelated arcs stay free.
TEST(TabuList, AddAndIsTabu) {
    TabuList tabu(kTabuSeed);
    EXPECT_FALSE(tabu.is_tabu(7));
    tabu.add(7, /*base_tenure=*/3, /*noise=*/false);
    EXPECT_TRUE(tabu.is_tabu(7));
    EXPECT_FALSE(tabu.is_tabu(8));
}

// age() forbids an arc for `tenure` cycles, then expires it and invokes on_expire once.
TEST(TabuList, AgeExpiresAfterTenure) {
    TabuList tabu(kTabuSeed);
    tabu.add(42, /*base_tenure=*/3, /*noise=*/false);  // extra_ == 0 -> tenure 3
    EXPECT_TRUE(tabu.is_tabu(42));

    std::vector<size_t> expired;
    auto on_expire = [&](size_t id) { expired.push_back(id); };

    tabu.age(on_expire);
    EXPECT_TRUE(tabu.is_tabu(42));  // 3 -> 2
    tabu.age(on_expire);
    EXPECT_TRUE(tabu.is_tabu(42));  // 2 -> 1
    tabu.age(on_expire);
    EXPECT_FALSE(tabu.is_tabu(42));  // expired

    ASSERT_EQ(expired.size(), 1u);
    EXPECT_EQ(expired[0], 42u);
    EXPECT_TRUE(tabu.empty());
}

// grow_extra() is additive and symmetric with shrink_extra(): the adaptive tenure equals the
// number of grow calls (1, 2, ..., 10) and each shrink cancels one grow, so it stays bounded by
// the iteration count and cannot ratchet up.
TEST(TabuList, AdaptiveTenureIsAdditiveAndSymmetric) {
    TabuList tabu(kTabuSeed);
    EXPECT_EQ(tabu.extra(), 0u);

    for (int i = 1; i <= 10; ++i) {
        tabu.grow_extra();
        EXPECT_EQ(tabu.extra(), static_cast<size_t>(i));  // additive: 1,2,...,10 (not 1,3,7,...)
    }
    EXPECT_EQ(tabu.extra(), 10u);

    // shrink cancels grow one-for-one and saturates at zero.
    tabu.shrink_extra();
    EXPECT_EQ(tabu.extra(), 9u);
    for (int i = 0; i < 9; ++i) {
        tabu.shrink_extra();
    }
    EXPECT_EQ(tabu.extra(), 0u);
    tabu.shrink_extra();
    EXPECT_EQ(tabu.extra(), 0u);
}

// add() must not overflow when base_tenure + extra_ exceeds INT_MAX: the assigned tenure must
// stay within the ±1 jitter window of the requested value (never wrapping to a garbage value).
TEST(TabuList, AddDoesNotOverflowWithLargeTenure) {
    TabuList tabu(kTabuSeed);
    const size_t huge = std::numeric_limits<size_t>::max() / 2;  // >> INT_MAX
    const size_t assigned = tabu.add(1, huge, /*noise=*/true);
    EXPECT_GE(assigned, huge - 1);
    EXPECT_LE(assigned, huge + 1);
    EXPECT_TRUE(tabu.is_tabu(1));
}
