// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/rcspp.hpp"

#include <map>
#include <set>

using namespace rcspp;

namespace {

// Build a SetResource<int> populated with the given values.
SetResource<int> make_set_resource(const std::set<int>& values) {
    SetResource<int> r;
    r.set_value(values);
    return r;
}

}  // namespace

// Empty-map construction: the function should report every resource as feasible because
// no node ever populates `values_` (preprocess() leaves `empty_` true).
bool test_intersection_feasibility_empty_map() {
    IntersectionFeasibilityFunction<SetResource<int>, int> fn({});

    // No preprocess() call -> empty_ stays true (the default).
    if (!fn.is_feasible(make_set_resource({1, 2, 3}))) {
        LOG_ERROR("test_intersection_feasibility_empty_map: expected feasible for non-empty resource\n");
        return false;
    }
    if (!fn.is_feasible(make_set_resource({}))) {
        LOG_ERROR("test_intersection_feasibility_empty_map: expected feasible for empty resource\n");
        return false;
    }
    return true;
}

// `forbidden = true` (default): feasibility requires NO intersection with the per-node set.
bool test_intersection_feasibility_forbidden() {
    std::map<size_t, std::set<int>> per_node = {
        {0, {1, 2, 3}},
        {1, {7, 8, 9}},
    };
    IntersectionFeasibilityFunction<SetResource<int>, int> fn(per_node, /*forbidden=*/true);

    // Drive preprocess via the public reset hook (no friend access required).
    fn.reset(/*node_id=*/0);

    // Resource {4,5,6} shares nothing with {1,2,3} -> feasible.
    if (!fn.is_feasible(make_set_resource({4, 5, 6}))) {
        LOG_ERROR("test_intersection_feasibility_forbidden: expected feasible for disjoint resource\n");
        return false;
    }
    // Resource {3,4} intersects {1,2,3} -> infeasible.
    if (fn.is_feasible(make_set_resource({3, 4}))) {
        LOG_ERROR("test_intersection_feasibility_forbidden: expected infeasible for intersecting resource\n");
        return false;
    }

    // Switch node: now forbidden set is {7,8,9}.
    fn.reset(/*node_id=*/1);
    if (!fn.is_feasible(make_set_resource({1, 2, 3}))) {
        LOG_ERROR("test_intersection_feasibility_forbidden: expected feasible after switching node\n");
        return false;
    }
    if (fn.is_feasible(make_set_resource({0, 9}))) {
        LOG_ERROR("test_intersection_feasibility_forbidden: expected infeasible after switching node\n");
        return false;
    }
    return true;
}

// `forbidden = false` (required): feasibility requires AT LEAST ONE intersection.
bool test_intersection_feasibility_required() {
    std::map<size_t, std::set<int>> per_node = {
        {0, {1, 2, 3}},
    };
    IntersectionFeasibilityFunction<SetResource<int>, int> fn(per_node, /*forbidden=*/false);
    fn.reset(/*node_id=*/0);

    // Resource {2,5} intersects {1,2,3} -> feasible.
    if (!fn.is_feasible(make_set_resource({2, 5}))) {
        LOG_ERROR("test_intersection_feasibility_required: expected feasible for intersecting resource\n");
        return false;
    }
    // Resource {4,5} disjoint from {1,2,3} -> infeasible.
    if (fn.is_feasible(make_set_resource({4, 5}))) {
        LOG_ERROR("test_intersection_feasibility_required: expected infeasible for disjoint resource\n");
        return false;
    }
    return true;
}

// Unknown node-id: preprocess sets empty_ = true, so every resource is feasible.
bool test_intersection_feasibility_unknown_node() {
    std::map<size_t, std::set<int>> per_node = {
        {0, {1, 2, 3}},
    };
    IntersectionFeasibilityFunction<SetResource<int>, int> fn(per_node, /*forbidden=*/true);

    fn.reset(/*node_id=*/42);  // not in the map
    if (!fn.is_feasible(make_set_resource({1, 2, 3}))) {
        LOG_ERROR("test_intersection_feasibility_unknown_node: expected feasible for unknown node\n");
        return false;
    }

    // After visiting a known node we should once again enforce the rule.
    fn.reset(/*node_id=*/0);
    if (fn.is_feasible(make_set_resource({1}))) {
        LOG_ERROR("test_intersection_feasibility_unknown_node: expected infeasible after switching to known node\n");
        return false;
    }
    return true;
}

// Empty per-node set is treated the same as a missing entry (empty_ stays true).
bool test_intersection_feasibility_empty_per_node_set() {
    std::map<size_t, std::set<int>> per_node = {
        {0, {}},
    };
    IntersectionFeasibilityFunction<SetResource<int>, int> fn(per_node, /*forbidden=*/true);

    fn.reset(/*node_id=*/0);
    if (!fn.is_feasible(make_set_resource({1, 2}))) {
        LOG_ERROR("test_intersection_feasibility_empty_per_node_set: expected feasible when per-node set is empty\n");
        return false;
    }
    return true;
}

// Default ValueType deduction: `IntersectionFeasibilityFunction<SetResource<int>>` (no
// second template argument) must compile and behave identically to the explicit form -
// proves the default `typename ContainerResourceType::ValueType` resolves to `int`.
bool test_intersection_feasibility_default_value_type() {
    std::map<size_t, std::set<int>> per_node = {
        {0, {1, 2, 3}},
    };
    IntersectionFeasibilityFunction<SetResource<int>> fn(per_node, /*forbidden=*/true);
    fn.reset(/*node_id=*/0);

    if (!fn.is_feasible(make_set_resource({4, 5, 6}))) {
        LOG_ERROR("test_intersection_feasibility_default_value_type: expected feasible for disjoint\n");
        return false;
    }
    if (fn.is_feasible(make_set_resource({2, 7}))) {
        LOG_ERROR("test_intersection_feasibility_default_value_type: expected infeasible for intersecting\n");
        return false;
    }
    return true;
}

// Clone preserves behaviour (exercises the Clonable<Derived,Base> wiring after the override fix).
bool test_intersection_feasibility_clone() {
    std::map<size_t, std::set<int>> per_node = {
        {0, {1, 2, 3}},
    };
    IntersectionFeasibilityFunction<SetResource<int>, int> fn(per_node, /*forbidden=*/true);

    auto cloned = fn.clone();  // unique_ptr<FeasibilityFunction<SetResource<int>>>
    auto bound = cloned->create(/*node_id=*/0);

    if (!bound->is_feasible(make_set_resource({4, 5}))) {
        LOG_ERROR("test_intersection_feasibility_clone: cloned function failed disjoint case\n");
        return false;
    }
    if (bound->is_feasible(make_set_resource({1, 4}))) {
        LOG_ERROR("test_intersection_feasibility_clone: cloned function failed intersecting case\n");
        return false;
    }
    return true;
}

inline std::pair<int, int> all_tests_intersection_feasibility_function() {
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

    run(test_intersection_feasibility_empty_map,           "test_intersection_feasibility_empty_map");
    run(test_intersection_feasibility_forbidden,           "test_intersection_feasibility_forbidden");
    run(test_intersection_feasibility_required,            "test_intersection_feasibility_required");
    run(test_intersection_feasibility_unknown_node,        "test_intersection_feasibility_unknown_node");
    run(test_intersection_feasibility_empty_per_node_set,  "test_intersection_feasibility_empty_per_node_set");
    run(test_intersection_feasibility_default_value_type,  "test_intersection_feasibility_default_value_type");
    run(test_intersection_feasibility_clone,               "test_intersection_feasibility_clone");

    return {passed, total};
}
