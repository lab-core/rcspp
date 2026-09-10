// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The type-aware add_resource overload.
//
// Positive cases only, plus trait assertions. A static_assert firing is a *compile* failure, which
// a runtime suite cannot observe -- so the negative cases are not tested here. That is a
// deliberate choice rather than an oversight: the two assertions are one `!=` and one `&&` over
// traits this file pins directly, and each has a runtime counterpart that *is* tested
// (test_bidirectional.hpp's UndeclaredComponentIsRefused and AccumulateWithBackSeedIsRefused).
// A CMake try_compile fixture would be the alternative, and its own failure modes are harder to
// reason about than the thing it would check.

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <utility>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

// The traits, asserted directly. These are what the overload's static_asserts read, so pinning
// them here is what makes a wrong trait specialisation a test failure rather than a silent
// weakening of the overload.
//
// A specialisation with the wrong arity is the nastiest failure mode in this area: nothing fails
// to compile, the trait just keeps answering Unknown and the check stops firing. These lines are
// what catch that.
static_assert(back_seed_end_v<TrivialFeasibilityFunction<RealResource>> == BackSeedEnd::Never);
static_assert(back_seed_end_v<TimeWindowFeasibilityFunction<RealResource>> == BackSeedEnd::Ceiling);
static_assert(back_seed_end_v<IntersectionFeasibilityFunction<SetResource<int>>> ==
              BackSeedEnd::Never);
static_assert(back_seed_end_v<SizeFeasibilityFunction<SetResource<int>>> == BackSeedEnd::Never);

// Deliberately Unknown: MinMaxFeasibilityFunction picks its seed end from a constructor argument.
// Marking it Ceiling would falsely reject the legitimate `merge_by_increasing_value = false`
// pairing, so this assertion is what fails the build if someone "completes" the trait.
static_assert(back_seed_end_v<MinMaxFeasibilityFunction<RealResource>> == BackSeedEnd::Unknown);

namespace typed_add_resource_test {

inline std::map<size_t, std::pair<double, double>> windows() {
    return {{0, {0.0, 100.0}}, {1, {0.0, 100.0}}};
}

}  // namespace typed_add_resource_test

// A user-written feasibility function is Unknown, so it is never falsely rejected. This is what
// makes BackSeedEnd opt-in.
TEST(TypedAddResource, UnmigratedFeasibilityFunctionIsUnknown) {
    class UserFeasibility : public Clonable<UserFeasibility, FeasibilityFunction<RealResource>> {
        public:
            [[nodiscard]] auto is_feasible(const RealResource& /*resource*/) -> bool override {
                return true;
            }

            [[nodiscard]] MergeRule merge_rule() const override { return MergeRule::AlwaysTrue; }
    };

    static_assert(back_seed_end_v<UserFeasibility> == BackSeedEnd::Unknown);

    // And it can still be registered through the typed overload: Accumulate + Unknown is not
    // rejected statically.
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<UserFeasibility>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    SUCCEED();
}

// The claim the whole step rests on: the typed overload delegates, and adds only checks.
//
// Same model built twice -- once with inline make_unique (typed), once through a base-typed local
// (erased). The derived reversal flag and the solved solution set must agree.
TEST(TypedAddResource, TypedAndErasedProduceTheSameModel) {
    const auto build = [](bool erased) {
        auto graph = std::make_unique<ResourceGraph<RealResource>>();
        auto dominance = std::make_unique<ValueDominanceFunction<RealResource>>();
        auto* borrowed = dominance.get();

        if (erased) {
            std::unique_ptr<ExtensionFunction<RealResource>> extension =
                std::make_unique<TimeWindowExtensionFunction<RealResource>>(
                    typed_add_resource_test::windows());
            graph->add_resource<RealResource>(
                std::move(extension),
                std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(
                    typed_add_resource_test::windows()),
                std::make_unique<ValueCostFunction<RealResource>>(),
                std::move(dominance));
        } else {
            graph->add_resource<RealResource>(
                std::make_unique<TimeWindowExtensionFunction<RealResource>>(
                    typed_add_resource_test::windows()),
                std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(
                    typed_add_resource_test::windows()),
                std::make_unique<ValueCostFunction<RealResource>>(),
                std::move(dominance));
        }
        graph->add_node(0, /*source=*/true, /*sink=*/false);
        graph->add_node(1, /*source=*/false, /*sink=*/true);
        graph->add_arc<RealResource>(std::make_tuple(10.0), 0, 1, 10.0);
        return std::pair{std::move(graph), borrowed};
    };

    auto [typed_graph, typed_dominance] = build(/*erased=*/false);
    auto [erased_graph, erased_dominance] = build(/*erased=*/true);

    // A threshold extension derives reversed == true, through either overload.
    EXPECT_TRUE(typed_dominance->is_backward_reversed());
    EXPECT_EQ(typed_dominance->is_backward_reversed(), erased_dominance->is_backward_reversed());

    const auto typed_result = typed_graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    const auto erased_result = erased_graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    ASSERT_EQ(typed_result.solutions.size(), erased_result.solutions.size());
    if (!typed_result.solutions.empty()) {
        EXPECT_DOUBLE_EQ(typed_result.solutions.front().cost, erased_result.solutions.front().cost);
        EXPECT_EQ(typed_result.solutions.front().path_arc_ids,
                  erased_result.solutions.front().path_arc_ids);
    }
}

// The erased overload stays reachable and unambiguous through base-typed locals. If the
// `!same_as` constraints are missing, THIS is what fails -- with an ambiguity error, at compile
// time. It is therefore a compile-time test that happens to be spelled as a runtime one.
TEST(TypedAddResource, BaseTypedLocalSelectsTheErasedOverload) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    std::unique_ptr<ExtensionFunction<RealResource>> extension =
        std::make_unique<AdditionExtensionFunction<RealResource>>();
    std::unique_ptr<FeasibilityFunction<RealResource>> feasibility =
        std::make_unique<TrivialFeasibilityFunction<RealResource>>();
    std::unique_ptr<CostFunction<RealResource>> cost =
        std::make_unique<ValueCostFunction<RealResource>>();
    std::unique_ptr<DominanceFunction<RealResource>> dominance =
        std::make_unique<ValueDominanceFunction<RealResource>>();

    graph->add_resource<RealResource>(std::move(extension),
                                      std::move(feasibility),
                                      std::move(cost),
                                      std::move(dominance));
    SUCCEED();
}

// One base-typed argument is enough to disqualify the typed overload -- the mixed case works, and
// it is worth pinning, because the reroutes in test_bidirectional.hpp rely on it.
TEST(TypedAddResource, OneBaseTypedArgumentIsEnoughToSelectTheErasedOverload) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    std::unique_ptr<ExtensionFunction<RealResource>> extension =
        std::make_unique<AdditionExtensionFunction<RealResource>>();

    graph->add_resource<RealResource>(std::move(extension),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    SUCCEED();
}
