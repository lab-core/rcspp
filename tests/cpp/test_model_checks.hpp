// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The model checks a backward or a bidirectional search needs (BackwardExtensionCheck, JoinCheck),
// run through ResourceGraph::check_model, without solving. Each incoherent model below is refused
// for the direction whose search it would mislead, and never for a forward search, which reads
// none of the backward semantics. Where it helps, a forward solve shows what the model's answer
// is.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace model_checks_test {

using Composed = ResourceTypeComposition<RealResource>;

constexpr double kTolerance = 1e-9;

/// @brief Every problem the checks of a search in @p direction find, one per line.
inline std::string problems(const ResourceGraph<RealResource>& graph, SearchDirection direction) {
    std::string text;
    for (const auto& problem : graph.check_model(direction).problems) {
        text += problem + "\n";
    }
    return text;
}

inline double best_cost(const SolveResult& result) {
    return result.solutions.empty() ? std::numeric_limits<double>::infinity()
                                    : result.solutions.front().cost;
}

/// @brief A cost slot plus a time-window clock.
inline std::unique_ptr<ResourceGraph<RealResource>> clocked_graph(
    const std::map<size_t, std::pair<double, double>>& windows,
    const std::vector<std::tuple<double, double, size_t, size_t>>& arcs) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());

    const size_t last = windows.empty() ? 0 : windows.rbegin()->first;
    for (const auto& [node_id, window] : windows) {
        graph->add_node(node_id, node_id == 0, node_id == last);
    }
    for (const auto& [cost, time, origin, destination] : arcs) {
        graph->add_arc<RealResource, RealResource>({cost, time}, origin, destination, cost);
    }
    return graph;
}

/// @brief A cost slot plus @p extension and @p feasibility on slot 1, on the given arcs.
inline std::unique_ptr<ResourceGraph<RealResource>> threshold_pairing_graph(
    std::unique_ptr<ExtensionFunction<RealResource>> extension,
    std::unique_ptr<FeasibilityFunction<RealResource>> feasibility, size_t num_nodes,
    const std::vector<std::tuple<double, double, size_t, size_t>>& arcs) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(std::move(extension),
                                      std::move(feasibility),
                                      std::make_unique<TrivialCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    for (size_t node_id = 0; node_id < num_nodes; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id + 1 == num_nodes);
    }
    for (const auto& [cost, time, origin, destination] : arcs) {
        graph->add_arc<RealResource, RealResource>({cost, time}, origin, destination, cost);
    }
    return graph;
}

/// @brief A composition dominance function written for forward-only use: cost alone decides.
///
/// Overrides only `check_dominance`; must keep compiling, so `check_back_dominance` is not pure.
class CostOnlyDominance : public Clonable<CostOnlyDominance, DominanceFunction<Composed>> {
    public:
        [[nodiscard]] auto check_dominance(const Resource<Composed>& lhs_resource,
                                           const Resource<Composed>& rhs_resource)
            -> bool override {
            return lhs_resource.get_cost() <= rhs_resource.get_cost();
        }
};

/// @brief Objective = distance + arrival time when @p time_cost is a `ValueCostFunction`.
///
/// Route A 0-1-3: distance 1 + 1, time 10 + 10, so 22. Route B 0-2-3: distance 5 + 5, time 1 + 1,
/// so 12. The sink closes at 22.
inline std::unique_ptr<ResourceGraph<RealResource>> distance_plus_time_graph(
    std::unique_ptr<CostFunction<Composed>> cost,
    std::unique_ptr<CostFunction<RealResource>> time_cost) {
    const std::map<size_t, std::pair<double, double>> windows{{3, {0.0, 22.0}}};
    auto graph = std::make_unique<ResourceGraph<RealResource>>(
        std::make_unique<CompositionExtensionFunction<RealResource>>(),
        std::make_unique<CompositionFeasibilityFunction<RealResource>>(),
        std::move(cost),
        std::make_unique<CompositionDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        std::move(time_cost),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    for (size_t node_id = 0; node_id < 4; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id == 3);
    }
    graph->add_arc<RealResource, RealResource>({1.0, 10.0}, 0, 1, 1.0);
    graph->add_arc<RealResource, RealResource>({1.0, 10.0}, 1, 3, 1.0);
    graph->add_arc<RealResource, RealResource>({5.0, 1.0}, 0, 2, 5.0);
    graph->add_arc<RealResource, RealResource>({5.0, 1.0}, 2, 3, 5.0);
    return graph;
}

constexpr double kStartOfTen = 10.0;

/// @brief A bottleneck: the largest arc value seen. It extends backward the same way, so it
///        declares an accumulation, but a path's value is not the sum of its halves' values.
class Bottleneck
    : public Clonable<Bottleneck,
                      BackwardForm<ExtensionFunction<RealResource>, BackwardKind::Accumulate>,
                      ExtensionFunction<RealResource>> {
    public:
        void extend(const RealResource& resource, const RealResource& extender_value,
                    RealResource* extended_resource) override {
            extended_resource->set_value(
                std::max(resource.get_value(), extender_value.get_value()));
        }
};

/// @brief An addition whose backward labels start at 10 rather than at the type default.
class AdditionStartingAtTen
    : public Clonable<AdditionStartingAtTen,
                      BackwardForm<ExtensionFunction<RealResource>, BackwardKind::Accumulate>,
                      ExtensionFunction<RealResource>> {
    public:
        void extend(const RealResource& resource, const RealResource& extender_value,
                    RealResource* extended_resource) override {
            extended_resource->set_value(resource.get_value() + extender_value.get_value());
        }

        void start_back(RealResource* resource) override { resource->set_value(kStartOfTen); }
};

/// @brief Distance plus a load, on the line 0 -> 1 -> 2 whose arcs carry loads 3, then 5; the
///        cost adds every component's cost.
inline std::unique_ptr<ResourceGraph<RealResource>> load_line(
    std::unique_ptr<ExtensionFunction<RealResource>> load,
    std::unique_ptr<FeasibilityFunction<RealResource>> load_feasibility,
    std::unique_ptr<CostFunction<RealResource>> load_cost) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>(
        std::make_unique<CompositionExtensionFunction<RealResource>>(),
        std::make_unique<CompositionFeasibilityFunction<RealResource>>(),
        std::make_unique<CompositionCostFunction<RealResource>>(),
        std::make_unique<CompositionDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(std::move(load),
                                      std::move(load_feasibility),
                                      std::move(load_cost),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    for (size_t node_id = 0; node_id < 3; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id == 2);
    }
    graph->add_arc<RealResource, RealResource>({1.0, 3.0}, 0, 1, 1.0);
    graph->add_arc<RealResource, RealResource>({1.0, 5.0}, 1, 2, 1.0);
    return graph;
}

/// @brief A time window written by hand, without @c NodeBounds: node 1 opens at 5, node 2
///        closes at 6, every other bound is 0 or 100.
class HandWrittenWindows : public Clonable<HandWrittenWindows, FeasibilityFunction<RealResource>> {
    public:
        auto is_feasible(const RealResource& resource) -> bool override {
            return resource.leq(close_);
        }

        auto is_back_feasible(const RealResource& resource) -> bool override {
            return resource.geq(open_);
        }

        [[nodiscard]] JoinRule join_rule() const override { return JoinRule::ValueOrder; }

    protected:
        void preprocess(size_t node_id) override {
            open_ = node_id == 1 ? 5.0 : 0.0;
            close_ = node_id == 2 ? 6.0 : 100.0;
        }

    private:
        double open_ = 0.0;
        double close_ = 100.0;
};

/// @brief Cost, a time-window clock and a load under a floor of 0, on 0 -> 1 -> 2 -> 4 (loads 0,
///        -3, +5) and 0 -> 4; with @p detour, node 3 keeps the feasibility preprocessor from
///        removing 1 -> 2 and 2 -> 4. The load is a capacity or, without @p capacity, an addition.
inline std::unique_ptr<ResourceGraph<RealResource>> negative_load_graph(bool capacity,
                                                                        bool detour) {
    std::map<size_t, std::pair<double, double>> windows;
    for (size_t node_id = 0; node_id < 5; ++node_id) {
        windows[node_id] = {0.0, 100.0};
    }
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    if (capacity) {
        graph->add_resource<RealResource>(
            std::make_unique<CapacityExtensionFunction<RealResource>>(10.0),
            std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 10.0),
            std::make_unique<TrivialCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
    } else {
        graph->add_resource<RealResource>(
            std::make_unique<AdditionExtensionFunction<RealResource>>(),
            std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 10.0),
            std::make_unique<TrivialCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
    }
    for (size_t node_id = 0; node_id < 5; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id == 4);
    }
    // {cost, load, origin, destination}; every arc takes one unit of time. Arc 1 is 1->2.
    std::vector<std::tuple<double, double, size_t, size_t>> arcs{{-10.0, 0.0, 0, 1},
                                                                 {0.0, -3.0, 1, 2},
                                                                 {0.0, 5.0, 2, 4},
                                                                 {0.0, 0.0, 0, 4}};
    if (detour) {
        arcs.insert(arcs.end(), {{0.0, 0.0, 0, 3}, {0.0, 5.0, 3, 1}, {0.0, 5.0, 3, 2}});
    }
    for (const auto& [cost, load, origin, destination] : arcs) {
        graph->add_arc<RealResource, RealResource, RealResource>(
            std::make_tuple(std::make_tuple(cost), std::make_tuple(1.0), std::make_tuple(load)),
            origin,
            destination,
            cost);
    }
    return graph;
}

/// @brief A composition extension function written for forward-only use.
class ForwardOnlyCompositionExtension
    : public Clonable<ForwardOnlyCompositionExtension, ExtensionFunction<Composed>> {
    public:
        void extend(const Resource<Composed>& resource, const Extender<Composed>& extender,
                    Resource<Composed>* extended_resource) override {
            extended_resource->for_each_component(
                resource,
                extender,
                [](auto& extended, const auto& current, const auto& component_extender) {
                    component_extender.extend(current, &extended);
                });
        }
};

/// @brief A composition feasibility function written for forward-only use.
class ForwardOnlyCompositionFeasibility
    : public Clonable<ForwardOnlyCompositionFeasibility, FeasibilityFunction<Composed>> {
    public:
        auto is_feasible(const Resource<Composed>& resource) -> bool override {
            return resource.for_each_component_and(
                [](const auto& component) { return component.is_feasible(); });
        }
};

/// @brief A 3-arc line of unit-cost arcs taking 3 each, the sink closing at 8, with the given
///        composition functions.
inline std::unique_ptr<ResourceGraph<RealResource>> composition_line(
    std::unique_ptr<ExtensionFunction<Composed>> extension,
    std::unique_ptr<FeasibilityFunction<Composed>> feasibility) {
    const std::map<size_t, std::pair<double, double>> windows{{3, {0.0, 8.0}}};
    auto graph = std::make_unique<ResourceGraph<RealResource>>(
        std::move(extension),
        std::move(feasibility),
        std::make_unique<ComponentCostFunction<0, RealResource>>(0),
        std::make_unique<CompositionDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    for (size_t node_id = 0; node_id <= 3; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id == 3);
    }
    for (size_t node_id = 0; node_id < 3; ++node_id) {
        graph->add_arc<RealResource, RealResource>({1.0, 3.0}, node_id, node_id + 1, 1.0);
    }
    return graph;
}

/// @brief A per-node cap written the way the library's functions are: the bound is cached in
///        `preprocess()`.
class CachedNodeCap : public Clonable<CachedNodeCap, FeasibilityFunction<RealResource>> {
    public:
        CachedNodeCap(std::map<size_t, double> caps, double default_cap)
            : caps_(std::make_shared<const std::map<size_t, double>>(std::move(caps))),
              default_cap_(default_cap),
              cap_(default_cap) {}

        auto is_feasible(const RealResource& resource) -> bool override {
            return resource.get_value() >= 0.0 && resource.get_value() <= cap_;
        }
        [[nodiscard]] JoinRule join_rule() const override {
            return backward_kind_ == BackwardKind::Threshold ? JoinRule::ValueOrder
                                                             : JoinRule::Unspecified;
        }

    protected:
        void preprocess(size_t node_id) override {
            auto it = caps_->find(node_id);
            cap_ = it != caps_->end() ? it->second : default_cap_;
        }

    private:
        std::shared_ptr<const std::map<size_t, double>> caps_;
        double default_cap_;
        double cap_;
};

/// @brief A threshold written without `ThresholdForm`: it adds going forward, going backward
///        subtracts and clamps to its own closing time at the node left, and starts a backward
///        label at the closing time of the node it enters.
///
/// It declares nothing about its clamps or starts, so the checks can only learn them by running
/// it.
class HandWrittenThreshold
    : public Clonable<HandWrittenThreshold, ExtensionFunction<RealResource>> {
    public:
        HandWrittenThreshold(std::map<size_t, double> closing, double default_closing)
            : closing_(std::make_shared<const std::map<size_t, double>>(std::move(closing))),
              default_closing_(default_closing),
              close_(default_closing) {}

        [[nodiscard]] BackwardKind backward_kind() const override {
            return BackwardKind::Threshold;
        }

        void extend(const RealResource& resource, const RealResource& extender_value,
                    RealResource* extended_resource) override {
            extended_resource->set_value(resource.get_value() + extender_value.get_value());
        }

        void extend_back(const RealResource& resource, const RealResource& extender_value,
                         RealResource* extended_resource) override {
            extended_resource->set_value(
                std::min(close_, resource.get_value() - extender_value.get_value()));
        }

        void start_back(RealResource* resource) override { resource->set_value(start_); }

    protected:
        void preprocess(size_t origin_id, size_t destination_id) override {
            close_ = closing_at(origin_id);
            start_ = closing_at(destination_id);
        }

    private:
        std::shared_ptr<const std::map<size_t, double>> closing_;
        double default_closing_;
        double close_;
        double start_ = 0.0;

        [[nodiscard]] double closing_at(size_t node_id) const {
            const auto it = closing_->find(node_id);
            return it != closing_->end() ? it->second : default_closing_;
        }
};

/// @brief A `ThresholdForm` that waits for nothing and clamps nothing.
class UnclampedThreshold
    : public Clonable<UnclampedThreshold,
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

/// @brief Node 1 closes at 3, every other node at 100.
inline std::unique_ptr<TimeWindowFeasibilityFunction<RealResource>> closes_node_one_at_three() {
    return std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(
        std::map<size_t, std::pair<double, double>>{{1, {0.0, 3.0}}},
        100.0);
}

/// @brief A cost slot plus a capacity whose per-node caps live wherever the caller says.
inline std::unique_ptr<ResourceGraph<RealResource>> capacity_graph(
    SharedNodeBounds<double> extension_caps, SharedNodeBounds<double> feasibility_caps) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<CapacityExtensionFunction<RealResource>>(std::move(extension_caps)),
        std::make_unique<MinMaxFeasibilityFunction<RealResource>>(std::move(feasibility_caps)),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    return graph;
}

/// @brief A coherent model: cost, a time-window clock and a capacity, on two routes.
inline std::unique_ptr<ResourceGraph<RealResource>> coherent_graph() {
    std::map<size_t, std::pair<double, double>> windows;
    for (size_t node_id = 0; node_id < 4; ++node_id) {
        windows[node_id] = {0.0, 100.0};
    }
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<CapacityExtensionFunction<RealResource>>(10.0),
        std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 10.0),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    for (size_t node_id = 0; node_id < 4; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id == 3);
    }
    // {cost, time, load, origin, destination}
    for (const auto& [cost, time, load, origin, destination] :
         std::vector<std::tuple<double, double, double, size_t, size_t>>{{1.0, 2.0, 3.0, 0, 1},
                                                                         {1.0, 2.0, 3.0, 1, 3},
                                                                         {4.0, 1.0, 1.0, 0, 2},
                                                                         {4.0, 1.0, 1.0, 2, 3}}) {
        graph->add_arc<RealResource, RealResource, RealResource>(
            std::make_tuple(std::make_tuple(cost), std::make_tuple(time), std::make_tuple(load)),
            origin,
            destination,
            cost);
    }
    return graph;
}

}  // namespace model_checks_test

// ============================================================================
// What a forward search needs, and what passes
// ============================================================================

/// @brief A coherent model passes the checks of every direction.
TEST(ModelChecks, ACoherentModelPasses) {
    namespace mc = model_checks_test;
    const auto graph = mc::coherent_graph();
    for (const auto direction :
         {SearchDirection::Forward, SearchDirection::Backward, SearchDirection::Bidirectional}) {
        EXPECT_TRUE(graph->check_model(direction).ok())
            << to_string(direction) << ": " << mc::problems(*graph, direction);
    }
}

/// @brief A refusal lists every problem, under a header that names the search.
TEST(ModelChecks, ModelRefusedListsEveryProblem) {
    const ModelRefused refused(refusal_header(SearchDirection::Bidirectional),
                               {"first problem", "second problem"});
    EXPECT_EQ(std::string(refused.what()),
              "a bidirectional search cannot run on this model:\n  - first problem\n  - second "
              "problem");
    EXPECT_EQ(refused.problems(), (std::vector<std::string>{"first problem", "second problem"}));
    EXPECT_EQ(refusal_header(SearchDirection::Backward),
              "a backward search cannot run on this model:");

    namespace mc = model_checks_test;
    auto graph =
        mc::distance_plus_time_graph(std::make_unique<CompositionCostFunction<RealResource>>(),
                                     std::make_unique<ValueCostFunction<RealResource>>());
    try {
        enforce_checks(*graph, SearchDirection::Backward);
        FAIL() << "a non-additive cost must be refused";
    } catch (const ModelRefused& error) {
        EXPECT_EQ(std::string(error.what()).rfind("a backward search cannot run on this model:", 0),
                  0U)
            << error.what();
        ASSERT_FALSE(error.problems().empty());
    }
    EXPECT_NO_THROW(enforce_checks(*graph, SearchDirection::Forward));
}

/// @brief The checks read arcs that were removed: a model whose arcs are all gone is refused
///        like any other, rather than passing because nothing is left to read.
TEST(ModelChecks, ARemovedArcStillCounts) {
    namespace mc = model_checks_test;
    auto graph =
        mc::distance_plus_time_graph(std::make_unique<CompositionCostFunction<RealResource>>(),
                                     std::make_unique<ValueCostFunction<RealResource>>());
    const std::string before = mc::problems(*graph, SearchDirection::Bidirectional);
    ASSERT_FALSE(before.empty());
    for (size_t arc_id = 0; arc_id < graph->next_arc_id(); ++arc_id) {
        graph->remove_arc(arc_id);
    }
    ASSERT_EQ(graph->get_number_of_arcs(), 0U);
    EXPECT_EQ(mc::problems(*graph, SearchDirection::Bidirectional), before);
}

// ============================================================================
// Declarations
// ============================================================================

/// @brief An undeclared component is named, for its backward kind (a backward search's problem)
///        and for its join rule (a bidirectional search's).
TEST(ModelChecks, AnUndeclaredComponentIsNamed) {
    namespace mc = model_checks_test;

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

    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<UndeclaredExtensionFunction>(),
                                      std::make_unique<UndeclaredFeasibilityFunction>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource>(std::make_tuple(1.0), 0, 1, 1.0);

    const std::string both = mc::problems(*graph, SearchDirection::Bidirectional);
    EXPECT_NE(both.find("component 0: its extension function declares no backward_kind()"),
              std::string::npos)
        << both;
    EXPECT_NE(both.find("component 0: its feasibility function declares no join_rule()"),
              std::string::npos)
        << both;
    const std::string backward = mc::problems(*graph, SearchDirection::Backward);
    EXPECT_NE(backward.find("backward_kind"), std::string::npos) << backward;
    EXPECT_EQ(backward.find("join_rule"), std::string::npos) << backward;
    EXPECT_TRUE(graph->check_model(SearchDirection::Forward).ok());
}

/// @brief A floor on a threshold resource declares no join rule, so a bidirectional search is
///        refused rather than joining past it.
///
/// A backward label carries only a ceiling, so it cannot check the floor of 5 that makes the only
/// path infeasible.
TEST(ModelChecks, AFloorOnAThresholdResourceIsRefused) {
    namespace mc = model_checks_test;
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<CapacityExtensionFunction<RealResource>>(100.0),
        std::make_unique<MinMaxFeasibilityFunction<RealResource>>(5.0, 100.0),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource, RealResource>({-5.0, 1.0}, 0, 1, -5.0);
    graph->add_arc<RealResource, RealResource>({-5.0, 1.0}, 1, 2, -5.0);

    const auto forward =
        graph->solve<SimpleDominanceAlgorithm>(std::numeric_limits<double>::infinity(),
                                               AlgorithmParams<LabelList<mc::Composed>>{},
                                               /*preprocess=*/false);
    EXPECT_TRUE(forward.solutions.empty()) << "node 1 is below the floor on the only path";

    const std::string message = mc::problems(*graph, SearchDirection::Bidirectional);
    EXPECT_NE(message.find("component 1"), std::string::npos) << message;
    EXPECT_NE(message.find("join_rule"), std::string::npos) << message;
    EXPECT_EQ(mc::problems(*graph, SearchDirection::Backward).find("join_rule"), std::string::npos);
}

/// @brief A join rule that reads a bound, on an accumulation, is refused for a bidirectional
///        search, rather than answering with it.
TEST(ModelChecks, AccumulatingExtensionWithAValueOrderJoinIsRefused) {
    namespace mc = model_checks_test;

    /// A bound's join rule on an accumulating resource.
    class BoundRuleOnAnAccumulation
        : public Clonable<BoundRuleOnAnAccumulation, FeasibilityFunction<RealResource>> {
        public:
            auto is_feasible(const RealResource& resource) -> bool override {
                return resource.leq(10.0);
            }
            [[nodiscard]] JoinRule join_rule() const override { return JoinRule::ValueOrder; }
    };

    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<BoundRuleOnAnAccumulation>(),
                                      std::make_unique<TrivialCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource, RealResource>({1.0, 1.0}, 0, 1, 1.0);

    const std::string message = mc::problems(*graph, SearchDirection::Bidirectional);
    EXPECT_NE(message.find("component 1: its extension accumulates but its feasibility function "
                           "declares JoinRule::ValueOrder"),
              std::string::npos)
        << message;
    EXPECT_TRUE(graph->check_model(SearchDirection::Backward).ok())
        << mc::problems(*graph, SearchDirection::Backward);
}

// ============================================================================
// Composition functions without a backward form
// ============================================================================

/// @brief A forward-only composition dominance function solves forward, and is refused for a
///        backward search with the override it lacks named.
TEST(ModelChecks, AForwardOnlyCompositionDominanceIsRefused) {
    namespace mc = model_checks_test;
    auto graph = std::make_unique<ResourceGraph<RealResource>>(
        std::make_unique<CompositionExtensionFunction<RealResource>>(),
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

    const auto forward = graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    ASSERT_FALSE(forward.solutions.empty());
    EXPECT_NEAR(forward.solutions.front().cost, 2.0, mc::kTolerance);

    for (const auto direction : {SearchDirection::Backward, SearchDirection::Bidirectional}) {
        const std::string message = mc::problems(*graph, direction);
        EXPECT_NE(message.find("check_back_dominance"), std::string::npos)
            << to_string(direction) << ": " << message;
    }
    EXPECT_TRUE(graph->check_model(SearchDirection::Forward).ok());
}

/// @brief A composition extension or feasibility function that overrides only its forward half
///        is refused, naming the override it lacks; a join test is a bidirectional search's
///        problem only.
///
/// On a line arriving at 9 against a sink closing at 8, the forward-only extension used to return
/// cost 3, status complete, and the forward-only feasibility function threw "can_be_joined not
/// implemented" from the join, after both searches had run.
TEST(ModelChecks, CompositionFunctionsWithoutABackwardFormAreRefused) {
    namespace mc = model_checks_test;
    auto extension_only =
        mc::composition_line(std::make_unique<mc::ForwardOnlyCompositionExtension>(),
                             std::make_unique<CompositionFeasibilityFunction<RealResource>>());
    const std::string extension_message =
        mc::problems(*extension_only, SearchDirection::Bidirectional);
    EXPECT_NE(extension_message.find("extend_back"), std::string::npos) << extension_message;
    EXPECT_NE(mc::problems(*extension_only, SearchDirection::Backward).find("extend_back"),
              std::string::npos);

    auto feasibility_only =
        mc::composition_line(std::make_unique<CompositionExtensionFunction<RealResource>>(),
                             std::make_unique<mc::ForwardOnlyCompositionFeasibility>());
    const std::string both = mc::problems(*feasibility_only, SearchDirection::Bidirectional);
    EXPECT_NE(both.find("is_back_feasible"), std::string::npos) << both;
    EXPECT_NE(both.find("can_be_joined"), std::string::npos) << both;
    EXPECT_EQ(both.find("not implemented"), std::string::npos) << both;
    const std::string backward = mc::problems(*feasibility_only, SearchDirection::Backward);
    EXPECT_NE(backward.find("is_back_feasible"), std::string::npos) << backward;
    EXPECT_EQ(backward.find("can_be_joined"), std::string::npos) << backward;
    EXPECT_TRUE(feasibility_only->check_model(SearchDirection::Forward).ok());
}

// ============================================================================
// The join adds the two halves' costs, so a cost that does not add is refused
// ============================================================================

/// @brief A cost that reads a threshold's value is refused, naming the cost function.
///
/// The backward value of a time window is a deadline, so adding it as a cost gave route A at cost
/// 4 (distance 2 + deadline 2) where its true cost is 22 and the optimum is route B at 12. A
/// backward search alone pays it too: a backward label reaching a source is a complete path.
TEST(ModelChecks, ANonAdditiveCostIsRefused) {
    namespace mc = model_checks_test;
    auto graph =
        mc::distance_plus_time_graph(std::make_unique<CompositionCostFunction<RealResource>>(),
                                     std::make_unique<ValueCostFunction<RealResource>>());
    ASSERT_NEAR(mc::best_cost(graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{})),
                12.0,
                mc::kTolerance);
    for (const auto direction : {SearchDirection::Backward, SearchDirection::Bidirectional}) {
        const std::string message = mc::problems(*graph, direction);
        EXPECT_NE(message.find("cost function"), std::string::npos)
            << to_string(direction) << ": " << message;
    }
    EXPECT_TRUE(graph->check_model(SearchDirection::Forward).ok());
}

/// @brief The controls: the same model with a zero cost on time, and with the default cost
///        function, passes.
TEST(ModelChecks, AnAdditiveCostOverAThresholdClockPasses) {
    namespace mc = model_checks_test;
    const auto zero_time_cost =
        mc::distance_plus_time_graph(std::make_unique<CompositionCostFunction<RealResource>>(),
                                     std::make_unique<TrivialCostFunction<RealResource>>());
    const auto default_cost =
        mc::distance_plus_time_graph(std::make_unique<ComponentCostFunction<0, RealResource>>(0),
                                     std::make_unique<ValueCostFunction<RealResource>>());
    EXPECT_TRUE(zero_time_cost->check_model(SearchDirection::Bidirectional).ok())
        << mc::problems(*zero_time_cost, SearchDirection::Bidirectional);
    EXPECT_TRUE(default_cost->check_model(SearchDirection::Bidirectional).ok())
        << mc::problems(*default_cost, SearchDirection::Bidirectional);
}

// ============================================================================
// The join adds an accumulation's two halves, so it must be a sum
// ============================================================================

/// @brief A bottleneck whose two halves the join adds is refused for a bidirectional search,
///        whether its cost or its join test adds them.
///
/// The path's peak load is 5, but its halves hold 3 and 5: priced by its value, the join costed
/// the path at 2 + 8 rather than 2 + 5; under `MinMaxFeasibilityFunction(0, 6)`, whose join tests
/// `forward + backward <= 6`, it rejected a path whose peak is within the cap.
TEST(ModelChecks, ABottleneckWhoseHalvesAreAddedIsRefused) {
    namespace mc = model_checks_test;
    auto priced = mc::load_line(std::make_unique<mc::Bottleneck>(),
                                std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                std::make_unique<ValueCostFunction<RealResource>>());
    auto capped = mc::load_line(std::make_unique<mc::Bottleneck>(),
                                std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 6.0),
                                std::make_unique<TrivialCostFunction<RealResource>>());
    EXPECT_NEAR(mc::best_cost(priced->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{})),
                7.0,
                mc::kTolerance);
    EXPECT_NEAR(mc::best_cost(capped->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{})),
                2.0,
                mc::kTolerance);

    for (const auto* graph : {priced.get(), capped.get()}) {
        const std::string message = mc::problems(*graph, SearchDirection::Bidirectional);
        EXPECT_NE(message.find("component 1: its extension declares BackwardKind::Accumulate, but "
                               "along "),
                  std::string::npos)
            << message;
        EXPECT_NE(message.find("it is not a sum"), std::string::npos) << message;
        EXPECT_EQ(mc::problems(*graph, SearchDirection::Backward).find("not a sum"),
                  std::string::npos);
    }
}

/// @brief A bottleneck nothing adds is a valid model: a zero cost, and a join test that never
///        blocks.
TEST(ModelChecks, ABottleneckNothingAddsPasses) {
    namespace mc = model_checks_test;
    auto graph = mc::load_line(std::make_unique<mc::Bottleneck>(),
                               std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                               std::make_unique<TrivialCostFunction<RealResource>>());
    EXPECT_TRUE(graph->check_model(SearchDirection::Bidirectional).ok())
        << mc::problems(*graph, SearchDirection::Bidirectional);
}

/// @brief An accumulation whose backward labels start elsewhere than the forward ones is refused
///        for a bidirectional search: the join would count the start on top of the path's own sum.
TEST(ModelChecks, AnAccumulationThatStartsElsewhereIsRefused) {
    namespace mc = model_checks_test;
    auto graph = mc::load_line(std::make_unique<mc::AdditionStartingAtTen>(),
                               std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                               std::make_unique<ValueCostFunction<RealResource>>());
    const std::string message = mc::problems(*graph, SearchDirection::Bidirectional);
    EXPECT_NE(message.find("component 1: its extension declares BackwardKind::Accumulate, but "
                           "starts its backward labels at sink 2 at 10"),
              std::string::npos)
        << message;
    EXPECT_EQ(message.find("not a sum"), std::string::npos) << message;
}

// ============================================================================
// A threshold's backward floor against its forward clamp, and negative loads
// ============================================================================

/// @brief A backward floor the extension never clamps forward to is refused.
///
/// Capacity + TimeWindowFeasibility on the same windows: node 1 opens at 5, which the capacity
/// never waits for, and the forward test checks only the upper bound, so the path 0-1-2 (cost -3)
/// is feasible. The backward search rejected its deadline 4 at node 1 and returned nothing.
TEST(ModelChecks, ABackwardFloorWithoutAForwardClampIsRefused) {
    namespace mc = model_checks_test;
    const auto windows = make_node_bounds(0.0,
                                          std::numeric_limits<double>::max() / 2,
                                          {{1, {5.0, 100.0}}, {2, {0.0, 6.0}}});
    const auto build = [&] {
        return mc::threshold_pairing_graph(
            std::make_unique<CapacityExtensionFunction<RealResource>>(windows),
            std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
            3,
            {{1.0, 2.0, 0, 1}, {-4.0, 2.0, 1, 2}});
    };
    ASSERT_NEAR(mc::best_cost(build()->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{})),
                -3.0,
                mc::kTolerance);

    const auto graph = build();
    for (const auto direction : {SearchDirection::Backward, SearchDirection::Bidirectional}) {
        const std::string message = mc::problems(*graph, direction);
        EXPECT_NE(message.find("component 1"), std::string::npos) << message;
        EXPECT_NE(message.find("node 1"), std::string::npos) << message;
        EXPECT_EQ(message.find("clamped to"), std::string::npos) << "the caps agree: " << message;
    }
}

/// @brief A floor is found by asking the backward test, however the feasibility function is
///        written.
///
/// The case above, with windows a hand-written function tests and declares nothing about: the
/// capacity never waits for node 1's opening time, so the backward search would reject the
/// deadline 4 there while the forward path arrives at 2. The capacity starts the sink's backward
/// labels, so it shares the sink's closing time, 6.
TEST(ModelChecks, AHandWrittenFloorIsCheckedByWhatItTests) {
    namespace mc = model_checks_test;
    const auto build = [] {
        return mc::threshold_pairing_graph(
            std::make_unique<CapacityExtensionFunction<RealResource>>(
                make_node_bounds(0.0, 100.0, {{2, {0.0, 6.0}}})),
            std::make_unique<mc::HandWrittenWindows>(),
            3,
            {{1.0, 2.0, 0, 1}, {-4.0, 2.0, 1, 2}});
    };
    ASSERT_NEAR(mc::best_cost(build()->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{})),
                -3.0,
                mc::kTolerance);

    const auto graph = build();
    const std::string message = mc::problems(*graph, SearchDirection::Backward);
    EXPECT_NE(message.find("component 1: its feasibility function rejects a backward value of "
                           "0.000000 at node 1"),
              std::string::npos)
        << message;
    EXPECT_EQ(message.find("clamped to"), std::string::npos) << "the caps agree: " << message;
    EXPECT_EQ(message.find("start at"), std::string::npos) << "the caps agree: " << message;
}

/// @brief A negative load under a zero floor is refused, for a capacity and for an addition.
///
/// The path 0-1-2-4 carries loads 0, -3, +5, so its running load is -3 at node 2 and simple
/// returns 0 via 0-4. A backward label carries only a ceiling and cannot see the dip, so
/// bidirectional returned -10 via 0-1-2-4.
TEST(ModelChecks, ANegativeLoadUnderAFloorIsRefused) {
    namespace mc = model_checks_test;
    for (const bool capacity : {true, false}) {
        const auto graph = mc::negative_load_graph(capacity, /*detour=*/true);
        ASSERT_NEAR(mc::best_cost(mc::negative_load_graph(capacity, /*detour=*/true)
                                      ->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{})),
                    0.0,
                    mc::kTolerance)
            << "capacity = " << capacity;
        for (const auto direction : {SearchDirection::Backward, SearchDirection::Bidirectional}) {
            const std::string message = mc::problems(*graph, direction);
            EXPECT_NE(message.find("component 2"), std::string::npos)
                << "capacity = " << capacity << ": " << message;
            EXPECT_NE(message.find("non-negative"), std::string::npos) << message;
        }
    }
}

/// @brief A negative load is refused even on an arc that preprocessing removed.
///
/// Without node 3, nothing raises the load at node 1, so the feasibility preprocessing of a
/// forward solve removes the arc 1->2 that carries -3, for good. The checks still read it.
TEST(ModelChecks, ANegativeLoadOnARemovedArcIsStillRefused) {
    namespace mc = model_checks_test;
    constexpr size_t kNegativeArc = 1;
    for (const bool capacity : {true, false}) {
        for (const bool removed_first : {false, true}) {
            auto graph = mc::negative_load_graph(capacity, /*detour=*/false);
            if (removed_first) {
                static_cast<void>(graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}));
                const auto removed = graph->get_removed_arc_ids();
                ASSERT_NE(std::ranges::find(removed, kNegativeArc), removed.end())
                    << "preprocessing no longer removes arc 1->2, so this test no longer "
                       "exercises the scan of removed arcs";
            }
            const std::string message = mc::problems(*graph, SearchDirection::Bidirectional);
            EXPECT_NE(message.find("component 2: arc 1 -> 2 consumes -3"), std::string::npos)
                << "capacity = " << capacity << ", removed first = " << removed_first << ": "
                << message;
            EXPECT_NE(message.find("non-negative"), std::string::npos) << message;
        }
    }
}

/// @brief An arc of infinite time, a forbidden arc, does not make a time window look unclamped.
///
/// The clamps are observed by pushing values beyond every bound through an arc. Through an arc
/// that consumes infinity they come out as NaN or as an infinity of the other sign, so a clamp the
/// window does apply would read as none, and the model would be refused. Node 1 is entered and
/// left first by a forbidden arc, then by a usable one, which the probe must pick instead. Node 2
/// has only forbidden arcs; its window opens at 5, so a forward clamp read as none would also be
/// refused as an unchecked floor.
TEST(ModelChecks, AnArcOfInfiniteTimeIsNotProbedForAClamp) {
    namespace mc = model_checks_test;
    constexpr double kForbidden = std::numeric_limits<double>::infinity();
    for (const bool removed_first : {false, true}) {
        auto graph = mc::clocked_graph(
            {{0, {0.0, 100.0}}, {1, {0.0, 100.0}}, {2, {5.0, 100.0}}, {3, {0.0, 100.0}}},
            {{-5.0, kForbidden, 0, 1},
             {1.0, 1.0, 0, 1},
             {-5.0, kForbidden, 1, 3},
             {1.0, 1.0, 1, 3},
             {-5.0, kForbidden, 0, 2},
             {-5.0, kForbidden, 2, 3}});
        if (removed_first) {
            // The feasibility preprocessing of a forward solve removes the forbidden arcs.
            ASSERT_NEAR(
                mc::best_cost(graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{})),
                2.0,
                mc::kTolerance);
        }
        EXPECT_TRUE(graph->check_model(SearchDirection::Bidirectional).ok())
            << "removed first = " << removed_first << ": "
            << mc::problems(*graph, SearchDirection::Bidirectional);
    }
}

// ============================================================================
// Threshold ceilings and starts
// ============================================================================

/// @brief Caps that differ between the extension and the feasibility function are refused, in
///        both directions, rather than letting the two searches solve different models.
///
/// Tighter: the extension caps node 1 at 5 where the forward search allows 100, so the backward
/// search would reject the deadline of the -10 path. Looser: the feasibility function caps node 1
/// at 5 and the extension does not, which `MinMaxFeasibilityFunction` rejects backward too.
TEST(ModelChecks, CapsThatDifferBetweenTheTwoFunctionsAreRefused) {
    namespace mc = model_checks_test;
    const auto build =
        [](SharedNodeBounds<double> extension_caps, SharedNodeBounds<double> feasibility_caps) {
            auto graph = mc::capacity_graph(std::move(extension_caps), std::move(feasibility_caps));
            graph->add_node(0, /*source=*/true, /*sink=*/false);
            graph->add_node(1);
            graph->add_node(3);
            graph->add_node(2, /*source=*/false, /*sink=*/true);
            graph->add_arc<RealResource, RealResource>({-5.0, 10.0}, 0, 1, -5.0);
            graph->add_arc<RealResource, RealResource>({-5.0, 10.0}, 1, 2, -5.0);
            graph->add_arc<RealResource, RealResource>({-0.5, 1.0}, 0, 3, -0.5);
            graph->add_arc<RealResource, RealResource>({-0.5, 1.0}, 3, 2, -0.5);
            return graph;
        };
    const auto uniform = make_node_bounds(0.0, 100.0);
    const auto capped = make_node_bounds(0.0, 100.0, {{1, {0.0, 5.0}}});

    const std::string tighter = mc::problems(*build(capped, uniform), SearchDirection::Backward);
    EXPECT_NE(tighter.find("component 1: its backward labels at node 1 are clamped to 5"),
              std::string::npos)
        << tighter;
    EXPECT_NE(tighter.find("rejects deadlines a forward path meets"), std::string::npos) << tighter;
    EXPECT_NE(tighter.find("NodeBounds"), std::string::npos) << tighter;

    const std::string looser = mc::problems(*build(uniform, capped), SearchDirection::Backward);
    EXPECT_NE(looser.find("its backward labels at node 1 are clamped to 100"), std::string::npos)
        << looser;
    EXPECT_NE(looser.find("which its feasibility function rejects there"), std::string::npos)
        << looser;
}

/// @brief A per-node cap the capacity does not share leaves the capacity's own clamp in force,
///        which the cap rejects, so it is refused rather than returning nothing.
TEST(ModelChecks, ABackwardClampTheNodeRejectsIsRefused) {
    namespace mc = model_checks_test;
    const auto graph = mc::threshold_pairing_graph(
        std::make_unique<CapacityExtensionFunction<RealResource>>(make_node_bounds(0.0, 10.0)),
        std::make_unique<mc::CachedNodeCap>(std::map<size_t, double>{{2, 2.0}}, 10.0),
        4,
        {{1.0, 1.0, 0, 1}, {1.0, 1.0, 1, 2}, {1.0, 1.0, 2, 3}});
    const std::string message = mc::problems(*graph, SearchDirection::Backward);
    EXPECT_NE(message.find("component 1: its backward labels at node 2 are clamped to 10"),
              std::string::npos)
        << message;
    EXPECT_NE(message.find("which its feasibility function rejects there"), std::string::npos)
        << message;
    EXPECT_NE(message.find("NodeBounds"), std::string::npos) << message;
}

/// @brief A per-node cap shared with the capacity passes.
TEST(ModelChecks, ACustomPerNodeCapSharedWithTheCapacityPasses) {
    namespace mc = model_checks_test;
    const auto caps = make_node_bounds(0.0, 10.0, {{2, {0.0, 2.0}}});
    const auto graph = mc::threshold_pairing_graph(
        std::make_unique<CapacityExtensionFunction<RealResource>>(caps),
        std::make_unique<mc::CachedNodeCap>(std::map<size_t, double>{{2, 2.0}}, 10.0),
        4,
        {{1.0, 1.0, 0, 1}, {1.0, 1.0, 1, 2}, {1.0, 1.0, 2, 3}});
    EXPECT_TRUE(graph->check_model(SearchDirection::Bidirectional).ok())
        << mc::problems(*graph, SearchDirection::Bidirectional);
}

/// @brief A clamp looser than the node's upper bound is refused even where the backward test
///        reads only the opening time, as a time window's does.
///
/// The extension closes node 1 at 100 while the feasibility function closes it at 3, and
/// `TimeWindowFeasibilityFunction::is_back_feasible` accepts any deadline after the opening time.
/// Only asking the forward test sees it.
TEST(ModelChecks, AClampAboveATimeWindowsClosingTimeIsRefused) {
    namespace mc = model_checks_test;
    const std::map<size_t, std::pair<double, double>> closes_late{{1, {0.0, 100.0}}};
    const std::map<size_t, std::pair<double, double>> closes_early{{1, {0.0, 3.0}}};
    const auto graph = mc::threshold_pairing_graph(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(closes_late),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(closes_early),
        3,
        {{1.0, 2.0, 0, 1}, {1.0, 2.0, 1, 2}});
    const std::string message = mc::problems(*graph, SearchDirection::Backward);
    EXPECT_NE(message.find("component 1: its backward labels at node 1 are clamped to 100"),
              std::string::npos)
        << message;
    EXPECT_NE(message.find("above the largest value its feasibility function admits there"),
              std::string::npos)
        << message;
    EXPECT_NE(message.find("admits deadlines the forward search rejects"), std::string::npos)
        << message;
}

/// @brief A threshold that is not a `ThresholdForm` is checked by what its backward step does.
///
/// It closes node 1 at 100 where the feasibility function closes it at 3. Nothing in its
/// declarations says so, but its `extend_back` clamps there to 100, which the check observes.
TEST(ModelChecks, AHandWrittenThresholdIsCheckedByWhatItDoes) {
    namespace mc = model_checks_test;
    const auto graph = mc::threshold_pairing_graph(
        std::make_unique<mc::HandWrittenThreshold>(std::map<size_t, double>{}, 100.0),
        mc::closes_node_one_at_three(),
        3,
        {{1.0, 1.0, 0, 1}, {1.0, 1.0, 1, 2}});
    const std::string message = mc::problems(*graph, SearchDirection::Backward);
    EXPECT_NE(message.find("component 1: its backward labels at node 1 are clamped to 100"),
              std::string::npos)
        << message;
    EXPECT_NE(message.find("above the largest value its feasibility function admits there"),
              std::string::npos)
        << message;
}

/// @brief The same hand-written threshold, closing node 1 at 3 as the feasibility function does,
///        passes.
TEST(ModelChecks, AHandWrittenThresholdThatMatchesItsWindowsPasses) {
    namespace mc = model_checks_test;
    const auto graph = mc::threshold_pairing_graph(
        std::make_unique<mc::HandWrittenThreshold>(std::map<size_t, double>{{1, 3.0}}, 100.0),
        mc::closes_node_one_at_three(),
        3,
        {{-1.0, 2.0, 0, 1}, {-1.0, 1.0, 1, 2}, {0.0, 1.0, 0, 2}});
    EXPECT_TRUE(graph->check_model(SearchDirection::Bidirectional).ok())
        << mc::problems(*graph, SearchDirection::Bidirectional);
}

/// @brief A threshold whose backward step does not clamp at all is refused where the feasibility
///        function bounds the value: its backward search would admit deadlines past every
///        closing time.
TEST(ModelChecks, AThresholdThatDoesNotClampIsRefused) {
    namespace mc = model_checks_test;
    const auto graph = mc::threshold_pairing_graph(std::make_unique<mc::UnclampedThreshold>(),
                                                   mc::closes_node_one_at_three(),
                                                   3,
                                                   {{1.0, 1.0, 0, 1}, {1.0, 1.0, 1, 2}});
    const std::string message = mc::problems(*graph, SearchDirection::Backward);
    EXPECT_NE(message.find("component 1: its backward labels at node 0 are not clamped at all"),
              std::string::npos)
        << message;
    EXPECT_NE(message.find("rejects values above some bound there"), std::string::npos) << message;
}

/// @brief A backward start the feasibility function does not share is refused.
///
/// The capacity caps the sink at 5 where the feasibility function caps it at 100. No backward step
/// arrives at the sink, but the capacity starts its backward labels there, at 5, so the backward
/// search would reject the path 0-1-2, which reaches the sink at 20 and which simple returns.
TEST(ModelChecks, ABackwardStartTheFeasibilityFunctionDoesNotShareIsRefused) {
    namespace mc = model_checks_test;
    const auto build = [] {
        return mc::threshold_pairing_graph(
            std::make_unique<CapacityExtensionFunction<RealResource>>(
                make_node_bounds(0.0, 100.0, {{2, {0.0, 5.0}}})),
            std::make_unique<MinMaxFeasibilityFunction<RealResource>>(make_node_bounds(0.0, 100.0)),
            3,
            {{-1.0, 10.0, 0, 1}, {-1.0, 10.0, 1, 2}});
    };
    ASSERT_NEAR(mc::best_cost(build()->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{})),
                -2.0,
                mc::kTolerance);

    const auto graph = build();
    const std::string message = mc::problems(*graph, SearchDirection::Backward);
    EXPECT_NE(message.find("component 1: its backward labels at sink 2 start at 5"),
              std::string::npos)
        << message;
    EXPECT_NE(message.find("below the largest value its feasibility function admits there"),
              std::string::npos)
        << message;
}
