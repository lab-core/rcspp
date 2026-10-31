// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The half-way point, and the checks that the critical resource is a valid clock.
//
// A non-monotone clock can cross H more than once, so both searches may discard a valid path.
// A failed check therefore turns the point off (correct but slower) instead of throwing.

#include <gtest/gtest.h>

#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace half_way_test {

using Composed = ResourceTypeComposition<RealResource>;
constexpr double kUpperBound = 100.0;

/// @brief A small probe set, enough to catch a sign error.
inline std::vector<double> probes(double upper_bound = kUpperBound) {
    return {0.0, upper_bound / 2.0};
}

/// @brief A two-node graph whose single component uses @p extension.
template <typename ExtensionFunctionType, typename FeasibilityFunctionType>
std::unique_ptr<ResourceGraph<RealResource>> two_node_graph(
    std::unique_ptr<ExtensionFunctionType> extension, std::unique_ptr<FeasibilityFunctionType> feas,
    double arc_value) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::move(extension),
                                      std::move(feas),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource>(std::make_tuple(arc_value), 0, 1);
    return graph;
}

/// @brief A two-node graph whose one resource is a time window on [0, 1000] at both nodes: a
///        valid clock.
inline std::unique_ptr<ResourceGraph<RealResource>> clocked_graph() {
    const std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 1000.0}},
                                                              {1, {0.0, 1000.0}}};
    return two_node_graph(std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
                          std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
                          /*arc_value=*/15.0);
}

/// @brief Labels whose clock (the one resource) is a given value.
class Labels {
    public:
        Labels()
            : graph_(two_node_graph(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                    std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                    /*arc_value=*/1.0)),
              pool_(std::make_unique<LabelFactory<Composed>>(&graph_->get_resource_factory())) {}

        /// @brief A label whose clock is @p value.
        const Label<Composed>& at(double value) {
            auto& made = pool_.get_next_label(graph_->get_node(0));
            made.get_resource().get_component<RealResource>(0).set_value(value);
            return made;
        }

    private:
        std::unique_ptr<ResourceGraph<RealResource>> graph_;
        LabelPool<Composed> pool_;
};

/// @brief The params of a half-way point at @p h on component 0.
inline AlgorithmBaseParams params_at(double h) {
    AlgorithmBaseParams params;
    params.half_way_point = h;
    params.critical_resource_index = 0;
    return params;
}

}  // namespace half_way_test

// ============================================================================
// The half-way point
// ============================================================================

/// @brief The params turn the point on at half_way_point when it is positive, and leave it off
///        otherwise.
TEST(HalfWay, IsOnAtTheParamsPointWhenOneIsGiven) {
    namespace hwt = half_way_test;
    const auto on = detail::make_half_way<RealResource, hwt::Composed>(hwt::params_at(30.0));
    EXPECT_TRUE(on.on());
    EXPECT_EQ(on.h(), 30.0);
    EXPECT_FALSE((detail::make_half_way<RealResource, hwt::Composed>(hwt::params_at(0.0)).on()));
    EXPECT_FALSE(detail::HalfWay<hwt::Composed>{}.on()) << "off by default";
}

/// @brief Forward is past above H, backward below it, and neither exactly at H.
///
/// A label exactly on H must survive in both directions, or a path crossing there is lost.
TEST(HalfWay, ForwardIsPastAboveHBackwardBelowIt) {
    namespace hwt = half_way_test;
    hwt::Labels labels;
    const auto half_way = detail::make_half_way<RealResource, hwt::Composed>(hwt::params_at(50.0));

    EXPECT_FALSE(half_way.past<ForwardDirection>(labels.at(49.0)));
    EXPECT_TRUE(half_way.past<ForwardDirection>(labels.at(51.0)));
    EXPECT_FALSE(half_way.past<BackwardDirection>(labels.at(51.0)));
    EXPECT_TRUE(half_way.past<BackwardDirection>(labels.at(49.0)));

    // Exactly at H: neither direction stops.
    EXPECT_FALSE(half_way.past<ForwardDirection>(labels.at(50.0)));
    EXPECT_FALSE(half_way.past<BackwardDirection>(labels.at(50.0)));
}

/// @brief Turned off, the point holds nothing back, in either direction, for any value.
TEST(HalfWay, TurnedOffHoldsNothingBack) {
    namespace hwt = half_way_test;
    hwt::Labels labels;
    auto half_way = detail::make_half_way<RealResource, hwt::Composed>(hwt::params_at(50.0));
    ASSERT_TRUE(half_way.on());
    half_way.turn_off();
    EXPECT_FALSE(half_way.on());
    for (const double value : {-1000.0, 0.0, 49.0, 50.0, 51.0, 1000.0}) {
        EXPECT_FALSE(half_way.past<ForwardDirection>(labels.at(value)));
        EXPECT_FALSE(half_way.past<BackwardDirection>(labels.at(value)));
    }
}

// ============================================================================
// Monotonicity validation
// ============================================================================

/// @brief A plain additive time resource is monotone.
TEST(HalfWay, MonotoneResourceAccepted) {
    auto graph =
        half_way_test::two_node_graph(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      /*arc_value=*/15.0);

    const auto probes = half_way_test::probes();
    EXPECT_TRUE(detail::critical_resource_is_monotone<RealResource>(*graph, 0, probes));
}

/// @brief A negative consumption on the critical slot is not monotone.
///
/// E.g. a reduced-cost slot during pricing, which must not be used as the clock.
TEST(HalfWay, NegativeConsumptionRejected) {
    auto graph =
        half_way_test::two_node_graph(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      /*arc_value=*/-5.0);

    const auto probes = half_way_test::probes();
    EXPECT_FALSE(detail::critical_resource_is_monotone<RealResource>(*graph, 0, probes));
}

/// @brief A monotone-but-clamped extension is ACCEPTED, though its stored arc value is negative.
///
/// The extension clamps up to the opening time of 60, so the result still rises from every probe.
/// This is why monotonicity is probed rather than read from the stored arc value.
TEST(HalfWay, ClampedExtensionIsAccepted) {
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 1000.0}}, {1, {60.0, 1000.0}}};

    auto graph = half_way_test::two_node_graph(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        /*arc_value=*/-5.0);

    const auto probes = half_way_test::probes();
    EXPECT_TRUE(detail::critical_resource_is_monotone<RealResource>(*graph, 0, probes));
}

/// @brief Once one arc fails, the remaining arcs are skipped rather than probed.
TEST(HalfWay, StopsProbingAfterTheFirstFailure) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource>(std::make_tuple(-5.0), 0, 1);  // fails first
    graph->add_arc<RealResource>(std::make_tuple(5.0), 1, 2);   // never probed

    const auto probes = half_way_test::probes();
    EXPECT_FALSE(detail::critical_resource_is_monotone<RealResource>(*graph, 0, probes));
}

/// @brief A resource type absent from the pack turns the point off rather than failing.
TEST(HalfWay, AbsentCriticalTypeIsNotMonotone) {
    auto graph =
        half_way_test::two_node_graph(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      /*arc_value=*/15.0);

    const auto probes = half_way_test::probes();
    // IntResource is not part of this graph's pack.
    EXPECT_FALSE((detail::critical_resource_is_monotone<IntResource>(*graph, 0, probes)));
}

// ============================================================================
// Dominance-order validation
// ============================================================================

/// @brief A clock compared by value is accepted.
///
/// A dominator's clock is then never above the evicted label's, so it stays a join candidate.
TEST(HalfWay, ValueDominanceOnTheClockIsAccepted) {
    auto graph =
        half_way_test::two_node_graph(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      /*arc_value=*/15.0);

    EXPECT_TRUE((detail::critical_resource_dominance_is_increasing<RealResource, RealResource>(
        *graph,
        /*critical_resource_index=*/0)));
}

/// @brief A clock with a trivial dominance is rejected.
///
/// A dominator above H could evict a label below H, leaving neither available to the join.
TEST(HalfWay, TrivialDominanceOnTheClockIsRejected) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<TrivialDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource>(std::make_tuple(15.0), 0, 1);

    EXPECT_FALSE((detail::critical_resource_dominance_is_increasing<RealResource, RealResource>(
        *graph,
        /*critical_resource_index=*/0)));
}

/// @brief An absent critical type has no dominance order, like the monotonicity probe.
TEST(HalfWay, AbsentCriticalTypeHasNoDominanceOrder) {
    auto graph =
        half_way_test::two_node_graph(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      /*arc_value=*/15.0);

    // IntResource is not part of this graph's pack.
    EXPECT_FALSE((detail::critical_resource_dominance_is_increasing<IntResource, RealResource>(
        *graph,
        /*critical_resource_index=*/0)));
}

// ============================================================================
// Why the point is off
// ============================================================================

/// @brief A valid clock with a half-way point gives no reason; no point, or a failed check, gives
///        one, as a sentence, and nothing throws.
TEST(HalfWay, AFailedCheckGivesAReasonRatherThanThrowing) {
    namespace hwt = half_way_test;
    const auto clocked = hwt::clocked_graph();
    EXPECT_EQ((detail::half_way_off_reason<RealResource>(*clocked, hwt::params_at(50.0))),
              std::nullopt);

    const auto none = detail::half_way_off_reason<RealResource>(*clocked, hwt::params_at(0.0));
    ASSERT_TRUE(none.has_value());
    EXPECT_NE(none->find("no half_way_point"), std::string::npos) << *none;

    const auto out_of_range = [&] {
        auto params = hwt::params_at(50.0);
        params.critical_resource_index = 3;
        return detail::half_way_off_reason<RealResource>(*clocked, params);
    }();
    ASSERT_TRUE(out_of_range.has_value());
    EXPECT_NE(out_of_range->find("does not exist"), std::string::npos) << *out_of_range;

    const auto absent = detail::half_way_off_reason<IntResource>(*clocked, hwt::params_at(50.0));
    ASSERT_TRUE(absent.has_value());
    EXPECT_NE(absent->find("not in the model"), std::string::npos) << *absent;

    auto additive =
        hwt::two_node_graph(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                            std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                            /*arc_value=*/-5.0);
    std::optional<std::string> reason;
    EXPECT_NO_THROW(reason =
                        detail::half_way_off_reason<RealResource>(*additive, hwt::params_at(50.0)));
    EXPECT_TRUE(reason.has_value());
}
