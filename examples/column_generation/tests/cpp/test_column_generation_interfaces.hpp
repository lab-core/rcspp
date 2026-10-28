// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The column generation interfaces: the column batch view and the defaults of the base classes.

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

#include "rcspp/column_generation/master_solver.hpp"
#include "rcspp/column_generation/pricer.hpp"
#include "rcspp/column_generation/types.hpp"
#include "test_column_generation_fakes.hpp"

using namespace rcspp;                              // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation;           // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation::testing;  // NOLINT(google-build-using-namespace)

TEST(ColumnGenerationTypes, ColumnBatchSizeIsTheNumberOfIds) {
    const std::vector<ColumnId> ids{4, 5};
    ColumnBatch batch;
    batch.ids = ids;
    EXPECT_EQ(batch.size(), 2U);
}

TEST(ColumnGenerationInterfaces, DefaultsOfTheBaseClasses) {
    ScriptedMasterSolver solver;
    std::vector<double> ray(1);
    EXPECT_FALSE(solver.farkas(ray));
    solver.set_option("presolve", OptionValue{false});
    ASSERT_EQ(solver.options.size(), 1U);
    EXPECT_EQ(solver.options[0].first, "presolve");

    ScriptedPricer pricer;
    pricer.update(PricingChanges{.costs = true});  // the base class ignores it
    EXPECT_DOUBLE_EQ(pricer.cost_scale(), 1.0);
}

TEST(ColumnGenerationInterfaces, AddingRowsIsOptional) {
    WrappedModelMasterSolver solver;
    EXPECT_FALSE(solver.capabilities().add_rows);
    EXPECT_THROW(solver.add_rows(RowBatch{}), std::logic_error);
    // The other methods of this minimal master solver do nothing.
    std::vector<double> none;
    solver.row_bounds(none, none);
    solver.add_columns(ColumnBatch{});
    EXPECT_EQ(solver.solve_relaxation(RelaxationParams{}), SolverStatus::Optimal);
    EXPECT_EQ(solver.objective(), 0.0);
    solver.duals(none);
    solver.primal({}, none);
    EXPECT_EQ(solver.solve_integer(IntegerParams{}), SolverStatus::Optimal);
    solver.set_option("threads", OptionValue{std::int64_t{1}});
    EXPECT_EQ(solver.number_of_rows(), 0U);
    solver.restore_user_model();  // changes nothing, so restores nothing
    EXPECT_EQ(solver.dual_tolerance(), 0.0);
}
