// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Column generation end to end on HiGHS: the tests shared by every master solver, over a model
// the user built in HiGHS.

#include <Highs.h>
#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

#include "rcspp_backends/highs/highs_master_solver.hpp"
#include "test_column_generation_end_to_end.hpp"

namespace {

// A HiGHS model whose rows the user builds, then wraps.
class HighsUserModel {
    public:
        static bool available() { return true; }

        HighsUserModel() { highs_.setOptionValue("output_flag", false); }

        void add_row(double lower, double upper, const std::string& name) {
            const HighsInt row = highs_.getNumRow();
            highs_.addRow(lower, upper, 0, nullptr, nullptr);
            highs_.passRowName(row, name);
            rows_.push_back(row);
        }

        MasterSolver& solver() {
            if (!solver_.has_value()) {
                solver_.emplace(highs_, rows_);
            }
            return *solver_;
        }

    private:
        Highs highs_;
        std::vector<HighsInt> rows_;
        std::optional<HighsMasterSolver> solver_;
};

}  // namespace

INSTANTIATE_TYPED_TEST_SUITE_P(Highs, ColumnGenerationEndToEnd, HighsUserModel);
