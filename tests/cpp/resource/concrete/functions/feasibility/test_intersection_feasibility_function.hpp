// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <gtest/gtest.h>

#include <map>
#include <set>

#include "rcspp/rcspp.hpp"

using namespace rcspp;

namespace {

SetResource<int> make_set_resource(const std::set<int>& values) {
    SetResource<int> r;
    r.set_value(values);
    return r;
}

}  // namespace

// Empty-map construction: no preprocess() call → empty_ stays true → all resources feasible.
TEST(IntersectionFeasibilityFunction, EmptyMap) {
    IntersectionFeasibilityFunction<SetResource<int>, int> fn({});
    EXPECT_TRUE(fn.is_feasible(make_set_resource({1, 2, 3})));
    EXPECT_TRUE(fn.is_feasible(make_set_resource({})));
}

// forbidden=true: feasibility requires NO intersection with the per-node set.
TEST(IntersectionFeasibilityFunction, Forbidden) {
    std::map<size_t, std::set<int>> per_node = {{0, {1, 2, 3}}, {1, {7, 8, 9}}};
    IntersectionFeasibilityFunction<SetResource<int>, int> fn(per_node, /*forbidden=*/true);

    fn.reset(0);
    EXPECT_TRUE(fn.is_feasible(make_set_resource({4, 5, 6})));
    EXPECT_FALSE(fn.is_feasible(make_set_resource({3, 4})));

    fn.reset(1);
    EXPECT_TRUE(fn.is_feasible(make_set_resource({1, 2, 3})));
    EXPECT_FALSE(fn.is_feasible(make_set_resource({0, 9})));
}

// forbidden=false: feasibility requires AT LEAST ONE intersection.
TEST(IntersectionFeasibilityFunction, Required) {
    std::map<size_t, std::set<int>> per_node = {{0, {1, 2, 3}}};
    IntersectionFeasibilityFunction<SetResource<int>, int> fn(per_node, /*forbidden=*/false);
    fn.reset(0);
    EXPECT_TRUE(fn.is_feasible(make_set_resource({2, 5})));
    EXPECT_FALSE(fn.is_feasible(make_set_resource({4, 5})));
}

// Unknown node-id: preprocess sets empty_=true, so every resource is feasible.
TEST(IntersectionFeasibilityFunction, UnknownNode) {
    std::map<size_t, std::set<int>> per_node = {{0, {1, 2, 3}}};
    IntersectionFeasibilityFunction<SetResource<int>, int> fn(per_node, /*forbidden=*/true);

    fn.reset(42);
    EXPECT_TRUE(fn.is_feasible(make_set_resource({1, 2, 3})));

    fn.reset(0);
    EXPECT_FALSE(fn.is_feasible(make_set_resource({1})));
}

// Empty per-node set is treated as a missing entry (empty_ stays true).
TEST(IntersectionFeasibilityFunction, EmptyPerNodeSet) {
    std::map<size_t, std::set<int>> per_node = {{0, {}}};
    IntersectionFeasibilityFunction<SetResource<int>, int> fn(per_node, /*forbidden=*/true);
    fn.reset(0);
    EXPECT_TRUE(fn.is_feasible(make_set_resource({1, 2})));
}

// Default ValueType deduction: no second template argument → resolves to int.
TEST(IntersectionFeasibilityFunction, DefaultValueType) {
    std::map<size_t, std::set<int>> per_node = {{0, {1, 2, 3}}};
    IntersectionFeasibilityFunction<SetResource<int>> fn(per_node, /*forbidden=*/true);
    fn.reset(0);
    EXPECT_TRUE(fn.is_feasible(make_set_resource({4, 5, 6})));
    EXPECT_FALSE(fn.is_feasible(make_set_resource({2, 7})));
}

// Clone preserves behaviour (exercises the Clonable<Derived,Base> wiring).
TEST(IntersectionFeasibilityFunction, Clone) {
    std::map<size_t, std::set<int>> per_node = {{0, {1, 2, 3}}};
    IntersectionFeasibilityFunction<SetResource<int>, int> fn(per_node, /*forbidden=*/true);
    auto cloned = fn.clone();
    auto bound = cloned->create(0);
    EXPECT_TRUE(bound->is_feasible(make_set_resource({4, 5})));
    EXPECT_FALSE(bound->is_feasible(make_set_resource({1, 4})));
}
