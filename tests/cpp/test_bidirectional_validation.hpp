// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Phase 13: the three checks that sit beside the equivalence sweep.
//
//  1. **The short path**, with its mechanism proved rather than assumed. A route whose clock never
//     reaches `H` crosses `H` on no arc, so the join cannot emit it; it survives only because a
//     forward label ran all the way to a sink and terminal collection swept that direction too. A
//     joiner-only design loses it silently, and an assertion that the answer is merely *found* does
//     not distinguish the two designs -- so the join count is asserted to be zero.
//
//  2. **The reverse-graph oracle**, a third implementation. Reversing a graph and running the
//     ordinary forward search is a backward search that shares no code with `BackwardDirection`.
//     It is only valid on additive models -- a reversed arc still applies the *forward* extension
//     function -- and that limitation is what makes it independent rather than a second copy.
//
//  3. **Backward `LabelBuckets`**, the restriction phase 7 left in place. The question is whether a
//     bucketed backward container keeps the same labels as the list, and it is answered at the
//     container level; see `BucketedBackwardContainerMatchesTheList` for why the algorithm is not
//     switched over even though the answer is yes.

#include <gtest/gtest.h>

#include <algorithm>
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
///
/// Threshold-shaped on purpose: a backward container only differs from a forward one when the
/// dominance order actually reverses, so an Accumulate label would make the comparison vacuous.
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
/// Two components so a bucket container can bucket on one and sort on the other, which is the
/// configuration `LabelBuckets` is for and the only one in which its sort-resource comparisons are
/// reachable. `threshold_label`'s single component made bucket and sort the same slot, so the
/// comparison could never disagree with itself.
///
/// @param label_id The label's id, used to compare survivor sets.
/// @param deadline Slot 0: a threshold, so its backward dominance is reversed.
/// @param cost     Slot 1: an accumulation, so its dominance is not reversed in either direction.
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
/// `add_label` only appends -- dominance lives in `is_dominated` / `remove_dominated_labels`, and
/// the algorithm calls all three. A comparison that only called `add_label` would find two
/// containers agreeing because neither had pruned anything.
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
/// The assertion that matters is `number_of_joined_paths() == 0`. Without it the test passes on a
/// joiner-only implementation for the wrong reason, and the one design mistake it exists to catch
/// goes undetected.
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
/// The complement of the test above: if the join count were always zero the assertion there would
/// be measuring a broken joiner rather than a short path.
TEST(BidirectionalValidation, ALongRouteIsJoinedOnItsCrossingArc) {
    namespace bv = bidirectional_validation_test;

    // Each window closes exactly at its node's earliest arrival, which is what stops the backward
    // search: a deadline can only shrink as it propagates back, so it falls below H and the search
    // gives up before reaching the source. Loose windows would let the backward search run all the
    // way to node 0 and produce the path on its own -- the join would then find only an equal-cost
    // duplicate, be rejected by the incumbent, and this test would assert zero joins for a reason
    // that has nothing to do with the crossing rule.
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

// ============================================================================
// The join is not owed after a run-level stop
// ============================================================================

namespace bidirectional_validation_test {

/// @brief The 5-node chain from `ALongRouteIsJoinedOnItsCrossingArc`, whose optimum exists only
///        as a join: forward stops at node 3, backward at node 2, neither reaches a terminal.
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
/// `Algorithm::solve` calls `extract_remaining_solutions()` -- and therefore the join --
/// unconditionally after `main_loop()` returns, including when `main_loop` broke on the timeout,
/// the stop callback, or the hard memory limit. The join is the most expensive pass in the
/// algorithm: `Joiner::join` records 3 064 862 pairs and 12.23 s on a full C201. Spending that
/// after a stop has fired is the opposite of what the stop was asked for.
///
/// The stop callback is the testable one of the three -- a timeout needs a wall clock and the
/// memory limit needs a real allocation -- and it reaches the identical guard.
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

    auto algorithm = graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    const SolveResult result = graph->solve(algorithm.get());

    ASSERT_EQ(result.status, AlgorithmStatus::INTERRUPTED)
        << "the callback did not stop the search, so the guard was never reached";
    EXPECT_EQ(result.number_of_joined_paths, 0U) << "the join ran after the solve was interrupted";
    EXPECT_TRUE(algorithm->get_label_pool().check_ref_count_consistency())
        << "skipping the join must not change what the pool owns";
}

/// @brief `stop_after_X_solutions` deliberately does NOT skip the join.
///
/// The other half of the guard, and the reason it tests the frontiers rather than
/// `should_stop()`. A solution budget caps what is *returned*: `Joiner::join` runs to completion
/// and `Algorithm::solve` resizes afterwards, so a `COMPLETE` status still means the search was
/// exhaustive. Skipping the join for it would change the answer rather than the cost.
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

/// @brief The joiner never rejects a half on the half's own cost.
///
/// The regression test for the defect this phase's benchmark exposed. `Joiner::join` used to open
/// with `if (forward->get_cost() >= best_cost_upper_bound) break;` -- a half compared against the
/// incumbent *total*. Sound while every cost is positive, and wrong the moment reduced costs are
/// not: a half costing 30 completes to -70, and the break discarded it in cost-sorted order along
/// with everything behind it.
///
/// It cost the repository's own VRPTW pricing instance its optimum: -190.46 returned against a
/// true -319.88, in less time, with status COMPLETE. That is the failure mode this phase exists to
/// catch, and the reason the benchmark runs after the equivalence suite rather than before it.
///
/// The instance below is the smallest shape that reproduces it. A cheap direct route to the sink
/// gives the incumbent (-10); the good route's forward half costs +30 on its own and completes to
/// -70, and it can only be produced by the join -- the forward search stops before the sink and the
/// backward search stops before the source.
TEST(BidirectionalValidation, TheJoinerDoesNotPruneAHalfOnItsOwnCost) {
    namespace bv = bidirectional_validation_test;

    // Each window closes at its node's earliest arrival on the long route, which is what stops the
    // backward search short of the source.
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
/// `Simple` forward, `Simple` on the reversed graph, and bidirectional. The oracle is the only one
/// of the three that does not share the library's backward machinery, so an error in that
/// machinery cannot hide by being consistent with itself.
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
        // Everything here accumulates, so nothing is a clock: the bound switches itself off and
        // both searches run to completion. That is the point -- the oracle is only valid where
        // forward and backward extension coincide.
        auto algorithm =
            candidate_graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
        const double candidate = bv::best_cost(candidate_graph->solve(algorithm.get()));

        EXPECT_NEAR(oracle, reference, bv::kTolerance) << "the reversed graph disagrees";
        EXPECT_NEAR(candidate, reference, bv::kTolerance) << "bidirectional disagrees";
        EXPECT_FALSE(algorithm->bounded_by_half_way())
            << "an additive-only model has no clock, so the bound must be off";
        EXPECT_TRUE(algorithm->get_label_pool().check_ref_count_consistency());
    }
}

// ============================================================================
// An accumulating capacity joins on the sum, not on a comparison
// ============================================================================

/// @brief A bidirectional solve never splices two halves whose loads together break the cap.
///
/// The regression for the one defect that made this algorithm return a *wrong* answer rather than
/// a slow or an incomplete one. `MinMaxFeasibilityFunction` declared `MergeRule::DominanceOrder`
/// for every pairing, which compares the forward value against the backward one. Under a
/// threshold extension that is right -- the backward label carries a remaining-capacity ceiling.
/// Under `AdditionExtensionFunction` the backward label carries the suffix's own load, so the
/// comparison asks `prefix <= suffix`, which is not a constraint the model contains. The join then
/// accepted a pair whose loads add to more than the cap, and since refusals are replayed but
/// acceptances are not, the infeasible path came back as `solutions.front()` with a COMPLETE
/// status.
///
/// The graph below is the smallest one that reaches it with preprocessing ON, which is the
/// default. Node 5 exists only to give node 2 a light-but-expensive second route, so the
/// preprocessor cannot delete arc 2->3 on minimum-load grounds and erase the backward label the
/// bad join needs -- which is exactly what masks the defect on `oracle_instances()`'s own capacity
/// instance.
///
/// Feasible paths, capacity 4: 0-1-3-4 costs 6 carrying 3, and 0-5-2-3-4 costs 12 carrying 3.
/// 0-2-3-4 costs 3 but carries 6, and is the path the defect returned.
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
        // An accumulating load with a cap. The merge-direction flag is false so the backward
        // label seeds at the minimum and accumulates upward, which is the only coherent way to
        // pair a back seed with an addition -- see BidirectionalDominanceAlgorithm's
        // seeds_itself_out_of_range.
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

    // Not just the best one: every returned path must actually fit under the cap.
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
/// The general guard behind the specific fix: `MinMaxFeasibilityFunction` no longer reaches this,
/// because its own `merge_rule()` reads the same kind. Any other feasibility function that
/// declares `DominanceOrder` without consulting its extension function has the identical hazard,
/// so the pairing is checked once where both declarations are visible.
TEST(BidirectionalValidation, AccumulatingExtensionWithADominanceOrderMergeIsRefused) {
    namespace bv = bidirectional_validation_test;

    /// A stand-in for the defect: a bound's merge rule on an accumulating resource.
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
/// The earlier form of this test could not fail. It used one component, so the bucket resource and
/// the sort resource were the same slot, and a set in which one label dominated all the others, so
/// exactly one survived. `LabelBuckets` has five direction-sensitive comparisons: two bucket
/// boundary predicates and three on the sort resource. Only the first two were reachable, and only
/// they were routed.
///
/// The set below is a genuine Pareto front under backward dominance -- a later deadline is more
/// permissive, a lower cost is better, so a label survives unless something beats it on both --
/// which is what makes the early exits in `is_dominated` and `remove_dominated_labels` run.
///
/// **The algorithm is still not switched over.** Making the backward container configurable needs a
/// fourth template parameter on `BidirectionalDominanceAlgorithm`, which would take it outside the
/// two-parameter template-template slot `ResourceGraph::solve` accepts. The correctness question is
/// what this test answers; the plumbing is a separate decision.
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

        // And the list itself keeps the right labels, so the two cannot be wrong together.
        EXPECT_EQ(from_list, (std::vector<size_t>{0, 1, 2}))
            << "the Pareto front is {90,5}, {70,3}, {50,1}; {60,4} is beaten on both: " << what;
    };

    // The realistic configuration: bucket on the clock, sort on the cost.
    compare(/*bucket_index=*/0, /*sort_index=*/1, "bucket = deadline, sort = cost");
    // The one that reaches the sort-resource comparisons with a REVERSED resource.
    compare(/*bucket_index=*/1, /*sort_index=*/0, "bucket = cost, sort = deadline");
}
