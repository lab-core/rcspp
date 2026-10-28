// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The master solver self-test, run on a master solver that knows the answers of the self-test's
// LP and can be told to make one mistake: each mistake must be reported.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "rcspp/column_generation/master_solver.hpp"
#include "rcspp/column_generation/master_solver_self_test.hpp"
#include "rcspp/column_generation/types.hpp"

using namespace rcspp;                     // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation;  // NOLINT(google-build-using-namespace)

namespace {

// What the master solver below gets wrong.
enum class Mistake {
    None,
    CannotAddRows,
    LosesUpperBounds,
    FailsFirstSolve,
    FailsSecondSolve,
    FailsIntegerSolve,
    SolvesTheRelaxation,
    NoIntegerSolve,
    FlipsLessEqualDual,
    KeepsMaximization,
    ValuesByPosition,
    IgnoresAddedColumn,
};

// The optimum of the self-test's LP with x1, x2, x3 (first) and with x4 too (second), as a
// minimization; values in the order the columns are added.
constexpr double first_objective = 3.75;
constexpr std::array<double, 3> first_duals{1.5, 1.5, -0.5};
constexpr std::array<double, 3> first_values{1.25, 0.75, 0.25};
constexpr double second_objective = 1.0;
constexpr std::array<double, 3> second_duals{0.5, 0.0, 0.0};
constexpr std::array<double, 4> second_values{0.0, 0.0, 0.0, 2.0};

// With x5 too: the integer optimum, and the LP optimum a master solver ignoring integrality gives.
constexpr double integer_objective = 1.0;
constexpr std::array<double, 5> integer_values{0.0, 0.0, 0.0, 2.0, 0.0};
constexpr double relaxed_objective = 0.55;
constexpr std::array<double, 5> relaxed_values{0.0, 0.0, 0.0, 1.0, 0.25};

// Answers what a correct master solver would, except for its mistake.
class SelfTestMasterSolver final : public MasterSolver {
    public:
        explicit SelfTestMasterSolver(Mistake mistake = Mistake::None, bool maximizing = false)
            : mistake_(mistake), maximizing_(maximizing) {}

        [[nodiscard]] size_t number_of_rows() const override { return lower_.size(); }

        void row_bounds(std::span<double> lower, std::span<double> upper) const override {
            std::ranges::copy(lower_, lower.begin());
            if (mistake_ == Mistake::LosesUpperBounds) {
                std::ranges::fill(upper, std::numeric_limits<double>::infinity());
                return;
            }
            std::ranges::copy(upper_, upper.begin());
        }

        void add_rows(const RowBatch& rows) override {
            lower_.insert(lower_.end(), rows.lower.begin(), rows.lower.end());
            upper_.insert(upper_.end(), rows.upper.begin(), rows.upper.end());
        }

        void add_columns(const ColumnBatch& columns) override {
            if (mistake_ == Mistake::IgnoresAddedColumn && !ids_.empty()) {
                return;
            }
            ids_.insert(ids_.end(), columns.ids.begin(), columns.ids.end());
        }

        SolverStatus solve_relaxation(const RelaxationParams& /*params*/) override {
            const bool first = ids_.size() == first_values.size();
            if ((first && mistake_ == Mistake::FailsFirstSolve) ||
                (!first && mistake_ == Mistake::FailsSecondSolve)) {
                return SolverStatus::Other;
            }
            if (first) {
                answer(first_objective, first_duals, first_values);
            } else {
                answer(second_objective, second_duals, second_values);
            }
            return SolverStatus::Optimal;
        }

        [[nodiscard]] double objective() const override { return sign() * objective_; }

        void duals(std::span<double> out) const override {
            std::ranges::transform(duals_, out.begin(), [this](double dual) {
                return sign() * dual;
            });
            if (mistake_ == Mistake::FlipsLessEqualDual) {
                out[2] = -out[2];
            }
        }

        // An id it never received reads as 0.
        void primal(std::span<const ColumnId> ids, std::span<double> out) const override {
            for (size_t index = 0; index < ids.size(); ++index) {
                const auto position =
                    mistake_ == Mistake::ValuesByPosition
                        ? index
                        : static_cast<size_t>(std::ranges::find(ids_, ids[index]) - ids_.begin());
                out[index] = position < values_.size() ? values_[position] : 0.0;
            }
        }

        SolverStatus solve_integer(const IntegerParams& /*params*/) override {
            if (mistake_ == Mistake::FailsIntegerSolve) {
                return SolverStatus::Infeasible;
            }
            if (mistake_ == Mistake::SolvesTheRelaxation) {
                answer(relaxed_objective, {}, relaxed_values);
            } else {
                answer(integer_objective, {}, integer_values);
            }
            return SolverStatus::Optimal;
        }

        void set_option(std::string_view /*name*/, const OptionValue& /*value*/) override {}

        [[nodiscard]] Capabilities capabilities() const override {
            return Capabilities{.add_rows = mistake_ != Mistake::CannotAddRows,
                                .integer_solve = mistake_ != Mistake::NoIntegerSolve};
        }

    private:
        void answer(double objective, std::span<const double> duals,
                    std::span<const double> values) {
            objective_ = objective;
            duals_.assign(duals.begin(), duals.end());
            values_.assign(values.begin(), values.end());
        }

        // A maximizing model that forgets to negate reports the values of the maximization.
        [[nodiscard]] double sign() const {
            return maximizing_ && mistake_ == Mistake::KeepsMaximization ? -1.0 : 1.0;
        }

        Mistake mistake_;
        bool maximizing_;
        std::vector<double> lower_;
        std::vector<double> upper_;
        std::vector<ColumnId> ids_;
        double objective_ = 0.0;
        std::vector<double> duals_;
        std::vector<double> values_;
};

// The self-test of a correct maximizing master solver and a minimizing one with @p mistake.
MasterSolverSelfTestReport self_test_with(Mistake mistake) {
    SelfTestMasterSolver minimizing(mistake);
    SelfTestMasterSolver maximizing(Mistake::None, true);
    return master_solver_self_test(minimizing, maximizing);
}

// Whether some failure contains @p text.
bool mentions(const MasterSolverSelfTestReport& report, std::string_view text) {
    return std::ranges::any_of(report.failures, [text](const std::string& failure) {
        return failure.find(text) != std::string::npos;
    });
}

}  // namespace

TEST(ColumnGenerationSelfTest, ACorrectMasterSolverPasses) {
    SelfTestMasterSolver minimizing;
    SelfTestMasterSolver maximizing(Mistake::None, true);

    const MasterSolverSelfTestReport report = master_solver_self_test(minimizing, maximizing);

    EXPECT_TRUE(report.passed());
    EXPECT_TRUE(report.failures.empty());
}

TEST(ColumnGenerationSelfTest, NeedsEmptyMasterSolversThatAddRows) {
    SelfTestMasterSolver correct(Mistake::None, true);
    SelfTestMasterSolver cannot_add_rows(Mistake::CannotAddRows);
    EXPECT_THROW((void)master_solver_self_test(cannot_add_rows, correct), std::invalid_argument);

    SelfTestMasterSolver with_rows;
    constexpr std::array<double, 1> bound{1.0};
    with_rows.add_rows(RowBatch{.lower = bound, .upper = bound});
    SelfTestMasterSolver empty;
    EXPECT_THROW((void)master_solver_self_test(empty, with_rows), std::invalid_argument);
}

TEST(ColumnGenerationSelfTest, ReportsLostRowBounds) {
    const MasterSolverSelfTestReport report = self_test_with(Mistake::LosesUpperBounds);

    EXPECT_FALSE(report.passed());
    EXPECT_TRUE(mentions(report, "minimizing model: rows: upper bound of row 0 (=): expected 2"));
    EXPECT_TRUE(mentions(report, "upper bound of row 2 (<=)"));
    EXPECT_FALSE(mentions(report, "upper bound of row 1 (>=)"));  // infinite either way
}

TEST(ColumnGenerationSelfTest, ReportsAFlippedDualSign) {
    const MasterSolverSelfTestReport report = self_test_with(Mistake::FlipsLessEqualDual);

    ASSERT_EQ(report.failures.size(), 1U);
    EXPECT_EQ(report.failures[0],
              "minimizing model: first LP: dual of row 2 (<=): expected -0.5, got 0.5");
}

TEST(ColumnGenerationSelfTest, ReportsAMaximizationThatIsNotNormalized) {
    SelfTestMasterSolver minimizing;
    SelfTestMasterSolver maximizing(Mistake::KeepsMaximization, true);

    const MasterSolverSelfTestReport report = master_solver_self_test(minimizing, maximizing);

    EXPECT_TRUE(
        mentions(report, "maximizing model: first LP: objective: expected 3.75, got -3.75"));
    EXPECT_TRUE(mentions(report, "maximizing model: first LP: dual of row 1 (>=)"));
    EXPECT_FALSE(mentions(report, "minimizing model"));
}

TEST(ColumnGenerationSelfTest, ReportsValuesReadByPositionInsteadOfId) {
    const MasterSolverSelfTestReport report = self_test_with(Mistake::ValuesByPosition);

    EXPECT_TRUE(mentions(report, "first LP: value of x3 (id 5): expected 0.25, got 1.25"));
    EXPECT_TRUE(mentions(report, "LP after adding x4: value of x4 (id 9)"));
}

TEST(ColumnGenerationSelfTest, ReportsAnIgnoredColumn) {
    const MasterSolverSelfTestReport report = self_test_with(Mistake::IgnoresAddedColumn);

    EXPECT_TRUE(mentions(report, "LP after adding x4: objective: expected 1, got 3.75"));
    EXPECT_FALSE(mentions(report, "first LP"));
}

TEST(ColumnGenerationSelfTest, StopsAtASolveThatIsNotOptimal) {
    MasterSolverSelfTestReport report = self_test_with(Mistake::FailsFirstSolve);
    ASSERT_EQ(report.failures.size(), 1U);
    EXPECT_EQ(report.failures[0],
              "minimizing model: first LP: status: expected Optimal, got Other");

    report = self_test_with(Mistake::FailsSecondSolve);
    ASSERT_EQ(report.failures.size(), 1U);
    EXPECT_TRUE(mentions(report, "LP after adding x4: status"));

    report = self_test_with(Mistake::FailsIntegerSolve);
    ASSERT_EQ(report.failures.size(), 1U);
    EXPECT_TRUE(mentions(report, "integer solve: status: expected Optimal, got Infeasible"));
}

TEST(ColumnGenerationSelfTest, ReportsAnIntegerSolveThatSolvesTheRelaxation) {
    const MasterSolverSelfTestReport report = self_test_with(Mistake::SolvesTheRelaxation);

    EXPECT_TRUE(mentions(report, "integer solve: objective: expected 1, got 0.55"));
    EXPECT_TRUE(mentions(report, "integer solve: value of x5 (id 11): expected 0, got 0.25"));
}

TEST(ColumnGenerationSelfTest, SkipsTheIntegerSolveWhenUnsupported) {
    EXPECT_TRUE(self_test_with(Mistake::NoIntegerSolve).passed());
}
