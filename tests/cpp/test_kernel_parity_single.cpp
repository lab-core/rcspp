// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Parity of the single-direction labelling classes with their frozen copies: forward Simple, A*,
// Pushing and Pulling, and backward Simple, with label lists and with buckets. Its own
// translation unit: the copies double the labelling templates' instantiations.

#include <gtest/gtest.h>

#include <string>

#include "reference/astar_dominance_algorithm.hpp"
#include "reference/pulling_dominance_algorithm.hpp"
#include "reference/pushing_dominance_algorithm.hpp"
#include "reference/simple_dominance_algorithm.hpp"
#include "util/parity.hpp"

namespace {

using parity::Buckets;
using parity::Composed;
using parity::List;
using rcspp::RealResource;
using rcspp::SearchDirection;
namespace ref = rcspp::reference;

/// @brief The frozen Simple, except that its dominance test keeps every label.
template <typename ResourceType, typename LabelContainerType>
class KeepsEveryLabel : public ref::SimpleDominanceAlgorithm<ResourceType, LabelContainerType> {
    public:
        using ref::SimpleDominanceAlgorithm<ResourceType,
                                            LabelContainerType>::SimpleDominanceAlgorithm;

    protected:
        bool update_non_dominated_labels(const rcspp::Label<ResourceType>& /*label*/) override {
            return true;
        }
};

/// @brief The setting named @p name, in @p direction, on label lists.
parity::Setting<List> setting(const std::string& name,
                              SearchDirection direction = SearchDirection::Forward) {
    for (const auto& s : parity::settings<List>(direction, List{})) {
        if (s.name == name) {
            return s;
        }
    }
    ADD_FAILURE() << "no setting " << name;
    return {name, rcspp::AlgorithmParams<List>()};
}

/// @brief The golden 11-node instance of seed 1, the second case.
parity::Case golden_s1_n11() {
    const auto c = parity::cases().at(1);
    EXPECT_EQ(c.name, "golden s1 n11 t1");
    return c;
}

}  // namespace

/// @brief The library's Simple against itself: the harness sees no difference, in either release
///        mode.
TEST(Parity, HarnessSeesNoDifference) {
    using SimpleType = parity::Traced<rcspp::SimpleDominanceAlgorithm<Composed, List>>;
    const auto c = golden_s1_n11();
    for (const bool release : {false, true}) {
        const auto first = parity::observe<SimpleType>(c, setting("exact"), release);
        ASSERT_FALSE(first.front().trace.empty());
        EXPECT_TRUE(parity::same(first, parity::observe<SimpleType>(c, setting("exact"), release)));
    }
}

/// @brief A copy of Simple whose dominance test keeps every label, against the library's Simple:
///        the harness sees the difference, first in the extension trace.
///
/// Thirty iterations: without dominance the label count grows fast.
TEST(Parity, HarnessSeesADifference) {
    const auto c = golden_s1_n11();
    const auto changed =
        parity::observe<parity::Traced<KeepsEveryLabel<Composed, List>>>(c,
                                                                         setting("early"),
                                                                         false);
    const auto library =
        parity::observe<parity::Traced<rcspp::SimpleDominanceAlgorithm<Composed, List>>>(
            c,
            setting("early"),
            false);
    const auto result = parity::same(changed, library);
    ASSERT_FALSE(result);
    EXPECT_NE(std::string(result.message()).find("traces"), std::string::npos) << result.message();
}

/// @brief Pressure forced on the first memory check reaches the result through a real solve, and
///        the trim it makes leaves fewer labels extended.
TEST(Parity, ForcedPressureTrims) {
    using SimpleType = parity::Traced<rcspp::SimpleDominanceAlgorithm<Composed, List>>;
    const auto c = golden_s1_n11();
    const auto exact = parity::observe<SimpleType>(c, setting("exact"), true);
    auto pressed = setting("exact");
    pressed.params.memory_check_interval = 1;
    pressed.params.memory_pressure_max_labels_per_node = 1;
    pressed.forced_pressure_checks = {1};
    const auto runs = parity::observe<SimpleType>(c, pressed, true);
    EXPECT_FALSE(exact.front().memory_pressure);
    EXPECT_TRUE(runs.front().memory_pressure);
    EXPECT_LT(runs.front().extended, exact.front().extended);
}

TEST(Parity, SimpleForward) {
    parity::sweep<parity::Traced<ref::SimpleDominanceAlgorithm<Composed, List>>,
                  parity::Traced<rcspp::SimpleDominanceAlgorithm<Composed, List>>>(
        SearchDirection::Forward,
        List{});
    parity::sweep<parity::Traced<ref::SimpleDominanceAlgorithm<Composed, Buckets>>,
                  parity::Traced<rcspp::SimpleDominanceAlgorithm<Composed, Buckets>>>(
        SearchDirection::Forward,
        Buckets(10, 1, 0),
        /*buckets=*/true);
}

TEST(Parity, SimpleBackward) {
    parity::sweep<parity::Traced<ref::detail::BackwardSimpleDominanceAlgorithm<Composed, List>>,
                  parity::Traced<rcspp::detail::BackwardSimpleDominanceAlgorithm<Composed, List>>>(
        SearchDirection::Backward,
        List{});
    // The backward search turns the buckets backward.
    parity::sweep<
        parity::Traced<ref::detail::BackwardSimpleDominanceAlgorithm<Composed, Buckets>>,
        parity::Traced<rcspp::detail::BackwardSimpleDominanceAlgorithm<Composed, Buckets>>>(
        SearchDirection::Backward,
        Buckets(10, 1, 0),
        /*buckets=*/true);
}

TEST(Parity, AStarForward) {
    parity::sweep<parity::Traced<ref::AStarDominanceAlgorithm<Composed, List>>,
                  parity::Traced<rcspp::AStarDominanceAlgorithm<Composed, List>>>(
        SearchDirection::Forward,
        List{});
    parity::sweep<parity::Traced<ref::AStarDominanceAlgorithm<Composed, Buckets>>,
                  parity::Traced<rcspp::AStarDominanceAlgorithm<Composed, Buckets>>>(
        SearchDirection::Forward,
        Buckets(10, 1, 0),
        /*buckets=*/true);
}

TEST(Parity, PushingForward) {
    parity::sweep<parity::Traced<ref::PushingDominanceAlgorithm<Composed, List>>,
                  parity::Traced<rcspp::PushingDominanceAlgorithm<Composed, List>>>(
        SearchDirection::Forward,
        List{});
    parity::sweep<parity::Traced<ref::PushingDominanceAlgorithm<Composed, Buckets>>,
                  parity::Traced<rcspp::PushingDominanceAlgorithm<Composed, Buckets>>>(
        SearchDirection::Forward,
        Buckets(10, 1, 0),
        /*buckets=*/true);
}

TEST(Parity, PullingForward) {
    parity::sweep<parity::Traced<ref::PullingDominanceAlgorithm<Composed, List>>,
                  parity::Traced<rcspp::PullingDominanceAlgorithm<Composed, List>>>(
        SearchDirection::Forward,
        List{});
    parity::sweep<parity::Traced<ref::PullingDominanceAlgorithm<Composed, Buckets>>,
                  parity::Traced<rcspp::PullingDominanceAlgorithm<Composed, Buckets>>>(
        SearchDirection::Forward,
        Buckets(10, 1, 0),
        /*buckets=*/true);
}
