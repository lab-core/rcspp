// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The ng-path half of the equivalence suite.
//
// Split out of test_equivalence.hpp, and compiled in its own translation unit, for the reason
// test_main.cpp gives: the ng model is a *two-slot* pack
// (`ResourceTypeComposition<RealResource, SizeTBitsetResource>`), so every template in the engine
// instantiates a second time here. Left in test_algorithms.cpp that doubling pushed the object
// past MinGW's assembler limit under coverage instrumentation -- "string table overflow ...
// file too big" -- which is exactly the growth the split exists to absorb.
//
// The shared helpers live in util/equivalence_helpers.hpp, which both this header and
// test_equivalence.hpp include -- a util/ header carries no TEST macros, so unlike a test_*.hpp
// it may be included by more than one TU.

#include <gtest/gtest.h>

#include <cmath>
#include <string>

#include "rcspp/rcspp.hpp"
#include "util/equivalence_helpers.hpp"
#include "util/random_instance.hpp"

// ─────────────────────────────────────────────────────────────────────────────────────────────
// The ng-path component. Two tiers, with different claims -- see 09-ng-path-benchmark.md §2.5.
// ─────────────────────────────────────────────────────────────────────────────────────────────

// Tier 1. The ng component runs in both directions across the acyclic sweep.
//
// It cannot *reject* here: no path revisits a node on a DAG, and the two halves of a join are
// node-disjoint by construction. So the claim is coverage and non-interference -- extend and
// extend_back run, the merge test is dispatched, apply/make_side are covered by a real search
// rather than by a unit test, and the answer is unchanged -- not semantics. Tier 2 is where the
// semantics are checked.
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

// The ng component must not change the answer it cannot constrain: on a DAG, the same instance
// with ng on and off must give the same optimum. This is what "non-interference" means, and it
// would catch an ng component that rejected something it should not.
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

// Tier 2. THE test: cyclic instances, where ng-feasibility actually forbids something.
//
// ┌─ Why this compares against an ENUMERATOR and not against the forward search ──────────────┐
// │ The forward search is **not a valid reference on a cyclic instance** -- it loses solutions │
// │ there. See CyclicOptimalityAtLargerSizes below for the measurements and the                │
// │ scope of the problem. `ng_cyclic_optimum` shares no code with either algorithm and is the  │
// │ honest reference. The acyclic sweep keeps comparing the two searches, where the forward    │
// │ one IS valid.                                                                              │
// └────────────────────────────────────────────────────────────────────────────────────────────┘
//
// Sized so the oracle *completes*: at these dimensions `ng_cyclic_optimum` enumerates every
// feasible walk rather than hitting its state budget, so the comparison is against the truth and
// not against a partial search. It throws rather than truncating, so growing this is loud.
//
// **What bounds the size, measured.** num_nodes was 7 while the interior-sink defect was open,
// which sized the tier by where a bug appeared rather than by what the oracle can do. It is now 8.
// The limit at 8 is *runtime*, not the state budget: in Debug this test costs 0.35 s at 8 and
// 9.85 s at 9, against the suite's informal ~2 s ceiling for an always-on test, and the oracle's
// 20 M state budget is not exhausted at either size. Do not raise the budget to go further -- it
// exists so the oracle throws rather than silently under-enumerating.
//
// This is also the regression test for step 6: the join's disjointness check is what stops the
// joiner gluing two halves that both remember the same node. Break it and either the cost stops
// matching the oracle, or `path_problem` replays the result and reports "path is infeasible at
// node N".
TEST(Equivalence, NgPathBindsOnCyclicInstancesAndBidirectionalMatchesTheOracle) {
    namespace eq = equivalence_test;

    for (unsigned seed = 0; seed < 6; ++seed) {
        test_util::InstanceConfig config;
        config.num_nodes = 8;
        config.density = 0.6;
        config.back_arc_density = 0.3;   // cycles, so ng is load-bearing
        config.mixed_sign_costs = true;  // or a shortest path never revisits a node and ng
                                         // cannot bind -- see back_arc_density's doc comment
        config.with_time_window = true;  // the horizon is what keeps a cyclic instance finite
        config.with_ng_path = true;
        config.seed = seed;
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const double oracle = test_util::ng_cyclic_optimum(config);

        // The forward search agrees at this size too, so it is checked rather than assumed --
        // that is what makes CyclicOptimalityAtLargerSizes a statement about *size*.
        const auto forward = eq::solve_forward_ng(config);
        EXPECT_NEAR(forward.best_cost(), oracle, eq::kTolerance) << where;
        EXPECT_EQ(eq::any_path_problem(forward), "") << where;

        for (bool with_bound : {false, true}) {
            const auto bidi = eq::solve_bidirectional_ng(config, with_bound);
            EXPECT_NEAR(bidi.best_cost(), oracle, eq::kTolerance)
                << "bound=" << with_bound << " " << where;
            EXPECT_EQ(eq::any_path_problem(bidi), "")
                << "a joined path violated ng-feasibility: " << where;
        }
    }
}

// Cyclic instances past the size where tier 2 sits. This was DISABLED_ and failing: the forward
// search and the half-way-bounded bidirectional search returned one cost while the unbounded
// bidirectional search returned a better one, matching an oracle that enumerated walks passing
// THROUGH a sink. The cause was not dominance and not ordering: the backward search treated a sink
// as an ordinary interior node while the forward search stopped there, so the two admitted
// different paths and the answer depended on where H sat.
//
// Both halves now state the same rule -- a terminal node may not be strictly inside a path -- and
// the oracle states it too. Measured before the fix, on the same six seeds: seeds 1 and 3 returned
// -3.406447 / -20.475812 from the unbounded search against -2.839186 / -18.018406 from the other
// two, and the winning paths visited the sink two and three times.
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
/// `with_ng_path = false` leaves the component registered with empty forbidden sets, so it is
/// documented as "present, extended in both directions, constraining nothing". That was true of
/// `is_feasible` and false of the join: `merge_rule()` returned `Custom` whatever the sets
/// contained, so `DisjointMergeForm` still demanded the two halves' memories be disjoint -- a
/// restriction the extension does not impose. Measured before the fix on seed 5: the bounded
/// bidirectional search returned -51.95 where the forward and unbounded searches both returned
/// -80.02.
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
/// `ng_cyclic_optimum` can end a walk at the first sink (the model the library solves) or let it
/// pass through (the older, more permissive one). If those two ever agree everywhere, the flag is
/// dead and the test above has stopped checking the thing it was written for.
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

// The claim tier 2 rests on: ng must actually REJECT something on at least one of these seeds, or
// the test above is a second copy of tier 1 and nobody would notice.
//
// Without this, a later generator change that quietly widened the neighborhoods -- or lowered
// back_arc_density, or turned mixed_sign_costs off -- would hollow tier 2 out silently. It
// already earned its keep once: the first draft of the cyclic tier used non-negative costs, and
// this test is what showed that ng could not bind there at all, because a shortest path over
// non-negative arcs never revisits a node. Measured today: ng cuts the optimum on 5 of the 8
// seeds.
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

    // Both sides go through the same search, so the forward search's cyclic defect cancels: the
    // question here is only whether ng changes the answer, not what the answer is.
    bool bound_somewhere = false;
    for (unsigned seed = 0; seed < 8; ++seed) {
        // Both solved through solve_forward_ng, i.e. the SAME two-slot graph type.
        // `with_ng_path = false` leaves the ng component present but inert -- empty forbidden
        // sets -- rather than absent, so the two runs differ in ng and not in label width.
        const auto restricted = eq::solve_forward_ng(make_config(seed, true));
        const auto unrestricted = eq::solve_forward_ng(make_config(seed, false));

        // A restriction can only make the optimum worse or leave it alone. If it is ever BETTER,
        // the ng component is not behaving as a restriction and something is wrong.
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

// The generator's guard. The illegal combination must be refused at the draw, not documented --
// the tests are where someone will get this wrong.
//
// Only ONE combination is illegal. Cyclic + mixed-sign is legal and is what tier 2 uses: the
// window horizon bounds path length, so a negative cycle still has a finite optimum.
TEST(Equivalence, TheGeneratorRefusesTheIllegalCyclicCombinations) {
    test_util::InstanceConfig unbounded;
    unbounded.back_arc_density = 0.3;
    unbounded.with_time_window = false;
    EXPECT_THROW(test_util::draw_instance(unbounded), std::logic_error);

    // Cyclic + mixed-sign is explicitly allowed, and is what makes tier 2 non-vacuous.
    test_util::InstanceConfig pricing_shaped;
    pricing_shaped.back_arc_density = 0.3;
    pricing_shaped.mixed_sign_costs = true;
    pricing_shaped.with_time_window = true;
    EXPECT_NO_THROW(test_util::draw_instance(pricing_shaped));

    // And brute force stays acyclic: it walks forwards in node order with no visited set.
    test_util::InstanceConfig cyclic;
    cyclic.back_arc_density = 0.3;
    cyclic.with_time_window = true;
    EXPECT_THROW(test_util::brute_force_optimum(cyclic), std::logic_error);
}
