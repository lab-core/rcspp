// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Every labelling strategy that searches bidirectionally: what each side's schedule does with a
// cut (in a bidirectional search it abandons the label, which is never extended, since the
// search runs one phase), how Pulling keeps a boundary label, and whether the join keeps the
// optimum.
//
// Its own translation unit: each strategy's bidirectional instantiation.

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <memory>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/bidirectional_test_util.hpp"
#include "util/equivalence_helpers.hpp"
#include "util/join_optimality.hpp"
#include "util/random_instance.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace bidirectional_strategies_test {

using Composed = ResourceTypeComposition<RealResource>;
using List = LabelList<Composed>;

/// @brief Six hops of two parallel arcs, one cheap and slow and one dear and quick, so every node
///        holds many non-dominated labels and a quota of 2 has plenty to refuse; the second
///        resource is the clock.
inline std::unique_ptr<ResourceGraph<RealResource>> fan() {
    constexpr size_t kHops = 6;
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<CapacityExtensionFunction<RealResource>>(1000.0),
        std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 1000.0),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    for (size_t node_id = 0; node_id <= kHops; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id == kHops);
    }
    for (size_t node_id = 0; node_id < kHops; ++node_id) {
        graph->add_arc<RealResource, RealResource>({1.0, 4.0}, node_id, node_id + 1, 1.0);
        graph->add_arc<RealResource, RealResource>({3.0, 1.0}, node_id, node_id + 1, 3.0);
    }
    return graph;
}

/// @brief Bidirectional params on the fan's clock, with a per-node quota of @p quota.
inline AlgorithmParams<List> params(size_t quota) {
    AlgorithmParams<List> result;
    result.direction = SearchDirection::Bidirectional;
    result.critical_resource_index = 1;
    result.half_way_point = 6.0;
    result.max_extended_labels_per_node = quota;
    return result;
}

/// @brief Expects a bidirectional @p Strategy under a quota of 2 to abandon labels, so the solve
///        is not Complete, and still to return only paths that replay through the model.
template <typename Strategy>
void expect_a_quota_abandons_and_the_result_stays_a_path(const char* name) {
    auto graph = fan();
    auto algorithm = graph->template create_algorithm<Strategy>(params(2));
    const auto result = graph->solve(algorithm.get());
    EXPECT_EQ(result.status, AlgorithmStatus::Truncated) << name << ": a quota of 2 abandons";
    EXPECT_FALSE(algorithm->is_optimal()) << name;
    ASSERT_FALSE(result.solutions.empty()) << name;
    for (const auto& solution : result.solutions) {
        EXPECT_EQ(equivalence_test::path_problem(*graph, solution), "") << name;
    }
}

/// @brief Expects a bidirectional @p Strategy that keeps its labels between solves
///        (`release_after_solve = false`) to find forward Simple's optimum twice in a row.
template <typename Strategy>
void expect_a_second_solve_finds_the_same_optimum(const char* name) {
    auto graph = fan();
    const auto forward = graph->template solve<strategy::Simple>(AlgorithmBaseParams{});
    ASSERT_FALSE(forward.solutions.empty());

    auto kept = params(kNoLimit);
    kept.release_after_solve = false;
    auto algorithm = graph->template create_algorithm<Strategy>(kept);
    for (const char* solve : {"first", "second"}) {
        const auto result = graph->solve(algorithm.get());
        EXPECT_EQ(result.status, AlgorithmStatus::Complete) << name << ", " << solve;
        ASSERT_FALSE(result.solutions.empty()) << name << ", " << solve;
        EXPECT_DOUBLE_EQ(result.solutions.front().cost, forward.solutions.front().cost)
            << name << ", " << solve;
        EXPECT_EQ(result.solutions.front().path_arc_ids, forward.solutions.front().path_arc_ids)
            << name << ", " << solve;
    }
}

/// @brief The line 0 -> 1 -> 2 -> 3 -> 4: each arc costs -1 and loads 1, under a capacity of 4,
///        the clock (component 1).
inline std::unique_ptr<ResourceGraph<RealResource>> line() {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<CapacityExtensionFunction<RealResource>>(4.0),
        std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 4.0),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    for (size_t node_id = 0; node_id <= 4; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id == 4);
    }
    for (size_t node_id = 0; node_id < 4; ++node_id) {
        graph->add_arc<RealResource, RealResource>({-1.0, 1.0}, node_id, node_id + 1, -1.0);
    }
    return graph;
}

/// @brief The clock values of the labels @p containers hold at node @p node_id, in order.
template <typename ContainerType>
std::vector<double> clocks_at(const ResourceGraph<RealResource>& graph,
                              const std::vector<ContainerType>& containers, size_t node_id) {
    std::vector<double> clocks;
    for (const auto* label : containers.at(graph.get_node(node_id)->pos()).get_labels()) {
        clocks.push_back(detail::critical_value<RealResource, Composed>(label->get_resource(), 1));
    }
    return clocks;
}

/// @brief Expects the bidirectional @p Strategy to find brute force's optimum on every instance of
///        the join-optimality cross product, at every `H`.
///
/// @return The (instance, `H`) pairs with a finite optimum.
template <typename Strategy>
size_t expect_the_join_matches_brute_force(const char* name) {
    namespace jo = join_optimality;
    size_t with_solutions = 0;
    for (const auto& config : jo::cross_product(/*num_seeds=*/6, /*num_nodes=*/8)) {
        const double truth = test_util::brute_force_optimum(config);
        for (const double factor : jo::half_way_factors()) {
            EXPECT_NEAR(jo::solve_with<Strategy>(config, factor).cost, truth, jo::kTolerance)
                << name << " lost the optimum: " << test_util::describe(config) << ", H factor "
                << factor;
            if (std::isfinite(truth)) {
                ++with_solutions;
            }
        }
    }
    return with_solutions;
}

/// @brief Expects the bidirectional @p Strategy to find the forward search's optimum on larger
///        instances of the cross product, at every `H`.
template <typename Strategy>
void expect_the_join_matches_the_forward_search(const char* name) {
    namespace jo = join_optimality;
    for (const size_t num_nodes : {14U, 20U, 28U}) {
        for (const auto& config : jo::cross_product(/*num_seeds=*/3, num_nodes)) {
            const double reference = jo::forward_optimum(config);
            for (const double factor : jo::half_way_factors()) {
                EXPECT_NEAR(jo::solve_with<Strategy>(config, factor).cost,
                            reference,
                            jo::kTolerance)
                    << name << " disagrees with the forward search: " << test_util::describe(config)
                    << ", H factor " << factor;
            }
        }
    }
}

}  // namespace bidirectional_strategies_test

/// @brief In every bidirectional search, a quota abandons labels and the paths stay paths.
TEST(BidirectionalStrategies, AQuotaAbandonsAndTheResultStaysAPath) {
    namespace bst = bidirectional_strategies_test;
    bst::expect_a_quota_abandons_and_the_result_stays_a_path<strategy::Simple>("Simple");
    bst::expect_a_quota_abandons_and_the_result_stays_a_path<strategy::AStar>("A*");
    bst::expect_a_quota_abandons_and_the_result_stays_a_path<strategy::Pushing>("Pushing");
    bst::expect_a_quota_abandons_and_the_result_stays_a_path<strategy::Pulling>("Pulling");
}

/// @brief Every bidirectional search starts each solve afresh: kept labels change no optimum.
TEST(BidirectionalStrategies, ASecondSolveWithoutReleaseFindsTheSameOptimum) {
    namespace bst = bidirectional_strategies_test;
    bst::expect_a_second_solve_finds_the_same_optimum<strategy::Simple>("Simple");
    bst::expect_a_second_solve_finds_the_same_optimum<strategy::AStar>("A*");
    bst::expect_a_second_solve_finds_the_same_optimum<strategy::Pushing>("Pushing");
    bst::expect_a_second_solve_finds_the_same_optimum<strategy::Pulling>("Pulling");
}

/// @brief A label past the half-way point is stored, never pulled: on a line it ends each half
///        one node past `H`, and the join completes the one path.
///
/// The line 0 -> 1 -> 2 -> 3 -> 4 costs -1 and loads 1 per arc, under a capacity of 4, with
/// `H` = 2. Forward, the label at node 3 (load 3) is past `H`: it is stored, and nothing reaches
/// sink 4. Backward, the label at node 1 (threshold 1) is past `H`: it is stored, and nothing
/// reaches source 0. Each half extends 3 labels; the path 0 1 2 3 4 comes from the join alone.
TEST(BidirectionalPulling, ABoundaryLabelIsNeverPulled) {
    namespace bst = bidirectional_strategies_test;
    using PullingBidir = detail::BidirectionalLabellingAlgorithm<
        bst::Composed,
        bst::List,
        RealResource,
        detail::Preset<parts::PullSchedule<bst::Composed, ForwardDirection>>,
        detail::Preset<parts::PullSchedule<bst::Composed, BackwardDirection>>>;
    auto graph = bst::line();
    auto params = bst::params(kNoLimit);
    params.half_way_point = 2.0;
    params.release_after_solve = false;  // the containers are read below
    auto algorithm = graph->create_algorithm<strategy::Pulling>(params);
    const auto result = graph->solve(algorithm.get());

    ASSERT_EQ(result.solutions.size(), 1U);
    EXPECT_DOUBLE_EQ(result.solutions.front().cost, -4.0);
    EXPECT_EQ(result.solutions.front().path_node_ids, (std::vector<size_t>{0, 1, 2, 3, 4}));
    EXPECT_TRUE(result.bidirectional->half_way_used);
    EXPECT_EQ(result.bidirectional->joined_paths, 1U) << "only the join completes it";
    EXPECT_EQ(algorithm->get_number_of_extended_labels(), 6U) << "3 extensions per half";

    const auto* impl = test_util::bidirectional_impl<PullingBidir>(algorithm.get());
    EXPECT_EQ(bst::clocks_at(*graph, impl->forward_labels(), 3), std::vector<double>{3.0})
        << "the forward boundary label, past H";
    EXPECT_TRUE(bst::clocks_at(*graph, impl->forward_labels(), 4).empty())
        << "the forward boundary label was pulled";
    EXPECT_EQ(bst::clocks_at(*graph, impl->backward_labels(), 1), std::vector<double>{1.0})
        << "the backward boundary label, past H";
    EXPECT_TRUE(bst::clocks_at(*graph, impl->backward_labels(), 0).empty())
        << "the backward boundary label was pulled";
}

/// @brief Every strategy's bidirectional search matches brute force across the join-optimality
///        cross product and every `H`.
TEST(BidirectionalStrategies, TheJoinMatchesBruteForceForEveryStrategy) {
    namespace bst = bidirectional_strategies_test;
    EXPECT_GT(bst::expect_the_join_matches_brute_force<strategy::AStar>("A*"), 0U);
    EXPECT_GT(bst::expect_the_join_matches_brute_force<strategy::Pushing>("Pushing"), 0U);
    EXPECT_GT(bst::expect_the_join_matches_brute_force<strategy::Pulling>("Pulling"), 0U);
}

/// @brief On instances too large to enumerate, every strategy's bidirectional search matches the
///        forward search.
TEST(BidirectionalStrategies, TheJoinMatchesTheForwardSearchForEveryStrategy) {
    namespace bst = bidirectional_strategies_test;
    bst::expect_the_join_matches_the_forward_search<strategy::AStar>("A*");
    bst::expect_the_join_matches_the_forward_search<strategy::Pushing>("Pushing");
    bst::expect_the_join_matches_the_forward_search<strategy::Pulling>("Pulling");
}
