// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Validation tests for the bidirectional algorithm: which mechanism produces a path (join vs.
// terminal collection), agreement with a reverse-graph oracle on additive models, setup refusals,
// and backward `LabelBuckets` matching `LabelList`.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/equivalence_helpers.hpp"
#include "util/random_instance.hpp"
#include "util/reverse_graph_oracle.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace bidirectional_validation_test {

using Composed = ResourceTypeComposition<RealResource>;

constexpr double kTolerance = 1e-9;

/// @brief A cost slot plus a time-window clock, the shape a bidirectional solve wants.
inline std::unique_ptr<ResourceGraph<RealResource>> clocked_graph(
    const std::map<size_t, std::pair<double, double>>& windows,
    const std::vector<std::tuple<double, double, size_t, size_t>>& arcs) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());

    const size_t last = windows.empty() ? 0 : windows.rbegin()->first;
    for (const auto& [node_id, window] : windows) {
        graph->add_node(node_id, node_id == 0, node_id == last);
    }
    for (const auto& [cost, time, origin, destination] : arcs) {
        graph->add_arc<RealResource, RealResource>({cost, time}, origin, destination, cost);
    }
    return graph;
}

/// @brief Params pointing the clock at slot 1.
inline AlgorithmParams<LabelList<Composed>> clocked_params(double half_way_point) {
    AlgorithmParams<LabelList<Composed>> params;
    params.critical_resource_index = 1;
    params.half_way_point = half_way_point;
    return params;
}

/// @brief The three-way agreement instances for the oracle: additive only, by construction.
inline std::vector<test_util::AdditiveInstance> oracle_instances() {
    std::vector<test_util::AdditiveInstance> instances;

    // A diamond with a detour, so there is a real choice and more than one path.
    instances.push_back({.num_nodes = 5,
                         .sources = {0},
                         .sinks = {4},
                         .arcs = {{0, 1, 3.0, 1.0},
                                  {0, 2, 1.0, 3.0},
                                  {1, 3, 2.0, 1.0},
                                  {2, 3, 1.0, 2.0},
                                  {3, 4, 1.0, 1.0},
                                  {1, 4, 9.0, 1.0}},
                         .capacity = 0.0});

    // The same, with a load that binds: the cheap route is too heavy.
    instances.push_back({.num_nodes = 5,
                         .sources = {0},
                         .sinks = {4},
                         .arcs = {{0, 1, 3.0, 1.0},
                                  {0, 2, 1.0, 3.0},
                                  {1, 3, 2.0, 1.0},
                                  {2, 3, 1.0, 2.0},
                                  {3, 4, 1.0, 1.0}},
                         .capacity = 4.0});

    // Negative costs, which is what a reduced cost looks like during pricing.
    instances.push_back({.num_nodes = 6,
                         .sources = {0},
                         .sinks = {5},
                         .arcs = {{0, 1, 4.0, 1.0},
                                  {1, 2, -6.0, 1.0},
                                  {0, 2, 1.0, 1.0},
                                  {2, 3, 2.0, 1.0},
                                  {3, 5, 1.0, 1.0},
                                  {2, 4, 1.0, 1.0},
                                  {4, 5, 3.0, 1.0}},
                         .capacity = 0.0});

    return instances;
}

/// @brief The best cost of @p result, or infinity when nothing was found.
inline double best_cost(const SolveResult& result) {
    return result.solutions.empty() ? std::numeric_limits<double>::infinity()
                                    : result.solutions.front().cost;
}

/// @brief A one-component composition label carrying @p value, with a reversing dominance function.
inline std::unique_ptr<Label<Composed>> threshold_label(size_t label_id, double value) {
    auto dominance = std::make_unique<ValueDominanceFunction<RealResource>>();
    dominance->set_backward_reversed(true);
    auto component = std::make_unique<Resource<RealResource>>(
        RealResource(value),
        std::move(dominance),
        std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<TrivialCostFunction<RealResource>>());

    std::tuple<std::vector<std::unique_ptr<Resource<RealResource>>>> components;
    std::get<0>(components).push_back(std::move(component));

    auto resource = std::make_unique<Resource<Composed>>(
        std::move(components),
        std::make_unique<CompositionDominanceFunction<RealResource>>(),
        std::make_unique<CompositionFeasibilityFunction<RealResource>>(),
        std::make_unique<CompositionCostFunction<RealResource>>(),
        0);
    return std::make_unique<Label<Composed>>(label_id, std::move(resource));
}

/// @brief A two-component label: a *reversing* deadline in slot 0 and a plain cost in slot 1.
///
/// Two components let `LabelBuckets` bucket on one slot and sort on the other.
///
/// @param label_id The label's id, used to compare survivor sets.
/// @param deadline Slot 0: backward dominance reversed.
/// @param cost     Slot 1: dominance not reversed.
inline std::unique_ptr<Label<Composed>> deadline_and_cost_label(size_t label_id, double deadline,
                                                                double cost) {
    const auto make_component = [](double value, bool reversed) {
        auto dominance = std::make_unique<ValueDominanceFunction<RealResource>>();
        dominance->set_backward_reversed(reversed);
        return std::make_unique<Resource<RealResource>>(
            RealResource(value),
            std::move(dominance),
            std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
            std::make_unique<TrivialCostFunction<RealResource>>());
    };

    std::tuple<std::vector<std::unique_ptr<Resource<RealResource>>>> components;
    std::get<0>(components).push_back(make_component(deadline, /*reversed=*/true));
    std::get<0>(components).push_back(make_component(cost, /*reversed=*/false));

    auto resource = std::make_unique<Resource<Composed>>(
        std::move(components),
        std::make_unique<CompositionDominanceFunction<RealResource>>(),
        std::make_unique<CompositionFeasibilityFunction<RealResource>>(),
        std::make_unique<CompositionCostFunction<RealResource>>(),
        0);
    return std::make_unique<Label<Composed>>(label_id, std::move(resource));
}

/// @brief Inserts @p label the way the search does: skip if dominated, else evict and add.
///
/// `add_label` alone only appends, so dominance would never be exercised.
template <typename Container, typename LabelType>
inline void insert_non_dominated(Container* container, LabelType* label) {
    if (container->is_dominated(*label)) {
        return;
    }
    container->remove_dominated_labels(*label);
    container->add_label(label);
}

/// @brief The ids surviving in a container, sorted so two containers can be compared as sets.
template <typename Container>
inline std::vector<size_t> surviving_ids(const Container& container) {
    std::vector<size_t> ids;
    for (const auto* label : container.get_labels()) {
        ids.push_back(label->id);
    }
    std::ranges::sort(ids);
    return ids;
}

}  // namespace bidirectional_validation_test

// ============================================================================
// The short path, and its mechanism
// ============================================================================

/// @brief A route whose clock never reaches H is found, and the join is not what found it.
///
/// The key assertion is `number_of_joined_paths() == 0`: the path must come from a search
/// reaching a terminal.
TEST(BidirectionalValidation, ShortPathIsFoundByTerminalCollectionNotByTheJoin) {
    namespace bv = bidirectional_validation_test;

    const std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 1000.0}},
                                                              {1, {0.0, 1000.0}},
                                                              {2, {0.0, 1000.0}}};
    auto graph = bv::clocked_graph(windows, {{1.0, 1.0, 0, 1}, {1.0, 1.0, 1, 2}});

    // Total clock along the only route is 2, against H = 50: no arc crosses the middle.
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bv::clocked_params(50.0));
    const auto result = graph->solve(algorithm.get());

    ASSERT_TRUE(algorithm->bounded_by_half_way())
        << "with the bound off this proves nothing: every pair would be joined";
    ASSERT_FALSE(result.solutions.empty())
        << "a route that crosses H on no arc must still be found";
    EXPECT_NEAR(result.solutions.front().cost, 2.0, bv::kTolerance);
    EXPECT_EQ(algorithm->number_of_joined_paths(), 0U)
        << "the join cannot produce this path; it must have come from a search reaching a terminal";
}

/// @brief On a route that does cross H, the join is the mechanism.
///
/// Complements the test above by showing the join count is not always zero.
TEST(BidirectionalValidation, ALongRouteIsJoinedOnItsCrossingArc) {
    namespace bv = bidirectional_validation_test;

    // Tight windows (closing at each node's earliest arrival) stop the backward search before the
    // source, so only the join can produce the path.
    const std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 0.0}},
                                                              {1, {0.0, 10.0}},
                                                              {2, {0.0, 20.0}},
                                                              {3, {0.0, 30.0}},
                                                              {4, {0.0, 40.0}}};
    auto graph = bv::clocked_graph(
        windows,
        {{1.0, 10.0, 0, 1}, {1.0, 10.0, 1, 2}, {1.0, 10.0, 2, 3}, {1.0, 10.0, 3, 4}});

    // Clock runs 0, 10, 20, 30, 40 and H = 20, so the crossing arc is 2 -> 3. Forward stops
    // extending at node 3 and backward stops at node 2, so neither reaches a terminal.
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bv::clocked_params(20.0));
    const auto result = graph->solve(algorithm.get());

    ASSERT_TRUE(algorithm->bounded_by_half_way());
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions.front().cost, 4.0, bv::kTolerance);
    EXPECT_GT(algorithm->number_of_joined_paths(), 0U)
        << "a route whose clock straddles H must be produced by the join";
}

/// @brief A path that crosses `H` on its last arc is the forward search's, not the join's.
///
/// The join must not pair the forward label at the sink with the sink's seed and count the
/// already-recorded path as joined.
TEST(BidirectionalValidation, APathThatCrossesHOnItsLastArcIsNotJoined) {
    namespace bv = bidirectional_validation_test;
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<BudgetExtensionFunction<RealResource>>(100.0),
        std::make_unique<MinMaxFeasibilityFunction<RealResource>>(
            0.0,
            100.0,
            /*merge_by_increasing_value=*/true),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2, /*source=*/false, /*sink=*/true);
    // The clock reads 0, 1, 11, so with H = 5 it crosses on the arc into the sink. A finite upper
    // bound keeps the join from pruning the duplicate against the incumbent.
    graph->add_arc<RealResource, RealResource>({1.0, 1.0}, 0, 1, 1.0);
    graph->add_arc<RealResource, RealResource>({1.0, 10.0}, 1, 2, 1.0);

    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bv::clocked_params(5.0));
    const auto result = graph->solve(algorithm.get(), /*upper_bound=*/100.0, /*preprocess=*/false);

    ASSERT_TRUE(algorithm->bounded_by_half_way());
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions.front().cost, 2.0, bv::kTolerance);
    EXPECT_EQ(algorithm->number_of_joined_paths(), 0U)
        << "the forward search recorded this path at the sink; the join must not count it again";
}

// ============================================================================
// The join is not owed after a run-level stop
// ============================================================================

namespace bidirectional_validation_test {

/// @brief A 5-node chain whose optimum can only come from the join (with H = 20).
inline std::unique_ptr<ResourceGraph<RealResource>> join_only_graph() {
    const std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 0.0}},
                                                              {1, {0.0, 10.0}},
                                                              {2, {0.0, 20.0}},
                                                              {3, {0.0, 30.0}},
                                                              {4, {0.0, 40.0}}};
    return clocked_graph(
        windows,
        {{1.0, 10.0, 0, 1}, {1.0, 10.0, 1, 2}, {1.0, 10.0, 2, 3}, {1.0, 10.0, 3, 4}});
}

}  // namespace bidirectional_validation_test

/// @brief An interrupted solve does not then run the join.
///
/// The join is expensive and should not run after a timeout, stop callback or memory limit. The
/// stop callback is used here since it is deterministic and reaches the same guard.
TEST(BidirectionalValidation, AnInterruptedSolveSkipsTheJoin) {
    namespace bv = bidirectional_validation_test;

    // Control: uninterrupted, the optimum comes from the join.
    {
        auto graph = bv::join_only_graph();
        auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
            bv::clocked_params(20.0));
        const SolveResult result = graph->solve(algorithm.get());
        ASSERT_EQ(result.status, AlgorithmStatus::COMPLETE);
        ASSERT_GT(result.number_of_joined_paths, 0U)
            << "the control must join, or the interrupted run below proves nothing";
    }

    // Interrupted after a couple of iterations, with labels still on both frontiers.
    auto graph = bv::join_only_graph();
    size_t calls = 0;
    auto params = bv::clocked_params(20.0);
    params.should_stop = [&calls]() { return ++calls > 2; };
    params.release_after_solve = false;  // so the pool still holds what the check reads

    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    const SolveResult result = graph->solve(algorithm.get());

    ASSERT_EQ(result.status, AlgorithmStatus::INTERRUPTED)
        << "the callback did not stop the search, so the guard was never reached";
    EXPECT_EQ(result.number_of_joined_paths, 0U) << "the join ran after the solve was interrupted";
    EXPECT_GT(algorithm->get_label_pool().get_nb_total_labels(), 0U);
    EXPECT_TRUE(algorithm->get_label_pool().check_ref_count_consistency())
        << "skipping the join must not change what the pool owns";
}

/// @brief `stop_after_X_solutions` deliberately does NOT skip the join.
///
/// A solution budget caps what is returned, not the search, so skipping the join would change the
/// answer.
TEST(BidirectionalValidation, ASolutionBudgetStillRunsTheJoin) {
    namespace bv = bidirectional_validation_test;

    auto graph = bv::join_only_graph();
    auto params = bv::clocked_params(20.0);
    params.stop_after_X_solutions = 1;

    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    const SolveResult result = graph->solve(algorithm.get());

    EXPECT_GT(result.number_of_joined_paths, 0U)
        << "a solution budget caps the returned set, not the search";
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions.front().cost, 4.0, bv::kTolerance);
}

/// @brief A pair budget truncates the join, says so, and marks the params as inexact.
TEST(BidirectionalValidation, APairBudgetTruncatesTheJoinAndFlagsIt) {
    namespace bv = bidirectional_validation_test;

    // Control: no cap. The join tests pairs and is not truncated.
    {
        auto graph = bv::join_only_graph();
        auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
            bv::clocked_params(20.0));
        const SolveResult result = graph->solve(algorithm.get());
        ASSERT_GT(result.number_of_joined_paths, 0U);
        EXPECT_GT(result.join_pairs_tested, 0U);
        EXPECT_FALSE(result.join_truncated);
    }

    auto graph = bv::join_only_graph();
    auto params = bv::clocked_params(20.0);
    params.max_join_pairs = 0;
    EXPECT_TRUE(params.could_be_non_optimal());

    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    const SolveResult result = graph->solve(algorithm.get());

    EXPECT_EQ(result.status, AlgorithmStatus::COMPLETE) << "the search itself was exhaustive";
    EXPECT_TRUE(result.join_truncated);
    EXPECT_EQ(result.join_pairs_tested, 0U);
    EXPECT_EQ(result.number_of_joined_paths, 0U) << "the only path here comes from the join";
}

/// @brief The joiner never rejects a half on the half's own cost.
///
/// With negative reduced costs a half's own cost says nothing about its completion. A direct
/// route sets the incumbent (-10); the good route's forward half costs +30 and completes to -70,
/// which only the join can produce.
TEST(BidirectionalValidation, TheJoinerDoesNotPruneAHalfOnItsOwnCost) {
    namespace bv = bidirectional_validation_test;

    // Tight windows stop the backward search short of the source.
    const std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 0.0}},
                                                              {1, {0.0, 15.0}},
                                                              {2, {0.0, 30.0}},
                                                              {3, {0.0, 45.0}}};

    auto graph = bv::clocked_graph(windows,
                                   {{-10.0, 1.0, 0, 3},  // the cheap direct route: incumbent -10
                                    {30.0, 15.0, 0, 1},  // the good route's expensive first half
                                    {0.0, 15.0, 1, 2},
                                    {-100.0, 15.0, 2, 3}});  // ... which completes to -70

    // Forward keeps labels to t = 20, so it stops at node 2 (t = 30); backward keeps deadlines from
    // 20 up, so it stops at node 1 (deadline 15). The crossing arc is 1 -> 2.
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bv::clocked_params(20.0));
    const auto result = graph->solve(algorithm.get());

    ASSERT_TRUE(algorithm->bounded_by_half_way());
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_NEAR(result.solutions.front().cost, -70.0, bv::kTolerance)
        << "a forward half costing 30 completes to -70; pruning it against an incumbent of -10 "
           "loses the optimum";
    EXPECT_GT(algorithm->number_of_joined_paths(), 0U)
        << "the optimum here has to come from the join, or the test is not exercising it";
}

// ============================================================================
// The reverse-graph oracle
// ============================================================================

/// @brief Three independent implementations agree on additive instances.
///
/// Forward, forward on the reversed graph, and bidirectional. The reversed-graph oracle shares no
/// backward code with the library. It is only valid on additive models.
TEST(BidirectionalValidation, ReverseGraphOracleAgreesOnAdditiveInstances) {
    namespace bv = bidirectional_validation_test;

    size_t instance_index = 0;
    for (const auto& instance : bv::oracle_instances()) {
        SCOPED_TRACE("oracle instance " + std::to_string(instance_index++));

        auto forward = test_util::build_additive_graph(instance, /*reversed=*/false);
        const double reference =
            bv::best_cost(forward->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}));

        auto reversed = test_util::build_additive_graph(instance, /*reversed=*/true);
        const double oracle =
            bv::best_cost(reversed->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}));

        auto candidate_graph = test_util::build_additive_graph(instance, /*reversed=*/false);
        AlgorithmParams<LabelList<bv::Composed>> params;
        params.release_after_solve = false;  // so the pool still holds what the check reads
        // Everything accumulates, so there is no clock and the bound switches itself off.
        auto algorithm =
            candidate_graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
        const double candidate = bv::best_cost(candidate_graph->solve(algorithm.get()));

        EXPECT_NEAR(oracle, reference, bv::kTolerance) << "the reversed graph disagrees";
        EXPECT_NEAR(candidate, reference, bv::kTolerance) << "bidirectional disagrees";
        EXPECT_FALSE(algorithm->bounded_by_half_way())
            << "an additive-only model has no clock, so the bound must be off";
        EXPECT_GT(algorithm->get_label_pool().get_nb_total_labels(), 0U);
        EXPECT_TRUE(algorithm->get_label_pool().check_ref_count_consistency());
    }
}

// ============================================================================
// An accumulating capacity joins on the sum, not on a comparison
// ============================================================================

/// @brief A bidirectional solve never splices two halves whose loads together break the cap.
///
/// With an accumulating load, the backward label holds the suffix load, so the join must check
/// prefix + suffix <= cap rather than compare the two. Node 5 keeps preprocessing from deleting
/// arc 2->3. Feasible optimum is 0-1-3-4 (cost 6); 0-2-3-4 costs 3 but carries 6 > 4.
TEST(BidirectionalValidation, AccumulatingCapacityJoinsOnlyWithinTheCap) {
    namespace bv = bidirectional_validation_test;

    constexpr double kCapacity = 4.0;
    // {cost, load, origin, destination}
    const std::vector<std::tuple<double, double, size_t, size_t>> arcs{{3.0, 1.0, 0, 1},
                                                                       {1.0, 3.0, 0, 2},
                                                                       {2.0, 1.0, 1, 3},
                                                                       {1.0, 2.0, 2, 3},
                                                                       {1.0, 1.0, 3, 4},
                                                                       {5.0, 0.0, 0, 5},
                                                                       {5.0, 0.0, 5, 2}};

    auto build = [&]() {
        auto graph = std::make_unique<ResourceGraph<RealResource>>();
        graph->add_resource<RealResource>(
            std::make_unique<AdditionExtensionFunction<RealResource>>(),
            std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
            std::make_unique<ValueCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
        // An accumulating load with a cap. merge_by_increasing_value = false seeds the backward
        // label at the minimum so it accumulates upward.
        graph->add_resource<RealResource>(
            std::make_unique<AdditionExtensionFunction<RealResource>>(),
            std::make_unique<MinMaxFeasibilityFunction<RealResource>>(
                0.0,
                kCapacity,
                /*merge_by_increasing_value=*/false),
            std::make_unique<TrivialCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
        for (size_t node_id = 0; node_id < 6; ++node_id) {
            graph->add_node(node_id, node_id == 0, node_id == 4);
        }
        for (const auto& [cost, load, origin, destination] : arcs) {
            graph->add_arc<RealResource, RealResource>({cost, load}, origin, destination, cost);
        }
        return graph;
    };

    auto forward_graph = build();
    const double reference =
        bv::best_cost(forward_graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}));
    ASSERT_NEAR(reference, 6.0, bv::kTolerance) << "the forward search should find 0-1-3-4";

    auto candidate_graph = build();
    AlgorithmParams<LabelList<bv::Composed>> params;
    auto algorithm =
        candidate_graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    const SolveResult result = candidate_graph->solve(algorithm.get());

    EXPECT_NEAR(bv::best_cost(result), reference, bv::kTolerance)
        << "bidirectional returned a cheaper path than the forward search, which on this graph "
           "means it spliced two halves whose loads break the cap";

    // Every returned path must fit under the cap.
    for (const auto& solution : result.solutions) {
        double load = 0.0;
        for (const size_t arc_id : solution.path_arc_ids) {
            load += std::get<1>(arcs.at(arc_id));
        }
        EXPECT_LE(load, kCapacity + bv::kTolerance)
            << "a returned path carries " << load << " against a capacity of " << kCapacity;
    }
}

/// @brief A feasibility function that declares a bound's merge rule against an accumulation is
///        refused at setup, rather than answering with it.
///
/// Guards any feasibility function that declares `DominanceOrder` without consulting its
/// extension function.
TEST(BidirectionalValidation, AccumulatingExtensionWithADominanceOrderMergeIsRefused) {
    namespace bv = bidirectional_validation_test;

    /// A bound's merge rule on an accumulating resource.
    class BoundRuleOnAnAccumulation
        : public Clonable<BoundRuleOnAnAccumulation, FeasibilityFunction<RealResource>> {
        public:
            auto is_feasible(const RealResource& resource) -> bool override {
                return resource.leq(10.0);
            }
            [[nodiscard]] MergeRule merge_rule() const override {
                return MergeRule::DominanceOrder;
            }
    };

    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<BoundRuleOnAnAccumulation>(),
                                      std::make_unique<TrivialCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource, RealResource>({1.0, 1.0}, 0, 1, 1.0);

    AlgorithmParams<LabelList<bv::Composed>> params;
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    EXPECT_THROW(graph->solve(algorithm.get()), std::runtime_error);
}

// ============================================================================
// Backward LabelBuckets
// ============================================================================

/// @brief A bucketed backward container keeps exactly the labels a list keeps -- in both
///        bucket/sort configurations, and on a set where several labels survive.
///
/// The labels form a Pareto front under backward dominance (later deadline and lower cost are
/// better), so bucket-boundary and sort-resource comparisons are all exercised.
TEST(BidirectionalValidation, BucketedBackwardContainerMatchesTheList) {
    namespace bv = bidirectional_validation_test;

    // {deadline, cost}. The first three are mutually non-dominated backwards (a later deadline
    // costs more); the fourth is beaten on both by {70, 3} and must be removed by both containers.
    const std::vector<std::pair<double, double>> values{{90.0, 5.0},
                                                        {70.0, 3.0},
                                                        {50.0, 1.0},
                                                        {60.0, 4.0}};

    const auto compare = [&](size_t bucket_index, size_t sort_index, const char* what) {
        using Buckets = LabelBuckets<RealResource, RealResource, bv::Composed, BackwardDirection>;

        std::vector<std::unique_ptr<Label<bv::Composed>>> for_list;
        std::vector<std::unique_ptr<Label<bv::Composed>>> for_buckets;
        for (size_t i = 0; i < values.size(); ++i) {
            for_list.push_back(bv::deadline_and_cost_label(i, values[i].first, values[i].second));
            for_buckets.push_back(
                bv::deadline_and_cost_label(i, values[i].first, values[i].second));
        }

        LabelList<bv::Composed, BackwardDirection> list;
        Buckets buckets(/*range_buckets=*/20, bucket_index, sort_index);
        for (size_t i = 0; i < values.size(); ++i) {
            bv::insert_non_dominated(&list, for_list[i].get());
            bv::insert_non_dominated(&buckets, for_buckets[i].get());
        }

        const auto from_list = bv::surviving_ids(list);
        EXPECT_EQ(bv::surviving_ids(buckets), from_list)
            << "bucketed and list backward containers disagree with " << what;

        // The list itself keeps the right labels, so the two cannot be wrong together.
        EXPECT_EQ(from_list, (std::vector<size_t>{0, 1, 2}))
            << "the Pareto front is {90,5}, {70,3}, {50,1}; {60,4} is beaten on both: " << what;
    };

    // The realistic configuration: bucket on the clock, sort on the cost.
    compare(/*bucket_index=*/0, /*sort_index=*/1, "bucket = deadline, sort = cost");
    // Sort-resource comparisons on a reversed resource.
    compare(/*bucket_index=*/1, /*sort_index=*/0, "bucket = cost, sort = deadline");
}

// ============================================================================
// Refused at setup, not solved wrongly
// ============================================================================

namespace bidirectional_validation_test {

/// @brief A composition dominance function written for forward-only use: cost alone decides.
///
/// Overrides only `check_dominance`; must keep compiling, so `check_back_dominance` is not pure.
class CostOnlyDominance : public Clonable<CostOnlyDominance, DominanceFunction<Composed>> {
    public:
        [[nodiscard]] auto check_dominance(const Resource<Composed>& lhs_resource,
                                           const Resource<Composed>& rhs_resource)
            -> bool override {
            return lhs_resource.get_cost() <= rhs_resource.get_cost();
        }
};

/// @brief Runs a bidirectional solve that has to be refused, and returns what it said.
inline std::string refusal(ResourceGraph<RealResource>* graph,
                           const AlgorithmParams<LabelList<Composed>>& params,
                           bool preprocess = true) {
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    try {
        static_cast<void>(
            graph->solve(algorithm.get(), std::numeric_limits<double>::infinity(), preprocess));
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    return {};
}

}  // namespace bidirectional_validation_test

/// @brief A forward-only composition dominance function compiles, solves forward, and is refused
///        backward with the override it lacks named.
TEST(BidirectionalValidation, AForwardOnlyCompositionDominanceIsRefusedAtSetup) {
    namespace bv = bidirectional_validation_test;
    const auto build = [] {
        auto graph = std::make_unique<ResourceGraph<RealResource>>(
            std::make_unique<CompositionExtensionFunction<RealResource>>(),
            std::make_unique<CompositionFeasibilityFunction<RealResource>>(),
            std::make_unique<ComponentCostFunction<0, RealResource>>(0),
            std::make_unique<bv::CostOnlyDominance>());
        graph->add_resource<RealResource>(
            std::make_unique<AdditionExtensionFunction<RealResource>>(),
            std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
            std::make_unique<ValueCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
        graph->add_node(0, /*source=*/true, /*sink=*/false);
        graph->add_node(1, /*source=*/false, /*sink=*/true);
        graph->add_arc<RealResource>(std::make_tuple(2.0), 0, 1, 2.0);
        return graph;
    };

    const auto forward = build()->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    ASSERT_FALSE(forward.solutions.empty());
    EXPECT_NEAR(forward.solutions.front().cost, 2.0, bv::kTolerance);

    auto graph = build();
    const std::string message =
        bv::refusal(graph.get(), AlgorithmParams<LabelList<bv::Composed>>{});
    EXPECT_NE(message.find("check_back_dominance"), std::string::npos) << message;
}

/// @brief A floor on a threshold resource is refused rather than joined past.
///
/// A backward label carries only a ceiling, so it cannot check the floor of 5 that makes the only
/// path infeasible. Preprocessing is off because it would delete the arc and hide the case.
TEST(BidirectionalValidation, AFloorOnAThresholdResourceIsRefused) {
    namespace bv = bidirectional_validation_test;
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<BudgetExtensionFunction<RealResource>>(100.0),
        std::make_unique<MinMaxFeasibilityFunction<RealResource>>(
            5.0,
            100.0,
            /*merge_by_increasing_value=*/true),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource, RealResource>({-5.0, 1.0}, 0, 1, -5.0);
    graph->add_arc<RealResource, RealResource>({-5.0, 1.0}, 1, 2, -5.0);

    const auto forward =
        graph->solve<SimpleDominanceAlgorithm>(std::numeric_limits<double>::infinity(),
                                               AlgorithmParams<LabelList<bv::Composed>>{},
                                               /*preprocess=*/false);
    EXPECT_TRUE(forward.solutions.empty()) << "node 1 is below the floor on the only path";

    const std::string message =
        bv::refusal(graph.get(), bv::clocked_params(50.0), /*preprocess=*/false);
    EXPECT_NE(message.find("component 1"), std::string::npos) << message;
    EXPECT_NE(message.find("merge_rule"), std::string::npos) << message;
}

// ============================================================================
// A throwing solve leaves the caller's graph whole
// ============================================================================

namespace bidirectional_validation_test {

/// @brief A cap that declares no merge rule, so a bidirectional solve refuses it.
class CapWithoutAMergeRule
    : public Clonable<CapWithoutAMergeRule, FeasibilityFunction<RealResource>> {
    public:
        auto is_feasible(const RealResource& resource) -> bool override {
            return resource.leq(100.0);
        }
};

/// @brief An addition that throws mid-path once armed, standing in for any user exception.
class ThrowingAddition : public Clonable<ThrowingAddition, ExtensionFunction<RealResource>> {
    public:
        static inline bool armed = false;
        void extend(const RealResource& resource, const RealResource& extender_value,
                    RealResource* extended_resource) override {
            if (armed && resource.get_value() > 0.5) {
                throw std::runtime_error("user extension failed");
            }
            extended_resource->set_value(resource.get_value() + extender_value.get_value());
        }
        [[nodiscard]] BackwardKind backward_kind() const override {
            return BackwardKind::Accumulate;
        }
};

/// @brief Two routes, 0-1-3 (cost 2) and 0-2-3 (cost 10), so a bound of 5 removes two arcs.
inline std::unique_ptr<ResourceGraph<RealResource>> two_route_graph(
    std::unique_ptr<ExtensionFunction<RealResource>> extension,
    std::unique_ptr<FeasibilityFunction<RealResource>> feasibility) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(std::move(extension),
                                      std::move(feasibility),
                                      std::make_unique<TrivialCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    for (size_t node_id = 0; node_id < 4; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id == 3);
    }
    graph->add_arc<RealResource, RealResource>({1.0, 1.0}, 0, 1, 1.0);
    graph->add_arc<RealResource, RealResource>({1.0, 1.0}, 1, 3, 1.0);
    // Lighter, so 0-2-3 is not dominated by 0-1-3.
    graph->add_arc<RealResource, RealResource>({5.0, 0.0}, 0, 2, 5.0);
    graph->add_arc<RealResource, RealResource>({5.0, 1.0}, 2, 3, 5.0);
    return graph;
}

}  // namespace bidirectional_validation_test

/// @brief A refused bidirectional solve restores the arcs preprocessing removed, so a fallback
///        forward solve prices on the whole graph.
TEST(BidirectionalValidation, ArcsSurviveARefusedBidirectionalSolve) {
    namespace bv = bidirectional_validation_test;
    auto graph = bv::two_route_graph(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                     std::make_unique<bv::CapWithoutAMergeRule>());
    ASSERT_EQ(graph->get_arcs_size(), 4U);

    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bv::clocked_params(1.0));
    EXPECT_THROW(static_cast<void>(graph->solve(algorithm.get(), /*upper_bound=*/5.0)),
                 std::runtime_error);
    EXPECT_EQ(graph->get_arcs_size(), 4U) << "the refusal deleted arcs from the caller's graph";

    const auto fallback = graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    EXPECT_EQ(fallback.solutions.size(), 2U);
}

/// @brief The same for a user function that throws mid-labelling, on a forward solve.
TEST(BidirectionalValidation, ArcsSurviveAUserExceptionDuringASolve) {
    namespace bv = bidirectional_validation_test;
    auto graph =
        bv::two_route_graph(std::make_unique<bv::ThrowingAddition>(),
                            std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 100.0));
    // A healthy first solve runs the one-off feasibility preprocessing, so the throw below comes
    // from the labelling, after the shortest-path preprocessor removed arcs.
    static_cast<void>(graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}));
    ASSERT_EQ(graph->get_arcs_size(), 4U);

    bv::ThrowingAddition::armed = true;
    EXPECT_THROW(
        static_cast<void>(graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}, 5.0)),
        std::runtime_error);
    bv::ThrowingAddition::armed = false;
    EXPECT_EQ(graph->get_arcs_size(), 4U) << "the exception deleted arcs from the caller's graph";

    const auto fallback = graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    EXPECT_EQ(fallback.solutions.size(), 2U);
}

// ============================================================================
// The join adds the two halves' costs, so a cost that does not add is refused
// ============================================================================

namespace bidirectional_validation_test {

/// @brief Objective = distance + arrival time when @p time_cost is a `ValueCostFunction`.
///
/// Route A 0-1-3: distance 1 + 1, time 10 + 10, so 22. Route B 0-2-3: distance 5 + 5, time 1 + 1,
/// so 12. The sink closes at 22.
inline std::unique_ptr<ResourceGraph<RealResource>> distance_plus_time_graph(
    std::unique_ptr<CostFunction<Composed>> cost,
    std::unique_ptr<CostFunction<RealResource>> time_cost) {
    const std::map<size_t, std::pair<double, double>> windows{{3, {0.0, 22.0}}};
    auto graph = std::make_unique<ResourceGraph<RealResource>>(
        std::make_unique<CompositionExtensionFunction<RealResource>>(),
        std::make_unique<CompositionFeasibilityFunction<RealResource>>(),
        std::move(cost),
        std::make_unique<CompositionDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        std::move(time_cost),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    for (size_t node_id = 0; node_id < 4; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id == 3);
    }
    graph->add_arc<RealResource, RealResource>({1.0, 10.0}, 0, 1, 1.0);
    graph->add_arc<RealResource, RealResource>({1.0, 10.0}, 1, 3, 1.0);
    graph->add_arc<RealResource, RealResource>({5.0, 1.0}, 0, 2, 5.0);
    graph->add_arc<RealResource, RealResource>({5.0, 1.0}, 2, 3, 5.0);
    return graph;
}

}  // namespace bidirectional_validation_test

/// @brief A cost that reads a threshold's value is refused, naming the cost function.
///
/// The backward value of a time window is a deadline, so adding it as a cost gave route A at cost
/// 4 (distance 2 + deadline 2) where its true cost is 22 and the optimum is route B at 12.
TEST(BidirectionalValidation, ANonAdditiveCostIsRefused) {
    namespace bv = bidirectional_validation_test;
    const auto build = [] {
        return bv::distance_plus_time_graph(
            std::make_unique<CompositionCostFunction<RealResource>>(),
            std::make_unique<ValueCostFunction<RealResource>>());
    };

    const auto forward = build()->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    ASSERT_NEAR(bv::best_cost(forward), 12.0, bv::kTolerance);

    for (const double half_way_point : {0.0, 5.0}) {
        auto graph = build();
        const std::string message = bv::refusal(graph.get(), bv::clocked_params(half_way_point));
        EXPECT_NE(message.find("cost function"), std::string::npos)
            << "H = " << half_way_point << ": " << message;
    }
}

/// @brief The controls: the same model with a zero cost on time, and with the default cost
///        function, solves and matches the forward search.
TEST(BidirectionalValidation, AnAdditiveCostOverAThresholdClockSolves) {
    namespace bv = bidirectional_validation_test;
    const auto zero_time_cost = [] {
        return bv::distance_plus_time_graph(
            std::make_unique<CompositionCostFunction<RealResource>>(),
            std::make_unique<TrivialCostFunction<RealResource>>());
    };
    const auto default_cost = [] {
        return bv::distance_plus_time_graph(
            std::make_unique<ComponentCostFunction<0, RealResource>>(0),
            std::make_unique<ValueCostFunction<RealResource>>());
    };

    for (const auto& build : {std::function(zero_time_cost), std::function(default_cost)}) {
        const double reference =
            bv::best_cost(build()->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}));
        ASSERT_NEAR(reference, 2.0, bv::kTolerance) << "route A's distance is 2";
        for (const double half_way_point : {0.0, 5.0}) {
            auto graph = build();
            auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
                bv::clocked_params(half_way_point));
            EXPECT_NEAR(bv::best_cost(graph->solve(algorithm.get())), reference, bv::kTolerance)
                << "H = " << half_way_point;
        }
    }
}

// ============================================================================
// A threshold's backward floor against its forward clamp, and negative loads
// ============================================================================

namespace bidirectional_validation_test {

/// @brief A cost slot plus @p extension and @p feasibility on slot 1, on the given arcs.
inline std::unique_ptr<ResourceGraph<RealResource>> threshold_pairing_graph(
    std::unique_ptr<ExtensionFunction<RealResource>> extension,
    std::unique_ptr<FeasibilityFunction<RealResource>> feasibility, size_t num_nodes,
    const std::vector<std::tuple<double, double, size_t, size_t>>& arcs) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(std::move(extension),
                                      std::move(feasibility),
                                      std::make_unique<TrivialCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    for (size_t node_id = 0; node_id < num_nodes; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id + 1 == num_nodes);
    }
    for (const auto& [cost, time, origin, destination] : arcs) {
        graph->add_arc<RealResource, RealResource>({cost, time}, origin, destination, cost);
    }
    return graph;
}

}  // namespace bidirectional_validation_test

/// @brief A deadline below a node's opening time is unmeetable, whatever the paired feasibility
///        function tests.
///
/// Time window extension + MinMax: node 1 opens at 5, so the forward clock reaches the sink at
/// 9 > 8 and the only path is infeasible. The backward deadlines are 8, 6, 4, and nothing but the
/// extension knows that 4 is below node 1's opening time.
TEST(BidirectionalValidation, ADeadlineBelowTheOpeningTimeIsUnmeetable) {
    namespace bv = bidirectional_validation_test;
    const std::map<size_t, std::pair<double, double>> windows{{1, {5.0, 100.0}}, {3, {0.0, 8.0}}};
    const auto build = [&] {
        return bv::threshold_pairing_graph(
            std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows,
                                                                        /*default_max=*/100.0),
            std::make_unique<MinMaxFeasibilityFunction<RealResource>>(
                0.0,
                100.0,
                std::map<size_t, std::pair<double, double>>{{3, {0.0, 8.0}}}),
            4,
            {{1.0, 2.0, 0, 1}, {-5.0, 2.0, 1, 2}, {1.0, 2.0, 2, 3}});
    };

    for (const bool preprocess : {true, false}) {
        const auto forward =
            build()->solve<SimpleDominanceAlgorithm>(std::numeric_limits<double>::infinity(),
                                                     AlgorithmParams<LabelList<bv::Composed>>{},
                                                     preprocess);
        ASSERT_TRUE(forward.solutions.empty()) << "the sink is reached at 9 > 8";
        for (const double half_way_point : {0.0, 3.0}) {
            auto graph = build();
            auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
                bv::clocked_params(half_way_point));
            const auto result =
                graph->solve(algorithm.get(), std::numeric_limits<double>::infinity(), preprocess);
            EXPECT_TRUE(result.solutions.empty())
                << "H = " << half_way_point << ", preprocess = " << preprocess
                << ": bidirectional accepted a path that misses the sink's deadline";
        }
    }
}

/// @brief A backward floor the extension never clamps forward to is refused.
///
/// Budget + TimeWindowFeasibility on the same windows: node 1 opens at 5, which the budget never
/// waits for, and the forward test checks only the upper bound, so the path 0-1-2 (cost -3) is
/// feasible. The backward search rejected its deadline 4 at node 1 and returned nothing.
TEST(BidirectionalValidation, ABackwardFloorWithoutAForwardClampIsRefused) {
    namespace bv = bidirectional_validation_test;
    const auto windows = make_node_bounds(0.0,
                                          std::numeric_limits<double>::max() / 2,
                                          {{1, {5.0, 100.0}}, {2, {0.0, 6.0}}});
    const auto build = [&] {
        return bv::threshold_pairing_graph(
            std::make_unique<BudgetExtensionFunction<RealResource>>(windows),
            std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
            3,
            {{1.0, 2.0, 0, 1}, {-4.0, 2.0, 1, 2}});
    };

    const auto forward = build()->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    ASSERT_NEAR(bv::best_cost(forward), -3.0, bv::kTolerance);

    auto graph = build();
    const std::string message = bv::refusal(graph.get(), bv::clocked_params(1.0));
    EXPECT_NE(message.find("component 1"), std::string::npos) << message;
    EXPECT_NE(message.find("node 1"), std::string::npos) << message;
    EXPECT_EQ(message.find("clamped to"), std::string::npos) << "the caps agree: " << message;
}

/// @brief A negative load under a zero floor is refused, for a budget and for an addition.
///
/// The path 0-1-2-4 carries loads 0, -3, +5, so its running load is -3 at node 2 and simple
/// returns 0 via 0-4. A backward label carries only a ceiling and cannot see the dip, so
/// bidirectional returned -10 via 0-1-2-4. Node 3 keeps the feasibility preprocessor from removing
/// arcs 1->2 and 2->4.
TEST(BidirectionalValidation, ANegativeLoadUnderAFloorIsRefused) {
    namespace bv = bidirectional_validation_test;
    const auto build = [](bool budget) {
        std::map<size_t, std::pair<double, double>> windows;
        for (size_t node_id = 0; node_id < 5; ++node_id) {
            windows[node_id] = {0.0, 100.0};
        }
        auto graph = std::make_unique<ResourceGraph<RealResource>>();
        graph->add_resource<RealResource>(
            std::make_unique<AdditionExtensionFunction<RealResource>>(),
            std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
            std::make_unique<ValueCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
        graph->add_resource<RealResource>(
            std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
            std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
            std::make_unique<TrivialCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
        if (budget) {
            graph->add_resource<RealResource>(
                std::make_unique<BudgetExtensionFunction<RealResource>>(10.0),
                std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 10.0, true),
                std::make_unique<TrivialCostFunction<RealResource>>(),
                std::make_unique<ValueDominanceFunction<RealResource>>());
        } else {
            graph->add_resource<RealResource>(
                std::make_unique<AdditionExtensionFunction<RealResource>>(),
                std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 10.0, false),
                std::make_unique<TrivialCostFunction<RealResource>>(),
                std::make_unique<ValueDominanceFunction<RealResource>>());
        }
        for (size_t node_id = 0; node_id < 5; ++node_id) {
            graph->add_node(node_id, node_id == 0, node_id == 4);
        }
        // {cost, load, origin, destination}; every arc takes one unit of time.
        const std::vector<std::tuple<double, double, size_t, size_t>> arcs{{-10.0, 0.0, 0, 1},
                                                                           {0.0, -3.0, 1, 2},
                                                                           {0.0, 5.0, 2, 4},
                                                                           {0.0, 0.0, 0, 4},
                                                                           {0.0, 0.0, 0, 3},
                                                                           {0.0, 5.0, 3, 1},
                                                                           {0.0, 5.0, 3, 2}};
        for (const auto& [cost, load, origin, destination] : arcs) {
            graph->add_arc<RealResource, RealResource, RealResource>(
                std::make_tuple(std::make_tuple(cost), std::make_tuple(1.0), std::make_tuple(load)),
                origin,
                destination,
                cost);
        }
        return graph;
    };

    for (const bool budget : {true, false}) {
        const auto forward = build(budget)->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
        ASSERT_NEAR(bv::best_cost(forward), 0.0, bv::kTolerance) << "budget = " << budget;
        for (const double half_way_point : {0.0, 2.0}) {
            auto graph = build(budget);
            const std::string message =
                bv::refusal(graph.get(), bv::clocked_params(half_way_point));
            EXPECT_NE(message.find("component 2"), std::string::npos)
                << "budget = " << budget << ", H = " << half_way_point << ": " << message;
            EXPECT_NE(message.find("non-negative"), std::string::npos) << message;
        }
    }
}

// ============================================================================
// Composition-level functions without a backward form are refused
// ============================================================================

namespace bidirectional_validation_test {

/// @brief A composition extension function written for forward-only use.
class ForwardOnlyCompositionExtension
    : public Clonable<ForwardOnlyCompositionExtension, ExtensionFunction<Composed>> {
    public:
        void extend(const Resource<Composed>& resource, const Extender<Composed>& extender,
                    Resource<Composed>* extended_resource) override {
            extended_resource->for_each_component(
                resource,
                extender,
                [](auto& extended, const auto& current, const auto& component_extender) {
                    component_extender.extend(current, &extended);
                });
        }
};

/// @brief A composition feasibility function written for forward-only use.
class ForwardOnlyCompositionFeasibility
    : public Clonable<ForwardOnlyCompositionFeasibility, FeasibilityFunction<Composed>> {
    public:
        auto is_feasible(const Resource<Composed>& resource) -> bool override {
            return resource.for_each_component_and(
                [](const auto& component) { return component.is_feasible(); });
        }
};

/// @brief A line of @p num_arcs unit-cost arcs taking @p time each, the sink closing at
///        @p sink_close, with the given composition functions.
inline std::unique_ptr<ResourceGraph<RealResource>> composition_line(
    std::unique_ptr<ExtensionFunction<Composed>> extension,
    std::unique_ptr<FeasibilityFunction<Composed>> feasibility, size_t num_arcs, double time,
    double sink_close) {
    const std::map<size_t, std::pair<double, double>> windows{{num_arcs, {0.0, sink_close}}};
    auto graph = std::make_unique<ResourceGraph<RealResource>>(
        std::move(extension),
        std::move(feasibility),
        std::make_unique<ComponentCostFunction<0, RealResource>>(0),
        std::make_unique<CompositionDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    for (size_t node_id = 0; node_id <= num_arcs; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id == num_arcs);
    }
    for (size_t node_id = 0; node_id < num_arcs; ++node_id) {
        graph->add_arc<RealResource, RealResource>({1.0, time}, node_id, node_id + 1, 1.0);
    }
    return graph;
}

}  // namespace bidirectional_validation_test

/// @brief A composition extension or feasibility function that overrides only its forward half
///        is refused at setup, naming the override it lacks.
///
/// On a line arriving at 9 against a sink closing at 8, the forward-only extension used to return
/// cost 3, status complete, and the forward-only feasibility function threw "can_be_merged not
/// implemented" from the join, after both searches had run.
TEST(BidirectionalValidation, CompositionFunctionsWithoutABackwardFormAreRefused) {
    namespace bv = bidirectional_validation_test;
    for (const double half_way_point : {0.0, 5.0}) {
        auto extension_only =
            bv::composition_line(std::make_unique<bv::ForwardOnlyCompositionExtension>(),
                                 std::make_unique<CompositionFeasibilityFunction<RealResource>>(),
                                 3,
                                 3.0,
                                 8.0);
        const std::string extension_message =
            bv::refusal(extension_only.get(), bv::clocked_params(half_way_point));
        EXPECT_NE(extension_message.find("extend_back"), std::string::npos)
            << "H = " << half_way_point << ": " << extension_message;

        for (const double sink_close : {8.0, 10.0}) {
            auto feasibility_only =
                bv::composition_line(std::make_unique<CompositionExtensionFunction<RealResource>>(),
                                     std::make_unique<bv::ForwardOnlyCompositionFeasibility>(),
                                     3,
                                     3.0,
                                     sink_close);
            const std::string feasibility_message =
                bv::refusal(feasibility_only.get(), bv::clocked_params(half_way_point));
            EXPECT_NE(feasibility_message.find("is_back_feasible"), std::string::npos)
                << "H = " << half_way_point << ": " << feasibility_message;
            EXPECT_NE(feasibility_message.find("can_be_merged"), std::string::npos)
                << "H = " << half_way_point << ": " << feasibility_message;
            EXPECT_EQ(feasibility_message.find("not implemented"), std::string::npos)
                << feasibility_message;
        }
    }
}

/// @brief The control: the library's composition functions solve and match the forward search.
TEST(BidirectionalValidation, TheLibraryCompositionFunctionsSolve) {
    namespace bv = bidirectional_validation_test;
    const auto build = [] {
        return bv::composition_line(
            std::make_unique<CompositionExtensionFunction<RealResource>>(),
            std::make_unique<CompositionFeasibilityFunction<RealResource>>(),
            3,
            3.0,
            10.0);
    };
    const double reference =
        bv::best_cost(build()->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}));
    ASSERT_NEAR(reference, 3.0, bv::kTolerance);
    for (const double half_way_point : {0.0, 5.0}) {
        auto graph = build();
        auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
            bv::clocked_params(half_way_point));
        EXPECT_NEAR(bv::best_cost(graph->solve(algorithm.get())), reference, bv::kTolerance)
            << "H = " << half_way_point;
    }
}

// ============================================================================
// Backward seeds follow the paired extension
// ============================================================================

namespace bidirectional_validation_test {

/// @brief The ways a model used to seed its backward labels wrongly, on a 4-arc line.
enum class SeedCase : std::uint8_t {
    BudgetWithMinMaxFlagFalse,    ///< a budget clock whose MinMax was built with the flag false
    BudgetWithTrivial,            ///< a budget clock with no feasibility bound at all
    WindowsWithTrivial,           ///< a time window clock with no windows and no feasibility
    AdditionWithMinMax,           ///< a capacity written as an addition, beside a window clock
    AdditionWithMinMaxTrivialDom  ///< the same, with a trivial dominance on the capacity
};

/// @brief A 4-arc line where every arc costs 1 and consumes 1 of everything; simple returns 4.
inline std::unique_ptr<ResourceGraph<RealResource>> seed_case_graph(SeedCase seed_case) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    const auto add = [&](std::unique_ptr<ExtensionFunction<RealResource>> extension,
                         std::unique_ptr<FeasibilityFunction<RealResource>>
                             feasibility,
                         std::unique_ptr<DominanceFunction<RealResource>>
                             dominance) {
        graph->add_resource<RealResource>(std::move(extension),
                                          std::move(feasibility),
                                          std::make_unique<TrivialCostFunction<RealResource>>(),
                                          std::move(dominance));
    };
    const auto value_dominance = [] {
        return std::make_unique<ValueDominanceFunction<RealResource>>();
    };
    bool capacity = false;
    switch (seed_case) {
        case SeedCase::BudgetWithMinMaxFlagFalse:
            add(std::make_unique<BudgetExtensionFunction<RealResource>>(10.0),
                std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 10.0, false),
                value_dominance());
            break;
        case SeedCase::BudgetWithTrivial:
            add(std::make_unique<BudgetExtensionFunction<RealResource>>(10.0),
                std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                value_dominance());
            break;
        case SeedCase::WindowsWithTrivial:
            add(std::make_unique<TimeWindowExtensionFunction<RealResource>>(
                    std::map<size_t, std::pair<double, double>>{}),
                std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                value_dominance());
            break;
        case SeedCase::AdditionWithMinMax:
        case SeedCase::AdditionWithMinMaxTrivialDom: {
            std::map<size_t, std::pair<double, double>> windows;
            for (size_t node_id = 0; node_id < 5; ++node_id) {
                windows[node_id] = {0.0, 100.0};
            }
            add(std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
                std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
                value_dominance());
            std::unique_ptr<DominanceFunction<RealResource>> capacity_dominance;
            if (seed_case == SeedCase::AdditionWithMinMax) {
                capacity_dominance = value_dominance();
            } else {
                capacity_dominance = std::make_unique<TrivialDominanceFunction<RealResource>>();
            }
            add(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 10.0),
                std::move(capacity_dominance));
            capacity = true;
            break;
        }
    }
    for (size_t node_id = 0; node_id < 5; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id == 4);
    }
    for (size_t node_id = 0; node_id < 4; ++node_id) {
        if (capacity) {
            graph->add_arc<RealResource, RealResource, RealResource>(
                std::make_tuple(std::make_tuple(1.0), std::make_tuple(1.0), std::make_tuple(1.0)),
                node_id,
                node_id + 1,
                1.0);
        } else {
            graph->add_arc<RealResource, RealResource>({1.0, 1.0}, node_id, node_id + 1, 1.0);
        }
    }
    return graph;
}

}  // namespace bidirectional_validation_test

/// @brief Every model that used to seed its backward labels wrongly now matches simple at H = 2.
///
/// Each returned 0 solutions, status complete, where simple returns 4:
///  - MinMax built with the flag false seeded a deadline at 0;
///  - a clock with no ceiling seeded at the type default, below H, so the backward search
///    stopped at the sink; the bound is now turned off, with a reason;
///  - a capacity written as an addition, with a trivial dominance, got past the seed check.
///    MinMax now seeds an accumulation at the empty suffix, so that model is correct.
TEST(BidirectionalValidation, BackwardSeedsFollowThePairedExtension) {
    namespace bv = bidirectional_validation_test;
    using bv::SeedCase;
    for (const SeedCase seed_case : {SeedCase::BudgetWithMinMaxFlagFalse,
                                     SeedCase::BudgetWithTrivial,
                                     SeedCase::WindowsWithTrivial,
                                     SeedCase::AdditionWithMinMax,
                                     SeedCase::AdditionWithMinMaxTrivialDom}) {
        const auto label = static_cast<int>(seed_case);
        const double reference = bv::best_cost(
            bv::seed_case_graph(seed_case)->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}));
        ASSERT_NEAR(reference, 4.0, bv::kTolerance) << "case " << label;

        auto graph = bv::seed_case_graph(seed_case);
        auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
            bv::clocked_params(2.0));
        const auto result = graph->solve(algorithm.get());
        EXPECT_NEAR(bv::best_cost(result), reference, bv::kTolerance) << "case " << label;

        if (seed_case == SeedCase::BudgetWithTrivial || seed_case == SeedCase::WindowsWithTrivial) {
            EXPECT_FALSE(algorithm->bounded_by_half_way()) << "case " << label;
            EXPECT_NE(algorithm->half_way_off_reason().find("no ceiling"), std::string::npos)
                << "case " << label << ": " << algorithm->half_way_off_reason();
        }
    }
}

// ============================================================================
// Out-of-range indices turn a bound off rather than throw
// ============================================================================

/// @brief A clock index or a cost index that names no component solves and matches simple.
///
/// A clock index of 2 on a two-component model threw std::out_of_range on the first label popped
/// with the bound off, and during setup with it on. A cost index of 5 with the prune on threw from
/// the completion bound.
TEST(BidirectionalValidation, AnOutOfRangeIndexTurnsItsBoundOff) {
    namespace bv = bidirectional_validation_test;
    const std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}},
                                                              {1, {0.0, 100.0}},
                                                              {2, {0.0, 100.0}},
                                                              {3, {0.0, 100.0}}};
    // {cost, time, origin, destination}
    const std::vector<std::tuple<double, double, size_t, size_t>> arcs{{1.0, 1.0, 0, 1},
                                                                       {1.0, 1.0, 1, 3},
                                                                       {5.0, 1.0, 0, 2},
                                                                       {5.0, 1.0, 2, 3}};
    const double reference = bv::best_cost(
        bv::clocked_graph(windows, arcs)->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}));
    ASSERT_NEAR(reference, 2.0, bv::kTolerance);

    for (const double half_way_point : {0.0, 5.0}) {
        auto graph = bv::clocked_graph(windows, arcs);
        auto params = bv::clocked_params(half_way_point);
        params.critical_resource_index = 2;
        auto algorithm =
            graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
        SolveResult result;
        ASSERT_NO_THROW(result = graph->solve(algorithm.get())) << "H = " << half_way_point;
        EXPECT_NEAR(bv::best_cost(result), reference, bv::kTolerance) << "H = " << half_way_point;
        EXPECT_FALSE(algorithm->bounded_by_half_way());
        if (half_way_point > 0.0) {
            EXPECT_NE(algorithm->half_way_off_reason().find("does not exist"), std::string::npos)
                << algorithm->half_way_off_reason();
        }
    }

    auto graph = bv::clocked_graph(windows, arcs);
    auto params = bv::clocked_params(5.0);
    params.heuristic_cost_index = 5;
    params.prune_based_on_upper_bound_ = true;
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    SolveResult result;
    ASSERT_NO_THROW(result = graph->solve(algorithm.get()));
    EXPECT_NEAR(bv::best_cost(result), reference, bv::kTolerance);
}

// ============================================================================
// The join builds no more than the caller will keep
// ============================================================================

/// @brief With a solution budget, the join splices only the cheapest joined paths, and the result
///        is the same as without the budget, truncated.
///
/// Eight parallel arcs on each of two hops, cheaper ones slower, so both halves keep eight labels
/// at node 1 and all 64 pairs join. The join used to build all 64 as Solution + Column before the
/// budget truncated them.
TEST(BidirectionalValidation, TheJoinBuildsAtMostStopAfterXSolutions) {
    namespace bv = bidirectional_validation_test;
    constexpr size_t kParallel = 8;
    constexpr size_t kBudget = 5;
    const auto build = [] {
        const std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}},
                                                                  {1, {0.0, 100.0}},
                                                                  {2, {0.0, 100.0}}};
        std::vector<std::tuple<double, double, size_t, size_t>> arcs;
        for (size_t hop = 0; hop < 2; ++hop) {
            for (size_t i = 0; i < kParallel; ++i) {
                const auto rank = static_cast<double>(i);
                arcs.emplace_back(rank + 1.0,
                                  static_cast<double>(kParallel) - rank + 1.0,
                                  hop,
                                  hop + 1);
            }
        }
        return bv::clocked_graph(windows, arcs);
    };
    const auto solve = [&](size_t budget) {
        auto graph = build();
        auto params = bv::clocked_params(1.0);
        params.stop_after_X_solutions = budget;
        auto algorithm =
            graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
        // A finite bound with the prune off asks for every column under it.
        return graph->solve(algorithm.get(), 1e9);
    };

    const auto all = solve(MAX_INT);
    ASSERT_EQ(all.solutions.size(), kParallel * kParallel);
    // Some are also reached by the backward search alone, so the join adds fewer than 64.
    ASSERT_GT(all.number_of_joined_paths, kBudget);

    const auto budgeted = solve(kBudget);
    EXPECT_LE(budgeted.number_of_joined_paths, kBudget);
    ASSERT_EQ(budgeted.solutions.size(), kBudget);
    for (size_t i = 0; i < kBudget; ++i) {
        EXPECT_NEAR(budgeted.solutions[i].cost, all.solutions[i].cost, bv::kTolerance)
            << "solution " << i;
    }
}

// ============================================================================
// Threshold ceilings: a custom feasibility function shares its caps with the budget
// ============================================================================

namespace bidirectional_validation_test {

/// @brief A per-node cap written the way the library's functions are: the bound is cached in
///        `preprocess()`.
class CachedNodeCap : public Clonable<CachedNodeCap, FeasibilityFunction<RealResource>> {
    public:
        CachedNodeCap(std::map<size_t, double> caps, double default_cap)
            : caps_(std::make_shared<const std::map<size_t, double>>(std::move(caps))),
              default_cap_(default_cap),
              cap_(default_cap) {}

        auto is_feasible(const RealResource& resource) -> bool override {
            return resource.get_value() >= 0.0 && resource.get_value() <= cap_;
        }
        [[nodiscard]] MergeRule merge_rule() const override {
            return backward_kind_ == BackwardKind::Threshold ? MergeRule::DominanceOrder
                                                             : MergeRule::Unspecified;
        }
        [[nodiscard]] auto back_seed_value() const -> std::optional<RealResource> override {
            return RealResource(cap_);
        }

    protected:
        void preprocess(size_t node_id) override {
            auto it = caps_->find(node_id);
            cap_ = it != caps_->end() ? it->second : default_cap_;
        }

    private:
        std::shared_ptr<const std::map<size_t, double>> caps_;
        double default_cap_;
        double cap_;
};

/// @brief A 3-arc line under a cap of 2 at node 2 and 10 elsewhere; simple returns 3.
///
/// @param budget_caps The budget's capacities: the same caps, or not.
inline std::unique_ptr<ResourceGraph<RealResource>> cached_cap_line(
    SharedNodeBounds<double> budget_caps) {
    return threshold_pairing_graph(
        std::make_unique<BudgetExtensionFunction<RealResource>>(std::move(budget_caps)),
        std::make_unique<CachedNodeCap>(std::map<size_t, double>{{2, 2.0}}, 10.0),
        4,
        {{1.0, 1.0, 0, 1}, {1.0, 1.0, 1, 2}, {1.0, 1.0, 2, 3}});
}

}  // namespace bidirectional_validation_test

/// @brief A feasibility function of your own with per-node caps binds at each node once the
///        budget is built from the same caps.
TEST(BidirectionalValidation, ACustomPerNodeCapSharedWithTheBudgetBindsAtEachNode) {
    namespace bv = bidirectional_validation_test;
    const auto caps = make_node_bounds(0.0, 10.0, {{2, {0.0, 2.0}}});
    const double reference = bv::best_cost(
        bv::cached_cap_line(caps)->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}));
    ASSERT_NEAR(reference, 3.0, bv::kTolerance);

    auto graph = bv::cached_cap_line(caps);
    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(
        bv::clocked_params(1.5));
    EXPECT_NEAR(bv::best_cost(graph->solve(algorithm.get())), reference, bv::kTolerance);
}

/// @brief A per-node cap the budget does not share leaves the budget's own clamp in force, which
///        the cap rejects, so it is refused rather than returning nothing.
TEST(BidirectionalValidation, ABackwardClampTheNodeRejectsIsRefused) {
    namespace bv = bidirectional_validation_test;
    auto graph = bv::cached_cap_line(make_node_bounds(0.0, 10.0));
    const std::string message = bv::refusal(graph.get(), bv::clocked_params(1.5));
    EXPECT_NE(message.find("component 1: its backward labels at node 2 are clamped to 10"),
              std::string::npos)
        << message;
    EXPECT_NE(message.find("which its feasibility function rejects there"), std::string::npos)
        << message;
    EXPECT_NE(message.find("NodeBounds"), std::string::npos) << message;
}

/// @brief A clamp looser than the node's upper bound is refused even where the backward test
///        reads only the opening time, as a time window's does.
///
/// The extension closes node 1 at 100 while the feasibility function closes it at 3, and
/// `TimeWindowFeasibilityFunction::is_back_feasible` accepts any deadline after the opening time.
/// Only the comparison with the node's backward seed sees it.
TEST(BidirectionalValidation, AClampAboveATimeWindowsClosingTimeIsRefused) {
    namespace bv = bidirectional_validation_test;
    const std::map<size_t, std::pair<double, double>> closes_late{{1, {0.0, 100.0}}};
    const std::map<size_t, std::pair<double, double>> closes_early{{1, {0.0, 3.0}}};
    auto graph = bv::threshold_pairing_graph(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(closes_late),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(closes_early),
        3,
        {{1.0, 2.0, 0, 1}, {1.0, 2.0, 1, 2}});
    const std::string message = bv::refusal(graph.get(), bv::clocked_params(1.0));
    EXPECT_NE(message.find("component 1: its backward labels at node 1 are clamped to 100"),
              std::string::npos)
        << message;
    EXPECT_NE(message.find("bounds the value there at 3"), std::string::npos) << message;
    EXPECT_NE(message.find("admits deadlines the forward search rejects"), std::string::npos)
        << message;
}
