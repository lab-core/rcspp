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

        [[nodiscard]] LabelPool<Composed>& pool() { return pool_; }

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

/// @brief A trim that empties the frontier ends the pop with a null label.
///
/// With a quota of 0 every trim drops its whole queue, and `pop` used to go on sweeping an empty
/// frontier forever.
TEST(LabelFrontier, ASweepWithAZeroQuotaReturnsANullLabel) {
    namespace lf = label_frontier_test;
    lf::Fixture fixture(5);
    LabelFrontier<lf::Composed> frontier;
    frontier.reset_sweep(fixture.nodes(), /*descending=*/false);
    frontier.push(fixture.label(1, 9.0));
    frontier.push(fixture.label(3, 5.0));
    EXPECT_EQ(frontier.pop(0, nullptr).first, nullptr);
    EXPECT_TRUE(frontier.empty());
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

TEST(LabelFrontier, ClockOrderPopsTheSmallestClockFirst) {
    namespace lf = label_frontier_test;
    lf::Fixture fixture(5);
    LabelFrontier<lf::Composed> frontier;
    frontier.reset_clock_order(/*descending=*/false);
    frontier.push(fixture.label(1, 50.0), 5.0);
    frontier.push(fixture.label(2, 10.0), 1.0);
    frontier.push(fixture.label(3, 30.0), 3.0);
    EXPECT_TRUE(frontier.clock_ordered());
    EXPECT_EQ(frontier.size(), 3U);
    EXPECT_EQ(frontier.front_key(), 1.0);
    EXPECT_EQ(fixture.drain(&frontier, 0), (std::vector<double>{10.0, 30.0, 50.0}));
}

TEST(LabelFrontier, DescendingClockOrderPopsTheLargestClockFirst) {
    namespace lf = label_frontier_test;
    lf::Fixture fixture(5);
    LabelFrontier<lf::Composed> frontier;
    frontier.reset_clock_order(/*descending=*/true);
    frontier.push(fixture.label(1, 50.0), 5.0);
    frontier.push(fixture.label(2, 10.0), 1.0);
    frontier.push(fixture.label(3, 30.0), 3.0);
    EXPECT_EQ(frontier.front_key(), 5.0);
    EXPECT_EQ(fixture.drain(&frontier, 0), (std::vector<double>{50.0, 30.0, 10.0}));
}

TEST(LabelFrontier, ClockTiesPopInInsertionOrder) {
    namespace lf = label_frontier_test;
    lf::Fixture fixture(5);
    for (const bool descending : {false, true}) {
        LabelFrontier<lf::Composed> frontier;
        frontier.reset_clock_order(descending);
        frontier.push(fixture.label(1, 10.0), 2.0);
        frontier.push(fixture.label(2, 20.0), 2.0);
        frontier.push(fixture.label(3, 30.0), 2.0);
        EXPECT_EQ(fixture.drain(&frontier, 0), (std::vector<double>{10.0, 20.0, 30.0}))
            << "descending=" << descending;
    }
}

TEST(LabelFrontier, FrontKeyTracksEveryPop) {
    namespace lf = label_frontier_test;
    lf::Fixture fixture(5);
    LabelFrontier<lf::Composed> frontier;
    frontier.reset_clock_order(/*descending=*/false);
    for (const double key : {4.0, 2.0, 8.0, 6.0}) {
        frontier.push(fixture.label(1, key), key);
    }
    std::vector<double> fronts;
    while (!frontier.empty()) {
        fronts.push_back(frontier.front_key());
        static_cast<void>(frontier.pop(0, &fixture.pool()));
    }
    EXPECT_EQ(fronts, (std::vector<double>{2.0, 4.0, 6.0, 8.0}));
}

TEST(LabelFrontier, SheddingInClockOrderKeepsTheCheapestAndTheOrder) {
    namespace lf = label_frontier_test;
    lf::Fixture fixture(3);
    LabelFrontier<lf::Composed> frontier;
    frontier.reset_clock_order(/*descending=*/false);
    // Clock 1..6 with the cost falling as the clock rises: the three cheapest are the latest.
    for (int i = 1; i <= 6; ++i) {
        frontier.push(fixture.label(1, 100.0 - i), static_cast<double>(i));
    }
    frontier.shed(/*per_node=*/1, /*num_nodes=*/3, &fixture.pool());
    EXPECT_EQ(frontier.size(), 3U);
    // Clocks 4, 5, 6 survive, still popped in clock order.
    EXPECT_EQ(fixture.drain(&frontier, 0), (std::vector<double>{96.0, 95.0, 94.0}));
}

TEST(LabelFrontier, DiscardEmptiesEveryMode) {
    namespace lf = label_frontier_test;
    lf::Fixture fixture(5);
    LabelFrontier<lf::Composed> clock;
    clock.reset_clock_order(/*descending=*/false);
    clock.push(fixture.label(1, 1.0), 1.0);
    clock.push(fixture.label(2, 2.0), 2.0);
    clock.discard(&fixture.pool());
    EXPECT_TRUE(clock.empty());
    EXPECT_EQ(clock.size(), 0U);

    LabelFrontier<lf::Composed> fifo;
    fifo.reset_fifo();
    fifo.push(fixture.label(1, 1.0));
    fifo.discard(&fixture.pool());
    EXPECT_TRUE(fifo.empty());
}

TEST(LabelFrontier, ResettingLeavesClockOrder) {
    namespace lf = label_frontier_test;
    LabelFrontier<lf::Composed> frontier;
    frontier.reset_clock_order(/*descending=*/false);
    frontier.reset_fifo();
    EXPECT_FALSE(frontier.clock_ordered());
    frontier.reset_clock_order(/*descending=*/true);
    frontier.reset_sweep(/*num_nodes=*/3, /*descending=*/false);
    EXPECT_FALSE(frontier.clock_ordered());
    EXPECT_TRUE(frontier.sweeping());
}

TEST(LabelFrontier, AKeyIsIgnoredOutsideClockOrder) {
    namespace lf = label_frontier_test;
    lf::Fixture fixture(5);
    LabelFrontier<lf::Composed> frontier;
    frontier.reset_fifo();
    frontier.push(fixture.label(1, 5.0), 9.0);
    frontier.push(fixture.label(2, 1.0), 0.0);
    EXPECT_EQ(fixture.drain(&frontier, 0), (std::vector<double>{5.0, 1.0}))
        << "FIFO keeps arrival order whatever the keys";
}
