// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <gtest/gtest.h>

#include "rcspp/rcspp.hpp"

#include <memory>
#include <tuple>
#include <vector>

using namespace rcspp;

// Resource setup: ResourceTypeComposition<RealResource> with two components per label.
//   Component 0 (bucket_resource_index=0): value used for bucket assignment.
//   Component 1 (sort_resource_index=1):   value used for within-bucket ordering.
// Dominance: label A dominates B iff A.bucket <= B.bucket AND A.sort <= B.sort.

namespace {

using RComp = ResourceTypeComposition<RealResource>;
using BucketLabelList = LabelBuckets<RealResource, RealResource, RComp>;

std::unique_ptr<Resource<RComp>> make_resource(double bucket_val, double sort_val) {
    auto make_comp = [](double v) {
        auto r = std::make_unique<Resource<RealResource>>(
            std::make_unique<ValueDominanceFunction<RealResource>>(),
            std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
            std::make_unique<TrivialCostFunction<RealResource>>());
        r->set_value(v);
        return r;
    };
    std::tuple<std::vector<std::unique_ptr<Resource<RealResource>>>> components;
    std::get<0>(components).push_back(make_comp(bucket_val));
    std::get<0>(components).push_back(make_comp(sort_val));
    return std::make_unique<Resource<RComp>>(
        std::move(components),
        std::make_unique<CompositionDominanceFunction<RealResource>>(),
        std::make_unique<CompositionFeasibilityFunction<RealResource>>(),
        std::make_unique<CompositionCostFunction<RealResource>>(),
        0);
}

std::unique_ptr<Label<RComp>> make_label(size_t id, double bucket_val, double sort_val) {
    return std::make_unique<Label<RComp>>(id, make_resource(bucket_val, sort_val));
}

}  // namespace

// Within a single bucket labels are ordered ascending by sort resource.
TEST(LabelBuckets, SortOrder) {
    BucketLabelList bl(10, 0, 1);
    auto l0 = make_label(0, 5.0, 3.0);
    auto l1 = make_label(1, 5.0, 1.0);
    auto l2 = make_label(2, 5.0, 5.0);
    bl.add_label(l0.get());
    bl.add_label(l1.get());
    bl.add_label(l2.get());
    const auto& labels = bl.get_labels();
    ASSERT_EQ(labels.size(), 3u);
    auto it = labels.begin();
    EXPECT_EQ((*it)->id, 1u);  // sort=1
    ++it;
    EXPECT_EQ((*it)->id, 0u);  // sort=3
    ++it;
    EXPECT_EQ((*it)->id, 2u);  // sort=5
}

// Labels whose bucket resource falls outside the current bucket range go into separate buckets.
TEST(LabelBuckets, MultipleBuckets) {
    BucketLabelList bl(10, 0, 1);
    auto l0 = make_label(0, 5.0, 3.0);
    auto l1 = make_label(1, 20.0, 1.0);
    auto l2 = make_label(2, 35.0, 5.0);
    bl.add_label(l0.get());
    bl.add_label(l1.get());
    bl.add_label(l2.get());
    const auto& labels = bl.get_labels();
    ASSERT_EQ(labels.size(), 3u);
    auto it = labels.begin();
    EXPECT_EQ((*it)->id, 0u);
    ++it;
    EXPECT_EQ((*it)->id, 1u);
    ++it;
    EXPECT_EQ((*it)->id, 2u);
}

// remove_dominated_labels removes existing labels dominated by the new label.
TEST(LabelBuckets, RemoveDominated) {
    BucketLabelList bl(10, 0, 1);
    auto l0 = make_label(0, 5.0, 3.0);
    auto l1 = make_label(1, 5.0, 1.0);
    auto l2 = make_label(2, 20.0, 2.0);
    bl.add_label(l0.get());
    bl.add_label(l1.get());
    bl.add_label(l2.get());
    // dominator(3,2): dominates l0(5,3) and l2(20,2) but not l1(5,1) since sort 2 > 1.
    auto dominator = make_label(99, 3.0, 2.0);
    size_t removed = bl.remove_dominated_labels(*dominator);
    EXPECT_EQ(removed, 2u);
    const auto& labels = bl.get_labels();
    ASSERT_EQ(labels.size(), 1u);
    EXPECT_EQ(labels.front()->id, 1u);
}

// is_dominated returns true iff a label in the list dominates the query label.
TEST(LabelBuckets, IsDominated) {
    BucketLabelList bl(10, 0, 1);
    auto l0 = make_label(0, 2.0, 1.0);
    bl.add_label(l0.get());
    auto dominated = make_label(1, 3.0, 2.0);
    EXPECT_TRUE(bl.is_dominated(*dominated));
    auto not_dominated = make_label(2, 1.0, 0.0);
    EXPECT_FALSE(bl.is_dominated(*not_dominated));
}

// erase_label correctly removes the label and maintains bucket integrity.
TEST(LabelBuckets, Erase) {
    BucketLabelList bl(10, 0, 1);
    auto l0 = make_label(0, 5.0, 3.0);
    auto l1 = make_label(1, 5.0, 1.0);
    auto l2 = make_label(2, 5.0, 5.0);
    auto pos0 = bl.add_label(l0.get());
    auto pos1 = bl.add_label(l1.get());
    auto pos2 = bl.add_label(l2.get());

    // Sorted: l1(1), l0(3), l2(5). Erase middle l0.
    bl.erase_label(pos0);
    const auto& labels = bl.get_labels();
    ASSERT_EQ(labels.size(), 2u);
    auto it = labels.begin();
    EXPECT_EQ((*it)->id, 1u);
    ++it;
    EXPECT_EQ((*it)->id, 2u);

    // Erase bucket-begin l1; bucket begin must advance to l2.
    bl.erase_label(pos1);
    const auto& labels2 = bl.get_labels();
    ASSERT_EQ(labels2.size(), 1u);
    EXPECT_EQ(labels2.front()->id, 2u);

    // Erase last label; bucket should be removed.
    bl.erase_label(pos2);
    EXPECT_TRUE(bl.get_labels().empty());
}