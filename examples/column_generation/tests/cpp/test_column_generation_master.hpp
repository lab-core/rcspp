// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The master problem: rows, pricers and their convexity rows, columns sent as CSC batches.

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/column_generation/master_problem.hpp"
#include "test_column_generation_fakes.hpp"

using namespace rcspp;                              // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation;           // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation::testing;  // NOLINT(google-build-using-namespace)

TEST(MasterProblemRows, AddRowsBySense) {
    ScriptedMasterSolver solver;
    MasterProblem master(solver);
    const RowRange equal = master.add_rows(2, RowSense::Equal, 3.0);
    const RowRange greater = master.add_rows(1, RowSense::GreaterEqual, 1.0);
    const RowRange less = master.add_rows(1, RowSense::LessEqual, 5.0);

    EXPECT_EQ(equal.first, 0U);
    EXPECT_EQ(equal.count, 2U);
    EXPECT_EQ(greater.first, 2U);
    EXPECT_EQ(less.first, 3U);
    EXPECT_EQ(master.number_of_rows(), 4U);
    EXPECT_EQ(solver.lower, (std::vector<double>{3.0, 3.0, 1.0, -infinity}));
    EXPECT_EQ(solver.upper, (std::vector<double>{3.0, 3.0, infinity, 5.0}));
}

TEST(MasterProblemRows, AddRowsWithNames) {
    ScriptedMasterSolver solver;
    MasterProblem master(solver);
    const std::vector<double> lower{1.0, 1.0};
    const std::vector<double> upper{infinity, infinity};
    const std::vector<std::string> names{"cover[0]", "cover[1]"};
    master.add_rows(lower, upper, names);
    EXPECT_EQ(solver.names, names);
}

TEST(MasterProblemRows, AddRowsRejectsMismatchedSizes) {
    ScriptedMasterSolver solver;
    MasterProblem master(solver);
    const std::vector<double> two{1.0, 1.0};
    const std::vector<double> one{1.0};
    const std::vector<std::string> one_name{"a"};
    EXPECT_THROW(master.add_rows(two, one), std::invalid_argument);
    EXPECT_THROW(master.add_rows(two, two, one_name), std::invalid_argument);
    EXPECT_EQ(master.number_of_rows(), 0U);
}

TEST(MasterProblemRows, AddRowsNeedsTheCapability) {
    ScriptedMasterSolver solver;
    solver.capabilities_value.add_rows = false;
    MasterProblem master(solver);
    EXPECT_THROW(master.add_rows(1, RowSense::Equal, 1.0), std::logic_error);
}

TEST(MasterProblemPricers, ConvexityRows) {
    ScriptedMasterSolver solver;
    MasterProblem master(solver);
    master.add_rows(2, RowSense::GreaterEqual, 1.0);
    ScriptedPricer first;
    ScriptedPricer second;
    ScriptedPricer third;

    const size_t created = master.add_pricer(first, ConvexityRow{.max_paths = 2.0});
    const size_t existing = master.add_pricer(second, ConvexityRow{.existing_row = 1});
    const size_t none = master.add_pricer(third);

    EXPECT_EQ(created, 0U);
    EXPECT_EQ(master.convexity_row(created), 2U);
    EXPECT_EQ(solver.lower[2], -infinity);
    EXPECT_EQ(solver.upper[2], 2.0);
    EXPECT_EQ(master.convexity_row(existing), 1U);
    EXPECT_FALSE(master.convexity_row(none).has_value());
    EXPECT_EQ(master.number_of_pricers(), 3U);
    EXPECT_EQ(&master.pricer(existing), &second);
    EXPECT_EQ(&master.solver(), &solver);
}

TEST(MasterProblemPricers, RejectsBadConvexityRows) {
    ScriptedMasterSolver solver;
    MasterProblem master(solver);
    master.add_rows(1, RowSense::GreaterEqual, 1.0);
    ScriptedPricer pricer;
    EXPECT_THROW(master.add_pricer(pricer, ConvexityRow{.existing_row = 0, .max_paths = 2.0}),
                 std::invalid_argument);
    EXPECT_THROW(master.add_pricer(pricer, ConvexityRow{.existing_row = 1}), std::out_of_range);
    EXPECT_EQ(master.number_of_pricers(), 0U);
}

TEST(MasterProblemColumns, AddColumnsSendsOneCscBatch) {
    ScriptedMasterSolver solver;
    MasterProblem master(solver);
    master.add_rows(3, RowSense::GreaterEqual, 1.0);
    ScriptedPricer pricer;
    const size_t k = master.add_pricer(pricer, ConvexityRow{.max_paths = 2.0});  // row 3

    master.add_columns(k, {make_path(7.0, {{0, 1.0}}), make_path(9.0, {{1, 1.0}, {2, 2.0}}, -4.0)});

    EXPECT_EQ(solver.add_columns_calls, 1U);
    ASSERT_EQ(solver.columns.size(), 2U);
    EXPECT_EQ(solver.columns[0].id, 0U);
    EXPECT_EQ(solver.columns[0].cost, 7.0);
    EXPECT_EQ(solver.columns[0].lower, 0.0);
    EXPECT_EQ(solver.columns[0].upper, infinity);
    using Entries = std::vector<std::pair<std::uint32_t, double>>;
    EXPECT_EQ(solver.columns[0].entries, (Entries{{0, 1.0}, {3, 1.0}}));
    EXPECT_EQ(solver.columns[1].id, 1U);
    EXPECT_EQ(solver.columns[1].entries, (Entries{{1, 1.0}, {2, 2.0}, {3, 1.0}}));

    ASSERT_EQ(master.number_of_columns(), 2U);
    const MasterColumn& second = master.columns()[1];
    EXPECT_EQ(second.id, 1U);
    EXPECT_EQ(second.pricer, k);
    EXPECT_EQ(second.tag, ColumnTag::Real);
    EXPECT_EQ(second.solution.cost, -4.0);
    EXPECT_EQ(second.solution.column.cost, 9.0);
    EXPECT_TRUE(master.artificial_column_ids().empty());
}

TEST(MasterProblemColumns, AddColumnsWithoutConvexityRow) {
    ScriptedMasterSolver solver;
    MasterProblem master(solver);
    master.add_rows(1, RowSense::GreaterEqual, 1.0);
    ScriptedPricer pricer;
    master.add_pricer(pricer);
    master.add_columns(0, {make_path(5.0, {{0, 1.0}})});
    ASSERT_EQ(solver.columns.size(), 1U);
    EXPECT_EQ(solver.columns[0].entries.size(), 1U);
}

TEST(MasterProblemColumns, AddColumnsRejectsBadInput) {
    ScriptedMasterSolver solver;
    MasterProblem master(solver);
    master.add_rows(1, RowSense::GreaterEqual, 1.0);
    ScriptedPricer pricer;
    master.add_pricer(pricer, ConvexityRow{.max_paths = 1.0});  // row 1

    EXPECT_THROW(master.add_columns(1, {make_path(1.0, {{0, 1.0}})}), std::out_of_range);
    EXPECT_THROW(master.add_columns(0, {make_path(1.0, {{2, 1.0}})}), std::out_of_range);
    EXPECT_THROW(master.add_columns(0, {make_path(1.0, {{1, 1.0}})}), std::invalid_argument);
    master.add_columns(0, {});
    EXPECT_EQ(solver.add_columns_calls, 0U);
    EXPECT_EQ(master.number_of_columns(), 0U);
}

TEST(MasterProblemColumns, AddArtificialColumns) {
    ScriptedMasterSolver solver;
    MasterProblem master(solver);
    master.add_rows(2, RowSense::Equal, 1.0);
    const std::vector<size_t> rows{0, 1};
    const std::vector<double> coefficients{1.0, -1.0};
    const std::vector<double> one_coefficient{1.0};
    const std::vector<size_t> bad_row{2};

    EXPECT_THROW(master.add_artificial_columns(rows, one_coefficient, 5.0), std::invalid_argument);
    EXPECT_THROW(master.add_artificial_columns(bad_row, one_coefficient, 5.0), std::out_of_range);

    master.add_artificial_columns(rows, coefficients, 5.0);
    ASSERT_EQ(master.number_of_columns(), 2U);
    EXPECT_EQ(master.columns()[1].tag, ColumnTag::Artificial);
    EXPECT_EQ(master.columns()[1].pricer, MasterProblem::no_pricer);
    EXPECT_EQ(solver.columns[1].cost, 5.0);
    using Entries = std::vector<std::pair<std::uint32_t, double>>;
    EXPECT_EQ(solver.columns[1].entries, (Entries{{1, -1.0}}));
    EXPECT_EQ(std::vector<ColumnId>(master.artificial_column_ids().begin(),
                                    master.artificial_column_ids().end()),
              (std::vector<ColumnId>{0, 1}));
}
