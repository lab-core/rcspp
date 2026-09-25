// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The ng growth rule a column-generation driver applies between convergences
// (examples/cpp/vrp/ng_growth.hpp): which cycles of an LP solution grow which neighbourhoods.

#include <gtest/gtest.h>

#include <cstddef>
#include <map>
#include <set>
#include <span>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "vrp/ng_growth.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace ng_growth_test {

using Table = std::map<size_t, std::set<size_t>>;

/// @brief Nodes 0..n-1, every neighbourhood empty.
inline Table empty_table(size_t n) {
    Table table;
    for (size_t node = 0; node < n; ++node) {
        table[node] = {};
    }
    return table;
}

}  // namespace ng_growth_test

/// @brief One cycle 1 -> 2 -> 1 adds 1 to the neighbourhood of 2, and nothing else.
TEST(NgGrowth, OneCycleAddsItsNodeToEveryInteriorNode) {
    namespace gt = ng_growth_test;

    const auto growth =
        grow_ng(gt::empty_table(5), {{.value = 0.5, .nodes = {0, 1, 2, 1, 4}}}, NgGrowthParams{});

    EXPECT_EQ(growth.members_added, 1U);
    EXPECT_EQ(growth.cycles_used, 1U);
    EXPECT_EQ(growth.neighborhoods.at(2), (std::set<size_t>{1}));
    EXPECT_TRUE(growth.neighborhoods.at(1).empty());
    EXPECT_FALSE(is_ng_feasible(std::span<const size_t>(std::vector<size_t>{0, 1, 2, 1, 4}),
                                growth.neighborhoods))
        << "the grown table must forbid the cycle it grew from";
}

/// @brief Longer cycles than `max_cycle_length` are left out, once a shorter one is used.
TEST(NgGrowth, OnlyTheShortestCyclesAndThoseWithinTheCapAreUsed) {
    namespace gt = ng_growth_test;

    NgGrowthParams params;
    params.max_cycle_length = 2;
    // 1 -> 2 -> 1 (length 1), and 3 -> 4 -> 5 -> 6 -> 3 (length 3), in two columns.
    const auto growth = grow_ng(
        gt::empty_table(8),
        {{.value = 0.4, .nodes = {0, 1, 2, 1, 7}}, {.value = 0.6, .nodes = {0, 3, 4, 5, 6, 3, 7}}},
        params);

    EXPECT_EQ(growth.neighborhoods.at(2), (std::set<size_t>{1}));
    EXPECT_TRUE(growth.neighborhoods.at(4).empty()) << "length 3 is over the cap of 2";
    EXPECT_EQ(growth.members_added, 1U);
}

/// @brief The shortest cycles present are used even when they exceed the cap.
///
/// Otherwise a solution whose only cycles are long would stop growth with cycles still in it.
TEST(NgGrowth, TheShortestCyclesAreUsedEvenOverTheCap) {
    namespace gt = ng_growth_test;

    NgGrowthParams params;
    params.max_cycle_length = 1;
    const auto growth =
        grow_ng(gt::empty_table(8), {{.value = 1.0, .nodes = {0, 3, 4, 5, 6, 3, 7}}}, params);

    EXPECT_EQ(growth.members_added, 3U) << "3 joins the neighbourhoods of 4, 5 and 6";
    EXPECT_EQ(growth.neighborhoods.at(5), (std::set<size_t>{3}));
}

/// @brief A column the LP does not use contributes nothing, however cyclic.
TEST(NgGrowth, AZeroValueColumnIsIgnored) {
    namespace gt = ng_growth_test;

    const auto growth =
        grow_ng(gt::empty_table(5), {{.value = 0.0, .nodes = {0, 1, 2, 1, 4}}}, NgGrowthParams{});

    EXPECT_EQ(growth.members_added, 0U);
    EXPECT_EQ(growth.neighborhoods, gt::empty_table(5));
}

/// @brief A neighbourhood already at `max_ng_size` is left as it is.
TEST(NgGrowth, AFullNeighbourhoodIsNotGrown) {
    namespace gt = ng_growth_test;

    auto table = gt::empty_table(6);
    table[2] = {3, 4};
    NgGrowthParams params;
    params.max_ng_size = 2;
    const auto growth = grow_ng(table, {{.value = 1.0, .nodes = {0, 1, 2, 1, 5}}}, params);

    EXPECT_EQ(growth.members_added, 0U);
    EXPECT_EQ(growth.neighborhoods.at(2), (std::set<size_t>{3, 4}));
}

/// @brief Only the highest-value cyclic columns count, up to `max_columns_with_a_cycle`.
TEST(NgGrowth, TheColumnCapKeepsTheHighestValueCyclicColumns) {
    namespace gt = ng_growth_test;

    NgGrowthParams params;
    params.max_columns_with_a_cycle = 1;
    const auto growth = grow_ng(gt::empty_table(8),
                                {{.value = 0.2, .nodes = {0, 1, 2, 1, 7}},
                                 {.value = 0.9, .nodes = {0, 3, 4, 3, 7}},
                                 {.value = 0.95, .nodes = {0, 5, 7}}},  // no cycle: not counted
                                params);

    EXPECT_EQ(growth.neighborhoods.at(4), (std::set<size_t>{3})) << "the 0.9 column is used";
    EXPECT_TRUE(growth.neighborhoods.at(2).empty()) << "the 0.2 column is past the cap";
}

/// @brief Whatever the input, the grown table is one `set_ng_neighborhoods` accepts.
TEST(NgGrowth, EveryGrowthPassesTheSwapRule) {
    namespace gt = ng_growth_test;

    // 9 is not a node of the table: a cycle through it, and one with it inside, are skipped there.
    const auto table = gt::empty_table(6);
    const auto growth = grow_ng(
        table,
        {{.value = 1.0, .nodes = {0, 9, 2, 9, 5}}, {.value = 0.5, .nodes = {0, 1, 9, 1, 5}}},
        NgGrowthParams{});

    EXPECT_NO_THROW(check_ng_update(table, growth.neighborhoods));
    EXPECT_EQ(growth.members_added, 0U);
}

/// @brief `reduce_to_support` keeps adjacent members and the node itself, and drops the rest.
TEST(NgGrowth, ReduceToSupportKeepsOnlyAdjacentMembers) {
    namespace gt = ng_growth_test;

    auto table = gt::empty_table(6);
    table[2] = {1, 2, 4};  // 1 is adjacent to 2 in the support graph, 4 is not
    const auto reduced = reduce_to_support(
        table,
        {{.value = 0.5, .nodes = {0, 1, 2, 5}}, {.value = 0.0, .nodes = {0, 4, 2, 5}}});

    EXPECT_EQ(reduced.at(2), (std::set<size_t>{1, 2})) << "the 0-value column adds no adjacency";
    EXPECT_EQ(reduced.size(), table.size()) << "keys are kept";
}
