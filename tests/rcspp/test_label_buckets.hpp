// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

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
bool test_bucket_label_list_sort_order() {
    BucketLabelList bl(10, 0, 1);

    // Same bucket value so all three are unambiguously in the same bucket.
    auto l0 = make_label(0, 5.0, 3.0);
    auto l1 = make_label(1, 5.0, 1.0);
    auto l2 = make_label(2, 5.0, 5.0);

    bl.add_label(l0.get());
    bl.add_label(l1.get());
    bl.add_label(l2.get());

    const auto& labels = bl.get_labels();
    if (labels.size() != 3) {
        LOG_ERROR("test_bucket_label_list_sort_order: expected 3 labels, got ", labels.size(), "\n");
        return false;
    }
    auto it = labels.begin();
    // Expected ascending sort order: l1 (sort=1), l0 (sort=3), l2 (sort=5)
    if ((*it)->id != 1) {
        LOG_ERROR("test_bucket_label_list_sort_order: expected id=1 first, got ", (*it)->id, "\n");
        return false;
    }
    ++it;
    if ((*it)->id != 0) {
        LOG_ERROR("test_bucket_label_list_sort_order: expected id=0 second, got ", (*it)->id, "\n");
        return false;
    }
    ++it;
    if ((*it)->id != 2) {
        LOG_ERROR("test_bucket_label_list_sort_order: expected id=2 third, got ", (*it)->id, "\n");
        return false;
    }
    return true;
}

// Labels whose bucket resource falls outside the current bucket range go into separate buckets.
bool test_bucket_label_list_multiple_buckets() {
    BucketLabelList bl(10, 0, 1);

    // Three labels each >10 apart so each lands in its own bucket.
    auto l0 = make_label(0, 5.0, 3.0);
    auto l1 = make_label(1, 20.0, 1.0);
    auto l2 = make_label(2, 35.0, 5.0);

    bl.add_label(l0.get());
    bl.add_label(l1.get());
    bl.add_label(l2.get());

    const auto& labels = bl.get_labels();
    if (labels.size() != 3) {
        LOG_ERROR("test_bucket_label_list_multiple_buckets: expected 3, got ", labels.size(), "\n");
        return false;
    }
    // Global order follows bucket order: l0 (bucket=5), l1 (bucket=20), l2 (bucket=35)
    auto it = labels.begin();
    if ((*it)->id != 0) {
        LOG_ERROR("test_bucket_label_list_multiple_buckets: expected id=0 first, got ", (*it)->id, "\n");
        return false;
    }
    ++it;
    if ((*it)->id != 1) {
        LOG_ERROR("test_bucket_label_list_multiple_buckets: expected id=1 second, got ", (*it)->id, "\n");
        return false;
    }
    ++it;
    if ((*it)->id != 2) {
        LOG_ERROR("test_bucket_label_list_multiple_buckets: expected id=2 third, got ", (*it)->id, "\n");
        return false;
    }
    return true;
}

// remove_dominated_labels removes existing labels dominated by the new label.
bool test_bucket_label_list_remove_dominated() {
    BucketLabelList bl(10, 0, 1);

    // l0(5,3), l1(5,1) share a bucket; l2(20,2) is in a separate bucket.
    auto l0 = make_label(0, 5.0, 3.0);
    auto l1 = make_label(1, 5.0, 1.0);
    auto l2 = make_label(2, 20.0, 2.0);

    bl.add_label(l0.get());
    bl.add_label(l1.get());
    bl.add_label(l2.get());

    // Label (2, 0) dominates l0(5,3) and l2(20,2) but NOT l1(5,1) because sort 0 <= 1.
    // Actually (2,0) <= (5,1): 2<=5 ✓ and 0<=1 ✓ → dominates l1 too.
    // Use (3, 2): dominates l0(5,3) [3<=5, 2<=3] but not l1(5,1) [2<=1 fails] and l2(20,2) [3<=20, 2<=2].
    auto dominator = make_label(99, 3.0, 2.0);

    size_t removed = bl.remove_dominated_labels(*dominator);

    // l0 and l2 are dominated; l1 is not (sort 2 > 1).
    if (removed != 2) {
        LOG_ERROR("test_bucket_label_list_remove_dominated: expected 2 removed, got ", removed, "\n");
        return false;
    }
    const auto& labels = bl.get_labels();
    if (labels.size() != 1) {
        LOG_ERROR("test_bucket_label_list_remove_dominated: expected 1 remaining, got ", labels.size(), "\n");
        return false;
    }
    if (labels.front()->id != 1) {
        LOG_ERROR("test_bucket_label_list_remove_dominated: remaining label should be id=1, got ",
                  labels.front()->id, "\n");
        return false;
    }
    return true;
}

// is_dominated returns true iff a label in the list dominates the query label.
bool test_bucket_label_list_is_dominated() {
    BucketLabelList bl(10, 0, 1);

    auto l0 = make_label(0, 2.0, 1.0);  // (bucket=2, sort=1)
    bl.add_label(l0.get());

    // (3, 2) is dominated by (2, 1) since 2<=3 and 1<=2.
    auto dominated = make_label(1, 3.0, 2.0);
    if (!bl.is_dominated(*dominated)) {
        LOG_ERROR("test_bucket_label_list_is_dominated: (3,2) should be dominated by (2,1)\n");
        return false;
    }

    // (1, 0) is not dominated by (2, 1) since 1 < 2.
    auto not_dominated = make_label(2, 1.0, 0.0);
    if (bl.is_dominated(*not_dominated)) {
        LOG_ERROR("test_bucket_label_list_is_dominated: (1,0) should not be dominated by (2,1)\n");
        return false;
    }
    return true;
}

// erase_label correctly removes the label and maintains bucket integrity.
bool test_bucket_label_list_erase() {
    BucketLabelList bl(10, 0, 1);

    auto l0 = make_label(0, 5.0, 3.0);
    auto l1 = make_label(1, 5.0, 1.0);
    auto l2 = make_label(2, 5.0, 5.0);

    auto pos0 = bl.add_label(l0.get());
    auto pos1 = bl.add_label(l1.get());
    auto pos2 = bl.add_label(l2.get());

    // Labels sorted by sort: l1(1), l0(3), l2(5). pos1 is the bucket begin.
    // Erase the middle label l0.
    bl.erase_label(pos0);

    const auto& labels = bl.get_labels();
    if (labels.size() != 2) {
        LOG_ERROR("test_bucket_label_list_erase: expected 2 labels after erase, got ", labels.size(), "\n");
        return false;
    }
    auto it = labels.begin();
    if ((*it)->id != 1) {
        LOG_ERROR("test_bucket_label_list_erase: expected id=1 first, got ", (*it)->id, "\n");
        return false;
    }
    ++it;
    if ((*it)->id != 2) {
        LOG_ERROR("test_bucket_label_list_erase: expected id=2 second, got ", (*it)->id, "\n");
        return false;
    }

    // Now erase the bucket-begin label l1; bucket begin must advance to l2.
    bl.erase_label(pos1);

    const auto& labels2 = bl.get_labels();
    if (labels2.size() != 1 || labels2.front()->id != 2) {
        LOG_ERROR("test_bucket_label_list_erase: expected only id=2 after second erase\n");
        return false;
    }

    // Erase the last label; the bucket should be removed.
    bl.erase_label(pos2);
    if (!bl.get_labels().empty()) {
        LOG_ERROR("test_bucket_label_list_erase: expected empty list after last erase\n");
        return false;
    }
    return true;
}

std::pair<int, int> all_tests_label_buckets() {
    int passed = 0;
    int total = 0;

    auto run = [&](bool (*fn)(), const char* name) {
        LOG_INFO("Run test ", name, '\n');
        ++total;
        if (fn()) {
            ++passed;
        } else {
            LOG_ERROR("FAILED: ", name, "\n");
        }
    };

    run(test_bucket_label_list_sort_order,      "test_bucket_label_list_sort_order");
    run(test_bucket_label_list_multiple_buckets, "test_bucket_label_list_multiple_buckets");
    run(test_bucket_label_list_remove_dominated, "test_bucket_label_list_remove_dominated");
    run(test_bucket_label_list_is_dominated,     "test_bucket_label_list_is_dominated");
    run(test_bucket_label_list_erase,            "test_bucket_label_list_erase");

    return {passed, total};
}