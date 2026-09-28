// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// LabelFrontier: arrival order for an exact search, a node-by-node sweep that keeps each node's
// cheapest labels for a search with a per-node quota.

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace label_frontier_test {

using Composed = ResourceTypeComposition<RealResource>;

/// @brief A line of @p nodes nodes whose only resource is the cost, sorted by id.
inline std::unique_ptr<ResourceGraph<RealResource>> line(size_t nodes) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    presets::add_cost_resource<RealResource>(*graph);
    for (size_t id = 0; id < nodes; ++id) {
        graph->add_node(id, id == 0, id + 1 == nodes);
    }
    for (size_t id = 0; id + 1 < nodes; ++id) {
        graph->add_arc<RealResource>(std::make_tuple(1.0), id, id + 1, 1.0);
    }
    graph->sort_nodes();
    return graph;
}

/// @brief Builds labels at given nodes with given costs, and reads a frontier back as costs.
class Fixture {
    public:
        explicit Fixture(size_t nodes)
            : graph_(line(nodes)),
              pool_(std::make_unique<LabelFactory<Composed>>(&graph_->get_resource_factory())) {}

        LabelIteratorPair<Composed> label(size_t node_id, double cost) {
            auto& made = pool_.get_next_label(graph_->get_node(node_id));
            made.get_resource().get_component<RealResource>(0).set_value(cost);
            return {&made, {}};
        }

        std::vector<double> drain(LabelFrontier<Composed>* frontier, size_t quota) {
            std::vector<double> costs;
            while (!frontier->empty()) {
                costs.push_back(frontier->pop(quota, &pool_).first->get_cost());
            }
            return costs;
        }

        [[nodiscard]] size_t nodes() const { return graph_->get_number_of_nodes(); }

    private:
        std::unique_ptr<ResourceGraph<RealResource>> graph_;
        LabelPool<Composed> pool_;
};

}  // namespace label_frontier_test

TEST(LabelFrontier, FifoKeepsArrivalOrderAndIgnoresTheQuota) {
    namespace lf = label_frontier_test;
    lf::Fixture fixture(5);
    LabelFrontier<lf::Composed> frontier;
    frontier.reset_fifo();
    for (const auto& [node, cost] :
         std::vector<std::pair<size_t, double>>{{3, 5.0}, {1, 9.0}, {3, 1.0}, {3, 3.0}}) {
        frontier.push(fixture.label(node, cost));
    }
    EXPECT_EQ(frontier.size(), 4U);
    EXPECT_FALSE(frontier.sweeping());
    EXPECT_EQ(fixture.drain(&frontier, 1), (std::vector<double>{5.0, 9.0, 1.0, 3.0}));
}

TEST(LabelFrontier, ASweepVisitsNodesInOrderAndKeepsEachNodesCheapest) {
    namespace lf = label_frontier_test;
    lf::Fixture fixture(5);
    LabelFrontier<lf::Composed> frontier;
    frontier.reset_sweep(fixture.nodes(), /*descending=*/false);
    for (const auto& [node, cost] :
         std::vector<std::pair<size_t, double>>{{3, 5.0}, {1, 9.0}, {3, 1.0}, {3, 3.0}}) {
        frontier.push(fixture.label(node, cost));
    }
    EXPECT_TRUE(frontier.sweeping());
    // Node 1 first, then node 3 trimmed to its two cheapest: the label costing 5 is never taken.
    EXPECT_EQ(fixture.drain(&frontier, 2), (std::vector<double>{9.0, 1.0, 3.0}));
    EXPECT_TRUE(frontier.empty());
}

TEST(LabelFrontier, ADescendingSweepStartsFromTheLastNode) {
    namespace lf = label_frontier_test;
    lf::Fixture fixture(5);
    LabelFrontier<lf::Composed> frontier;
    frontier.reset_sweep(fixture.nodes(), /*descending=*/true);
    for (const auto& [node, cost] :
         std::vector<std::pair<size_t, double>>{{1, 9.0}, {3, 5.0}, {3, 1.0}}) {
        frontier.push(fixture.label(node, cost));
    }
    EXPECT_EQ(fixture.drain(&frontier, std::numeric_limits<size_t>::max()),
              (std::vector<double>{5.0, 1.0, 9.0}));
}

TEST(LabelFrontier, ASweepWrapsRoundForLabelsBehindIt) {
    namespace lf = label_frontier_test;
    lf::Fixture fixture(5);
    LabelFrontier<lf::Composed> frontier;
    frontier.reset_sweep(fixture.nodes(), /*descending=*/false);
    frontier.push(fixture.label(3, 2.0));
    EXPECT_EQ(frontier.pop(10, nullptr).first->get_cost(), 2.0);
    // Pushed while the sweep is past node 1: taken on the next round.
    frontier.push(fixture.label(1, 7.0));
    EXPECT_EQ(fixture.drain(&frontier, 10), (std::vector<double>{7.0}));
}
