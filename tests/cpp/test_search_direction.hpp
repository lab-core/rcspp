// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The search direction as a parameter: which algorithm searches in which direction, and what
// create_algorithm and solve build from it.

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/reverse_graph_oracle.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace search_direction_test {

using Composed = ResourceTypeComposition<RealResource>;
using List = LabelList<Composed>;
using Buckets = LabelBuckets<RealResource, RealResource, Composed>;

constexpr double kTolerance = 1e-9;

/// @brief Two routes to the sink, with a capacity that rules the cheaper one out: -4 wins.
inline std::unique_ptr<ResourceGraph<RealResource>> graph() {
    return test_util::build_additive_graph(
        {.num_nodes = 4,
         .sources = {0},
         .sinks = {3},
         .arcs = {{.origin = 0, .destination = 1, .cost = -2.0, .load = 4.0},
                  {.origin = 1, .destination = 3, .cost = -4.0, .load = 4.0},
                  {.origin = 0, .destination = 2, .cost = -1.0, .load = 1.0},
                  {.origin = 2, .destination = 3, .cost = -3.0, .load = 1.0}},
         .capacity = 5.0},
        /*reversed=*/false);
}

/// @brief Params searching in @p direction, everything else default.
inline AlgorithmParams<List> params(SearchDirection direction) {
    AlgorithmParams<List> result;
    result.direction = direction;
    return result;
}

/// @brief Expects @p Strategy to refuse a backward search, both when it is created and when a
///        graph is asked to solve with it.
template <template <typename, typename> class Strategy>
void expect_backward_refused(const char* name) {
    auto g = graph();
    EXPECT_THROW((void)g->template create_algorithm<Strategy>(params(SearchDirection::Backward)),
                 std::invalid_argument)
        << name;
    EXPECT_THROW((void)g->template solve<Strategy>(params(SearchDirection::Backward)),
                 std::invalid_argument)
        << name;
}

}  // namespace search_direction_test

/// @brief Params search forward unless told otherwise, and an algorithm says what it supports.
TEST(SearchDirection, DefaultIsForward) {
    namespace sdt = search_direction_test;
    EXPECT_EQ(AlgorithmBaseParams{}.direction, SearchDirection::Forward);

    auto g = sdt::graph();
    auto forward = g->create_algorithm<SimpleDominanceAlgorithm>(AlgorithmParams<sdt::List>{});
    EXPECT_EQ(forward->direction(), SearchDirection::Forward);
    EXPECT_EQ(forward->supported_directions(),
              std::vector<SearchDirection>{SearchDirection::Forward});

    auto backward =
        g->create_algorithm<SimpleDominanceAlgorithm>(sdt::params(SearchDirection::Backward));
    EXPECT_EQ(backward->direction(), SearchDirection::Backward);
    EXPECT_EQ(backward->supported_directions(),
              std::vector<SearchDirection>{SearchDirection::Backward});
}

/// @brief Simple searches backward when its params say so, and finds the forward optimum along
///        complete paths.
TEST(SearchDirection, SimpleRunsBackward) {
    namespace sdt = search_direction_test;
    auto g = sdt::graph();
    const auto forward = g->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});

    AlgorithmBaseParams base;
    base.direction = SearchDirection::Backward;
    const auto backward = g->solve<SimpleDominanceAlgorithm>(base);

    ASSERT_FALSE(forward.solutions.empty());
    ASSERT_FALSE(backward.solutions.empty());
    EXPECT_NEAR(forward.solutions.front().cost, -4.0, sdt::kTolerance);
    EXPECT_NEAR(backward.solutions.front().cost, forward.solutions.front().cost, sdt::kTolerance);
    EXPECT_EQ(backward.solutions.front().path_arc_ids, forward.solutions.front().path_arc_ids);
    for (const auto& solution : backward.solutions) {
        ASSERT_GE(solution.path_node_ids.size(), 2U);
        EXPECT_TRUE(g->get_node(solution.path_node_ids.front())->source);
        EXPECT_TRUE(g->get_node(solution.path_node_ids.back())->sink);
    }
}

/// @brief Every other algorithm searches forward only, and says so when it is created.
TEST(SearchDirection, OtherAlgorithmsRefuseBackward) {
    namespace sdt = search_direction_test;
    sdt::expect_backward_refused<PushingDominanceAlgorithm>("Pushing");
    sdt::expect_backward_refused<PullingDominanceAlgorithm>("Pulling");
    sdt::expect_backward_refused<AStarAlgoBound<RealResource>::Algo>("A*");
    sdt::expect_backward_refused<GreedyAlgorithm>("Greedy");
    sdt::expect_backward_refused<TabuSearchAlgorithm>("Tabu");
}

/// @brief No algorithm searches bidirectionally in this version.
TEST(SearchDirection, BidirectionalIsNotYetSupported) {
    namespace sdt = search_direction_test;
    auto g = sdt::graph();
    EXPECT_THROW((void)g->create_algorithm<SimpleDominanceAlgorithm>(
                     sdt::params(SearchDirection::Bidirectional)),
                 std::invalid_argument);
}

/// @brief A backward search compares labels backward, which only a LabelList can.
TEST(SearchDirection, BackwardNeedsALabelList) {
    namespace sdt = search_direction_test;
    auto g = sdt::graph();
    AlgorithmParams<sdt::Buckets> params(sdt::Buckets(1, 0, 0));
    params.direction = SearchDirection::Backward;
    EXPECT_THROW((void)(g->create_algorithm<SimpleDominanceAlgorithm, sdt::Buckets>(params)),
                 std::invalid_argument);

    params.direction = SearchDirection::Forward;
    EXPECT_NO_THROW((void)(g->create_algorithm<SimpleDominanceAlgorithm, sdt::Buckets>(params)));
}

/// @brief An algorithm built without create_algorithm still refuses a direction it does not
///        support, when it solves.
TEST(SearchDirection, AConstructedAlgorithmChecksItsDirection) {
    namespace sdt = search_direction_test;
    auto g = sdt::graph();
    PushingDominanceAlgorithm<sdt::Composed, sdt::List> pushing(
        &g->get_resource_factory(),
        sdt::params(SearchDirection::Backward));
    EXPECT_THROW((void)g->solve(&pushing), std::invalid_argument);
    EXPECT_THROW((void)pushing.solve(g.get(), std::numeric_limits<double>::infinity()),
                 std::invalid_argument);
}

/// @brief With params alone, create_algorithm returns the base class; with other constructor
///        arguments, the type it names.
TEST(SearchDirection, CreateAlgorithmStillAcceptsOtherArguments) {
    namespace sdt = search_direction_test;
    auto g = sdt::graph();
    auto inner = g->create_algorithm<GreedyAlgorithm>(AlgorithmParams<sdt::List>{});
    static_assert(
        std::is_same_v<decltype(inner), std::unique_ptr<Algorithm<sdt::Composed, sdt::List>>>);

    AlgorithmParams<sdt::List> params;
    params.max_iterations = 2;
    auto outer = g->create_algorithm<DiversificationSearch>(params, std::move(inner));
    static_assert(std::is_same_v<decltype(outer),
                                 std::unique_ptr<DiversificationSearch<sdt::Composed, sdt::List>>>);
    EXPECT_EQ(outer->direction(), SearchDirection::Forward);
}
