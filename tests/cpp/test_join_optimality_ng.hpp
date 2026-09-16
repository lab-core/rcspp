// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The ng-path tier of the join-optimality sweep.
//
// `test_join_optimality.hpp` asks one question at length: can the boundary-label join lose an
// optimum? It answers it on the single-slot pack, across a cross product of the generator's knobs
// times several positions of `H`, against `brute_force_optimum`. This header asks the same
// question of the ng model, which is where it is hardest.
//
// **Why ng is the harder case, and why bound-on/bound-off does not settle it.** The join pairs a
// forward boundary label with a backward label at the same node and asks each component whether
// the two may be merged. For an ng component that test is disjointness over the memory each half
// stores -- and the ng memory *forgets*, narrowing by the arrival node's neighbourhood. `H` is what
// decides which labels become boundary labels, hence which pairs of memories are ever compared,
// hence how much of the forgetting the join has to be right about. The equivalence suite's ng tier
// (`test_equivalence_ng.hpp`) runs each cyclic instance at bound-off and at one `H`; that names a
// dimension when it fails, which is its job. It does not sweep `H`, and `H` is the dimension this
// interaction lives on.
//
// **Why its own translation unit.** The ng model is a two-slot pack
// (`ResourceTypeComposition<RealResource, SizeTBitsetResource>`), so every template in the engine
// instantiates again for it. `test_algorithms.cpp`, where `test_join_optimality.hpp` lives, already
// carries two packs -- the single-slot one and `test_rcspp.hpp`'s `<RealResource, IntResource>` --
// and adding a third is exactly the growth that pushed that object past MinGW's assembler limit
// under coverage instrumentation twice before ("string table overflow ... file too big"). See the
// same note on `test_equivalence_ng.hpp` and `test_presets.cpp`.
//
// **The oracle is different too, and it has to be.** `brute_force_optimum` enumerates simple paths
// by walking forwards in node order and refuses a cyclic instance outright. On the cyclic ng model
// the forward search is not a valid reference either -- see the note in `test_equivalence.hpp` --
// so the tier that matters uses `ng_cyclic_optimum`, which enumerates *walks* under the window
// horizon and shares no code with either search.

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
        /// Carried alongside the cost because a join that glues two halves remembering the same
        /// node can produce a path that is *cheaper* than the optimum rather than merely wrong,
        /// and then the cost comparison alone reports "better than the oracle" instead of
        /// "infeasible". The replay says which.
        std::string problem;
};

/// @brief Solves one ng instance bidirectionally with `H` at a given fraction of the clock's range.
///
/// The ng counterpart of @c join_optimality::solve_with. It does not reuse
/// @c equivalence_test::solve_bidirectional_ng because that takes a *boolean* bound -- off, or
/// `R/2` -- and the whole point here is to sweep where `H` sits.
///
/// **A finite, non-binding cost bound is passed on purpose, and it is what makes this tier bite.**
/// `Joiner::join` turns its incumbent cutoff on whenever `cost_upper_bound` is infinite -- a solve
/// that expressed no filter is not asking for every admissible pair -- so with the default bound
/// the join *accepts* only pairs that improve on an incumbent the forward search has usually
/// already set. Measured on this sweep: 6 accepted joins across 192 (instance, H) pairs with an
/// infinite bound. A finite bound that filters nothing takes the cutoff off, and the join is then
/// asked about every pair it can form. The same note appears on
/// `vrp_subproblem.hpp::solve_result`, for the same reason.
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

    // Far above any cost this generator can produce, so it drops nothing; see above for why it is
    // finite rather than absent.
    constexpr double kNonBindingUpperBound = 1e9;

    auto algorithm =
        run.graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    run.result = run.graph->solve(algorithm.get(), kNonBindingUpperBound);
    run.bounded = algorithm->bounded_by_half_way();

    EXPECT_TRUE(algorithm->get_label_pool().check_ref_count_consistency())
        << "reference counts must survive two searches and a join";

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
/// The same four positions `test_join_optimality.hpp` uses, and for the same reason: at 0.25 almost
/// every complete path is assembled by the join, at 0.75 the forward search finds most of them
/// itself, and 0 is the disabled bound where the boundary predicate degenerates to "has a
/// predecessor" and every stored non-seed label is paired.
inline const std::vector<double>& half_way_factors() {
    static const std::vector<double> factors{0.0, 0.25, 0.5, 0.75};
    return factors;
}

/// @brief The cyclic ng cross product -- the tier where ng is load-bearing.
///
/// Three of the generator's knobs are not free here, and the constraints interlock:
///
///  - `with_time_window` must be on. `back_arc_density > 0` without it is refused by the generator
///    (nothing bounds path length on a cyclic graph, so the forward search does not terminate),
///    and `ng_cyclic_optimum` refuses it too, because the horizon is what makes its walk
///    enumeration finite.
///  - `mixed_sign_costs` must be on, or a shortest path never revisits a node and the ng component
///    cannot bind -- the sweep would run green while asserting nothing about ng.
///  - `with_ng_path` is always on; the inert case is `test_equivalence_ng.hpp`'s.
///
/// What is left to vary is capacity, sink count and how cyclic the graph is.
///
/// **The shape is not free to choose.** `TheCyclicSweepIsOneWhereNgActuallyBinds` asserts that ng
/// changes the answer somewhere in this product, and the first version of this sweep -- eight
/// nodes, `back_arc_density` 0.25/0.35, three seeds -- failed it: every instance's optimum happened
/// to be a walk that never revisits a node inside its memory, so the oracle sweep was green while
/// asserting nothing about ng. Nine nodes at 0.3/0.35 is what binds, which matches the
/// configuration `Equivalence.NgPathActuallyBindsOnAtLeastOneCyclicInstance` had to reach for.
/// Binding is rare enough that narrowing any of this silently hollows the tier out, which is
/// exactly what that guard is there to catch.
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
/// ng cannot *reject* anything here -- no path revisits a node on a DAG, and the two halves of a
/// join are node-disjoint by construction -- so this tier is not about ng semantics. It is about
/// the join running the disjointness test at every position of `H` against the strongest oracle
/// the suite has, on a label whose width includes a container component. A container's
/// `can_be_merged` that was wrong in a way the cyclic tier's coarser oracle tolerated would show
/// up here as a cost mismatch against brute force.
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
/// The headline. `ng_cyclic_optimum` enumerates walks under the window horizon and shares no code
/// with either search, which is what makes this more than two implementations agreeing -- and on a
/// cyclic ng model it is the *only* available reference, because the forward search is not one.
///
/// Every returned path is also replayed through the model's own extenders. A join that glues two
/// halves remembering the same node produces a path that is cheaper than the optimum rather than
/// merely different, so a cost-only assertion would report "better than the oracle" and leave the
/// reader to work out that better means infeasible.
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
    // The second way this sweep could be vacuous, and the one the half-way factors exist to rule
    // out: if no solve ever produced a joined path, every answer came from a search reaching a
    // terminal and the join -- the thing under test -- contributed nothing.
    EXPECT_GT(joined, 0U) << "the join produced no paths anywhere in the sweep, so this tests the "
                             "two searches rather than the join";
}

/// @brief The sweep above is not vacuous: ng changes the answer somewhere in it.
///
/// Without this, a cross product whose instances all happen to have a shortest walk that never
/// revisits a node would pass every assertion while testing nothing about ng. The comparison runs
/// both sides through the same solve on the same two-slot graph, so the only difference is whether
/// the ng component's forbidden sets are populated; label width is identical either way.
///
/// Also asserts the direction: a restriction may make the optimum worse or leave it alone, never
/// better. If ng ever *improves* the answer it is not behaving as a restriction.
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
/// ng is inert here by construction, so this is not a semantics test -- it is the strongest oracle
/// the suite has, applied to a label that carries a container component, at every `H`. See
/// @c acyclic_cross_product for why that is worth its runtime.
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
/// `DISABLED_` for what it costs, not because it is less trustworthy -- it is the same comparison,
/// wider. The one to run after touching the joiner, the half-way policy, `NgPathExtensionFunction`
/// or `DisjointMergeForm`.
///
/// Run with `--gtest_also_run_disabled_tests`.
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
