// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// What a path is, asserted across every algorithm at once: it starts at a source, ends at a sink,
// and has neither a source nor a sink strictly inside it. Checking all algorithms together catches
// them disagreeing on this rule.

#include <gtest/gtest.h>

#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace path_semantics_test {

constexpr double kTolerance = 1e-9;

struct Arc {
        size_t origin;
        size_t destination;
        double cost;
};

/// @brief A cost-only graph: each node is `{id, source, sink}`, each arc carries its cost.
inline std::unique_ptr<ResourceGraph<RealResource>> build(
    const std::vector<std::tuple<size_t, bool, bool>>& nodes, const std::vector<Arc>& arcs) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    for (const auto& [id, source, sink] : nodes) {
        graph->add_node(id, source, sink);
    }
    for (const auto& arc : arcs) {
        graph->add_arc<RealResource>(std::make_tuple(arc.cost),
                                     arc.origin,
                                     arc.destination,
                                     arc.cost);
    }
    return graph;
}

/// @brief One algorithm to run, and whether it is exact (so its optimum is asserted too).
struct Run {
        std::string name;
        bool exact;
        std::function<SolveResult(ResourceGraph<RealResource>*)> solve;
};

inline std::vector<Run> every_algorithm() {
    AlgorithmBaseParams bounded;
    bounded.max_iterations = 10;  // the tabu searches refuse to run without a finite budget
    return {
        {"simple",
         true,
         [](auto* g) {
             return g->template solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
         }},
        {"pushing",
         true,
         [](auto* g) {
             return g->template solve<PushingDominanceAlgorithm>(AlgorithmBaseParams{});
         }},
        {"pulling",
         true,
         [](auto* g) {
             return g->template solve<PullingDominanceAlgorithm>(AlgorithmBaseParams{});
         }},
        {"astar",
         true,
         [](auto* g) {
             return g->template solve<AStarAlgoBound<RealResource>::Algo>(AlgorithmBaseParams{});
         }},
        {"bidirectional",
         true,
         [](auto* g) {
             return g->template solve<BidirectionalAlgoBound<RealResource>::Algo>(
                 AlgorithmBaseParams{});
         }},
        {"greedy",
         false,
         [](auto* g) { return g->template solve<GreedyAlgorithm>(AlgorithmBaseParams{}); }},
        {"tabu",
         false,
         [bounded](auto* g) { return g->template solve<TabuSearchAlgorithm>(bounded); }},
        {"improving tabu",
         false,
         [bounded](auto* g) { return g->template solve<ImprovingTabuSearch>(bounded); }},
    };
}

/// @brief Every returned path starts at a source, ends at a sink, and has no terminal inside.
inline void expect_well_formed(ResourceGraph<RealResource>* graph, const SolveResult& result,
                               const std::string& algorithm) {
    for (const auto& solution : result.solutions) {
        const auto& nodes = solution.path_node_ids;
        ASSERT_GE(nodes.size(), 2U) << algorithm;
        EXPECT_TRUE(graph->get_node(nodes.front())->source) << algorithm;
        EXPECT_TRUE(graph->get_node(nodes.back())->sink) << algorithm;
        for (size_t i = 1; i + 1 < nodes.size(); ++i) {
            const auto* node = graph->get_node(nodes[i]);
            EXPECT_FALSE(node->source)
                << algorithm << ": source " << nodes[i] << " inside a returned path";
            EXPECT_FALSE(node->sink)
                << algorithm << ": sink " << nodes[i] << " inside a returned path";
        }
    }
}

}  // namespace path_semantics_test

/// @brief No algorithm passes through a second source.
///
///   sources {0, 1}, sink 2:  0 -> 1 (-5), 1 -> 2 (-1), 0 -> 2 (0)
///
/// `0 1 2` would cost -6, but it has source 1 inside it; the answer is `1 2` at -1.
TEST(PathSemantics, NoAlgorithmPassesThroughASource) {
    namespace pst = path_semantics_test;
    const std::vector<std::tuple<size_t, bool, bool>> nodes{{0, true, false},
                                                            {1, true, false},
                                                            {2, false, true}};
    const std::vector<pst::Arc> arcs{{0, 1, -5.0}, {1, 2, -1.0}, {0, 2, 0.0}};

    for (const auto& run : pst::every_algorithm()) {
        SCOPED_TRACE(run.name);
        auto graph = pst::build(nodes, arcs);
        const auto result = run.solve(graph.get());
        ASSERT_FALSE(result.solutions.empty()) << run.name;
        pst::expect_well_formed(graph.get(), result, run.name);
        if (run.exact) {
            EXPECT_NEAR(result.solutions.front().cost, -1.0, pst::kTolerance) << run.name;
        }
    }
}

/// @brief No algorithm continues past a sink.
///
///   source 0, sinks {1, 3}:  0 -> 1 (1), 1 -> 2 (-10), 2 -> 3 (1), 0 -> 3 (5)
///
/// `0 1 2 3` would cost -8, but it has sink 1 inside it; the answer is `0 1` at 1.
TEST(PathSemantics, NoAlgorithmContinuesPastASink) {
    namespace pst = path_semantics_test;
    const std::vector<std::tuple<size_t, bool, bool>> nodes{{0, true, false},
                                                            {1, false, true},
                                                            {2, false, false},
                                                            {3, false, true}};
    const std::vector<pst::Arc> arcs{{0, 1, 1.0}, {1, 2, -10.0}, {2, 3, 1.0}, {0, 3, 5.0}};

    for (const auto& run : pst::every_algorithm()) {
        SCOPED_TRACE(run.name);
        auto graph = pst::build(nodes, arcs);
        const auto result = run.solve(graph.get());
        ASSERT_FALSE(result.solutions.empty()) << run.name;
        pst::expect_well_formed(graph.get(), result, run.name);
        if (run.exact) {
            EXPECT_NEAR(result.solutions.front().cost, 1.0, pst::kTolerance) << run.name;
        }
    }
}
