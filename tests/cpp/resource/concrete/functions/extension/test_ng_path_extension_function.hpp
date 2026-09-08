// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <set>
#include <utility>

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

inline std::unique_ptr<rcspp::Resource<rcspp::SetResource<int>>> make_resource(
    const std::set<int>& values) {
    return std::make_unique<rcspp::Resource<rcspp::SetResource<int>>>(
        make_set_resource(values),
        std::make_unique<rcspp::InclusionDominanceFunction<rcspp::SetResource<int>>>(),
        std::make_unique<rcspp::TrivialFeasibilityFunction<rcspp::SetResource<int>>>(),
        std::make_unique<rcspp::TrivialCostFunction<rcspp::SetResource<int>>>());
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

// Backward extension uses ng_neighborhood_back_ loaded from destination, and adds the
// *destination* singleton.
//
// Driven through a real Extender: the arc carries the origin singleton {2}, which is the same
// object Extender forwards in BOTH directions. Handing extend_back a hand-built {3} instead --
// a value the real Extender never produces -- would bypass the contract and hide a defect here.
//
// extend_back on arc (2→3): N_3={2,4,5}; ({2,5} ∩ {2,4,5}) ∪ {3} = {2,3,5}.
TEST(NgPathExtensionFunction, PreprocessBackwardExtend) {
    test_util::TestArc<SetResource<int>> fixture(2, 3);
    Extender<SetResource<int>> proto_extender(
        ng_path_test::make_set_resource({2}),  // the arc's real value: the ORIGIN singleton
        std::make_unique<NgPathExtensionFunction<SetResource<int>>>(ng_path_test::sample_ng_map()),
        0);
    auto extender = proto_extender.clone(fixture.arc);

    auto old_ng = ng_path_test::make_resource({2, 5});
    auto extended = ng_path_test::make_resource({});
    extender->extend_back(*old_ng, extended.get());

    EXPECT_EQ(extended->get_value().get_value(), (std::set<int>{2, 3, 5}));
}

// The destination -- not the origin -- is the node a backward extension leaves, so it is the
// one that lands in the set.
//
// arc (1→2), arc value {1}: N_2={1,3,4}; ({4} ∩ {1,3,4}) ∪ {2} = {2,4}.
// Had extend_back used the arc's own extender value {1}, the result would be {1,4} instead --
// the node the label is arriving at, which the forward feasibility check at that node rejects.
TEST(NgPathExtensionFunction, BackwardExtendAddsDestinationNotOrigin) {
    test_util::TestArc<SetResource<int>> fixture(1, 2);
    Extender<SetResource<int>> proto_extender(
        ng_path_test::make_set_resource({1}),
        std::make_unique<NgPathExtensionFunction<SetResource<int>>>(ng_path_test::sample_ng_map()),
        0);
    auto extender = proto_extender.clone(fixture.arc);

    auto old_ng = ng_path_test::make_resource({4});
    auto extended = ng_path_test::make_resource({});
    extender->extend_back(*old_ng, extended.get());

    EXPECT_EQ(extended->get_value().get_value(), (std::set<int>{2, 4}));
    EXPECT_TRUE(extended->get_value().contains(2));   // destination present
    EXPECT_FALSE(extended->get_value().contains(1));  // origin absent
}

// Missing key (destination): ng_neighborhood_back_ stays empty, but back_self_ still holds the
// destination id, so it alone survives.
//
// extend_back on arc (2→77): ({2,5} ∩ {}) ∪ {77} = {77}.
TEST(NgPathExtensionFunction, PreprocessMissingDestination) {
    test_util::TestArc<SetResource<int>> fixture(2, 77);
    Extender<SetResource<int>> proto_extender(
        ng_path_test::make_set_resource({2}),
        std::make_unique<NgPathExtensionFunction<SetResource<int>>>(ng_path_test::sample_ng_map()),
        0);
    auto extender = proto_extender.clone(fixture.arc);

    auto old_ng = ng_path_test::make_resource({2, 5});
    auto extended = ng_path_test::make_resource({});
    extender->extend_back(*old_ng, extended.get());

    EXPECT_EQ(extended->get_value().get_value(), (std::set<int>{77}));
}

// The forward direction is unchanged and still uses the arc's own value.
TEST(NgPathExtensionFunction, ForwardExtendThroughExtenderUsesArcValue) {
    test_util::TestArc<SetResource<int>> fixture(1, 2);
    Extender<SetResource<int>> proto_extender(
        ng_path_test::make_set_resource({1}),
        std::make_unique<NgPathExtensionFunction<SetResource<int>>>(ng_path_test::sample_ng_map()),
        0);
    auto extender = proto_extender.clone(fixture.arc);

    auto old_ng = ng_path_test::make_resource({0});
    auto extended = ng_path_test::make_resource({});
    extender->extend(*old_ng, extended.get());

    // N_1={0,2,3}: ({0} ∩ {0,2,3}) ∪ {1} = {0,1}.
    EXPECT_EQ(extended->get_value().get_value(), (std::set<int>{0, 1}));
}

// The ng-set is a container whose node identity swaps between directions.
TEST(NgPathExtensionFunction, DeclaresMirrorBackwardKind) {
    NgPathExtensionFunction<SetResource<int>> proto(ng_path_test::sample_ng_map());
    EXPECT_EQ(proto.backward_kind(), BackwardKind::Mirror);
}
