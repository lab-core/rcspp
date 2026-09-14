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
// ┌─ Why this compares against an ENUMERATOR as well as against the forward search ───────────┐
// │ `ng_cyclic_optimum` shares no code with either algorithm, so it catches a rule that is     │
// │ wrong the same way in both -- which comparing two label-setting searches cannot. The       │
// │ forward search is checked against it here too, and agrees; that is a claim worth making    │
// │ rather than assuming, because it did not hold before the interior-terminal rule was        │
// │ settled (see CyclicOptimalityAtLargerSizes below).                                         │
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

// ─────────────────────────────────────────────────────────────────────────────────────────────
// The join must not be stricter than the model. (Review finding F1.)
// ─────────────────────────────────────────────────────────────────────────────────────────────

// A hand-built instance whose optimum revisits a node the join used to refuse.
//
// Six nodes, every arc 10 time units, windows [0,55], H = 25 so the clock crosses on arc 2->3.
// The optimum `0 1 2 3 1 5` costs -14 and revisits node 1 -- legally, because leaving node 3
// narrows the memory by ng(3)={3}, which forgets 1.
//
// The forward half arriving at 3 remembers {1,2} under the OLD representation, the backward half
// remembers {1}, they overlap, and disjointness refused the splice: the bounded search returned
// -2 and reported COMPLETE while the forward search returned -14. Storing the memory already
// narrowed by the node arrived at makes the forward half {2} there, and the splice goes through.
//
// Deterministic, six nodes, microseconds -- and it fails the moment the narrowing moves back to
// the node being left.
TEST(Equivalence, TheJoinAcceptsARevisitTheModelPermits) {
    auto build = []() {
        auto graph = std::make_unique<ResourceGraph<RealResource, SizeTBitsetResource>>();
        std::map<size_t, std::pair<double, double>> windows;
        std::map<size_t, std::set<size_t>> forbidden;
        for (size_t node : {0U, 1U, 2U, 3U, 5U}) {
            windows[node] = {0.0, 55.0};
            forbidden[node] = {node};
        }
        const std::map<size_t, std::set<size_t>> ng{
            {0, {0}}, {1, {1}}, {2, {1, 2}}, {3, {3}}, {5, {5}}};

        presets::add_cost_resource<RealResource>(*graph);
        presets::add_window_resource<RealResource>(*graph, windows);
        presets::add_ng_path_resource<SizeTBitsetResource>(*graph, ng, forbidden);

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

// The same property across the generator, at the sizes where it actually bites.
//
// The tier above sits at num_nodes = 8, where ng_neighborhoods(width = 3) spans 7 of the 8 nodes:
// almost nothing is ever forgotten, so the relaxation is effectively elementarity and an
// over-strict join cannot show. It first shows at num_nodes = 11. These seeds are the ones that
// exposed it -- measured before the fix, `bidirectional` with the bound on returned an optimum up
// to 11.8 % worse than `simple` on them.
//
// No oracle here on purpose: the claim is that the two algorithms agree, which is the property the
// join broke and the one a caller relies on.
TEST(Equivalence, BoundedBidirectionalMatchesForwardWhereNgActuallyForgets) {
    namespace eq = equivalence_test;

    struct Case {
            size_t num_nodes;
            unsigned seed;
    };
    // Chosen, not swept: one per size, from the set that disagreed before the fix.
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
            const auto bidi = eq::solve_bidirectional_ng(config, with_bound);
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
// the concatenation replays feasibly. See util/merge_contract.hpp for why COMPLETENESS is the half
// that needed a test -- an over-strict rule loses no elementary route, so it cannot corrupt a
// bound, and it surfaces only as `bidirectional` and `simple` disagreeing on an instance where the
// difference happens to move the optimum. That is how F1 survived.
//
// This would have failed on the pre-fix ng representation at any of these seeds, without needing
// an instance where the defect reached the optimum.
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

// The same contract on the acyclic sweep, where ng cannot reject anything and the rule must
// therefore accept every pair whose replay is feasible. Cheap, and it covers the capacity and
// time-window merge rules (MergeRule::DominanceOrder) rather than only the container one.
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

namespace size_cap_test {

/// @brief A visited set with a cardinality cap: each arc carries the singleton of the node it
///        arrives at, so the set is "nodes visited so far".
///
/// The cycle `1 -> 2 -> 3 -> 1` is what lets a forward half and a backward half share a node, which
/// is what the sum-of-counts merge rule used to over-count.
inline std::unique_ptr<ResourceGraph<RealResource, SizeTBitsetResource>> build(
    std::map<size_t, std::pair<size_t, size_t>> caps, size_t default_max) {
    auto graph = std::make_unique<ResourceGraph<RealResource, SizeTBitsetResource>>();
    presets::add_cost_resource<RealResource>(*graph);
    graph->add_resource<SizeTBitsetResource>(
        std::make_unique<UnionExtensionFunction<SizeTBitsetResource>>(),
        // The `(min, max, overrides)` overload, deliberately: it is the one that keeps a *null*
        // override map when the caller passes none, which is what tells the rule its refusals are
        // exact. The `(overrides, min, max)` overload stores an empty map instead.
        std::make_unique<SizeFeasibilityFunction<SizeTBitsetResource>>(0U,
                                                                       default_max,
                                                                       std::move(caps)),
        std::make_unique<TrivialCostFunction<SizeTBitsetResource>>(),
        std::make_unique<InclusionDominanceFunction<SizeTBitsetResource>>());

    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2);
    graph->add_node(3);
    graph->add_node(4, /*source=*/false, /*sink=*/true);

    auto arc = [&](size_t origin, size_t destination) {
        graph->add_arc<RealResource, SizeTBitsetResource>(
            std::make_tuple(std::make_tuple(1.0),
                            std::make_tuple(std::set<size_t>{destination})),
            origin,
            destination,
            1.0);
    };
    arc(0, 1);
    arc(1, 2);
    arc(2, 3);
    arc(3, 1);
    arc(2, 4);
    return graph;
}

}  // namespace size_cap_test

// A cardinality cap, uniform. The rule decides this exactly, so no replay is needed.
//
// This test was `DISABLED_` and failing: `can_be_merged` summed the two halves' counts, which
// over-counts what they share. Measured then:
//
//   the join REFUSED a splice that replays as feasible (incomplete):
//     join arc 1 (1 -> 2), prefix [0], suffix [2 3 1 4], merged [0 1 2 3 1 4]
//
//   forward {1,2} (2) + backward {1,2,3,4} (4) = 6 > 4, so the rule refused; the merged path's
//   visited set is {1,2,3,4} -- 4 elements, inside the cap.
//
// It now takes the union instead of the sum. For a cumulative container the count is largest at the
// end of the path, so |forward u backward| IS the count at the sink and bounds it everywhere
// earlier: exact, and `verified` stays at zero because no refusal needs rescuing.
TEST(MergeContract, HoldsOnAModelWithAUniformCardinalityCap) {
    const auto graph = size_cap_test::build({}, /*default_max=*/4U);

    const auto report = test_util::merge_contract_violation(*graph,
                                                            /*max_depth=*/4,
                                                            /*max_per_node=*/20);
    EXPECT_GT(report.pairs, 0U);
    EXPECT_EQ(report.violation, "");
    EXPECT_EQ(report.verified, 0U)
        << "with a uniform cap the rule is exact, so no refusal should need a replay to rescue it";
}

// The same cap, but per node -- where the rule cannot be exact, and the joiner's replay is what
// makes up the difference.
//
// With per-node caps `can_be_merged` compares the union against the *tightest* cap in the model,
// because it sees two values and not the nodes the suffix passes through. That is sound and
// conservative: a cap on a node the merged path never visits still gates the join. Node 3 here is
// capped at 1 while everything else allows 10, so a route avoiding node 3 is refused by the rule
// and rescued by the replay.
//
// `verified > 0` is the assertion that matters: it is what says the replay path did real work
// rather than sitting dormant.
TEST(MergeContract, ConservativeRefusalsAreRescuedByReplay) {
    std::map<size_t, std::pair<size_t, size_t>> caps;
    caps[3] = {0U, 1U};
    const auto graph = size_cap_test::build(caps, /*default_max=*/10U);

    const auto report = test_util::merge_contract_violation(*graph,
                                                            /*max_depth=*/4,
                                                            /*max_per_node=*/20);
    EXPECT_GT(report.pairs, 0U);
    EXPECT_EQ(report.violation, "")
        << "a refusal the replay should have overturned was left standing";
    EXPECT_GT(report.verified, 0U)
        << "no refusal was rescued, so this instance does not exercise the replay at all";
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
