// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// ThresholdForm / TranslationThresholdForm, tested as forms rather than through their users.
//
// Two reasons this file exists. The gate reason: the `if (lower_)` / `if (upper_)` false branches
// are reached by nothing in the tree, because both production threshold functions always return a
// bound. The design reason: the form exists to own the one direction-dependent decision -- which
// node each clamp reads -- so that decision is asserted here rather than reasoned about.

#include <gtest/gtest.h>

#include <optional>
#include <set>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/backward_contract.hpp"
#include "util/test_arc.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace threshold_form_test {

// A minimal threshold function that declines both clamps, so the unclamped arms of
// ThresholdForm::extend and ::extend_back are exercised. No production resource does this today;
// the branches exist because the form is meant to serve resources that will.
class UnclampedThreshold
    : public Clonable<UnclampedThreshold,
                      TranslationThresholdForm<RealResource, ExtensionFunction<RealResource>>,
                      ExtensionFunction<RealResource>> {
    protected:
        [[nodiscard]] std::optional<double> lower_bound_at(size_t /*node_id*/) const final {
            return std::nullopt;
        }

        [[nodiscard]] std::optional<double> upper_bound_at(size_t /*node_id*/) const final {
            return std::nullopt;
        }
};

// lower_bound_at(n) = 100n ; upper_bound_at(n) = 1000 + 100n. Both bounds name their own node,
// so a transposed lookup in preprocess is visible in the result rather than silent.
class LabelledThreshold
    : public Clonable<LabelledThreshold,
                      TranslationThresholdForm<RealResource, ExtensionFunction<RealResource>>,
                      ExtensionFunction<RealResource>> {
    protected:
        [[nodiscard]] std::optional<double> lower_bound_at(size_t node_id) const final {
            return 100.0 * static_cast<double>(node_id);
        }

        [[nodiscard]] std::optional<double> upper_bound_at(size_t node_id) const final {
            return 1000.0 + 100.0 * static_cast<double>(node_id);
        }
};

// An unsigned instantiation, to pin the saturating branch of TranslationThresholdForm at the
// level of the form. Its upper bound of 100 mirrors TimeWindowExtensionFunction's own unsigned
// test so the two results are directly comparable.
class UnsignedThreshold
    : public Clonable<UnsignedThreshold,
                      TranslationThresholdForm<UIntResource, ExtensionFunction<UIntResource>>,
                      ExtensionFunction<UIntResource>> {
    protected:
        [[nodiscard]] std::optional<unsigned int> lower_bound_at(size_t /*node_id*/) const final {
            return 0U;
        }

        [[nodiscard]] std::optional<unsigned int> upper_bound_at(size_t /*node_id*/) const final {
            return 100U;
        }
};

}  // namespace threshold_form_test

// Both clamps declined: covers the two `if (...)` FALSE branches, which nothing else reaches.
TEST(ThresholdForm, DeclinedClampsLeaveTheValueAlone) {
    threshold_form_test::UnclampedThreshold proto;
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    RealResource out;
    fn->extend(RealResource(-50.0), RealResource(30.0), &out);
    EXPECT_DOUBLE_EQ(out.get_value(), -20.0);  // no max(0, .) applied

    fn->extend_back(RealResource(-50.0), RealResource(30.0), &out);
    EXPECT_DOUBLE_EQ(out.get_value(), -80.0);  // no min(bound, .) applied
}

// The biconditional holds for any TranslationThresholdForm, not just for the two production
// classes -- which is the guarantee a resource inherits by choosing the form. Unclamped is the
// biconditional's own precondition, so UnclampedThreshold is the right subject.
TEST(ThresholdForm, SatisfiesTheBiconditionalGenerically) {
    threshold_form_test::UnclampedThreshold proto;
    test_util::TestArc<RealResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    test_util::check_backward_contract<ExtensionFunction<RealResource>, RealResource, double>(
        *fn,
        RealResource(7.0),
        /*samples=*/{0.0, 1.0, 17.0, 100.0},
        /*thetas=*/{0.0, 20.0, 60.0, 200.0});
}

// Forward arrives at the DESTINATION; backward arrives at the ORIGIN. Transposing the two
// lookups is exactly the class of defect this library had twice, so it is asserted rather than
// reasoned about.
TEST(ThresholdForm, PreprocessReadsDestinationForLowerAndOriginForUpper) {
    threshold_form_test::LabelledThreshold proto;
    test_util::TestArc<RealResource> fixture(/*origin_id=*/2, /*destination_id=*/7);
    auto fn = proto.create(fixture.arc);

    RealResource out;
    fn->extend(RealResource(0.0), RealResource(1.0), &out);
    EXPECT_DOUBLE_EQ(out.get_value(), 700.0);  // lower_bound_at(7); transposed would give 200

    fn->extend_back(RealResource(5000.0), RealResource(1.0), &out);
    EXPECT_DOUBLE_EQ(out.get_value(), 1200.0);  // upper_bound_at(2); transposed would give 1700
}

// The clamp discipline itself: forward clamps UP to the lower bound, backward DOWN to the upper
// one, and backward is never clamped up -- the last of which is what leaves an infeasible
// backward value visible to is_back_feasible.
TEST(ThresholdForm, ForwardClampsUpAndBackwardClampsDown) {
    threshold_form_test::LabelledThreshold proto;
    test_util::TestArc<RealResource> fixture(/*origin_id=*/2, /*destination_id=*/7);
    auto fn = proto.create(fixture.arc);

    RealResource out;
    // Forward: 900 + 1 = 901 > lower_bound_at(7) = 700, so the clamp does not bind.
    fn->extend(RealResource(900.0), RealResource(1.0), &out);
    EXPECT_DOUBLE_EQ(out.get_value(), 901.0);

    // Backward: 500 - 1 = 499 < upper_bound_at(2) = 1200, so the clamp does not bind -- and the
    // result is NOT lifted to lower_bound_at(2) = 200 either, because it never is.
    fn->extend_back(RealResource(500.0), RealResource(1.0), &out);
    EXPECT_DOUBLE_EQ(out.get_value(), 499.0);

    // And below the lower bound the backward value is left alone rather than raised.
    fn->extend_back(RealResource(100.0), RealResource(1.0), &out);
    EXPECT_DOUBLE_EQ(out.get_value(), 99.0);
}

// The translation pair: signed subtracts, unsigned saturates. A wrapping form would turn the
// first case into min(100, 4294967276) == 100, which reads as a very loose bound.
TEST(ThresholdForm, TranslationSaturatesOnUnsignedAndSubtractsOnSigned) {
    threshold_form_test::UnsignedThreshold proto;
    test_util::TestArc<UIntResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    UIntResource out;
    fn->extend_back(UIntResource(10U), UIntResource(30U), &out);
    EXPECT_EQ(out.get_value(), 0U);  // saturated, not wrapped

    fn->extend_back(UIntResource(90U), UIntResource(30U), &out);
    EXPECT_EQ(out.get_value(), 60U);  // the ordinary subtraction

    // An unsigned threshold cannot represent "this bound cannot be met", so it declines to
    // declare and a bidirectional solve refuses to start on it.
    static_assert(threshold_form_test::UnsignedThreshold::kind == BackwardKind::Unspecified);
    static_assert(threshold_form_test::UnclampedThreshold::kind == BackwardKind::Threshold);
    EXPECT_EQ(proto.backward_kind(), BackwardKind::Unspecified);
}
