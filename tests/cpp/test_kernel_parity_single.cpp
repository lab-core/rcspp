// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Parity of the single-direction labelling classes with their frozen copies: forward Simple, A*,
// Pushing and Pulling, and backward Simple, with label lists and with buckets. Its own
// translation unit: the copies double the labelling templates' instantiations.

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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

/// @brief A cost feasibility that rules one node out as unreachable, as an ng memory can.
class BlocksNode : public rcspp::Clonable<BlocksNode, rcspp::FeasibilityFunction<RealResource>> {
    public:
        explicit BlocksNode(size_t blocked) : blocked_(blocked) {}

        auto is_feasible(const RealResource& /*resource*/) -> bool override { return true; }

        auto is_reachable(const rcspp::Resource<RealResource>& /*resource*/,
                          size_t destination_node_id) -> bool override {
            return destination_node_id != blocked_;
        }

    private:
        size_t blocked_;
};

/// @brief Source 0, sink 3, arcs 0 -> 1, 0 -> 2, 1 -> 3, 2 -> 3 (ids 0 to 3), each costing 1, and
///        node 2 unreachable: the composition's look-ahead asks the cost's feasibility.
std::unique_ptr<parity::GraphType> blocked_diamond() {
    auto graph = std::make_unique<parity::GraphType>(
        std::make_unique<rcspp::CompositionExtensionFunction<RealResource>>(),
        std::make_unique<
            rcspp::ReachableCompositionFeasibilityFunction<RealResource, RealResource>>(0),
        std::make_unique<rcspp::ComponentCostFunction<0, RealResource>>(0),
        std::make_unique<rcspp::CompositionDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<rcspp::AdditionExtensionFunction<RealResource>>(),
        std::make_unique<BlocksNode>(2),
        std::make_unique<rcspp::ValueCostFunction<RealResource>>(),
        std::make_unique<rcspp::ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2);
    graph->add_node(3, /*source=*/false, /*sink=*/true);
    for (const auto& [origin, destination] :
         std::vector<std::pair<size_t, size_t>>{{0, 1}, {0, 2}, {1, 3}, {2, 3}}) {
        graph->add_arc<RealResource>({1.0}, origin, destination, 1.0);
    }
    return graph;
}

/// @brief The golden 11-node instance of seed 1, the second case.
parity::Case golden_s1_n11() {
    const auto c = parity::cases().at(1);
    EXPECT_EQ(c.name, "golden s1 n11 t1");
    return c;
}

}  // namespace

/// @brief The library's Simple (traced) against itself: the harness sees no difference, in either
///        release mode.
TEST(Parity, HarnessSeesNoDifference) {
    using SimpleType = parity::TracedLabellingAlgorithm<rcspp::parts::FifoSchedule, List>;
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
        parity::observe<parity::TracedLabellingAlgorithm<rcspp::parts::FifoSchedule, List>>(
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
    using SimpleType = parity::TracedLabellingAlgorithm<rcspp::parts::FifoSchedule, List>;
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

/// @brief The frozen Simple against the kernel's single-direction search on a first-in,
///        first-out schedule, which Simple is built from.
TEST(Parity, SimpleForward) {
    parity::sweep<parity::Traced<ref::SimpleDominanceAlgorithm<Composed, List>>,
                  parity::TracedLabellingAlgorithm<rcspp::parts::FifoSchedule, List>>(
        SearchDirection::Forward,
        List{});
    parity::sweep<parity::Traced<ref::SimpleDominanceAlgorithm<Composed, Buckets>>,
                  parity::TracedLabellingAlgorithm<rcspp::parts::FifoSchedule, Buckets>>(
        SearchDirection::Forward,
        Buckets(10, 1, 0),
        /*buckets=*/true);
}

/// @brief The same, backward.
TEST(Parity, SimpleBackward) {
    parity::sweep<parity::Traced<ref::detail::BackwardSimpleDominanceAlgorithm<Composed, List>>,
                  parity::TracedLabellingAlgorithm<rcspp::parts::FifoSchedule,
                                                   List,
                                                   rcspp::BackwardDirection>>(
        SearchDirection::Backward,
        List{});
    // The backward search turns the buckets backward.
    parity::sweep<parity::Traced<ref::detail::BackwardSimpleDominanceAlgorithm<Composed, Buckets>>,
                  parity::TracedLabellingAlgorithm<rcspp::parts::FifoSchedule,
                                                   Buckets,
                                                   rcspp::BackwardDirection>>(
        SearchDirection::Backward,
        Buckets(10, 1, 0),
        /*buckets=*/true);
}

/// @brief The two traces hold the same entries where an arc's head is unreachable: a frozen
///        copy's hook is entered before the look-ahead, and the kernel tells its observer of the
///        unreachable outcome.
TEST(Parity, TracesAgreeOnUnreachableArcs) {
    const parity::Case c{"diamond with node 2 unreachable", blocked_diamond, 1, 0.0};
    for (const bool release : {false, true}) {
        const auto expected =
            parity::observe<parity::Traced<ref::SimpleDominanceAlgorithm<Composed, List>>>(
                c,
                setting("exact"),
                release);
        const auto actual =
            parity::observe<parity::TracedLabellingAlgorithm<rcspp::parts::FifoSchedule, List>>(
                c,
                setting("exact"),
                release);
        EXPECT_TRUE(parity::same(expected, actual)) << (release ? "released" : "kept");
        // Arc 1 (0 -> 2) was tried and traced, but drew no label: two extensions, three entries.
        const auto& trace = actual.front().trace;
        EXPECT_EQ(trace.size(), 3U);
        EXPECT_EQ(std::ranges::count(trace, size_t{1}, &parity::TraceEntry::arc_id), 1);
        EXPECT_EQ(actual.front().extended, 2U);
    }
}

/// @brief The frozen A* against the kernel's best-first search, which A* is built from.
TEST(Parity, AStarForward) {
    parity::sweep<parity::Traced<ref::AStarDominanceAlgorithm<Composed, List>>,
                  parity::TracedLabellingAlgorithm<parity::CompletionOrder, List>>(
        SearchDirection::Forward,
        List{});
    parity::sweep<parity::Traced<ref::AStarDominanceAlgorithm<Composed, Buckets>>,
                  parity::TracedLabellingAlgorithm<parity::CompletionOrder, Buckets>>(
        SearchDirection::Forward,
        Buckets(10, 1, 0),
        /*buckets=*/true);
}

/// @brief The frozen Pushing against the kernel's search on the forward sweep, which Pushing is
///        built from.
TEST(Parity, PushingForward) {
    parity::sweep<parity::Traced<ref::PushingDominanceAlgorithm<Composed, List>>,
                  parity::TracedLabellingAlgorithm<parity::ForwardSweep, List>>(
        SearchDirection::Forward,
        List{});
    parity::sweep<parity::Traced<ref::PushingDominanceAlgorithm<Composed, Buckets>>,
                  parity::TracedLabellingAlgorithm<parity::ForwardSweep, Buckets>>(
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
