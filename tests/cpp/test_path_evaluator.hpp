// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The path evaluator finds each fault it names, and accepts what every search returns.

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/golden_harness.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace path_evaluator_test {

using GraphType = ResourceGraph<RealResource>;

/// @brief Source 0, sink 3; node 2's window closes at 8.
///
/// Arcs (cost, time): 0->1 (1, 5), 1->2 (1, 5), 0->2 (4, 1), 2->3 (1, 1), 1->3 (5, 1), ids 0 to
/// 4. So 0 1 2 3 arrives at node 2 at time 10, past its window.
inline std::unique_ptr<GraphType> windowed() {
    auto graph = std::make_unique<GraphType>();
    const std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}},
                                                              {1, {0.0, 100.0}},
                                                              {2, {0.0, 8.0}},
                                                              {3, {0.0, 100.0}}};
    presets::add_cost_resource<RealResource>(*graph);
    presets::add_window_resource<RealResource>(*graph, windows);
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2);
    graph->add_node(3, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource, RealResource>({1.0, 5.0}, 0, 1, 1.0);
    graph->add_arc<RealResource, RealResource>({1.0, 5.0}, 1, 2, 1.0);
    graph->add_arc<RealResource, RealResource>({4.0, 1.0}, 0, 2, 4.0);
    graph->add_arc<RealResource, RealResource>({1.0, 1.0}, 2, 3, 1.0);
    graph->add_arc<RealResource, RealResource>({5.0, 1.0}, 1, 3, 5.0);
    return graph;
}

/// @brief A cost feasibility that rules node 2 out as unreachable, as an ng memory can.
class BlocksNodeTwo : public Clonable<BlocksNodeTwo, FeasibilityFunction<RealResource>> {
    public:
        auto is_feasible(const RealResource& /*resource*/) -> bool override { return true; }

        auto is_reachable(const Resource<RealResource>& /*resource*/,
                          size_t destination_node_id) -> bool override {
            return destination_node_id != 2;
        }
};

/// @brief Source 0, sink 3, arcs 0 -> 1, 0 -> 2, 1 -> 3, 2 -> 3 (ids 0 to 3), each costing 1, and
///        node 2 unreachable: the composition's look-ahead asks the cost's feasibility.
inline std::unique_ptr<GraphType> blocked_diamond() {
    auto graph = std::make_unique<GraphType>(
        std::make_unique<CompositionExtensionFunction<RealResource>>(),
        std::make_unique<ReachableCompositionFeasibilityFunction<RealResource, RealResource>>(0),
        std::make_unique<ComponentCostFunction<0, RealResource>>(0),
        std::make_unique<CompositionDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<BlocksNodeTwo>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2);
    graph->add_node(3, /*source=*/false, /*sink=*/true);
    for (const auto& [origin, destination] :
         std::vector<std::pair<size_t, size_t>>{{0, 1}, {0, 2}, {1, 3}, {2, 3}}) {
        graph->add_arc<RealResource>({1.0}, origin, destination, 1.0);
    }
    return graph;
}

/// @brief Expects every solution of @p result to be valid on @p graph.
inline void expect_valid(const GraphType& graph, const SolveResult& result,
                         const std::string& what) {
    const PathEvaluator evaluator(graph);
    for (size_t i = 0; i < result.solutions.size(); ++i) {
        const PathEvaluation evaluation = evaluator.evaluate(result.solutions[i]);
        EXPECT_TRUE(evaluation.valid())
            << what << ", solution " << i << ": fault " << static_cast<int>(evaluation.fault)
            << " at " << evaluation.position;
    }
}

/// @brief Solves @p in with @p Strategy in @p direction, and expects every solution to be valid.
template <typename Strategy>
void expect_valid_solutions(const golden_labelling::Instance& in, SearchDirection direction,
                            const std::string& what, size_t max_iterations = kNoLimit) {
    auto graph = golden_labelling::build(in);
    AlgorithmParams<golden_labelling::ForwardList> params;
    params.direction = direction;
    params.max_iterations = max_iterations;
    params.critical_resource_index = 1;
    params.half_way_point = 25.0;  // NOLINT(readability-magic-numbers)
    const SolveResult result = graph->solve<Strategy>(params, /*preprocess=*/true);
    ASSERT_FALSE(result.solutions.empty()) << what;
    expect_valid(*graph, result, what);
}

}  // namespace path_evaluator_test

/// @brief A path from the source to the sink replays at its cost; a solution's node ids and cost
///        are checked too.
TEST(PathEvaluator, ReplaysAValidPath) {
    namespace pe = path_evaluator_test;
    auto graph = pe::windowed();
    const PathEvaluator evaluator(*graph);
    const PathEvaluation evaluation = evaluator.evaluate(std::vector<size_t>{2, 3});
    EXPECT_TRUE(evaluation.valid());
    EXPECT_DOUBLE_EQ(evaluation.cost, 5.0);
    EXPECT_TRUE(evaluator.evaluate(Solution(6.0, {0, 1, 3}, {0, 4})).valid());
    EXPECT_TRUE(evaluator.evaluate(Solution(6.0, {}, {0, 4})).valid());
}

/// @brief Each fault is found, at its place.
TEST(PathEvaluator, FindsEachFault) {
    namespace pe = path_evaluator_test;
    auto graph = pe::windowed();
    const PathEvaluator evaluator(*graph);
    struct Expected {
            const char* what;
            Solution solution;
            PathFault fault;
            size_t position;
    };
    const std::vector<Expected> expected{
        {"empty", Solution(0.0, {}, {}), PathFault::Empty, 0},
        {"an arc the graph does not have", Solution(5.0, {}, {2, 99}), PathFault::UnknownArc, 1},
        {"0 -> 1, then 2 -> 3", Solution(2.0, {}, {0, 3}), PathFault::Disconnected, 1},
        {"from node 1", Solution(2.0, {}, {1, 3}), PathFault::NotFromSource, 0},
        {"to node 2", Solution(2.0, {}, {0, 1}), PathFault::NotToSink, 1},
        {"past node 2's window", Solution(3.0, {}, {0, 1, 3}), PathFault::Infeasible, 1},
        {"through node 1, say its nodes",
         Solution(5.0, {0, 1, 3}, {2, 3}),
         PathFault::NodesDisagree,
         1},
        {"one node too many", Solution(5.0, {0, 2, 3, 3}, {2, 3}), PathFault::NodesDisagree, 3},
        {"one node short", Solution(5.0, {0, 2}, {2, 3}), PathFault::NodesDisagree, 2},
        {"costing 4", Solution(4.0, {}, {2, 3}), PathFault::CostDisagrees, 0},
    };
    for (const auto& e : expected) {
        const PathEvaluation evaluation = evaluator.evaluate(e.solution);
        EXPECT_EQ(evaluation.fault, e.fault) << e.what;
        EXPECT_EQ(evaluation.position, e.position) << e.what;
    }
    EXPECT_DOUBLE_EQ(evaluator.evaluate(Solution(4.0, {}, {2, 3})).cost, 5.0);

    auto diamond = pe::blocked_diamond();
    const PathEvaluation unreachable = PathEvaluator(*diamond).evaluate(std::vector<size_t>{1, 3});
    EXPECT_EQ(unreachable.fault, PathFault::Unreachable);
    EXPECT_EQ(unreachable.position, 0U);
    EXPECT_TRUE(PathEvaluator(*diamond).evaluate(std::vector<size_t>{0, 2}).valid());
}

/// @brief An arc removed from the graph is not one of its arcs until it is restored.
TEST(PathEvaluator, ARemovedArcIsUnknown) {
    namespace pe = path_evaluator_test;
    auto graph = pe::windowed();
    ASSERT_TRUE(graph->remove_arc(4));
    const PathEvaluator evaluator(*graph);
    EXPECT_EQ(evaluator.evaluate(std::vector<size_t>{0, 4}).fault, PathFault::UnknownArc);
    graph->restore_arc(4);
    EXPECT_TRUE(evaluator.evaluate(std::vector<size_t>{0, 4}).valid());
}

/// @brief Every search's solutions replay as valid paths at their cost, after a solve that
///        preprocessed the graph: the exact searches in every direction, and the heuristics.
TEST(PathEvaluator, AcceptsWhatEverySearchReturns) {
    namespace pe = path_evaluator_test;
    namespace gl = golden_labelling;
    constexpr size_t kIterations = 30;
    for (const gl::Spec spec :
         {gl::Spec{1, 11, 1}, gl::Spec{3, 11, 1}, gl::Spec{9, 11, 2}, gl::Spec{13, 16, 1}}) {
        const gl::Instance in = gl::generate(spec);
        const std::string name = "seed " + std::to_string(spec.seed);
        for (const auto direction : {SearchDirection::Forward,
                                     SearchDirection::Backward,
                                     SearchDirection::Bidirectional}) {
            const std::string where =
                name + ", direction " + std::to_string(static_cast<int>(direction));
            pe::expect_valid_solutions<strategy::Simple>(in, direction, where + ", Simple");
            pe::expect_valid_solutions<strategy::AStar>(in, direction, where + ", A*");
            pe::expect_valid_solutions<strategy::Pushing>(in, direction, where + ", Pushing");
            pe::expect_valid_solutions<strategy::Pulling>(in, direction, where + ", Pulling");
        }
        const auto forward = SearchDirection::Forward;
        pe::expect_valid_solutions<strategy::Greedy>(in, forward, name + ", Greedy");
        pe::expect_valid_solutions<strategy::Tabu>(in, forward, name + ", Tabu", kIterations);
        pe::expect_valid_solutions<strategy::ImprovingTabu>(in,
                                                            forward,
                                                            name + ", ImprovingTabu",
                                                            kIterations);
        pe::expect_valid_solutions<strategy::Diversification<>>(in,
                                                                forward,
                                                                name + ", Diversification",
                                                                kIterations);
    }
}
