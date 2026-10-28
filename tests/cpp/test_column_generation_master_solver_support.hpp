// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// What every master solver shares: the solver column of each id, and the bounds a row can hold.

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include "rcspp/column_generation/master_solver_support.hpp"

using namespace rcspp;                     // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation;  // NOLINT(google-build-using-namespace)

namespace {

// A batch of two columns with one entry each, in rows 0 and @p second_row.
struct TwoColumnBatch {
        std::array<ColumnId, 2> ids;
        std::array<double, 2> cost{1.0, 1.0};
        std::array<double, 2> lower{0.0, 0.0};
        std::array<double, 2> upper{1.0, 1.0};
        std::array<std::uint32_t, 3> starts{0, 1, 2};
        std::array<std::uint32_t, 2> row_indices;
        std::array<double, 2> values{1.0, 1.0};

        TwoColumnBatch(ColumnId first, ColumnId second, std::uint32_t second_row)
            : ids{first, second}, row_indices{0, second_row} {}

        [[nodiscard]] ColumnBatch batch() const {
            return ColumnBatch{.ids = ids,
                               .cost = cost,
                               .lower = lower,
                               .upper = upper,
                               .starts = starts,
                               .row_indices = row_indices,
                               .values = values};
        }
};

}  // namespace

TEST(ColumnGenerationMasterSolverSupport, SolverColumnsByIdInTheOrderAdded) {
    SolverColumns<int> columns;
    constexpr int first_column = 10;
    constexpr int second_column = 20;
    columns.add(3, first_column);
    columns.add(1, second_column);

    EXPECT_EQ(columns.at(3), first_column);
    EXPECT_EQ(columns.at(1), second_column);
    EXPECT_EQ(std::vector<int>(columns.all().begin(), columns.all().end()),
              (std::vector<int>{first_column, second_column}));
    EXPECT_THROW((void)columns.at(2), std::out_of_range);
}

TEST(ColumnGenerationMasterSolverSupport, ABatchIsCheckedAsAWhole) {
    SolverColumns<int> columns;
    columns.add(1, 0);
    constexpr size_t rows = 3;

    EXPECT_NO_THROW(columns.check(TwoColumnBatch(2, 3, 2).batch(), rows));
    EXPECT_THROW(columns.check(TwoColumnBatch(2, 1, 0).batch(), rows), std::invalid_argument);
    EXPECT_THROW(columns.check(TwoColumnBatch(2, 2, 0).batch(), rows), std::invalid_argument);
    EXPECT_THROW(columns.check(TwoColumnBatch(2, 3, rows).batch(), rows), std::out_of_range);
}

TEST(ColumnGenerationMasterSolverSupport, NumbersNoSolverCanHoldAreRefused) {
    const SolverColumns<int> columns;
    constexpr size_t rows = 3;
    constexpr double infinity = std::numeric_limits<double>::infinity();
    const double not_a_number = std::numeric_limits<double>::quiet_NaN();

    TwoColumnBatch nan_cost(1, 2, 1);
    nan_cost.cost[1] = not_a_number;
    EXPECT_THROW(columns.check(nan_cost.batch(), rows), std::invalid_argument);

    TwoColumnBatch infinite_coefficient(1, 2, 1);
    infinite_coefficient.values[0] = infinity;
    EXPECT_THROW(columns.check(infinite_coefficient.batch(), rows), std::invalid_argument);

    TwoColumnBatch crossed_bounds(1, 2, 1);
    crossed_bounds.lower[0] = 2.0;
    EXPECT_THROW(columns.check(crossed_bounds.batch(), rows), std::invalid_argument);

    TwoColumnBatch unbounded(1, 2, 1);
    unbounded.upper[0] = infinity;
    EXPECT_NO_THROW(columns.check(unbounded.batch(), rows));
}

TEST(ColumnGenerationMasterSolverSupport, BoundsARowCanHold) {
    constexpr double infinity = std::numeric_limits<double>::infinity();
    const double not_a_number = std::numeric_limits<double>::quiet_NaN();

    EXPECT_TRUE(row_bounds_can_be_held(1.0, 1.0));
    EXPECT_TRUE(row_bounds_can_be_held(1.0, 2.0));
    EXPECT_TRUE(row_bounds_can_be_held(-infinity, infinity));
    EXPECT_FALSE(row_bounds_can_be_held(2.0, 1.0));
    EXPECT_FALSE(row_bounds_can_be_held(infinity, infinity));
    EXPECT_FALSE(row_bounds_can_be_held(-infinity, -infinity));
    EXPECT_FALSE(row_bounds_can_be_held(not_a_number, 1.0));
    EXPECT_FALSE(row_bounds_can_be_held(1.0, not_a_number));
}
