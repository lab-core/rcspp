// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The ng-path half of the equivalence suite, compiled in its own translation unit because the
// two-slot ng pack doubles the engine's template instantiations. Shared helpers live in
// util/equivalence_helpers.hpp.

#include <gtest/gtest.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/equivalence_helpers.hpp"
#include "util/merge_contract.hpp"
#include "util/random_instance.hpp"

// ─────────────────────────────────────────────────────────────────────────────────────────────
// The ng-path component. Tier 1: acyclic instances. Tier 2: cyclic instances.
// ─────────────────────────────────────────────────────────────────────────────────────────────

// Tier 1. On a DAG ng cannot reject anything, so this checks coverage and non-interference: the
// ng component runs in both directions and the answer is unchanged.
TEST(Equivalence, NgPathComponentDoesNotChangeTheAnswerOnADag) {
    namespace eq = equivalence_test;

    for (auto config : eq::sweep()) {
        config.with_ng_path = true;
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const auto forward = eq::solve_forward_ng(config);
        for (bool with_bound : {false, true}) {
            const auto bidi = eq::solve_bidirectional_ng(config, with_bound);
            EXPECT_NEAR(bidi.best_cost(), forward.best_cost(), eq::kTolerance) << where;
            EXPECT_EQ(eq::any_path_problem(bidi), "") << where;
        }
        EXPECT_EQ(eq::any_path_problem(forward), "") << where;
    }
}

// On a DAG, ng on and ng off must give the same optimum.
TEST(Equivalence, NgPathIsInertOnADag) {
    namespace eq = equivalence_test;

    for (auto config : eq::sweep()) {
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        config.with_ng_path = false;
        const auto without = eq::solve_forward_ng(config);
        config.with_ng_path = true;
        const auto with = eq::solve_forward_ng(config);

        EXPECT_NEAR(with.best_cost(), without.best_cost(), eq::kTolerance)
            << "ng changed the optimum on an acyclic instance, where it cannot bind: " << where;
    }
}

// Tier 2. Cyclic instances, where ng-feasibility actually forbids something.
//
// Compares both searches against `ng_cyclic_optimum`, which shares no code with either and so
// catches a rule both get wrong the same way. Sized so the oracle enumerates completely (it
// throws rather than truncating). Also guards the join's disjointness check: gluing two halves
// that remember the same node shows up as a cost mismatch or an infeasible replay.
TEST(Equivalence, NgPathBindsOnCyclicInstancesAndBidirectionalMatchesTheOracle) {
    namespace eq = equivalence_test;

    size_t joined = 0;
    for (unsigned seed = 0; seed < 6; ++seed) {
        test_util::InstanceConfig config;
        config.num_nodes = 8;
        config.density = 0.6;
        config.back_arc_density = 0.3;   // cycles, so ng is load-bearing
        config.mixed_sign_costs = true;  // otherwise no shortest path revisits a node
        config.with_time_window = true;  // the horizon is what keeps a cyclic instance finite
        config.with_ng_path = true;
        config.seed = seed;
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const double oracle = test_util::ng_cyclic_optimum(config);

        // The forward search is checked against the oracle too, not assumed correct.
        const auto forward = eq::solve_forward_ng(config);
        EXPECT_NEAR(forward.best_cost(), oracle, eq::kTolerance) << where;
        EXPECT_EQ(eq::any_path_problem(forward), "") << where;

        for (bool with_bound : {false, true}) {
            const auto bidi = eq::solve_bidirectional_ng(config, with_bound);
            EXPECT_NEAR(bidi.best_cost(), oracle, eq::kTolerance)
                << "bound=" << with_bound << " " << where;
            EXPECT_EQ(eq::any_path_problem(bidi), "")
                << "a joined path violated ng-feasibility: " << where;
            joined += bidi.result.number_of_joined_paths;
        }
    }

    // `any_path_problem` can only inspect paths the join returned, so require that it returned
    // some (see `equivalence_test::kNonBindingUpperBound`).
    std::cout << "[ SWEEP ] ng cyclic joins: " << joined << std::endl;
    EXPECT_GT(joined, 0U)
        << "the join produced no paths across the whole cyclic sweep, so any_path_problem "
           "inspected only search output and this tier is not testing the join";
}

// Cyclic instances with interior sinks: a terminal node may not be strictly inside a path. Both
// search directions and the oracle must apply this rule, or the answer depends on where H sits.
TEST(Equivalence, CyclicOptimalityAtLargerSizes) {
    namespace eq = equivalence_test;

    for (unsigned seed = 0; seed < 6; ++seed) {
        test_util::InstanceConfig config;
        config.num_nodes = 8;
        config.density = 0.5;
        config.back_arc_density = 0.35;
        config.mixed_sign_costs = true;
        config.with_time_window = true;
        config.with_ng_path = true;
        config.seed = seed;
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const double oracle = test_util::ng_cyclic_optimum(config);

        const auto forward = eq::solve_forward_ng(config);
        EXPECT_NEAR(forward.best_cost(), oracle, eq::kTolerance)
            << "the forward search lost solutions on a cyclic instance: " << where;

        for (bool with_bound : {false, true}) {
            const auto bidi = eq::solve_bidirectional_ng(config, with_bound);
            EXPECT_NEAR(bidi.best_cost(), oracle, eq::kTolerance)
                << "bound=" << with_bound << " " << where;
        }
    }
}

/// @brief An inert ng component does not change the answer, in either direction.
///
/// `with_ng_path = false` leaves the component registered with empty forbidden sets; its merge
/// rule must then not demand disjoint memories, or the bounded search loses the optimum.
TEST(Equivalence, AnInertNgComponentDoesNotNarrowTheJoin) {
    namespace eq = equivalence_test;

    for (unsigned seed = 0; seed < 6; ++seed) {
        test_util::InstanceConfig config;
        config.num_nodes = 8;
        config.density = 0.5;
        config.back_arc_density = 0.35;
        config.mixed_sign_costs = true;
        config.with_time_window = true;
        config.with_ng_path = false;  // present, and meant to constrain nothing
        config.seed = seed;
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const auto forward = eq::solve_forward_ng(config);
        const auto unbounded = eq::solve_bidirectional_ng(config, /*with_bound=*/false);
        const auto bounded = eq::solve_bidirectional_ng(config, /*with_bound=*/true);

        EXPECT_NEAR(unbounded.best_cost(), forward.best_cost(), eq::kTolerance) << where;
        EXPECT_NEAR(bounded.best_cost(), forward.best_cost(), eq::kTolerance)
            << "an inert ng component narrowed the join: " << where;
        EXPECT_EQ(eq::any_path_problem(bounded), "") << where;
    }
}

/// @brief The oracle's two models are genuinely different on at least one cyclic seed.
///
/// `ng_cyclic_optimum` can stop a walk at the first sink (the library's model) or let it pass
/// through. If the two always agree, the flag is dead and CyclicOptimalityAtLargerSizes
/// checks nothing new.
TEST(Equivalence, TheOracleTwoSinkModelsDisagreeSomewhere) {
    bool differ_somewhere = false;
    for (unsigned seed = 0; seed < 6; ++seed) {
        test_util::InstanceConfig config;
        config.num_nodes = 8;
        config.density = 0.5;
        config.back_arc_density = 0.35;
        config.mixed_sign_costs = true;
        config.with_time_window = true;
        config.with_ng_path = true;
        config.seed = seed;

        const double stopping = test_util::ng_cyclic_optimum(config);
        const double walking_through =
            test_util::ng_cyclic_optimum(config, 20000000LL, /*allow_interior_sinks=*/true);

        // The permissive model enumerates a superset of walks, so it can only be better or equal.
        EXPECT_LE(walking_through, stopping + 1e-9) << "seed " << seed;
        if (walking_through < stopping - 1e-9) {
            differ_somewhere = true;
        }
    }
    EXPECT_TRUE(differ_somewhere)
        << "the two sink models agree on every seed, so the distinction is untested";
}

// Guards tier 2 against vacuity: ng must change the optimum on at least one of these seeds.
// Generator changes (wider neighborhoods, fewer back arcs, non-negative costs) would otherwise
// hollow it out silently.
TEST(Equivalence, NgPathActuallyBindsOnAtLeastOneCyclicInstance) {
    namespace eq = equivalence_test;

    // One helper so the two configs cannot drift apart: everything is identical except the flag.
    const auto make_config = [](unsigned seed, bool with_ng) {
        test_util::InstanceConfig config;
        config.num_nodes = 9;
        config.density = 0.5;
        config.back_arc_density = 0.35;
        config.mixed_sign_costs = true;
        config.with_time_window = true;
        config.with_ng_path = with_ng;
        config.seed = seed;
        return config;
    };

    // Both sides go through the same search; the question is only whether ng changes the answer.
    bool bound_somewhere = false;
    for (unsigned seed = 0; seed < 8; ++seed) {
        // Same two-slot graph type for both; with ng off the component is present but inert.
        const auto restricted = eq::solve_forward_ng(make_config(seed, true));
        const auto unrestricted = eq::solve_forward_ng(make_config(seed, false));

        // A restriction can only make the optimum worse or leave it alone.
        if (!restricted.result.solutions.empty() && !unrestricted.result.solutions.empty()) {
            EXPECT_GE(restricted.best_cost(), unrestricted.best_cost() - eq::kTolerance)
                << "ng made the optimum BETTER, which a restriction cannot do, at seed " << seed;
        }

        const bool ng_cut_the_optimum =
            unrestricted.result.solutions.empty() != restricted.result.solutions.empty() ||
            (!restricted.result.solutions.empty() &&
             restricted.best_cost() > unrestricted.best_cost() + eq::kTolerance);
        if (ng_cut_the_optimum) {
            bound_somewhere = true;
        }
    }

    EXPECT_TRUE(bound_somewhere)
        << "ng never bound across 8 cyclic seeds: the neighborhoods are too loose, so the cyclic "
           "tier is testing nothing the acyclic tier does not already cover. Tighten the "
           "neighborhood window in test_util::ng_neighborhoods, or raise back_arc_density.";
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// The join must not be stricter than the model.
// ─────────────────────────────────────────────────────────────────────────────────────────────

// A hand-built instance whose optimum revisits a node legally.
//
// Every arc takes 10 time units and H = 25, so the clock crosses on arc 2->3. The optimum
// `0 1 2 3 1 5` costs -14 and revisits node 1, which is allowed because ng(3)={3} forgets 1. The
// join must store memories narrowed by the node arrived at, or it refuses this splice.
TEST(Equivalence, TheJoinAcceptsARevisitTheModelPermits) {
    auto build = []() {
        auto graph = std::make_unique<ResourceGraph<RealResource, SizeTBitsetResource>>();
        std::map<size_t, std::pair<double, double>> windows;
        for (size_t node : {0U, 1U, 2U, 3U, 5U}) {
            windows[node] = {0.0, 55.0};
        }
        const std::map<size_t, std::set<size_t>> ng{{0, {0}},
                                                    {1, {1}},
                                                    {2, {1, 2}},
                                                    {3, {3}},
                                                    {5, {5}}};

        presets::add_cost_resource<RealResource>(*graph);
        presets::add_window_resource<RealResource>(*graph, windows);
        presets::add_ng_path_resource<SizeTBitsetResource>(*graph, ng);

        graph->add_node(0, /*source=*/true, /*sink=*/false);
        graph->add_node(1);
        graph->add_node(2);
        graph->add_node(3);
        graph->add_node(5, /*source=*/false, /*sink=*/true);

        const std::set<size_t> no_arc_set;
        auto arc = [&](size_t origin, size_t destination, double cost) {
            graph->add_arc<RealResource, RealResource, SizeTBitsetResource>(
                std::make_tuple(std::make_tuple(cost),
                                std::make_tuple(10.0),
                                std::make_tuple(no_arc_set)),
                origin,
                destination,
                cost);
        };
        arc(0, 1, -1.0);
        arc(1, 2, -1.0);
        arc(2, 3, -1.0);
        arc(3, 1, -10.0);
        arc(1, 5, -1.0);
        arc(0, 5, 0.0);  // an escape arc, so "no solution" cannot pass for success
        return graph;
    };

    constexpr double kExpected = -14.0;

    auto forward = build();
    const auto forward_result = forward->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    ASSERT_FALSE(forward_result.solutions.empty());
    EXPECT_NEAR(forward_result.solutions.front().cost, kExpected, 1e-9);

    for (const double half_way : {0.0, 25.0}) {
        auto graph = build();
        AlgorithmParams<LabelList<equivalence_test::NgComposed>> params;
        params.critical_resource_index = 1;  // the time slot
        params.half_way_point = half_way;
        auto algorithm =
            graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
        const auto result = graph->solve(algorithm.get());

        ASSERT_FALSE(result.solutions.empty()) << "half_way_point = " << half_way;
        EXPECT_NEAR(result.solutions.front().cost, kExpected, 1e-9)
            << "the join refused a revisit the model permits, at half_way_point = " << half_way;
    }
}

// The same property on generated instances large enough that ng actually forgets nodes (at
// num_nodes = 8 the neighborhoods cover almost everything). These seeds exposed an over-strict
// join. No oracle: the claim is that forward and bidirectional agree.
TEST(Equivalence, BoundedBidirectionalMatchesForwardWhereNgActuallyForgets) {
    namespace eq = equivalence_test;

    struct Case {
            size_t num_nodes;
            unsigned seed;
    };
    // Hand-picked seeds that exposed the over-strict join.
    const std::vector<Case> cases{{11, 11}, {11, 17}, {12, 20}, {13, 10}, {14, 6}, {14, 18}};

    for (const auto& instance : cases) {
        test_util::InstanceConfig config;
        config.num_nodes = instance.num_nodes;
        config.density = 0.5;
        config.back_arc_density = 0.35;
        config.mixed_sign_costs = true;
        config.with_time_window = true;
        config.with_ng_path = true;
        config.seed = instance.seed;
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const auto forward = eq::solve_forward_ng(config);
        ASSERT_FALSE(forward.result.solutions.empty()) << where;

        for (bool with_bound : {false, true}) {
            // Infinite bound keeps the joiner's incumbent cutoff on; replaying every admissible
            // pair at these sizes is too slow, and join breadth is covered elsewhere.
            const auto bidi = eq::solve_bidirectional_ng(config,
                                                         with_bound,
                                                         std::numeric_limits<double>::infinity());
            EXPECT_NEAR(bidi.best_cost(), forward.best_cost(), eq::kTolerance)
                << "bound=" << with_bound << " " << where;
            EXPECT_EQ(eq::any_path_problem(bidi), "") << where;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// The merge-rule contract, checked directly rather than through an optimum.
// ─────────────────────────────────────────────────────────────────────────────────────────────

// Every pair the joiner could be handed, on the ng model: the join's verdict must equal whether
// the concatenation replays feasibly. Checks completeness directly, since an over-strict rule
// only shows up when it happens to move the optimum.
TEST(MergeContract, HoldsOnTheNgModel) {
    size_t total_pairs = 0;
    size_t total_rejected = 0;

    for (unsigned seed = 0; seed < 4; ++seed) {
        test_util::InstanceConfig config;
        config.num_nodes = 9;
        config.density = 0.5;
        config.back_arc_density = 0.35;
        config.mixed_sign_costs = true;
        config.with_time_window = true;
        config.with_ng_path = true;
        config.seed = seed;
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const auto built = test_util::build_ng_instance(config);
        const auto report = test_util::merge_contract_violation(*built.graph,
                                                                /*max_depth=*/4,
                                                                /*max_per_node=*/12);

        EXPECT_EQ(report.violation, "") << where;
        total_pairs += report.pairs;
        total_rejected += report.rejected;
    }

    // Without these the test can pass by checking nothing: a model whose merge rule accepts
    // everything, or an enumeration that produced no pairs, is not evidence.
    EXPECT_GT(total_pairs, 0U) << "no candidate pairs were enumerated";
    EXPECT_GT(total_rejected, 0U)
        << "the merge rule accepted every pair, so its rejection path was never exercised";
}

// The same contract on the acyclic sweep, where the rule must accept every feasible pair. Also
// covers the capacity and time-window merge rules, not only the container one.
TEST(MergeContract, HoldsAcrossTheAcyclicSweep) {
    namespace eq = equivalence_test;

    size_t total_pairs = 0;
    for (auto config : eq::sweep()) {
        if (config.num_nodes > 16) {
            continue;  // enumeration is exponential; the small rows carry the same rules
        }
        config.with_ng_path = true;
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const auto built = test_util::build_ng_instance(config);
        const auto report = test_util::merge_contract_violation(*built.graph,
                                                                /*max_depth=*/3,
                                                                /*max_per_node=*/8);
        EXPECT_EQ(report.violation, "") << where;
        total_pairs += report.pairs;
    }
    EXPECT_GT(total_pairs, 0U);
}

// The size-cap merge-contract tests live in test_merge_contract.hpp, since that rule is not ng's.

// The generator must refuse cyclic instances without time windows (unbounded path length).
// Cyclic + mixed-sign is legal: the window horizon keeps the optimum finite.
TEST(Equivalence, TheGeneratorRefusesTheIllegalCyclicCombinations) {
    test_util::InstanceConfig unbounded;
    unbounded.back_arc_density = 0.3;
    unbounded.with_time_window = false;
    EXPECT_THROW(test_util::draw_instance(unbounded), std::logic_error);

    // Cyclic + mixed-sign is allowed, and is what makes tier 2 non-vacuous.
    test_util::InstanceConfig pricing_shaped;
    pricing_shaped.back_arc_density = 0.3;
    pricing_shaped.mixed_sign_costs = true;
    pricing_shaped.with_time_window = true;
    EXPECT_NO_THROW(test_util::draw_instance(pricing_shaped));

    // Brute force requires an acyclic instance.
    test_util::InstanceConfig cyclic;
    cyclic.back_arc_density = 0.3;
    cyclic.with_time_window = true;
    EXPECT_THROW(test_util::brute_force_optimum(cyclic), std::logic_error);
}
