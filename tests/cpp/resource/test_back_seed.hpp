// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Phase 5: the backward seed.
//
// A forward label starts at "nothing consumed" -- time 0, load 0, empty set. A backward label at a
// sink starts at that sink's upper bound, and the number differs per sink, which is why the seed is
// read from the node's own preprocessed feasibility function rather than from a solve parameter.
//
// These tests assert on the seed VALUE directly rather than on a solve result on purpose: a wrong
// seed fails silently. The backward search finds nothing, the join has nothing to pair, and the
// result is "no solutions" -- the same signature as a path-reconstruction bug.

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace back_seed_test {

constexpr double kTolerance = 1e-12;
constexpr double kSinkAClose = 100.0;
constexpr double kSinkBClose = 140.0;

/// @brief Builds a scalar Resource carrying the given feasibility function.
template <typename ResourceType, typename FeasibilityFn, typename DominanceFn>
std::unique_ptr<Resource<ResourceType>> make_resource(std::unique_ptr<FeasibilityFn> feasibility,
                                                      std::unique_ptr<DominanceFn> dominance,
                                                      size_t node_id) {
    return std::make_unique<Resource<ResourceType>>(
        std::move(dominance),
        std::move(feasibility),
        std::make_unique<TrivialCostFunction<ResourceType>>(),
        node_id);
}

/// @brief A time-window resource already preprocessed for @p node_id, as a node's resource is.
inline std::unique_ptr<Resource<RealResource>> make_time_window_resource(
    const std::map<size_t, std::pair<double, double>>& windows, size_t node_id) {
    TimeWindowFeasibilityFunction<RealResource> prototype(windows);
    return std::make_unique<Resource<RealResource>>(
        std::make_unique<ValueDominanceFunction<RealResource>>(),
        prototype.create(node_id),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        node_id);
}

}  // namespace back_seed_test

// ============================================================================
// Time windows
// ============================================================================

/// @brief A time-window sink seeds at its closing time, not at zero.
TEST(BackSeed, TimeWindowSeedsAtClosingTime) {
    std::map<size_t, std::pair<double, double>> windows{{2, {0.0, back_seed_test::kSinkAClose}}};
    auto resource = back_seed_test::make_time_window_resource(windows, 2);

    // Before seeding it holds the type default.
    EXPECT_NEAR(resource->get_value().get_value(), 0.0, back_seed_test::kTolerance);

    resource->apply_back_seed();

    EXPECT_NEAR(resource->get_value().get_value(),
                back_seed_test::kSinkAClose,
                back_seed_test::kTolerance);
}

/// @brief Two sinks with different closing times seed differently.
///
/// This is the case a single global parameter could not express, and it is the whole reason the
/// seed is read per node rather than passed once per solve. Reading it from the *prototype* instead
/// of the node's preprocessed clone would give both sinks the same number.
TEST(BackSeed, TwoSinksSeedDifferently) {
    std::map<size_t, std::pair<double, double>> windows{
        {2, {0.0, back_seed_test::kSinkAClose}},
        {3, {0.0, back_seed_test::kSinkBClose}},
    };

    auto sink_a = back_seed_test::make_time_window_resource(windows, 2);
    auto sink_b = back_seed_test::make_time_window_resource(windows, 3);

    sink_a->apply_back_seed();
    sink_b->apply_back_seed();

    EXPECT_NEAR(sink_a->get_value().get_value(),
                back_seed_test::kSinkAClose,
                back_seed_test::kTolerance);
    EXPECT_NEAR(sink_b->get_value().get_value(),
                back_seed_test::kSinkBClose,
                back_seed_test::kTolerance);
    EXPECT_NE(sink_a->get_value().get_value(), sink_b->get_value().get_value());
}

// ============================================================================
// MinMax: the loosest end, both ways
// ============================================================================

/// @brief Smaller-is-better (a load) seeds at the maximum.
TEST(BackSeed, MinMaxSeedsAtMaximumWhenSmallerIsBetter) {
    auto resource = back_seed_test::make_resource<RealResource>(
        std::make_unique<MinMaxFeasibilityFunction<RealResource>>(
            10.0,
            90.0,
            /*merge_by_increasing_value=*/true),
        std::make_unique<ValueDominanceFunction<RealResource>>(),
        0);

    resource->apply_back_seed();

    EXPECT_NEAR(resource->get_value().get_value(), 90.0, back_seed_test::kTolerance);
}

/// @brief Larger-is-better (remaining fuel) seeds at the minimum.
TEST(BackSeed, MinMaxSeedsAtMinimumWhenLargerIsBetter) {
    auto resource = back_seed_test::make_resource<RealResource>(
        std::make_unique<MinMaxFeasibilityFunction<RealResource>>(
            10.0,
            90.0,
            /*merge_by_increasing_value=*/false),
        std::make_unique<ValueDominanceFunction<RealResource>>(),
        0);

    resource->apply_back_seed();

    EXPECT_NEAR(resource->get_value().get_value(), 10.0, back_seed_test::kTolerance);
}

// ============================================================================
// The nullopt branch -- asserted, not left as an untaken path
// ============================================================================

/// @brief Cost has no upper bound, so it seeds at zero.
///
/// TrivialFeasibilityFunction returns nullopt and apply_back_seed leaves the value alone. Asserted
/// explicitly rather than by omission: this is the branch the coverage gate needs, and "the value
/// happened to already be 0" is not the same claim as "the seed did not fire".
TEST(BackSeed, CostSeedsAtZero) {
    auto resource = back_seed_test::make_resource<RealResource>(
        std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>(),
        0);

    // Start from a non-zero value so an accidental seed would be visible either way.
    resource->set_value(RealResource(42.0));
    resource->apply_back_seed();

    // Untouched: nullopt means "leave the value alone", not "reset it".
    EXPECT_NEAR(resource->get_value().get_value(), 42.0, back_seed_test::kTolerance);

    // And the function really does decline to supply one.
    EXPECT_FALSE(TrivialFeasibilityFunction<RealResource>{}.back_seed_value().has_value());
}

/// @brief A container resource seeds empty: a backward half legitimately starts having seen
///        nothing.
TEST(BackSeed, ContainerSeedsEmpty) {
    auto resource = back_seed_test::make_resource<SetResource<int>>(
        std::make_unique<TrivialFeasibilityFunction<SetResource<int>>>(),
        std::make_unique<InclusionDominanceFunction<SetResource<int>>>(),
        0);

    resource->apply_back_seed();

    EXPECT_TRUE(resource->get_value().empty());
    EXPECT_FALSE(TrivialFeasibilityFunction<SetResource<int>>{}.back_seed_value().has_value());
}

// ============================================================================
// Composition fan-out
// ============================================================================

/// @brief Each component seeds independently: the time window takes the closing time while cost
///        stays at zero and the set stays empty.
TEST(BackSeed, CompositionSeedsEachComponentIndependently) {
    std::map<size_t, std::pair<double, double>> windows{{2, {0.0, back_seed_test::kSinkAClose}}};

    // Component vector for the RealResource slot: cost, then time window.
    std::tuple<std::vector<std::unique_ptr<Resource<RealResource>>>,
               std::vector<std::unique_ptr<Resource<SetResource<int>>>>>
        components;
    std::get<0>(components)
        .push_back(back_seed_test::make_resource<RealResource>(
            std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>(),
            2));
    std::get<0>(components).push_back(back_seed_test::make_time_window_resource(windows, 2));
    std::get<1>(components)
        .push_back(back_seed_test::make_resource<SetResource<int>>(
            std::make_unique<TrivialFeasibilityFunction<SetResource<int>>>(),
            std::make_unique<InclusionDominanceFunction<SetResource<int>>>(),
            2));

    using Composed = ResourceTypeComposition<RealResource, SetResource<int>>;
    Resource<Composed> composition(
        std::move(components),
        std::make_unique<CompositionDominanceFunction<RealResource, SetResource<int>>>(),
        std::make_unique<CompositionFeasibilityFunction<RealResource, SetResource<int>>>(),
        std::make_unique<CompositionCostFunction<RealResource, SetResource<int>>>(),
        2);

    composition.apply_back_seed();

    const auto& real_components = std::get<0>(composition.get_components());
    EXPECT_NEAR(real_components[0]->get_value().get_value(), 0.0, back_seed_test::kTolerance);
    EXPECT_NEAR(real_components[1]->get_value().get_value(),
                back_seed_test::kSinkAClose,
                back_seed_test::kTolerance);

    const auto& set_components = std::get<1>(composition.get_components());
    EXPECT_TRUE(set_components[0]->get_value().empty());
}
