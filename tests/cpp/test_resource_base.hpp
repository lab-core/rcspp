// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Tests for TrivialFeasibilityFunction, Resource back-feasibility / can_be_merged,
// and ResourceFactory creation methods not exercised by algorithm integration tests.

#include <gtest/gtest.h>

#include <memory>
#include <tuple>

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

/// @brief TrivialFeasibilityFunction::can_be_merged always returns true.
TEST(TrivialFeasibilityFunction, CanBeMergedReturnsTrue) {
    TrivialFeasibilityFunction<R> fn;
    R a;
    R b;
    EXPECT_TRUE(fn.can_be_merged(a, b));
}

// ── Resource back-feasibility and merge ───────────────────────────────────────

/// @brief Resource::is_back_feasible delegates to the feasibility function.
TEST(Resource, IsBackFeasible) {
    Resource<R> r(std::make_unique<ValueDominanceFunction<R>>(),
                  std::make_unique<TrivialFeasibilityFunction<R>>(),
                  std::make_unique<TrivialCostFunction<R>>());
    EXPECT_TRUE(r.is_back_feasible());
}

/// @brief Resource::can_be_merged delegates to the feasibility function.
TEST(Resource, CanBeMerged) {
    Resource<R> front(std::make_unique<ValueDominanceFunction<R>>(),
                      std::make_unique<TrivialFeasibilityFunction<R>>(),
                      std::make_unique<TrivialCostFunction<R>>());
    Resource<R> back(std::make_unique<ValueDominanceFunction<R>>(),
                     std::make_unique<TrivialFeasibilityFunction<R>>(),
                     std::make_unique<TrivialCostFunction<R>>());
    EXPECT_TRUE(front.can_be_merged(back));
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
