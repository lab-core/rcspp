// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Tests for TrivialFeasibilityFunction, Resource back-feasibility / can_be_joined,
// and ResourceFactory creation methods not exercised by algorithm integration tests.

#include <gtest/gtest.h>

#include <memory>
#include <tuple>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/base/resource_factory.hpp"
#include "rcspp/resource/concrete/functions/dominance/value_dominance_function.hpp"
#include "rcspp/resource/concrete/functions/extension/addition_extension_function.hpp"
#include "rcspp/resource/concrete/numerical_resource.hpp"
#include "rcspp/resource/functions/cost/trivial_cost_function.hpp"
#include "rcspp/resource/functions/feasibility/trivial_feasibility_function.hpp"
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
