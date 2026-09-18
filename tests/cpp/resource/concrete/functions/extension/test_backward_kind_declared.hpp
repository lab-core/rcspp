// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Every concrete extension function must declare a backward_kind() other than Unspecified.
//
// Phase 11 refuses to start a bidirectional solve on any component still reporting Unspecified,
// so an accidental omission here would surface much later as a confusing setup failure. The
// static_assert block below guards the whole table at compile time; the one runtime test that
// remains does the job a static_assert cannot -- see it for what that is.

#include <gtest/gtest.h>

#include <initializer_list>
#include <map>
#include <set>
#include <utility>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

// The whole table, as a compile-time guarantee. A new extension function that forgets to declare
// its shape fails HERE, naming the class -- rather than at a solve, naming a component index.
//
// These are `backward_kind_of_v`, not `T::kind`, so that a class which publishes no constant at
// all fails with the same message as one that publishes `Unspecified`.
static_assert(backward_kind_of_v<AdditionExtensionFunction<RealResource>> ==
              BackwardKind::Accumulate);
static_assert(backward_kind_of_v<TrivialExtensionFunction<RealResource>> ==
              BackwardKind::Accumulate);
static_assert(backward_kind_of_v<BudgetExtensionFunction<RealResource>> == BackwardKind::Threshold);
static_assert(backward_kind_of_v<BudgetExtensionFunction<IntResource>> == BackwardKind::Threshold);
static_assert(backward_kind_of_v<TimeWindowExtensionFunction<RealResource>> ==
              BackwardKind::Threshold);
static_assert(backward_kind_of_v<TimeWindowExtensionFunction<IntResource>> ==
              BackwardKind::Threshold);
static_assert(backward_kind_of_v<UnionExtensionFunction<SetResource<int>>> == BackwardKind::Mirror);
static_assert(backward_kind_of_v<IntersectionExtensionFunction<SetResource<int>>> ==
              BackwardKind::Mirror);
static_assert(backward_kind_of_v<SubtractExtensionFunction<SetResource<int>>> ==
              BackwardKind::Mirror);
// NodeMirror, not Mirror. The three rows above accumulate the arc's VALUE; this one reads node
// identities off the arc's ENDPOINTS, and only that gives a memory excluding the node it sits on in
// both directions. Merging these two rows back together re-opens F12 -- see check 7 in
// backward_kind.hpp.
static_assert(backward_kind_of_v<NgPathExtensionFunction<SetResource<int>>> ==
              BackwardKind::NodeMirror);

// The two deliberate Unspecified rows, asserted so that "removing" either reads as a decision.
//
// An unsigned time window cannot represent "this deadline cannot be met", so it declines to
// declare and a bidirectional solve refuses to start on it. This assertion is what makes an
// attempt to quietly promote it to Threshold fail at compile time. The base class publishes no
// constant at all, so the second row also exercises the trait fallback.
static_assert(backward_kind_of_v<TimeWindowExtensionFunction<UIntResource>> ==
              BackwardKind::Unspecified);
static_assert(backward_kind_of_v<ExtensionFunction<RealResource>> == BackwardKind::Unspecified);

// The threshold biconditional, over a sample grid, evaluated at compile time.
//
// This covers only the *unclamped* relation, which is the part that is a definition: both clamps
// encode a node bound and deliberately sit outside the inverse relation, so they are covered by
// the worked cases in test_threshold_form.hpp instead. It is a proof about the translation pair
// itself -- that (+, -) is a genuine inverse -- so any resource built on
// TranslationThresholdForm inherits a correct backward step. It does not replace
// test_util::check_backward_contract, which drives a real object through a real Extender and so
// also exercises preprocess, the clamps and the value type own arithmetic.
template <typename V>
consteval bool translation_biconditional_holds(std::initializer_list<V> samples,
                                               std::initializer_list<V> thetas, V arc) {
    for (const V theta : thetas) {
        for (const V x : samples) {
            const bool forward_fits = (x + arc) <= theta;
            const bool backward_fits = x <= (theta - arc);
            if (forward_fits != backward_fits) {
                return false;
            }
        }
    }
    return true;
}

static_assert(translation_biconditional_holds<int>({0, 1, 17, 100}, {0, 20, 60, 200}, 7));
static_assert(translation_biconditional_holds<double>({0.0, 1.0, 17.0, 100.0},
                                                      {0.0, 20.0, 60.0, 200.0}, 7.0));

// The three per-shape tests and the NoneAreUnspecified table that used to live here are now the
// static_assert block above -- which is strictly stronger, since it also covers the `int`
// instantiations no runtime test ever constructed. Do not restore them; the one runtime test
// worth having is ConstantAgreesWithTheVirtual, below.

// The composition wrapper is deliberately left Unspecified.
//
// CompositionExtensionFunction fans every operation out across components, and validation walks
// the *components* rather than the wrapper -- so the wrapper has no kind of its own to declare,
// and its inherited default is the correct answer rather than an omission.
TEST(BackwardKindDeclared, CompositionWrapperStaysUnspecified) {
    CompositionExtensionFunction<RealResource> composition;
    EXPECT_EQ(composition.backward_kind(), BackwardKind::Unspecified);
}

// The base-class default stays Unspecified: that is what phase 11 keys off, and what makes an
// undeclared component fail loudly at setup rather than return a plausible number.
TEST(BackwardKindDeclared, BaseDefaultRemainsUnspecified) {
    class UndeclaredExtensionFunction
        : public Clonable<UndeclaredExtensionFunction, ExtensionFunction<RealResource>> {
        public:
            void extend(const RealResource& /*resource*/, const RealResource& /*extender_value*/,
                        RealResource* /*extended_resource*/) override {}
    };

    EXPECT_EQ(UndeclaredExtensionFunction{}.backward_kind(), BackwardKind::Unspecified);
}

// The one thing a static_assert cannot check: that the *virtual* returns what the constant says.
//
// A form supplies both from one place, so for a migrated class this is belt-and-braces. It is
// not belt-and-braces for a class written by hand against ExtensionFunction directly -- the
// documented escape hatch -- where the two are independent statements.
TEST(BackwardKindDeclared, ConstantAgreesWithTheVirtual) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}}};
    std::map<size_t, std::pair<unsigned int, unsigned int>> uwindows{{0, {0U, 100U}}};
    std::map<size_t, std::set<int>> ng_map{{0, {1, 2}}};

    EXPECT_EQ(AdditionExtensionFunction<RealResource>{}.backward_kind(),
              AdditionExtensionFunction<RealResource>::kind);
    EXPECT_EQ(TrivialExtensionFunction<RealResource>{}.backward_kind(),
              TrivialExtensionFunction<RealResource>::kind);
    EXPECT_EQ(TimeWindowExtensionFunction<RealResource>{windows}.backward_kind(),
              TimeWindowExtensionFunction<RealResource>::kind);
    EXPECT_EQ(TimeWindowExtensionFunction<UIntResource>{uwindows}.backward_kind(),
              TimeWindowExtensionFunction<UIntResource>::kind);
    EXPECT_EQ(BudgetExtensionFunction<RealResource>{}.backward_kind(),
              BudgetExtensionFunction<RealResource>::kind);
    EXPECT_EQ(UnionExtensionFunction<SetResource<int>>{}.backward_kind(),
              UnionExtensionFunction<SetResource<int>>::kind);
    EXPECT_EQ(IntersectionExtensionFunction<SetResource<int>>{}.backward_kind(),
              IntersectionExtensionFunction<SetResource<int>>::kind);
    EXPECT_EQ(SubtractExtensionFunction<SetResource<int>>{}.backward_kind(),
              SubtractExtensionFunction<SetResource<int>>::kind);
    EXPECT_EQ(NgPathExtensionFunction<SetResource<int>>{ng_map}.backward_kind(),
              NgPathExtensionFunction<SetResource<int>>::kind);
}

// The trait tolerates a class that publishes nothing -- which the base and the composition
// wrapper legitimately do, forever.
TEST(BackwardKindDeclared, TraitFallsBackToUnspecified) {
    class NoConstant : public Clonable<NoConstant, ExtensionFunction<RealResource>> {
        public:
            void extend(const RealResource& /*resource*/, const RealResource& /*extender_value*/,
                        RealResource* /*extended_resource*/) override {}
    };

    static_assert(!DeclaresBackwardKind<NoConstant>);
    static_assert(backward_kind_of_v<NoConstant> == BackwardKind::Unspecified);
    EXPECT_EQ(NoConstant{}.backward_kind(), BackwardKind::Unspecified);
}
