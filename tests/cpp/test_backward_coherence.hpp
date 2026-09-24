// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Backward coherence at compile time: the trait, and the forward-only models it must not block.
//
// Every preset asserts `backward_coherent_v<Ext, Feas>`; `add_resource` does not, since a model
// never solved bidirectionally needs no coherence. This file pins the traits' answers with
// static_asserts, and checks that forward-only models the trait flags still register, solve
// forward, and are refused by a bidirectional solve at setup.

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

// Where each feasibility function's backward seed sits. A specialisation with the wrong arity
// compiles silently and answers Unknown; these lines catch that.
static_assert(back_seed_end_v<TrivialFeasibilityFunction<RealResource>> == BackSeedEnd::Never);
static_assert(back_seed_end_v<TimeWindowFeasibilityFunction<RealResource>> == BackSeedEnd::Ceiling);
static_assert(back_seed_end_v<IntersectionFeasibilityFunction<SetResource<int>>> ==
              BackSeedEnd::Never);
static_assert(back_seed_end_v<SizeFeasibilityFunction<SetResource<int>>> == BackSeedEnd::Never);

// Deliberately Unknown: MinMaxFeasibilityFunction picks its seed end at construction, so marking
// it Ceiling would falsely flag the `merge_by_increasing_value = false` pairing.
static_assert(back_seed_end_v<MinMaxFeasibilityFunction<RealResource>> == BackSeedEnd::Unknown);

// The pairings the presets build are coherent.
static_assert(backward_coherent_v<AdditionExtensionFunction<RealResource>,
                                  TrivialFeasibilityFunction<RealResource>>);
static_assert(backward_coherent_v<TimeWindowExtensionFunction<RealResource>,
                                  TimeWindowFeasibilityFunction<RealResource>>);
static_assert(backward_coherent_v<BudgetExtensionFunction<IntResource>,
                                  MinMaxFeasibilityFunction<IntResource>>);
static_assert(backward_coherent_v<NgPathExtensionFunction<SizeTBitsetResource, size_t>,
                                  IntersectionFeasibilityFunction<SizeTBitsetResource, size_t>>);

// A capacity written as an addition passes, since MinMax's seed end is Unknown; the solver refuses
// it at setup instead (see AccumulateWithBackSeedIsRefused).
static_assert(backward_coherent_v<AdditionExtensionFunction<RealResource>,
                                  MinMaxFeasibilityFunction<RealResource>>);

namespace backward_coherence_test {

/// @brief A custom extension function that declares no backward kind, as a forward-only author
///        would write it.
class DoublingExtension : public Clonable<DoublingExtension, ExtensionFunction<RealResource>> {
    public:
        void extend(const RealResource& resource, const RealResource& extender_value,
                    RealResource* extended_resource) override {
            extended_resource->set_value(resource.get_value() + 2.0 * extender_value.get_value());
        }
};

/// @brief Node 1 closes at 3; nodes 0 and 2 are open until 100.
template <typename Clock>
inline std::map<size_t, std::pair<Clock, Clock>> windows() {
    return {{0, {Clock{0}, Clock{100}}}, {1, {Clock{0}, Clock{3}}}, {2, {Clock{0}, Clock{100}}}};
}

/// @brief Two routes: 0 -> 1 -> 2 is cheaper (-10) but reaches node 1 after it closes; 0 -> 2
///        costs -1 and is on time. Components are (cost, clock).
template <typename ClockResource, typename Graph>
inline void add_routes(Graph* graph) {
    using Clock = std::decay_t<decltype(std::declval<ClockResource>().get_value())>;
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2, /*source=*/false, /*sink=*/true);
    graph->template add_arc<RealResource, ClockResource>({-5.0, Clock{5}}, 0, 1, -5.0);
    graph->template add_arc<RealResource, ClockResource>({-5.0, Clock{1}}, 1, 2, -5.0);
    graph->template add_arc<RealResource, ClockResource>({-1.0, Clock{1}}, 0, 2, -1.0);
}

/// @brief Asserts that a forward solve of @ref add_routes kept the window.
inline void expect_on_time_route(const SolveResult& result) {
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_DOUBLE_EQ(result.solutions.front().cost, -1.0);
    EXPECT_EQ(result.solutions.front().path_arc_ids, (std::vector<size_t>{2}));
}

/// @brief Asserts that a bidirectional solve refuses @p graph at setup, and says why.
///
/// @param graph     The model.
/// @param component The component the refusal must name, e.g. "component 1".
/// @param reason    A phrase the refusal must contain.
template <typename... ResourceTypes>
inline void expect_refused_bidirectionally(ResourceGraph<ResourceTypes...>* graph,
                                           const std::string& component,
                                           const std::string& reason) {
    AlgorithmParams<LabelList<ResourceTypeComposition<ResourceTypes...>>> params;
    auto algorithm =
        graph->template create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    try {
        graph->solve(algorithm.get());
        FAIL() << "a bidirectional solve must refuse this model at setup";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find(component), std::string::npos) << message;
        EXPECT_NE(message.find(reason), std::string::npos) << message;
    }
}

}  // namespace backward_coherence_test

// A user-written feasibility function is Unknown, so the trait never falsely flags it.
TEST(BackwardCoherence, AnUnmigratedFeasibilityFunctionIsUnknown) {
    class UserFeasibility : public Clonable<UserFeasibility, FeasibilityFunction<RealResource>> {
        public:
            [[nodiscard]] auto is_feasible(const RealResource& /*resource*/) -> bool override {
                return true;
            }

            [[nodiscard]] MergeRule merge_rule() const override { return MergeRule::AlwaysTrue; }
    };

    static_assert(back_seed_end_v<UserFeasibility> == BackSeedEnd::Unknown);
    static_assert(backward_coherent_v<AdditionExtensionFunction<RealResource>, UserFeasibility>);

    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<UserFeasibility>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    SUCCEED();
}

// A custom extension function that declares no kind is a valid forward-only model.
TEST(BackwardCoherence, AKindlessCustomExtensionIsAValidForwardOnlyModel) {
    namespace bc = backward_coherence_test;
    static_assert(
        !backward_coherent_v<bc::DoublingExtension, TrivialFeasibilityFunction<RealResource>>);

    const auto build = [] {
        auto graph = std::make_unique<ResourceGraph<RealResource>>();
        graph->add_resource<RealResource>(
            std::make_unique<bc::DoublingExtension>(),
            std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
            std::make_unique<ValueCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
        graph->add_node(0, /*source=*/true, /*sink=*/false);
        graph->add_node(1);
        graph->add_node(2, /*source=*/false, /*sink=*/true);
        graph->add_arc<RealResource>(std::make_tuple(1.0), 0, 1, 1.0);
        graph->add_arc<RealResource>(std::make_tuple(2.0), 1, 2, 2.0);
        return graph;
    };

    const auto result = build()->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    ASSERT_FALSE(result.solutions.empty());
    EXPECT_DOUBLE_EQ(result.solutions.front().cost, 6.0);  // 2 * (1 + 2): the custom rule ran
    EXPECT_EQ(result.solutions.front().path_arc_ids, (std::vector<size_t>{0, 1}));

    backward_coherence_test::expect_refused_bidirectionally(build().get(),
                                                            "component 0",
                                                            "declares no backward_kind()");
}

// Travel time accumulating against due dates, with no waiting: the trait flags it (an
// accumulation with a ceiling-seeded feasibility), but forward it is correct.
TEST(BackwardCoherence, AnAccumulationAgainstDueDatesIsAValidForwardOnlyModel) {
    namespace bc = backward_coherence_test;
    static_assert(!backward_coherent_v<AdditionExtensionFunction<RealResource>,
                                       TimeWindowFeasibilityFunction<RealResource>>);

    const auto build = [] {
        auto graph = std::make_unique<ResourceGraph<RealResource>>();
        presets::add_cost_resource<RealResource>(*graph);
        graph->add_resource<RealResource>(
            std::make_unique<AdditionExtensionFunction<RealResource>>(),
            std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(bc::windows<double>()),
            std::make_unique<TrivialCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
        bc::add_routes<RealResource>(graph.get());
        return graph;
    };

    bc::expect_on_time_route(build()->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}));
    bc::expect_refused_bidirectionally(build().get(), "component 1", "BudgetExtensionFunction");
}

// An unsigned time window declares no backward kind, but forward it is correct.
TEST(BackwardCoherence, AnUnsignedTimeWindowIsAValidForwardOnlyModel) {
    namespace bc = backward_coherence_test;
    static_assert(backward_kind_of_v<TimeWindowExtensionFunction<UIntResource>> ==
                  BackwardKind::Unspecified);
    static_assert(!backward_coherent_v<TimeWindowExtensionFunction<UIntResource>,
                                       TimeWindowFeasibilityFunction<UIntResource>>);

    const auto build = [] {
        auto graph = std::make_unique<ResourceGraph<RealResource, UIntResource>>();
        presets::add_cost_resource<RealResource>(*graph);
        graph->add_resource<UIntResource>(
            std::make_unique<TimeWindowExtensionFunction<UIntResource>>(
                bc::windows<unsigned int>()),
            std::make_unique<TimeWindowFeasibilityFunction<UIntResource>>(
                bc::windows<unsigned int>()),
            std::make_unique<TrivialCostFunction<UIntResource>>(),
            std::make_unique<ValueDominanceFunction<UIntResource>>());
        bc::add_routes<UIntResource>(graph.get());
        return graph;
    };

    bc::expect_on_time_route(build()->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}));
    bc::expect_refused_bidirectionally(build().get(), "component 1", "declares no backward_kind()");
}
