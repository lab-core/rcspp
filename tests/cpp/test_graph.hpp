// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <gtest/gtest.h>

#include "rcspp/rcspp.hpp"

using namespace rcspp;

namespace {

// Build a simple graph:   0 --a0--> 1 --a2--> 2
//                         0 --a1--> 2
// Arc ids: a0=0, a1=1, a2=2
std::unique_ptr<Graph<RealResource>> make_diamond() {
    auto g = std::make_unique<Graph<RealResource>>();
    g->add_node(0, /*source=*/true);
    g->add_node(1);
    g->add_node(2, /*sink=*/true);
    g->add_arc(0, 1);  // id=0
    g->add_arc(0, 2);  // id=1
    g->add_arc(1, 2);  // id=2
    return g;
}

bool contains(const std::vector<size_t>& v, size_t val) {
    return std::ranges::find(v, val) != v.end();
}

}  // namespace

// Forcing arc 2 (1→2) removes arc 1 (0→2, the other in-arc of node 2) and
// nothing from node 1's out-arcs (arc 2 is its only outgoing arc).
TEST(Graph, ForceArcRemovesCompetingInArc) {
    auto g = make_diamond();
    auto removed = g->force_arc(2);
    ASSERT_EQ(removed.size(), 1u);
    EXPECT_TRUE(contains(removed, 1u));
    EXPECT_NE(g->get_arc(2), nullptr);
    EXPECT_EQ(g->get_arc(1), nullptr);
    EXPECT_NE(g->get_arc(0), nullptr);
    auto* n2 = g->get_node(2);
    ASSERT_EQ(n2->in_arcs.size(), 1u);
    EXPECT_EQ(n2->in_arcs[0]->id, 2u);
}

// Forcing arc 0 (0→1) removes arc 1 (0→2, other out-arc of node 0) and
// nothing from node 1's in-arcs (arc 0 is its only incoming arc).
TEST(Graph, ForceArcRemovesCompetingOutArc) {
    auto g = make_diamond();
    auto removed = g->force_arc(0);
    ASSERT_EQ(removed.size(), 1u);
    EXPECT_TRUE(contains(removed, 1u));
    auto* n0 = g->get_node(0);
    ASSERT_EQ(n0->out_arcs.size(), 1u);
    EXPECT_EQ(n0->out_arcs[0]->id, 0u);
}

// Forcing arc 1 (0→2) removes arc 0 (0→1, other out-arc of node 0) AND
// arc 2 (1→2, other in-arc of node 2) — two arcs total.
TEST(Graph, ForceArcRemovesBothSides) {
    auto g = make_diamond();
    auto removed = g->force_arc(1);
    ASSERT_EQ(removed.size(), 2u);
    EXPECT_TRUE(contains(removed, 0u));
    EXPECT_TRUE(contains(removed, 2u));
    EXPECT_EQ(g->get_arc(0), nullptr);
    EXPECT_EQ(g->get_arc(2), nullptr);
    EXPECT_NE(g->get_arc(1), nullptr);
}

// force_arc via Arc& overload produces the same result as via arc_id.
TEST(Graph, ForceArcByArcRef) {
    auto g = make_diamond();
    auto* arc = g->get_arc(1);
    ASSERT_NE(arc, nullptr);
    auto removed = g->force_arc(*arc);
    ASSERT_EQ(removed.size(), 2u);
    EXPECT_TRUE(contains(removed, 0u));
    EXPECT_TRUE(contains(removed, 2u));
}

// Forcing a non-existent arc_id returns an empty vector without modifying the graph.
TEST(Graph, ForceArcNonexistent) {
    auto g = make_diamond();
    auto removed = g->force_arc(99);
    EXPECT_TRUE(removed.empty());
    EXPECT_EQ(g->get_number_of_arcs(), 3u);
}

// Forcing the only arc on both sides returns an empty vector (nothing to remove).
TEST(Graph, ForceArcAlreadyUnique) {
    auto g = std::make_unique<Graph<RealResource>>();
    g->add_node(0, /*source=*/true);
    g->add_node(1, /*sink=*/true);
    g->add_arc(0, 1);
    auto removed = g->force_arc(0);
    EXPECT_TRUE(removed.empty());
    EXPECT_NE(g->get_arc(0), nullptr);
}

// Parallel arcs (same origin→destination): forcing one deduplicates the other
// so it only appears once in the removed list.
TEST(Graph, ForceArcParallelDedup) {
    auto g = std::make_unique<Graph<RealResource>>();
    g->add_node(0, /*source=*/true);
    g->add_node(1, /*sink=*/true);
    g->add_arc(0, 1);  // id=0
    g->add_arc(0, 1);  // id=1 (parallel)
    auto removed = g->force_arc(0);
    ASSERT_EQ(removed.size(), 1u);
    EXPECT_TRUE(contains(removed, 1u));
}
