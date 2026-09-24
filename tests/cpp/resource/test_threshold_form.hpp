// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// ThresholdForm / TranslationThresholdForm, tested directly: the declined-clamp branches that no
// production resource reaches, and which node each clamp reads in each direction.

#include <gtest/gtest.h>

#include <limits>
#include <optional>
#include <set>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/backward_contract.hpp"
#include "util/test_arc.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace threshold_form_test {

// Declines both clamps, exercising the unclamped arms of extend and extend_back.
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

// lower_bound_at(n) = 100n ; upper_bound_at(n) = 1000 + 100n, so a transposed node lookup shows
// up in the result.
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

// An unsigned instantiation, to pin the saturating branch of TranslationThresholdForm.
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

// Both clamps declined: the value passes through unclamped.
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

// The backward contract holds for any TranslationThresholdForm, checked on the unclamped one.
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

// Forward clamps with the destination's lower bound; backward with the origin's upper bound.
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

// Forward clamps up to the lower bound, backward clamps down to the upper bound, and backward is
// never clamped up: below the origin's lower bound it is unmeetable, and stays so.
TEST(ThresholdForm, ForwardClampsUpAndBackwardClampsDown) {
    threshold_form_test::LabelledThreshold proto;
    test_util::TestArc<RealResource> fixture(/*origin_id=*/2, /*destination_id=*/7);
    auto fn = proto.create(fixture.arc);

    RealResource out;
    // Forward: 900 + 1 = 901 > lower_bound_at(7) = 700, so the clamp does not bind.
    fn->extend(RealResource(900.0), RealResource(1.0), &out);
    EXPECT_DOUBLE_EQ(out.get_value(), 901.0);

    // Backward: 500 - 1 = 499 < upper_bound_at(2) = 1200, so the clamp does not bind.
    fn->extend_back(RealResource(500.0), RealResource(1.0), &out);
    EXPECT_DOUBLE_EQ(out.get_value(), 499.0);

    // Below lower_bound_at(2) = 200 the backward value is not raised: no arrival meets it.
    fn->extend_back(RealResource(100.0), RealResource(1.0), &out);
    EXPECT_EQ(out.get_value(), std::numeric_limits<double>::lowest());

    // And it stays unmeetable, whatever the arc.
    fn->extend_back(out, RealResource(-1000.0), &out);
    EXPECT_EQ(out.get_value(), std::numeric_limits<double>::lowest());
}

// Unsigned subtraction saturates at zero rather than wrapping to a huge, very loose bound.
TEST(ThresholdForm, TranslationSaturatesOnUnsignedAndSubtractsOnSigned) {
    threshold_form_test::UnsignedThreshold proto;
    test_util::TestArc<UIntResource> fixture(0, 1);
    auto fn = proto.create(fixture.arc);

    UIntResource out;
    fn->extend_back(UIntResource(10U), UIntResource(30U), &out);
    EXPECT_EQ(out.get_value(), 0U);  // saturated, not wrapped

    fn->extend_back(UIntResource(90U), UIntResource(30U), &out);
    EXPECT_EQ(out.get_value(), 60U);  // the ordinary subtraction

    // An unsigned threshold cannot represent an unmeetable bound, so it stays Unspecified.
    static_assert(threshold_form_test::UnsignedThreshold::kind == BackwardKind::Unspecified);
    static_assert(threshold_form_test::UnclampedThreshold::kind == BackwardKind::Threshold);
    EXPECT_EQ(proto.backward_kind(), BackwardKind::Unspecified);
}
