// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The cost of a composed resource is kept between changes of its components
// (Resource<ResourceTypeComposition>::get_cost): it is evaluated once after a change, then read.
// A change is a mutable access to a component, an extension into the resource, or a reset.
//
// The tests that count evaluations are skipped under RCSPP_VERIFY_COST_CACHE
// (rcspp::kVerifyCostCache: the Debug builds of the suite), where every read of a kept cost
// evaluates the cost function again in order to check the kept value.

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

namespace cost_cache_test {

using rcspp::RealResource;
using RealComposition = rcspp::ResourceTypeComposition<RealResource>;
using Composed = rcspp::Resource<RealComposition>;
using Component = rcspp::Resource<RealResource>;

/// @brief The sum of the components' costs, as @c CompositionCostFunction, counting its
///        evaluations. Clones (one per node) share the count.
class CountingCost : public rcspp::Clonable<CountingCost, rcspp::CostFunction<RealComposition>> {
    public:
        explicit CountingCost(std::shared_ptr<long> count) : count_(std::move(count)) {}

        [[nodiscard]] double get_cost(const Composed& resource) const override {
            ++*count_;
            double total = 0.0;
            resource.for_each_component(
                [&total](const auto& component) { total += component.get_cost(); });
            return total;
        }

        /// @brief Additive when every component's cost is, as @c CompositionCostFunction: a
        ///        bidirectional search requires it.
        [[nodiscard]] auto is_additive(const Composed& resource) const -> bool override {
            bool additive = true;
            resource.for_each_component([&additive](const auto& component) {
                additive = additive && component.is_cost_additive();
            });
            return additive;
        }

    private:
        std::shared_ptr<long> count_;
};

inline std::unique_ptr<Component> component(double value) {
    return std::make_unique<Component>(
        RealResource(value),
        std::make_unique<rcspp::ValueDominanceFunction<RealResource>>(),
        std::make_unique<rcspp::TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<rcspp::ValueCostFunction<RealResource>>());
}

/// @brief A composition of two components holding @p first and @p second; its cost is their sum.
inline std::unique_ptr<Composed> composed(double first, double second,
                                          const std::shared_ptr<long>& count) {
    std::tuple<std::vector<std::unique_ptr<Component>>> components;
    std::get<0>(components).push_back(component(first));
    std::get<0>(components).push_back(component(second));
    return std::make_unique<Composed>(
        std::move(components),
        std::make_unique<rcspp::CompositionDominanceFunction<RealResource>>(),
        std::make_unique<rcspp::CompositionFeasibilityFunction<RealResource>>(),
        std::make_unique<CountingCost>(count),
        0);
}

/// @brief A graph with a cost (component 0, the label cost) and a time limited to 4
///        (component 1, costing nothing):
///
///     0 -> 1 (cost 1, time 1)   0 -> 2 (cost 4, time 1)   1 -> 2 (cost 1, time 3)
///     1 -> 3 (cost 5, time 1)   2 -> 3 (cost 1, time 1)
///
/// From source 0 to sink 3, 0-1-2-3 costs 3 but takes 5; the cheapest feasible path is 0-2-3,
/// cost 5. Arc ids follow the order above.
class TwoResourceGraph {
    public:
        explicit TwoResourceGraph(const std::shared_ptr<long>& count)
            : graph_(std::make_unique<rcspp::CompositionExtensionFunction<RealResource>>(),
                     std::make_unique<rcspp::CompositionFeasibilityFunction<RealResource>>(),
                     std::make_unique<CountingCost>(count),
                     std::make_unique<rcspp::CompositionDominanceFunction<RealResource>>()) {
            graph_.add_resource<RealResource>(
                std::make_unique<rcspp::AdditionExtensionFunction<RealResource>>(),
                std::make_unique<rcspp::TrivialFeasibilityFunction<RealResource>>(),
                std::make_unique<rcspp::ValueCostFunction<RealResource>>(),
                std::make_unique<rcspp::ValueDominanceFunction<RealResource>>());
            graph_.add_resource<RealResource>(
                std::make_unique<rcspp::AdditionExtensionFunction<RealResource>>(),
                std::make_unique<rcspp::MinMaxFeasibilityFunction<RealResource>>(0.0, 4.0),
                std::make_unique<rcspp::TrivialCostFunction<RealResource>>(),
                std::make_unique<rcspp::ValueDominanceFunction<RealResource>>());
            graph_.add_node(0, true, false);
            graph_.add_node(1);
            graph_.add_node(2);
            graph_.add_node(3, false, true);
            add_arc(0, 1, 1.0, 1.0);
            add_arc(0, 2, 4.0, 1.0);
            add_arc(1, 2, 1.0, 3.0);
            add_arc(1, 3, 5.0, 1.0);
            add_arc(2, 3, 1.0, 1.0);
        }

        rcspp::ResourceGraph<RealResource>& graph() { return graph_; }

    private:
        void add_arc(size_t origin, size_t destination, double cost, double time) {
            std::tuple<std::vector<std::tuple<double>>> values;
            std::get<0>(values) = {std::tuple<double>(cost), std::tuple<double>(time)};
            graph_.add_arc(values, origin, destination, cost);
        }

        rcspp::ResourceGraph<RealResource> graph_;
};

}  // namespace cost_cache_test

/// @brief The cost function is evaluated once, then its value is kept until a component changes.
TEST(CostCache, EvaluatesOnceUntilAComponentChanges) {
    namespace cct = cost_cache_test;
    if (rcspp::kVerifyCostCache) {
        GTEST_SKIP() << "every get_cost evaluates under RCSPP_VERIFY_COST_CACHE";
    }
    auto count = std::make_shared<long>(0);
    auto resource = cct::composed(1.0, 2.0, count);

    EXPECT_EQ(resource->get_cost(), 3.0);
    EXPECT_EQ(resource->get_cost(), 3.0);
    EXPECT_EQ(resource->get_cost(), 3.0);
    EXPECT_EQ(*count, 1);

    resource->get_component<0>(1).set_value(5.0);  // a mutable access: a change
    EXPECT_EQ(resource->get_cost(), 6.0);
    EXPECT_EQ(resource->get_cost(), 6.0);
    EXPECT_EQ(*count, 2);
}

/// @brief Reading the components through a const reference keeps the cost.
TEST(CostCache, ConstReadsKeepTheCost) {
    namespace cct = cost_cache_test;
    if (rcspp::kVerifyCostCache) {
        GTEST_SKIP() << "every get_cost evaluates under RCSPP_VERIFY_COST_CACHE";
    }
    auto count = std::make_shared<long>(0);
    auto resource = cct::composed(1.0, 2.0, count);
    const cct::Composed& view = *resource;

    EXPECT_EQ(view.get_cost(), 3.0);
    double sum = 0.0;
    view.for_each_component(
        [&sum](const auto& component) { sum += component.get_value().get_value(); });
    sum += view.get_component<0>(0).get_value().get_value();
    sum += view.get_components<RealResource>().size();
    EXPECT_EQ(sum, 6.0);
    EXPECT_EQ(view.get_cost(), 3.0);
    EXPECT_EQ(*count, 1);
}

/// @brief Every mutable access counts as a change, whether or not it writes: the next get_cost
///        evaluates, and returns what the components hold.
TEST(CostCache, EveryMutableAccessIsAChange) {
    namespace cct = cost_cache_test;
    auto count = std::make_shared<long>(0);
    auto resource = cct::composed(1.0, 2.0, count);

    EXPECT_EQ(resource->get_cost(), 3.0);
    resource->get_components<0>()[0]->set_value(10.0);
    EXPECT_EQ(resource->get_cost(), 12.0);
    std::get<0>(resource->get_components())[1]->set_value(20.0);
    EXPECT_EQ(resource->get_cost(), 30.0);
    resource->get_component<RealResource>(0).set_value(-1.0);
    EXPECT_EQ(resource->get_cost(), 19.0);
    resource->for_each_component([](auto& component) { component.set_value(0.5); });
    EXPECT_EQ(resource->get_cost(), 1.0);
    resource->apply([](auto& components) { components[1]->set_value(2.5); });
    EXPECT_EQ(resource->get_cost(), 3.0);
}

/// @brief An extension and a reset change the resource they write into.
TEST(CostCache, ExtensionAndResetAreChanges) {
    namespace cct = cost_cache_test;
    auto count = std::make_shared<long>(0);
    cct::TwoResourceGraph model(count);
    auto& graph = model.graph();
    const auto* source = graph.get_node(0);
    const auto* node = graph.get_node(2);

    const cct::Composed start(*source->resource);
    cct::Composed arrival(*node->resource);
    EXPECT_EQ(arrival.get_cost(), 0.0);

    graph.get_arc(1)->extender->extend(start, &arrival);  // 0 -> 2: cost 4
    EXPECT_EQ(arrival.get_cost(), 4.0);

    arrival.reset(*node->resource);
    EXPECT_EQ(arrival.get_cost(), 0.0);

    graph.get_arc(0)->extender->extend(start, &arrival);  // 0 -> 1: cost 1
    EXPECT_EQ(arrival.get_cost(), 1.0);
}

/// @brief A copy, a move and a swap carry the right costs.
TEST(CostCache, CopyMoveAndSwapCarryTheirCosts) {
    namespace cct = cost_cache_test;
    auto count = std::make_shared<long>(0);
    auto first = cct::composed(1.0, 2.0, count);
    auto second = cct::composed(10.0, 20.0, count);
    EXPECT_EQ(first->get_cost(), 3.0);
    EXPECT_EQ(second->get_cost(), 30.0);

    swap(*first, *second);
    EXPECT_EQ(first->get_cost(), 30.0);
    EXPECT_EQ(second->get_cost(), 3.0);

    const cct::Composed copy(*first);
    EXPECT_EQ(copy.get_cost(), 30.0);
    first->get_component<0>(0).set_value(0.0);
    EXPECT_EQ(first->get_cost(), 20.0);
    EXPECT_EQ(copy.get_cost(), 30.0);

    cct::Composed moved(std::move(*second));
    EXPECT_EQ(moved.get_cost(), 3.0);
}

/// @brief A write through a const composition does not invalidate the kept cost (a const
///        unique_ptr still hands out a mutable component). invalidate_cost() declares it; without
///        it, the verification build reports the stale cost.
TEST(CostCache, AWriteThroughAConstCompositionNeedsInvalidateCost) {
    namespace cct = cost_cache_test;
    auto count = std::make_shared<long>(0);
    auto resource = cct::composed(1.0, 2.0, count);
    EXPECT_EQ(resource->get_cost(), 3.0);

    const cct::Composed& view = *resource;
    view.get_components<0>()[0]->set_value(10.0);
    if (rcspp::kVerifyCostCache) {
        EXPECT_THROW(static_cast<void>(view.get_cost()), std::logic_error);
    } else {
        EXPECT_EQ(view.get_cost(), 3.0);  // stale, as documented
    }

    view.invalidate_cost();
    EXPECT_EQ(view.get_cost(), 12.0);
}

namespace cost_cache_test {

using Buckets = rcspp::LabelBuckets<RealResource, RealResource, RealComposition>;

/// @brief Solves TwoResourceGraph with @p Strategy and @p params and checks the optimum, and that
///        the search evaluated the cost function at most once per label the pool handed out: a
///        new or reused label is reset or extended once, then read as often as the search needs.
///
/// A solve first runs the model checks its direction needs (a bidirectional one does), which
/// evaluate costs of resources of their own: their evaluations are counted apart and left out.
template <typename Strategy, typename Params>
void check_solve_evaluates_each_label_once(const Params& params) {
    constexpr double kNoBound = std::numeric_limits<double>::infinity();
    auto count = std::make_shared<long>(0);
    TwoResourceGraph model(count);
    auto& graph = model.graph();
    auto algorithm = graph.template create_algorithm<Strategy>(params);
    // A first solve also prepares the graph (sorted, indexed) for Algorithm::solve below.
    const auto result = graph.solve(algorithm.get(), kNoBound, false);
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_EQ(result.solutions.front().cost, 5.0);
    if (rcspp::kVerifyCostCache) {
        return;  // every read evaluates
    }

    *count = 0;
    rcspp::SolveContext context;
    rcspp::PreSolvePlan<RealComposition>(params.direction).enforce(graph, context);
    const long checks = *count;

    const auto& pool = algorithm->get_label_pool();
    const auto drawn_before = pool.get_nb_created_labels() + pool.get_nb_reused_labels();
    *count = 0;
    static_cast<void>(algorithm->solve(&graph, kNoBound));
    const auto drawn = pool.get_nb_created_labels() + pool.get_nb_reused_labels() - drawn_before;
    EXPECT_GT(drawn, 0);
    EXPECT_LE(*count - checks, drawn);
}

}  // namespace cost_cache_test

TEST(CostCache, ASolveEvaluatesEachLabelAtMostOnce) {
    namespace cct = cost_cache_test;
    const rcspp::AlgorithmParams<rcspp::LabelList<cct::RealComposition>> forward;
    cct::check_solve_evaluates_each_label_once<rcspp::strategy::Simple>(forward);
    cct::check_solve_evaluates_each_label_once<rcspp::strategy::Pulling>(forward);
    cct::check_solve_evaluates_each_label_once<rcspp::strategy::Pushing>(forward);
}

/// @brief The same in both directions, where the join reads the cost of every pair it tries, and
///        with LabelBuckets.
TEST(CostCache, ABidirectionalOrBucketSolveEvaluatesEachLabelAtMostOnce) {
    namespace cct = cost_cache_test;
    rcspp::AlgorithmParams<rcspp::LabelList<cct::RealComposition>> both;
    both.direction = rcspp::SearchDirection::Bidirectional;
    both.critical_resource_index = 1;  // the time
    both.half_way_point = 2.0;
    cct::check_solve_evaluates_each_label_once<rcspp::strategy::Simple>(both);
    cct::check_solve_evaluates_each_label_once<rcspp::strategy::Pulling>(both);

    const auto buckets = rcspp::AlgorithmBaseParams().with_container(
        cct::Buckets(/*bucket_width=*/1, /*bucket_resource_index=*/1, /*sort_resource_index=*/0));
    cct::check_solve_evaluates_each_label_once<rcspp::strategy::Simple>(buckets);
    cct::check_solve_evaluates_each_label_once<rcspp::strategy::Pulling>(buckets);
}
