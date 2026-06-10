// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.
//
// Regression test for the ng-route unreachable-set dominance augmentation
// (NgUnreachableCompositionExtensionFunction).  It builds a tiny VRPTW-like
// resource graph directly (no master problem / Gurobi) using the exact resource
// pack the VRP uses, then runs the labeling with the augmentation OFF and ON.
//
// The augmentation must be PURE ACCELERATION: the optimal source->sink cost is
// identical, while the number of non-dominated / extended labels strictly drops.

#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

namespace {

using namespace rcspp;

// Same pack as the VRP (src/vrp/vrp.hpp) so this also exercises the exact
// NgUnreachableCompositionExtensionFunction instantiation the VRP needs.
using NgGraph = ResourceGraph<RealResource, IntResource, SizeTSetResource, SizeTBitsetResource>;
using NgComposition =
    ResourceTypeComposition<RealResource, IntResource, SizeTSetResource, SizeTBitsetResource>;
using NgResource = SizeTBitsetResource;

struct NgInstance {
        std::vector<double> x;    // node x coordinates (y assumed 0)
        std::vector<double> due;  // latest service time per node
        std::vector<int> demand;  // demand per node
        int capacity = 1000000;   // vehicle capacity
        size_t ng_size = 2;       // ng-neighbourhood size
        // node 0 is the depot/source; node (n-1) is the sink (a depot copy).
};

struct NgSolveStats {
        double best_cost = std::numeric_limits<double>::infinity();
        size_t non_dominated = 0;
        size_t extended = 0;
};

inline double dist(const NgInstance& inst, size_t i, size_t j) {
    return std::abs(inst.x[i] - inst.x[j]);
}

// Closest ng_size customers (excluding the depot, the sink and the node itself).
inline std::map<size_t, std::set<size_t>> build_ng_neighborhoods(const NgInstance& inst) {
    const size_t n = inst.x.size();
    const size_t sink = n - 1;
    std::map<size_t, std::set<size_t>> ng;
    for (size_t j = 1; j < sink; ++j) {  // customers only
        std::vector<std::pair<double, size_t>> d;
        for (size_t k = 1; k < sink; ++k) {
            if (k != j) {
                d.emplace_back(dist(inst, j, k), k);
            }
        }
        std::ranges::sort(d);
        std::set<size_t> nbh;
        for (size_t i = 0; i < std::min(inst.ng_size, d.size()); ++i) {
            nbh.insert(d[i].second);
        }
        ng[j] = nbh;
    }
    ng[0] = {};
    ng[sink] = {};
    return ng;
}

inline NgSolveStats solve_ng_instance(const NgInstance& inst, bool augment,
                                      size_t max_labels = std::numeric_limits<size_t>::max(),
                                      size_t stop_after = std::numeric_limits<size_t>::max()) {
    const size_t n = inst.x.size();
    const size_t sink = n - 1;

    std::map<size_t, std::pair<double, double>> tw;
    std::map<size_t, std::set<size_t>> node_set;
    for (size_t i = 0; i < n; ++i) {
        tw[i] = {0.0, inst.due[i]};
        node_set[i] = (i == 0 || i == sink) ? std::set<size_t>{} : std::set<size_t>{i};
    }
    node_set[0] = {0};
    auto ng_neighborhoods = build_ng_neighborhoods(inst);

    NgGraph graph;
    // Cost (distance).
    graph.add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                     std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                     std::make_unique<ValueCostFunction<RealResource>>(),
                                     std::make_unique<ValueDominanceFunction<RealResource>>());
    // Time (time windows).
    graph.add_resource<RealResource>(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(tw),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(tw),
        std::make_unique<ValueCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    // Demand (capacity).
    graph.add_resource<IntResource>(
        std::make_unique<AdditionExtensionFunction<IntResource>>(),
        std::make_unique<MinMaxFeasibilityFunction<IntResource>>(0, inst.capacity),
        std::make_unique<ValueCostFunction<IntResource>>(),
        std::make_unique<ValueDominanceFunction<IntResource>>());
    // ng-memory.
    graph.add_resource<NgResource>(
        std::make_unique<NgPathExtensionFunction<NgResource, size_t>>(ng_neighborhoods),
        std::make_unique<IntersectionFeasibilityFunction<NgResource>>(node_set),
        std::make_unique<TrivialCostFunction<NgResource>>(),
        std::make_unique<InclusionDominanceFunction<NgResource>>());

    std::map<size_t, std::vector<const Arc<NgComposition>*>> ng_arcs;
    if (augment) {
        graph.set_composition_extension_function(
            std::make_unique<NgUnreachableCompositionExtensionFunction<NgResource,
                                                                       RealResource,
                                                                       IntResource,
                                                                       SizeTSetResource,
                                                                       SizeTBitsetResource>>(
                &ng_arcs));
    }

    // Nodes: 0 = depot/source, 1..sink-1 = customers, sink = sink.
    for (size_t i = 0; i < n; ++i) {
        graph.add_node(i, /*source=*/i == 0, /*sink=*/i == sink);
    }

    // Arcs: depot->customers, customer->customer, customer->sink.
    auto add = [&](size_t o, size_t d) {
        const double cost = dist(inst, o, d);
        const double time = dist(inst, o, d);  // service time 0
        const int demand = inst.demand[d];
        graph.add_arc<RealResource, RealResource, IntResource, SizeTBitsetResource>(
            {cost, time, demand, {{o}}},
            o,
            d,
            cost,
            {Row(o, 1.0)});
    };
    for (size_t d = 1; d < sink; ++d) {
        add(0, d);  // depot -> customer
    }
    for (size_t o = 1; o < sink; ++o) {
        for (size_t d = 1; d < sink; ++d) {
            if (o != d) {
                add(o, d);
            }
        }
        add(o, sink);  // customer -> sink
    }

    if (augment) {
        for (const auto& [j, nbh] : ng_neighborhoods) {
            if (nbh.empty()) {
                continue;
            }
            auto* node = graph.get_node(j);
            if (node == nullptr) {
                continue;
            }
            std::vector<const Arc<NgComposition>*> arcs;
            for (const auto* arc : node->out_arcs) {
                if (nbh.contains(arc->destination->id)) {
                    arcs.push_back(arc);
                }
            }
            if (!arcs.empty()) {
                ng_arcs.emplace(j, std::move(arcs));
            }
        }
    }

    AlgorithmParams<LabelList<NgComposition>> params;
    params.num_labels_to_extend_by_node = max_labels;
    params.stop_after_X_solutions = stop_after;
    params.return_dominated_solutions =
        stop_after < std::numeric_limits<size_t>::max();  // needed for stop_after to fire
    auto algo = graph.create_algorithm<SimpleDominanceAlgorithm, LabelList<NgComposition>>(
        std::move(params));
    auto result = graph.solve(algo.get());

    NgSolveStats stats;
    if (!result.solutions.empty()) {
        stats.best_cost = result.solutions[0].cost;
    }
    stats.non_dominated = algo->total_non_dominated_labels();
    stats.extended = result.num_extended_labels;             // SolveResult carries the count
    EXPECT_EQ(stats.extended, algo->num_extended_labels());  // ... matching the getter
    return stats;
}

// A tight-time-window instance: customers spread on a line with late due dates
// that shrink with distance from the depot, so many ng-neighbours become
// time-unreachable from later labels.
inline NgInstance tight_tw_instance() {
    NgInstance inst;
    //              depot  c1    c2    c3    c4    c5    c6   sink
    inst.x = {0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 0.0};
    inst.due = {100.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 100.0};
    inst.demand = {0, 1, 1, 1, 1, 1, 1, 0};
    inst.capacity = 1000;  // loose: only time windows bind
    inst.ng_size = 3;
    return inst;
}

// A clustered instance (customers packed in a small box => tiny travel times)
// with a tunable time-window width.  Wide windows + loose capacity let ng-route
// keep cycles longer than the neighbourhood, which makes the label count blow up.
inline NgInstance clustered_instance(size_t n_customers, double tw_width) {
    NgInstance inst;
    inst.x.push_back(0.0);  // depot
    for (size_t i = 0; i < n_customers; ++i) {
        inst.x.push_back(1.0 + 0.1 * static_cast<double>(i));  // tightly clustered
    }
    inst.x.push_back(0.0);  // sink

    inst.due.assign(inst.x.size(), tw_width);
    inst.due.front() = 1e6;
    inst.due.back() = 1e6;

    inst.demand.assign(inst.x.size(), 1);
    inst.demand.front() = 0;
    inst.demand.back() = 0;

    inst.capacity = static_cast<int>(n_customers);  // a route may visit everyone
    inst.ng_size = 3;
    return inst;
}

}  // namespace

// ── Tests ─────────────────────────────────────────────────────────────────────

// Diagnostic (disabled by default): shows how the non-dominated label count of
// ng-route pricing explodes as time windows widen on a clustered instance --
// the root cause of the benchmark "hang" on wider-TW Solomon instances (C102+).
// Run with: tests-rcspp --gtest_also_run_disabled_tests
//                       --gtest_filter='*NgLabelExplosion*'
TEST(NgUnreachableAugmentation, DISABLED_NgLabelExplosion) {
    const size_t n = 30;
    const auto inst = clustered_instance(n, /*tw_width=*/1000.0);  // wide TW
    const auto uncapped = solve_ng_instance(inst, /*augment=*/false);
    std::cout << "uncapped: extended=" << uncapped.extended << "\n";
    // The per-node label cap bounds the work once it drops below the natural
    // processed-labels-per-node (~n^2 on the real Solomon instances, so a cap of
    // 100 binds hard at n=100 even though it does not at this toy scale).
    for (size_t cap : {5, 10, 25, 50}) {
        const auto capped = solve_ng_instance(inst, /*augment=*/false, /*max_labels=*/cap);
        std::cout << "max_labels=" << cap << " -> extended=" << capped.extended << "\n";
    }
    // stop_after_X_solutions (with return_dominated_solutions) bounds it too.
    for (size_t stop : {10, 40, 120}) {
        const auto s = solve_ng_instance(inst,
                                         /*augment=*/false,
                                         /*max_labels=*/std::numeric_limits<size_t>::max(),
                                         /*stop_after=*/stop);
        std::cout << "stop_after=" << stop << " -> extended=" << s.extended << "\n";
    }
}

TEST(NgUnreachableAugmentation, SameOptimumFewerLabels) {
    const auto inst = tight_tw_instance();
    const auto base = solve_ng_instance(inst, /*augment=*/false);
    const auto aug = solve_ng_instance(inst, /*augment=*/true);

    std::cout << "base: cost=" << base.best_cost << " extended=" << base.extended << "\n";
    std::cout << "aug : cost=" << aug.best_cost << " extended=" << aug.extended << "\n";

    // The optimal route must actually be found.
    EXPECT_LT(base.best_cost, std::numeric_limits<double>::infinity());
    // Pure acceleration: identical optimal cost.
    EXPECT_NEAR(base.best_cost, aug.best_cost, 1e-9);
    // ... with strictly fewer labels extended (stronger dominance).
    EXPECT_LT(aug.extended, base.extended);
}

TEST(NgUnreachableAugmentation, NoFoldingWhenWindowsLoose) {
    // Wide time windows + loose capacity => nothing is unreachable => the
    // augmentation is a no-op (identical cost and identical label counts).
    NgInstance inst = tight_tw_instance();
    for (auto& d : inst.due) {
        d = 1000.0;
    }
    const auto base = solve_ng_instance(inst, /*augment=*/false);
    const auto aug = solve_ng_instance(inst, /*augment=*/true);

    EXPECT_NEAR(base.best_cost, aug.best_cost, 1e-9);
    EXPECT_EQ(aug.extended, base.extended);
}
