// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The ng-path tier of the join-optimality sweep: can the boundary-label join lose an optimum on
// the ng model, at any position of `H`?
//
// ng is the hard case because the join tests disjointness of memories that forget, and `H`
// decides which memories get compared. The forward search is not a valid reference on cyclic ng
// instances, so the cyclic tier checks against `ng_cyclic_optimum`, an independent walk oracle.
// Compiled in its own TU because the two-slot pack doubles the engine's instantiations.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/equivalence_helpers.hpp"
#include "util/random_instance.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace join_optimality_ng {

constexpr double kTolerance = 1e-9;

/// @brief One solve's outcome on the ng pack.
struct Outcome {
        double cost = std::numeric_limits<double>::infinity();
        size_t joined_paths = 0;
        bool bounded = false;

        /// @brief What is wrong with any returned path, or "" -- see @c path_problem.
        ///
        /// A join that glues two halves remembering the same node can return a path cheaper
        /// than the optimum; the replay reports it as infeasible rather than "too good".
        std::string problem;
};

/// @brief Solves one ng instance bidirectionally with `H` at a given fraction of the clock's range.
///
/// A finite, non-binding cost bound is passed on purpose: with an infinite bound
/// `Joiner::join` enables its incumbent cutoff and accepts few pairs, whereas a finite bound
/// makes the join consider every pair it can form.
///
/// @param config   The instance to build.
/// @param h_factor `H` as a fraction of the clock's upper bound; 0 leaves the bound disabled.
/// @return What the solve returned, plus the replay verdict on its paths.
inline Outcome solve_with(const test_util::InstanceConfig& config, double h_factor) {
    equivalence_test::NgRun run;
    auto built = test_util::build_ng_instance(config);
    run.graph = std::move(built.graph);

    AlgorithmParams<LabelList<equivalence_test::NgComposed>> params;
    params.critical_resource_index = built.clock_index;
    params.half_way_point = built.clock_upper_bound * h_factor;
    params.release_after_solve = false;

    // Far above any cost this generator can produce, so it drops nothing.
    constexpr double kNonBindingUpperBound = 1e9;

    auto algorithm =
        run.graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    run.result = run.graph->solve(algorithm.get(), kNonBindingUpperBound);
    run.bounded = algorithm->bounded_by_half_way();

    equivalence_test::expect_consistent_ref_counts(algorithm->get_label_pool());

    Outcome outcome;
    outcome.cost = run.best_cost();
    outcome.joined_paths = run.result.number_of_joined_paths;
    outcome.bounded = run.bounded;
    outcome.problem = equivalence_test::any_path_problem(run);
    return outcome;
}

/// @brief The forward reference on the ng pack, for the acyclic tier.
inline double forward_optimum(const test_util::InstanceConfig& config) {
    return equivalence_test::solve_forward_ng(config).best_cost();
}

/// @brief Where to put `H`, as a fraction of the clock's range.
///
/// At 0.25 the join assembles most complete paths, at 0.75 the forward search finds most itself,
/// and 0 disables the bound so every stored non-seed label is paired.
inline const std::vector<double>& half_way_factors() {
    static const std::vector<double> factors{0.0, 0.25, 0.5, 0.75};
    return factors;
}

/// @brief The cyclic ng cross product -- the tier where ng is load-bearing.
///
/// Time windows are required (they bound the cycles), mixed-sign costs are required (otherwise no
/// shortest path revisits a node and ng never binds), and ng is always on. The shape (nine nodes,
/// back-arc density 0.3/0.35) was chosen so ng actually binds; narrowing it can make the tier
/// vacuous, which `TheCyclicSweepIsOneWhereNgActuallyBinds` guards against.
///
/// @param num_seeds How many seeds per combination.
/// @param num_nodes Instance size. Small, because `ng_cyclic_optimum` enumerates walks.
/// @return The configurations.
inline std::vector<test_util::InstanceConfig> cyclic_cross_product(std::uint32_t num_seeds,
                                                                   size_t num_nodes) {
    std::vector<test_util::InstanceConfig> configs;
    for (std::uint32_t seed = 0; seed < num_seeds; ++seed) {
        for (const bool with_capacity : {false, true}) {
            for (const size_t num_sinks : {size_t{1}, size_t{2}}) {
                for (const double back_arcs : {0.3, 0.35}) {
                    test_util::InstanceConfig config;
                    config.num_nodes = num_nodes;
                    config.density = 0.5;
                    config.seed = seed;
                    config.with_time_window = true;
                    config.window_slack = 0.6;
                    config.with_capacity = with_capacity;
                    config.capacity_binds = with_capacity;
                    config.mixed_sign_costs = true;
                    config.num_sinks = num_sinks;
                    config.with_ng_path = true;
                    config.back_arc_density = back_arcs;
                    configs.push_back(config);
                }
            }
        }
    }
    return configs;
}

/// @brief The acyclic ng cross product, where `brute_force_optimum` is available.
///
/// ng cannot reject anything on a DAG, so this checks the join's container disjointness test at
/// every `H` against the strongest oracle available, brute force.
///
/// @param num_seeds How many seeds per combination.
/// @param num_nodes Instance size. Small, because `brute_force_optimum` is exponential.
/// @return The configurations.
inline std::vector<test_util::InstanceConfig> acyclic_cross_product(std::uint32_t num_seeds,
                                                                    size_t num_nodes) {
    std::vector<test_util::InstanceConfig> configs;
    for (std::uint32_t seed = 0; seed < num_seeds; ++seed) {
        for (const bool with_time_window : {false, true}) {
            for (const bool with_capacity : {false, true}) {
                for (const bool mixed_sign_costs : {false, true}) {
                    test_util::InstanceConfig config;
                    config.num_nodes = num_nodes;
                    config.density = 0.6;
                    config.seed = seed;
                    config.with_time_window = with_time_window;
                    config.window_slack = with_time_window ? 0.6 : 1.0;
                    config.with_capacity = with_capacity;
                    config.capacity_binds = with_capacity;
                    config.mixed_sign_costs = mixed_sign_costs;
                    config.with_ng_path = true;
                    configs.push_back(config);
                }
            }
        }
    }
    return configs;
}

}  // namespace join_optimality_ng

// ============================================================================
// The tier that matters: cyclic, against the walk oracle
// ============================================================================

/// @brief On cyclic ng instances the join matches an independent oracle at every position of `H`.
///
/// Every returned path is also replayed, so an infeasible join is reported as such rather than as
/// "better than the oracle".
TEST(JoinOptimalityNg, TheJoinMatchesTheCyclicOracleAcrossTheCrossProduct) {
    namespace jo = join_optimality_ng;

    size_t pairs = 0;
    size_t with_solutions = 0;
    size_t joined = 0;
    for (const auto& config : jo::cyclic_cross_product(/*num_seeds=*/6, /*num_nodes=*/9)) {
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const double truth = test_util::ng_cyclic_optimum(config);

        for (const double factor : jo::half_way_factors()) {
            SCOPED_TRACE("H factor " + std::to_string(factor));
            ++pairs;

            const auto outcome = jo::solve_with(config, factor);
            EXPECT_NEAR(outcome.cost, truth, jo::kTolerance)
                << "the join lost the optimum, or returned a path the model forbids: " << where;
            EXPECT_EQ(outcome.problem, "")
                << "a joined path is not ng-feasible when replayed: " << where;
            if (std::isfinite(truth)) {
                ++with_solutions;
            }
            joined += outcome.joined_paths;
        }
    }
    std::cout << "[ SWEEP ] " << pairs << " cyclic ng (instance, H) pairs, " << with_solutions
              << " with a finite optimum, " << joined << " joined paths" << std::endl;
    EXPECT_GT(with_solutions, 0U) << "every instance was infeasible, so this asserted nothing";
    // Guard against vacuity: if no joined path was ever produced, the join was never exercised.
    EXPECT_GT(joined, 0U) << "the join produced no paths anywhere in the sweep, so this tests the "
                             "two searches rather than the join";
}

/// @brief The same, when some nodes the neighbourhoods mention forbid nothing.
///
/// Both halves of a path may then remember such a node, and the join must let them. Plain
/// disjointness refused those splices and lost the optimum.
TEST(JoinOptimalityNg, TheJoinMatchesTheCyclicOracleWhenSomeNodesMayBeRevisited) {
    namespace jo = join_optimality_ng;

    size_t with_solutions = 0;
    size_t joined = 0;
    for (auto config : jo::cyclic_cross_product(/*num_seeds=*/4, /*num_nodes=*/9)) {
        config.ng_revisitable_share = 0.3;
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const double truth = test_util::ng_cyclic_optimum(config);
        for (const double factor : jo::half_way_factors()) {
            SCOPED_TRACE("H factor " + std::to_string(factor));
            const auto outcome = jo::solve_with(config, factor);
            EXPECT_NEAR(outcome.cost, truth, jo::kTolerance) << where;
            EXPECT_EQ(outcome.problem, "") << where;
            if (std::isfinite(truth)) {
                ++with_solutions;
            }
            joined += outcome.joined_paths;
        }
    }
    EXPECT_GT(with_solutions, 0U) << "every instance was infeasible, so this asserted nothing";
    EXPECT_GT(joined, 0U) << "the join produced no paths anywhere in the sweep";
}

/// @brief The sweep above is not vacuous: ng changes the answer somewhere in it.
///
/// Both sides run the same solve on the same two-slot graph; only the ng forbidden sets differ.
/// Also checks that ng, as a restriction, never makes the optimum better.
TEST(JoinOptimalityNg, TheCyclicSweepIsOneWhereNgActuallyBinds) {
    namespace jo = join_optimality_ng;

    bool bound_somewhere = false;
    for (auto config : jo::cyclic_cross_product(/*num_seeds=*/6, /*num_nodes=*/9)) {
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const auto restricted = jo::solve_with(config, 0.5);
        config.with_ng_path = false;
        const auto unrestricted = jo::solve_with(config, 0.5);

        if (!std::isfinite(restricted.cost) || !std::isfinite(unrestricted.cost)) {
            continue;
        }
        EXPECT_GE(restricted.cost, unrestricted.cost - jo::kTolerance)
            << "ng made the optimum BETTER, which a restriction cannot do: " << where;
        if (restricted.cost > unrestricted.cost + jo::kTolerance) {
            bound_somewhere = true;
        }
    }
    EXPECT_TRUE(bound_somewhere)
        << "the ng component never changed the answer anywhere in the cyclic cross product, so "
           "TheJoinMatchesTheCyclicOracleAcrossTheCrossProduct is not testing ng semantics. Widen "
           "back_arc_density, or narrow the neighbourhood window in test_util::ng_neighborhoods.";
}

// ============================================================================
// Acyclic, against brute force
// ============================================================================

/// @brief On acyclic ng instances the join matches brute force at every position of `H`.
///
/// ng is inert here, so this checks a label with a container component against brute force.
TEST(JoinOptimalityNg, TheJoinMatchesBruteForceOnAcyclicNgInstances) {
    namespace jo = join_optimality_ng;

    size_t pairs = 0;
    size_t with_solutions = 0;
    size_t joined = 0;
    for (const auto& config : jo::acyclic_cross_product(/*num_seeds=*/3, /*num_nodes=*/8)) {
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const double truth = test_util::brute_force_optimum(config);
        EXPECT_NEAR(jo::forward_optimum(config), truth, jo::kTolerance)
            << "the forward search disagrees with brute force on the ng pack, so nothing below "
               "means anything: "
            << where;

        for (const double factor : jo::half_way_factors()) {
            SCOPED_TRACE("H factor " + std::to_string(factor));
            ++pairs;

            const auto outcome = jo::solve_with(config, factor);
            EXPECT_NEAR(outcome.cost, truth, jo::kTolerance)
                << "the join lost the optimum: " << where;
            EXPECT_EQ(outcome.problem, "") << "a joined path does not replay: " << where;
            if (std::isfinite(truth)) {
                ++with_solutions;
            }
            joined += outcome.joined_paths;
        }
    }
    std::cout << "[ SWEEP ] " << pairs << " acyclic ng (instance, H) pairs, " << with_solutions
              << " with a finite optimum, " << joined << " joined paths" << std::endl;
    EXPECT_GT(with_solutions, 0U) << "every instance was infeasible, so this asserted nothing";
    EXPECT_GT(joined, 0U) << "the join produced no paths anywhere in the sweep, so this tests the "
                             "two searches rather than the join";
}

// ============================================================================
// The soak
// ============================================================================

/// @brief Many more seeds than the suite can afford every time.
///
/// Disabled for runtime only; run it after touching the joiner, the half-way policy,
/// `NgPathExtensionFunction` or `DisjointMergeForm`, with `--gtest_also_run_disabled_tests`.
TEST(JoinOptimalityNg, DISABLED_DeepNgOptimalitySoak) {
    namespace jo = join_optimality_ng;

    size_t against_oracle = 0;
    for (const auto& config : jo::cyclic_cross_product(/*num_seeds=*/20, /*num_nodes=*/9)) {
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);
        const double truth = test_util::ng_cyclic_optimum(config);
        for (const double factor : jo::half_way_factors()) {
            SCOPED_TRACE("H factor " + std::to_string(factor));
            const auto outcome = jo::solve_with(config, factor);
            EXPECT_NEAR(outcome.cost, truth, jo::kTolerance);
            EXPECT_EQ(outcome.problem, "");
            ++against_oracle;
        }
    }

    size_t against_brute_force = 0;
    for (const auto& config : jo::acyclic_cross_product(/*num_seeds=*/12, /*num_nodes=*/8)) {
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);
        const double truth = test_util::brute_force_optimum(config);
        for (const double factor : jo::half_way_factors()) {
            SCOPED_TRACE("H factor " + std::to_string(factor));
            const auto outcome = jo::solve_with(config, factor);
            EXPECT_NEAR(outcome.cost, truth, jo::kTolerance);
            EXPECT_EQ(outcome.problem, "");
            ++against_brute_force;
        }
    }

    std::cout << "[ SOAK ] " << against_oracle << " cyclic (instance, H) pairs against the walk "
              << "oracle, " << against_brute_force << " acyclic against brute force" << std::endl;
}
