// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Tests for TrivialFeasibilityFunction, Resource back-feasibility / can_be_joined,
// and ResourceFactory creation methods not exercised by algorithm integration tests.

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <tuple>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/base/resource_factory.hpp"
#include "rcspp/resource/composition/resource_composition_factory.hpp"
#include "rcspp/resource/concrete/functions/cost/value_cost_function.hpp"
#include "rcspp/resource/concrete/functions/dominance/value_dominance_function.hpp"
#include "rcspp/resource/concrete/functions/extension/addition_extension_function.hpp"
#include "rcspp/resource/concrete/numerical_resource.hpp"
#include "rcspp/resource/functions/cost/trivial_cost_function.hpp"
#include "rcspp/resource/functions/feasibility/trivial_feasibility_function.hpp"
#include "rcspp/resource/presets.hpp"
#include "rcspp/resource/resource_graph.hpp"
#include "rcspp/resource/resource_traits.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace {
using R = RealResource;

constexpr double kInitialValue = 3.14;

/// Build a minimal ResourceFactory<RealResource>.
ResourceFactory<R> make_factory() {
    return ResourceFactory<R>(std::make_unique<AdditionExtensionFunction<R>>(),
                              std::make_unique<TrivialFeasibilityFunction<R>>(),
                              std::make_unique<TrivialCostFunction<R>>(),
                              std::make_unique<ValueDominanceFunction<R>>());
}
}  // namespace

// ── TrivialFeasibilityFunction ────────────────────────────────────────────────

/// @brief TrivialFeasibilityFunction never blocks a join: its rule is AlwaysTrue, which decides
///        without calling can_be_joined.
TEST(TrivialFeasibilityFunction, NeverBlocksAJoin) {
    TrivialFeasibilityFunction<R> fn;
    EXPECT_EQ(fn.join_rule(), JoinRule::AlwaysTrue);
}

// ── Resource back-feasibility and join ────────────────────────────────────────

/// @brief Resource::is_back_feasible delegates to the feasibility function.
TEST(Resource, IsBackFeasible) {
    Resource<R> r(std::make_unique<ValueDominanceFunction<R>>(),
                  std::make_unique<TrivialFeasibilityFunction<R>>(),
                  std::make_unique<TrivialCostFunction<R>>());
    EXPECT_TRUE(r.is_back_feasible());
}

/// @brief Resource::can_be_joined delegates to the feasibility function.
TEST(Resource, CanBeJoined) {
    Resource<R> front(std::make_unique<ValueDominanceFunction<R>>(),
                      std::make_unique<TrivialFeasibilityFunction<R>>(),
                      std::make_unique<TrivialCostFunction<R>>());
    Resource<R> back(std::make_unique<ValueDominanceFunction<R>>(),
                     std::make_unique<TrivialFeasibilityFunction<R>>(),
                     std::make_unique<TrivialCostFunction<R>>());
    EXPECT_TRUE(front.can_be_joined(back));
}

// ── ResourceFactory ───────────────────────────────────────────────────────────

/// @brief create_resource(node_id, resource_base) creates a resource without crashing.
TEST(ResourceFactory, CreateResourceWithNodeIdAndBase) {
    auto factory = make_factory();
    R base;
    base.set_value(kInitialValue);
    auto res = factory.create_resource(/*node_id=*/1, base);
    ASSERT_NE(res, nullptr);
}

/// @brief clone() produces an independent copy that can still create resources.
TEST(ResourceFactory, CloneProducesWorkingCopy) {
    auto factory = make_factory();
    auto cloned = factory.clone();
    ASSERT_NE(cloned, nullptr);
    EXPECT_NE(cloned->create_resource(), nullptr);
}

/// @brief A composition factory cloned through its base class keeps its per-type sub-factories.
///
/// The inherited ResourceFactory::clone copied only the composed prototype, so the copy was a plain
/// ResourceFactory with no sub-factory at all.
TEST(ResourceFactory, CompositionFactoryClonedThroughItsBaseKeepsItsSubFactories) {
    ResourceGraph<RealResource, IntResource> graph;
    presets::add_cost_resource<RealResource>(graph);
    presets::add_capacity_resource<IntResource>(graph, /*capacity=*/10);
    using Composed = ResourceCompositionFactory<RealResource, IntResource>;
    using Base = ResourceFactory<ResourceTypeComposition<RealResource, IntResource>>;

    const Base& factory = graph.get_resource_factory();
    const std::unique_ptr<Base> cloned = factory.clone();

    const auto* composed = dynamic_cast<const Composed*>(cloned.get());
    ASSERT_NE(composed, nullptr);
    EXPECT_EQ(composed->get_num_resource_type<RealResource>(), 1U);
    EXPECT_EQ(composed->get_num_resource_type<IntResource>(), 1U);
    EXPECT_NE(cloned->create_resource(), nullptr);
}

/// @brief A node added to a cloned graph is priced as the same node added to the original.
///
/// clone_factory() built the copy with the default composition functions. Their cost sums every
/// component's, while a default graph's cost is its first component's alone, so a node added to
/// the clone summed two priced resources where the original counted one.
TEST(ResourceFactory, NodeAddedToAClonedGraphIsPricedAsInTheOriginal) {
    // Two resources, both priced by their value; the graph's cost is the first one's.
    const auto build = [] {
        auto graph = std::make_unique<ResourceGraph<RealResource>>();
        for (int i = 0; i < 2; ++i) {
            graph->add_resource<RealResource>(
                std::make_unique<AdditionExtensionFunction<RealResource>>(),
                std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                std::make_unique<ValueCostFunction<RealResource>>(),
                std::make_unique<ValueDominanceFunction<RealResource>>());
        }
        graph->add_node(0, /*source=*/true);
        graph->add_node(1, /*source=*/false, /*sink=*/true);
        graph->add_arc<RealResource, RealResource>(std::make_tuple(1.0, 10.0), 0, 1, 1.0);
        return graph;
    };
    // A second sink, added after the clone: cost 5, second resource 100.
    const auto add_second_sink = [](ResourceGraph<RealResource>& graph) {
        graph.add_node(2, /*source=*/false, /*sink=*/true);
        graph.add_arc<RealResource, RealResource>(std::make_tuple(5.0, 100.0), 0, 2, 5.0);
    };
    const auto cost_to = [](ResourceGraph<RealResource>& graph, size_t sink_id) {
        graph.sort_nodes();
        const auto result =
            graph.solve(std::numeric_limits<double>::infinity(),
                        AlgorithmParams<LabelList<ResourceTypeComposition<RealResource>>>{},
                        /*preprocess=*/false);
        for (const auto& solution : result.solutions) {
            if (solution.path_node_ids.back() == sink_id) {
                return solution.cost;
            }
        }
        return std::numeric_limits<double>::quiet_NaN();
    };

    const auto original = build();
    const auto clone = original->clone();
    add_second_sink(*original);
    add_second_sink(*clone);

    ASSERT_DOUBLE_EQ(cost_to(*original, 2), 5.0);
    EXPECT_DOUBLE_EQ(cost_to(*clone, 2), 5.0) << "the clone summed both resources' costs";
}

/// @brief reset() adopts the other resource's join rule along with its function objects.
///
/// It is copied rather than re-read through a virtual call, since the feasibility function is the
/// same object.
TEST(ResourceBase, ResetCopiesTheCachedJoinRule) {
    Resource<R> always(std::make_unique<ValueDominanceFunction<R>>(),
                       std::make_unique<TrivialFeasibilityFunction<R>>(),
                       std::make_unique<TrivialCostFunction<R>>());

    class Undeclared : public Clonable<Undeclared, FeasibilityFunction<R>> {
        public:
            auto is_feasible(const R& /*resource*/) -> bool override { return true; }
    };
    Resource<R> pooled(std::make_unique<ValueDominanceFunction<R>>(),
                       std::make_unique<Undeclared>(),
                       std::make_unique<TrivialCostFunction<R>>());
    ASSERT_EQ(pooled.join_rule(), JoinRule::Unspecified);
    ASSERT_NE(always.join_rule(), JoinRule::Unspecified);

    pooled.reset(always);
    EXPECT_EQ(pooled.join_rule(), always.join_rule());
    EXPECT_TRUE(pooled.can_be_joined(always));
}
