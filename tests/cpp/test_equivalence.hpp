// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Does the bidirectional algorithm produce the same answers as the forward one?
//
// - Best costs are compared within a tolerance: a joined path sums two chains in a different order.
// - Every returned path is replayed arc by arc to check it is contiguous and feasible.
// - Small instances are also checked against a brute-force enumerator.
//
// Full solution sets are not compared: a path can be lost when its backward half is dominated at
// the crossing node, though a path at least as good is kept. The sweep runs both with an infinite
// and with a finite upper bound.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/equivalence_helpers.hpp"
#include "util/random_instance.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

// ============================================================================
// The equivalence sweep
// ============================================================================

/// @brief Bidirectional matches the forward search on every generated instance, bound on and off.
TEST(Equivalence, MatchesTheForwardSearchAcrossTheSweep) {
    namespace eq = equivalence_test;

    size_t configs_that_join = 0;
    for (const auto& config : eq::sweep()) {
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const eq::Run reference = eq::solve_forward(config);
        const eq::Run bounded = eq::solve_bidirectional(config, /*with_bound=*/true);
        const eq::Run unbounded = eq::solve_bidirectional(config, /*with_bound=*/false);
        if (bounded.bounded && bounded.result.number_of_joined_paths > 0) {
            ++configs_that_join;
        }

        EXPECT_EQ(reference.result.solutions.empty(), bounded.result.solutions.empty())
            << "one found solutions and the other did not: " << where;
        EXPECT_EQ(reference.result.solutions.empty(), unbounded.result.solutions.empty()) << where;

        if (!reference.result.solutions.empty()) {
            EXPECT_NEAR(bounded.best_cost(), reference.best_cost(), eq::kTolerance)
                << "bound enabled: " << where;
            EXPECT_NEAR(unbounded.best_cost(), reference.best_cost(), eq::kTolerance)
                << "bound disabled: " << where;
        }

        EXPECT_EQ(eq::any_path_problem(bounded), "") << "bound enabled: " << where;
        EXPECT_EQ(eq::any_path_problem(unbounded), "") << "bound disabled: " << where;
    }
    // Without a path crossing H at an interior node, the sweep would pass with the join removed.
    EXPECT_GT(configs_that_join, 0U) << "no config's bounded run produced a joined path";
}

/// @brief The half-way bound never changes the answer.
///
/// Separate from the forward comparison so a failure points at the half-way policy, not the join.
TEST(Equivalence, TheHalfWayBoundDoesNotChangeTheAnswer) {
    namespace eq = equivalence_test;

    size_t instances_with_the_bound_in_force = 0;
    for (const auto& config : eq::sweep()) {
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const eq::Run bounded = eq::solve_bidirectional(config, /*with_bound=*/true);
        const eq::Run unbounded = eq::solve_bidirectional(config, /*with_bound=*/false);

        EXPECT_NEAR(bounded.best_cost(), unbounded.best_cost(), eq::kTolerance) << where;
        EXPECT_FALSE(unbounded.bounded) << "half_way_point = 0 must leave the bound off: " << where;
        if (bounded.bounded) {
            ++instances_with_the_bound_in_force;
        }
    }

    // Otherwise the "bound enabled" assertions above would be vacuous.
    EXPECT_GT(instances_with_the_bound_in_force, 0U)
        << "no instance in the sweep actually ran with the bound enabled";
}

/// @brief On cost-only instances there is no clock, so the bound stays off.
TEST(Equivalence, ACostOnlyInstanceRunsWithTheBoundOff) {
    namespace eq = equivalence_test;
    const test_util::InstanceConfig config{.num_nodes = 10, .density = 0.4, .seed = 101};

    const eq::Run bounded = eq::solve_bidirectional(config, /*with_bound=*/true);
    EXPECT_FALSE(bounded.bounded) << "an accumulating cost slot is not a clock";
    EXPECT_NEAR(bounded.best_cost(), eq::solve_forward(config).best_cost(), eq::kTolerance);
}

// ============================================================================
// The sweep again, with a finite upper bound
// ============================================================================

/// @brief Under a bound that still admits the optimum, both searches find it and nothing above it.
///
/// Also checks that no returned solution costs at or above the bound.
TEST(Equivalence, MatchesTheForwardSearchUnderAnAdmittingUpperBound) {
    namespace eq = equivalence_test;

    size_t instances_compared = 0;
    for (const auto& config : eq::sweep()) {
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const eq::Run unbounded = eq::solve_forward(config);
        if (unbounded.result.solutions.empty()) {
            continue;  // nothing to bound
        }
        const double optimum = unbounded.best_cost();

        eq::BoundedOptions options;
        options.upper_bound = optimum + 1.0;  // strictly above, so the optimum still qualifies

        const eq::Run forward = eq::solve_forward_bounded(config, options);
        const eq::Run bidirectional = eq::solve_bidirectional_bounded(config, options);

        ASSERT_FALSE(forward.result.solutions.empty()) << where;
        ASSERT_FALSE(bidirectional.result.solutions.empty()) << where;
        EXPECT_NEAR(forward.best_cost(), optimum, eq::kTolerance) << where;
        EXPECT_NEAR(bidirectional.best_cost(), optimum, eq::kTolerance) << where;
        EXPECT_EQ(eq::any_path_problem(bidirectional), "") << where;

        for (const auto& solution : bidirectional.result.solutions) {
            EXPECT_LT(solution.cost, options.upper_bound)
                << "a solution at or above the caller's bound was returned: " << where;
        }
        ++instances_compared;
    }

    EXPECT_GT(instances_compared, 0U) << "every instance in the sweep was infeasible";
}

/// @brief A bound below the optimum returns nothing from both searches, and raises from neither.
///
/// With `preprocess = true` such a bound removes every arc, as in the last iteration of a pricing
/// loop; setup validation must still cope with an arc-less graph.
TEST(Equivalence, ABoundBelowTheOptimumReturnsNothingFromBothSearches) {
    namespace eq = equivalence_test;

    for (const auto& config : eq::sweep()) {
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const eq::Run unbounded = eq::solve_forward(config);
        if (unbounded.result.solutions.empty()) {
            continue;
        }

        eq::BoundedOptions options;
        options.upper_bound = unbounded.best_cost() - 1.0;  // nothing can meet it
        options.preprocess = true;

        const eq::Run forward = eq::solve_forward_bounded(config, options);
        EXPECT_TRUE(forward.result.solutions.empty()) << where;

        eq::Run bidirectional;
        EXPECT_NO_THROW({ bidirectional = eq::solve_bidirectional_bounded(config, options); })
            << "an unsatisfiable bound is not a modelling error: " << where;
        EXPECT_TRUE(bidirectional.result.solutions.empty()) << where;
    }
}

/// @brief With `prune_based_on_upper_bound_` on, the completion bound still keeps the optimum.
///
/// Runs with `arc_cost_offset` set, so a bound wrongly built from `arc.cost` over-estimates and
/// loses the optimum. The reference is a forward search with pruning off.
TEST(Equivalence, PruningOnTheCompletionBoundKeepsTheOptimum) {
    namespace eq = equivalence_test;

    size_t instances_compared = 0;
    for (auto config : eq::sweep()) {
        // Positive: a bound summed from arc.cost would over-estimate.
        config.arc_cost_offset = 50.0;
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const eq::Run reference = eq::solve_forward(config);
        if (reference.result.solutions.empty()) {
            continue;
        }

        eq::BoundedOptions options;
        options.prune_based_on_upper_bound = true;  // upper_bound stays +infinity

        const eq::Run bidirectional = eq::solve_bidirectional_bounded(config, options);

        ASSERT_FALSE(bidirectional.result.solutions.empty())
            << "the completion bound pruned every path: " << where;
        EXPECT_NEAR(bidirectional.best_cost(), reference.best_cost(), eq::kTolerance)
            << "the completion bound was not admissible: " << where;
        EXPECT_EQ(eq::any_path_problem(bidirectional), "") << where;
        ++instances_compared;
    }

    EXPECT_GT(instances_compared, 0U);
}

// ============================================================================
// The independent oracle
// ============================================================================

/// @brief Both algorithms match an enumerator that shares no code with either.
///
/// Catches dominance errors common to both algorithms. Small instances only: enumeration is
/// exponential.
TEST(Equivalence, BothMatchBruteForceOnSmallInstances) {
    namespace eq = equivalence_test;

    const std::vector<test_util::InstanceConfig> configs{
        {.num_nodes = 8, .density = 0.4, .seed = 21},
        {.num_nodes = 9, .density = 0.3, .with_time_window = true, .window_slack = 0.3, .seed = 22},
        {.num_nodes = 9, .density = 0.3, .with_capacity = true, .capacity_binds = true, .seed = 23},
        {.num_nodes = 10, .density = 0.25, .mixed_sign_costs = true, .seed = 24},
        {.num_nodes = 10,
         .density = 0.25,
         .with_time_window = true,
         .window_slack = 0.4,
         .with_capacity = true,
         .capacity_binds = true,
         .num_sinks = 2,
         .seed = 25},
    };

    for (const auto& config : configs) {
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const double expected = test_util::brute_force_optimum(config);
        EXPECT_NEAR(eq::solve_forward(config).best_cost(), expected, eq::kTolerance)
            << "the forward search disagrees with enumeration: " << where;
        EXPECT_NEAR(eq::solve_bidirectional(config, /*with_bound=*/true).best_cost(),
                    expected,
                    eq::kTolerance)
            << "bidirectional disagrees with enumeration: " << where;
    }
}

/// @brief The solution sets agree on the optimum, and bidirectional never invents a path.
///
/// Full set equality does not hold (a backward half can be dominated at the crossing node), but
/// every returned path must be valid and the optimum must be among them.
TEST(Equivalence, SolutionSetsAgreeOnTheOptimumNotOnEveryPath) {
    namespace eq = equivalence_test;
    const test_util::InstanceConfig config{.num_nodes = 12,
                                           .density = 0.5,
                                           .with_time_window = true,
                                           .window_slack = 0.4,
                                           .seed = 31};
    const std::string where = test_util::describe(config);

    const eq::Run reference = eq::solve_forward(config);
    const eq::Run candidate = eq::solve_bidirectional(config, /*with_bound=*/true);
    ASSERT_FALSE(reference.result.solutions.empty()) << where;
    ASSERT_FALSE(candidate.result.solutions.empty()) << where;

    EXPECT_NEAR(candidate.best_cost(), reference.best_cost(), eq::kTolerance) << where;
    EXPECT_EQ(eq::any_path_problem(candidate), "") << where;

    // Set sizes may differ; at least one optimal path must be returned.
    const size_t optimal_paths =
        static_cast<size_t>(std::ranges::count_if(candidate.result.solutions, [&](const auto& s) {
            return std::abs(s.cost - candidate.best_cost()) < eq::kTolerance;
        }));
    EXPECT_GT(optimal_paths, 0U) << where;
}
