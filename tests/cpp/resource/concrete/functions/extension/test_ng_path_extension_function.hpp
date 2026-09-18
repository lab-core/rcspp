// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The ng-path extension formula, pinned value by value.
//
//     memory' = ( memory u {node left} ) n ng(node ARRIVED at)
//
// Two things are asserted that were once the other way round and are now the whole point:
//
//   1. the narrowing reads the neighborhood of the node being ARRIVED at, not the one being left,
//      so the stored memory is what the label will carry OUT of the node it sits on. That is what
//      lets the bidirectional join compare a forward half against a backward half at the node
//      where they meet -- see merge_form.hpp;
//   2. the union happens BEFORE the intersection, so the node just left is itself subject to the
//      arrival node's filter, and a label arriving somewhere it still remembers keeps that node in
//      the result for IntersectionFeasibilityFunction to reject.
//
// Every expected value below differs from what the pre-narrowing formula produced, so these tests
// discriminate rather than merely describe; each carries the old value in its comment.

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

// Deliberately ragged: N_1 and N_2 differ, so a test can tell which of the two a step read. Every
// entry is self-inclusive, so nothing here depends on the singleton `make_side` forces in -- that
// is pinned separately by ArrivalNodeIsAlwaysInItsOwnNeighborhood.
inline std::map<size_t, std::set<int>> sample_ng_map() {
    return {
        {0, {0, 1, 2}},
        {1, {0, 1, 2, 3}},
        {2, {0, 1, 2, 3, 4}},
        {3, {2, 3, 4, 5}},
        {4, {3, 4, 5}},
    };
}

}  // namespace ng_path_test

// The narrowing reads the ARRIVAL node's neighborhood, not the node left's.
//
// arc (1 -> 2), memory {0,4}:  ({0,4} u {1}) n N_2={0,1,2,3,4} = {0,1,4}.
// Narrowing by the node left would give ({0,4} n N_1={0,1,2,3}) u {1} = {0,1}: node 4 is outside
// N_1 and inside N_2, so it survives here and did not before.
TEST(NgPathExtensionFunction, NarrowsByTheArrivalNeighborhood) {
    NgPathExtensionFunction<SetResource<int>> proto(ng_path_test::sample_ng_map());
    test_util::TestArc<SetResource<int>> fixture(1, 2);
    auto fn = proto.create(fixture.arc);

    auto old_ng = ng_path_test::make_set_resource({0, 4});
    auto extender_val = ng_path_test::make_set_resource({1});
    auto extended = ng_path_test::make_set_resource({});
    fn->extend(old_ng, extender_val, &extended);

    EXPECT_EQ(extended.get_value(), (std::set<int>{0, 1, 4}));
    EXPECT_TRUE(extended.contains(4)) << "4 is in N_2, the node arrived at, so it survives";
}

// The union comes first, so the node just LEFT is itself filtered by the arrival neighborhood.
//
// arc (0 -> 3), memory {2}:  ({2} u {0}) n N_3={2,3,4,5} = {2}.  Node 0 is added and immediately
// dropped. Intersecting first would give ({2} n N_0={0,1,2}) u {0} = {0,2}, keeping it.
TEST(NgPathExtensionFunction, TheNodeLeftIsItselfNarrowedByTheArrivalNeighborhood) {
    NgPathExtensionFunction<SetResource<int>> proto(ng_path_test::sample_ng_map());
    test_util::TestArc<SetResource<int>> fixture(0, 3);
    auto fn = proto.create(fixture.arc);

    auto old_ng = ng_path_test::make_set_resource({2});
    auto extender_val = ng_path_test::make_set_resource({0});
    auto extended = ng_path_test::make_set_resource({});
    fn->extend(old_ng, extender_val, &extended);

    EXPECT_EQ(extended.get_value(), (std::set<int>{2}));
    EXPECT_FALSE(extended.contains(0)) << "the node left is not exempt from the arrival filter";
}

// Stale nodes drop out as the walk moves away from them: the trace 0 -> 1 -> 2 -> 3.
TEST(NgPathExtensionFunction, DropsStaleNodesAlongAWalk) {
    NgPathExtensionFunction<SetResource<int>> proto(ng_path_test::sample_ng_map());
    const SetResource<int> unused;

    test_util::TestArc<SetResource<int>> first(0, 1);
    auto leave_zero = proto.create(first.arc);
    auto at_one = ng_path_test::make_set_resource({});
    leave_zero->extend(ng_path_test::make_set_resource({}), unused, &at_one);
    EXPECT_EQ(at_one.get_value(), (std::set<int>{0}));  // ({} u {0}) n N_1 = {0}

    test_util::TestArc<SetResource<int>> second(1, 2);
    auto leave_one = proto.create(second.arc);
    auto at_two = ng_path_test::make_set_resource({});
    leave_one->extend(at_one, unused, &at_two);
    EXPECT_EQ(at_two.get_value(), (std::set<int>{0, 1}));  // ({0} u {1}) n N_2 = {0,1}

    test_util::TestArc<SetResource<int>> third(2, 3);
    auto leave_two = proto.create(third.arc);
    auto at_three = ng_path_test::make_set_resource({});
    leave_two->extend(at_two, unused, &at_three);
    EXPECT_EQ(at_three.get_value(), (std::set<int>{2}));  // ({0,1} u {2}) n N_3={2,3,4,5} = {2}
    EXPECT_FALSE(at_three.contains(0)) << "0 is outside N_3 and the walk has moved past it";
    EXPECT_FALSE(at_three.contains(1)) << "1 is outside N_3";
}

// A node absent from the map narrows to {itself}: it forgets everything it arrived with, while
// still being able to see itself -- which is what the feasibility test at that node needs.
TEST(NgPathExtensionFunction, ArrivalNodeIsAlwaysInItsOwnNeighborhood) {
    NgPathExtensionFunction<SetResource<int>> proto(ng_path_test::sample_ng_map());
    test_util::TestArc<SetResource<int>> fixture(2, 99);  // 99 is not in the map
    auto fn = proto.create(fixture.arc);
    const SetResource<int> unused;

    // Nothing the label was carrying survives the arrival.
    auto extended = ng_path_test::make_set_resource({});
    fn->extend(ng_path_test::make_set_resource({1, 3}), unused, &extended);
    EXPECT_EQ(extended.get_value(), (std::set<int>{}));

    // But a label that still remembers 99 arrives at 99 WITH 99 in the set, which is precisely
    // what IntersectionFeasibilityFunction(forbidden={99}) rejects there. Drop the forced
    // self-inclusion and this becomes the empty set and the revisit goes unnoticed.
    fn->extend(ng_path_test::make_set_resource({99}), unused, &extended);
    EXPECT_EQ(extended.get_value(), (std::set<int>{99}));
}

// Backward: the node LEFT is the arc's destination, and the narrowing reads the ORIGIN.
//
// Driven through a real Extender, because the arc carries the origin singleton {2} and that is the
// same object Extender forwards in BOTH directions. Handing extend_back a hand-built value instead
// would bypass the contract and hide a defect here.
//
// arc (2 -> 3), memory {1,5}:  ({1,5} u {3}) n N_2={0,1,2,3,4} = {1,3}.
// Narrowing by the node left (3) would give ({1,5} n N_3={2,3,4,5}) u {3} = {3,5}.
TEST(NgPathExtensionFunction, BackwardLeavesTheDestinationAndNarrowsByTheOrigin) {
    test_util::TestArc<SetResource<int>> fixture(2, 3);
    Extender<SetResource<int>> proto_extender(
        ng_path_test::make_set_resource({2}),  // the arc's real value: the ORIGIN singleton
        std::make_unique<NgPathExtensionFunction<SetResource<int>>>(ng_path_test::sample_ng_map()),
        0);
    auto extender = proto_extender.clone(fixture.arc);

    auto old_ng = ng_path_test::make_resource({1, 5});
    auto extended = ng_path_test::make_resource({});
    extender->extend_back(*old_ng, extended.get());

    EXPECT_EQ(extended->get_value().get_value(), (std::set<int>{1, 3}));
    EXPECT_TRUE(extended->get_value().contains(3));   // the destination is the node left
    EXPECT_FALSE(extended->get_value().contains(5));  // 5 is outside N_2
}

// The destination -- not the origin -- is the node a backward extension leaves.
//
// arc (1 -> 2), memory {0,4}:  ({0,4} u {2}) n N_1={0,1,2,3} = {0,2}.
// Had extend_back added the arc's own extender value {1}, the node the label is *arriving* at
// would land in the set, which the backward feasibility check at that node then rejects.
TEST(NgPathExtensionFunction, BackwardExtendAddsDestinationNotOrigin) {
    test_util::TestArc<SetResource<int>> fixture(1, 2);
    Extender<SetResource<int>> proto_extender(
        ng_path_test::make_set_resource({1}),
        std::make_unique<NgPathExtensionFunction<SetResource<int>>>(ng_path_test::sample_ng_map()),
        0);
    auto extender = proto_extender.clone(fixture.arc);

    auto old_ng = ng_path_test::make_resource({0, 4});
    auto extended = ng_path_test::make_resource({});
    extender->extend_back(*old_ng, extended.get());

    EXPECT_EQ(extended->get_value().get_value(), (std::set<int>{0, 2}));
    EXPECT_TRUE(extended->get_value().contains(2));   // destination present
    EXPECT_FALSE(extended->get_value().contains(1));  // origin absent
}

// The forward direction through a real Extender agrees with the bare function object.
TEST(NgPathExtensionFunction, ForwardExtendThroughExtenderMatches) {
    test_util::TestArc<SetResource<int>> fixture(1, 2);
    Extender<SetResource<int>> proto_extender(
        ng_path_test::make_set_resource({1}),
        std::make_unique<NgPathExtensionFunction<SetResource<int>>>(ng_path_test::sample_ng_map()),
        0);
    auto extender = proto_extender.clone(fixture.arc);

    auto old_ng = ng_path_test::make_resource({0, 4});
    auto extended = ng_path_test::make_resource({});
    extender->extend(*old_ng, extended.get());

    // ({0,4} u {1}) n N_2={0,1,2,3,4} = {0,1,4}.
    EXPECT_EQ(extended->get_value().get_value(), (std::set<int>{0, 1, 4}));
}

// The ng-set is a container whose node identity swaps between directions.
//
// NodeMirror, not Mirror: the two are separate kinds because only this one gives a memory that
// excludes the node it sits on in both directions. A feasibility function forbidding each node at
// itself needs exactly that, and `UnionExtensionFunction` -- which declares plain Mirror because it
// accumulates the arc's direction-independent value -- does not supply it. See check 7 in
// backward_kind.hpp.
TEST(NgPathExtensionFunction, DeclaresNodeMirrorBackwardKind) {
    NgPathExtensionFunction<SetResource<int>> proto(ng_path_test::sample_ng_map());
    EXPECT_EQ(proto.backward_kind(), BackwardKind::NodeMirror);
    EXPECT_NE(proto.backward_kind(), UnionExtensionFunction<SetResource<int>>{}.backward_kind());
}

// The arc's extender value is ignored: the node added is the one the label LEAVES, derived from
// the arc's endpoints. This is the ng-path defect made inexpressible -- and it is a narrowing of
// what the forward direction used to accept, so it is asserted rather than left implicit.
TEST(NgPathExtensionFunction, ForwardIgnoresTheArcValueAndUsesTheOrigin) {
    NgPathExtensionFunction<SetResource<int>> proto(ng_path_test::sample_ng_map());
    test_util::TestArc<SetResource<int>> fixture(1, 2);
    auto fn = proto.create(fixture.arc);

    auto old_ng = ng_path_test::make_set_resource({0, 4});
    auto misleading = ng_path_test::make_set_resource({42});  // NOT the origin singleton
    auto extended = ng_path_test::make_set_resource({});
    fn->extend(old_ng, misleading, &extended);

    // ({0,4} u {1}) n N_2={0,1,2,3,4} = {0,1,4}; the 42 does not appear.
    EXPECT_EQ(extended.get_value(), (std::set<int>{0, 1, 4}));
}
