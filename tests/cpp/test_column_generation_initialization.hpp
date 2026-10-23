// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The default master initialization: big-M artificial columns.

#include <gtest/gtest.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "rcspp/column_generation/master_initialization.hpp"
#include "test_column_generation_fakes.hpp"

using namespace rcspp;                              // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation;           // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation::testing;  // NOLINT(google-build-using-namespace)

TEST(ArtificialColumnsInitialization, OneColumnPerRowThatZeroViolates) {
    ScriptedBackend backend;
    MasterProblem master(backend);
    master.add_rows(1, RowSense::Equal, 3.0);         // row 0: violated, +1
    master.add_rows(1, RowSense::GreaterEqual, 1.0);  // row 1: violated, +1
    master.add_rows(1, RowSense::LessEqual, 5.0);     // row 2: satisfied by 0
    master.add_rows(1, RowSense::LessEqual, -2.0);    // row 3: violated, -1
    const std::vector<double> lower{-1.0};
    const std::vector<double> upper{1.0};
    master.add_rows(lower, upper);  // row 4: satisfied by 0

    ArtificialColumns initialization;
    initialization.initialize(master, ToleranceContext{2.0});

    using Entries = std::vector<std::pair<std::uint32_t, double>>;
    ASSERT_EQ(backend.columns.size(), 3U);
    EXPECT_EQ(backend.columns[0].entries, (Entries{{0, 1.0}}));
    EXPECT_EQ(backend.columns[1].entries, (Entries{{1, 1.0}}));
    EXPECT_EQ(backend.columns[2].entries, (Entries{{3, -1.0}}));
    EXPECT_EQ(backend.columns[0].cost, 2e6);
    EXPECT_EQ(master.artificial_column_ids().size(), 3U);
}

TEST(ArtificialColumnsInitialization, ExplicitCostAndNothingToDo) {
    const ArtificialColumns explicit_cost(50.0);
    EXPECT_EQ(explicit_cost.cost(ToleranceContext{2.0}), 50.0);

    ScriptedBackend backend;
    MasterProblem master(backend);
    master.add_rows(1, RowSense::LessEqual, 5.0);
    ArtificialColumns initialization;
    initialization.initialize(master, ToleranceContext{});
    EXPECT_EQ(backend.add_columns_calls, 0U);
}
