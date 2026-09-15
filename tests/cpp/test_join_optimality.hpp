// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The join, checked at length against independent references: can it lose an optimum?
//
// The join pairs a forward *boundary* label -- one the search stored and then declined to extend,
// because its clock is past `H` -- with a backward label at the same node. A boundary label had to
// survive dominance where it sits, so the join considers strictly fewer forward halves than an
// implementation that rebuilt the crossing label across each arc would. "A dominated prefix
// completes no better than the prefix that dominated it, so the optimum cannot be among the ones
// dropped" is an argument; this file is the evidence.
//
// Two things are asserted here that `test_equivalence.hpp`'s sweep does not cover:
//
//  1. **An independent oracle.** `brute_force_optimum` enumerates every simple path and shares no
//     code with the labelling algorithms, so it catches an error the forward and bidirectional
//     searches could make together.
//  2. **Breadth.** The equivalence sweep is eight hand-chosen configurations, picked so a failure
//     names a dimension. Here it is a cross product over every knob the generator has, times
//     several seeds, times several positions of `H` -- because `H` is what decides which labels
//     become boundary labels, and it is therefore the dimension the join is most sensitive to.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/random_instance.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace join_optimality {

using Composed = ResourceTypeComposition<RealResource>;

constexpr double kTolerance = 1e-9;

/// @brief One solve's outcome.
struct Outcome {
        double cost = std::numeric_limits<double>::infinity();
        size_t solutions = 0;
        size_t joined_paths = 0;
        bool bounded = false;
};

/// @brief Solves one instance bidirectionally with `H` at a given fraction of the clock's range.
///
/// @param config   The instance to build.
/// @param h_factor `H` as a fraction of the clock's upper bound; 0 leaves the bound disabled.
/// @return What the solve returned.
inline Outcome solve_with(const test_util::InstanceConfig& config, double h_factor) {
    auto built = test_util::build_instance(config);

    AlgorithmParams<LabelList<Composed>> params;
    params.critical_resource_index = built.clock_index;
    params.half_way_point = built.clock_upper_bound * h_factor;

    auto algorithm =
        built.graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    const auto result = built.graph->solve(algorithm.get());

    Outcome outcome;
    outcome.cost = result.solutions.empty() ? std::numeric_limits<double>::infinity()
                                            : result.solutions.front().cost;
    outcome.solutions = result.solutions.size();
    outcome.joined_paths = result.number_of_joined_paths;
    outcome.bounded = result.bounded_by_half_way;
    return outcome;
}

/// @brief The forward reference, for instances too large to enumerate.
inline double forward_optimum(const test_util::InstanceConfig& config) {
    auto built = test_util::build_instance(config);
    const auto result = built.graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    return result.solutions.empty() ? std::numeric_limits<double>::infinity()
                                    : result.solutions.front().cost;
}

/// @brief Every combination of the generator's knobs, at @p num_seeds seeds each.
///
/// A cross product rather than a hand-picked list. The equivalence suite picks its rows so that a
/// failure names a dimension; this one is looking for a case nobody thought of, which is a
/// different job and wants coverage instead of legibility.
///
/// @param num_seeds How many seeds per combination.
/// @param num_nodes Instance size. Small, because `brute_force_optimum` is exponential.
/// @return The configurations.
inline std::vector<test_util::InstanceConfig> cross_product(std::uint32_t num_seeds,
                                                            size_t num_nodes) {
    std::vector<test_util::InstanceConfig> configs;
    for (std::uint32_t seed = 0; seed < num_seeds; ++seed) {
        for (const bool with_time_window : {false, true}) {
            for (const bool with_capacity : {false, true}) {
                for (const bool mixed_sign_costs : {false, true}) {
                    for (const size_t num_sinks : {size_t{1}, size_t{2}}) {
                        for (const double density : {0.3, 0.75}) {
                            test_util::InstanceConfig config;
                            config.num_nodes = num_nodes;
                            config.density = density;
                            config.seed = seed;
                            config.with_time_window = with_time_window;
                            config.window_slack = with_time_window ? 0.6 : 1.0;
                            config.with_capacity = with_capacity;
                            config.capacity_binds = with_capacity;
                            config.mixed_sign_costs = mixed_sign_costs;
                            config.num_sinks = num_sinks;
                            configs.push_back(config);
                        }
                    }
                }
            }
        }
    }
    return configs;
}

/// @brief Where to put `H`, as a fraction of the clock's range.
///
/// `H` decides which labels become boundary labels, so it is the dimension the join is most
/// sensitive to: at 0.25 the forward search stops early and almost every complete path is assembled
/// by the join; at 0.75 it is nearly all found forward and the join does little. 0 is the disabled
/// bound, where the boundary predicate degenerates to "has a predecessor".
inline const std::vector<double>& half_way_factors() {
    static const std::vector<double> factors{0.0, 0.25, 0.5, 0.75};
    return factors;
}

}  // namespace join_optimality

// ============================================================================
// Optimality
// ============================================================================

/// @brief Against an INDEPENDENT oracle, across the generator's whole cross product.
///
/// The headline optimality test. For every instance and every position of `H`, three numbers must
/// agree: brute force, the forward search, and the bidirectional search. Brute force is what makes
/// this more than two implementations agreeing with each other -- it enumerates every simple path
/// and shares no code with the labelling algorithms, so a dominance rule that discards something it
/// should not is still caught.
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
/// Brute force is exponential, so the test above runs at eight nodes. Dominance only starts doing
/// real work above that, and the join's whole exposure is that it reads a dominance-filtered
/// container -- so the sizes where dominance actually discards things are the ones worth covering,
/// even at the cost of a weaker oracle.
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

/// @brief A deep soak: many more seeds and sizes than the suite can afford to run every time.
///
/// The two enabled tests above are sized to keep the suite fast. This is the one to run when the
/// question is "how sure are we", after touching dominance, the half-way policy, the joiner, or any
/// resource's backward form. `DISABLED_` only because of what it costs, not because it is less
/// trustworthy -- it is the same comparison, wider.
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

/// @brief The oracle ends a path at its first sink, and the difference is not hypothetical.
///
/// `brute_force_optimum` used to walk *through* a sink and keep going. D5 made that illegal in the
/// algorithm -- a path may not have a terminal strictly inside it -- and the oracle was not
/// updated, so on the right instance it reported an optimum no search could reach.
///
/// The right instance needs two things at once, which is why this went unnoticed: at least two
/// sinks, so that a sink has outgoing arcs at all, and mixed-sign costs, so that continuing past
/// one is actually attractive. The configuration below is the one the cross-product sweep found.
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
