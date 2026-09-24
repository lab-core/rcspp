// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Where a backward label starts: where the extension of an arc entering its sink says
// (ExtensionFunction::start_back). A threshold starts at the sink's upper bound; every other form
// leaves the type default. Tests check the start value directly, since a wrong start in a solve
// just shows up as "no solutions".

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <tuple>
#include <utility>

#include "rcspp/rcspp.hpp"
#include "util/test_arc.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace back_start_test {

constexpr double kTolerance = 1e-12;
constexpr double kSinkAClose = 100.0;
constexpr double kSinkBClose = 140.0;
constexpr double kCapacity = 90.0;
constexpr double kNonZero = 42.0;

/// @brief The value a backward label starting at @p sink holds, as an arc entering @p sink
///        starts it from @p before.
template <typename ResourceType>
ResourceType start_at(ExtensionFunction<ResourceType>& prototype, size_t sink,
                      ResourceType before = ResourceType{}) {
    test_util::TestArc<ResourceType> into_sink(sink + 1, sink);
    auto extension = prototype.create(into_sink.arc);
    extension->start_back(&before);
    return before;
}

/// @brief A @c ThresholdForm that states no bound at any node.
class Unbounded
    : public Clonable<Unbounded,
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

/// @brief A @c ThresholdForm that caps every node at @ref kCapacity and has no floor, as a
///        capacity does.
class CappedAtNinety
    : public Clonable<CappedAtNinety,
                      TranslationThresholdForm<RealResource, ExtensionFunction<RealResource>>,
                      ExtensionFunction<RealResource>> {
    protected:
        [[nodiscard]] std::optional<double> lower_bound_at(size_t /*node_id*/) const final {
            return std::nullopt;
        }

        [[nodiscard]] std::optional<double> upper_bound_at(size_t /*node_id*/) const final {
            return kCapacity;
        }
};

}  // namespace back_start_test

// ============================================================================
// Thresholds
// ============================================================================

/// @brief A time-window sink starts at its closing time, not at zero.
TEST(BackStart, TimeWindowStartsAtClosingTime) {
    TimeWindowExtensionFunction<RealResource> windows(
        std::map<size_t, std::pair<double, double>>{{2, {0.0, back_start_test::kSinkAClose}}});

    EXPECT_NEAR(back_start_test::start_at(windows, 2).get_value(),
                back_start_test::kSinkAClose,
                back_start_test::kTolerance);
}

/// @brief Two sinks with different closing times start differently, so the start must come from
///        each arc's preprocessed clone rather than the prototype.
TEST(BackStart, TwoSinksStartDifferently) {
    TimeWindowExtensionFunction<RealResource> windows(std::map<size_t, std::pair<double, double>>{
        {2, {0.0, back_start_test::kSinkAClose}},
        {3, {0.0, back_start_test::kSinkBClose}},
    });

    EXPECT_NEAR(back_start_test::start_at(windows, 2).get_value(),
                back_start_test::kSinkAClose,
                back_start_test::kTolerance);
    EXPECT_NEAR(back_start_test::start_at(windows, 3).get_value(),
                back_start_test::kSinkBClose,
                back_start_test::kTolerance);
}

/// @brief A cap starts at the cap and an addition at the empty suffix: the start follows the
///        extension, whatever the feasibility function bounds.
TEST(BackStart, TheStartFollowsTheExtension) {
    back_start_test::CappedAtNinety capacity;
    EXPECT_NEAR(back_start_test::start_at(capacity, 2).get_value(),
                back_start_test::kCapacity,
                back_start_test::kTolerance);

    AdditionExtensionFunction<RealResource> addition;
    EXPECT_NEAR(back_start_test::start_at(addition, 2).get_value(),
                0.0,
                back_start_test::kTolerance);
}

// ============================================================================
// Leaving the value alone
// ============================================================================

/// @brief An accumulation leaves the value alone rather than resetting it.
TEST(BackStart, AnAccumulationLeavesTheValueAlone) {
    AdditionExtensionFunction<RealResource> addition;

    // Non-zero, so an accidental start value would be visible.
    EXPECT_NEAR(
        back_start_test::start_at(addition, 2, RealResource(back_start_test::kNonZero)).get_value(),
        back_start_test::kNonZero,
        back_start_test::kTolerance);
}

/// @brief A threshold that states no upper bound at the sink sets no start either.
TEST(BackStart, AThresholdWithoutAnUpperBoundLeavesTheValueAlone) {
    back_start_test::Unbounded unbounded;

    EXPECT_NEAR(back_start_test::start_at(unbounded, 2, RealResource(back_start_test::kNonZero))
                    .get_value(),
                back_start_test::kNonZero,
                back_start_test::kTolerance);
}

/// @brief A container resource starts empty: a backward half legitimately starts having seen
///        nothing.
TEST(BackStart, ContainerStartsEmpty) {
    UnionExtensionFunction<SetResource<int>> memory;

    EXPECT_TRUE(back_start_test::start_at(memory, 2).get_value().empty());
}

// ============================================================================
// Composition fan-out
// ============================================================================

/// @brief Each component starts through its own extender: the time window takes the closing time
///        while cost stays at zero and the set stays empty.
TEST(BackStart, CompositionStartsEachComponentIndependently) {
    const std::map<size_t, std::pair<double, double>> windows{
        {2, {0.0, back_start_test::kSinkAClose}}};

    ResourceGraph<RealResource, SetResource<int>> graph;
    graph.add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                     std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                     std::make_unique<ValueCostFunction<RealResource>>(),
                                     std::make_unique<ValueDominanceFunction<RealResource>>());
    graph.add_resource<RealResource>(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    graph.add_resource<SetResource<int>>(
        std::make_unique<UnionExtensionFunction<SetResource<int>>>(),
        std::make_unique<TrivialFeasibilityFunction<SetResource<int>>>(),
        std::make_unique<TrivialCostFunction<SetResource<int>>>(),
        std::make_unique<InclusionDominanceFunction<SetResource<int>>>());
    graph.add_node(0, /*source=*/true, /*sink=*/false);
    graph.add_node(2, /*source=*/false, /*sink=*/true);
    auto& arc = graph.add_arc<RealResource, RealResource, SetResource<int>>(
        std::make_tuple(std::make_tuple(1.0),
                        std::make_tuple(1.0),
                        std::make_tuple(std::set<int>{0})),
        0,
        2,
        1.0);

    using Composed = ResourceTypeComposition<RealResource, SetResource<int>>;
    Resource<Composed> seed(*graph.get_node(2)->resource);
    arc.extender->start_back(&seed);

    const auto& real_components = std::get<0>(seed.get_components());
    EXPECT_NEAR(real_components[0]->get_value().get_value(), 0.0, back_start_test::kTolerance);
    EXPECT_NEAR(real_components[1]->get_value().get_value(),
                back_start_test::kSinkAClose,
                back_start_test::kTolerance);

    const auto& set_components = std::get<1>(seed.get_components());
    EXPECT_TRUE(set_components[0]->get_value().empty());
}
