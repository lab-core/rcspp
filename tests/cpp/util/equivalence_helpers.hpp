// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Shared machinery for the equivalence suites: build a model, solve it, replay every returned
// path through the real Extender and report what is wrong with it.
//
// This lives under util/ rather than in test_equivalence.hpp so that the sweep header holds
// assertions and nothing else: half of it was machinery, and a reader looking for what the sweep
// claims had to skip 250 lines of setup to find it. A util/ header carries no TEST macros, so it
// is also the only kind of test header the rule at the top of test_main.cpp lets a second
// translation unit include, should one ever need to.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/random_instance.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace equivalence_test {

using Composed = ResourceTypeComposition<RealResource>;
using Graph = ResourceGraph<RealResource>;

constexpr double kTolerance = 1e-9;

/// @brief A solve and the graph it ran on, kept together so the paths can be replayed afterwards.
struct Run {
        std::unique_ptr<Graph> graph;
        SolveResult result;
        bool bounded = false;

        [[nodiscard]] double best_cost() const {
            return result.solutions.empty() ? std::numeric_limits<double>::infinity()
                                            : result.solutions.front().cost;
        }
};

/// @brief The reference: an ordinary forward search.
inline Run solve_forward(const test_util::InstanceConfig& config) {
    Run run;
    auto built = test_util::build_instance(config);
    run.graph = std::move(built.graph);
    run.result = run.graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    return run;
}

/// @brief The candidate.
///
/// @param config       The instance to build.
/// @param with_bound   When false the half-way point is left at 0, which leaves the policy with no
///                     explicit `H` and no finite `R` to derive one from, so it starts disabled.
///                     That is the correct-but-slow path, and running every instance both ways is
///                     what isolates a disagreement to the bound rather than to the join.
inline Run solve_bidirectional(const test_util::InstanceConfig& config, bool with_bound) {
    Run run;
    auto built = test_util::build_instance(config);
    run.graph = std::move(built.graph);

    AlgorithmParams<LabelList<Composed>> params;
    params.critical_resource_index = built.clock_index;
    params.half_way_point = with_bound ? built.clock_upper_bound / 2.0 : 0.0;

    auto algorithm =
        run.graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    run.result = run.graph->solve(algorithm.get());
    run.bounded = algorithm->bounded_by_half_way();
    EXPECT_TRUE(algorithm->get_label_pool().check_ref_count_consistency())
        << "reference counts must survive two searches and a join";
    return run;
}

/// @brief What a bounded run varies. Defaults reproduce an unbounded run exactly.
struct BoundedOptions {
        /// @brief The caller's `upper_bound`: solutions costing at least this are not returned.
        double upper_bound = std::numeric_limits<double>::infinity();

        /// @brief Whether the search prunes against the incumbent.
        ///
        /// For the forward algorithms that is `label.get_cost() >= best`, which is invalid on a
        /// frontier; for the bidirectional one it is a *completion* bound, which is valid only if
        /// it relaxes on the slot the labels accumulate.
        bool prune_based_on_upper_bound = false;

        /// @brief Whether the half-way bound is in force (bidirectional only).
        bool with_half_way_bound = true;

        /// @brief Whether `solve()` preprocesses. A binding upper bound plus preprocessing is what
        ///        strips every arc from the graph, which is its own failure mode.
        bool preprocess = true;
};

/// @brief The reference, under a caller-supplied upper bound.
inline Run solve_forward_bounded(const test_util::InstanceConfig& config,
                                 const BoundedOptions& options) {
    Run run;
    auto built = test_util::build_instance(config);
    run.graph = std::move(built.graph);

    AlgorithmParams<LabelList<Composed>> params;
    params.prune_based_on_upper_bound_ = options.prune_based_on_upper_bound;
    run.result =
        run.graph->solve<SimpleDominanceAlgorithm>(options.upper_bound, params, options.preprocess);
    return run;
}

/// @brief The candidate, under a caller-supplied upper bound.
inline Run solve_bidirectional_bounded(const test_util::InstanceConfig& config,
                                       const BoundedOptions& options) {
    Run run;
    auto built = test_util::build_instance(config);
    run.graph = std::move(built.graph);

    AlgorithmParams<LabelList<Composed>> params;
    params.critical_resource_index = built.clock_index;
    params.half_way_point = options.with_half_way_bound ? built.clock_upper_bound / 2.0 : 0.0;
    params.prune_based_on_upper_bound_ = options.prune_based_on_upper_bound;
    // The completion bounds relax on this slot; the labels accumulate slot 0. See finding D1.
    params.heuristic_cost_index = 0;

    auto algorithm =
        run.graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    run.result = run.graph->solve(algorithm.get(), options.upper_bound, options.preprocess);
    run.bounded = algorithm->bounded_by_half_way();
    EXPECT_TRUE(algorithm->get_label_pool().check_ref_count_consistency())
        << "reference counts must survive two searches and a join";
    return run;
}

/// @brief Replays one path through the model, returning what is wrong with it, or "".
///
/// Extends a fresh source resource arc by arc exactly as the search would, and requires every
/// intermediate node to be feasible. Mirrors what `FeasibilityPreprocessor` does with an arc's
/// extender, which is the only route to an arc's stored consumption.
inline std::string path_problem(const rcspp::Graph<Composed>& graph, const Solution& solution) {
    if (solution.path_arc_ids.empty()) {
        return "empty path";
    }

    const auto* first = graph.get_arc(solution.path_arc_ids.front());
    if (first == nullptr) {
        return "unknown arc id";
    }
    if (!first->origin->source) {
        return "path does not start at a source";
    }

    auto current = std::make_unique<Resource<Composed>>(*first->origin->resource);
    const Arc<Composed>* previous = nullptr;

    for (size_t arc_id : solution.path_arc_ids) {
        const auto* arc = graph.get_arc(arc_id);
        if (arc == nullptr) {
            return "unknown arc id";
        }
        if (previous != nullptr && previous->destination != arc->origin) {
            return "path is not contiguous at arc " + std::to_string(arc_id);
        }
        if (arc->extender == nullptr) {
            return "arc has no extender";
        }

        auto extended = std::make_unique<Resource<Composed>>(*arc->destination->resource);
        arc->extender->extend(*current, extended.get());
        if (!extended->is_feasible()) {
            return "path is infeasible at node " + std::to_string(arc->destination->id);
        }
        current = std::move(extended);
        previous = arc;
    }

    if (!previous->destination->sink) {
        return "path does not end at a sink";
    }
    return "";
}

/// @brief Replays every returned path and reports the first problem found.
inline std::string any_path_problem(const Run& run) {
    for (const auto& solution : run.result.solutions) {
        const std::string problem = path_problem(*run.graph, solution);
        if (!problem.empty()) {
            return problem;
        }
    }
    return "";
}

/// @brief The configurations the sweep runs, one per interesting combination of knobs.
///
/// Chosen rather than sampled: each row turns on one dimension the phase plan names, so a failure
/// points at a dimension instead of at "randomness". Seeds vary per row so the rows are not
/// correlated draws of the same graph.
inline std::vector<test_util::InstanceConfig> sweep() {
    std::vector<test_util::InstanceConfig> configs;

    // Cost only, sparse and dense. The clock is the cost slot, so the bound must switch itself off.
    configs.push_back({.num_nodes = 10, .density = 0.2, .seed = 1});
    configs.push_back({.num_nodes = 12, .density = 0.9, .seed = 2});

    // Time windows, loose and tight. Tight windows are where bidirectional wins and where a wrong
    // deadline shows.
    configs.push_back({.num_nodes = 14,
                       .density = 0.4,
                       .with_time_window = true,
                       .window_slack = 2.0,
                       .seed = 3});
    configs.push_back({.num_nodes = 14,
                       .density = 0.4,
                       .with_time_window = true,
                       .window_slack = 0.05,
                       .seed = 4});
    configs.push_back({.num_nodes = 24,
                       .density = 0.6,
                       .with_time_window = true,
                       .window_slack = 0.2,
                       .seed = 5});

    // Capacity, binding and not -- the Threshold clamp on its own.
    configs.push_back({.num_nodes = 12,
                       .density = 0.5,
                       .with_capacity = true,
                       .capacity_binds = true,
                       .seed = 6});
    configs.push_back({.num_nodes = 12,
                       .density = 0.5,
                       .with_capacity = true,
                       .capacity_binds = false,
                       .seed = 7});

    // Both, so the composition fans out over three components.
    configs.push_back({.num_nodes = 16,
                       .density = 0.5,
                       .with_time_window = true,
                       .window_slack = 0.5,
                       .with_capacity = true,
                       .capacity_binds = true,
                       .seed = 8});

    // Mixed-sign costs: the completion bound and the monotonicity fallback.
    configs.push_back({.num_nodes = 14, .density = 0.5, .mixed_sign_costs = true, .seed = 9});
    configs.push_back({.num_nodes = 18,
                       .density = 0.4,
                       .with_time_window = true,
                       .window_slack = 0.5,
                       .mixed_sign_costs = true,
                       .seed = 10});

    // Two sinks with different windows: per-node backward seeding.
    configs.push_back({.num_nodes = 15,
                       .density = 0.5,
                       .with_time_window = true,
                       .window_slack = 0.5,
                       .num_sinks = 2,
                       .seed = 11});
    configs.push_back({.num_nodes = 15,
                       .density = 0.5,
                       .with_capacity = true,
                       .capacity_binds = true,
                       .mixed_sign_costs = true,
                       .num_sinks = 2,
                       .seed = 12});

    // The large end, to catch anything that only appears with many labels per node.
    configs.push_back({.num_nodes = 40,
                       .density = 0.3,
                       .with_time_window = true,
                       .window_slack = 0.3,
                       .with_capacity = true,
                       .capacity_binds = true,
                       .seed = 13});

    return configs;
}

}  // namespace equivalence_test
