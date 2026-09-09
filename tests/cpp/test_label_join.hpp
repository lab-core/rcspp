// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Phase 10: the joiner.
//
// The label sets here are built BY HAND rather than by running a search. That is what makes the
// joiner testable independently of the two frontiers, and what makes a failure here unambiguous.
//
// CrossingRuleJoinsEachPathExactlyOnce is the acceptance test for the arc rule. A four-arc path
// with labels at every node must produce ONE solution, not four. If it produces four, the crossing
// rule was written with b.critical(v) on the right instead of f'.critical(v) -- a form that filters
// nothing, because the backward search already keeps only labels at or above H and a surviving
// forward label is already at or below it.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace label_join_test {

using Composed = ResourceTypeComposition<RealResource>;
using ForwardList = LabelList<Composed>;
using BackwardList = LabelList<Composed, BackwardDirection>;

constexpr double kTolerance = 1e-9;
constexpr double kInfinity = std::numeric_limits<double>::infinity();

/// @brief One recorded join: what the callback was handed.
struct Recorded {
        double cost;
        std::vector<size_t> arc_ids;
        size_t end_node_id;
};

/// @brief A linear graph s(0) -> 1 -> 2 -> ... -> n, one component: a monotone clock.
///
/// The clock is a plain additive resource, so it is monotone and the half-way rule applies.
inline std::unique_ptr<ResourceGraph<RealResource>> line_graph(
    const std::vector<double>& arc_values) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());

    const size_t num_nodes = arc_values.size() + 1;
    for (size_t node_id = 0; node_id < num_nodes; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id + 1 == num_nodes);
    }
    for (size_t i = 0; i < arc_values.size(); ++i) {
        graph->add_arc<RealResource>(std::make_tuple(arc_values[i]), i, i + 1, arc_values[i]);
    }
    graph->sort_nodes();
    graph->build_csr();
    return graph;
}

/// @brief Harness owning a pool and the two per-node container sets.
class JoinHarness {
    public:
        explicit JoinHarness(ResourceGraph<RealResource>* graph)
            : graph_(graph),
              pool_(std::make_unique<LabelFactory<Composed>>(&graph->get_resource_factory())) {
            forward_.resize(graph->get_number_of_nodes());
            backward_.resize(graph->get_number_of_nodes());
        }

        /// @brief Adds a forward label at @p node_id, chained onto @p previous when given.
        Label<Composed>* add_forward(size_t node_id, const Arc<Composed>* in_arc,
                                     Label<Composed>* previous) {
            auto* node = graph_->get_node(node_id);
            auto& label = pool_.get_next_label(node, in_arc, nullptr);
            if (previous != nullptr) {
                label.set_prev_label(previous);
            }
            forward_.at(node->pos()).add_label(&label);
            return &label;
        }

        /// @brief Adds a backward label at @p node_id, chained onto @p previous when given.
        ///
        /// A backward label's `prev_label` is the label nearer the SINK, and the arc it remembers
        /// is its out-arc.
        Label<Composed>* add_backward(size_t node_id, const Arc<Composed>* out_arc,
                                      Label<Composed>* previous) {
            auto* node = graph_->get_node(node_id);
            auto& label = pool_.get_next_label(node, nullptr, out_arc);
            if (previous != nullptr) {
                label.set_prev_label(previous);
            }
            backward_.at(node->pos()).add_label(&label);
            return &label;
        }

        /// @brief Sets a label's single component value, which is both its clock and its cost.
        static void set_value(Label<Composed>* label, double value) {
            auto& component = label->get_resource().template get_component<RealResource>(0);
            component.set_value(value);
        }

        /// @brief Runs the joiner, recording every solution offered.
        std::vector<Recorded> run(const HalfWayPolicy& policy, double upper_bound = kInfinity) {
            std::vector<Recorded> recorded;
            Joiner<Composed, RealResource> joiner;
            double best = upper_bound;
            joiner.join(*graph_,
                        forward_,
                        backward_,
                        pool_,
                        policy,
                        /*critical_resource_index=*/0,
                        best,
                        [&](double cost, std::vector<size_t> arc_ids, size_t end_node_id) {
                            recorded.push_back({cost, std::move(arc_ids), end_node_id});
                        });
            return recorded;
        }

        [[nodiscard]] LabelPool<Composed>& pool() { return pool_; }

    private:
        ResourceGraph<RealResource>* graph_;
        LabelPool<Composed> pool_;
        std::vector<ForwardList> forward_;
        std::vector<BackwardList> backward_;
};

/// @brief A feasibility function that counts how often can_be_merged is asked.
class CountingFeasibilityFunction
    : public Clonable<CountingFeasibilityFunction, FeasibilityFunction<RealResource>> {
    public:
        explicit CountingFeasibilityFunction(size_t* counter) : counter_(counter) {}

        [[nodiscard]] auto is_feasible(const RealResource& /*resource*/) -> bool override {
            return true;
        }

        [[nodiscard]] auto can_be_merged(const RealResource& /*resource*/,
                                         const RealResource& /*back_resource*/) -> bool override {
            if (counter_ != nullptr) {
                ++(*counter_);
            }
            return true;
        }

        [[nodiscard]] MergeRule merge_rule() const override { return MergeRule::Custom; }

    private:
        size_t* counter_ = nullptr;
};

/// @brief A feasibility function that is locally feasible but never mergeable.
///
/// Stands in for the real case -- two halves that both visit the same node -- without needing a
/// container resource, so the rejection under test is unambiguously the merge step.
class UnmergeableFeasibilityFunction
    : public Clonable<UnmergeableFeasibilityFunction, FeasibilityFunction<RealResource>> {
    public:
        [[nodiscard]] auto is_feasible(const RealResource& /*resource*/) -> bool override {
            return true;
        }

        [[nodiscard]] auto can_be_merged(const RealResource& /*resource*/,
                                         const RealResource& /*back_resource*/) -> bool override {
            return false;
        }

        [[nodiscard]] MergeRule merge_rule() const override { return MergeRule::Custom; }
};

/// @brief A four-node line whose single component is a time window.
inline std::unique_ptr<ResourceGraph<RealResource>> time_window_line(
    const std::map<size_t, std::pair<double, double>>& windows, double arc_time) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        std::make_unique<ValueCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    for (size_t node_id = 0; node_id < 4; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id == 3);
    }
    for (size_t i = 0; i < 3; ++i) {
        graph->add_arc<RealResource>(std::make_tuple(arc_time), i, i + 1, arc_time);
    }
    graph->sort_nodes();
    graph->build_csr();
    return graph;
}

}  // namespace label_join_test

// ============================================================================
// A simple join
// ============================================================================

/// @brief A forward half and a backward half splice into the expected arc sequence and cost.
TEST(LabelJoin, SimpleJoinProducesExpectedPath) {
    namespace ljt = label_join_test;
    // s(0) -1-> 1 -2-> 2 -3-> 3, clock crosses H = 2 on arc 1->2.
    auto graph = ljt::line_graph({1.0, 2.0, 3.0});
    ljt::JoinHarness harness(graph.get());

    // Forward chain s -> 1, sitting at node 1 with clock 1.
    auto* at_source = harness.add_forward(0, nullptr, nullptr);
    ljt::JoinHarness::set_value(at_source, 0.0);
    auto* forward = harness.add_forward(1, graph->get_arc(0), at_source);
    ljt::JoinHarness::set_value(forward, 1.0);

    // Backward chain t <- 2, sitting at node 2. prev_label is the label nearer the SINK.
    auto* at_sink = harness.add_backward(3, nullptr, nullptr);
    ljt::JoinHarness::set_value(at_sink, 0.0);
    auto* backward = harness.add_backward(2, graph->get_arc(2), at_sink);
    ljt::JoinHarness::set_value(backward, 3.0);

    const HalfWayPolicy policy(/*half_way_point=*/2.0, /*resource_upper_bound=*/6.0);
    const auto recorded = harness.run(policy);

    ASSERT_EQ(recorded.size(), 1U);
    // All three arcs, in path order: s->1, 1->2, 2->3.
    EXPECT_EQ(recorded[0].arc_ids, (std::vector<size_t>{0, 1, 2}));
    EXPECT_EQ(recorded[0].end_node_id, 3U);
}

/// @brief The join arc is counted exactly once.
///
/// f' has consumed it; a backward label sitting *at* v has consumed no arc into v.
TEST(LabelJoin, JoinArcCountedExactlyOnce) {
    namespace ljt = label_join_test;
    auto graph = ljt::line_graph({1.0, 2.0, 3.0});
    ljt::JoinHarness harness(graph.get());

    auto* at_source = harness.add_forward(0, nullptr, nullptr);
    ljt::JoinHarness::set_value(at_source, 0.0);
    auto* forward = harness.add_forward(1, graph->get_arc(0), at_source);
    ljt::JoinHarness::set_value(forward, 1.0);

    auto* at_sink = harness.add_backward(3, nullptr, nullptr);
    ljt::JoinHarness::set_value(at_sink, 0.0);
    auto* backward = harness.add_backward(2, graph->get_arc(2), at_sink);
    ljt::JoinHarness::set_value(backward, 3.0);

    const HalfWayPolicy policy(2.0, 6.0);
    const auto recorded = harness.run(policy);

    ASSERT_EQ(recorded.size(), 1U);
    // f' = 1 (forward) + 2 (join arc) = 3; b = 3. Total 6 = 1 + 2 + 3, each arc once.
    EXPECT_NEAR(recorded[0].cost, 6.0, ljt::kTolerance);
}

// ============================================================================
// THE acceptance test for the arc rule
// ============================================================================

/// @brief A four-arc path whose clock crosses H once yields exactly ONE solution.
///
/// Labels sit at every node in both directions, so every arc is a candidate join arc. Only the arc
/// on which the clock actually straddles H may produce a solution. Four solutions here would mean
/// the crossing rule was written with the backward value on the right.
TEST(LabelJoin, CrossingRuleJoinsEachPathExactlyOnce) {
    namespace ljt = label_join_test;
    // Clock: 0, 1, 2, 3, 4 at nodes 0..4. With H = 2.5 the crossing arc is 2->3.
    auto graph = ljt::line_graph({1.0, 1.0, 1.0, 1.0});
    ljt::JoinHarness harness(graph.get());

    // A forward label at every node, chained, clock = node index.
    std::vector<Label<ljt::Composed>*> forward_chain;
    Label<ljt::Composed>* previous = nullptr;
    for (size_t node_id = 0; node_id <= 4; ++node_id) {
        const Arc<ljt::Composed>* in_arc = node_id == 0 ? nullptr : graph->get_arc(node_id - 1);
        previous = harness.add_forward(node_id, in_arc, previous);
        ljt::JoinHarness::set_value(previous, static_cast<double>(node_id));
        forward_chain.push_back(previous);
    }

    // A backward label at every node, chained from the sink, clock = 4 - node index.
    Label<ljt::Composed>* backward_previous = nullptr;
    for (size_t step = 0; step <= 4; ++step) {
        const size_t node_id = 4 - step;
        const Arc<ljt::Composed>* out_arc = node_id == 4 ? nullptr : graph->get_arc(node_id);
        backward_previous = harness.add_backward(node_id, out_arc, backward_previous);
        ljt::JoinHarness::set_value(backward_previous, static_cast<double>(4 - node_id));
    }

    const HalfWayPolicy policy(/*half_way_point=*/2.5, /*resource_upper_bound=*/8.0);
    const auto recorded = harness.run(policy);

    // Exactly one -- not four.
    ASSERT_EQ(recorded.size(), 1U) << "the crossing rule must admit exactly one join per path";
    EXPECT_EQ(recorded[0].arc_ids, (std::vector<size_t>{0, 1, 2, 3}));
    EXPECT_EQ(recorded[0].end_node_id, 4U);
}

// ============================================================================
// Each rejection step, for its own reason
// ============================================================================

/// @brief Step 2: the clock does not straddle H, so there is no join on this arc.
TEST(LabelJoin, CrossingRuleRejectsWhenClockDoesNotStraddleH) {
    namespace ljt = label_join_test;
    auto graph = ljt::line_graph({1.0, 2.0, 3.0});
    ljt::JoinHarness harness(graph.get());

    auto* at_source = harness.add_forward(0, nullptr, nullptr);
    ljt::JoinHarness::set_value(at_source, 0.0);
    auto* forward = harness.add_forward(1, graph->get_arc(0), at_source);
    ljt::JoinHarness::set_value(forward, 1.0);

    auto* at_sink = harness.add_backward(3, nullptr, nullptr);
    ljt::JoinHarness::set_value(at_sink, 0.0);
    auto* backward = harness.add_backward(2, graph->get_arc(2), at_sink);
    ljt::JoinHarness::set_value(backward, 3.0);

    // H = 20: f'(v) = 3 never exceeds it, so the clock does not cross here.
    const HalfWayPolicy policy(/*half_way_point=*/20.0, /*resource_upper_bound=*/100.0);
    EXPECT_TRUE(harness.run(policy).empty());
}

/// @brief Step 3: f' is locally infeasible at v, so there is no join.
///
/// This is the step that catches "v is already in the forward half's visited set", with no
/// special-casing -- it is just the ordinary feasibility check applied at v after the extension.
/// Here the same mechanism is driven with a time window instead, which is easier to state exactly.
TEST(LabelJoin, FeasibilityRejectsWhenExtendedLabelViolatesTheNodeWindow) {
    namespace ljt = label_join_test;
    // Node 2 closes at 5, but reaching it costs 10 from a forward label at time 0.
    std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 1000.0}},
                                                        {1, {0.0, 1000.0}},
                                                        {2, {0.0, 5.0}},
                                                        {3, {0.0, 1000.0}}};
    auto graph = ljt::time_window_line(windows, /*arc_time=*/10.0);
    ljt::JoinHarness harness(graph.get());

    auto* at_source = harness.add_forward(0, nullptr, nullptr);
    ljt::JoinHarness::set_value(at_source, 0.0);
    auto* forward = harness.add_forward(1, graph->get_arc(0), at_source);
    ljt::JoinHarness::set_value(forward, 0.0);

    auto* at_sink = harness.add_backward(3, nullptr, nullptr);
    ljt::JoinHarness::set_value(at_sink, 0.0);
    auto* backward = harness.add_backward(2, graph->get_arc(2), at_sink);
    ljt::JoinHarness::set_value(backward, 0.0);

    // H = 5: the clock does straddle it (0 <= 5 < 10), so the crossing test passes and the
    // rejection under test really is the feasibility one.
    const HalfWayPolicy policy(/*half_way_point=*/5.0, /*resource_upper_bound=*/1000.0);
    EXPECT_TRUE(harness.run(policy).empty());
}

/// @brief Step 4: the two halves cannot be merged, so there is no join.
TEST(LabelJoin, MergeTestRejectsIncompatibleHalves) {
    namespace ljt = label_join_test;
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<ljt::UnmergeableFeasibilityFunction>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    for (size_t node_id = 0; node_id < 4; ++node_id) {
        graph->add_node(node_id, node_id == 0, node_id == 3);
    }
    graph->add_arc<RealResource>(std::make_tuple(1.0), 0, 1, 1.0);
    graph->add_arc<RealResource>(std::make_tuple(2.0), 1, 2, 2.0);
    graph->add_arc<RealResource>(std::make_tuple(3.0), 2, 3, 3.0);
    graph->sort_nodes();
    graph->build_csr();

    ljt::JoinHarness harness(graph.get());
    auto* at_source = harness.add_forward(0, nullptr, nullptr);
    ljt::JoinHarness::set_value(at_source, 0.0);
    auto* forward = harness.add_forward(1, graph->get_arc(0), at_source);
    ljt::JoinHarness::set_value(forward, 1.0);
    auto* at_sink = harness.add_backward(3, nullptr, nullptr);
    ljt::JoinHarness::set_value(at_sink, 0.0);
    auto* backward = harness.add_backward(2, graph->get_arc(2), at_sink);
    ljt::JoinHarness::set_value(backward, 3.0);

    // Crossing and feasibility both pass; only the merge test refuses.
    const HalfWayPolicy policy(2.0, 6.0);
    EXPECT_TRUE(harness.run(policy).empty());
}

/// @brief Step 5: a cost at or above the incumbent is rejected.
TEST(LabelJoin, UpperBoundRejectsExpensiveJoins) {
    namespace ljt = label_join_test;
    auto graph = ljt::line_graph({1.0, 2.0, 3.0});
    ljt::JoinHarness harness(graph.get());

    auto* at_source = harness.add_forward(0, nullptr, nullptr);
    ljt::JoinHarness::set_value(at_source, 0.0);
    auto* forward = harness.add_forward(1, graph->get_arc(0), at_source);
    ljt::JoinHarness::set_value(forward, 1.0);

    auto* at_sink = harness.add_backward(3, nullptr, nullptr);
    ljt::JoinHarness::set_value(at_sink, 0.0);
    auto* backward = harness.add_backward(2, graph->get_arc(2), at_sink);
    ljt::JoinHarness::set_value(backward, 3.0);

    const HalfWayPolicy policy(2.0, 6.0);
    // The join costs 6; an incumbent of 6 must reject it, and one of 7 must accept.
    EXPECT_TRUE(harness.run(policy, /*upper_bound=*/6.0).empty());

    ljt::JoinHarness accepting(graph.get());
    auto* s2 = accepting.add_forward(0, nullptr, nullptr);
    ljt::JoinHarness::set_value(s2, 0.0);
    auto* f2 = accepting.add_forward(1, graph->get_arc(0), s2);
    ljt::JoinHarness::set_value(f2, 1.0);
    auto* t2 = accepting.add_backward(3, nullptr, nullptr);
    ljt::JoinHarness::set_value(t2, 0.0);
    auto* b2 = accepting.add_backward(2, graph->get_arc(2), t2);
    ljt::JoinHarness::set_value(b2, 3.0);
    EXPECT_EQ(accepting.run(policy, /*upper_bound=*/7.0).size(), 1U);
}

/// @brief An infinite cost is dropped rather than becoming a "solution".
///
/// Forbidden arcs carry an infinite cost -- the VRP example marks depot-to-depot that way.
TEST(LabelJoin, InfiniteCostsAreDropped) {
    namespace ljt = label_join_test;
    auto graph = ljt::line_graph({1.0, 2.0, 3.0});
    ljt::JoinHarness harness(graph.get());

    auto* at_source = harness.add_forward(0, nullptr, nullptr);
    ljt::JoinHarness::set_value(at_source, 0.0);
    auto* forward = harness.add_forward(1, graph->get_arc(0), at_source);
    ljt::JoinHarness::set_value(forward, 1.0);

    auto* at_sink = harness.add_backward(3, nullptr, nullptr);
    ljt::JoinHarness::set_value(at_sink, 0.0);
    auto* backward = harness.add_backward(2, graph->get_arc(2), at_sink);
    ljt::JoinHarness::set_value(backward, ljt::kInfinity);

    const HalfWayPolicy policy(2.0, 6.0);
    EXPECT_TRUE(harness.run(policy).empty());
}

// ============================================================================
// Reconstruction
// ============================================================================

/// @brief The backward chain comes out in forward order, unreversed.
///
/// Verified by test rather than by reading: each backward label's prev_label is the label nearer
/// the sink and the arc it remembers is its out-arc, so the walk is already forward. Getting this
/// backwards silently drops or mis-orders the path.
TEST(LabelJoin, BackwardChainIsEmittedInForwardOrder) {
    namespace ljt = label_join_test;
    // Five nodes, so the backward half spans three arcs: 2->3, 3->4, 4->5.
    auto graph = ljt::line_graph({1.0, 1.0, 1.0, 1.0, 1.0});
    ljt::JoinHarness harness(graph.get());

    auto* at_source = harness.add_forward(0, nullptr, nullptr);
    ljt::JoinHarness::set_value(at_source, 0.0);
    auto* forward = harness.add_forward(1, graph->get_arc(0), at_source);
    ljt::JoinHarness::set_value(forward, 1.0);

    // Backward chain from the sink: 5, then 4, 3, 2.
    Label<ljt::Composed>* previous = harness.add_backward(5, nullptr, nullptr);
    ljt::JoinHarness::set_value(previous, 0.0);
    for (size_t node_id : {4U, 3U, 2U}) {
        previous = harness.add_backward(node_id, graph->get_arc(node_id), previous);
        ljt::JoinHarness::set_value(previous, static_cast<double>(5 - node_id));
    }

    const HalfWayPolicy policy(/*half_way_point=*/1.5, /*resource_upper_bound=*/10.0);
    const auto recorded = harness.run(policy);

    ASSERT_EQ(recorded.size(), 1U);
    // s->1, join 1->2, then 2->3, 3->4, 4->5 in that order -- not reversed.
    EXPECT_EQ(recorded[0].arc_ids, (std::vector<size_t>{0, 1, 2, 3, 4}));
    EXPECT_EQ(recorded[0].end_node_id, 5U);
}

/// @brief The contiguity check accepts a real merged path and rejects a broken one.
///
/// Asserted on every reconstruction in debug builds; tested here in both directions, because a
/// check that has never rejected anything is not much of a check.
TEST(LabelJoin, ContiguityCheckAcceptsAndRejects) {
    namespace ljt = label_join_test;
    auto graph = ljt::line_graph({1.0, 1.0, 1.0, 1.0});
    using JoinerType = Joiner<ljt::Composed, RealResource>;

    // A real path through the line: each arc's destination is the next arc's origin.
    EXPECT_TRUE(JoinerType::is_contiguous(*graph, {0, 1, 2, 3}));
    EXPECT_TRUE(JoinerType::is_contiguous(*graph, {}));   // nothing to disagree
    EXPECT_TRUE(JoinerType::is_contiguous(*graph, {2}));  // a single arc is trivially fine

    // Out of order, and with a gap: both must be rejected.
    EXPECT_FALSE(JoinerType::is_contiguous(*graph, {0, 2}));
    EXPECT_FALSE(JoinerType::is_contiguous(*graph, {3, 2, 1, 0}));
}

// ============================================================================
// Sorting, early exit, and pool hygiene
// ============================================================================

/// @brief Cost sorting lets the upper bound cut the pairing short.
///
/// With many labels on both sides and a tight incumbent, the merge test must be asked far fewer
/// than |fwd| x |bwd| times. Without the sort the early exits would break on the first expensive
/// label and silently discard valid joins.
TEST(LabelJoin, CostSortingEnablesTheEarlyExit) {
    namespace ljt = label_join_test;
    constexpr size_t kLabelsPerSide = 20;

    size_t merge_calls = 0;
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(
        std::make_unique<AdditionExtensionFunction<RealResource>>(),
        std::make_unique<ljt::CountingFeasibilityFunction>(&merge_calls),
        std::make_unique<ValueCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2);
    graph->add_node(3, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource>(std::make_tuple(0.0), 0, 1, 0.0);
    graph->add_arc<RealResource>(std::make_tuple(0.0), 1, 2, 0.0);
    graph->add_arc<RealResource>(std::make_tuple(0.0), 2, 3, 0.0);
    graph->sort_nodes();
    graph->build_csr();

    ljt::JoinHarness harness(graph.get());
    auto* at_source = harness.add_forward(0, nullptr, nullptr);
    ljt::JoinHarness::set_value(at_source, 0.0);
    auto* at_sink = harness.add_backward(3, nullptr, nullptr);
    ljt::JoinHarness::set_value(at_sink, 0.0);

    // Deliberately inserted worst-first, so only sorting can produce a usable order.
    for (size_t i = 0; i < kLabelsPerSide; ++i) {
        auto* forward = harness.add_forward(1, graph->get_arc(0), at_source);
        ljt::JoinHarness::set_value(forward, static_cast<double>(kLabelsPerSide - i));
        auto* backward = harness.add_backward(2, graph->get_arc(2), at_sink);
        ljt::JoinHarness::set_value(backward, static_cast<double>(kLabelsPerSide - i));
    }

    // Bound disabled so every pair is a candidate and only cost prunes.
    HalfWayPolicy policy(0.0, 0.0);
    ASSERT_FALSE(policy.enabled());
    harness.run(policy, /*upper_bound=*/5.0);

    EXPECT_GT(merge_calls, 0U);
    EXPECT_LT(merge_calls, kLabelsPerSide * kLabelsPerSide)
        << "the early exit should stop well short of the full product";
}

/// @brief Reference counts stay consistent across a join pass.
///
/// The joiner reads labels from both chains without releasing them and takes one pooled f' per
/// forward label -- a prime spot for a release_label / release_with_ref_count mix-up.
TEST(LabelJoin, RefCountsStayConsistent) {
    namespace ljt = label_join_test;
    auto graph = ljt::line_graph({1.0, 1.0, 1.0, 1.0});
    ljt::JoinHarness harness(graph.get());

    Label<ljt::Composed>* previous = nullptr;
    for (size_t node_id = 0; node_id <= 4; ++node_id) {
        const Arc<ljt::Composed>* in_arc = node_id == 0 ? nullptr : graph->get_arc(node_id - 1);
        previous = harness.add_forward(node_id, in_arc, previous);
        ljt::JoinHarness::set_value(previous, static_cast<double>(node_id));
    }
    Label<ljt::Composed>* backward_previous = nullptr;
    for (size_t step = 0; step <= 4; ++step) {
        const size_t node_id = 4 - step;
        const Arc<ljt::Composed>* out_arc = node_id == 4 ? nullptr : graph->get_arc(node_id);
        backward_previous = harness.add_backward(node_id, out_arc, backward_previous);
        ljt::JoinHarness::set_value(backward_previous, static_cast<double>(4 - node_id));
    }

    ASSERT_TRUE(harness.pool().check_ref_count_consistency());

    const HalfWayPolicy policy(2.5, 8.0);
    harness.run(policy);

    EXPECT_TRUE(harness.pool().check_ref_count_consistency())
        << "a join pass must not disturb the reference counts of either chain";
}

/// @brief With the bound disabled the joiner still finds the same path at the same cost.
///
/// "Correct but slow" is the claim the disable path rests on, so it is the claim tested here: the
/// disabled run must agree with the enabled one on both the arc sequence and the cost.
///
/// Note what it does *not* produce: four duplicate solutions, one per candidate join arc. The
/// crossing test is indeed skipped, but the first accepted join tightens best_cost_upper_bound to
/// that path's cost, and every later join of the same path costs exactly the same and is rejected
/// by `cost >= best`. So for equal-cost duplicates the bound already absorbs them before
/// `Solution`'s hash ever sees them -- the crossing rule's value is avoiding the *work*, and it
/// bites hardest when candidate paths differ in cost.
TEST(LabelJoin, DisabledBoundStillFindsThePath) {
    namespace ljt = label_join_test;
    auto graph = ljt::line_graph({1.0, 1.0, 1.0, 1.0});
    ljt::JoinHarness harness(graph.get());

    Label<ljt::Composed>* previous = nullptr;
    for (size_t node_id = 0; node_id <= 4; ++node_id) {
        const Arc<ljt::Composed>* in_arc = node_id == 0 ? nullptr : graph->get_arc(node_id - 1);
        previous = harness.add_forward(node_id, in_arc, previous);
        ljt::JoinHarness::set_value(previous, static_cast<double>(node_id));
    }
    Label<ljt::Composed>* backward_previous = nullptr;
    for (size_t step = 0; step <= 4; ++step) {
        const size_t node_id = 4 - step;
        const Arc<ljt::Composed>* out_arc = node_id == 4 ? nullptr : graph->get_arc(node_id);
        backward_previous = harness.add_backward(node_id, out_arc, backward_previous);
        ljt::JoinHarness::set_value(backward_previous, static_cast<double>(4 - node_id));
    }

    HalfWayPolicy disabled(0.0, 0.0);
    ASSERT_FALSE(disabled.enabled());
    const auto recorded = harness.run(disabled);

    ASSERT_FALSE(recorded.empty()) << "disabling the bound must not lose the path";
    EXPECT_EQ(recorded.front().arc_ids, (std::vector<size_t>{0, 1, 2, 3}));
    EXPECT_EQ(recorded.front().end_node_id, 4U);

    // The same path, at the same cost, as the enabled run finds.
    ljt::JoinHarness enabled_harness(graph.get());
    Label<ljt::Composed>* enabled_previous = nullptr;
    for (size_t node_id = 0; node_id <= 4; ++node_id) {
        const Arc<ljt::Composed>* in_arc = node_id == 0 ? nullptr : graph->get_arc(node_id - 1);
        enabled_previous = enabled_harness.add_forward(node_id, in_arc, enabled_previous);
        ljt::JoinHarness::set_value(enabled_previous, static_cast<double>(node_id));
    }
    Label<ljt::Composed>* enabled_backward = nullptr;
    for (size_t step = 0; step <= 4; ++step) {
        const size_t node_id = 4 - step;
        const Arc<ljt::Composed>* out_arc = node_id == 4 ? nullptr : graph->get_arc(node_id);
        enabled_backward = enabled_harness.add_backward(node_id, out_arc, enabled_backward);
        ljt::JoinHarness::set_value(enabled_backward, static_cast<double>(4 - node_id));
    }
    const HalfWayPolicy enabled(2.5, 8.0);
    const auto enabled_recorded = enabled_harness.run(enabled);

    ASSERT_EQ(enabled_recorded.size(), 1U);
    EXPECT_EQ(recorded.front().arc_ids, enabled_recorded.front().arc_ids);
    EXPECT_NEAR(recorded.front().cost, enabled_recorded.front().cost, ljt::kTolerance);
}
