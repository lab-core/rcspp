// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The backward seed: a backward label at a sink starts at that sink's upper bound, read from the
// node's preprocessed feasibility function. Tests check the seed value directly, since a wrong
// seed in a solve just shows up as "no solutions".

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

/// @brief Two sinks with different closing times seed differently, so the seed must come from
///        each node's preprocessed clone rather than the prototype.
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

/// @brief The seed follows the paired extension, not the deprecated flag: a threshold starts at
///        the maximum whatever the flag says, and an accumulation at the empty suffix.
TEST(BackSeed, MinMaxSeedFollowsThePairedExtension) {
    auto threshold = std::make_unique<MinMaxFeasibilityFunction<RealResource>>(
        10.0,
        90.0,
        /*merge_by_increasing_value=*/false);
    threshold->set_backward_kind(BackwardKind::Threshold);
    auto deadline = back_seed_test::make_resource<RealResource>(
        std::move(threshold),
        std::make_unique<ValueDominanceFunction<RealResource>>(),
        0);
    deadline->apply_back_seed();
    EXPECT_NEAR(deadline->get_value().get_value(), 90.0, back_seed_test::kTolerance);

    auto accumulation = std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 90.0);
    accumulation->set_backward_kind(BackwardKind::Accumulate);
    auto suffix = back_seed_test::make_resource<RealResource>(
        std::move(accumulation),
        std::make_unique<ValueDominanceFunction<RealResource>>(),
        0);
    EXPECT_FALSE(suffix->has_back_seed());
    suffix->apply_back_seed();
    EXPECT_NEAR(suffix->get_value().get_value(), 0.0, back_seed_test::kTolerance);
}

// ============================================================================
// The nullopt branch
// ============================================================================

/// @brief Cost has no upper bound: TrivialFeasibilityFunction returns nullopt and apply_back_seed
///        leaves the value alone.
TEST(BackSeed, CostSeedsAtZero) {
    auto resource = back_seed_test::make_resource<RealResource>(
        std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>(),
        0);

    // Non-zero, so an accidental seed would be visible.
    resource->set_value(RealResource(42.0));
    resource->apply_back_seed();

    // Untouched: nullopt means "leave the value alone", not "reset it".
    EXPECT_NEAR(resource->get_value().get_value(), 42.0, back_seed_test::kTolerance);

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
