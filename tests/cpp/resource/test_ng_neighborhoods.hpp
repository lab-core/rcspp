// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The ng utilities a column-generation driver grows neighbourhoods with: finding the cycles in a
// path, testing a path against an ng memory, and swapping a built graph's neighbourhoods.

#include <gtest/gtest.h>

#include <cstddef>
#include <map>
#include <set>
#include <span>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

/// @brief Each pair of consecutive visits to one node is one cycle, with the nodes between it.
TEST(NgNeighborhoods, FindCyclesReportsEachConsecutiveRevisit) {
    const std::vector<size_t> elementary{0, 1, 2, 3};
    EXPECT_TRUE(find_cycles(elementary).empty());

    const std::vector<size_t> once{0, 1, 2, 1, 3};
    const auto one = find_cycles(once);
    ASSERT_EQ(one.size(), 1U);
    EXPECT_EQ(one[0].node, 1U);
    EXPECT_EQ(one[0].first, 1U);
    EXPECT_EQ(one[0].last, 3U);
    EXPECT_EQ(one[0].interior, (std::vector<size_t>{2}));
    EXPECT_EQ(one[0].length(), 1U);

    const std::vector<size_t> thrice{0, 1, 2, 1, 4, 5, 1, 6};
    const auto two = find_cycles(thrice);
    ASSERT_EQ(two.size(), 2U) << "one cycle per consecutive pair of visits";
    EXPECT_EQ(two[0].interior, (std::vector<size_t>{2}));
    EXPECT_EQ(two[1].interior, (std::vector<size_t>{4, 5}));

    const std::vector<size_t> overlapping{0, 1, 2, 3, 1, 2, 4};
    const auto both = find_cycles(overlapping);
    ASSERT_EQ(both.size(), 2U);
    EXPECT_EQ(both[0].node, 1U);
    EXPECT_EQ(both[0].interior, (std::vector<size_t>{2, 3}));
    EXPECT_EQ(both[1].node, 2U);
    EXPECT_EQ(both[1].interior, (std::vector<size_t>{3, 1}));
}

/// @brief A revisit is forbidden exactly when every node since the first visit remembers it.
TEST(NgNeighborhoods, IsNgFeasibleForgetsWhatANeighbourhoodDoesNotHold) {
    const std::vector<size_t> revisit{0, 1, 2, 1, 3};

    // 2 forgets 1 on arrival, so returning to 1 is allowed.
    const std::map<size_t, std::set<size_t>> forgets{{0, {}}, {1, {}}, {2, {}}, {3, {}}};
    EXPECT_TRUE(is_ng_feasible(std::span<const size_t>(revisit), forgets));

    // 2 remembers 1, so the return is forbidden.
    const std::map<size_t, std::set<size_t>> remembers{{0, {}}, {1, {}}, {2, {1}}, {3, {}}};
    EXPECT_FALSE(is_ng_feasible(std::span<const size_t>(revisit), remembers));

    // Returning straight away is always forbidden: a node always keeps itself on arrival.
    const std::vector<size_t> immediate{0, 1, 1, 3};
    EXPECT_FALSE(is_ng_feasible(std::span<const size_t>(immediate), forgets));

    // A node the table does not name has no forbidden set, as under the presets.
    const std::map<size_t, std::set<size_t>> unnamed{{0, {}}, {2, {1}}, {3, {}}};
    EXPECT_TRUE(is_ng_feasible(std::span<const size_t>(revisit), unnamed));
}
