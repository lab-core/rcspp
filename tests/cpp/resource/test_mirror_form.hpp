// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// NodeMirrorForm and DeclaredKindForm, tested as forms rather than through their users.
//
// The centrepiece is the mirror twin of test_threshold_form's
// PreprocessReadsDestinationForLowerAndOriginForUpper: a form whose per-node neighborhood names
// its own node, so a transposed make_side is visible in the result rather than silent. That
// transposition is the ng-path defect, and this is where it is asserted.
//
// A Side names TWO nodes: the one being left and the one being arrived at. The test double keeps
// both independently visible in its output, so swapping either half fails loudly.

#include <gtest/gtest.h>

#include <set>

#include "rcspp/rcspp.hpp"
#include "util/test_arc.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace mirror_form_test {

// N_k = {100 + k}, and the node left is {k}. Each half of a Side therefore names the node it came
// from, which is what makes a transposed lookup visible.
//
// `apply` here narrows first and unions second -- the OPPOSITE order from the real ng formula, on
// purpose: it keeps both halves of the Side in the output, so this tests the form's plumbing
// rather than the ng semantics. The order that the ng relaxation needs is asserted in
// test_ng_path_extension_function.hpp.
class LabelledMirror
    : public Clonable<LabelledMirror,
                      NodeMirrorForm<SetResource<int>, ExtensionFunction<SetResource<int>>>,
                      ExtensionFunction<SetResource<int>>> {
    protected:
        using Side =
            typename NodeMirrorForm<SetResource<int>, ExtensionFunction<SetResource<int>>>::Side;

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

// Forward leaves the ORIGIN and arrives at the DESTINATION; backward is the mirror. Both halves
// of the Side are visible in the output, so transposing either one fails loudly -- which is the
// point: it is the ng-path defect, asserted rather than reasoned about.
TEST(NodeMirrorForm, ForwardLeavesTheOriginAndArrivesAtTheDestination) {
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

// The form ignores the arc's extender value in both directions. For a node-identity mirror the
// arc value is the wrong input -- that is the finding, made structural -- so a value that would
// be visible if it were honoured is passed and must not appear.
TEST(NodeMirrorForm, IgnoresTheArcValueInBothDirections) {
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

// A marker declares a shape WITHOUT finalising extend, so a marker-based class still writes its
// own -- which the three container markers and AdditionExtensionFunction do.
TEST(DeclaredKindForm, DeclaresWithoutFinalisingExtend) {
    static_assert(
        DeclaredKindForm<ExtensionFunction<RealResource>, BackwardKind::Accumulate>::kind ==
        BackwardKind::Accumulate);
    static_assert(
        DeclaredKindForm<ExtensionFunction<SetResource<int>>, BackwardKind::Mirror>::kind ==
        BackwardKind::Mirror);

    // That each marker's own extend is still reached is covered by its existing tests; what is
    // checked here is that the declaration survives the trip through the form.
    EXPECT_EQ(UnionExtensionFunction<SetResource<int>>{}.backward_kind(), BackwardKind::Mirror);
    EXPECT_EQ(AdditionExtensionFunction<RealResource>{}.backward_kind(), BackwardKind::Accumulate);
}
