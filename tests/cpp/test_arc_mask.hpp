// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The arc mask on its own: its states, its count, and the ids it was not sized for.

#include <gtest/gtest.h>

#include <stdexcept>

#include "rcspp/rcspp.hpp"

/// @brief A mask starts open; setting a state counts the arcs masked, once each.
TEST(ArcMask, CountsTheArcsItMasks) {
    rcspp::ArcMask mask(4);
    EXPECT_TRUE(mask.empty());
    mask.set(1, rcspp::ArcMask::State::Hard);
    mask.set(2, rcspp::ArcMask::State::Soft);
    mask.set(2, rcspp::ArcMask::State::Hard);  // still one arc
    EXPECT_EQ(mask.count(), 2U);
    EXPECT_TRUE(mask.is_hard(1));
    EXPECT_TRUE(mask.is_hard(2));
    EXPECT_FALSE(mask.is_soft(2));
    mask.set(1, rcspp::ArcMask::State::Open);
    EXPECT_EQ(mask.count(), 1U);
    EXPECT_EQ(mask.state(1), rcspp::ArcMask::State::Open);
    mask.reset(4);
    EXPECT_TRUE(mask.empty());
    EXPECT_FALSE(mask.is_hard(2));
}

/// @brief An id the mask was not sized for reads open, and cannot be set.
TEST(ArcMask, AnIdBeyondItsSizeIsOpen) {
    rcspp::ArcMask mask(2);
    EXPECT_EQ(mask.state(5), rcspp::ArcMask::State::Open);
    EXPECT_FALSE(mask.is_hard(5));
    EXPECT_THROW(mask.set(5, rcspp::ArcMask::State::Hard), std::out_of_range);
    EXPECT_TRUE(rcspp::ArcMask{}.empty());
}
