// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The HiGHS master solver on small models built as a user would build them.

#include <Highs.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "rcspp/column_generation/highs/highs_master_solver.hpp"
#include "rcspp/column_generation/master_problem.hpp"
#include "rcspp/column_generation/master_solver_self_test.hpp"
#include "test_column_generation_fakes.hpp"

using namespace rcspp;                              // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation;           // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation::testing;  // NOLINT(google-build-using-namespace)

namespace {

constexpr double tolerance = 1e-9;

// Right-hand sides and the one cost above 1 of the LP most tests use (see HighsMasterSolverTest).
constexpr double lp_equal_side = 2.0;
constexpr double lp_less_side = 1.5;
constexpr double lp_x2_cost = 3.0;

// The rows of a path: a coefficient 1 in each of @p rows.
std::vector<Row> covering(std::initializer_list<size_t> rows) {
    std::vector<Row> entries;
    for (const size_t row : rows) {
        entries.push_back(Row{.index = row, .coefficient = 1.0});
    }
    return entries;
}

// A model per test, and the LP most tests use:
//
//   min x1 + 3 x2 + x3   s.t.  x1 + x2 = 2,  x2 + x3 ≥ 1,  x1 + x3 ≤ 1.5,  x ≥ 0
//
// Its optimum 3.75 is at (1.25, 0.75, 0.25), every row tight, with the duals (1.5, 1.5, −0.5).
class HighsMasterSolverTest : public ::testing::Test {
    protected:
        void SetUp() override { highs.setOptionValue("output_flag", false); }

        // The three rows, empty: the columns fill them. Their indices are 0, 1, 2.
        std::vector<HighsInt> lp_rows() {
            const std::array<double, 3> lower{lp_equal_side, 1.0, -kHighsInf};
            const std::array<double, 3> upper{lp_equal_side, kHighsInf, lp_less_side};
            highs.addRows(3, lower.data(), upper.data(), 0, nullptr, nullptr, nullptr);
            return {0, 1, 2};
        }

        // x1, x2, x3; they get the ids 0, 1, 2.
        static std::vector<Solution> lp_columns() {
            return {make_path(1.0, covering({0, 2})),
                    make_path(lp_x2_cost, covering({0, 1})),
                    make_path(1.0, covering({1, 2}))};
        }

        // Adds the LP's columns through a master over @p solver.
        static void add_lp_columns(HighsMasterSolver& solver) {
            MasterProblem master(solver);
            ScriptedPricer pricer;
            master.add_columns(master.add_pricer(pricer), lp_columns());
        }

        Highs highs;
};

// Whether @p master refuses a batch of a valid row followed by the row lower ≤ a·x ≤ upper.
bool refuses_after_a_valid_row(MasterProblem& master, double lower, double upper) {
    const std::vector<double> lowers{0.0, lower};
    const std::vector<double> uppers{0.0, upper};
    try {
        (void)master.add_rows(lowers, uppers);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

}  // namespace

TEST_F(HighsMasterSolverTest, RowBoundsFromTheModel) {
    HighsMasterSolver solver(highs, lp_rows());

    std::vector<double> lower(3);
    std::vector<double> upper(3);
    solver.row_bounds(lower, upper);

    EXPECT_EQ(solver.number_of_rows(), 3U);
    EXPECT_EQ(lower, (std::vector<double>{lp_equal_side, 1.0, -infinity}));
    EXPECT_EQ(upper, (std::vector<double>{lp_equal_side, infinity, lp_less_side}));
}

TEST_F(HighsMasterSolverTest, PassesTheMasterSolverSelfTest) {
    Highs maximizing_model;
    maximizing_model.setOptionValue("output_flag", false);
    maximizing_model.changeObjectiveSense(ObjSense::kMaximize);
    HighsMasterSolver minimizing(highs, {});
    HighsMasterSolver maximizing(maximizing_model, {});

    const MasterSolverSelfTestReport report = master_solver_self_test(minimizing, maximizing);

    EXPECT_TRUE(report.passed());
    for (const std::string& failure : report.failures) {
        ADD_FAILURE() << failure;
    }
}

TEST_F(HighsMasterSolverTest, MaximizingModelIsSeenAsAMinimization) {
    highs.changeObjectiveSense(ObjSense::kMaximize);
    HighsMasterSolver solver(highs, lp_rows());
    add_lp_columns(solver);

    ASSERT_EQ(solver.solve_relaxation({}), SolverStatus::Optimal);

    // HiGHS maximizes the negated costs; the master solver reports the minimization (the
    // self-test checks the normalized answers).
    EXPECT_NEAR(highs.getLp().col_cost_[1], -lp_x2_cost, tolerance);
    EXPECT_NEAR(highs.getInfo().objective_function_value, -3.75, tolerance);
    EXPECT_NEAR(solver.objective(), 3.75, tolerance);
}

TEST_F(HighsMasterSolverTest, InfeasibleAndUnboundedAreToldApart) {
    // x ≥ 2 and x ≤ 1.
    const std::array<double, 2> lower{2.0, -kHighsInf};
    const std::array<double, 2> upper{kHighsInf, 1.0};
    highs.addRows(2, lower.data(), upper.data(), 0, nullptr, nullptr, nullptr);
    HighsMasterSolver infeasible(highs, {0, 1});
    MasterProblem master(infeasible);
    ScriptedPricer pricer;
    master.add_columns(master.add_pricer(pricer), {make_path(1.0, covering({0, 1}))});
    EXPECT_EQ(infeasible.solve_relaxation({}), SolverStatus::Infeasible);

    // min −y with y ≥ 1 only.
    Highs unbounded_model;
    unbounded_model.setOptionValue("output_flag", false);
    const double at_least = 1.0;
    const double no_limit = kHighsInf;
    unbounded_model.addRows(1, &at_least, &no_limit, 0, nullptr, nullptr, nullptr);
    HighsMasterSolver unbounded(unbounded_model, {0});
    MasterProblem unbounded_master(unbounded);
    unbounded_master.add_columns(unbounded_master.add_pricer(pricer),
                                 {make_path(-1.0, covering({0}))});
    EXPECT_EQ(unbounded.solve_relaxation({}), SolverStatus::Unbounded);
}

TEST_F(HighsMasterSolverTest, ZeroTimeLeftIsATimeLimit) {
    HighsMasterSolver solver(highs, lp_rows());
    add_lp_columns(solver);

    EXPECT_EQ(solver.solve_relaxation({.timeout_s = 0.0}), SolverStatus::TimeLimit);
}

TEST_F(HighsMasterSolverTest, TheTimeLimitCountsFromEachSolve) {
    // HiGHS counts its time limit over all the model's solves, its MIP solver from the MIP's
    // start: every solve zeroes the clocks and is given its own limit.
    HighsMasterSolver solver(highs, lp_rows());
    add_lp_columns(solver);
    constexpr int many = 50;
    constexpr double short_limit_s = 0.05;
    double longest_s = 0.0;
    for (int solve = 0; solve < many; ++solve) {
        ASSERT_EQ(solver.solve_relaxation({.timeout_s = short_limit_s}), SolverStatus::Optimal);
        longest_s = std::max(longest_s, highs.getRunTime());
    }

    // The clock counted each solve alone, so it never reached the short limit.
    EXPECT_LT(longest_s, short_limit_s);
    ASSERT_EQ(solver.solve_integer({.timeout_s = short_limit_s}), SolverStatus::Optimal);
    EXPECT_LT(highs.getRunTime(), short_limit_s);
}

TEST_F(HighsMasterSolverTest, TheUserOptionsComeBackAfterEachSolveAndTheMethodIsLeftToHighs) {
    // The user's own settings.
    constexpr double user_time_limit = 50.0;
    constexpr double user_gap = 0.2;
    highs.setOptionValue("time_limit", user_time_limit);
    highs.setOptionValue("mip_rel_gap", user_gap);
    highs.setOptionValue("output_flag", true);  // the solves themselves run quietly
    HighsMasterSolver solver(highs, lp_rows());
    add_lp_columns(solver);
    HighsInt strategy = 0;
    highs.getOptionValue("simplex_strategy", strategy);

    ASSERT_EQ(solver.solve_relaxation({.timeout_s = 100.0}), SolverStatus::Optimal);
    EXPECT_NEAR(solver.objective(), 3.75, tolerance);  // the solution outlives the options
    double time_limit = 0.0;
    highs.getOptionValue("time_limit", time_limit);
    EXPECT_EQ(time_limit, user_time_limit);
    bool output = false;
    highs.getOptionValue("output_flag", output);
    EXPECT_TRUE(output);
    bool relaxation = true;
    highs.getOptionValue("solve_relaxation", relaxation);
    EXPECT_FALSE(relaxation);
    HighsInt strategy_after = 0;
    highs.getOptionValue("simplex_strategy", strategy_after);
    EXPECT_EQ(strategy_after, strategy);

    ASSERT_EQ(solver.solve_integer({}), SolverStatus::Optimal);
    double gap = 0.0;
    highs.getOptionValue("mip_rel_gap", gap);
    EXPECT_EQ(gap, user_gap);
    highs.setOptionValue("output_flag", false);
}

TEST_F(HighsMasterSolverTest, TheUserSolvesTheirIntegerModelAfterARun) {
    // max x + y  s.t. 2x + 2y ≤ 3, x and y integer: 1 as a MIP, 1.5 as an LP.
    highs.changeObjectiveSense(ObjSense::kMaximize);
    const std::array<double, 2> cost{1.0, 1.0};
    const std::array<double, 2> lower{0.0, 0.0};
    const std::array<double, 2> upper{kHighsInf, kHighsInf};
    highs.addCols(2, cost.data(), lower.data(), upper.data(), 0, nullptr, nullptr, nullptr);
    highs.changeColIntegrality(0, HighsVarType::kInteger);
    highs.changeColIntegrality(1, HighsVarType::kInteger);
    const std::array<HighsInt, 2> columns{0, 1};
    const std::array<double, 2> twos{2.0, 2.0};
    const double at_most = 3.0;
    const double no_lower = -kHighsInf;
    highs.addRow(no_lower, at_most, 2, columns.data(), twos.data());
    HighsMasterSolver solver(highs, {0});

    ASSERT_EQ(solver.solve_relaxation({}), SolverStatus::Optimal);
    EXPECT_NEAR(solver.objective(), -1.5, tolerance);
    solver.restore_user_model();

    ASSERT_EQ(highs.run(), HighsStatus::kOk);
    EXPECT_NEAR(highs.getInfo().objective_function_value, 1.0, 1e-6);
}

TEST_F(HighsMasterSolverTest, AnotherThreadCountIsWarnedAbout) {
    HighsMasterSolver solver(highs, lp_rows());
    add_lp_columns(solver);

    // Whatever count HiGHS took first in this process, 1 and 2 cannot both be it: HiGHS refuses
    // the other one, and the solve runs with the count in force.
    ::testing::internal::CaptureStdout();
    ASSERT_EQ(solver.solve_relaxation({.threads = 1}), SolverStatus::Optimal);
    ASSERT_EQ(solver.solve_relaxation({.threads = 2}), SolverStatus::Optimal);
    const std::string output = ::testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("HiGHS keeps the thread count"), std::string::npos) << output;
}

TEST_F(HighsMasterSolverTest, ResolvesFromTheLastBasisAfterAddingAColumn) {
    HighsMasterSolver solver(highs, lp_rows());
    MasterProblem master(solver);
    ScriptedPricer pricer;
    const size_t index = master.add_pricer(pricer);
    master.add_columns(index, lp_columns());
    ASSERT_EQ(solver.solve_relaxation({}), SolverStatus::Optimal);

    // x4 covers the first two rows at cost 0.5: reduced cost 0.5 − 1.5 − 1.5 < 0.
    constexpr double cheap = 0.5;
    master.add_columns(index, {make_path(cheap, covering({0, 1}))});
    ASSERT_EQ(solver.solve_relaxation({}), SolverStatus::Optimal);

    EXPECT_NEAR(solver.objective(), 1.0, tolerance);
    EXPECT_LE(highs.getInfo().simplex_iteration_count, 3);
}

TEST_F(HighsMasterSolverTest, LpSolvesLeaveTheIntegralityAsItIs) {
    std::vector<HighsInt> rows = lp_rows();
    // The user's own integer column, in the `≥` row, too dear to be used.
    constexpr double dear = 10.0;
    constexpr double at_most = 5.0;
    const double zero = 0.0;
    const HighsInt start = 0;
    const HighsInt row = 1;
    const double one = 1.0;
    highs.addCols(1, &dear, &zero, &at_most, 1, &start, &row, &one);
    highs.changeColIntegrality(0, HighsVarType::kInteger);

    HighsMasterSolver solver(highs, rows);
    add_lp_columns(solver);

    // LP: the integer column is solved as continuous, and stays integer; duals are there.
    ASSERT_EQ(solver.solve_relaxation({}), SolverStatus::Optimal);
    EXPECT_NEAR(solver.objective(), 3.75, tolerance);
    EXPECT_TRUE(highs.getSolution().dual_valid);
    EXPECT_EQ(highs.getLp().integrality_[0], HighsVarType::kInteger);

    // MIP: the columns are integer; x1 = x2 = 1 costs 4.
    ASSERT_EQ(solver.solve_integer({}), SolverStatus::Optimal);
    EXPECT_NEAR(solver.objective(), 4.0, 1e-6);
    EXPECT_EQ(highs.getLp().integrality_[solver.column(0)], HighsVarType::kInteger);

    // Binary: an upper bound of 1 during the MIP, the column's own back for the next LP.
    ASSERT_EQ(solver.solve_integer({.column_type = ColumnType::Binary}), SolverStatus::Optimal);
    EXPECT_EQ(highs.getLp().col_upper_[solver.column(2)], 1.0);
    ASSERT_EQ(solver.solve_relaxation({}), SolverStatus::Optimal);
    EXPECT_NEAR(solver.objective(), 3.75, tolerance);
    EXPECT_EQ(highs.getLp().col_upper_[solver.column(2)], kHighsInf);

    // The end of a run gives them back too.
    ASSERT_EQ(solver.solve_integer({.column_type = ColumnType::Binary}), SolverStatus::Optimal);
    solver.restore_user_model();
    EXPECT_EQ(highs.getLp().col_upper_[solver.column(2)], kHighsInf);

    // A bound the user tightened holds during the binary solve, and stays.
    highs.changeColBounds(solver.column(2), 0.0, 0.0);
    ASSERT_EQ(solver.solve_integer({.column_type = ColumnType::Binary}), SolverStatus::Optimal);
    EXPECT_EQ(highs.getLp().col_upper_[solver.column(2)], 0.0);
    EXPECT_EQ(highs.getLp().col_upper_[solver.column(1)], 1.0);
    solver.restore_user_model();
    EXPECT_EQ(highs.getLp().col_upper_[solver.column(2)], 0.0);
    EXPECT_EQ(highs.getLp().col_upper_[solver.column(1)], kHighsInf);
}

TEST_F(HighsMasterSolverTest, AddsRowsWithNamesRangesIncluded) {
    HighsMasterSolver solver(highs, {});
    MasterProblem master(solver);
    const std::vector<double> lower{1.0, 2.0, -infinity, -infinity, 1.0};
    const std::vector<double> upper{1.0, infinity, 3.0, infinity, 4.0};
    const std::vector<std::string> names{"equal", "greater", "less", "free", "range"};
    const RowRange rows = master.add_rows(lower, upper, names);

    EXPECT_EQ(rows.count, 5U);
    std::vector<double> lower_out(lower.size());
    std::vector<double> upper_out(upper.size());
    solver.row_bounds(lower_out, upper_out);
    EXPECT_EQ(lower_out, lower);
    EXPECT_EQ(upper_out, upper);
    std::string name;
    highs.getRowName(2, name);
    EXPECT_EQ(name, "less");

    // A convexity row created by the master.
    ScriptedPricer pricer;
    constexpr double max_paths = 2.0;
    master.add_pricer(pricer, ConvexityRow{.max_paths = max_paths});
    EXPECT_EQ(solver.number_of_rows(), 6U);
    EXPECT_EQ(highs.getLp().row_upper_[5], max_paths);
}

TEST_F(HighsMasterSolverTest, RefusedRowsAddNothing) {
    HighsMasterSolver solver(highs, {});
    MasterProblem master(solver);
    const double not_a_number = std::numeric_limits<double>::quiet_NaN();

    // Bounds no row holds.
    const std::vector<std::array<double, 2>> refused{{2.0, 1.0},
                                                     {infinity, infinity},
                                                     {-infinity, -infinity},
                                                     {not_a_number, 1.0},
                                                     {-infinity, not_a_number}};
    for (const auto& [lower, upper] : refused) {
        EXPECT_TRUE(refuses_after_a_valid_row(master, lower, upper))
            << lower << " <= a.x <= " << upper;
    }

    EXPECT_EQ(solver.number_of_rows(), 0U);
    EXPECT_EQ(highs.getNumRow(), 0);
}

TEST_F(HighsMasterSolverTest, UnknownIdsAndRowsThrow) {
    EXPECT_THROW(HighsMasterSolver(highs, {0}), std::out_of_range);  // the model has no row

    HighsMasterSolver solver(highs, lp_rows());
    MasterProblem master(solver);
    ScriptedPricer pricer;
    const size_t index = master.add_pricer(pricer);
    master.add_columns(index, {make_path(1.0, covering({0}))});

    // The same id again, a row that does not exist, an id never received.
    EXPECT_THROW(solver.add_columns(OneEntryColumns({0}, {0}).batch()), std::invalid_argument);
    EXPECT_THROW(solver.add_columns(OneEntryColumns({1}, {7}).batch()), std::out_of_range);
    EXPECT_EQ(highs.getNumCol(), 1);

    EXPECT_THROW((void)solver.column(42), std::out_of_range);
    const std::vector<ColumnId> unknown{42};
    std::vector<double> out(1);
    EXPECT_THROW(solver.primal(unknown, out), std::out_of_range);
}

TEST_F(HighsMasterSolverTest, ABatchHighsRefusesAddsNoColumn) {
    HighsMasterSolver solver(highs, lp_rows());
    solver.add_columns(OneEntryColumns({0}, {0}).batch());

    // A column listing row 1 twice: HiGHS refuses the whole batch (the master merges such rows
    // before a master solver sees them; this batch is given directly).
    OneEntryColumns repeated_row({1, 2}, {0, 1});
    repeated_row.starts = {0, 1, 3};
    repeated_row.row_indices = {0, 1, 1};
    repeated_row.values = {1.0, 0.5, 0.5};
    EXPECT_THROW(solver.add_columns(repeated_row.batch()), std::invalid_argument);

    EXPECT_EQ(highs.getNumCol(), 1);
    EXPECT_THROW((void)solver.column(1), std::out_of_range);

    // The ids are still free, and later columns get the right indices.
    solver.add_columns(OneEntryColumns({1, 2}, {0, 1}).batch());
    EXPECT_EQ(solver.column(2), 2);
}

TEST_F(HighsMasterSolverTest, AnIntegerOptionHighsCannotHoldIsRefused) {
    HighsMasterSolver solver(highs, {});
    if constexpr (sizeof(HighsInt) < sizeof(std::int64_t)) {
        constexpr std::int64_t too_large = std::int64_t{1} << 40;
        EXPECT_THROW(solver.set_option("mip_max_nodes", too_large), std::invalid_argument);
    }
}

TEST_F(HighsMasterSolverTest, NoSolutionIsReadWithoutASolveThatGaveOne) {
    HighsMasterSolver solver(highs, lp_rows());
    MasterProblem master(solver);
    ScriptedPricer pricer;
    const size_t index = master.add_pricer(pricer);
    master.add_columns(index, lp_columns());

    // No solve yet.
    std::vector<double> duals(3);
    std::vector<double> values(1);
    const std::vector<ColumnId> first{0};
    EXPECT_THROW(solver.duals(duals), std::logic_error);
    EXPECT_THROW(solver.primal(first, values), std::logic_error);

    // A column added after the solve has no value yet.
    ASSERT_EQ(solver.solve_relaxation({}), SolverStatus::Optimal);
    master.add_columns(index, {make_path(1.0, covering({0}))});
    const std::vector<ColumnId> added{3};
    EXPECT_THROW(solver.primal(added, values), std::logic_error);
}

TEST_F(HighsMasterSolverTest, ToleranceAndTheUserCostsComeFromTheModel) {
    constexpr double feasibility_tolerance = 1e-8;
    highs.setOptionValue("dual_feasibility_tolerance", feasibility_tolerance);
    // A starting column of the user's, costlier than every generated one.
    constexpr double user_cost = -40.0;
    const double zero = 0.0;
    const double unbounded = kHighsInf;
    highs.addCols(1, &user_cost, &zero, &unbounded, 0, nullptr, nullptr, nullptr);
    HighsMasterSolver solver(highs, lp_rows());
    EXPECT_EQ(solver.largest_user_cost(), -user_cost);

    // The generated columns do not count, however costly.
    MasterProblem master(solver);
    ScriptedPricer pricer;
    constexpr double generated_cost = 100.0;
    master.add_columns(master.add_pricer(pricer), {make_path(generated_cost, covering({0}))});
    EXPECT_EQ(solver.largest_user_cost(), -user_cost);
    EXPECT_EQ(solver.dual_tolerance(), feasibility_tolerance);
}

TEST_F(HighsMasterSolverTest, OptionsByHighsName) {
    HighsMasterSolver solver(highs, {});

    constexpr std::int64_t seed_value = 7;
    solver.set_option("random_seed", seed_value);
    solver.set_option("mip_rel_gap", std::int64_t{0});  // an integer for a double option
    constexpr double tolerance_value = 1e-8;
    solver.set_option("primal_feasibility_tolerance", tolerance_value);
    solver.set_option("output_flag", false);
    solver.set_option("presolve", std::string("off"));

    HighsInt seed = 0;
    highs.getOptionValue("random_seed", seed);
    EXPECT_EQ(seed, seed_value);
    double gap = 1.0;
    highs.getOptionValue("mip_rel_gap", gap);
    EXPECT_EQ(gap, 0.0);
    double feasibility = 0.0;
    highs.getOptionValue("primal_feasibility_tolerance", feasibility);
    EXPECT_EQ(feasibility, tolerance_value);
    std::string presolve;
    highs.getOptionValue("presolve", presolve);
    EXPECT_EQ(presolve, "off");

    EXPECT_THROW(solver.set_option("no_such_option", true), std::invalid_argument);
    EXPECT_THROW(solver.set_option("presolve", std::string("sometimes")), std::invalid_argument);
}
