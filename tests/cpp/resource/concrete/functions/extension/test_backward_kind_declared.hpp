// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Every concrete extension function must declare a backward_kind() other than Unspecified.
//
// Phase 11 refuses to start a bidirectional solve on any component still reporting Unspecified,
// so an accidental omission here would surface much later as a confusing setup failure. This
// single test guards the whole table: adding a new extension function without declaring its kind
// should fail right here.

#include <gtest/gtest.h>

#include <map>
#include <set>
#include <utility>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

// Accumulate: no feasibility bound, so extend_back is inherited from extend.
TEST(BackwardKindDeclared, AccumulateFunctions) {
    EXPECT_EQ(AdditionExtensionFunction<RealResource>{}.backward_kind(), BackwardKind::Accumulate);
    EXPECT_EQ(TrivialExtensionFunction<RealResource>{}.backward_kind(), BackwardKind::Accumulate);
}

// Threshold: stores a deadline/ceiling, so extend_back inverts extend and clamps.
TEST(BackwardKindDeclared, ThresholdFunctions) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}}};
    EXPECT_EQ(TimeWindowExtensionFunction<RealResource>{windows}.backward_kind(),
              BackwardKind::Threshold);
    EXPECT_EQ(BudgetExtensionFunction<RealResource>{}.backward_kind(), BackwardKind::Threshold);
}

// Mirror: a container holding the set seen on its own half.
TEST(BackwardKindDeclared, MirrorFunctions) {
    EXPECT_EQ(UnionExtensionFunction<SetResource<int>>{}.backward_kind(), BackwardKind::Mirror);
    EXPECT_EQ(IntersectionExtensionFunction<SetResource<int>>{}.backward_kind(),
              BackwardKind::Mirror);
    EXPECT_EQ(SubtractExtensionFunction<SetResource<int>>{}.backward_kind(), BackwardKind::Mirror);

    std::map<size_t, std::set<int>> ng_map{{0, {1, 2}}};
    EXPECT_EQ(NgPathExtensionFunction<SetResource<int>>{ng_map}.backward_kind(),
              BackwardKind::Mirror);
}

// The point of the table: nothing is left Unspecified.
TEST(BackwardKindDeclared, NoneAreUnspecified) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}}};
    std::map<size_t, std::set<int>> ng_map{{0, {1, 2}}};

    EXPECT_NE(AdditionExtensionFunction<RealResource>{}.backward_kind(), BackwardKind::Unspecified);
    EXPECT_NE(TrivialExtensionFunction<RealResource>{}.backward_kind(), BackwardKind::Unspecified);
    EXPECT_NE(TimeWindowExtensionFunction<RealResource>{windows}.backward_kind(),
              BackwardKind::Unspecified);
    EXPECT_NE(BudgetExtensionFunction<RealResource>{}.backward_kind(), BackwardKind::Unspecified);
    EXPECT_NE(UnionExtensionFunction<SetResource<int>>{}.backward_kind(),
              BackwardKind::Unspecified);
    EXPECT_NE(IntersectionExtensionFunction<SetResource<int>>{}.backward_kind(),
              BackwardKind::Unspecified);
    EXPECT_NE(SubtractExtensionFunction<SetResource<int>>{}.backward_kind(),
              BackwardKind::Unspecified);
    EXPECT_NE(NgPathExtensionFunction<SetResource<int>>{ng_map}.backward_kind(),
              BackwardKind::Unspecified);
}

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

// The constant and the virtual must never diverge. A form supplies both from one place, but a
// hand-written class can still get them out of step, and only this catches it.
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

// The unsigned time window is the row that earns its place: it is the only class whose kind
// differs between instantiations, so a mistake in the ternary would be invisible to the
// `real` row above.
TEST(BackwardKindDeclared, UnsignedTimeWindowDeclaresNothing) {
    static_assert(backward_kind_of_v<TimeWindowExtensionFunction<RealResource>> ==
                  BackwardKind::Threshold);
    static_assert(backward_kind_of_v<TimeWindowExtensionFunction<UIntResource>> ==
                  BackwardKind::Unspecified);
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
