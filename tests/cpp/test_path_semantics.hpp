// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// What a path is, asserted across every algorithm at once.
//
// A path starts at a source and ends at a sink, with neither a source nor a sink strictly inside
// it. The graph enforces it first: Graph::add_arc refuses an arc into a source or out of a sink, so
// no path could even try. The algorithms keep the rule too: the dominance algorithms in
// DominanceAlgorithm::extend_label (no extension INTO a source) and in their main loops (a label at
// a sink is terminal), the dive heuristics in their own extend_label. It lives here rather than
// beside any one algorithm because the failure it guards against is a *disagreement between*
// algorithms: the dominance algorithms used to be the only ones applying it, so `greedy` could
// return a better "path" than `simple` on the same graph by passing through a second source.

#include <gtest/gtest.h>

#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
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

/// @brief The message of the `std::invalid_argument` that adding @p arc to @p graph throws, or an
///        empty string when the arc is added.
inline std::string refusal(ResourceGraph<RealResource>* graph, const Arc& arc) {
    try {
        graph->add_arc<RealResource>(std::make_tuple(arc.cost),
                                     arc.origin,
                                     arc.destination,
                                     arc.cost);
    } catch (const std::invalid_argument& error) {
        return error.what();
    }
    return "";
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

/// @brief No arc may enter a source, and refusing one adds nothing.
///
///   sources {0, 1}, sink 2:  1 -> 2 (-1), 0 -> 2 (0), then 0 -> 1 (-5) is refused
///
/// The walk `0 1 2` would cost -6 with source 1 inside it, so no path can use the arc.
TEST(PathSemantics, AnArcCannotEnterASource) {
    namespace pst = path_semantics_test;
    auto graph = pst::build({{0, true, false}, {1, true, false}, {2, false, true}},
                            {{1, 2, -1.0}, {0, 2, 0.0}});

    const std::string message = pst::refusal(graph.get(), {0, 1, -5.0});
    EXPECT_NE(message.find("arc 0 -> 1 enters source 1"), std::string::npos) << message;

    EXPECT_EQ(graph->get_number_of_arcs(), 2U);
    EXPECT_EQ(graph->next_arc_id(), 2U);
    EXPECT_TRUE(graph->get_node(1)->in_arcs.empty());
    EXPECT_EQ(graph->get_node(0)->out_arcs.size(), 1U);
}

/// @brief No arc may leave a sink, and refusing one adds nothing.
///
///   source 0, sinks {1, 3}:  0 -> 1 (1), 2 -> 3 (1), 0 -> 3 (5), then 1 -> 2 (-10) is refused
///
/// The walk `0 1 2 3` would cost -8 with sink 1 inside it, so no path can use the arc.
TEST(PathSemantics, AnArcCannotLeaveASink) {
    namespace pst = path_semantics_test;
    auto graph =
        pst::build({{0, true, false}, {1, false, true}, {2, false, false}, {3, false, true}},
                   {{0, 1, 1.0}, {2, 3, 1.0}, {0, 3, 5.0}});

    const std::string message = pst::refusal(graph.get(), {1, 2, -10.0});
    EXPECT_NE(message.find("arc 1 -> 2 leaves sink 1"), std::string::npos) << message;

    EXPECT_EQ(graph->get_number_of_arcs(), 3U);
    EXPECT_EQ(graph->next_arc_id(), 3U);
    EXPECT_TRUE(graph->get_node(1)->out_arcs.empty());
    EXPECT_TRUE(graph->get_node(2)->in_arcs.empty());
}

/// @brief With several sources and sinks, every algorithm keeps them at the ends of its paths.
///
///   sources {0, 1}, sinks {3, 4}:  0 -> 2 (-5), 1 -> 2 (-1), 2 -> 3 (1), 2 -> 4 (-2),
///                                  0 -> 3 (5), 1 -> 4 (0)
///
/// The optimum is `0 2 4` at -7, which every exact algorithm must find.
TEST(PathSemantics, EveryAlgorithmKeepsSourcesAndSinksAtTheEnds) {
    namespace pst = path_semantics_test;
    const std::vector<std::tuple<size_t, bool, bool>> nodes{{0, true, false},
                                                            {1, true, false},
                                                            {2, false, false},
                                                            {3, false, true},
                                                            {4, false, true}};
    const std::vector<pst::Arc> arcs{{0, 2, -5.0},
                                     {1, 2, -1.0},
                                     {2, 3, 1.0},
                                     {2, 4, -2.0},
                                     {0, 3, 5.0},
                                     {1, 4, 0.0}};

    for (const auto& run : pst::every_algorithm()) {
        SCOPED_TRACE(run.name);
        auto graph = pst::build(nodes, arcs);
        const auto result = run.solve(graph.get());
        ASSERT_FALSE(result.solutions.empty()) << run.name;
        pst::expect_well_formed(graph.get(), result, run.name);
        if (run.exact) {
            EXPECT_NEAR(result.solutions.front().cost, -7.0, pst::kTolerance) << run.name;
        }
    }
}
