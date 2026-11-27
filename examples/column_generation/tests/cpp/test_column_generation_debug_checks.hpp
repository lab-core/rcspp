// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The debug checks: each fires on a master solver or a pricer that contradicts the others, stays
// silent on noise below the tolerance, skips what it cannot check, and costs nothing when off.

#include <gtest/gtest.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "rcspp/column_generation/column_generation.hpp"
#include "rcspp/column_generation/debug_checks.hpp"
#include "test_column_generation_fakes.hpp"

using namespace rcspp;                              // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation;           // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation::testing;  // NOLINT(google-build-using-namespace)

namespace rcspp::column_generation::testing {

// One LP's answers: its value, its duals and its columns' values.
struct ScriptedLp {
        double objective;
        std::vector<double> duals;
        std::unordered_map<ColumnId, double> values;
};

// Row 0 "cover >= 1" with an artificial column (id 0, cost 100), and a pricer whose convexity row
// 1 allows one path; the pricer offers a path of cost 10 covering row 0 while its reduced cost is
// negative. The LP of iteration 1 uses the artificial column: value 100, duals (100, 0). That of
// iteration 2 uses the path (id 1): value 10, duals (12, -2), so that the path's reduced cost,
// 10 - 12 - (-2), is 0 with its convexity entry. A test may change `lps` before the run.
struct DebugToy {
        static constexpr double path_cost = 10.0;
        static constexpr double artificial_cost = 100.0;

        DebugToy() {
            master.add_rows(1, RowSense::GreaterEqual, 1.0);
            master.add_pricer(pricer, ConvexityRow{.max_paths = 1.0});
            const std::vector<size_t> rows{0};
            const std::vector<double> coefficients{1.0};
            master.add_artificial_columns(rows, coefficients, artificial_cost);
            solver.on_solve = [this](ScriptedMasterSolver& lp) {
                const ScriptedLp& answer =
                    lps[std::min(lp.relaxation_calls.size(), lps.size()) - 1];
                lp.objective_value = answer.objective;
                lp.dual_values = answer.duals;
                lp.values = answer.values;
                return SolverStatus::Optimal;
            };
            pricer.respond = [this](std::span<const double> duals, const PricingRequest& request) {
                PricingResult result = ScriptedPricer::proven_none();
                const double reduced_cost = reported(duals);
                if (reduced_cost < request.reduced_cost_threshold) {
                    result.columns.push_back(
                        make_path(path_cost, {{.index = 0, .coefficient = 1.0}}, reduced_cost));
                    result.min_reduced_cost_bound = reduced_cost;
                }
                return result;
            };
            params.initialization = std::make_shared<NoInitialization>();
            // A broken LP may keep offering the same path: a few iterations are enough.
            params.max_iterations = 5;
        }

        ColumnGenerationResult run(DebugChecks checks) {
            params.debug_checks = checks;
            ColumnGeneration loop(master, params);
            return loop.run();
        }

        // The error a run throws; fails the test when it throws none.
        DebugCheckError failure(DebugChecks checks) {
            try {
                (void)run(checks);
            } catch (const DebugCheckError& error) {
                return error;
            }
            ADD_FAILURE() << "no DebugCheckError";
            return {"", 0, ""};
        }

        std::vector<ScriptedLp> lps{{artificial_cost, {artificial_cost, 0.0}, {{0, 1.0}}},
                                    {path_cost, {12.0, -2.0}, {{1, 1.0}}}};
        // What the pricer reports as its path's reduced cost: by default, the true one.
        std::function<double(std::span<const double>)> reported =
            [](std::span<const double> duals) { return path_cost - duals[0]; };

        ScriptedMasterSolver solver;
        MasterProblem master{solver};
        ScriptedPricer pricer;
        ColumnGenerationParams params;
};

inline constexpr DebugChecks every_check{.pricer_reduced_costs = true,
                                         .dual_signs = true,
                                         .master_reduced_costs = true,
                                         .objective = true};

// The four faults, one per check.
inline void misreport(DebugToy& toy) {
    toy.reported = [](std::span<const double> duals) {
        return DebugToy::path_cost - duals[0] + 1.0;
    };
}

inline void flip_signs(DebugToy& toy) {
    toy.lps[0] = {DebugToy::artificial_cost, {-1.0, 1.0}, {{0, 1.0}}};
}

inline void keep_old_duals(DebugToy& toy) {
    toy.lps[1] = {DebugToy::path_cost, {DebugToy::artificial_cost, 0.0}, {{0, 1.0}, {1, 1.0}}};
}

inline void add_to_objective(DebugToy& toy) {
    toy.lps[1] = {DebugToy::path_cost + 1.0, {12.0, -2.0}, {{1, 1.0}}};
}

}  // namespace rcspp::column_generation::testing

// MARK: A correct run

TEST(ColumnGenerationDebugChecks, ACorrectRunPassesEveryCheck) {
    DebugToy toy;
    EXPECT_EQ(toy.run(every_check).status, ColumnGenerationStatus::Optimal);
    EXPECT_EQ(toy.solver.relaxation_calls.size(), 2U);
}

TEST(ColumnGenerationDebugChecks, TheChecksAreOffByDefault) {
    const DebugChecks checks = ColumnGenerationParams{}.debug_checks;
    EXPECT_FALSE(checks.pricer_reduced_costs);
    EXPECT_FALSE(checks.dual_signs);
    EXPECT_FALSE(checks.master_reduced_costs);
    EXPECT_FALSE(checks.objective);
}

TEST(ColumnGenerationDebugChecks, ArtificialColumnsKeepTheirEntry) {
    DebugToy toy;
    const MasterColumn& artificial = toy.master.columns()[0];
    EXPECT_EQ(artificial.tag, ColumnTag::Artificial);
    EXPECT_EQ(artificial.solution.column.cost, DebugToy::artificial_cost);
    ASSERT_EQ(artificial.solution.column.rows.size(), 1U);
    EXPECT_EQ(artificial.solution.column.rows[0].index, 0U);
    EXPECT_EQ(artificial.solution.column.rows[0].coefficient, 1.0);
    EXPECT_TRUE(artificial.solution.path_arc_ids.empty());
}

// MARK: Each check

TEST(ColumnGenerationDebugChecks, PricerReducedCostsFireOnAMisreportedReducedCost) {
    DebugToy toy;
    misreport(toy);
    const DebugCheckError error = toy.failure(every_check);
    EXPECT_EQ(error.check(), "pricer_reduced_costs");
    EXPECT_EQ(error.iteration(), 1U);
    EXPECT_EQ(error.message(),
              "pricer 0's column 0 reports the reduced cost -89, its cost and rows give -90; "
              "tolerance 1e-09");
    EXPECT_EQ(std::string(error.what()),
              "debug check pricer_reduced_costs failed at iteration 1: " + error.message());
}

TEST(ColumnGenerationDebugChecks, PricerReducedCostsAllowNoiseBelowTheTolerance) {
    // The scale: |10| + |100 * 1|; the tolerance 1e-9 by default.
    const double noise = 0.5 * 1e-9 * (DebugToy::path_cost + DebugToy::artificial_cost);
    DebugToy quiet;
    quiet.reported = [noise](std::span<const double> duals) {
        return DebugToy::path_cost - duals[0] + noise;
    };
    EXPECT_EQ(quiet.run(every_check).status, ColumnGenerationStatus::Optimal);

    DebugToy loud;
    loud.reported = [noise](std::span<const double> duals) {
        return DebugToy::path_cost - duals[0] + 3 * noise;
    };
    EXPECT_EQ(loud.failure(every_check).check(), "pricer_reduced_costs");
}

TEST(ColumnGenerationDebugChecks, PricerReducedCostsSkipStartingColumns) {
    // A starting column's reduced cost was never priced: whatever it holds is not checked.
    DebugToy toy;
    std::vector<Solution> start;
    start.push_back(
        make_path(DebugToy::path_cost + 5.0, {{.index = 0, .coefficient = 1.0}}, 12345.0));
    toy.master.add_columns(0, std::move(start));
    EXPECT_EQ(toy.run(DebugChecks{.pricer_reduced_costs = true}).status,
              ColumnGenerationStatus::Optimal);
}

TEST(ColumnGenerationDebugChecks, DualSignsFireOnAWrongSignOfAGreaterOrLessRow) {
    DebugToy greater;
    greater.lps[0] = {DebugToy::artificial_cost, {-1.0, 0.0}, {{0, 1.0}}};
    const DebugCheckError first = greater.failure(DebugChecks{.dual_signs = true});
    EXPECT_EQ(first.check(), "dual_signs");
    EXPECT_EQ(first.iteration(), 1U);
    EXPECT_EQ(first.message(), "row 0 (>= 1) has the dual -1; tolerance 1e-09");

    DebugToy less;
    less.lps[0] = {DebugToy::artificial_cost, {DebugToy::artificial_cost, 1.0}, {{0, 1.0}}};
    EXPECT_EQ(less.failure(DebugChecks{.dual_signs = true}).message(),
              "row 1 (<= 1) has the dual 1; tolerance 1e-09");
}

TEST(ColumnGenerationDebugChecks, DualSignsCountTheOtherRowsThatFail) {
    DebugToy toy;
    flip_signs(toy);
    EXPECT_EQ(toy.failure(DebugChecks{.dual_signs = true}).message(),
              "row 0 (>= 1) has the dual -1 (1 more); tolerance 1e-09");
}

TEST(ColumnGenerationDebugChecks, DualSignsSkipEqualAndRangedRowsAndCheckRowsWithNoBound) {
    // Rows 2 (= 3), 3 (0 ... 2) and 4 (no bound), none used by a column.
    const auto toy_with_rows = [](DebugToy& toy, double dual_of_free_row) {
        toy.master.add_rows(1, RowSense::Equal, 3.0);
        const std::vector<double> lower{0.0, -infinity};
        const std::vector<double> upper{2.0, infinity};
        toy.master.add_rows(lower, upper);
        toy.lps = {{DebugToy::artificial_cost,
                    {DebugToy::artificial_cost, 0.0, -7.0, -5.0, dual_of_free_row},
                    {{0, 1.0}}},
                   {DebugToy::path_cost, {12.0, -2.0, 7.0, 5.0, dual_of_free_row}, {{1, 1.0}}}};
    };

    DebugToy quiet;
    toy_with_rows(quiet, 0.0);
    EXPECT_EQ(quiet.run(DebugChecks{.dual_signs = true}).status, ColumnGenerationStatus::Optimal);

    DebugToy loud;
    toy_with_rows(loud, 0.5);
    EXPECT_EQ(loud.failure(DebugChecks{.dual_signs = true}).message(),
              "row 4 (no bound) has the dual 0.5; tolerance 1e-09");
}

TEST(ColumnGenerationDebugChecks, DualSignsAllowTheMasterSolversOwnTolerance) {
    DebugToy toy;
    toy.solver.dual_tolerance_value = 1e-3;
    toy.lps[0] = {DebugToy::artificial_cost, {DebugToy::artificial_cost, 5e-4}, {{0, 1.0}}};
    EXPECT_EQ(toy.run(DebugChecks{.dual_signs = true}).status, ColumnGenerationStatus::Optimal);
}

TEST(ColumnGenerationDebugChecks, MasterReducedCostsFireOnTheDualsOfAnotherSolve) {
    // Iteration 2 answers iteration 1's duals: the path's reduced cost is 10 - 100 = -90.
    DebugToy toy;
    toy.lps[1] = {DebugToy::path_cost, {DebugToy::artificial_cost, 0.0}, {{1, 1.0}}};
    const DebugCheckError error = toy.failure(DebugChecks{.master_reduced_costs = true});
    EXPECT_EQ(error.check(), "master_reduced_costs");
    EXPECT_EQ(error.iteration(), 2U);
    EXPECT_EQ(error.message(), "column 1 has the reduced cost -90; tolerance 1e-09");
}

TEST(ColumnGenerationDebugChecks, MasterReducedCostsFireOnAUsedColumnWhoseReducedCostIsNotZero) {
    // Iteration 2 says the artificial column is used, while its reduced cost is 100 - 12.
    DebugToy toy;
    toy.lps[1] = {DebugToy::path_cost, {12.0, -2.0}, {{0, 1.0}, {1, 1.0}}};
    const DebugCheckError error = toy.failure(DebugChecks{.master_reduced_costs = true});
    EXPECT_EQ(error.iteration(), 2U);
    EXPECT_EQ(error.message(), "column 0, of value 1, has the reduced cost 88; tolerance 1e-09");
}

TEST(ColumnGenerationDebugChecks, ObjectiveFiresOnAValueItsColumnsDoNotCost) {
    DebugToy toy;
    add_to_objective(toy);
    const DebugCheckError error = toy.failure(DebugChecks{.objective = true});
    EXPECT_EQ(error.check(), "objective");
    EXPECT_EQ(error.iteration(), 2U);
    EXPECT_EQ(error.message(), "the LP value is 11, its columns' values cost 10; tolerance 1e-09");
}

TEST(ColumnGenerationDebugChecks, ObjectiveIsNotCheckedWhenTheUsersVariablesCostSomething) {
    DebugToy toy;
    toy.solver.largest_user_cost_value = 1.0;
    add_to_objective(toy);
    EXPECT_EQ(toy.run(DebugChecks{.objective = true}).status, ColumnGenerationStatus::Optimal);
}

// MARK: Around a failure

TEST(ColumnGenerationDebugChecks, EachFaultGoesUnseenWhenItsCheckIsOff) {
    const std::vector<std::pair<std::string, void (*)(DebugToy&)>> faults{
        {"pricer_reduced_costs", &misreport},
        {"dual_signs", &flip_signs},
        {"master_reduced_costs", &keep_old_duals},
        {"objective", &add_to_objective}};

    for (const auto& [check, fault] : faults) {
        DebugToy off;
        fault(off);
        EXPECT_NO_THROW((void)off.run(DebugChecks{})) << check;

        DebugToy on;
        fault(on);
        EXPECT_EQ(on.failure(every_check).check(), check);
    }
}

TEST(ColumnGenerationDebugChecks, AFailureStopsBeforeTheHookAndRestoresTheUsersModel) {
    DebugToy toy;
    add_to_objective(toy);
    std::vector<size_t> seen;
    toy.params.on_iteration = [&seen](const IterationInfo& info) {
        seen.push_back(info.iteration);
    };
    (void)toy.failure(DebugChecks{.objective = true});
    EXPECT_EQ(seen, std::vector<size_t>{1});
    EXPECT_EQ(toy.solver.restore_calls, 1U);
}

TEST(ColumnGenerationDebugChecks, ASecondRunChecksTheColumnsAddedBetweenRuns) {
    DebugToy toy;
    toy.params.debug_checks = every_check;
    ColumnGeneration loop(toy.master, toy.params);
    EXPECT_EQ(loop.run().status, ColumnGenerationStatus::Optimal);

    // A column costing 1 under the duals (12, -2): its reduced cost is 1 - 12 + 2 < 0.
    std::vector<Solution> column;
    column.push_back(make_path(1.0, {{.index = 0, .coefficient = 1.0}}, 0.0));
    toy.master.add_columns(0, std::move(column));

    try {
        (void)loop.run();
        ADD_FAILURE() << "no DebugCheckError";
    } catch (const DebugCheckError& error) {
        EXPECT_EQ(error.message(), "column 2 has the reduced cost -9; tolerance 1e-09");
    }
}
