// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Every concrete extension function must declare a backward_kind() other than Unspecified, except
// for the deliberate exceptions asserted below. A bidirectional solve refuses to start on an
// Unspecified component, so an omission would otherwise surface late as a setup failure.

#include <gtest/gtest.h>

#include <initializer_list>
#include <map>
#include <set>
#include <utility>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

// The whole table, checked at compile time. Uses `backward_kind_of_v` rather than `T::kind` so a
// class that publishes no constant fails the same way as one that publishes `Unspecified`.
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
static_assert(backward_kind_of_v<UnionExtensionFunction<SetResource<int>>> ==
              BackwardKind::ArcValue);
static_assert(backward_kind_of_v<IntersectionExtensionFunction<SetResource<int>>> ==
              BackwardKind::ArcValue);
static_assert(backward_kind_of_v<SubtractExtensionFunction<SetResource<int>>> ==
              BackwardKind::ArcValue);

// The deliberate Unspecified rows:
// - An unsigned time window cannot represent an unmeetable deadline, so it stays forward-only.
// - The base class publishes no constant, which also exercises the trait fallback.
static_assert(backward_kind_of_v<TimeWindowExtensionFunction<UIntResource>> ==
              BackwardKind::Unspecified);
static_assert(backward_kind_of_v<ExtensionFunction<RealResource>> == BackwardKind::Unspecified);

// Checks at compile time, over a sample grid, that x + arc <= theta iff x <= theta - arc, i.e.
// that the unclamped translation pair is a genuine inverse. The clamps are covered in
// test_threshold_form.hpp.
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

// The composition wrapper stays Unspecified: validation checks its components, not the wrapper.
TEST(BackwardKindDeclared, CompositionWrapperStaysUnspecified) {
    CompositionExtensionFunction<RealResource> composition;
    EXPECT_EQ(composition.backward_kind(), BackwardKind::Unspecified);
}

// The base-class default stays Unspecified, so an undeclared component fails loudly at setup.
TEST(BackwardKindDeclared, BaseDefaultRemainsUnspecified) {
    class UndeclaredExtensionFunction
        : public Clonable<UndeclaredExtensionFunction, ExtensionFunction<RealResource>> {
        public:
            void extend(const RealResource& /*resource*/, const RealResource& /*extender_value*/,
                        RealResource* /*extended_resource*/) override {}
    };

    EXPECT_EQ(UndeclaredExtensionFunction{}.backward_kind(), BackwardKind::Unspecified);
}

// What a static_assert cannot check: that the virtual backward_kind() matches the constant.
TEST(BackwardKindDeclared, ConstantAgreesWithTheVirtual) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}}};
    std::map<size_t, std::pair<unsigned int, unsigned int>> uwindows{{0, {0U, 100U}}};

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
}

// The trait falls back to Unspecified for a class that publishes no constant.
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
