// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <gtest/gtest.h>

#include <map>
#include <set>

#include "rcspp/rcspp.hpp"
#include "util/test_arc.hpp"

using namespace rcspp;

// Named namespace (not anonymous) so helpers don't collide with the IFF test header
// that also defines make_set_resource in its own anonymous namespace.
namespace ng_path_test {

inline SetResource<int> make_set_resource(const std::set<int>& values) {
    SetResource<int> r;
    r.set_value(values);
    return r;
}

inline std::map<size_t, std::set<int>> sample_ng_map() {
    return {
        {0, {1, 2, 3}},
        {1, {0, 2, 3}},
        {2, {1, 3, 4}},
        {3, {2, 4, 5}},
    };
}

}  // namespace ng_path_test

// Happy path: binding the prototype to arc (1→2) loads N_1={0,2,3}.
// extend: ({0} ∩ {0,2,3}) ∪ {1} = {0,1}.
TEST(NgPathExtensionFunction, PreprocessForwardExtend) {
    NgPathExtensionFunction<SetResource<int>> proto(ng_path_test::sample_ng_map());
    test_util::TestArc<SetResource<int>> fixture(1, 2);
    auto fn = proto.create(fixture.arc);

    auto old_ng = ng_path_test::make_set_resource({0});
    auto extender_val = ng_path_test::make_set_resource({1});
    auto extended = ng_path_test::make_set_resource({});
    fn->extend(old_ng, extender_val, &extended);

    EXPECT_EQ(extended.get_value(), (std::set<int>{0, 1}));
}

// Same path two arcs later (2→3): N_2={1,3,4} drops stale node 0.
// extend: ({0,1} ∩ {1,3,4}) ∪ {2} = {1,2}.
TEST(NgPathExtensionFunction, PreprocessDropsStaleNode) {
    NgPathExtensionFunction<SetResource<int>> proto(ng_path_test::sample_ng_map());
    test_util::TestArc<SetResource<int>> fixture(2, 3);
    auto fn = proto.create(fixture.arc);

    auto old_ng = ng_path_test::make_set_resource({0, 1});
    auto extender_val = ng_path_test::make_set_resource({2});
    auto extended = ng_path_test::make_set_resource({});
    fn->extend(old_ng, extender_val, &extended);

    EXPECT_EQ(extended.get_value(), (std::set<int>{1, 2}));
}

// Missing key (origin): ng_neighborhood_ stays empty.
// extend: ({0,1,2} ∩ {}) ∪ {99} = {99}.
TEST(NgPathExtensionFunction, PreprocessMissingOrigin) {
    NgPathExtensionFunction<SetResource<int>> proto(ng_path_test::sample_ng_map());
    test_util::TestArc<SetResource<int>> fixture(99, 2);
    auto fn = proto.create(fixture.arc);

    auto old_ng = ng_path_test::make_set_resource({0, 1, 2});
    auto extender_val = ng_path_test::make_set_resource({99});
    auto extended = ng_path_test::make_set_resource({});
    fn->extend(old_ng, extender_val, &extended);

    EXPECT_EQ(extended.get_value(), (std::set<int>{99}));
}

// Backward extension uses ng_neighborhood_back_ loaded from destination.
// extend_back on arc (2→3): N_3={2,4,5}; ({2,5} ∩ {2,4,5}) ∪ {3} = {2,3,5}.
TEST(NgPathExtensionFunction, PreprocessBackwardExtend) {
    NgPathExtensionFunction<SetResource<int>> proto(ng_path_test::sample_ng_map());
    test_util::TestArc<SetResource<int>> fixture(2, 3);
    auto fn = proto.create(fixture.arc);

    auto old_ng = ng_path_test::make_set_resource({2, 5});
    auto extender_val = ng_path_test::make_set_resource({3});
    auto extended = ng_path_test::make_set_resource({});
    fn->extend_back(old_ng, extender_val, &extended);

    EXPECT_EQ(extended.get_value(), (std::set<int>{2, 3, 5}));
}

// Missing key (destination): ng_neighborhood_back_ stays empty.
// extend_back: ({2,5} ∩ {}) ∪ {77} = {77}.
TEST(NgPathExtensionFunction, PreprocessMissingDestination) {
    NgPathExtensionFunction<SetResource<int>> proto(ng_path_test::sample_ng_map());
    test_util::TestArc<SetResource<int>> fixture(2, 77);
    auto fn = proto.create(fixture.arc);

    auto old_ng = ng_path_test::make_set_resource({2, 5});
    auto extender_val = ng_path_test::make_set_resource({77});
    auto extended = ng_path_test::make_set_resource({});
    fn->extend_back(old_ng, extender_val, &extended);

    EXPECT_EQ(extended.get_value(), (std::set<int>{77}));
}
