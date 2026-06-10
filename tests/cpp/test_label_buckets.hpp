// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <gtest/gtest.h>

#include <memory>
#include <tuple>
#include <vector>

#include "rcspp/rcspp.hpp"

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

// remove_dominated_labels with labels in multiple buckets.
// Binary-search fast-path skips buckets that are entirely "after" the dominator.
TEST(LabelBuckets, RemoveDominatedMultiBucket) {
    // range=10: bucket [0,10], [15,25].
    // Dominator at bucket=12 -> is_after_bucket check: bucket [0,10] has begin=0;
    // 0 < 12-10=2? YES -> skipped by binary search.  Only bucket [15,25] is examined.
    BucketLabelList bl(10, 0, 1);
    auto l0 = make_label(0, 5.0, 3.0);   // bucket [0-10], sort=3
    auto l1 = make_label(1, 20.0, 1.0);  // bucket [20-30], sort=1
    auto l2 = make_label(2, 20.0, 4.0);  // bucket [20-30], sort=4
    bl.add_label(l0.get());
    bl.add_label(l1.get());
    bl.add_label(l2.get());

    // Dominator(12, 2): bucket=12 means binary search skips bucket [0-10] (5 < 12-10=2? NO,
    // actually 5 >= 2 so it is NOT skipped — let's pick a value that does skip it).
    // Actually with begin=5 and range=10: is_after_bucket(12) = 5 < 12-10=2? NO.
    // Use dominator at bucket=18 to skip bucket [5-15] (begin=5, 5 < 18-10=8 -> YES, skipped).
    // Dominator(18, 2) dominates l2(20,4) [18<=20, 2<=4] but not l1(20,1) [2>1].
    // l0(5,3) is in the skipped bucket.
    auto dominator = make_label(99, 18.0, 2.0);
    size_t removed = bl.remove_dominated_labels(*dominator);
    EXPECT_EQ(removed, 1u);  // only l2
    const auto& labels = bl.get_labels();
    ASSERT_EQ(labels.size(), 2u);
    auto it = labels.begin();
    EXPECT_EQ((*it)->id, 0u);  // l0 untouched (skipped bucket)
    ++it;
    EXPECT_EQ((*it)->id, 1u);  // l1 not dominated (sort 2 > 1)
}

// is_dominated with labels in multiple buckets.
// Binary-search fast-path skips upper buckets that cannot dominate the query.
TEST(LabelBuckets, IsDominatedMultiBucket) {
    // Buckets: [5-15] contains l0(5,3); [20-30] contains l1(20,1).
    BucketLabelList bl(10, 0, 1);
    auto l0 = make_label(0, 5.0, 3.0);
    auto l1 = make_label(1, 20.0, 1.0);
    bl.add_label(l0.get());
    bl.add_label(l1.get());

    // Query(15, 2): binary search finds first bucket where begin_value >= 15.
    // Bucket [5-15] has begin=5 (5 >= 15? NO). Bucket [20-30] has begin=20 (20 >= 15? YES).
    // end_idx=1 -> only bucket [5-15] is checked.
    // l0(5,3) <= Query(15,2)? 5<=15 AND 3<=2? NO. Sort pruning: !(3<=2)=true. Not dominated.
    auto query_not_dominated = make_label(2, 15.0, 2.0);
    EXPECT_FALSE(bl.is_dominated(*query_not_dominated));

    // Query(25, 4): end_idx=2 -> both buckets checked.
    // Bucket [5-15]: l0(5,3) <= (25,4)? 5<=25 AND 3<=4? YES -> dominated.
    auto query_dominated = make_label(3, 25.0, 4.0);
    EXPECT_TRUE(bl.is_dominated(*query_dominated));
}

// erase_label of a bucket begin in a multi-bucket scenario.
// Verifies that begin_label_to_bucket_idx_ stays consistent after erase.
TEST(LabelBuckets, EraseBeginMultiBucket) {
    BucketLabelList bl(10, 0, 1);
    // Bucket [5-15]: l1(sort=1) is begin, l0(sort=3) interior.
    // Bucket [20-30]: l2 is begin.
    auto l0 = make_label(0, 5.0, 3.0);
    auto l1 = make_label(1, 5.0, 1.0);
    auto l2 = make_label(2, 20.0, 2.0);
    bl.add_label(l0.get());
    auto pos1 = bl.add_label(l1.get());  // becomes bucket [5-15] begin (sort=1 < 3)
    bl.add_label(l2.get());

    // Erase l1 (bucket begin of bucket [5-15]); begin should advance to l0.
    bl.erase_label(pos1);
    const auto& labels = bl.get_labels();
    ASSERT_EQ(labels.size(), 2u);
    EXPECT_EQ(labels.front()->id, 0u);  // l0 is now the begin of bucket [5-15]
    EXPECT_EQ(labels.back()->id, 2u);

    // After erasing l1 the is_dominated path should still work correctly:
    // l0(5,3) in bucket [5-15] can dominate query(10,4).
    auto query = make_label(99, 10.0, 4.0);
    EXPECT_TRUE(bl.is_dominated(*query));
}

// suggest_range returns a range calibrated for the requested number of buckets.
TEST(LabelBuckets, SuggestRange) {
    BucketLabelList bl(10, 0, 1);
    // Add 7 labels, each 15 apart; with range=10 each lands in its own bucket.
    std::vector<std::unique_ptr<Label<RComp>>> labels_storage;
    for (int i = 0; i < 7; ++i) {
        labels_storage.push_back(make_label(static_cast<size_t>(i), i * 15.0, 0.0));
        bl.add_label(labels_storage.back().get());
    }
    // max_live_buckets_ == 7, range_buckets_ == 10 -> estimated_span = 70.
    // suggest_range(7)  -> (70 + 6) / 7  = 10 (same number of buckets -> same range)
    // suggest_range(14) -> (70 + 13) / 14 = 5  (twice as many buckets -> half the range)
    // suggest_range(35) -> (70 + 34) / 35 = 2
    EXPECT_EQ(bl.suggest_range(7), 10u);
    EXPECT_EQ(bl.suggest_range(14), 5u);
    EXPECT_EQ(bl.suggest_range(35), 2u);
    // Edge: target=0 returns unchanged range_buckets_.
    EXPECT_EQ(bl.suggest_range(0), 10u);
}
