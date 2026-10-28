// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Column generation end to end on Gurobi: the tests shared by every master solver, over a model
// the user built in Gurobi. Skipped without a Gurobi license.

#include <gtest/gtest.h>
#include <gurobi_c++.h>

#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "gurobi_environment.hpp"
#include "rcspp/column_generation/gurobi/gurobi_master_solver.hpp"
#include "test_column_generation_end_to_end.hpp"

namespace rcspp::column_generation::testing {

namespace {

// A Gurobi model whose rows the user builds, then wraps.
class GurobiUserModel {
    public:
        static bool available() { return gurobi_environment() != nullptr; }

        GurobiUserModel() : model_(*gurobi_environment()) {}

        // An `=` row for equal bounds, else `≥` for a finite lower bound, else `≤`.
        void add_row(double lower, double upper, const std::string& name) {
            if (lower == upper) {
                rows_.push_back(model_.addConstr(GRBLinExpr(), GRB_EQUAL, lower, name));
            } else if (std::isfinite(lower)) {
                rows_.push_back(model_.addConstr(GRBLinExpr(), GRB_GREATER_EQUAL, lower, name));
            } else {
                rows_.push_back(model_.addConstr(GRBLinExpr(), GRB_LESS_EQUAL, upper, name));
            }
        }

        MasterSolver& solver() {
            if (!solver_.has_value()) {
                solver_.emplace(model_, rows_);
            }
            return *solver_;
        }

    private:
        GRBModel model_;
        std::vector<GRBConstr> rows_;
        std::optional<GurobiMasterSolver> solver_;
};

}  // namespace

INSTANTIATE_TYPED_TEST_SUITE_P(Gurobi, ColumnGenerationEndToEnd, GurobiUserModel);

}  // namespace rcspp::column_generation::testing
