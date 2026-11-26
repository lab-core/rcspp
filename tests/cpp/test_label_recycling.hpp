// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// An algorithm with release_after_solve false keeps its labels between solves, and the next solve
// reuses them (LabelPool::recycle) unless the graph's resources have another shape. A reused label
// must make no difference: each solve returns what a fresh algorithm returns.

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

namespace label_recycling_test {

using rcspp::RealResource;
using Graph = rcspp::ResourceGraph<RealResource>;
inline constexpr double kInfinity = std::numeric_limits<double>::infinity();

/// @brief A layered graph: a source (0), three layers of three nodes (1-9) with every arc
///        between consecutive layers, and a sink (10). Component 0 is the cost (the label cost),
///        component 1 a time limited to @p time_limit, and @p extra_components more components
///        that add 1 per arc and cost nothing. Each arc into node v carries row v, so duals change
///        the reduced costs, and therefore the paths.
///
/// With @p source_time, the source node's resource is seeded with that time (node values are not
/// a label's starting state).
inline std::unique_ptr<Graph> layered_graph(double time_limit, size_t extra_components = 0,
                                            double source_time = 0.0) {
    auto graph = std::make_unique<Graph>();
    graph->add_resource<RealResource>(
        std::make_unique<rcspp::AdditionExtensionFunction<RealResource>>(),
        std::make_unique<rcspp::TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<rcspp::ValueCostFunction<RealResource>>(),
        std::make_unique<rcspp::ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<rcspp::AdditionExtensionFunction<RealResource>>(),
        std::make_unique<rcspp::MinMaxFeasibilityFunction<RealResource>>(0.0, time_limit),
        std::make_unique<rcspp::TrivialCostFunction<RealResource>>(),
        std::make_unique<rcspp::ValueDominanceFunction<RealResource>>());
    for (size_t extra = 0; extra < extra_components; ++extra) {
        graph->add_resource<RealResource>(
            std::make_unique<rcspp::AdditionExtensionFunction<RealResource>>(),
            std::make_unique<rcspp::TrivialFeasibilityFunction<RealResource>>(),
            std::make_unique<rcspp::TrivialCostFunction<RealResource>>(),
            std::make_unique<rcspp::ValueDominanceFunction<RealResource>>());
    }
    const size_t components = 2 + extra_components;

    std::tuple<std::vector<std::tuple<double>>> source_values;
    std::get<0>(source_values).assign(components, std::tuple<double>(0.0));
    std::get<0>(source_values)[1] = std::tuple<double>(source_time);
    graph->add_node(0, source_values, /*source=*/true, /*sink=*/false);
    for (size_t node = 1; node <= 9; ++node) {
        graph->add_node(node);
    }
    graph->add_node(10, /*source=*/false, /*sink=*/true);

    auto add_arc = [&](size_t origin, size_t destination) {
        const double cost = static_cast<double>(((origin * 7) + (destination * 3)) % 11) + 1.0;
        const double time = static_cast<double>(((origin * 5) + destination) % 3) + 1.0;
        std::tuple<std::vector<std::tuple<double>>> values;
        std::get<0>(values).assign(components, std::tuple<double>(1.0));
        std::get<0>(values)[0] = std::tuple<double>(cost);
        std::get<0>(values)[1] = std::tuple<double>(time);
        graph->add_arc(values, origin, destination, cost, {rcspp::Row{destination, 1.0L}});
    };
    for (size_t node = 1; node <= 3; ++node) {
        add_arc(0, node);
    }
    for (size_t layer = 0; layer < 2; ++layer) {
        for (size_t from = 1; from <= 3; ++from) {
            for (size_t to = 1; to <= 3; ++to) {
                add_arc((3 * layer) + from, (3 * (layer + 1)) + to);
            }
        }
    }
    for (size_t node = 7; node <= 9; ++node) {
        add_arc(node, 10);
    }
    return graph;
}

/// @brief Dual sets that change which paths are cheapest (one dual per node, row = node id).
inline std::vector<std::vector<double>> dual_sets() {
    return {
        std::vector<double>(11, 0.0),
        {0.0, 4.0, 0.0, 1.0, 0.0, 6.0, 2.0, 0.0, 3.0, 5.0, 0.0},
        {0.0, 0.0, 7.0, 0.0, 5.0, 0.0, 1.0, 8.0, 0.0, 2.0, 0.0},
        std::vector<double>(11, 0.0),
    };
}

/// @brief Whether two results hold the same solutions, in the same order.
inline void expect_same_solutions(const rcspp::SolveResult& actual,
                                  const rcspp::SolveResult& expected) {
    ASSERT_EQ(actual.solutions.size(), expected.solutions.size());
    for (size_t i = 0; i < actual.solutions.size(); ++i) {
        EXPECT_EQ(actual.solutions[i].cost, expected.solutions[i].cost);
        EXPECT_EQ(actual.solutions[i].path_arc_ids, expected.solutions[i].path_arc_ids);
    }
    EXPECT_EQ(actual.status, expected.status);
}

/// @brief Solves the dual sets in turn with one algorithm that keeps its labels, and with a fresh
///        algorithm each time, both from @p base: the same solutions and the same extensions
///        every time; from the second solve on, labels are reused; and the last solve, a repeat
///        of the first, needs no new label. Some solve must find a path, so that the comparison
///        means something; a limited search (a per-node quota) may find none in others. Without
///        @p draws_from_its_pool (an algorithm that searches through an inner one, with a pool of
///        its own), the pool is not checked.
template <typename Strategy>
void check_repeat_solves_match_fresh_algorithms(const rcspp::AlgorithmBaseParams& base = {},
                                                bool draws_from_its_pool = true) {
    using Labels = rcspp::LabelList<rcspp::ResourceTypeComposition<RealResource>>;
    auto graph = layered_graph(/*time_limit=*/6.0);
    auto keep = base.with_container(Labels());
    keep.release_after_solve = false;
    auto kept = graph->create_algorithm<Strategy>(keep);

    int64_t created_before_last = 0;
    size_t solves_with_paths = 0;
    const auto duals = dual_sets();
    for (size_t solve = 0; solve < duals.size(); ++solve) {
        SCOPED_TRACE("solve " + std::to_string(solve));
        graph->update_reduced_costs(duals[solve]);
        auto fresh = graph->create_algorithm<Strategy>(base.with_container(Labels()));
        const auto expected = graph->solve(fresh.get(), kInfinity, false);
        if (solve + 1 == duals.size()) {
            created_before_last = kept->get_label_pool().get_nb_created_labels();
        }
        const auto actual = graph->solve(kept.get(), kInfinity, false);
        solves_with_paths += expected.solutions.empty() ? 0 : 1;
        expect_same_solutions(actual, expected);
        EXPECT_EQ(kept->get_number_of_extended_labels(), fresh->get_number_of_extended_labels());
        EXPECT_TRUE(kept->get_label_pool().check_ref_count_consistency());
        if (draws_from_its_pool && solve > 0) {
            EXPECT_GT(kept->get_label_pool().get_nb_reused_labels(), 0);
        }
    }
    EXPECT_GT(solves_with_paths, 0U);
    // The last dual set repeats the first: the labels the earlier solves made are enough.
    if (draws_from_its_pool) {
        EXPECT_EQ(kept->get_label_pool().get_nb_created_labels(), created_before_last);
    }
}

/// @brief The total cost of the cheapest solution of @p graph, with a fresh Simple algorithm.
inline double fresh_optimum(Graph* graph) {
    auto fresh = graph->create_algorithm<rcspp::strategy::Simple>();
    const auto result = graph->solve(fresh.get(), kInfinity, false);
    return result.solutions.empty() ? kInfinity : result.solutions.front().cost;
}

}  // namespace label_recycling_test

TEST(LabelRecycling, RepeatSolvesMatchFreshAlgorithms) {
    namespace lrt = label_recycling_test;
    lrt::check_repeat_solves_match_fresh_algorithms<rcspp::strategy::Simple>();
    lrt::check_repeat_solves_match_fresh_algorithms<rcspp::strategy::Pushing>();
    lrt::check_repeat_solves_match_fresh_algorithms<rcspp::strategy::Pulling>();
    lrt::check_repeat_solves_match_fresh_algorithms<rcspp::strategy::AStar>();
    lrt::check_repeat_solves_match_fresh_algorithms<rcspp::strategy::Greedy>();
    lrt::check_repeat_solves_match_fresh_algorithms<rcspp::strategy::BoundGuidedGreedy>();
}

/// @brief Backward too, for the labelling searches and the dives; and when the per-node quota
///        leaves labels stored aside at the end of a solve, which the next solve hands out again.
TEST(LabelRecycling, BackwardAndStoredAsideSolvesMatchFreshAlgorithms) {
    namespace lrt = label_recycling_test;
    rcspp::AlgorithmBaseParams backward;
    backward.direction = rcspp::SearchDirection::Backward;
    lrt::check_repeat_solves_match_fresh_algorithms<rcspp::strategy::Simple>(backward);
    lrt::check_repeat_solves_match_fresh_algorithms<rcspp::strategy::Pulling>(backward);
    lrt::check_repeat_solves_match_fresh_algorithms<rcspp::strategy::Greedy>(backward);
    rcspp::AlgorithmBaseParams backward_tabu = backward;
    backward_tabu.max_iterations = 50;  // NOLINT(readability-magic-numbers)
    lrt::check_repeat_solves_match_fresh_algorithms<rcspp::strategy::Tabu>(backward_tabu);

    rcspp::AlgorithmBaseParams quota;
    quota.max_extended_labels_per_node = 1;
    lrt::check_repeat_solves_match_fresh_algorithms<rcspp::strategy::Simple>(quota);
    lrt::check_repeat_solves_match_fresh_algorithms<rcspp::strategy::Pulling>(quota);
}

/// @brief The randomised tabu searches too: each solve restarts their tabu list and its generator
///        from the seed, so a kept one repeats a fresh one.
TEST(LabelRecycling, TabuSearchesMatchFreshAlgorithms) {
    namespace lrt = label_recycling_test;
    rcspp::AlgorithmBaseParams bounded;
    bounded.max_iterations = 50;  // NOLINT(readability-magic-numbers)
    lrt::check_repeat_solves_match_fresh_algorithms<rcspp::strategy::Tabu>(bounded);
    lrt::check_repeat_solves_match_fresh_algorithms<rcspp::strategy::ImprovingTabu>(bounded);
    // Diversification searches through an inner algorithm, which keeps the labels.
    lrt::check_repeat_solves_match_fresh_algorithms<
        rcspp::strategy::Diversification<rcspp::strategy::Greedy>>(bounded,
                                                                   /*draws_from_its_pool=*/false);
    lrt::check_repeat_solves_match_fresh_algorithms<
        rcspp::strategy::Diversification<rcspp::strategy::Tabu>>(bounded,
                                                                 /*draws_from_its_pool=*/false);
}

/// @brief An algorithm solving a graph whose resources have another number of components drops
///        the labels it kept: they could not be rebound to the new nodes.
TEST(LabelRecycling, AGraphWithAnotherShapeStartsAfresh) {
    namespace lrt = label_recycling_test;
    auto two = lrt::layered_graph(/*time_limit=*/6.0);
    auto four = lrt::layered_graph(/*time_limit=*/6.0, /*extra_components=*/2);
    rcspp::AlgorithmParams<rcspp::LabelList<rcspp::ResourceTypeComposition<RealResource>>> keep;
    keep.release_after_solve = false;
    auto kept = two->create_algorithm<rcspp::strategy::Pulling>(keep);

    const auto first = two->solve(kept.get(), lrt::kInfinity, false);
    ASSERT_FALSE(first.solutions.empty());
    EXPECT_EQ(first.solutions.front().cost, lrt::fresh_optimum(two.get()));
    const int64_t created = kept->get_label_pool().get_nb_created_labels();

    const auto other = four->solve(kept.get(), lrt::kInfinity, false);
    ASSERT_FALSE(other.solutions.empty());
    EXPECT_EQ(other.solutions.front().cost, lrt::fresh_optimum(four.get()));
    // Both graphs need as many labels: new ones were made only because the old ones were dropped.
    EXPECT_GT(kept->get_label_pool().get_nb_created_labels(), created);

    const auto again = two->solve(kept.get(), lrt::kInfinity, false);
    ASSERT_FALSE(again.solutions.empty());
    EXPECT_EQ(again.solutions.front().cost, first.solutions.front().cost);
    EXPECT_TRUE(kept->get_label_pool().check_ref_count_consistency());
}

/// @brief A start label starts from the default values, new or reused, not from its node's
///        values: the second solve, which reuses the first's start label, agrees with the first.
///
/// With a time limit of 8, the cheapest path from time 0 costs 10 (it takes 8); from the source's
/// seeded time 3, it would cost 20.
TEST(LabelRecycling, AReusedStartLabelStartsLikeANewOne) {
    namespace lrt = label_recycling_test;
    auto graph = lrt::layered_graph(/*time_limit=*/8.0,
                                    /*extra_components=*/0,
                                    /*source_time=*/3.0);
    rcspp::AlgorithmParams<rcspp::LabelList<rcspp::ResourceTypeComposition<RealResource>>> keep;
    keep.release_after_solve = false;
    auto kept = graph->create_algorithm<rcspp::strategy::Simple>(keep);

    const auto first = graph->solve(kept.get(), lrt::kInfinity, false);
    const auto second = graph->solve(kept.get(), lrt::kInfinity, false);
    EXPECT_GT(kept->get_label_pool().get_nb_reused_labels(), 0);
    lrt::expect_same_solutions(second, first);
    ASSERT_FALSE(first.solutions.empty());
    EXPECT_EQ(first.solutions.front().cost, 10.0);  // NOLINT(readability-magic-numbers)
    EXPECT_EQ(first.solutions.front().cost, lrt::fresh_optimum(graph.get()));
}

/// @brief max_retained_labels: a solve that ends with more labels releases them all; one that
///        ends with at most that many keeps them.
TEST(LabelRecycling, MaxRetainedLabelsBoundsWhatIsKept) {
    namespace lrt = label_recycling_test;
    auto graph = lrt::layered_graph(/*time_limit=*/6.0);
    rcspp::AlgorithmParams<rcspp::LabelList<rcspp::ResourceTypeComposition<RealResource>>> keep;
    keep.release_after_solve = false;

    keep.max_retained_labels = 2;
    auto capped = graph->create_algorithm<rcspp::strategy::Simple>(keep);
    const auto capped_result = graph->solve(capped.get(), lrt::kInfinity, false);
    ASSERT_FALSE(capped_result.solutions.empty());
    EXPECT_EQ(capped->get_label_pool().get_nb_total_labels(), 0U);

    keep.max_retained_labels = 1000;  // NOLINT(readability-magic-numbers)
    auto roomy = graph->create_algorithm<rcspp::strategy::Simple>(keep);
    const auto roomy_result = graph->solve(roomy.get(), lrt::kInfinity, false);
    lrt::expect_same_solutions(roomy_result, capped_result);
    EXPECT_GT(roomy->get_label_pool().get_nb_total_labels(), 2U);
}
