// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The merge-rule contract, on the one rule in the library that cannot decide its own question
// exactly: a cardinality cap with per-node overrides.
//
// util/merge_contract.hpp states what is being asserted and why COMPLETENESS is the half nothing
// else covers. What this header supplies is the model to assert it on -- a visited set with a cap,
// small enough to enumerate exhaustively and cyclic enough that a forward half and a backward half
// can share a node, which is the case the rule has to get right.
//
// Its own translation unit, by the rule at the top of test_main.cpp: the engine is header-only, so
// the two-slot `<RealResource, SizeTBitsetResource>` pack instantiates all of it again, and under
// --coverage MinGW's assembler overflows its 10 MB string table when that lands in a TU that
// already carries another pack.

#include <gtest/gtest.h>

#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <utility>

#include "rcspp/rcspp.hpp"
#include "util/merge_contract.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace size_cap_test {

/// @brief A visited set with a cardinality cap: each arc carries the singleton of the node it
///        arrives at, so the set is "nodes visited so far".
///
/// The cycle `1 -> 2 -> 3 -> 1` is what lets a forward half and a backward half share a node, which
/// is what the sum-of-counts merge rule used to over-count.
///
/// @param caps        Per-node `(min, max)` overrides; empty means a single uniform cap.
/// @param default_max The cap at every node without an override.
/// @return The built graph.
inline std::unique_ptr<ResourceGraph<RealResource, SizeTBitsetResource>> build(
    std::map<size_t, std::pair<size_t, size_t>> caps, size_t default_max) {
    auto graph = std::make_unique<ResourceGraph<RealResource, SizeTBitsetResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
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
            std::make_tuple(std::make_tuple(1.0), std::make_tuple(std::set<size_t>{destination})),
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
// Written against the pre-fix rule this failed: `can_be_merged` summed the two halves' counts,
// which over-counts what they share. Measured then:
//
//   the join REFUSED a splice that replays as feasible (incomplete):
//     join arc 1 (1 -> 2), prefix [0], suffix [2 3 1 4], merged [0 1 2 3 1 4]
//
//   forward {1,2} (2) + backward {1,2,3,4} (4) = 6 > 4, so the rule refused; the merged path's
//   visited set is {1,2,3,4} -- 4 elements, inside the cap.
//
// It takes the union instead of the sum. For a cumulative container the count is largest at the
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
