// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The ng unreachable fold on hand-built instances: it folds, it keeps the optimum, and it reads
// reachability over every path rather than the direct arc. The oracle sweep is in
// test_equivalence_ng.hpp.

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

namespace ng_unreachable_fold_test {

using Graph = rcspp::ResourceGraph<rcspp::RealResource, rcspp::SizeTBitsetResource>;
using Composed = rcspp::ResourceTypeComposition<rcspp::RealResource, rcspp::SizeTBitsetResource>;
using Params = rcspp::AlgorithmParams<rcspp::LabelList<Composed>>;

constexpr double kTolerance = 1e-9;

/// @brief An arc: origin, destination, cost, time.
struct ArcSpec {
        size_t origin;
        size_t destination;
        double cost;
        double time;
};

/// @brief A cost, a time window and an ng memory; node 0 is the source, the last node the sink.
struct Instance {
        size_t num_nodes = 0;
        std::map<size_t, std::pair<double, double>> windows;
        std::map<size_t, std::set<size_t>> neighbourhoods;
        std::vector<ArcSpec> arcs;
};

inline std::unique_ptr<Graph> build(const Instance& instance, bool fold) {
    using rcspp::RealResource;
    using rcspp::SizeTBitsetResource;
    auto graph = std::make_unique<Graph>();
    graph->add_resource<RealResource>(
        std::make_unique<rcspp::AdditionExtensionFunction<RealResource>>(),
        std::make_unique<rcspp::TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<rcspp::ValueCostFunction<RealResource>>(),
        std::make_unique<rcspp::ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<rcspp::TimeWindowExtensionFunction<RealResource>>(instance.windows),
        std::make_unique<rcspp::TimeWindowFeasibilityFunction<RealResource>>(instance.windows),
        std::make_unique<rcspp::TrivialCostFunction<RealResource>>(),
        std::make_unique<rcspp::ValueDominanceFunction<RealResource>>());
    rcspp::presets::add_ng_path_resource<SizeTBitsetResource>(*graph, instance.neighbourhoods);
    for (size_t node_id = 0; node_id < instance.num_nodes; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id + 1 == instance.num_nodes);
    }
    const std::set<size_t> no_arc_set;  // the ng extension reads the arc's endpoints
    for (const auto& arc : instance.arcs) {
        graph->add_arc<RealResource, RealResource, SizeTBitsetResource>(
            std::make_tuple(std::make_tuple(arc.cost),
                            std::make_tuple(arc.time),
                            std::make_tuple(no_arc_set)),
            arc.origin,
            arc.destination,
            arc.cost);
    }
    if (fold) {
        graph->enable_ng_unreachable_fold<SizeTBitsetResource>(0);
    }
    return graph;
}

/// @brief What one solve returned, and how much work it took.
struct Outcome {
        double best = std::numeric_limits<double>::infinity();
        std::vector<size_t> best_path;
        size_t extended = 0;
        size_t folds = 0;
};

inline Outcome solve(Graph* graph, Params params = Params{}) {
    auto algorithm = graph->create_algorithm<rcspp::SimpleDominanceAlgorithm>(std::move(params));
    const auto result = graph->solve(algorithm.get());
    Outcome outcome;
    if (!result.solutions.empty()) {
        outcome.best = result.solutions.front().cost;
        outcome.best_path = result.solutions.front().path_node_ids;
    }
    outcome.extended = algorithm->get_number_of_extended_labels();
    outcome.folds = result.unreachable_folds;
    return outcome;
}

/// @brief Six customers on a line, closing earlier the closer they are to the depot, with
///        reduced costs that make long routes pay; ng = the three nearest.
inline Instance tight_line() {
    Instance instance;
    instance.num_nodes = 8;  // 0 source, 1..6 customers at x = 1..6, 7 sink
    const auto x = [](size_t node) {
        return node == 0 || node == 7 ? 0.0 : static_cast<double>(node);
    };
    for (size_t node = 0; node < 8; ++node) {
        instance.windows[node] = {0.0, node == 0 || node == 7 ? 100.0 : 5.0 + x(node)};
    }
    for (size_t customer = 1; customer <= 6; ++customer) {
        std::set<size_t> near_ones;
        for (size_t other = 1; other <= 6; ++other) {
            if (other != customer && std::abs(x(other) - x(customer)) <= 1.5) {
                near_ones.insert(other);
            }
        }
        if (customer <= 2) {
            near_ones.insert(3);
        }
        if (customer >= 5) {
            near_ones.insert(4);
        }
        instance.neighbourhoods[customer] = near_ones;
    }
    const auto add = [&](size_t origin, size_t destination) {
        const double distance = std::abs(x(origin) - x(destination));
        const double dual = origin == 0 ? 0.0 : 10.0;
        instance.arcs.push_back({origin, destination, distance - dual, distance});
    };
    for (size_t customer = 1; customer <= 6; ++customer) {
        add(0, customer);
        add(customer, 7);
        for (size_t other = 1; other <= 6; ++other) {
            if (other != customer) {
                add(customer, other);
            }
        }
    }
    return instance;
}

/// @brief The direct arc a->x arrives too late, the detour a->b->x in time.
///
/// 0 source, a = 1, b = 2, x = 3 (closes at 5), 4 sink.
inline Instance detour() {
    Instance instance;
    instance.num_nodes = 5;
    for (size_t node = 0; node < 5; ++node) {
        instance.windows[node] = {0.0, node == 3 ? 5.0 : 100.0};
    }
    instance.neighbourhoods = {{1, {1, 2, 3}}, {2, {1, 2, 3}}, {3, {1, 2, 3}}};
    instance.arcs = {{0, 1, -10.0, 1.0},
                     {1, 2, -10.0, 1.0},
                     {2, 3, -10.0, 1.0},
                     {1, 3, -10.0, 10.0},
                     {3, 4, 0.0, 1.0},
                     {1, 4, 0.0, 1.0},
                     {2, 4, 0.0, 1.0}};
    return instance;
}

}  // namespace ng_unreachable_fold_test

/// @brief On tight windows the fold fires, keeps the optimum, and extends fewer labels.
TEST(NgUnreachableFold, TightWindowsFoldAndKeepTheOptimum) {
    namespace nf = ng_unreachable_fold_test;
    const auto instance = nf::tight_line();
    auto plain_graph = nf::build(instance, /*fold=*/false);
    auto folding_graph = nf::build(instance, /*fold=*/true);
    const auto plain = nf::solve(plain_graph.get());
    const auto folding = nf::solve(folding_graph.get());

    ASSERT_LT(plain.best, -20.0) << "a long route should be optimal";
    EXPECT_NEAR(folding.best, plain.best, nf::kTolerance);
    EXPECT_EQ(plain.folds, 0U);
    EXPECT_GT(folding.folds, 0U);
    EXPECT_LT(folding.extended, plain.extended);
}

/// @brief A node the direct arc reaches too late but a detour reaches in time is not folded.
TEST(NgUnreachableFold, ADetourKeepsANodeReachable) {
    namespace nf = ng_unreachable_fold_test;
    const auto instance = nf::detour();
    for (const bool fold : {false, true}) {
        auto graph = nf::build(instance, fold);
        const auto outcome = nf::solve(graph.get());
        EXPECT_NEAR(outcome.best, -30.0, nf::kTolerance) << "fold=" << fold;
        EXPECT_EQ(outcome.best_path, (std::vector<size_t>{0, 1, 2, 3, 4})) << "fold=" << fold;
    }
}

/// @brief A neighbour no path reaches from a node is folded there, and the optimum stays.
TEST(NgUnreachableFold, NoPathFolds) {
    namespace nf = ng_unreachable_fold_test;
    // 0 source, 1, 2, 3 sink. 2 reaches 1, but nothing leads from 1 to 2.
    nf::Instance instance;
    instance.num_nodes = 4;
    for (size_t node = 0; node < 4; ++node) {
        instance.windows[node] = {0.0, 100.0};
    }
    instance.neighbourhoods = {{1, {2}}, {2, {1}}};
    instance.arcs = {{0, 1, -1.0, 1.0},
                     {0, 2, -1.0, 1.0},
                     {2, 1, -1.0, 1.0},
                     {1, 3, -1.0, 1.0},
                     {2, 3, -1.0, 1.0}};
    auto plain_graph = nf::build(instance, /*fold=*/false);
    auto folding_graph = nf::build(instance, /*fold=*/true);
    const auto plain = nf::solve(plain_graph.get());
    const auto folding = nf::solve(folding_graph.get());

    EXPECT_NEAR(folding.best, plain.best, nf::kTolerance);
    EXPECT_NEAR(plain.best, -3.0, nf::kTolerance);
    EXPECT_GT(folding.folds, 0U);
}

/// @brief A negative time on one arc turns the time resource off for the fold.
TEST(NgUnreachableFold, NegativeConsumptionTurnsAThresholdOff) {
    namespace nf = ng_unreachable_fold_test;
    auto instance = nf::tight_line();
    instance.arcs.front().time = -1.0;
    auto plain_graph = nf::build(instance, /*fold=*/false);
    auto folding_graph = nf::build(instance, /*fold=*/true);
    const auto plain = nf::solve(plain_graph.get());
    const auto folding = nf::solve(folding_graph.get());

    EXPECT_EQ(folding.folds, 0U);
    EXPECT_NEAR(folding.best, plain.best, nf::kTolerance);
    EXPECT_EQ(folding.extended, plain.extended);
}

/// @brief Enabling the fold on a component that does not exist, or is not ng-path, throws.
TEST(NgUnreachableFold, RefusesAComponentThatIsNotNg) {
    namespace nf = ng_unreachable_fold_test;
    using rcspp::SizeTBitsetResource;
    auto graph = nf::build(nf::detour(), /*fold=*/false);
    EXPECT_THROW(graph->enable_ng_unreachable_fold<SizeTBitsetResource>(1), std::out_of_range);
    EXPECT_FALSE(graph->is_ng_unreachable_fold_enabled());

    nf::Graph visited;
    visited.add_resource<rcspp::RealResource>(
        std::make_unique<rcspp::AdditionExtensionFunction<rcspp::RealResource>>(),
        std::make_unique<rcspp::TrivialFeasibilityFunction<rcspp::RealResource>>(),
        std::make_unique<rcspp::ValueCostFunction<rcspp::RealResource>>(),
        std::make_unique<rcspp::ValueDominanceFunction<rcspp::RealResource>>());
    visited.add_resource<SizeTBitsetResource>(
        std::make_unique<rcspp::UnionExtensionFunction<SizeTBitsetResource>>(),
        std::make_unique<rcspp::TrivialFeasibilityFunction<SizeTBitsetResource>>(),
        std::make_unique<rcspp::TrivialCostFunction<SizeTBitsetResource>>(),
        std::make_unique<rcspp::InclusionDominanceFunction<SizeTBitsetResource>>());
    EXPECT_THROW(visited.enable_ng_unreachable_fold<SizeTBitsetResource>(0), std::invalid_argument);
    EXPECT_FALSE(visited.is_ng_unreachable_fold_enabled());
}

/// @brief After the neighbourhoods are replaced, the fold reads the new ones.
TEST(NgUnreachableFold, FollowsNewNeighbourhoods) {
    namespace nf = ng_unreachable_fold_test;
    const auto instance = nf::tight_line();
    std::map<size_t, std::set<size_t>> empty;
    for (const auto& [node, members] : instance.neighbourhoods) {
        empty[node] = {};
    }
    auto folding_graph = nf::build(instance, /*fold=*/true);
    rcspp::set_ng_neighborhoods<rcspp::SizeTBitsetResource>(*folding_graph, 0, empty);
    auto plain_graph = nf::build(instance, /*fold=*/false);
    rcspp::set_ng_neighborhoods<rcspp::SizeTBitsetResource>(*plain_graph, 0, empty);

    const auto folding = nf::solve(folding_graph.get());
    const auto plain = nf::solve(plain_graph.get());
    EXPECT_EQ(folding.folds, 0U);
    EXPECT_NEAR(folding.best, plain.best, nf::kTolerance);
    EXPECT_EQ(folding.extended, plain.extended);
}

/// @brief A cloned graph folds as the original does.
TEST(NgUnreachableFold, CloneKeepsTheSetting) {
    namespace nf = ng_unreachable_fold_test;
    auto graph = nf::build(nf::tight_line(), /*fold=*/true);
    auto clone = graph->clone();
    ASSERT_TRUE(clone->is_ng_unreachable_fold_enabled());

    const auto original = nf::solve(graph.get());
    const auto cloned = nf::solve(clone.get());
    EXPECT_NEAR(cloned.best, original.best, nf::kTolerance);
    EXPECT_EQ(cloned.folds, original.folds);
    EXPECT_EQ(cloned.extended, original.extended);
}

/// @brief Disabling the fold restores the plain solve.
TEST(NgUnreachableFold, DisableRestoresTheBaseline) {
    namespace nf = ng_unreachable_fold_test;
    auto graph = nf::build(nf::tight_line(), /*fold=*/true);
    graph->disable_ng_unreachable_fold();
    EXPECT_FALSE(graph->is_ng_unreachable_fold_enabled());
    auto plain_graph = nf::build(nf::tight_line(), /*fold=*/false);

    const auto disabled = nf::solve(graph.get());
    const auto plain = nf::solve(plain_graph.get());
    EXPECT_EQ(disabled.folds, 0U);
    EXPECT_EQ(disabled.extended, plain.extended);
    EXPECT_NEAR(disabled.best, plain.best, nf::kTolerance);
}

/// @brief A backward search is not folded.
TEST(NgUnreachableFold, BackwardSearchIsUntouched) {
    namespace nf = ng_unreachable_fold_test;
    nf::Params params;
    params.direction = rcspp::SearchDirection::Backward;
    auto folding_graph = nf::build(nf::tight_line(), /*fold=*/true);
    auto plain_graph = nf::build(nf::tight_line(), /*fold=*/false);

    const auto folding = nf::solve(folding_graph.get(), params);
    const auto plain = nf::solve(plain_graph.get(), params);
    EXPECT_EQ(folding.folds, 0U);
    EXPECT_EQ(folding.extended, plain.extended);
    EXPECT_NEAR(folding.best, plain.best, nf::kTolerance);
}
