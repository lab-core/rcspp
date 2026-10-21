// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Every concrete extension function must declare a directional_resource_kind() other than
// Unspecified, except for the deliberate exceptions asserted below. A bidirectional solve refuses
// to start on an Unspecified component, so an omission would otherwise surface late as a setup
// failure.

#include <gtest/gtest.h>

#include <initializer_list>
#include <map>
#include <set>
#include <utility>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

// The whole table, checked at compile time. Uses `directional_resource_kind_of_v` rather than
// `T::kind` so a class that publishes no constant fails the same way as one that publishes
// `Unspecified`.
static_assert(directional_resource_kind_of_v<AdditionExtensionFunction<RealResource>> ==
              DirectionalResourceKind::Accumulate);
static_assert(directional_resource_kind_of_v<TrivialExtensionFunction<RealResource>> ==
              DirectionalResourceKind::Accumulate);
static_assert(directional_resource_kind_of_v<CapacityExtensionFunction<RealResource>> ==
              DirectionalResourceKind::Threshold);
static_assert(directional_resource_kind_of_v<CapacityExtensionFunction<IntResource>> ==
              DirectionalResourceKind::Threshold);
static_assert(directional_resource_kind_of_v<TimeWindowExtensionFunction<RealResource>> ==
              DirectionalResourceKind::Threshold);
static_assert(directional_resource_kind_of_v<TimeWindowExtensionFunction<IntResource>> ==
              DirectionalResourceKind::Threshold);
static_assert(directional_resource_kind_of_v<UnionExtensionFunction<SetResource<int>>> ==
              DirectionalResourceKind::ArcValue);
static_assert(directional_resource_kind_of_v<IntersectionExtensionFunction<SetResource<int>>> ==
              DirectionalResourceKind::ArcValue);
static_assert(directional_resource_kind_of_v<SubtractExtensionFunction<SetResource<int>>> ==
              DirectionalResourceKind::ArcValue);
// ArcEndpoints, not ArcValue: ng-path reads node identities off the arc's endpoints rather than
// its value, which is what keeps a memory from containing its own node in both directions.
static_assert(directional_resource_kind_of_v<NgPathExtensionFunction<SetResource<int>>> ==
              DirectionalResourceKind::ArcEndpoints);

// The deliberate Unspecified rows:
// - An unsigned time window cannot represent an unmeetable deadline, so it stays forward-only.
// - The base class publishes no constant, which also exercises the trait fallback.
static_assert(directional_resource_kind_of_v<TimeWindowExtensionFunction<UIntResource>> ==
              DirectionalResourceKind::Unspecified);
static_assert(directional_resource_kind_of_v<ExtensionFunction<RealResource>> ==
              DirectionalResourceKind::Unspecified);

// Checks at compile time, over a sample grid, that x + arc <= theta iff x <= theta - arc, i.e.
// that the unclamped translation pair is a genuine inverse. The clamps are covered in
// test_threshold_extension_form.hpp.
template <typename ValueType>
consteval bool translation_biconditional_holds(std::initializer_list<ValueType> samples,
                                               std::initializer_list<ValueType> thetas,
                                               ValueType arc) {
    for (const ValueType theta : thetas) {
        for (const ValueType x : samples) {
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
TEST(DirectionalResourceKindDeclared, CompositionWrapperStaysUnspecified) {
    CompositionExtensionFunction<RealResource> composition;
    EXPECT_EQ(composition.directional_resource_kind(), DirectionalResourceKind::Unspecified);
}

// The base-class default stays Unspecified, so the model checks refuse an undeclared component.
TEST(DirectionalResourceKindDeclared, BaseDefaultRemainsUnspecified) {
    class UndeclaredExtensionFunction
        : public Clonable<UndeclaredExtensionFunction, ExtensionFunction<RealResource>> {
        public:
            void extend(const RealResource& /*resource*/, const RealResource& /*extender_value*/,
                        RealResource* /*extended_resource*/) override {}
    };

    EXPECT_EQ(UndeclaredExtensionFunction{}.directional_resource_kind(),
              DirectionalResourceKind::Unspecified);
}

// What a static_assert cannot check: that the virtual directional_resource_kind() matches the
// constant.
TEST(DirectionalResourceKindDeclared, ConstantAgreesWithTheVirtual) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}}};
    std::map<size_t, std::pair<unsigned int, unsigned int>> uwindows{{0, {0U, 100U}}};
    std::map<size_t, std::set<int>> ng_map{{0, {1, 2}}};

    EXPECT_EQ(AdditionExtensionFunction<RealResource>{}.directional_resource_kind(),
              AdditionExtensionFunction<RealResource>::kind);
    EXPECT_EQ(TrivialExtensionFunction<RealResource>{}.directional_resource_kind(),
              TrivialExtensionFunction<RealResource>::kind);
    EXPECT_EQ(TimeWindowExtensionFunction<RealResource>{windows}.directional_resource_kind(),
              TimeWindowExtensionFunction<RealResource>::kind);
    EXPECT_EQ(TimeWindowExtensionFunction<UIntResource>{uwindows}.directional_resource_kind(),
              TimeWindowExtensionFunction<UIntResource>::kind);
    EXPECT_EQ(CapacityExtensionFunction<RealResource>{10.0}.directional_resource_kind(),
              CapacityExtensionFunction<RealResource>::kind);
    EXPECT_EQ(UnionExtensionFunction<SetResource<int>>{}.directional_resource_kind(),
              UnionExtensionFunction<SetResource<int>>::kind);
    EXPECT_EQ(IntersectionExtensionFunction<SetResource<int>>{}.directional_resource_kind(),
              IntersectionExtensionFunction<SetResource<int>>::kind);
    EXPECT_EQ(SubtractExtensionFunction<SetResource<int>>{}.directional_resource_kind(),
              SubtractExtensionFunction<SetResource<int>>::kind);
    EXPECT_EQ(NgPathExtensionFunction<SetResource<int>>{ng_map}.directional_resource_kind(),
              NgPathExtensionFunction<SetResource<int>>::kind);
}

// The trait falls back to Unspecified for a class that publishes no constant.
TEST(DirectionalResourceKindDeclared, TraitFallsBackToUnspecified) {
    class NoConstant : public Clonable<NoConstant, ExtensionFunction<RealResource>> {
        public:
            void extend(const RealResource& /*resource*/, const RealResource& /*extender_value*/,
                        RealResource* /*extended_resource*/) override {}
    };

    static_assert(!DeclaresDirectionalResourceKind<NoConstant>);
    static_assert(directional_resource_kind_of_v<NoConstant> ==
                  DirectionalResourceKind::Unspecified);
    EXPECT_EQ(NoConstant{}.directional_resource_kind(), DirectionalResourceKind::Unspecified);
}
