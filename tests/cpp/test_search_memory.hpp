// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// An algorithm kept from solve to solve keeps the memory of its search: its labels (LabelPool),
// the list nodes of its schedule's queues and of its per-node containers (detail::SpareNodes). A
// solve that needs no more labels than one before it then allocates nothing per label.

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "allocation_counter.hpp"
#include "rcspp/rcspp.hpp"
#include "test_label_recycling.hpp"

namespace search_memory_test {

using rcspp::RealResource;
using Graph = rcspp::ResourceGraph<RealResource>;
inline constexpr double kInfinity = std::numeric_limits<double>::infinity();
using Params =
    rcspp::AlgorithmParams<rcspp::LabelList<rcspp::ResourceTypeComposition<RealResource>>>;

/// @brief A source (0), @p layers layers of @p width nodes with every arc between consecutive
///        layers, and a sink. Component 0 is the cost; component 1 a time that trades off with it,
///        12 minus the arc's cost, so that the labels at a node all have different costs and none
///        dominates another: a search keeps many labels.
inline std::unique_ptr<Graph> trade_off_layers(size_t layers, size_t width) {
    auto graph = std::make_unique<Graph>();
    graph->add_resource<RealResource>(
        std::make_unique<rcspp::AdditionExtensionFunction<RealResource>>(),
        std::make_unique<rcspp::TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<rcspp::ValueCostFunction<RealResource>>(),
        std::make_unique<rcspp::ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<rcspp::AdditionExtensionFunction<RealResource>>(),
        std::make_unique<rcspp::TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<rcspp::TrivialCostFunction<RealResource>>(),
        std::make_unique<rcspp::ValueDominanceFunction<RealResource>>());
    const size_t sink = (layers * width) + 1;
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    for (size_t node = 1; node < sink; ++node) {
        graph->add_node(node);
    }
    graph->add_node(sink, /*source=*/false, /*sink=*/true);

    auto add_arc = [&](size_t origin, size_t destination) {
        const double cost = static_cast<double>(((origin * 7) + (destination * 3)) % 11) + 1.0;
        std::tuple<std::vector<std::tuple<double>>> values;
        std::get<0>(values) = {std::tuple<double>(cost), std::tuple<double>(12.0 - cost)};
        graph->add_arc(values, origin, destination, cost);
    };
    for (size_t node = 1; node <= width; ++node) {
        add_arc(0, node);
        add_arc(((layers - 1) * width) + node, sink);
    }
    for (size_t layer = 0; layer + 1 < layers; ++layer) {
        for (size_t from = 1; from <= width; ++from) {
            for (size_t to = 1; to <= width; ++to) {
                add_arc((layer * width) + from, ((layer + 1) * width) + to);
            }
        }
    }
    return graph;
}

/// @brief How many times a solve allocates that repeats the ones before it, with one algorithm kept
///        (release_after_solve false), on a graph of @p layers layers.
///
/// The first solve allocates the labels and the list nodes. The second may still allocate one
/// list per container, for the nodes it keeps (@c detail::SpareNodes makes it the first time a
/// node is given back); the third is measured. The upper bound of 0 is below every path's cost,
/// so that no solution is recorded: a recorded solution allocates its path.
///
/// @param labels Set to the number of labels the algorithm holds.
template <typename Strategy>
size_t allocations_of_a_repeat_solve(size_t layers, size_t* labels) {
    auto graph = trade_off_layers(layers, /*width=*/4);
    Params params;
    params.release_after_solve = false;
    auto kept = graph->create_algorithm<Strategy>(params);
    const auto first = graph->solve(kept.get(), /*upper_bound=*/0.0, false);
    EXPECT_TRUE(first.solutions.empty());
    const auto second = graph->solve(kept.get(), /*upper_bound=*/0.0, false);
    *labels = kept->get_label_pool().get_nb_total_labels();

    size_t allocations = 0;
    {
        const allocation_counting::AllocationCounter counter;
        const auto third = graph->solve(kept.get(), /*upper_bound=*/0.0, false);
        allocations = counter.count();
    }
    EXPECT_EQ(kept->get_label_pool().get_nb_total_labels(), *labels) << "no new label";
    return allocations;
}

/// @brief A repeat solve allocates as many times on a graph of 6 layers (about 500 labels) as on
///        one of 10 (about 1,600), a handful (the solve's own bookkeeping: 4 in a Release build
///        with MSVC, more in a Debug build): no label, no queue node, no container node, and no
///        copy of the end-node labels.
template <typename Strategy>
void check_repeat_solves_allocate_nothing_per_label() {
    size_t fewer_labels = 0;
    size_t more_labels = 0;
    const size_t on_fewer = allocations_of_a_repeat_solve<Strategy>(/*layers=*/6, &fewer_labels);
    const size_t on_more = allocations_of_a_repeat_solve<Strategy>(/*layers=*/10, &more_labels);
    ASSERT_GT(fewer_labels, 300U) << "the graph should keep many labels";
    ASSERT_GT(more_labels, 2 * fewer_labels);
    EXPECT_EQ(on_more, on_fewer) << fewer_labels << " and " << more_labels << " labels";
    EXPECT_LT(on_more, 32U);
}

/// @brief Solves the dual sets in turn with one @p Strategy algorithm on LabelBuckets, kept from
///        solve to solve (its containers emptied by LabelBuckets::clear), and with a fresh one each
///        time: the same solutions and the same extensions every time.
template <typename Strategy>
void check_kept_buckets_match_fresh_ones() {
    namespace lrt = label_recycling_test;
    using Buckets = rcspp::
        LabelBuckets<RealResource, RealResource, rcspp::ResourceTypeComposition<RealResource>>;
    // Buckets of width 2 on the time (component 1), sorted by the cost (component 0).
    const auto buckets = [] { return Buckets(2, 1, 0); };
    auto graph = lrt::layered_graph(/*time_limit=*/6.0);
    rcspp::AlgorithmParams<Buckets> keep(buckets());
    keep.release_after_solve = false;
    auto kept = graph->create_algorithm<Strategy, Buckets>(keep);
    for (const auto& duals : lrt::dual_sets()) {
        graph->update_reduced_costs(duals);
        auto fresh =
            graph->create_algorithm<Strategy, Buckets>(rcspp::AlgorithmParams<Buckets>(buckets()));
        const auto expected = graph->solve(fresh.get(), lrt::kInfinity, false);
        const auto actual = graph->solve(kept.get(), lrt::kInfinity, false);
        ASSERT_FALSE(expected.solutions.empty());
        lrt::expect_same_solutions(actual, expected);
        EXPECT_EQ(kept->get_number_of_extended_labels(), fresh->get_number_of_extended_labels());
    }
}

}  // namespace search_memory_test

TEST(SearchMemory, ARepeatSolveAllocatesNothingPerLabel) {
    namespace smt = search_memory_test;
    {
        SCOPED_TRACE("Simple");
        smt::check_repeat_solves_allocate_nothing_per_label<rcspp::strategy::Simple>();
    }
    {
        SCOPED_TRACE("Pushing");
        smt::check_repeat_solves_allocate_nothing_per_label<rcspp::strategy::Pushing>();
    }
    {
        SCOPED_TRACE("Pulling");
        smt::check_repeat_solves_allocate_nothing_per_label<rcspp::strategy::Pulling>();
    }
}

/// @brief With release_after_solve (the default), a solve frees what it used: the next one makes
///        its labels again, and finds the same paths.
TEST(SearchMemory, ReleaseAfterSolveStillFreesTheSearch) {
    namespace smt = search_memory_test;
    auto graph = smt::trade_off_layers(/*layers=*/4, /*width=*/3);
    auto algorithm = graph->create_algorithm<rcspp::strategy::Pulling>();
    const auto first = graph->solve(algorithm.get(), smt::kInfinity, false);
    EXPECT_EQ(algorithm->get_label_pool().get_nb_total_labels(), 0U);
    const auto second = graph->solve(algorithm.get(), smt::kInfinity, false);
    ASSERT_EQ(second.solutions.size(), first.solutions.size());
    for (size_t i = 0; i < first.solutions.size(); ++i) {
        EXPECT_EQ(second.solutions[i].path_arc_ids, first.solutions[i].path_arc_ids);
    }
    EXPECT_GT(algorithm->get_label_pool().get_nb_created_labels(), 0);
}

/// @brief LabelBuckets kept from solve to solve gives what fresh ones give.
TEST(SearchMemory, KeptLabelBucketsMatchFreshOnes) {
    namespace smt = search_memory_test;
    smt::check_kept_buckets_match_fresh_ones<rcspp::strategy::Simple>();
    smt::check_kept_buckets_match_fresh_ones<rcspp::strategy::Pulling>();
}
