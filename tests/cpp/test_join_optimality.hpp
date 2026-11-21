// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Can the join lose an optimum?
//
// The join pairs forward boundary labels (stored but not extended past `H`) with backward labels
// at the same node; boundary labels are dominance-filtered. These tests compare against brute
// force and the forward search over the generator's full cross product and several values of `H`.
// This file covers the single-slot pack; the ng model is in `test_join_optimality_ng.hpp`.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/bidirectional_test_util.hpp"
#include "util/join_optimality.hpp"
#include "util/random_instance.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

// ============================================================================
// Optimality
// ============================================================================

/// @brief Brute force, forward and bidirectional agree across the cross product and every `H`.
TEST(JoinOptimality, TheJoinMatchesBruteForceAcrossTheCrossProduct) {
    namespace jo = join_optimality;

    size_t instances = 0;
    size_t with_solutions = 0;
    for (const auto& config : jo::cross_product(/*num_seeds=*/6, /*num_nodes=*/8)) {
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const double truth = test_util::brute_force_optimum(config);
        EXPECT_NEAR(jo::forward_optimum(config), truth, jo::kTolerance)
            << "the forward search disagrees with brute force, so nothing below means anything";

        for (const double factor : jo::half_way_factors()) {
            SCOPED_TRACE("H factor " + std::to_string(factor));
            ++instances;

            EXPECT_NEAR(jo::solve_with(config, factor).cost, truth, jo::kTolerance)
                << "the join lost the optimum: " << where;
            if (std::isfinite(truth)) {
                ++with_solutions;
            }
        }
    }
    std::cout << "[ SWEEP ] " << instances << " (instance, H) pairs, " << with_solutions
              << " with a finite optimum" << std::endl;
    EXPECT_GT(with_solutions, 0U) << "every instance was infeasible, so this asserted nothing";
}

/// @brief On instances too large to enumerate, the join still matches the forward search.
///
/// Larger instances are where dominance actually discards labels, which is what the join relies on.
TEST(JoinOptimality, TheJoinMatchesTheForwardSearchOnLargerInstances) {
    namespace jo = join_optimality;

    size_t compared = 0;
    for (const size_t num_nodes : {14U, 20U, 28U}) {
        for (const auto& config : jo::cross_product(/*num_seeds=*/3, num_nodes)) {
            const std::string where = test_util::describe(config);
            SCOPED_TRACE(where);

            const double reference = jo::forward_optimum(config);
            for (const double factor : jo::half_way_factors()) {
                SCOPED_TRACE("H factor " + std::to_string(factor));
                EXPECT_NEAR(jo::solve_with(config, factor).cost, reference, jo::kTolerance)
                    << "the join disagrees with the forward search: " << where;
                ++compared;
            }
        }
    }
    std::cout << "[ SWEEP ] " << compared << " (instance, H) pairs against the forward search"
              << std::endl;
    EXPECT_GT(compared, 0U);
}

/// @brief The same comparisons over many more seeds and sizes; disabled because it is slow.
///
/// Run with `--gtest_also_run_disabled_tests`.
TEST(JoinOptimality, DISABLED_DeepOptimalitySoak) {
    namespace jo = join_optimality;

    size_t against_brute_force = 0;
    for (const auto& config : jo::cross_product(/*num_seeds=*/40, /*num_nodes=*/8)) {
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);
        const double truth = test_util::brute_force_optimum(config);
        for (const double factor : jo::half_way_factors()) {
            SCOPED_TRACE("H factor " + std::to_string(factor));
            EXPECT_NEAR(jo::solve_with(config, factor).cost, truth, jo::kTolerance);
            ++against_brute_force;
        }
    }

    size_t against_forward = 0;
    for (const size_t num_nodes : {12U, 18U, 24U, 32U}) {
        for (const auto& config : jo::cross_product(/*num_seeds=*/12, num_nodes)) {
            const std::string where = test_util::describe(config);
            SCOPED_TRACE(where);
            const double reference = jo::forward_optimum(config);
            for (const double factor : jo::half_way_factors()) {
                SCOPED_TRACE("H factor " + std::to_string(factor));
                EXPECT_NEAR(jo::solve_with(config, factor).cost, reference, jo::kTolerance);
                ++against_forward;
            }
        }
    }

    std::cout << "[ SOAK ] " << against_brute_force << " (instance, H) pairs against brute force, "
              << against_forward << " against the forward search" << std::endl;
}

// ============================================================================
// The oracle itself
// ============================================================================

/// @brief The oracle ends a path at its first sink, matching the algorithms.
///
/// This instance (two sinks, mixed-sign costs) has a cheaper walk that passes through a sink.
TEST(JoinOptimality, TheOracleEndsAPathAtItsFirstSink) {
    namespace jo = join_optimality;

    test_util::InstanceConfig config;
    config.num_nodes = 8;
    config.density = 0.75;
    config.seed = 2;
    config.with_time_window = true;
    config.window_slack = 0.6;
    config.with_capacity = true;
    config.capacity_binds = true;
    config.mixed_sign_costs = true;
    config.num_sinks = 2;

    const double ends_at_first = test_util::brute_force_optimum(config);
    const double walks_through =
        test_util::brute_force_optimum(config, /*allow_interior_sinks=*/true);

    EXPECT_LT(walks_through, ends_at_first)
        << "the permissive enumeration no longer finds a cheaper walk on the one instance chosen "
           "to exhibit one, so this test has stopped distinguishing the two models";
    EXPECT_NEAR(jo::forward_optimum(config), ends_at_first, jo::kTolerance)
        << "the forward search and the oracle disagree about what a path is";
    for (const double factor : jo::half_way_factors()) {
        SCOPED_TRACE("H factor " + std::to_string(factor));
        EXPECT_NEAR(jo::solve_with(config, factor).cost, ends_at_first, jo::kTolerance);
    }
}
