// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// A backward solve cannot skip the model checks: every model ModelChecks refuses for a backward
// search is refused by the solve itself, whichever way it is started, before any preprocessing.
// Reuses the models of test_model_checks.hpp, which must be included first.

#include <gtest/gtest.h>

#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "test_model_checks.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace backward_checks_test {

namespace mc = model_checks_test;

using Model = ResourceGraph<RealResource>;
using List = LabelList<mc::Composed>;

constexpr double kInf = std::numeric_limits<double>::infinity();

/// @brief A model a backward search must refuse, and a line of the refusal.
struct RefusedModel {
        const char* name;
        std::function<std::unique_ptr<Model>()> build;
        std::string problem;
};

/// A component that declares neither a backward kind nor a join rule.
class UndeclaredExtensionFunction
    : public Clonable<UndeclaredExtensionFunction, ExtensionFunction<RealResource>> {
    public:
        void extend(const RealResource& resource, const RealResource& extender_value,
                    RealResource* extended_resource) override {
            extended_resource->set_value(resource.get_value() + extender_value.get_value());
        }
};

class UndeclaredFeasibilityFunction
    : public Clonable<UndeclaredFeasibilityFunction, FeasibilityFunction<RealResource>> {
    public:
        [[nodiscard]] auto is_feasible(const RealResource& /*resource*/) -> bool override {
            return true;
        }
};

inline std::unique_ptr<Model> undeclared_component() {
    auto graph = std::make_unique<Model>();
    graph->add_resource<RealResource>(std::make_unique<UndeclaredExtensionFunction>(),
                                      std::make_unique<UndeclaredFeasibilityFunction>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource>(std::make_tuple(1.0), 0, 1, 1.0);
    return graph;
}

inline std::unique_ptr<Model> forward_only_dominance() {
    auto graph =
        std::make_unique<Model>(std::make_unique<CompositionExtensionFunction<RealResource>>(),
                                std::make_unique<CompositionFeasibilityFunction<RealResource>>(),
                                std::make_unique<ComponentCostFunction<0, RealResource>>(0),
                                std::make_unique<mc::CostOnlyDominance>());
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource>(std::make_tuple(2.0), 0, 1, 2.0);
    return graph;
}

inline std::unique_ptr<Model> non_additive_cost() {
    return mc::distance_plus_time_graph(std::make_unique<CompositionCostFunction<RealResource>>(),
                                        std::make_unique<ValueCostFunction<RealResource>>());
}

inline std::unique_ptr<Model> caps_that_differ() {
    auto graph = mc::capacity_graph(make_node_bounds(0.0, 100.0, {{1, {0.0, 5.0}}}),
                                    make_node_bounds(0.0, 100.0));
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(3);
    graph->add_node(2, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource, RealResource>({-5.0, 10.0}, 0, 1, -5.0);
    graph->add_arc<RealResource, RealResource>({-5.0, 10.0}, 1, 2, -5.0);
    graph->add_arc<RealResource, RealResource>({-0.5, 1.0}, 0, 3, -0.5);
    graph->add_arc<RealResource, RealResource>({-0.5, 1.0}, 3, 2, -0.5);
    return graph;
}

/// @brief Every model of ModelChecks that a backward search refuses, with one line of each.
inline std::vector<RefusedModel> refused_models() {
    const auto windows = make_node_bounds(0.0,
                                          std::numeric_limits<double>::max() / 2,
                                          {{1, {5.0, 100.0}}, {2, {0.0, 6.0}}});
    return {
        {"undeclared component", undeclared_component, "declares no backward_kind()"},
        {"forward-only composition dominance", forward_only_dominance, "check_back_dominance"},
        {"forward-only composition extension",
         [] {
             return mc::composition_line(
                 std::make_unique<mc::ForwardOnlyCompositionExtension>(),
                 std::make_unique<CompositionFeasibilityFunction<RealResource>>());
         },
         "extend_back"},
        {"forward-only composition feasibility",
         [] {
             return mc::composition_line(
                 std::make_unique<CompositionExtensionFunction<RealResource>>(),
                 std::make_unique<mc::ForwardOnlyCompositionFeasibility>());
         },
         "is_back_feasible"},
        {"non-additive cost", non_additive_cost, "cost function is not additive"},
        {"backward floor without a forward clamp",
         [windows] {
             return mc::threshold_pairing_graph(
                 std::make_unique<CapacityExtensionFunction<RealResource>>(windows),
                 std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
                 3,
                 {{1.0, 2.0, 0, 1}, {-4.0, 2.0, 1, 2}});
         },
         "node 1"},
        {"hand-written floor",
         [] {
             return mc::threshold_pairing_graph(
                 std::make_unique<CapacityExtensionFunction<RealResource>>(
                     make_node_bounds(0.0, 100.0, {{2, {0.0, 6.0}}})),
                 std::make_unique<mc::HandWrittenWindows>(),
                 3,
                 {{1.0, 2.0, 0, 1}, {-4.0, 2.0, 1, 2}});
         },
         "rejects a backward value of 0.000000 at node 1"},
        {"negative load under a capacity",
         [] { return mc::negative_load_graph(/*capacity=*/true, /*detour=*/true); },
         "non-negative"},
        {"negative load under an addition",
         [] { return mc::negative_load_graph(/*capacity=*/false, /*detour=*/true); },
         "non-negative"},
        {"caps that differ", caps_that_differ, "are clamped to 5"},
        {"backward clamp the node rejects",
         [] {
             return mc::threshold_pairing_graph(
                 std::make_unique<CapacityExtensionFunction<RealResource>>(
                     make_node_bounds(0.0, 10.0)),
                 std::make_unique<mc::CachedNodeCap>(std::map<size_t, double>{{2, 2.0}}, 10.0),
                 4,
                 {{1.0, 1.0, 0, 1}, {1.0, 1.0, 1, 2}, {1.0, 1.0, 2, 3}});
         },
         "at node 2 are clamped to 10"},
        {"clamp above a time window's closing time",
         [] {
             return mc::threshold_pairing_graph(
                 std::make_unique<TimeWindowExtensionFunction<RealResource>>(
                     std::map<size_t, std::pair<double, double>>{{1, {0.0, 100.0}}}),
                 std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(
                     std::map<size_t, std::pair<double, double>>{{1, {0.0, 3.0}}}),
                 3,
                 {{1.0, 2.0, 0, 1}, {1.0, 2.0, 1, 2}});
         },
         "at node 1 are clamped to 100"},
        {"hand-written threshold",
         [] {
             return mc::threshold_pairing_graph(
                 std::make_unique<mc::HandWrittenThreshold>(std::map<size_t, double>{}, 100.0),
                 mc::closes_node_one_at_three(),
                 3,
                 {{1.0, 1.0, 0, 1}, {1.0, 1.0, 1, 2}});
         },
         "at node 1 are clamped to 100"},
        {"threshold that does not clamp",
         [] {
             return mc::threshold_pairing_graph(std::make_unique<mc::UnclampedThreshold>(),
                                                mc::closes_node_one_at_three(),
                                                3,
                                                {{1.0, 1.0, 0, 1}, {1.0, 1.0, 1, 2}});
         },
         "not clamped at all"},
        {"backward start the feasibility function does not share",
         [] {
             return mc::threshold_pairing_graph(
                 std::make_unique<CapacityExtensionFunction<RealResource>>(
                     make_node_bounds(0.0, 100.0, {{2, {0.0, 5.0}}})),
                 std::make_unique<MinMaxFeasibilityFunction<RealResource>>(
                     make_node_bounds(0.0, 100.0)),
                 3,
                 {{-1.0, 10.0, 0, 1}, {-1.0, 10.0, 1, 2}});
         },
         "at sink 2 start at 5"},
    };
}

/// @brief Params for a backward search.
inline AlgorithmParams<List> backward() {
    AlgorithmParams<List> params;
    params.direction = SearchDirection::Backward;
    return params;
}

/// @brief Expects @p solve to throw a backward search's refusal containing @p problem.
template <typename Solve>
void expect_refused(Solve&& solve, const std::string& problem) {
    try {
        static_cast<void>(solve());
        ADD_FAILURE() << "not refused";
    } catch (const ModelRefused& refused) {
        const std::string message = refused.what();
        EXPECT_EQ(message.rfind(refusal_header(SearchDirection::Backward), 0), 0U) << message;
        EXPECT_NE(message.find(problem), std::string::npos) << message;
    }
}

}  // namespace backward_checks_test

/// @brief ResourceGraph::solve refuses each model for a backward search, with preprocessing or
///        without, and solves it forward.
TEST(BackwardChecks, SolveRefusesEveryModelTheChecksRefuse) {
    namespace bc = backward_checks_test;
    for (const auto& model : bc::refused_models()) {
        SCOPED_TRACE(model.name);
        for (const bool preprocess : {true, false}) {
            auto graph = model.build();
            bc::expect_refused(
                [&] {
                    return graph->solve<SimpleDominanceAlgorithm>(bc::kInf,
                                                                  bc::backward(),
                                                                  preprocess);
                },
                model.problem);
        }
        auto graph = model.build();
        EXPECT_NO_THROW(
            static_cast<void>(graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{})));
    }
}

/// @brief An algorithm solved directly, without ResourceGraph::solve, runs the checks itself.
TEST(BackwardChecks, ADirectSolveRefusesToo) {
    namespace bc = backward_checks_test;
    for (const auto& model : bc::refused_models()) {
        SCOPED_TRACE(model.name);
        auto graph = model.build();
        auto algorithm = graph->create_algorithm<SimpleDominanceAlgorithm>(bc::backward());
        bc::expect_refused([&] { return algorithm->solve(graph.get(), bc::kInf); }, model.problem);
    }
}

/// @brief A forward search that runs a backward one inside it does not let the inner search skip
///        its checks.
TEST(BackwardChecks, DiversificationSearchCannotBypass) {
    namespace bc = backward_checks_test;
    auto graph = bc::non_additive_cost();
    auto inner = graph->create_algorithm<SimpleDominanceAlgorithm>(bc::backward());
    AlgorithmParams<bc::List> params;
    params.max_iterations = 3;
    auto outer = graph->create_algorithm<DiversificationSearch>(params, std::move(inner));
    ASSERT_EQ(outer->direction(), SearchDirection::Forward);
    bc::expect_refused([&] { return graph->solve(outer.get()); }, "cost function is not additive");
}

/// @brief An upper bound under which preprocessing would remove every arc does not hide a
///        refusal: the checks run first, on the whole model.
TEST(BackwardChecks, ABoundThatRemovesEveryArcDoesNotHideARefusal) {
    namespace bc = backward_checks_test;
    auto graph = bc::non_additive_cost();
    constexpr double kBelowEveryPath = -1e9;
    bc::expect_refused(
        [&] {
            return graph->solve<SimpleDominanceAlgorithm>(kBelowEveryPath,
                                                          bc::backward(),
                                                          /*preprocess=*/true);
        },
        "cost function is not additive");
    EXPECT_EQ(graph->get_removed_arc_ids().size(), 0U) << "nothing ran before the refusal";
}
