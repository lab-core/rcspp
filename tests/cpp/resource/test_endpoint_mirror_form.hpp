// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// EndpointMirrorForm and DeclaredKindForm, tested as forms rather than through their users.
//
// A Side names two nodes, the one left and the one arrived at. The test double keeps both visible
// in its output, so a transposed make_side fails loudly.

#include <gtest/gtest.h>

#include <set>

#include "rcspp/rcspp.hpp"
#include "util/test_arc.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace mirror_form_test {

// N_k = {100 + k} and the node left is {k}, so each half of a Side names its own node.
//
// `apply` narrows then unions -- the opposite of the real ng formula -- so both halves of the Side
// survive into the output. This tests the form's plumbing, not ng semantics.
class LabelledMirror
    : public Clonable<LabelledMirror,
                      EndpointMirrorForm<SetResource<int>, ExtensionFunction<SetResource<int>>>,
                      ExtensionFunction<SetResource<int>>> {
    protected:
        using Side = typename EndpointMirrorForm<SetResource<int>,
                                                 ExtensionFunction<SetResource<int>>>::Side;

        void apply(const SetResource<int>& resource, SetResource<int>* extended_resource,
                   const Side& side) const final {
            auto narrowed = resource.get_intersection(side.arrival_neighborhood.get_value());
            extended_resource->set_value(side.node_left.get_union(narrowed));
        }

        [[nodiscard]] Side make_side(size_t node_left_id, size_t node_arrived_id) const final {
            Side side;
            side.node_left.set_value(std::set<int>{static_cast<int>(node_left_id)});
            side.arrival_neighborhood.set_value(
                std::set<int>{100 + static_cast<int>(node_arrived_id)});
            return side;
        }
};

}  // namespace mirror_form_test

// Forward leaves the origin and arrives at the destination; backward is the mirror.
TEST(EndpointMirrorForm, ForwardLeavesTheOriginAndArrivesAtTheDestination) {
    mirror_form_test::LabelledMirror proto;
    test_util::TestArc<SetResource<int>> fixture(/*origin_id=*/2, /*destination_id=*/7);
    auto fn = proto.create(fixture.arc);

    SetResource<int> in;
    in.set_value(std::set<int>{102, 107});
    const SetResource<int> ignored_arc_value;
    SetResource<int> out;

    // leaves 2, arrives at 7: ({102,107} n N_7={107}) u {2}
    fn->extend(in, ignored_arc_value, &out);
    EXPECT_EQ(out.get_value(), (std::set<int>{2, 107}));

    // leaves 7, arrives at 2: ({102,107} n N_2={102}) u {7}
    fn->extend_back(in, ignored_arc_value, &out);
    EXPECT_EQ(out.get_value(), (std::set<int>{7, 102}));
}

// The form ignores the arc's extender value in both directions: a value that would be visible if
// honoured is passed and must not appear.
TEST(EndpointMirrorForm, IgnoresTheArcValueInBothDirections) {
    mirror_form_test::LabelledMirror proto;
    test_util::TestArc<SetResource<int>> fixture(/*origin_id=*/2, /*destination_id=*/7);
    auto fn = proto.create(fixture.arc);

    SetResource<int> in;
    in.set_value(std::set<int>{102, 107});
    SetResource<int> misleading;
    misleading.set_value(std::set<int>{42});
    SetResource<int> out;

    fn->extend(in, misleading, &out);
    EXPECT_EQ(out.get_value(), (std::set<int>{2, 107}));  // no 42

    fn->extend_back(in, misleading, &out);
    EXPECT_EQ(out.get_value(), (std::set<int>{7, 102}));  // no 42
}

// A marker declares a kind without finalising extend, so a marker-based class still writes its
// own.
TEST(DeclaredKindForm, DeclaresWithoutFinalisingExtend) {
    static_assert(
        DeclaredKindForm<ExtensionFunction<RealResource>, BackwardKind::Accumulate>::kind ==
        BackwardKind::Accumulate);
    static_assert(
        DeclaredKindForm<ExtensionFunction<SetResource<int>>, BackwardKind::ArcValue>::kind ==
        BackwardKind::ArcValue);

    // The declaration survives the trip through the form.
    EXPECT_EQ(UnionExtensionFunction<SetResource<int>>{}.backward_kind(), BackwardKind::ArcValue);
    EXPECT_EQ(AdditionExtensionFunction<RealResource>{}.backward_kind(), BackwardKind::Accumulate);
}

// The marker and the form declare different container kinds. A marker-declared container takes
// the arc's value, identical in both directions, so backward its memory includes its own node;
// only EndpointMirrorForm swaps the endpoint.
TEST(EndpointMirrorForm, DeclaresADifferentKindFromTheMarker) {
    static_assert(EndpointMirrorForm<SetResource<int>, ExtensionFunction<SetResource<int>>>::kind ==
                  BackwardKind::EndpointMirror);
    static_assert(
        EndpointMirrorForm<SetResource<int>, ExtensionFunction<SetResource<int>>>::kind !=
        DeclaredKindForm<ExtensionFunction<SetResource<int>>, BackwardKind::ArcValue>::kind);

    // Both are containers, so both keep the forward dominance order and neither can be the clock.
    static_assert(is_container_kind(BackwardKind::ArcValue));
    static_assert(is_container_kind(BackwardKind::EndpointMirror));
    static_assert(!is_container_kind(BackwardKind::Threshold));
    static_assert(!is_container_kind(BackwardKind::Accumulate));

    EXPECT_EQ(mirror_form_test::LabelledMirror{}.backward_kind(), BackwardKind::EndpointMirror);
}
