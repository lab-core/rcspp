// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/rcspp.hpp"

using namespace rcspp;

// ── helpers ───────────────────────────────────────────────────────────────────

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

// ── tests ─────────────────────────────────────────────────────────────────────

// Forcing arc 2 (1→2) removes arc 1 (0→2, the other in-arc of node 2) and
// nothing from node 1's out-arcs (arc 2 is its only outgoing arc).
bool test_force_arc_removes_competing_in_arc() {
    auto g = make_diamond();

    auto removed = g->force_arc(2);  // force arc 1→2

    if (removed.size() != 1 || !contains(removed, 1)) {
        LOG_ERROR("test_force_arc_removes_competing_in_arc: expected {1}, got size=",
                  removed.size(), "\n");
        return false;
    }
    // arc 2 must still be active
    if (g->get_arc(2) == nullptr) {
        LOG_ERROR("test_force_arc_removes_competing_in_arc: forced arc was removed\n");
        return false;
    }
    // arc 1 must be gone from active arcs
    if (g->get_arc(1) != nullptr) {
        LOG_ERROR("test_force_arc_removes_competing_in_arc: competing arc still active\n");
        return false;
    }
    // arc 0 (0→1) must still be active
    if (g->get_arc(0) == nullptr) {
        LOG_ERROR("test_force_arc_removes_competing_in_arc: unrelated arc was removed\n");
        return false;
    }
    // node 2 must have exactly one in-arc
    auto* n2 = g->get_node(2);
    if (n2->in_arcs.size() != 1 || n2->in_arcs[0]->id != 2) {
        LOG_ERROR("test_force_arc_removes_competing_in_arc: node 2 in_arcs wrong\n");
        return false;
    }
    return true;
}

// Forcing arc 0 (0→1) removes arc 1 (0→2, other out-arc of node 0) and
// nothing from node 1's in-arcs (arc 0 is its only incoming arc).
bool test_force_arc_removes_competing_out_arc() {
    auto g = make_diamond();

    auto removed = g->force_arc(0);  // force arc 0→1

    if (removed.size() != 1 || !contains(removed, 1)) {
        LOG_ERROR("test_force_arc_removes_competing_out_arc: expected {1}, got size=",
                  removed.size(), "\n");
        return false;
    }
    // node 0 must have exactly one out-arc
    auto* n0 = g->get_node(0);
    if (n0->out_arcs.size() != 1 || n0->out_arcs[0]->id != 0) {
        LOG_ERROR("test_force_arc_removes_competing_out_arc: node 0 out_arcs wrong\n");
        return false;
    }
    return true;
}

// Forcing arc 1 (0→2) removes arc 0 (0→1, other out-arc of node 0) AND
// arc 2 (1→2, other in-arc of node 2) — two arcs total.
bool test_force_arc_removes_both_sides() {
    auto g = make_diamond();

    auto removed = g->force_arc(1);  // force arc 0→2

    if (removed.size() != 2 || !contains(removed, 0) || !contains(removed, 2)) {
        LOG_ERROR("test_force_arc_removes_both_sides: expected {0,2}, got size=", removed.size(),
                  "\n");
        return false;
    }
    if (g->get_arc(0) != nullptr || g->get_arc(2) != nullptr) {
        LOG_ERROR("test_force_arc_removes_both_sides: competing arcs still active\n");
        return false;
    }
    if (g->get_arc(1) == nullptr) {
        LOG_ERROR("test_force_arc_removes_both_sides: forced arc was removed\n");
        return false;
    }
    return true;
}

// force_arc via Arc& overload produces the same result as via arc_id.
bool test_force_arc_by_arc_ref() {
    auto g = make_diamond();

    auto* arc = g->get_arc(1);
    auto removed = g->force_arc(*arc);

    if (removed.size() != 2 || !contains(removed, 0) || !contains(removed, 2)) {
        LOG_ERROR("test_force_arc_by_arc_ref: expected {0,2}, got size=", removed.size(), "\n");
        return false;
    }
    return true;
}

// Forcing a non-existent arc_id returns an empty vector without modifying the graph.
bool test_force_arc_nonexistent() {
    auto g = make_diamond();

    auto removed = g->force_arc(99);

    if (!removed.empty()) {
        LOG_ERROR("test_force_arc_nonexistent: expected empty, got size=", removed.size(), "\n");
        return false;
    }
    if (g->get_number_of_arcs() != 3) {
        LOG_ERROR("test_force_arc_nonexistent: graph was modified\n");
        return false;
    }
    return true;
}

// Forcing the only arc on both sides returns an empty vector (nothing to remove).
bool test_force_arc_already_unique() {
    auto g = std::make_unique<Graph<RealResource>>();
    g->add_node(0, /*source=*/true);
    g->add_node(1, /*sink=*/true);
    g->add_arc(0, 1);  // id=0

    auto removed = g->force_arc(0);

    if (!removed.empty()) {
        LOG_ERROR("test_force_arc_already_unique: expected empty, got size=", removed.size(), "\n");
        return false;
    }
    if (g->get_arc(0) == nullptr) {
        LOG_ERROR("test_force_arc_already_unique: sole arc was removed\n");
        return false;
    }
    return true;
}

// Parallel arcs (same origin→destination): forcing one of them deduplicates the
// other so it only appears once in the removed list.
bool test_force_arc_parallel_dedup() {
    auto g = std::make_unique<Graph<RealResource>>();
    g->add_node(0, /*source=*/true);
    g->add_node(1, /*sink=*/true);
    g->add_arc(0, 1);  // id=0
    g->add_arc(0, 1);  // id=1  (parallel)

    // Forcing arc 0: the other parallel arc 1 appears in both origin's out_arcs
    // and destination's in_arcs — should only be listed once.
    auto removed = g->force_arc(0);

    if (removed.size() != 1 || !contains(removed, 1)) {
        LOG_ERROR("test_force_arc_parallel_dedup: expected {1}, got size=", removed.size(), "\n");
        return false;
    }
    return true;
}

// ── runner ────────────────────────────────────────────────────────────────────

std::pair<int, int> all_tests_graph() {
    int passed = 0;
    int total = 0;

    auto run = [&](bool (*fn)(), const char* name) {
        ++total;
        if (fn()) {
            ++passed;
        } else {
            LOG_ERROR("FAILED: ", name, "\n");
        }
    };

    run(test_force_arc_removes_competing_in_arc, "test_force_arc_removes_competing_in_arc");
    run(test_force_arc_removes_competing_out_arc, "test_force_arc_removes_competing_out_arc");
    run(test_force_arc_removes_both_sides, "test_force_arc_removes_both_sides");
    run(test_force_arc_by_arc_ref, "test_force_arc_by_arc_ref");
    run(test_force_arc_nonexistent, "test_force_arc_nonexistent");
    run(test_force_arc_already_unique, "test_force_arc_already_unique");
    run(test_force_arc_parallel_dedup, "test_force_arc_parallel_dedup");

    return {passed, total};
}