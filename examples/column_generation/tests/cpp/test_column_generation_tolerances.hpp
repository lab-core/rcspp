// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The tolerances of a column generation run: absolute and relative to the cost scale.

#include <gtest/gtest.h>

#include "rcspp/column_generation/tolerances.hpp"
#include "test_column_generation_fakes.hpp"

using namespace rcspp;                              // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation;           // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation::testing;  // NOLINT(google-build-using-namespace)

TEST(ColumnGenerationTolerances, AbsoluteIgnoresTheScale) {
    const AbsoluteTolerances tolerances;
    const ToleranceContext context{1000.0};
    EXPECT_DOUBLE_EQ(tolerances.reduced_cost(context), 1e-9);
    EXPECT_DOUBLE_EQ(tolerances.feasibility(context), 1e-6);
    EXPECT_DOUBLE_EQ(tolerances.optimality_gap(context), 1e-6);
}

TEST(ColumnGenerationTolerances, RelativeScalesCostTolerancesOnly) {
    const RelativeTolerances tolerances(1e-8, 1e-5, 1e-4);
    const ToleranceContext context{100.0};
    EXPECT_DOUBLE_EQ(tolerances.reduced_cost(context), 1e-6);
    EXPECT_DOUBLE_EQ(tolerances.feasibility(context), 1e-5);
    EXPECT_DOUBLE_EQ(tolerances.optimality_gap(context), 1e-2);
}
