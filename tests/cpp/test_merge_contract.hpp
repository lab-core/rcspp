// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Merge-rule contract tests on a capped visited set: small enough to enumerate exhaustively,
// cyclic enough that a forward and a backward half can share a node.
//
// Kept in its own TU: this extra resource pack re-instantiates the header-only engine, which
// overflows MinGW's assembler string table under --coverage when combined with another pack.

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/merge_contract.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace size_cap_test {

/// @brief A visited set with a cardinality cap: each arc carries the singleton of the node it
///        arrives at, so the set is "nodes visited so far".
///
/// The cycle `1 -> 2 -> 3 -> 1` lets a forward half and a backward half share a node.
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
        // The `(min, max, overrides)` overload keeps a null override map when none is passed,
        // which tells the rule its refusals are exact.
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

// A uniform cardinality cap: the rule compares |forward u backward|, the count at the sink, so it
// is exact. Summing the counts instead over-counts shared nodes.
TEST(MergeContract, HoldsOnAModelWithAUniformCardinalityCap) {
    const auto graph = size_cap_test::build({}, /*default_max=*/3U);

    const auto report = test_util::merge_contract_violation(*graph,
                                                            /*max_depth=*/4,
                                                            /*max_per_node=*/20);
    EXPECT_GT(report.pairs, 0U);
    EXPECT_GT(report.rejected, 0U);
    EXPECT_EQ(report.violation, "");
}

namespace merge_contract_test {

struct ContainerArc {
        size_t origin;
        size_t destination;
        double cost;
        double clock;
};

/// @brief Cost (component 0), a time-window clock (component 1) and a visited set built with
///        `UnionExtensionFunction` over arcs carrying `{origin}` (component 2).
template <typename Feasibility>
std::unique_ptr<ResourceGraph<RealResource, SizeTBitsetResource>> container_model(
    size_t num_nodes, std::unique_ptr<Feasibility> feasibility,
    const std::vector<ContainerArc>& arcs) {
    auto graph = std::make_unique<ResourceGraph<RealResource, SizeTBitsetResource>>();
    std::map<size_t, std::pair<double, double>> windows;
    for (size_t node = 0; node < num_nodes; ++node) {
        windows[node] = {0.0, 100.0};
    }
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<SizeTBitsetResource>(
        std::make_unique<UnionExtensionFunction<SizeTBitsetResource>>(),
        std::move(feasibility),
        std::make_unique<TrivialCostFunction<SizeTBitsetResource>>(),
        std::make_unique<InclusionDominanceFunction<SizeTBitsetResource>>());
    for (size_t node = 0; node < num_nodes; ++node) {
        graph->add_node(node, /*source=*/node == 0, /*sink=*/node + 1 == num_nodes);
    }
    for (const auto& arc : arcs) {
        graph->add_arc<RealResource, RealResource, SizeTBitsetResource>(
            std::make_tuple(std::make_tuple(arc.cost),
                            std::make_tuple(arc.clock),
                            std::make_tuple(std::set<size_t>{arc.origin})),
            arc.origin,
            arc.destination,
            arc.cost);
    }
    return graph;
}

/// @brief The per-node cap counterexample: cost, a clock, and a set capped at 3 except at node 3,
///        which allows 1.
///
///   0 -{a}-> 1 -{b}-> 2 -{c}-> 4 (sink)   cost 7, feasible
///                     2 -{}-> 3 -{c}-> 4   cost 4, reaches node 3 holding {a, b}: infeasible
///
/// Going backward, node 3 sees only {c}, so a backward search accepts the cheap path.
inline std::unique_ptr<ResourceGraph<RealResource, SizeTBitsetResource>> per_node_cap_model(
    std::map<size_t, std::pair<size_t, size_t>> caps) {
    auto graph = std::make_unique<ResourceGraph<RealResource, SizeTBitsetResource>>();
    std::map<size_t, std::pair<double, double>> windows;
    for (size_t node = 0; node < 5; ++node) {
        windows[node] = {0.0, 100.0};
    }
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<SizeTBitsetResource>(
        std::make_unique<UnionExtensionFunction<SizeTBitsetResource>>(),
        std::make_unique<SizeFeasibilityFunction<SizeTBitsetResource>>(0U, 3U, std::move(caps)),
        std::make_unique<TrivialCostFunction<SizeTBitsetResource>>(),
        std::make_unique<InclusionDominanceFunction<SizeTBitsetResource>>());
    for (size_t node = 0; node < 5; ++node) {
        graph->add_node(node, /*source=*/node == 0, /*sink=*/node == 4);
    }
    auto arc = [&](size_t origin, size_t destination, double cost, std::set<size_t> items) {
        graph->add_arc<RealResource, RealResource, SizeTBitsetResource>(
            std::make_tuple(std::make_tuple(cost),
                            std::make_tuple(1.0),
                            std::make_tuple(std::move(items))),
            origin,
            destination,
            cost);
    };
    constexpr size_t kA = 10;
    constexpr size_t kB = 11;
    constexpr size_t kC = 12;
    arc(0, 1, 1.0, {kA});
    arc(1, 2, 1.0, {kB});
    arc(2, 4, 5.0, {kC});
    arc(2, 3, 1.0, {});
    arc(3, 4, 1.0, {kC});
    return graph;
}

/// @brief Expects a bidirectional solve on @p graph to be refused at setup, naming component 2.
inline void expect_refused(ResourceGraph<RealResource, SizeTBitsetResource>* graph,
                           double half_way_point) {
    AlgorithmParams<LabelList<ResourceTypeComposition<RealResource, SizeTBitsetResource>>> params;
    params.critical_resource_index = 1;
    params.half_way_point = half_way_point;
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    try {
        graph->solve(algorithm.get());
        ADD_FAILURE() << "the model has no backward reading and must be refused at H = "
                      << half_way_point;
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("component 2"), std::string::npos) << message;
    }
}

}  // namespace merge_contract_test

/// @brief Container constraints that ask a prefix question (what has this label collected so
///        far?) cannot be answered by a backward suffix, so bidirectional solves are refused.
///
/// Each model is a minimal case that would otherwise return a wrong or empty result. The forward
/// search is checked on the same models to show the refusal is bidirectional-only.
TEST(MergeContract, ContainerConstraintsWithoutABackwardReadingAreRefused) {
    namespace mct = merge_contract_test;
    using IFF = IntersectionFeasibilityFunction<SizeTBitsetResource, size_t>;

    //   0 --> 1 --> 2 --> 3 --> 4, plus 0 --> 2.
    const std::vector<mct::ContainerArc> five{{0, 1, -10.0, 10.0},
                                              {1, 2, -1.0, 10.0},
                                              {0, 2, 0.0, 10.0},
                                              {2, 3, -1.0, 10.0},
                                              {3, 4, -1.0, 10.0}};

    // forbidden(3) = {1}: `0 1 2 3 4` is infeasible at node 3, but the backward search never sees
    // 1 before it passes 3.
    {
        const std::map<size_t, std::set<size_t>> forbidden{{3, {1}}};
        for (const double half_way : {0.0, 15.0}) {
            auto graph = mct::container_model(5, std::make_unique<IFF>(forbidden, true), five);
            mct::expect_refused(graph.get(), half_way);
        }
        auto forward = mct::container_model(5, std::make_unique<IFF>(forbidden, true), five);
        const auto result = forward->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
        ASSERT_FALSE(result.solutions.empty());
        EXPECT_NEAR(result.solutions.front().cost, -2.0, 1e-9);
    }

    // forbidden(v) = {v} over a visited set: a backward label reaches v already holding v, so
    // every backward extension would be rejected.
    {
        std::map<size_t, std::set<size_t>> self_forbidden;
        for (size_t node = 0; node < 5; ++node) {
            self_forbidden[node] = {node};
        }
        for (const double half_way : {0.0, 15.0}) {
            auto graph = mct::container_model(5, std::make_unique<IFF>(self_forbidden, true), five);
            mct::expect_refused(graph.get(), half_way);
        }
        auto forward = mct::container_model(5, std::make_unique<IFF>(self_forbidden, true), five);
        const auto result = forward->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
        ASSERT_FALSE(result.solutions.empty());
        EXPECT_NEAR(result.solutions.front().cost, -13.0, 1e-9);
    }

    // Required values: a value the forward half collects before the meeting node rejects the
    // backward half.
    {
        const std::map<size_t, std::set<size_t>> required{{2, {0}}};
        auto graph = mct::container_model(5, std::make_unique<IFF>(required, false), five);
        mct::expect_refused(graph.get(), 15.0);
    }

    // A size floor at an interior node: a backward label can only check whether the suffix alone
    // is big enough.
    {
        const std::vector<mct::ContainerArc> line{{0, 1, -1.0, 1.0},
                                                  {1, 2, -1.0, 10.0},
                                                  {2, 3, -1.0, 1.0}};
        const std::map<size_t, std::pair<size_t, size_t>> floor{{2, {2, 100}}};
        auto make_size = [&floor] {
            return std::make_unique<SizeFeasibilityFunction<SizeTBitsetResource>>(0U, 100U, floor);
        };
        for (const double half_way : {0.0, 5.0}) {
            auto graph = mct::container_model(4, make_size(), line);
            mct::expect_refused(graph.get(), half_way);
        }
        auto forward = mct::container_model(4, make_size(), line);
        const auto result =
            forward->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{},
                                                     std::numeric_limits<double>::infinity(),
                                                     /*preprocess=*/false);
        ASSERT_FALSE(result.solutions.empty());
        EXPECT_NEAR(result.solutions.front().cost, -3.0, 1e-9);
    }

    // The control: a size CAP is suffix-safe, so the same model without the floor still solves
    // bidirectionally -- the refusal is specific, not "containers are refused".
    {
        const std::vector<mct::ContainerArc> line{{0, 1, -1.0, 1.0},
                                                  {1, 2, -1.0, 10.0},
                                                  {2, 3, -1.0, 1.0}};
        auto graph = mct::container_model(
            4,
            std::make_unique<SizeFeasibilityFunction<SizeTBitsetResource>>(0U, 100U),
            line);
        AlgorithmParams<LabelList<ResourceTypeComposition<RealResource, SizeTBitsetResource>>>
            params;
        params.critical_resource_index = 1;
        params.half_way_point = 5.0;
        auto algorithm =
            graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
        SolveResult result;
        EXPECT_NO_THROW({ result = graph->solve(algorithm.get()); });
        ASSERT_FALSE(result.solutions.empty());
        EXPECT_NEAR(result.solutions.front().cost, -3.0, 1e-9);
    }
}

/// @brief A per-node size cap is refused: it bounds what the path has collected up to a node,
///        which a backward label cannot see.
///
/// Before the refusal, the backward search returned the cost-4 path through node 3 as the optimum,
/// although it reaches node 3 holding 2 elements against a cap of 1.
TEST(MergeContract, PerNodeSizeCapsAreRefused) {
    namespace mct = merge_contract_test;
    const std::map<size_t, std::pair<size_t, size_t>> caps{{3, {0U, 1U}}};

    for (const double half_way : {0.0, 2.0}) {
        auto graph = mct::per_node_cap_model(caps);
        mct::expect_refused(graph.get(), half_way);
    }

    auto forward = mct::per_node_cap_model(caps);
    const auto result =
        forward->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{},
                                                 std::numeric_limits<double>::infinity(),
                                                 /*preprocess=*/false);
    ASSERT_EQ(result.solutions.size(), 1U);
    EXPECT_NEAR(result.solutions.front().cost, 7.0, 1e-9);

    // The control: the same model with a uniform cap still solves bidirectionally, and both paths
    // are feasible there.
    auto uniform = mct::per_node_cap_model({});
    AlgorithmParams<LabelList<ResourceTypeComposition<RealResource, SizeTBitsetResource>>> params;
    params.critical_resource_index = 1;
    params.half_way_point = 2.0;
    auto algorithm = uniform->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    const auto bidirectional = uniform->solve(algorithm.get());
    ASSERT_FALSE(bidirectional.solutions.empty());
    EXPECT_NEAR(bidirectional.solutions.front().cost, 4.0, 1e-9);
}

// ============================================================================
// A node the ng neighbourhoods mention but nothing forbids may be revisited
// ============================================================================

namespace revisitable_ng_test {

using Graph = ResourceGraph<RealResource, RealResource, SizeTBitsetResource>;

/// @brief A charging station, node 2, that the neighbourhoods mention but only node 1 is forbidden.
///
/// The best route, 0 2 1 2 3 at -40, revisits node 2. Both halves of it remember node 2 wherever
/// they meet.
inline std::unique_ptr<Graph> build(double capacity) {
    auto graph = std::make_unique<Graph>();
    presets::add_cost_resource<RealResource>(*graph);
    presets::add_budget_resource<RealResource>(*graph, capacity);
    graph->add_resource<SizeTBitsetResource>(
        std::make_unique<NgPathExtensionFunction<SizeTBitsetResource>>(
            std::map<size_t, std::set<size_t>>{{1, {2}}, {2, {1, 2}}}),
        std::make_unique<IntersectionFeasibilityFunction<SizeTBitsetResource>>(
            std::map<size_t, std::set<size_t>>{{1, {1}}},
            /*forbidden=*/true),
        std::make_unique<TrivialCostFunction<SizeTBitsetResource>>(),
        std::make_unique<InclusionDominanceFunction<SizeTBitsetResource>>());
    for (size_t node_id = 0; node_id < 4; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id == 3);
    }
    const auto arc = [&](size_t origin, size_t destination, double cost) {
        graph->add_arc<RealResource, RealResource, SizeTBitsetResource>(
            std::make_tuple(std::make_tuple(cost),
                            std::make_tuple(1.0),
                            std::make_tuple(std::set<size_t>{origin})),
            origin,
            destination,
            cost);
    };
    arc(0, 2, -10.0);
    arc(2, 1, -10.0);
    arc(1, 2, -10.0);
    arc(2, 3, -10.0);
    arc(0, 3, 0.0);
    return graph;
}

}  // namespace revisitable_ng_test

/// @brief The join lets both halves remember a node that forbids nothing.
///
/// Plain disjointness refused the only join at H = 1.5 (memories {2} and {2} at node 1) and
/// returned -20 via 0 3, status complete, where simple returns -40.
TEST(MergeContract, ARevisitableNgNodeDoesNotBlockTheJoin) {
    namespace rng = revisitable_ng_test;
    for (const double capacity : {4.0, 10.0}) {
        const auto forward =
            rng::build(capacity)->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
        ASSERT_FALSE(forward.solutions.empty());
        ASSERT_DOUBLE_EQ(forward.solutions.front().cost, -40.0) << "capacity " << capacity;

        for (const double half_way_point : {0.0, 1.5, 2.5}) {
            auto graph = rng::build(capacity);
            AlgorithmParams<
                LabelList<ResourceTypeComposition<RealResource, RealResource, SizeTBitsetResource>>>
                params;
            params.critical_resource_index = 1;
            params.half_way_point = half_way_point;
            auto algorithm =
                graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
            const auto result = graph->solve(algorithm.get());
            ASSERT_FALSE(result.solutions.empty());
            EXPECT_DOUBLE_EQ(result.solutions.front().cost, -40.0)
                << "capacity " << capacity << ", H = " << half_way_point;
        }
    }
}
