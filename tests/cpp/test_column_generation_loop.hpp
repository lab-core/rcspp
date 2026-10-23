// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The column generation loop: statuses, hooks, limits, statistics and default components.

#include <gtest/gtest.h>

#include <memory>
#include <span>
#include <vector>

#include "rcspp/column_generation/column_generation.hpp"
#include "test_column_generation_fakes.hpp"

using namespace rcspp;                              // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation;           // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation::testing;  // NOLINT(google-build-using-namespace)

namespace rcspp::column_generation::testing {

// The smallest master with a real choice: one row "cover ≥ 1"; the pricer offers one path of
// cost 10 covering it while its dual is above 10. With only the artificial column, the LP pays
// the artificial cost; once the path is in, it costs 10.
struct CoverOneRow {
        CoverOneRow() {
            master.add_rows(1, RowSense::GreaterEqual, 1.0);
            master.add_pricer(pricer);
            backend.on_solve = [](ScriptedBackend& lp) {
                const bool has_path = lp.columns.size() > 1;
                const double artificial_cost = lp.columns.front().cost;
                lp.objective_value = has_path ? 10.0 : artificial_cost;
                lp.dual_values = {lp.objective_value};
                lp.values = {{0, has_path ? 0.0 : 1.0}, {1, 1.0}};
                return SolverStatus::Optimal;
            };
            pricer.respond = [](std::span<const double> duals, const PricingRequest& request) {
                PricingResult result = ScriptedPricer::proven_none();
                const double reduced_cost = 10.0 - duals[0];
                if (reduced_cost < request.reduced_cost_threshold) {
                    result.columns.push_back(make_path(10.0, {{0, 1.0}}, reduced_cost));
                }
                return result;
            };
        }

        ScriptedBackend backend;
        MasterProblem master{backend};
        ScriptedPricer pricer;
};

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
    EXPECT_GE(result.statistics.total_seconds, 0.0);
    EXPECT_EQ(toy.backend.columns[0].cost, ArtificialColumns::automatic_cost_factor);
    // No convexity row: the threshold is minus the reduced-cost tolerance.
    ASSERT_EQ(toy.pricer.requests.size(), 2U);
    EXPECT_DOUBLE_EQ(toy.pricer.requests[0].reduced_cost_threshold, -1e-9);
}

TEST(ColumnGenerationLoop, ThresholdIncludesTheConvexityDual) {
    ScriptedBackend backend;
    MasterProblem master(backend);
    master.add_rows(1, RowSense::GreaterEqual, 1.0);
    ScriptedPricer pricer;
    master.add_pricer(pricer, ConvexityRow{.max_paths = 4.0});  // row 1
    backend.on_solve = [](ScriptedBackend& lp) {
        lp.dual_values = {12.0, 3.0};
        return SolverStatus::Optimal;
    };
    ColumnGeneration loop(master);
    static_cast<void>(loop.run());

    ASSERT_EQ(pricer.requests.size(), 1U);
    EXPECT_DOUBLE_EQ(pricer.requests[0].reduced_cost_threshold, 3.0 - 1e-9);
    EXPECT_EQ(pricer.seen_duals[0], (std::vector<double>{12.0, 3.0}));
}

TEST(ColumnGenerationLoop, NotProvenWithoutAnExactOptimalPricing) {
    for (const PricingResult& answer : {PricingResult{{}, PricingStatus::SolverLimit, 0.0, true},
                                        PricingResult{{}, PricingStatus::Optimal, 0.0, false}}) {
        ScriptedBackend backend;
        MasterProblem master(backend);
        ScriptedPricer pricer([answer](auto, auto) { return answer; });
        master.add_pricer(pricer);
        ColumnGeneration loop(master);
        const ColumnGenerationResult result = loop.run();
        EXPECT_EQ(result.status, ColumnGenerationStatus::NotProven);
        EXPECT_EQ(result.lower_bound, -infinity);
    }
}

TEST(ColumnGenerationLoop, InfeasibleWhenAnArtificialColumnStays) {
    ScriptedBackend backend;
    MasterProblem master(backend);
    master.add_rows(1, RowSense::GreaterEqual, 1.0);
    ScriptedPricer pricer;  // never finds a column
    master.add_pricer(pricer);
    backend.on_solve = [](ScriptedBackend& lp) {
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
    bool path_allowed = false;
    toy.pricer.respond = [&path_allowed](std::span<const double> duals,
                                         const PricingRequest& request) {
        PricingResult result = ScriptedPricer::proven_none();
        const double reduced_cost = 10.0 - duals[0];
        if (path_allowed && reduced_cost < request.reduced_cost_threshold) {
            result.columns.push_back(make_path(10.0, {{0, 1.0}}, reduced_cost));
        }
        return result;
    };

    std::vector<ConvergenceContext> contexts;
    std::vector<size_t> value_counts;
    ColumnGenerationParams params;
    params.on_converged = [&](ConvergenceContext& context) {
        contexts.push_back(context);
        value_counts.push_back(context.values.size());
        if (!path_allowed) {
            path_allowed = true;  // e.g. a pricer changed its problem
            context.master.pricer(0).update(PricingChanges{.structure = true});
            return ConvergenceAction::Continue;
        }
        return ConvergenceAction::Stop;
    };
    ColumnGeneration loop(toy.master, params);
    const ColumnGenerationResult result = loop.run();

    ASSERT_EQ(contexts.size(), 2U);
    EXPECT_EQ(contexts[0].iteration, 1U);
    EXPECT_EQ(contexts[0].objective, ArtificialColumns::automatic_cost_factor);
    EXPECT_EQ(contexts[0].duals.size(), 1U);
    EXPECT_EQ(value_counts, (std::vector<size_t>{1, 2}));
    EXPECT_EQ(contexts[1].iteration, 3U);
    EXPECT_EQ(result.status, ColumnGenerationStatus::Optimal);
    EXPECT_DOUBLE_EQ(result.objective, 10.0);
}

TEST(ColumnGenerationLoop, IterationHookSeesEveryIteration) {
    CoverOneRow toy;
    std::vector<IterationInfo> seen;
    ColumnGenerationParams params;
    params.on_iteration = [&seen](const IterationInfo& info) { seen.push_back(info); };
    ColumnGeneration loop(toy.master, params);
    static_cast<void>(loop.run());

    ASSERT_EQ(seen.size(), 2U);
    EXPECT_EQ(seen[0].iteration, 1U);
    EXPECT_EQ(seen[0].new_columns, 1U);
    EXPECT_EQ(seen[0].total_columns, 2U);
    EXPECT_EQ(seen[1].iteration, 2U);
    EXPECT_EQ(seen[1].new_columns, 0U);
    EXPECT_DOUBLE_EQ(seen[1].objective, 10.0);
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
    EXPECT_TRUE(untouched.backend.relaxation_calls.empty());
}

TEST(ColumnGenerationLoop, TimeLimitAndSolverErrors) {
    CoverOneRow no_time;
    ColumnGenerationParams params;
    params.timeout_s = 0.0;
    ColumnGeneration out_of_time(no_time.master, params);
    EXPECT_EQ(out_of_time.run().status, ColumnGenerationStatus::TimeLimit);
    EXPECT_TRUE(no_time.backend.relaxation_calls.empty());

    CoverOneRow solver_time;
    solver_time.backend.on_solve = [](ScriptedBackend&) { return SolverStatus::TimeLimit; };
    ColumnGeneration solver_out_of_time(solver_time.master);
    EXPECT_EQ(solver_out_of_time.run().status, ColumnGenerationStatus::TimeLimit);

    CoverOneRow solver_failure;
    solver_failure.backend.on_solve = [](ScriptedBackend&) { return SolverStatus::Infeasible; };
    ColumnGeneration failing(solver_failure.master);
    const ColumnGenerationResult result = failing.run();
    EXPECT_EQ(result.status, ColumnGenerationStatus::SolverError);
    EXPECT_EQ(result.iterations, 0U);
}

TEST(ColumnGenerationLoop, TimeLimitIsPassedOn) {
    CoverOneRow toy;
    ColumnGenerationParams params;
    params.timeout_s = 100.0;
    params.relaxation.threads = 2;
    ColumnGeneration loop(toy.master, params);
    static_cast<void>(loop.run());

    ASSERT_FALSE(toy.backend.relaxation_calls.empty());
    EXPECT_LE(toy.backend.relaxation_calls[0].timeout_s, 100.0);
    EXPECT_GT(toy.backend.relaxation_calls[0].timeout_s, 0.0);
    EXPECT_EQ(toy.backend.relaxation_calls[0].threads, 2);
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
    class StartingColumns final : public MasterInitialization {
        public:
            void initialize(MasterProblem& master, const ToleranceContext& /*context*/) override {
                master.add_columns(0, {make_path(10.0, {{0, 1.0}}, 0.0)});
            }
    };
    ScriptedBackend backend;
    MasterProblem master(backend);
    master.add_rows(1, RowSense::GreaterEqual, 1.0);
    ScriptedPricer pricer;
    master.add_pricer(pricer);
    backend.on_solve = [](ScriptedBackend& lp) {
        lp.objective_value = 10.0;
        lp.values = {{0, 1.0}};
        return SolverStatus::Optimal;
    };
    ColumnGenerationParams params;
    params.initialization = std::make_shared<StartingColumns>();
    params.tolerances = std::make_shared<RelativeTolerances>();
    ColumnGeneration loop(master, params);
    const ColumnGenerationResult result = loop.run();

    EXPECT_EQ(result.status, ColumnGenerationStatus::Optimal);
    EXPECT_DOUBLE_EQ(result.objective, 10.0);
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
    EXPECT_EQ(toy.master.num_columns(), 2U);
}

TEST(ColumnGenerationLoop, CostScaleFromPricersAndStartingColumns) {
    ScriptedBackend backend;
    MasterProblem master(backend);
    master.add_rows(1, RowSense::GreaterEqual, 1.0);
    ScaledPricer small(0.5);
    ScaledPricer large(5.0);
    master.add_pricer(small);
    master.add_pricer(large);

    ColumnGeneration from_pricers(master);
    static_cast<void>(from_pricers.run());
    EXPECT_DOUBLE_EQ(from_pricers.tolerance_context().cost_scale, 5.0);

    ScriptedBackend other_backend;
    MasterProblem with_columns(other_backend);
    with_columns.add_rows(1, RowSense::GreaterEqual, 1.0);
    ScaledPricer unit(0.5);
    with_columns.add_pricer(unit);
    with_columns.add_columns(0, {make_path(-20.0, {{0, 1.0}})});
    ColumnGeneration from_columns(with_columns);
    static_cast<void>(from_columns.run());
    EXPECT_DOUBLE_EQ(from_columns.tolerance_context().cost_scale, 20.0);
    EXPECT_EQ(unit.updates, 0);
}

TEST(ColumnGenerationLoop, RestrictedMasterIntegerSolve) {
    CoverOneRow toy;
    ColumnGeneration loop(toy.master);
    static_cast<void>(loop.run());
    IntegerParams params;
    params.column_type = ColumnType::Binary;
    EXPECT_EQ(loop.solve_restricted_master_integer(params), SolverStatus::Optimal);
    ASSERT_EQ(toy.backend.integer_calls.size(), 1U);
    EXPECT_EQ(toy.backend.integer_calls[0].column_type, ColumnType::Binary);
}
