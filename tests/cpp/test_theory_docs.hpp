// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The numbers of the theory pages (docs/theory/) that only C++ can see.
//
// The other numbers are checked from Python, in tests/python/test_theory_numbers.py. These two
// need what Python does not expose: the labels a bidirectional search keeps (page 5), and per-node
// windows on a MinMaxFeasibilityFunction (page 3). A failure means the library's behaviour changed
// and the page needs updating, or the page is wrong.

#include <gtest/gtest.h>

#include <algorithm>
#include <compare>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace theory_docs_test {

using Composed = ResourceTypeComposition<RealResource>;
using Container = LabelList<Composed>;
using Bidir = detail::BidirectionalDominanceAlgorithm<Composed, Container, RealResource>;
using Consumption = std::tuple<std::vector<std::tuple<double>>>;

constexpr size_t kSink = 4;
constexpr double kDual = 20.0;
constexpr double kCapacity = 11.0;
constexpr double kHalfWay = 16.0;

/// @brief One label, as the page's tables write it: its path and its three resource values.
struct LabelRow {
        std::string path;
        double cost;
        double time;
        double load;

        auto operator<=>(const LabelRow&) const = default;
};

/// @brief The bidirectional search, with read access to the forward labels it keeps.
class Probe : public Bidir {
    public:
        using Bidir::Bidir;

        [[nodiscard]] const auto& forward_labels() const {
            return this->non_dominated_labels_by_node_pos_;
        }
};

/// @brief Page 2's pricing graph: time windows, capacity 11, every dual 20.
///
/// Resources: reduced cost, time, load. The dual of customer `i` is on the arcs leaving `i`.
inline std::unique_ptr<ResourceGraph<RealResource>> page2_graph() {
    const std::map<std::pair<size_t, size_t>, double> distances{{{0, 1}, 10.0},
                                                                {{0, 2}, 10.0},
                                                                {{0, 3}, 10.0},
                                                                {{1, 2}, 4.0},
                                                                {{1, 3}, 5.0},
                                                                {{2, 3}, 6.0}};
    const auto distance = [&](size_t i, size_t j) {
        i = (i == kSink) ? 0 : i;
        j = (j == kSink) ? 0 : j;
        return distances.at({std::min(i, j), std::max(i, j)});
    };
    const std::map<size_t, double> demand{{1, 4.0}, {2, 5.0}, {3, 6.0}};
    const std::map<size_t, std::pair<double, double>> windows{{0, {0.0, 100.0}},
                                                              {1, {0.0, 15.0}},
                                                              {2, {20.0, 30.0}},
                                                              {3, {10.0, 18.0}},
                                                              {kSink, {0.0, 100.0}}};

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
        std::make_unique<AdditionExtensionFunction<RealResource>>(),
        std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, kCapacity),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());

    graph->add_node(0, /*source=*/true, /*sink=*/false);
    for (size_t i = 1; i <= 3; ++i) {
        graph->add_node(i);
    }
    graph->add_node(kSink, /*source=*/false, /*sink=*/true);

    for (size_t i = 0; i <= 3; ++i) {
        for (const size_t j : {size_t{1}, size_t{2}, size_t{3}, kSink}) {
            if (i == j || (i == 0 && j == kSink)) {
                continue;
            }
            const double d = distance(i, j);
            const double load = (j == kSink) ? 0.0 : demand.at(j);
            std::vector<Row> rows;
            if (i != 0) {
                rows.emplace_back(i, 1.0);
            }
            graph->add_arc(Consumption{{{d}, {d}, {load}}}, i, j, d, rows);
        }
    }
    graph->update_reduced_costs({0.0, kDual, kDual, kDual});
    return graph;
}

/// @brief A label's path as the page writes it, `0->1->2` or `2->0'`.
///
/// A forward label follows its in-arcs back to the source; a backward label follows its out-arcs
/// forward to the sink.
template <bool Backward>
std::string path_of(const Label<Composed>& label) {
    std::vector<size_t> nodes{label.get_end_node()->id};
    for (const Label<Composed>* cur = &label; cur != nullptr; cur = cur->prev_label) {
        const auto* arc = Backward ? cur->get_out_arc() : cur->get_in_arc();
        if (arc == nullptr) {
            break;
        }
        nodes.push_back(Backward ? arc->destination->id : arc->origin->id);
    }
    if (!Backward) {
        std::ranges::reverse(nodes);
    }
    std::string path;
    for (const size_t node : nodes) {
        path +=
            (path.empty() ? "" : "->") + (node == kSink ? std::string("0'") : std::to_string(node));
    }
    return path;
}

/// @brief @p rows, sorted, so two tables compare whatever order they were listed in.
inline std::vector<LabelRow> sorted(std::vector<LabelRow> rows) {
    std::ranges::sort(rows);
    return rows;
}

/// @brief The labels in @p containers, as sorted rows.
template <bool Backward, typename Containers>
std::vector<LabelRow> rows_of(const Containers& containers) {
    std::vector<LabelRow> rows;
    for (const auto& container : containers) {
        for (const auto* label : container.get_labels()) {
            const auto& resource = label->get_resource();
            const auto value = [&](size_t index) {
                return resource.template get_component<RealResource>(index).get_value().get_value();
            };
            rows.push_back({path_of<Backward>(*label), value(0), value(1), value(2)});
        }
    }
    return sorted(std::move(rows));
}

}  // namespace theory_docs_test

// ============================================================================
// Page 5: searching from both ends
// ============================================================================

/// @brief Section 5.4's tables: the labels each search keeps with H = 16, and where the three
///        routes come from.
TEST(TheoryDocs, Page5LabelTablesAtHalfWayPoint16) {
    namespace td = theory_docs_test;
    auto graph = td::page2_graph();

    AlgorithmParams<td::Container> params;
    params.direction = SearchDirection::Bidirectional;
    params.critical_resource_index = 1;  // time
    params.half_way_point = td::kHalfWay;
    params.release_after_solve = false;  // keep the labels for inspection
    td::Probe probe(&graph->get_resource_factory(), params);
    const auto result = graph->solve(&probe, -1e-9);

    // The forward table: time at most 16 is extended, above 16 is a boundary label.
    const std::vector<td::LabelRow> forward{
        {"0", 0.0, 0.0, 0.0},
        {"0->1", 10.0, 10.0, 4.0},
        {"0->1->0'", 0.0, 20.0, 4.0},
        {"0->1->2", -6.0, 20.0, 9.0},
        {"0->1->3", -5.0, 15.0, 10.0},
        {"0->1->3->0'", -15.0, 25.0, 10.0},
        {"0->2", 10.0, 20.0, 5.0},
        {"0->3", 10.0, 10.0, 6.0},
        {"0->3->1", -5.0, 15.0, 10.0},
    };
    EXPECT_EQ(td::rows_of<false>(probe.forward_labels()), td::sorted(forward));

    // The backward table: the time value is a deadline; below 16 a label is stopped.
    const std::vector<td::LabelRow> backward{
        {"0'", 0.0, 100.0, 0.0},
        {"0->2->0'", 0.0, 20.0, 5.0},
        {"0->3->2->0'", -14.0, 8.0, 11.0},
        {"1->0'", -10.0, 15.0, 0.0},
        {"1->2->0'", -26.0, 15.0, 5.0},
        {"1->3->2->0'", -39.0, 13.0, 11.0},
        {"2->0'", -10.0, 30.0, 0.0},
        {"3->0'", -10.0, 18.0, 0.0},
        {"3->2->0'", -24.0, 18.0, 5.0},
    };
    EXPECT_EQ(td::rows_of<true>(probe.get_backward_labels_by_node_pos()), td::sorted(backward));

    // -16 from the join, -15 from the forward search, -14 from the backward search.
    ASSERT_EQ(result.solutions.size(), 3U);
    EXPECT_DOUBLE_EQ(result.solutions[0].cost, -16.0);
    EXPECT_DOUBLE_EQ(result.solutions[1].cost, -15.0);
    EXPECT_DOUBLE_EQ(result.solutions[2].cost, -14.0);
    ASSERT_TRUE(result.bidirectional.has_value());
    EXPECT_TRUE(result.bidirectional->bounded_by_half_way);
    EXPECT_EQ(result.bidirectional->number_of_joined_paths, 1U);
}

/// @brief Section 5.8's C++ snippet: the model passes the checks and returns the three routes.
TEST(TheoryDocs, Page5CppSnippet) {
    auto graph = theory_docs_test::page2_graph();

    AlgorithmBaseParams params;
    params.direction = SearchDirection::Bidirectional;
    params.critical_resource_index = 1;
    params.half_way_point = 16.0;
    const auto result = graph->solve<SimpleDominanceAlgorithm>(params, -1e-9);
    const auto report = graph->check_model(SearchDirection::Bidirectional);

    EXPECT_TRUE(report.problems.empty());
    ASSERT_EQ(result.solutions.size(), 3U);
    EXPECT_DOUBLE_EQ(result.solutions.front().cost, -16.0);
}

// ============================================================================
// Page 3: dominance and elementarity
// ============================================================================

/// @brief Section 3.3: with no waiting, "lower time is better" discards the only feasible route.
///
/// Labels A (cost 5, time 10) and B (cost 6, time 12) reach node 1 through nodes 5 and 6. Arc
/// 1 -> 2 takes 4, and customer 2 must be reached inside [15, 16], with no waiting: time is an
/// addition, checked by a per-node MinMaxFeasibilityFunction. A dominates B, but only B can reach
/// 2 in time.
TEST(TheoryDocs, Page3NoWaitingDominanceIsUnsafe) {
    const auto build = [](bool with_label_a) {
        auto graph = std::make_unique<ResourceGraph<RealResource>>();
        graph->add_resource<RealResource>(
            std::make_unique<AdditionExtensionFunction<RealResource>>(),
            std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
            std::make_unique<ValueCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
        graph->add_resource<RealResource>(
            std::make_unique<AdditionExtensionFunction<RealResource>>(),
            std::make_unique<MinMaxFeasibilityFunction<RealResource>>(
                0.0,
                100.0,
                std::map<size_t, std::pair<double, double>>{{2, {15.0, 16.0}}}),
            std::make_unique<TrivialCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
        graph->add_node(0, /*source=*/true, /*sink=*/false);
        for (const size_t node : {1, 2, 5, 6}) {
            graph->add_node(node);
        }
        graph->add_node(theory_docs_test::kSink, /*source=*/false, /*sink=*/true);
        using Consumption = theory_docs_test::Consumption;
        if (with_label_a) {
            graph->add_arc(Consumption{{{5.0}, {10.0}}}, 0, 5);  // label A
            graph->add_arc(Consumption{{{0.0}, {0.0}}}, 5, 1);
        }
        graph->add_arc(Consumption{{{6.0}, {12.0}}}, 0, 6);  // label B
        graph->add_arc(Consumption{{{0.0}, {0.0}}}, 6, 1);
        graph->add_arc(Consumption{{{0.0}, {4.0}}}, 1, 2);
        graph->add_arc(Consumption{{{0.0}, {0.0}}}, 2, theory_docs_test::kSink);
        return graph;
    };

    // Preprocessing is off: it relies on the same property as dominance (page 4, section 4.6), so
    // on this model it removes arc 1 -> 2 before the search starts, and would hide what dominance
    // does.
    const auto solve = [](ResourceGraph<RealResource>* graph) {
        return graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{},
                                                      std::numeric_limits<double>::infinity(),
                                                      /*preprocess=*/false);
    };

    // A dominates B at node 1, A arrives at 2 at 14, too early: nothing is found.
    auto with_a = build(true);
    EXPECT_TRUE(solve(with_a.get()).solutions.empty());

    // The route through B exists: without A, it is found, at cost 6.
    auto without_a = build(false);
    const auto result = solve(without_a.get());
    ASSERT_EQ(result.solutions.size(), 1U);
    EXPECT_DOUBLE_EQ(result.solutions.front().cost, 6.0);
}
