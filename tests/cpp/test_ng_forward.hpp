// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// What the ng-path rewrite changed for forward-only solves: nothing, for the ng-route condition
// with arcs carrying `{origin}`, and a documented difference otherwise.

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace ng_forward_test {

using Graph = ResourceGraph<RealResource, RealResource, SizeTBitsetResource>;
using Sets = std::map<size_t, std::set<size_t>>;

/// @brief The formula `NgPathExtensionFunction` had before the rewrite, forward only:
///        `(memory n ng(origin)) u arc value`.
class LegacyNgPath : public Clonable<LegacyNgPath, ExtensionFunction<SizeTBitsetResource>> {
    public:
        explicit LegacyNgPath(Sets neighborhoods)
            : neighborhoods_(std::make_shared<const Sets>(std::move(neighborhoods))) {}

        void extend(const SizeTBitsetResource& resource, const SizeTBitsetResource& arc_value,
                    SizeTBitsetResource* extended_resource) override {
            extended_resource->set_value(
                arc_value.get_union(resource.get_intersection(origin_neighborhood_.get_value())));
        }

    protected:
        void preprocess(size_t origin_id, size_t /*destination_id*/) override {
            if (auto it = neighborhoods_->find(origin_id); it != neighborhoods_->end()) {
                origin_neighborhood_.set_value(it->second);
            } else {
                origin_neighborhood_.reset();
            }
        }

    private:
        std::shared_ptr<const Sets> neighborhoods_;
        SizeTBitsetResource origin_neighborhood_;
};

/// @brief A cost slot, a length capped at @p max_length, and an ng memory with @p extension.
inline std::unique_ptr<Graph> make_graph(
    std::unique_ptr<ExtensionFunction<SizeTBitsetResource>> extension, const Sets& forbidden,
    double max_length) {
    auto graph = std::make_unique<Graph>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<RealResource>(
        std::make_unique<AdditionExtensionFunction<RealResource>>(),
        std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, max_length),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_resource<SizeTBitsetResource>(
        std::move(extension),
        std::make_unique<IntersectionFeasibilityFunction<SizeTBitsetResource>>(forbidden,
                                                                               /*forbidden=*/true),
        std::make_unique<TrivialCostFunction<SizeTBitsetResource>>(),
        std::make_unique<InclusionDominanceFunction<SizeTBitsetResource>>());
    return graph;
}

inline void add_arc(Graph* graph, size_t origin, size_t destination, double cost) {
    graph->add_arc<RealResource, RealResource, SizeTBitsetResource>(
        std::make_tuple(std::make_tuple(cost),
                        std::make_tuple(1.0),
                        std::make_tuple(std::set<size_t>{origin})),
        origin,
        destination,
        cost);
}

/// @brief The best cost a forward solve finds, or infinity.
inline double forward_optimum(Graph* graph) {
    const auto result = graph->solve<SimpleDominanceAlgorithm>(
        std::numeric_limits<double>::infinity(),
        AlgorithmParams<
            LabelList<ResourceTypeComposition<RealResource, RealResource, SizeTBitsetResource>>>{},
        /*preprocess=*/false);
    return result.solutions.empty() ? std::numeric_limits<double>::infinity()
                                    : result.solutions.front().cost;
}

}  // namespace ng_forward_test

/// @brief A forbidden set other than `{v}` now tests a memory already narrowed by the node
///        arrived at. Pinned as the documented behaviour.
///
/// forbidden(3) = {1}, ng(1) = {1, 2}, ng(2) = {2}. Node 1 is forgotten on arrival at 3, before
/// forbidden(3) is tested, so 0 1 3 at -10 is feasible. The old formula remembered 1 at 3 and
/// returned 0 2 3 at -1.
TEST(NgPath, ANonRouteForbiddenSetSeesTheNarrowedMemory) {
    namespace nf = ng_forward_test;
    auto graph = nf::make_graph(std::make_unique<NgPathExtensionFunction<SizeTBitsetResource>>(
                                    nf::Sets{{1, {1, 2}}, {2, {2}}}),
                                nf::Sets{{3, {1}}},
                                100.0);
    for (size_t node_id = 0; node_id < 4; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id == 3);
    }
    nf::add_arc(graph.get(), 0, 1, -5.0);
    nf::add_arc(graph.get(), 1, 3, -5.0);
    nf::add_arc(graph.get(), 0, 2, -0.5);
    nf::add_arc(graph.get(), 2, 3, -0.5);
    EXPECT_DOUBLE_EQ(nf::forward_optimum(graph.get()), -10.0);
}

/// @brief Under the ng-route condition with arcs carrying `{origin}`, the rewrite keeps every
///        forward optimum.
///
/// 300 random cyclic instances, each solved with the current `NgPathExtensionFunction` and with the
/// old formula. Paths may differ on ties; optima may not.
TEST(NgPath, TheRouteConditionKeepsEveryForwardOptimum) {
    namespace nf = ng_forward_test;
    constexpr size_t kNodes = 7;
    constexpr size_t kSink = kNodes - 1;
    constexpr int kInstances = 300;
    std::mt19937 rng(12345);
    const auto draw = [&](int low, int high) {
        return std::uniform_int_distribution<int>(low, high)(rng);
    };

    size_t with_solutions = 0;
    for (int instance = 0; instance < kInstances; ++instance) {
        nf::Sets neighborhoods;
        for (size_t node = 1; node < kSink; ++node) {
            std::set<size_t> neighborhood;
            for (int k = draw(0, 3); k > 0; --k) {
                const auto member = static_cast<size_t>(draw(1, kSink - 1));
                if (member != node) {
                    neighborhood.insert(member);
                }
            }
            neighborhoods[node] = neighborhood;
        }
        nf::Sets forbidden;
        for (size_t node = 0; node < kNodes; ++node) {
            forbidden[node] = {node};
        }
        std::vector<std::tuple<size_t, size_t, double>> arcs;
        for (size_t j = 1; j < kSink; ++j) {
            arcs.emplace_back(0, j, draw(-5, 5));
        }
        for (size_t i = 1; i < kSink; ++i) {
            for (size_t j = 1; j < kSink; ++j) {
                if (i != j && draw(0, 1) == 1) {
                    arcs.emplace_back(i, j, draw(-10, 5));
                }
            }
        }
        for (size_t i = 1; i < kSink; ++i) {
            arcs.emplace_back(i, kSink, draw(-5, 5));
        }
        arcs.emplace_back(0, kSink, 0.0);

        const auto solve = [&](std::unique_ptr<ExtensionFunction<SizeTBitsetResource>> extension) {
            auto graph = nf::make_graph(std::move(extension), forbidden, /*max_length=*/8.0);
            for (size_t node = 0; node < kNodes; ++node) {
                graph->add_node(node, node == 0, node == kSink);
            }
            for (const auto& [origin, destination, cost] : arcs) {
                nf::add_arc(graph.get(), origin, destination, cost);
            }
            return nf::forward_optimum(graph.get());
        };
        const double current =
            solve(std::make_unique<NgPathExtensionFunction<SizeTBitsetResource>>(neighborhoods));
        const double legacy = solve(std::make_unique<nf::LegacyNgPath>(neighborhoods));
        EXPECT_DOUBLE_EQ(current, legacy) << "instance " << instance;
        if (current < 0.0) {
            ++with_solutions;
        }
    }
    EXPECT_GT(with_solutions, 0U) << "no instance had a negative optimum, so ng never mattered";
}
