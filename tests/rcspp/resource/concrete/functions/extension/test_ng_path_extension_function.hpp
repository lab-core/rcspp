// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/rcspp.hpp"

#include "util/test_arc.hpp"

#include <map>
#include <set>

using namespace rcspp;

// Named namespace (not anonymous) so helpers don't collide with the IFF test header
// that also defines make_set_resource in its own anonymous namespace.
namespace ng_path_test {

inline SetResource<int> make_set_resource(const std::set<int>& values) {
    SetResource<int> r;
    r.set_value(values);
    return r;
}

// Build the ng-path map used by the worked example in the analysis discussion.
inline std::map<size_t, std::set<int>> sample_ng_map() {
    return {
        {0, {1, 2, 3}},
        {1, {0, 2, 3}},
        {2, {1, 3, 4}},
        {3, {2, 4, 5}},
    };
}

}  // namespace ng_path_test

// Happy path: binding the prototype to arc (1 -> 2) via create() runs preprocess
// internally, loading ng_neighborhood_ from N_1 = {0, 2, 3}. extend then performs
// (old_ng intersect N_1) union {extender}. Confirms the bug fix (==/!=) actually
// loads the right neighborhood through the production binding path.
bool test_ng_path_preprocess_forward_extend() {
    NgPathExtensionFunction<SetResource<int>> proto(ng_path_test::sample_ng_map());

    test_util::TestArc<SetResource<int>> fixture(/*origin_id=*/1, /*destination_id=*/2);
    auto fn = proto.create(fixture.arc);

    auto old_ng       = ng_path_test::make_set_resource({0});  // label arriving at 1 carries {0}
    auto extender_val = ng_path_test::make_set_resource({1});  // visiting node 1 on this arc
    auto extended     = ng_path_test::make_set_resource({});

    fn->extend(old_ng, extender_val, &extended);

    // ({0} intersect {0, 2, 3}) union {1} = {0, 1}
    const std::set<int> expected = {0, 1};
    if (extended.get_value() != expected) {
        LOG_ERROR("test_ng_path_preprocess_forward_extend: expected {0,1}, got ",
                  extended.to_string(), "\n");
        return false;
    }
    return true;
}

// Same path two arcs later (2 -> 3): N_2 = {1, 3, 4} should drop the stale node 0.
bool test_ng_path_preprocess_drops_stale_node() {
    NgPathExtensionFunction<SetResource<int>> proto(ng_path_test::sample_ng_map());

    test_util::TestArc<SetResource<int>> fixture(/*origin_id=*/2, /*destination_id=*/3);
    auto fn = proto.create(fixture.arc);

    auto old_ng       = ng_path_test::make_set_resource({0, 1});  // label entering 2 carries {0, 1}
    auto extender_val = ng_path_test::make_set_resource({2});
    auto extended     = ng_path_test::make_set_resource({});

    fn->extend(old_ng, extender_val, &extended);

    // ({0,1} intersect {1,3,4}) union {2} = {1, 2}   — node 0 falls off
    const std::set<int> expected = {1, 2};
    if (extended.get_value() != expected) {
        LOG_ERROR("test_ng_path_preprocess_drops_stale_node: expected {1,2}, got ",
                  extended.to_string(), "\n");
        return false;
    }
    return true;
}

// Missing key (origin): preprocess must leave ng_neighborhood_ empty so the extension
// behaves as "no neighborhood constraint" rather than dereferencing end() (UB) or
// inheriting an unrelated value.
bool test_ng_path_preprocess_missing_origin() {
    NgPathExtensionFunction<SetResource<int>> proto(ng_path_test::sample_ng_map());

    test_util::TestArc<SetResource<int>> fixture(/*origin_id=*/99, /*destination_id=*/2);
    auto fn = proto.create(fixture.arc);

    auto old_ng       = ng_path_test::make_set_resource({0, 1, 2});
    auto extender_val = ng_path_test::make_set_resource({99});
    auto extended     = ng_path_test::make_set_resource({});

    fn->extend(old_ng, extender_val, &extended);

    // ({0,1,2} intersect {}) union {99} = {99}
    const std::set<int> expected = {99};
    if (extended.get_value() != expected) {
        LOG_ERROR("test_ng_path_preprocess_missing_origin: expected {99}, got ",
                  extended.to_string(), "\n");
        return false;
    }
    return true;
}

// Backward extension uses ng_neighborhood_back_, which is loaded from the destination.
bool test_ng_path_preprocess_backward_extend() {
    NgPathExtensionFunction<SetResource<int>> proto(ng_path_test::sample_ng_map());

    test_util::TestArc<SetResource<int>> fixture(/*origin_id=*/2, /*destination_id=*/3);
    auto fn = proto.create(fixture.arc);

    auto old_ng       = ng_path_test::make_set_resource({2, 5});
    auto extender_val = ng_path_test::make_set_resource({3});
    auto extended     = ng_path_test::make_set_resource({});

    fn->extend_back(old_ng, extender_val, &extended);

    // ({2,5} intersect N_3={2,4,5}) union {3} = {2, 3, 5}
    const std::set<int> expected = {2, 3, 5};
    if (extended.get_value() != expected) {
        LOG_ERROR("test_ng_path_preprocess_backward_extend: expected {2,3,5}, got ",
                  extended.to_string(), "\n");
        return false;
    }
    return true;
}

// Missing key (destination): ng_neighborhood_back_ stays empty.
bool test_ng_path_preprocess_missing_destination() {
    NgPathExtensionFunction<SetResource<int>> proto(ng_path_test::sample_ng_map());

    test_util::TestArc<SetResource<int>> fixture(/*origin_id=*/2, /*destination_id=*/77);
    auto fn = proto.create(fixture.arc);

    auto old_ng       = ng_path_test::make_set_resource({2, 5});
    auto extender_val = ng_path_test::make_set_resource({77});
    auto extended     = ng_path_test::make_set_resource({});

    fn->extend_back(old_ng, extender_val, &extended);

    // ({2,5} intersect {}) union {77} = {77}
    const std::set<int> expected = {77};
    if (extended.get_value() != expected) {
        LOG_ERROR("test_ng_path_preprocess_missing_destination: expected {77}, got ",
                  extended.to_string(), "\n");
        return false;
    }
    return true;
}

inline std::pair<int, int> all_tests_ng_path_extension_function() {
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

    run(test_ng_path_preprocess_forward_extend,    "test_ng_path_preprocess_forward_extend");
    run(test_ng_path_preprocess_drops_stale_node,  "test_ng_path_preprocess_drops_stale_node");
    run(test_ng_path_preprocess_missing_origin,    "test_ng_path_preprocess_missing_origin");
    run(test_ng_path_preprocess_backward_extend,   "test_ng_path_preprocess_backward_extend");
    run(test_ng_path_preprocess_missing_destination,
        "test_ng_path_preprocess_missing_destination");

    return {passed, total};
}
