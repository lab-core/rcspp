// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The column generation loop: statuses, hooks, limits, statistics and default components.

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <span>
#include <stdexcept>
#include <thread>
#include <vector>

#include "rcspp/column_generation/column_generation.hpp"
#include "test_column_generation_fakes.hpp"

using namespace rcspp;                              // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation;           // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation::testing;  // NOLINT(google-build-using-namespace)

namespace rcspp::column_generation::testing {

// The smallest master with a real choice: one row "cover ≥ 1"; the pricer offers one path of
// cost 10 covering it while its dual is above 10 and the path is allowed. With only the
// artificial column, the LP pays the artificial cost; once the path is in, it costs 10.
struct CoverOneRow {
        static constexpr double path_cost = 10.0;

        CoverOneRow() {
            master.add_rows(1, RowSense::GreaterEqual, 1.0);
            master.add_pricer(pricer);
            solver.on_solve = [](ScriptedMasterSolver& lp) {
                const bool has_path = lp.columns.size() > 1;
                const double artificial_cost = lp.columns.front().cost;
                lp.objective_value = has_path ? path_cost : artificial_cost;
                lp.dual_values = {lp.objective_value};
                lp.values = {{0, has_path ? 0.0 : 1.0}, {1, 1.0}};
                return SolverStatus::Optimal;
            };
            pricer.respond = [this](std::span<const double> duals, const PricingRequest& request) {
                PricingResult result = ScriptedPricer::proven_none();
                const double reduced_cost = path_cost - duals[0];
                if (path_allowed && reduced_cost < request.reduced_cost_threshold) {
                    result.columns.push_back(
                        make_path(path_cost, {{.index = 0, .coefficient = 1.0}}, reduced_cost));
                }
                return result;
            };
        }

        ScriptedMasterSolver solver;
        MasterProblem master{solver};
        ScriptedPricer pricer;
        bool path_allowed = true;
};

// What a convergence hook saw: the loop converges without the path, the hook allows it and
// restarts the loop, which converges again with the path.
struct RestartedLoop {
        std::vector<ConvergenceContext> contexts;
        std::vector<size_t> value_counts;  // the values' spans do not outlive the hook
        ColumnGenerationResult result;
};

inline RestartedLoop restart_once_the_path_is_allowed(CoverOneRow& toy) {
    RestartedLoop restarted;
    toy.path_allowed = false;
    ColumnGenerationParams params;
    params.on_converged = [&](ConvergenceContext& context) {
        restarted.contexts.push_back(context);
        restarted.value_counts.push_back(context.values.size());
        if (!toy.path_allowed) {
            toy.path_allowed = true;  // e.g. a pricer changed its problem
            context.master.pricer(0).update(PricingChanges{.structure = true});
            return ConvergenceAction::Continue;
        }
        return ConvergenceAction::Stop;
    };

    ColumnGeneration loop(toy.master, params);
    restarted.result = loop.run();
    return restarted;
}

// What the iteration hook saw while the loop solved @p toy.
inline std::vector<IterationInfo> iterations_seen_by_the_hook(CoverOneRow& toy) {
    std::vector<IterationInfo> seen;
    ColumnGenerationParams params;
    params.on_iteration = [&seen](const IterationInfo& info) { seen.push_back(info); };
    ColumnGeneration loop(toy.master, params);
    static_cast<void>(loop.run());
    return seen;
}

}  // namespace rcspp::column_generation::testing

TEST(ColumnGenerationLoop, ConvergesAndPushesTheArtificialColumnOut) {
    CoverOneRow toy;
    ColumnGeneration loop(toy.master);
    const ColumnGenerationResult result = loop.run();

    EXPECT_EQ(result.status, ColumnGenerationStatus::Optimal);
    EXPECT_EQ(result.iterations, 2U);
    EXPECT_DOUBLE_EQ(result.objective, 10.0);
    EXPECT_DOUBLE_EQ(result.lower_bound, 10.0);
    EXPECT_EQ(result.statistics.generated_columns, 1U);
    EXPECT_EQ(result.statistics.artificial_columns, 1U);
    EXPECT_EQ(result.statistics.artificial_cost, ArtificialColumns::automatic_cost_factor);
    EXPECT_GE(result.statistics.total_seconds, 0.0);
    EXPECT_EQ(toy.solver.columns[0].cost, ArtificialColumns::automatic_cost_factor);
    // No convexity row: the threshold is minus the reduced-cost tolerance.
    ASSERT_EQ(toy.pricer.requests.size(), 2U);
    EXPECT_DOUBLE_EQ(toy.pricer.requests[0].reduced_cost_threshold, -1e-9);
}

TEST(ColumnGenerationLoop, ThresholdIncludesTheConvexityDual) {
    ScriptedMasterSolver solver;
    MasterProblem master(solver);
    master.add_rows(1, RowSense::GreaterEqual, 1.0);
    ScriptedPricer pricer;
    constexpr double max_paths = 4.0;
    master.add_pricer(pricer, ConvexityRow{.max_paths = max_paths});  // row 1
    constexpr double cover_dual = 12.0;
    constexpr double convexity_dual = 3.0;
    solver.on_solve = [](ScriptedMasterSolver& lp) {
        lp.dual_values = {cover_dual, convexity_dual};
        return SolverStatus::Optimal;
    };
    ColumnGeneration loop(master);
    static_cast<void>(loop.run());

    ASSERT_EQ(pricer.requests.size(), 1U);
    EXPECT_DOUBLE_EQ(pricer.requests[0].reduced_cost_threshold, convexity_dual - 1e-9);
    EXPECT_EQ(pricer.seen_duals[0], (std::vector<double>{cover_dual, convexity_dual}));
}

TEST(ColumnGenerationLoop, TheLpSolverToleranceBoundsTheThreshold) {
    // The LP solver treats a reduced cost above −1e-6 as zero: asking for less would bring back,
    // forever, columns the solver leaves out.
    constexpr double solver_tolerance = 1e-6;
    CoverOneRow toy;
    toy.solver.dual_tolerance_value = solver_tolerance;
    ColumnGeneration loop(toy.master);
    static_cast<void>(loop.run());
    ASSERT_FALSE(toy.pricer.requests.empty());
    EXPECT_DOUBLE_EQ(toy.pricer.requests[0].reduced_cost_threshold, -solver_tolerance);

    // A larger tolerance of the user's is kept.
    constexpr double user_tolerance = 1e-4;
    CoverOneRow larger;
    larger.solver.dual_tolerance_value = solver_tolerance;
    ColumnGenerationParams params;
    params.tolerances = std::make_shared<AbsoluteTolerances>(user_tolerance);
    ColumnGeneration user_loop(larger.master, params);
    static_cast<void>(user_loop.run());
    EXPECT_DOUBLE_EQ(larger.pricer.requests[0].reduced_cost_threshold, -user_tolerance);
}

TEST(ColumnGenerationLoop, NotProvenWithoutAnExactOptimalPricing) {
    for (const PricingResult& answer : {PricingResult{.status = PricingStatus::SolverLimit,
                                                      .min_reduced_cost_bound = 0.0,
                                                      .exact_stage = true},
                                        PricingResult{.status = PricingStatus::Optimal,
                                                      .min_reduced_cost_bound = 0.0,
                                                      .exact_stage = false},
                                        PricingResult{.status = PricingStatus::Infeasible,
                                                      .min_reduced_cost_bound = 0.0,
                                                      .exact_stage = false}}) {
        ScriptedMasterSolver solver;
        MasterProblem master(solver);
        ScriptedPricer pricer([answer](auto, auto) { return answer; });
        master.add_pricer(pricer);
        ColumnGeneration loop(master);
        const ColumnGenerationResult result = loop.run();
        EXPECT_EQ(result.status, ColumnGenerationStatus::NotProven);
        EXPECT_EQ(result.lower_bound, -infinity);
    }
}

TEST(ColumnGenerationLoop, AnExactPricerWithNoPathProves) {
    // The pricer's problem has no path at all, e.g. a vehicle type that can serve nobody.
    const PricingResult no_path{.status = PricingStatus::Infeasible,
                                .min_reduced_cost_bound = 0.0,
                                .exact_stage = true};

    // Its columns are not needed: the master is still optimal.
    CoverOneRow toy;
    ScriptedPricer useless([no_path](auto, auto) { return no_path; });
    toy.master.add_pricer(useless);
    ColumnGeneration loop(toy.master);
    EXPECT_EQ(loop.run().status, ColumnGenerationStatus::Optimal);

    // Its columns are needed: an artificial column stays, and that is now a proof.
    ScriptedMasterSolver solver;
    MasterProblem master(solver);
    master.add_rows(1, RowSense::GreaterEqual, 1.0);
    ScriptedPricer only([no_path](auto, auto) { return no_path; });
    master.add_pricer(only);
    solver.on_solve = [](ScriptedMasterSolver& lp) {
        lp.values = {{0, 1.0}};
        return SolverStatus::Optimal;
    };
    ColumnGeneration needed(master);
    EXPECT_EQ(needed.run().status, ColumnGenerationStatus::Infeasible);
}

TEST(ColumnGenerationLoop, InfeasibleWhenAnArtificialColumnStays) {
    ScriptedMasterSolver solver;
    MasterProblem master(solver);
    master.add_rows(1, RowSense::GreaterEqual, 1.0);
    ScriptedPricer pricer;  // never finds a column
    master.add_pricer(pricer);
    solver.on_solve = [](ScriptedMasterSolver& lp) {
        lp.values = {{0, 1.0}};
        return SolverStatus::Optimal;
    };
    ColumnGeneration loop(master);
    const ColumnGenerationResult result = loop.run();
    EXPECT_EQ(result.status, ColumnGenerationStatus::Infeasible);
    EXPECT_EQ(result.lower_bound, -infinity);
}

TEST(ColumnGenerationLoop, ConvergenceHookCanRestartTheLoop) {
    CoverOneRow toy;
    const RestartedLoop restarted = restart_once_the_path_is_allowed(toy);

    ASSERT_EQ(restarted.contexts.size(), 2U);
    EXPECT_EQ(restarted.contexts[0].iteration, 1U);
    EXPECT_EQ(restarted.contexts[0].objective, ArtificialColumns::automatic_cost_factor);
    EXPECT_EQ(restarted.contexts[0].duals.size(), 1U);
    EXPECT_EQ(restarted.value_counts, (std::vector<size_t>{1, 2}));
    EXPECT_EQ(restarted.contexts[1].iteration, 3U);
    EXPECT_EQ(restarted.result.status, ColumnGenerationStatus::Optimal);
    EXPECT_DOUBLE_EQ(restarted.result.objective, CoverOneRow::path_cost);
}

TEST(ColumnGenerationLoop, IterationHookSeesEveryIteration) {
    CoverOneRow toy;
    const std::vector<IterationInfo> seen = iterations_seen_by_the_hook(toy);

    ASSERT_EQ(seen.size(), 2U);
    EXPECT_EQ(seen[0].iteration, 1U);
    EXPECT_EQ(seen[0].new_columns, 1U);
    EXPECT_EQ(seen[0].total_columns, 2U);
    EXPECT_EQ(seen[1].iteration, 2U);
    EXPECT_EQ(seen[1].new_columns, 0U);
    EXPECT_DOUBLE_EQ(seen[1].objective, CoverOneRow::path_cost);
    EXPECT_GE(seen[1].elapsed_seconds, seen[0].elapsed_seconds);
}

TEST(ColumnGenerationLoop, IterationLimit) {
    CoverOneRow toy;
    ColumnGenerationParams params;
    params.max_iterations = 1;
    ColumnGeneration loop(toy.master, params);
    const ColumnGenerationResult result = loop.run();
    EXPECT_EQ(result.status, ColumnGenerationStatus::IterationLimit);
    EXPECT_EQ(result.iterations, 1U);

    CoverOneRow untouched;
    params.max_iterations = 0;
    ColumnGeneration no_iteration(untouched.master, params);
    EXPECT_EQ(no_iteration.run().status, ColumnGenerationStatus::IterationLimit);
    EXPECT_TRUE(untouched.solver.relaxation_calls.empty());
}

TEST(ColumnGenerationLoop, TimeLimitAndSolverErrors) {
    CoverOneRow no_time;
    ColumnGenerationParams params;
    params.timeout_s = 0.0;
    ColumnGeneration out_of_time(no_time.master, params);
    EXPECT_EQ(out_of_time.run().status, ColumnGenerationStatus::TimeLimit);
    EXPECT_TRUE(no_time.solver.relaxation_calls.empty());

    CoverOneRow solver_time;
    solver_time.solver.on_solve = [](ScriptedMasterSolver&) { return SolverStatus::TimeLimit; };
    ColumnGeneration solver_out_of_time(solver_time.master);
    EXPECT_EQ(solver_out_of_time.run().status, ColumnGenerationStatus::TimeLimit);

    CoverOneRow solver_failure;
    solver_failure.solver.on_solve = [](ScriptedMasterSolver&) { return SolverStatus::Other; };
    ColumnGeneration failing(solver_failure.master);
    const ColumnGenerationResult result = failing.run();
    EXPECT_EQ(result.status, ColumnGenerationStatus::SolverError);
    EXPECT_EQ(result.iterations, 0U);
}

TEST(ColumnGenerationLoop, TimeLimitReachedDuringPricing) {
    // The pricer uses up the run's time and proves nothing: the run hit its limit.
    constexpr double run_limit_s = 0.05;
    CoverOneRow toy;
    toy.pricer.respond = [](std::span<const double> /*duals*/, const PricingRequest& request) {
        std::this_thread::sleep_for(std::chrono::duration<double>(request.timeout_s * 2));
        return PricingResult{.status = PricingStatus::SolverLimit, .exact_stage = true};
    };
    ColumnGenerationParams params;
    params.timeout_s = run_limit_s;
    ColumnGeneration loop(toy.master, params);
    EXPECT_EQ(loop.run().status, ColumnGenerationStatus::TimeLimit);
}

TEST(ColumnGenerationLoop, InfeasibleStartingMasterHasItsOwnStatus) {
    CoverOneRow toy;
    toy.solver.on_solve = [](ScriptedMasterSolver&) { return SolverStatus::Infeasible; };
    ColumnGenerationParams params;
    params.initialization = std::make_shared<NoInitialization>();
    ColumnGeneration loop(toy.master, params);
    const ColumnGenerationResult result = loop.run();

    EXPECT_EQ(result.status, ColumnGenerationStatus::RestrictedMasterInfeasible);
    EXPECT_EQ(result.iterations, 0U);
    EXPECT_EQ(toy.master.number_of_columns(), 0U);
}

TEST(ColumnGenerationLoop, UnboundedMasterHasItsOwnStatus) {
    CoverOneRow toy;
    toy.solver.on_solve = [](ScriptedMasterSolver&) { return SolverStatus::Unbounded; };
    ColumnGeneration loop(toy.master);
    EXPECT_EQ(loop.run().status, ColumnGenerationStatus::Unbounded);

    // A value outside the enum (e.g. from a buggy master solver) is a solver error.
    CoverOneRow unknown;
    constexpr int outside_the_enum = 99;
    unknown.solver.on_solve = [](ScriptedMasterSolver&) {
        return static_cast<SolverStatus>(outside_the_enum);
    };
    ColumnGeneration unknown_loop(unknown.master);
    EXPECT_EQ(unknown_loop.run().status, ColumnGenerationStatus::SolverError);
}

TEST(ColumnGenerationLoop, NoInitializationKeepsAFeasibleModelAsItIs) {
    // The user's model is feasible on its own: one starting column covers the row.
    CoverOneRow toy;
    constexpr double starting_cost = 12.0;  // above the path's
    toy.master.add_columns(0, {make_path(starting_cost, {{.index = 0, .coefficient = 1.0}}, 0.0)});
    toy.solver.on_solve = [](ScriptedMasterSolver& lp) {
        const bool cheaper_path = lp.columns.size() > 1;
        lp.objective_value = cheaper_path ? CoverOneRow::path_cost : starting_cost;
        lp.dual_values = {lp.objective_value};
        lp.values = {{0, cheaper_path ? 0.0 : 1.0}, {1, 1.0}};
        return SolverStatus::Optimal;
    };
    ColumnGenerationParams params;
    params.initialization = std::make_shared<NoInitialization>();
    ColumnGeneration loop(toy.master, params);
    const ColumnGenerationResult result = loop.run();

    EXPECT_EQ(result.status, ColumnGenerationStatus::Optimal);
    EXPECT_DOUBLE_EQ(result.objective, CoverOneRow::path_cost);
    EXPECT_EQ(result.statistics.artificial_columns, 0U);
    EXPECT_TRUE(toy.master.artificial_column_ids().empty());
    EXPECT_EQ(toy.master.number_of_columns(), 2U);  // the starting column and the generated one
}

TEST(ColumnGenerationLoop, TimeLimitIsPassedOn) {
    CoverOneRow toy;
    ColumnGenerationParams params;
    params.timeout_s = 100.0;
    params.relaxation.threads = 2;
    ColumnGeneration loop(toy.master, params);
    static_cast<void>(loop.run());

    ASSERT_FALSE(toy.solver.relaxation_calls.empty());
    EXPECT_LE(toy.solver.relaxation_calls[0].timeout_s, 100.0);
    EXPECT_GT(toy.solver.relaxation_calls[0].timeout_s, 0.0);
    EXPECT_EQ(toy.solver.relaxation_calls[0].threads, 2);
    EXPECT_LE(toy.pricer.requests[0].timeout_s, 100.0);
}

TEST(ColumnGenerationLoop, TimersCanBeSwitchedOff) {
    CoverOneRow toy;
    ColumnGenerationParams params;
    params.statistics.timers = false;
    ColumnGeneration loop(toy.master, params);
    const ColumnGenerationResult result = loop.run();
    EXPECT_EQ(result.status, ColumnGenerationStatus::Optimal);
    EXPECT_EQ(result.statistics.relaxation_seconds, 0.0);
    EXPECT_EQ(result.statistics.pricing_seconds, 0.0);
}

TEST(ColumnGenerationLoop, DefaultComponentsAndACustomInitialization) {
    CoverOneRow defaults;
    const ColumnGeneration default_loop(defaults.master);
    EXPECT_NE(dynamic_cast<const AbsoluteTolerances*>(default_loop.params().tolerances.get()),
              nullptr);
    EXPECT_NE(dynamic_cast<ArtificialColumns*>(default_loop.params().initialization.get()),
              nullptr);

    // A custom initialization that brings the starting columns instead of artificial ones.
    static constexpr double starting_cost = 10.0;
    class StartingColumns final : public MasterInitialization {
        public:
            void initialize(MasterProblem& master, const ToleranceContext& /*context*/) override {
                master.add_columns(
                    0,
                    {make_path(starting_cost, {{.index = 0, .coefficient = 1.0}}, 0.0)});
            }
    };
    ScriptedMasterSolver solver;
    MasterProblem master(solver);
    master.add_rows(1, RowSense::GreaterEqual, 1.0);
    ScriptedPricer pricer;
    master.add_pricer(pricer);
    solver.on_solve = [](ScriptedMasterSolver& lp) {
        lp.objective_value = starting_cost;
        lp.values = {{0, 1.0}};
        return SolverStatus::Optimal;
    };
    ColumnGenerationParams params;
    params.initialization = std::make_shared<StartingColumns>();
    params.tolerances = std::make_shared<RelativeTolerances>();
    ColumnGeneration loop(master, params);
    const ColumnGenerationResult result = loop.run();

    EXPECT_EQ(result.status, ColumnGenerationStatus::Optimal);
    EXPECT_DOUBLE_EQ(result.objective, starting_cost);
    EXPECT_EQ(result.statistics.artificial_columns, 0U);
    EXPECT_TRUE(master.artificial_column_ids().empty());
}

TEST(ColumnGenerationLoop, SecondRunDoesNotInitializeAgain) {
    CoverOneRow toy;
    ColumnGeneration loop(toy.master);
    static_cast<void>(loop.run());
    const ColumnGenerationResult again = loop.run();
    EXPECT_EQ(again.status, ColumnGenerationStatus::Optimal);
    EXPECT_EQ(again.iterations, 1U);
    EXPECT_EQ(again.statistics.artificial_columns, 1U);
    EXPECT_EQ(toy.master.number_of_columns(), 2U);
}

TEST(ColumnGenerationLoop, TheUserModelIsRestoredHoweverTheRunEnds) {
    CoverOneRow converged;
    ColumnGeneration loop(converged.master);
    static_cast<void>(loop.run());
    EXPECT_EQ(converged.solver.restore_calls, 1U);

    CoverOneRow failing;
    failing.pricer.respond = [](std::span<const double> /*duals*/,
                                const PricingRequest& /*request*/) -> PricingResult {
        throw std::runtime_error("a pricer failed");
    };
    ColumnGeneration failing_loop(failing.master);
    EXPECT_THROW(static_cast<void>(failing_loop.run()), std::runtime_error);
    EXPECT_EQ(failing.solver.restore_calls, 1U);
}

TEST(ColumnGenerationLoop, ASecondLoopDoesNotInitializeTheMasterAgain) {
    CoverOneRow toy;
    ColumnGeneration first(toy.master);
    static_cast<void>(first.run());
    EXPECT_TRUE(toy.master.initialized());

    ColumnGenerationParams longer;
    longer.max_iterations = 100;
    ColumnGeneration second(toy.master, longer);
    const ColumnGenerationResult again = second.run();
    EXPECT_EQ(again.status, ColumnGenerationStatus::Optimal);
    EXPECT_EQ(again.statistics.artificial_columns, 1U);
    EXPECT_EQ(toy.master.number_of_columns(), 2U);
}

TEST(ColumnGenerationLoop, CostScaleFromPricersAndStartingColumns) {
    ScriptedMasterSolver solver;
    MasterProblem master(solver);
    master.add_rows(1, RowSense::GreaterEqual, 1.0);
    constexpr double small_scale = 0.5;
    constexpr double large_scale = 5.0;
    ScaledPricer small_costs(small_scale);
    ScaledPricer large_costs(large_scale);
    master.add_pricer(small_costs);
    master.add_pricer(large_costs);

    ColumnGeneration from_pricers(master);
    static_cast<void>(from_pricers.run());
    EXPECT_DOUBLE_EQ(from_pricers.tolerance_context().cost_scale, large_scale);

    ScriptedMasterSolver other_solver;
    MasterProblem with_columns(other_solver);
    with_columns.add_rows(1, RowSense::GreaterEqual, 1.0);
    ScaledPricer unit(small_scale);
    with_columns.add_pricer(unit);
    constexpr double starting_cost = -20.0;  // its magnitude is the largest cost
    with_columns.add_columns(0, {make_path(starting_cost, {{.index = 0, .coefficient = 1.0}})});
    ColumnGeneration from_columns(with_columns);
    static_cast<void>(from_columns.run());
    EXPECT_DOUBLE_EQ(from_columns.tolerance_context().cost_scale, -starting_cost);
    EXPECT_EQ(unit.updates, 0);

    // Starting columns the user built in their own model count too.
    constexpr double user_cost = 30.0;
    ScriptedMasterSolver user_solver;
    user_solver.largest_user_cost_value = user_cost;
    MasterProblem user_model(user_solver);
    user_model.add_rows(1, RowSense::GreaterEqual, 1.0);
    ScaledPricer user_pricer(small_scale);
    user_model.add_pricer(user_pricer);
    ColumnGeneration from_user_model(user_model);
    static_cast<void>(from_user_model.run());
    EXPECT_DOUBLE_EQ(from_user_model.tolerance_context().cost_scale, user_cost);
    EXPECT_DOUBLE_EQ(user_model.artificial_cost(),
                     ArtificialColumns::automatic_cost_factor * user_cost);
}

TEST(ColumnGenerationLoop, RestrictedMasterIntegerSolve) {
    CoverOneRow toy;
    ColumnGeneration loop(toy.master);
    static_cast<void>(loop.run());
    IntegerParams params;
    params.column_type = ColumnType::Binary;
    EXPECT_EQ(loop.solve_restricted_master_integer(params), SolverStatus::Optimal);
    ASSERT_EQ(toy.solver.integer_calls.size(), 1U);
    EXPECT_EQ(toy.solver.integer_calls[0].column_type, ColumnType::Binary);
}
