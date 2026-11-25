// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The Lagrangian bound of each iteration and the early stops on the gap and the cutoff.

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "rcspp/column_generation/column_generation.hpp"
#include "test_column_generation_fakes.hpp"

using namespace rcspp;                              // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation;           // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation::testing;  // NOLINT(google-build-using-namespace)

namespace rcspp::column_generation::testing {

// The convexity dual of the runs below.
inline constexpr double bound_convexity_dual = 1.0;

// A pricing answer with a column per reduced cost (without the convexity dual) and the given
// bound.
inline PricingResult found(double bound, std::initializer_list<double> reduced_costs,
                           bool exact = true) {
    PricingResult result{.status = exact ? PricingStatus::Optimal : PricingStatus::SolverLimit,
                         .min_reduced_cost_bound = bound,
                         .exact_stage = exact};
    for (const double reduced_cost : reduced_costs) {
        result.columns.push_back(make_path(1.0, {{.index = 0, .coefficient = 1.0}}, reduced_cost));
    }
    return result;
}

// A run over one row "cover >= 1" and one pricer that gives `answers` in turn, while the LP values
// are `objectives` and the convexity dual bound_convexity_dual; with the iterations seen.
struct BoundedRun {
        BoundedRun(std::vector<PricingResult> answers, std::vector<double> objectives,
                   ConvexityRow convexity = ConvexityRow{.max_paths = 2.0})
            : answers_(std::move(answers)), objectives_(std::move(objectives)) {
            master.add_rows(1, RowSense::GreaterEqual, 1.0);
            pricer.respond = [this](auto, auto) { return answers_.at(answer_++); };
            master.add_pricer(pricer, convexity);
            solver.on_solve = [this](ScriptedMasterSolver& lp) {
                lp.objective_value = objectives_.at(solve_++);
                lp.dual_values = {0.0, bound_convexity_dual};
                return SolverStatus::Optimal;
            };
            params.initialization = std::make_shared<NoInitialization>();
            params.on_iteration = [this](const IterationInfo& info) { seen.push_back(info); };
        }

        ColumnGenerationResult run() {
            ColumnGeneration loop(master, params);
            return loop.run();
        }

        [[nodiscard]] std::vector<double> bounds() const {
            std::vector<double> out;
            for (const IterationInfo& info : seen) {
                out.push_back(info.lower_bound);
            }
            return out;
        }

        ScriptedMasterSolver solver;
        MasterProblem master{solver};
        ScriptedPricer pricer;
        ColumnGenerationParams params;
        std::vector<IterationInfo> seen;

    private:
        std::vector<PricingResult> answers_;
        std::vector<double> objectives_;
        size_t answer_ = 0;
        size_t solve_ = 0;
};

// The first iteration of the stop tests: LP 10, bound 10 + 2 * (-2 - 1) = 4 (a gap of 6), then a
// proof at 9.
inline ColumnGenerationResult run_with_a_gap_of_six(const ColumnGenerationParams& options) {
    BoundedRun run({found(-2.0, {-1.5}), ScriptedPricer::proven_none()}, {10.0, 9.0});
    run.params.absolute_gap = options.absolute_gap;
    run.params.relative_gap = options.relative_gap;
    run.params.cutoff = options.cutoff;
    run.params.integer_objective = options.integer_objective;
    return run.run();
}

// LP 10, bound 10 + 2 * (0.6 - 1) = 9.2, then a proof at 9.5.
inline ColumnGenerationResult run_with_a_fractional_bound(const ColumnGenerationParams& options) {
    BoundedRun run({found(0.6, {0.6}), ScriptedPricer::proven_none()}, {10.0, 9.5});
    run.params.relative_gap = options.relative_gap;
    run.params.cutoff = options.cutoff;
    run.params.integer_objective = options.integer_objective;
    return run.run();
}

}  // namespace rcspp::column_generation::testing

TEST(ColumnGenerationBound, CountsEachPathAtTheBestReducedCost) {
    BoundedRun run({found(-2.0, {-1.5}), ScriptedPricer::proven_none()}, {10.0, 9.0});
    const ColumnGenerationResult result = run.run();

    EXPECT_EQ(run.bounds(), (std::vector<double>{4.0, 9.0}));
    EXPECT_EQ(result.status, ColumnGenerationStatus::Optimal);
    EXPECT_EQ(result.lower_bound, 9.0);
}

TEST(ColumnGenerationBound, IsNeverAboveAColumnReturned) {
    BoundedRun run({found(0.0, {-4.0}), ScriptedPricer::proven_none()}, {10.0, 9.0});
    static_cast<void>(run.run());
    EXPECT_EQ(run.seen.at(0).lower_bound, 10.0 + 2.0 * (-4.0 - bound_convexity_dual));
}

TEST(ColumnGenerationBound, AProofWithTheThresholdAsItsBoundAddsNothing) {
    // The graph pricer's bound when it finds nothing: the threshold, a tolerance below 0.
    BoundedRun run({found(bound_convexity_dual - 1e-9, {})}, {10.0});
    const ColumnGenerationResult result = run.run();
    EXPECT_EQ(run.seen.at(0).lower_bound, 10.0);
    EXPECT_EQ(result.lower_bound, 10.0);
}

TEST(ColumnGenerationBound, AnUnknownTermLeavesTheBoundUnknown) {
    // A heuristic stage.
    BoundedRun heuristic({found(-infinity, {-1.0}, false), ScriptedPricer::proven_none()},
                         {10.0, 9.0});
    static_cast<void>(heuristic.run());
    EXPECT_EQ(heuristic.bounds(), (std::vector<double>{-infinity, 9.0}));

    // No convexity row.
    BoundedRun no_row({found(-2.0, {-1.0}), ScriptedPricer::proven_none()},
                      {10.0, 9.0},
                      ConvexityRow{});
    static_cast<void>(no_row.run());
    EXPECT_EQ(no_row.bounds(), (std::vector<double>{-infinity, 9.0}));
}

TEST(ColumnGenerationBound, ARowWithoutAnUpperBoundLeavesTheBoundUnknown) {
    ScriptedMasterSolver solver;
    MasterProblem master(solver);
    master.add_rows(1, RowSense::GreaterEqual, 1.0);
    const size_t fleet = master.add_rows(1, RowSense::GreaterEqual, 1.0).first;
    std::vector<PricingResult> answers{found(-2.0, {-1.0}), ScriptedPricer::proven_none()};
    size_t answer = 0;
    ScriptedPricer pricer([&](auto, auto) { return answers.at(answer++); });
    master.add_pricer(pricer, ConvexityRow{.existing_row = fleet});
    std::vector<IterationInfo> seen;
    ColumnGenerationParams params;
    params.initialization = std::make_shared<NoInitialization>();
    params.on_iteration = [&seen](const IterationInfo& info) { seen.push_back(info); };
    ColumnGeneration loop(master, params);
    static_cast<void>(loop.run());

    EXPECT_EQ(seen.at(0).lower_bound, -infinity);
}

TEST(ColumnGenerationBound, AnExistingEqualRowCountsItsRightHandSide) {
    ScriptedMasterSolver solver;
    MasterProblem master(solver);
    master.add_rows(1, RowSense::GreaterEqual, 1.0);
    const size_t fleet = master.add_rows(1, RowSense::Equal, 3.0).first;
    std::vector<PricingResult> answers{found(-2.0, {-1.0}), ScriptedPricer::proven_none()};
    size_t answer = 0;
    ScriptedPricer pricer([&](auto, auto) { return answers.at(answer++); });
    master.add_pricer(pricer, ConvexityRow{.existing_row = fleet});
    solver.dual_values = {0.0, bound_convexity_dual};
    std::vector<IterationInfo> seen;
    ColumnGenerationParams params;
    params.initialization = std::make_shared<NoInitialization>();
    params.on_iteration = [&seen](const IterationInfo& info) { seen.push_back(info); };
    ColumnGeneration loop(master, params);
    static_cast<void>(loop.run());

    EXPECT_EQ(seen.at(0).lower_bound, 3.0 * (-2.0 - bound_convexity_dual));
}

TEST(ColumnGenerationBound, ARowThatAllowsNoPathAddsNothing) {
    BoundedRun run({found(-infinity, {-1.0}, false), ScriptedPricer::proven_none()},
                   {10.0, 9.0},
                   ConvexityRow{.max_paths = 0.0});
    static_cast<void>(run.run());
    EXPECT_EQ(run.seen.at(0).lower_bound, 10.0);
}

TEST(ColumnGenerationBound, ThePricersTermsAddUp) {
    ScriptedMasterSolver solver;
    MasterProblem master(solver);
    master.add_rows(1, RowSense::GreaterEqual, 1.0);
    std::vector<PricingResult> first_answers{found(-2.0, {-1.0}), ScriptedPricer::proven_none()};
    std::vector<PricingResult> second_answers{found(-1.0, {-1.0}), ScriptedPricer::proven_none()};
    size_t first_answer = 0;
    size_t second_answer = 0;
    ScriptedPricer first([&](auto, auto) { return first_answers.at(first_answer++); });
    ScriptedPricer second([&](auto, auto) { return second_answers.at(second_answer++); });
    master.add_pricer(first, ConvexityRow{.max_paths = 2.0});
    master.add_pricer(second, ConvexityRow{.max_paths = 1.0});
    solver.on_solve = [](ScriptedMasterSolver& lp) {
        lp.objective_value = 10.0;
        lp.dual_values = {0.0, 1.0, 0.5};
        return SolverStatus::Optimal;
    };
    std::vector<IterationInfo> seen;
    ColumnGenerationParams params;
    params.initialization = std::make_shared<NoInitialization>();
    params.on_iteration = [&seen](const IterationInfo& info) { seen.push_back(info); };
    ColumnGeneration loop(master, params);
    static_cast<void>(loop.run());

    EXPECT_EQ(seen.at(0).lower_bound, 10.0 + 2.0 * (-2.0 - 1.0) + 1.0 * (-1.0 - 0.5));
}

TEST(ColumnGenerationBound, TheResultKeepsTheBestBound) {
    BoundedRun run({found(-2.0, {-1.0}), found(-3.0, {-1.0}), PricingResult{}}, {10.0, 9.0, 8.0});
    const ColumnGenerationResult result = run.run();

    EXPECT_EQ(run.bounds(), (std::vector<double>{4.0, 1.0, -infinity}));
    EXPECT_EQ(result.status, ColumnGenerationStatus::NotProven);
    EXPECT_EQ(result.lower_bound, 4.0);
}

TEST(ColumnGenerationBound, ARestartByTheConvergenceHookForgetsTheBounds) {
    BoundedRun run({found(-2.0, {-1.0}), ScriptedPricer::proven_none(), PricingResult{}},
                   {10.0, 9.0, 8.0});
    std::vector<size_t> restarts;
    run.params.on_converged = [&restarts](ConvergenceContext& context) {
        restarts.push_back(context.iteration);
        return ConvergenceAction::Continue;
    };
    const ColumnGenerationResult result = run.run();

    EXPECT_EQ(restarts, (std::vector<size_t>{2}));
    EXPECT_EQ(result.status, ColumnGenerationStatus::NotProven);
    EXPECT_EQ(result.lower_bound, -infinity);
}

TEST(ColumnGenerationBound, TheStopsOnTheGapAndTheCutoff) {
    struct Case {
            ColumnGenerationParams options;
            ColumnGenerationStatus status;
    };
    const std::vector<Case> cases{
        {.options = {}, .status = ColumnGenerationStatus::Optimal},
        {.options = {.absolute_gap = 6.0}, .status = ColumnGenerationStatus::AbsoluteGapLimit},
        {.options = {.absolute_gap = 5.9}, .status = ColumnGenerationStatus::Optimal},
        {.options = {.relative_gap = 0.6}, .status = ColumnGenerationStatus::RelativeGapLimit},
        {.options = {.relative_gap = 0.5}, .status = ColumnGenerationStatus::Optimal},
        {.options = {.cutoff = 4.0}, .status = ColumnGenerationStatus::Cutoff},
        {.options = {.cutoff = 4.5}, .status = ColumnGenerationStatus::Optimal},
        // When several hold, the first checked gives the status.
        {.options = {.absolute_gap = 6.0, .relative_gap = 0.6, .cutoff = 4.0},
         .status = ColumnGenerationStatus::Cutoff},
        {.options = {.absolute_gap = 6.0, .relative_gap = 0.6},
         .status = ColumnGenerationStatus::AbsoluteGapLimit},
    };

    for (const Case& stop : cases) {
        const ColumnGenerationResult result = run_with_a_gap_of_six(stop.options);
        EXPECT_EQ(result.status, stop.status);
        if (stop.status == ColumnGenerationStatus::Optimal) {
            EXPECT_EQ(result.iterations, 2U);
            EXPECT_EQ(result.objective, 9.0);
            EXPECT_EQ(result.lower_bound, 9.0);
        } else {
            EXPECT_EQ(result.iterations, 1U);
            EXPECT_EQ(result.objective, 10.0);
            EXPECT_EQ(result.lower_bound, 4.0);
        }
    }
}

TEST(ColumnGenerationBound, AnIntegerObjectiveRoundsTheBoundUp) {
    struct Case {
            ColumnGenerationParams options;
            ColumnGenerationStatus status;
    };
    const std::vector<Case> cases{
        {.options = {.integer_objective = true},
         .status = ColumnGenerationStatus::IntegerObjectiveGap},
        {.options = {.cutoff = 10.0, .integer_objective = true},
         .status = ColumnGenerationStatus::Cutoff},
        {.options = {.cutoff = 11.0, .integer_objective = true},
         .status = ColumnGenerationStatus::IntegerObjectiveGap},
        {.options = {.cutoff = 10.0}, .status = ColumnGenerationStatus::Optimal},
        {.options = {.relative_gap = 0.1, .integer_objective = true},
         .status = ColumnGenerationStatus::RelativeGapLimit},
        {.options = {}, .status = ColumnGenerationStatus::Optimal},
    };

    // With integer costs, any solution costs at least 10, the LP value rounded up; without, the
    // gap 0.8 goes on.
    for (const Case& stop : cases) {
        const ColumnGenerationResult result = run_with_a_fractional_bound(stop.options);
        EXPECT_EQ(result.status, stop.status);
        EXPECT_EQ(result.iterations, stop.status == ColumnGenerationStatus::Optimal ? 2U : 1U);
    }
}

TEST(ColumnGenerationBound, TheStopsWaitForColumnsAndABound) {
    // A proof wins over the cutoff: the run is optimal.
    BoundedRun proof({ScriptedPricer::proven_none()}, {10.0});
    proof.params.cutoff = -1e9;
    EXPECT_EQ(proof.run().status, ColumnGenerationStatus::Optimal);

    // No bound: nothing to compare.
    BoundedRun unknown({found(-infinity, {-1.0}, false), ScriptedPricer::proven_none()},
                       {10.0, 9.0});
    unknown.params.absolute_gap = 1e9;
    unknown.params.cutoff = -1e9;
    const ColumnGenerationResult result = unknown.run();
    EXPECT_EQ(result.status, ColumnGenerationStatus::Optimal);
    EXPECT_EQ(result.iterations, 2U);
}

TEST(ColumnGenerationBound, AGapBelowZeroOrNotANumberIsRefused) {
    ScriptedMasterSolver solver;
    MasterProblem master(solver);

    for (const double gap : {-1.0, std::nan("")}) {
        EXPECT_THROW(ColumnGeneration(master, ColumnGenerationParams{.absolute_gap = gap}),
                     std::invalid_argument);
        EXPECT_THROW(ColumnGeneration(master, ColumnGenerationParams{.relative_gap = gap}),
                     std::invalid_argument);
    }
}
