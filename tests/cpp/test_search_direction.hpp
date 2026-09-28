// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The search direction as a parameter: which algorithm searches in which direction, and what
// create_algorithm and solve build from it.

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/bidirectional_test_util.hpp"
#include "util/reverse_graph_oracle.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace search_direction_test {

using Composed = ResourceTypeComposition<RealResource>;
using List = LabelList<Composed>;
using Buckets = LabelBuckets<RealResource, RealResource, Composed>;
/// The internal bidirectional search on these graphs, with a clock of type @p Clock.
template <typename Clock>
using Bidir = detail::BidirectionalDominanceAlgorithm<Composed, List, Clock, RealResource>;

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

/// @brief Simple searches bidirectionally when its params say so, and finds the forward optimum
///        along complete paths.
TEST(SearchDirection, SimpleRunsBidirectionally) {
    namespace sdt = search_direction_test;
    auto g = sdt::graph();
    const auto forward = g->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});

    AlgorithmBaseParams base;
    base.direction = SearchDirection::Bidirectional;
    const auto bidirectional = g->solve<SimpleDominanceAlgorithm>(base);

    ASSERT_FALSE(forward.solutions.empty());
    ASSERT_FALSE(bidirectional.solutions.empty());
    EXPECT_NEAR(bidirectional.solutions.front().cost,
                forward.solutions.front().cost,
                sdt::kTolerance);
    EXPECT_EQ(bidirectional.solutions.front().path_arc_ids, forward.solutions.front().path_arc_ids);
    for (const auto& solution : bidirectional.solutions) {
        ASSERT_GE(solution.path_node_ids.size(), 2U);
        EXPECT_TRUE(g->get_node(solution.path_node_ids.front())->source);
        EXPECT_TRUE(g->get_node(solution.path_node_ids.back())->sink);
    }
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

// ============================================================================
// Which class a bidirectional search is
// ============================================================================

/// @brief Simple asked for a bidirectional search builds the internal bidirectional class, which
///        says so.
TEST(BidirectionalSelection, SimpleBuildsTheBidirectionalSearch) {
    namespace sdt = search_direction_test;
    auto g = sdt::graph();
    auto algorithm =
        g->create_algorithm<SimpleDominanceAlgorithm>(sdt::params(SearchDirection::Bidirectional));
    EXPECT_NO_THROW((void)test_util::bidirectional_impl<sdt::Bidir<RealResource>>(algorithm.get()));
    EXPECT_EQ(algorithm->direction(), SearchDirection::Bidirectional);
    EXPECT_EQ(algorithm->supported_directions(),
              std::vector<SearchDirection>{SearchDirection::Bidirectional});
}

/// @brief The clock's type, named after the cost's, selects its own instantiation; left out, it
///        is the cost's.
TEST(BidirectionalSelection, AnIntClockSelectsTheIntInstantiation) {
    namespace sdt = search_direction_test;
    auto g = sdt::graph();
    auto algorithm =
        g->create_algorithm<SimpleDominanceAlgorithm, sdt::List, RealResource, IntResource>(
            sdt::params(SearchDirection::Bidirectional));
    EXPECT_NO_THROW((void)test_util::bidirectional_impl<sdt::Bidir<IntResource>>(algorithm.get()));
    EXPECT_THROW((void)test_util::bidirectional_impl<sdt::Bidir<RealResource>>(algorithm.get()),
                 std::logic_error);
}

/// @brief Pushing searches bidirectionally too, sweeping the nodes; Simple keeps arrival order.
/// Both find the forward optimum.
TEST(BidirectionalSelection, PushingSearchesBidirectionallyBySweeping) {
    namespace sdt = search_direction_test;
    auto g = sdt::graph();
    const double forward = g->solve<SimpleDominanceAlgorithm>(sdt::params(SearchDirection::Forward))
                               .solutions.front()
                               .cost;
    auto pushing =
        g->create_algorithm<PushingDominanceAlgorithm>(sdt::params(SearchDirection::Bidirectional));
    auto simple =
        g->create_algorithm<SimpleDominanceAlgorithm>(sdt::params(SearchDirection::Bidirectional));
    EXPECT_EQ(
        test_util::bidirectional_impl<sdt::Bidir<RealResource>>(pushing.get())->frontier_order(),
        detail::FrontierOrder::Sweep);
    EXPECT_EQ(
        test_util::bidirectional_impl<sdt::Bidir<RealResource>>(simple.get())->frontier_order(),
        detail::FrontierOrder::Arrival);
    for (auto* algorithm : {pushing.get(), simple.get()}) {
        const auto result = g->solve(algorithm);
        ASSERT_FALSE(result.solutions.empty());
        EXPECT_DOUBLE_EQ(result.solutions.front().cost, forward);
    }
}

/// @brief A bidirectional search runs one phase, and says so when asked for more.
TEST(BidirectionalSelection, MorePhasesAreWarnedAbout) {
    namespace sdt = search_direction_test;
    auto g = sdt::graph();
    auto params = sdt::params(SearchDirection::Bidirectional);
    params.num_max_phases = 2;
    testing::internal::CaptureStdout();
    auto algorithm = g->create_algorithm<SimpleDominanceAlgorithm>(params);
    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("num_max_phases > 1 has no effect on a bidirectional search"),
              std::string::npos)
        << output;
}
