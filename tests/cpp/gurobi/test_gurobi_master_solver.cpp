// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The Gurobi master solver on small models built as a user would build them, and the column
// generation loop on it. Skipped without a Gurobi license.

#include <gtest/gtest.h>
#include <gurobi_c++.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/column_generation/column_generation.hpp"
#include "rcspp/column_generation/master_problem.hpp"
#include "rcspp/column_generation/master_solver_self_test.hpp"
#include "rcspp_backends/gurobi/gurobi_master_solver.hpp"
#include "test_column_generation_fakes.hpp"

using namespace rcspp;                              // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation;           // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation::testing;  // NOLINT(google-build-using-namespace)

namespace {

constexpr double tolerance = 1e-9;

// Right-hand sides and the one cost above 1 of the LP most tests use (see GurobiMasterSolverTest).
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

// One environment for every test; null when Gurobi has no license on this machine.
GRBEnv* environment() {
    static const std::unique_ptr<GRBEnv> shared = []() -> std::unique_ptr<GRBEnv> {
        try {
            auto created = std::make_unique<GRBEnv>(true);
            created->set(GRB_IntParam_OutputFlag, 0);
            created->start();
            return created;
        } catch (const GRBException&) {
            return nullptr;
        }
    }();
    return shared.get();
}

// A model per test, and the LP most tests use:
//
//   min x1 + 3 x2 + x3   s.t.  x1 + x2 = 2,  x2 + x3 ≥ 1,  x1 + x3 ≤ 1.5,  x ≥ 0
//
// Its optimum 3.75 is at (1.25, 0.75, 0.25), every row tight, with the duals (1.5, 1.5, −0.5):
// free for the `=` row, ≥ 0 for the `≥` row, ≤ 0 for the `≤` row.
class GurobiMasterSolverTest : public ::testing::Test {
    protected:
        void SetUp() override {
            GRBEnv* env = environment();
            if (env == nullptr) {
                GTEST_SKIP() << "Gurobi has no license on this machine";
            }
            model = std::make_unique<GRBModel>(*env);
        }

        // The three rows, empty: the columns fill them.
        std::vector<GRBConstr> lp_rows() {
            return {model->addConstr(GRBLinExpr(), GRB_EQUAL, lp_equal_side),
                    model->addConstr(GRBLinExpr(), GRB_GREATER_EQUAL, 1.0),
                    model->addConstr(GRBLinExpr(), GRB_LESS_EQUAL, lp_less_side)};
        }

        // x1, x2, x3; they get the ids 0, 1, 2.
        static std::vector<Solution> lp_columns() {
            return {make_path(1.0, covering({0, 2})),
                    make_path(lp_x2_cost, covering({0, 1})),
                    make_path(1.0, covering({1, 2}))};
        }

        // Adds the LP's columns through a master over @p solver.
        static void add_lp_columns(GurobiMasterSolver& solver) {
            MasterProblem master(solver);
            ScriptedPricer pricer;
            master.add_columns(master.add_pricer(pricer), lp_columns());
        }

        std::unique_ptr<GRBModel> model;
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

TEST_F(GurobiMasterSolverTest, RowBoundsFromTheModel) {
    GurobiMasterSolver solver(*model, lp_rows());

    std::vector<double> lower(3);
    std::vector<double> upper(3);
    solver.row_bounds(lower, upper);

    EXPECT_EQ(solver.number_of_rows(), 3U);
    EXPECT_EQ(lower, (std::vector<double>{2.0, 1.0, -infinity}));
    EXPECT_EQ(upper, (std::vector<double>{2.0, infinity, 1.5}));
}

TEST_F(GurobiMasterSolverTest, PassesTheMasterSolverSelfTest) {
    GRBModel maximizing_model(*environment());
    maximizing_model.set(GRB_IntAttr_ModelSense, GRB_MAXIMIZE);
    GurobiMasterSolver minimizing(*model, {});
    GurobiMasterSolver maximizing(maximizing_model, {});

    const MasterSolverSelfTestReport report = master_solver_self_test(minimizing, maximizing);

    EXPECT_TRUE(report.passed());
    for (const std::string& failure : report.failures) {
        ADD_FAILURE() << failure;
    }
}

TEST_F(GurobiMasterSolverTest, MaximizingModelIsSeenAsAMinimization) {
    model->set(GRB_IntAttr_ModelSense, GRB_MAXIMIZE);
    GurobiMasterSolver solver(*model, lp_rows());
    add_lp_columns(solver);

    ASSERT_EQ(solver.solve_relaxation({}), SolverStatus::Optimal);

    // Gurobi maximizes the negated costs; the master solver reports the minimization (the
    // self-test checks the normalized answers).
    EXPECT_NEAR(solver.variable(1).get(GRB_DoubleAttr_Obj), -3.0, tolerance);
    EXPECT_NEAR(model->get(GRB_DoubleAttr_ObjVal), -3.75, tolerance);
    EXPECT_NEAR(solver.objective(), 3.75, tolerance);
}

TEST_F(GurobiMasterSolverTest, InfeasibleAndUnboundedAreToldApart) {
    // x ≥ 2 and x ≤ 1.
    constexpr double at_least = 2.0;
    GurobiMasterSolver infeasible(*model,
                                  {model->addConstr(GRBLinExpr(), GRB_GREATER_EQUAL, at_least),
                                   model->addConstr(GRBLinExpr(), GRB_LESS_EQUAL, 1.0)});
    MasterProblem master(infeasible);
    ScriptedPricer pricer;
    master.add_columns(master.add_pricer(pricer), {make_path(1.0, covering({0, 1}))});
    EXPECT_EQ(infeasible.solve_relaxation({}), SolverStatus::Infeasible);

    // min −y with y ≥ 1 only.
    GRBModel unbounded_model(*environment());
    GurobiMasterSolver unbounded(unbounded_model,
                                 {unbounded_model.addConstr(GRBLinExpr(), GRB_GREATER_EQUAL, 1.0)});
    MasterProblem unbounded_master(unbounded);
    unbounded_master.add_columns(unbounded_master.add_pricer(pricer),
                                 {make_path(-1.0, covering({0}))});
    EXPECT_EQ(unbounded.solve_relaxation({}), SolverStatus::Unbounded);

    // The re-solve that tells them apart leaves the user's setting as it was.
    EXPECT_EQ(model->get(GRB_IntParam_DualReductions), 1);
    EXPECT_EQ(unbounded_model.get(GRB_IntParam_DualReductions), 1);
}

TEST_F(GurobiMasterSolverTest, ZeroTimeLeftIsATimeLimit) {
    GurobiMasterSolver solver(*model, lp_rows());
    add_lp_columns(solver);

    EXPECT_EQ(solver.solve_relaxation({.timeout_s = 0.0}), SolverStatus::TimeLimit);
}

TEST_F(GurobiMasterSolverTest, TheUserOptionsComeBackAfterEachSolve) {
    // The user's own settings.
    constexpr double user_time_limit = 50.0;
    constexpr int user_threads = 3;
    constexpr double user_gap = 0.2;
    model->set(GRB_DoubleParam_TimeLimit, user_time_limit);
    model->set(GRB_IntParam_Threads, user_threads);
    model->set(GRB_DoubleParam_MIPGap, user_gap);
    GurobiMasterSolver solver(*model, lp_rows());
    add_lp_columns(solver);

    ASSERT_EQ(solver.solve_relaxation({.timeout_s = 100.0, .threads = 1, .verbose = false}),
              SolverStatus::Optimal);
    EXPECT_NEAR(solver.objective(), 3.75, tolerance);  // the solution outlives the options
    EXPECT_EQ(model->get(GRB_DoubleParam_TimeLimit), user_time_limit);
    EXPECT_EQ(model->get(GRB_IntParam_Threads), user_threads);
    EXPECT_EQ(model->get(GRB_IntParam_Method), GRB_METHOD_AUTO);

    ASSERT_EQ(solver.solve_integer({.timeout_s = 100.0, .threads = 1}), SolverStatus::Optimal);
    EXPECT_EQ(model->get(GRB_DoubleParam_MIPGap), user_gap);
    EXPECT_EQ(model->get(GRB_IntParam_Threads), user_threads);
}

TEST_F(GurobiMasterSolverTest, TheUserModelIsRestoredAfterARun) {
    std::vector<GRBConstr> rows = lp_rows();
    const GRBVar integer = model->addVar(0.0, 5.0, 10.0, GRB_INTEGER);
    model->update();
    model->chgCoeff(rows[1], integer, 1.0);
    GurobiMasterSolver solver(*model, rows);
    add_lp_columns(solver);
    ASSERT_EQ(solver.solve_relaxation({}), SolverStatus::Optimal);
    ASSERT_EQ(integer.get(GRB_CharAttr_VType), GRB_CONTINUOUS);

    solver.restore_user_model();

    // The last solve's values can still be read; Gurobi applies the types at the next update.
    std::array<double, 1> value{};
    solver.primal(std::array<ColumnId, 1>{0}, value);
    EXPECT_NEAR(value[0], 1.25, tolerance);
    model->update();

    // The user's type is back; the generated columns stay, continuous.
    EXPECT_EQ(integer.get(GRB_CharAttr_VType), GRB_INTEGER);
    EXPECT_EQ(solver.variable(0).get(GRB_CharAttr_VType), GRB_CONTINUOUS);
    model->optimize();  // the user's own solve is a MIP again
    EXPECT_EQ(model->get(GRB_IntAttr_IsMIP), 1);

    // A further LP solve relaxes it again.
    ASSERT_EQ(solver.solve_relaxation({}), SolverStatus::Optimal);
    EXPECT_EQ(integer.get(GRB_CharAttr_VType), GRB_CONTINUOUS);
}

TEST_F(GurobiMasterSolverTest, AMethodTheUserChoseIsKept) {
    model->set(GRB_IntParam_Method, GRB_METHOD_BARRIER);
    GurobiMasterSolver solver(*model, lp_rows());
    add_lp_columns(solver);

    ASSERT_EQ(solver.solve_relaxation({}), SolverStatus::Optimal);
    EXPECT_NEAR(solver.objective(), 3.75, 1e-6);
    EXPECT_EQ(model->get(GRB_IntParam_Method), GRB_METHOD_BARRIER);
}

TEST_F(GurobiMasterSolverTest, ResolvesFromTheLastBasisAfterAddingAColumn) {
    GurobiMasterSolver solver(*model, lp_rows());
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
    EXPECT_LE(model->get(GRB_DoubleAttr_IterCount), 3.0);
}

TEST_F(GurobiMasterSolverTest, IntegralityIsRelaxedThenRestored) {
    std::vector<GRBConstr> rows = lp_rows();
    // The user's own variables: an integer one in the `≥` row, a binary one in no row.
    const GRBVar integer = model->addVar(0.0, 5.0, 10.0, GRB_INTEGER);
    const GRBVar binary = model->addVar(0.0, 1.0, 0.0, GRB_BINARY);
    model->update();
    model->chgCoeff(rows[1], integer, 1.0);

    GurobiMasterSolver solver(*model, rows);
    add_lp_columns(solver);

    // LP: everything continuous, same optimum (the integer variable costs too much).
    ASSERT_EQ(solver.solve_relaxation({}), SolverStatus::Optimal);
    EXPECT_NEAR(solver.objective(), 3.75, tolerance);
    EXPECT_EQ(integer.get(GRB_CharAttr_VType), GRB_CONTINUOUS);
    EXPECT_EQ(binary.get(GRB_CharAttr_VType), GRB_CONTINUOUS);
    EXPECT_EQ(solver.variable(0).get(GRB_CharAttr_VType), GRB_CONTINUOUS);

    // MIP: the user's types come back, the columns are integer; x1 = x2 = 1 costs 4.
    ASSERT_EQ(solver.solve_integer({}), SolverStatus::Optimal);
    EXPECT_NEAR(solver.objective(), 4.0, 1e-6);
    EXPECT_EQ(integer.get(GRB_CharAttr_VType), GRB_INTEGER);
    EXPECT_EQ(binary.get(GRB_CharAttr_VType), GRB_BINARY);
    EXPECT_EQ(solver.variable(0).get(GRB_CharAttr_VType), GRB_INTEGER);

    ASSERT_EQ(solver.solve_integer({.column_type = ColumnType::Binary}), SolverStatus::Optimal);
    EXPECT_EQ(solver.variable(2).get(GRB_CharAttr_VType), GRB_BINARY);

    // And the next LP solve relaxes everything again.
    ASSERT_EQ(solver.solve_relaxation({}), SolverStatus::Optimal);
    EXPECT_NEAR(solver.objective(), 3.75, tolerance);
    EXPECT_EQ(integer.get(GRB_CharAttr_VType), GRB_CONTINUOUS);
    EXPECT_EQ(solver.variable(2).get(GRB_CharAttr_VType), GRB_CONTINUOUS);
}

TEST_F(GurobiMasterSolverTest, ARelaxedBinaryStaysWithinZeroAndOne) {
    // A binary of the user's, in no row and worth 1. Gurobi stores an upper bound of 1 when a
    // binary is created, but keeps one set later: once continuous, the variable would make the
    // LP unbounded.
    GRBVar binary = model->addVar(0.0, 1.0, -1.0, GRB_BINARY);
    model->update();
    binary.set(GRB_DoubleAttr_UB, GRB_INFINITY);
    GurobiMasterSolver solver(*model, lp_rows());
    add_lp_columns(solver);

    ASSERT_EQ(solver.solve_relaxation({}), SolverStatus::Optimal);
    EXPECT_NEAR(solver.objective(), 3.75 - 1.0, tolerance);

    // Its own bounds come back with its type.
    solver.restore_user_model();
    model->update();
    EXPECT_EQ(binary.get(GRB_DoubleAttr_UB), GRB_INFINITY);
    EXPECT_EQ(binary.get(GRB_CharAttr_VType), GRB_BINARY);
}

TEST_F(GurobiMasterSolverTest, ToleranceAndTheUserCostsComeFromTheModel) {
    constexpr double optimality_tolerance = 1e-7;
    model->set(GRB_DoubleParam_OptimalityTol, optimality_tolerance);
    // A starting column of the user's, costlier than every generated one.
    constexpr double user_cost = -40.0;
    model->addVar(0.0, GRB_INFINITY, user_cost, GRB_CONTINUOUS);
    GurobiMasterSolver solver(*model, lp_rows());
    EXPECT_EQ(solver.largest_user_cost(), -user_cost);

    // The generated columns do not count, however costly.
    MasterProblem master(solver);
    ScriptedPricer pricer;
    constexpr double generated_cost = 100.0;
    master.add_columns(master.add_pricer(pricer), {make_path(generated_cost, covering({0}))});
    EXPECT_EQ(solver.largest_user_cost(), -user_cost);
    EXPECT_EQ(solver.dual_tolerance(), optimality_tolerance);
}

TEST_F(GurobiMasterSolverTest, AVariableTheUserAddsDuringTheRunIsRelaxedToo) {
    std::vector<GRBConstr> rows = lp_rows();
    GurobiMasterSolver solver(*model, rows);
    add_lp_columns(solver);
    ASSERT_EQ(solver.solve_relaxation({}), SolverStatus::Optimal);

    // An integer variable of the user's, added between two LP solves: in the `≥` row, too dear
    // to be used.
    GRBColumn in_second_row;
    in_second_row.addTerm(1.0, rows[1]);
    const GRBVar integer = model->addVar(0.0, 5.0, 10.0, GRB_INTEGER, in_second_row);

    // Still an LP solve, with duals.
    ASSERT_EQ(solver.solve_relaxation({}), SolverStatus::Optimal);
    std::vector<double> duals(rows.size());
    solver.duals(duals);
    EXPECT_NEAR(duals[0], 1.5, tolerance);
    EXPECT_EQ(integer.get(GRB_CharAttr_VType), GRB_CONTINUOUS);

    // The integer solve gives the user's type back.
    ASSERT_EQ(solver.solve_integer({}), SolverStatus::Optimal);
    EXPECT_EQ(integer.get(GRB_CharAttr_VType), GRB_INTEGER);
}

TEST_F(GurobiMasterSolverTest, AddsRowsBySenseWithNames) {
    GurobiMasterSolver solver(*model, {});
    ASSERT_TRUE(solver.capabilities().add_rows);
    ASSERT_TRUE(solver.capabilities().integer_solve);

    MasterProblem master(solver);
    const std::vector<double> lower{1.0, 2.0, -infinity, -infinity};
    const std::vector<double> upper{1.0, infinity, 3.0, infinity};
    const std::vector<std::string> names{"equal", "greater", "less", "free"};
    const RowRange rows = master.add_rows(lower, upper, names);

    EXPECT_EQ(rows.count, 4U);
    std::vector<double> lower_out(4);
    std::vector<double> upper_out(4);
    solver.row_bounds(lower_out, upper_out);
    EXPECT_EQ(lower_out, lower);
    EXPECT_EQ(upper_out, upper);
    EXPECT_EQ(model->getConstr(2).get(GRB_StringAttr_ConstrName), "less");

    // A convexity row created by the master.
    ScriptedPricer pricer;
    constexpr double max_paths = 2.0;
    master.add_pricer(pricer, ConvexityRow{.max_paths = max_paths});
    EXPECT_EQ(solver.number_of_rows(), 5U);
    EXPECT_EQ(model->getConstr(4).get(GRB_CharAttr_Sense), GRB_LESS_EQUAL);
}

TEST_F(GurobiMasterSolverTest, RefusedRowsAddNothing) {
    GurobiMasterSolver solver(*model, {});
    MasterProblem master(solver);
    const double not_a_number = std::numeric_limits<double>::quiet_NaN();

    // A range, then bounds no Gurobi row holds.
    const std::vector<std::array<double, 2>> refused{{1.0, 2.0},
                                                     {infinity, infinity},
                                                     {-infinity, -infinity},
                                                     {infinity, 1.0},
                                                     {1.0, -infinity},
                                                     {not_a_number, 1.0},
                                                     {-infinity, not_a_number}};
    for (const auto& [lower, upper] : refused) {
        EXPECT_TRUE(refuses_after_a_valid_row(master, lower, upper))
            << lower << " <= a.x <= " << upper;
    }

    model->update();
    EXPECT_EQ(solver.number_of_rows(), 0U);
    EXPECT_EQ(model->get(GRB_IntAttr_NumConstrs), 0);
}

TEST_F(GurobiMasterSolverTest, RefusedRowBySenseAddsNothing) {
    GurobiMasterSolver solver(*model, {});
    MasterProblem master(solver);

    EXPECT_THROW((void)master.add_rows(1, RowSense::Equal, infinity), std::invalid_argument);

    model->update();
    EXPECT_EQ(solver.number_of_rows(), 0U);
    EXPECT_EQ(model->get(GRB_IntAttr_NumConstrs), 0);
}

TEST_F(GurobiMasterSolverTest, UnknownIdsAndRowsThrow) {
    GurobiMasterSolver solver(*model, lp_rows());
    const OneEntryColumns first({0}, {0});
    solver.add_columns(first.batch());

    // The same id again, a row that does not exist, an id never received.
    EXPECT_THROW(solver.add_columns(first.batch()), std::invalid_argument);
    EXPECT_THROW(solver.add_columns(OneEntryColumns({1}, {7}).batch()), std::out_of_range);
    EXPECT_THROW((void)solver.variable(42), std::out_of_range);

    const std::vector<ColumnId> ids{42};
    std::vector<double> values(1);
    EXPECT_THROW(solver.primal(ids, values), std::out_of_range);
}

TEST_F(GurobiMasterSolverTest, ARefusedBatchAddsNoColumn) {
    GurobiMasterSolver solver(*model, lp_rows());

    // The second column is refused: it repeats the first's id, or enters a row that does not
    // exist.
    EXPECT_THROW(solver.add_columns(OneEntryColumns({3, 3}, {0, 0}).batch()),
                 std::invalid_argument);
    EXPECT_THROW(solver.add_columns(OneEntryColumns({2, 4}, {0, 7}).batch()), std::out_of_range);

    model->update();
    EXPECT_EQ(model->get(GRB_IntAttr_NumVars), 0);
    EXPECT_THROW((void)solver.variable(2), std::out_of_range);

    // The ids are still free.
    solver.add_columns(OneEntryColumns({2, 3}, {0, 1}).batch());
    EXPECT_EQ(model->get(GRB_IntAttr_NumVars), 2);
}

TEST_F(GurobiMasterSolverTest, OptionsBySolverName) {
    GurobiMasterSolver solver(*model, {});

    solver.set_option("Threads", std::int64_t{2});
    constexpr double gap = 0.05;
    solver.set_option("MIPGap", gap);
    solver.set_option("OutputFlag", false);
    solver.set_option("method", std::string("1"));  // Gurobi's names ignore case

    EXPECT_EQ(model->get(GRB_IntParam_Threads), 2);
    EXPECT_EQ(model->get(GRB_DoubleParam_MIPGap), gap);
    EXPECT_EQ(model->get(GRB_IntParam_OutputFlag), 0);
    EXPECT_EQ(model->get(GRB_IntParam_Method), GRB_METHOD_DUAL);

    // A name or a value Gurobi refuses.
    EXPECT_THROW(solver.set_option("NoSuchParameter", std::int64_t{1}), std::invalid_argument);
    EXPECT_THROW(solver.set_option("Threads", std::string("many")), std::invalid_argument);
}

namespace {

// The toy: three customers, each visited exactly once by vehicles of capacity 11. Customers 1, 2
// and 3 weigh 4, 5 and 6, so a route serves one or two of them: six routes, whose costs are in
// toy_costs. The LP optimum 37.5 takes half of each two-customer route; its duals (11.5, 12.5,
// 13.5) make the three of them tight, and are unique since the optimum is not degenerate. The
// best plan serves {1, 2} and {3}, at 24 + 20 = 44.
constexpr std::array<double, 6> toy_costs{20.0, 20.0, 20.0, 24.0, 25.0, 26.0};
constexpr double toy_lp = 37.5;
constexpr std::array<double, 3> toy_duals{11.5, 12.5, 13.5};
constexpr std::array<double, 6> toy_lp_values{0.0, 0.0, 0.0, 0.5, 0.5, 0.5};
constexpr double toy_best_plan = 44.0;
constexpr std::array<double, 6> toy_best_plan_values{0.0, 0.0, 1.0, 1.0, 0.0, 0.0};

// The routes serving {1}, {2}, {3}, {1, 2}, {1, 3}, {2, 3}; they get the ids 0 to 5.
std::vector<Solution> toy_routes() {
    const std::vector<std::vector<size_t>> customers{{0}, {1}, {2}, {0, 1}, {0, 2}, {1, 2}};
    std::vector<Solution> routes;
    for (size_t route = 0; route < customers.size(); ++route) {
        std::vector<Row> rows;
        for (const size_t customer : customers[route]) {
            rows.push_back(Row{.index = customer, .coefficient = 1.0});
        }
        routes.push_back(make_path(toy_costs.at(route), std::move(rows)));
    }
    return routes;
}

// The toy as a user builds it: their own model with a row "customer i is visited once" per
// customer, wrapped; every route is a starting column.
struct ToyMaster {
        ToyMaster(GRBModel& model, std::vector<GRBConstr> rows) : solver(model, std::move(rows)) {
            master.add_columns(master.add_pricer(pricer), toy_routes());
        }

        GurobiMasterSolver solver;
        MasterProblem master{solver};
        ScriptedPricer pricer;  // every route is in: it proves that none is missing
};

// Whether @p actual matches @p expected, entry by entry.
::testing::AssertionResult near(std::span<const double> actual, std::span<const double> expected,
                                double accuracy) {
    if (actual.size() != expected.size()) {
        return ::testing::AssertionFailure()
               << actual.size() << " entries instead of " << expected.size();
    }
    for (size_t entry = 0; entry < actual.size(); ++entry) {
        if (std::abs(actual[entry] - expected[entry]) > accuracy) {
            return ::testing::AssertionFailure() << "entry " << entry << " is " << actual[entry]
                                                 << " instead of " << expected[entry];
        }
    }
    return ::testing::AssertionSuccess();
}

class GurobiToyTest : public GurobiMasterSolverTest {
    protected:
        // The user's rows, empty until the routes fill them; row i is customer i + 1.
        std::vector<GRBConstr> customer_rows() {
            std::vector<GRBConstr> rows;
            for (const char* name : {"customer 1", "customer 2", "customer 3"}) {
                rows.push_back(model->addConstr(GRBLinExpr(), GRB_EQUAL, 1.0, name));
            }
            return rows;
        }

        // The value of every route.
        static std::vector<double> route_values(const GurobiMasterSolver& solver) {
            std::vector<ColumnId> ids(toy_costs.size());
            std::iota(ids.begin(), ids.end(), ColumnId{0});
            std::vector<double> values(ids.size());
            solver.primal(ids, values);
            return values;
        }
};

}  // namespace

TEST_F(GurobiToyTest, TheLoopFindsTheLpOptimumAndItsDuals) {
    ToyMaster toy(*model, customer_rows());

    ColumnGeneration loop(toy.master);
    const ColumnGenerationResult result = loop.run();

    EXPECT_EQ(result.status, ColumnGenerationStatus::Optimal);
    EXPECT_NEAR(result.objective, toy_lp, tolerance);
    ASSERT_EQ(toy.pricer.seen_duals.size(), 1U);
    EXPECT_TRUE(near(toy.pricer.seen_duals[0], toy_duals, tolerance));
    EXPECT_TRUE(near(route_values(toy.solver), toy_lp_values, tolerance));
}

TEST_F(GurobiToyTest, TheRestrictedMasterMipFindsTheBestPlan) {
    ToyMaster toy(*model, customer_rows());
    ColumnGeneration loop(toy.master);
    static_cast<void>(loop.run());

    ASSERT_EQ(loop.solve_restricted_master_integer({}), SolverStatus::Optimal);

    EXPECT_NEAR(toy.solver.objective(), toy_best_plan, 1e-6);
    EXPECT_TRUE(near(route_values(toy.solver), toy_best_plan_values, 1e-6));
}
