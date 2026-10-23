// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The column generation interfaces: the column batch view and the defaults of the base classes.

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

#include "rcspp/column_generation/pricer.hpp"
#include "rcspp/column_generation/solver_backend.hpp"
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
    ScriptedBackend backend;
    std::vector<double> ray(1);
    EXPECT_FALSE(backend.farkas(ray));
    backend.set_option("presolve", OptionValue{false});
    ASSERT_EQ(backend.options.size(), 1U);
    EXPECT_EQ(backend.options[0].first, "presolve");

    ScriptedPricer pricer;
    pricer.update(PricingChanges{.costs = true});  // the base class ignores it
    EXPECT_DOUBLE_EQ(pricer.cost_scale(), 1.0);
}

TEST(ColumnGenerationInterfaces, AddingRowsIsOptional) {
    WrappedModelBackend backend;
    EXPECT_FALSE(backend.capabilities().add_rows);
    EXPECT_THROW(backend.add_rows(RowBatch{}), std::logic_error);
    // The other methods of this minimal backend do nothing.
    std::vector<double> none;
    backend.row_bounds(none, none);
    backend.add_columns(ColumnBatch{});
    EXPECT_EQ(backend.solve_relaxation(RelaxationParams{}), SolverStatus::Optimal);
    EXPECT_EQ(backend.objective(), 0.0);
    backend.duals(none);
    backend.primal({}, none);
    EXPECT_EQ(backend.solve_integer(IntegerParams{}), SolverStatus::Optimal);
    backend.set_option("threads", OptionValue{std::int64_t{1}});
    EXPECT_EQ(backend.num_rows(), 0U);
}
