// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <gtest/gtest.h>

#include <memory>
#include <tuple>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;

namespace {
using LabelRefRComp = ResourceTypeComposition<RealResource>;

// Minimal single-component label; only its prev_label / ref_count bookkeeping is exercised here.
std::unique_ptr<Label<LabelRefRComp>> make_ref_label(size_t id) {
    auto component = std::make_unique<Resource<RealResource>>(
        std::make_unique<ValueDominanceFunction<RealResource>>(),
        std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<TrivialCostFunction<RealResource>>());
    std::tuple<std::vector<std::unique_ptr<Resource<RealResource>>>> components;
    std::get<0>(components).push_back(std::move(component));
    auto resource = std::make_unique<Resource<LabelRefRComp>>(
        std::move(components),
        std::make_unique<CompositionDominanceFunction<RealResource>>(),
        std::make_unique<CompositionFeasibilityFunction<RealResource>>(),
        std::make_unique<CompositionCostFunction<RealResource>>(),
        0);
    return std::make_unique<Label<LabelRefRComp>>(id, std::move(resource));
}
}  // namespace

// set_prev_label() increments the predecessor's ref_count once per live successor; that counter
// must be wide enough to hold every simultaneously-live successor. A high-out-degree node can have
// far more than 255 successors, so an 8-bit counter would wrap to 0 and make release_with_ref_count
// free a still-referenced predecessor. Drive 300 (> 255) successors and check the count does not
// wrap.
TEST(Label, RefCountDoesNotWrapPast255) {
    auto predecessor = make_ref_label(0);
    EXPECT_EQ(predecessor->ref_count, 0u);

    constexpr unsigned kChildren = 300;  // > 255 (uint8_t would wrap to 300 % 256 == 44)
    std::vector<std::unique_ptr<Label<LabelRefRComp>>> children;
    children.reserve(kChildren);
    for (unsigned i = 0; i < kChildren; ++i) {
        children.push_back(make_ref_label(i + 1));
        children.back()->set_prev_label(predecessor.get());
    }

    EXPECT_EQ(predecessor->ref_count, kChildren);
    EXPECT_GT(predecessor->ref_count, 255u);
}
